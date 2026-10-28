#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build for Ubuntu 24.04 without installing or accessing a reader."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PIN = '4dff813bb80ddedb2b801b89ced223e4e4594ca7'
ENV = {'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'LC_ALL': 'C'}
PACKAGES = ('gcc', 'meson', 'ninja-build', 'pkgconf', 'libglib2.0-dev',
            'libgusb-dev', 'libusb-1.0-0-dev', 'libssl-dev',
            'libjson-glib-dev', 'binutils')


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def run(*args, env=None):
    result = subprocess.run([str(x) for x in args], env=env or ENV,
                            text=True, capture_output=True)
    require(result.returncode == 0,
            str(args[0]) + ': ' + (result.stderr or result.stdout).strip())
    return result.stdout.strip()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prerequisites():
    require(os.geteuid() != 0, 'Build as an ordinary user.')
    release = platform.freedesktop_os_release()
    require(release.get('ID') == 'ubuntu' and release.get('VERSION_ID') == '24.04'
            and platform.machine() == 'x86_64', 'Ubuntu 24.04 x86_64 is required.')
    for command in ('cc', 'ar', 'meson', 'ninja', 'pkg-config', 'glib-mkenums',
                    'readelf', 'nm', 'ldd', 'dpkg-query'):
        require(shutil.which(command, path=ENV['PATH']), 'Missing command: ' + command)
    for package in PACKAGES:
        require(run('dpkg-query', '-W', '-f=${db:Status-Status}', package) == 'installed',
                'Missing package: ' + package)
    require(Path('/usr/libexec/fprintd').is_file(), 'Install Ubuntu fprintd first.')


def build(output, allow_pairing=False):
    prerequisites()
    upstream = load('ubuntu_upstream_builder', ROOT / 'production/build-public.py')
    upstream.validate_chicago_sources()
    before = upstream.source_inventory()
    adapter_before = digest(Path(__file__))
    require(output.is_absolute() and not output.exists() and not output.is_symlink(),
            'Use a new absolute output directory.')
    require(not output.resolve().is_relative_to(ROOT), 'Output must be outside source tree.')
    output.mkdir(mode=0o700)
    # A failed build never publishes a ready manifest. No installer uses this directory implicitly.
    with tempfile.TemporaryDirectory(prefix='goodix-ubuntu-build-') as temporary:
        work = Path(temporary)
        flags = shlex.split(run('pkg-config', '--cflags', '--libs', 'glib-2.0', 'openssl'))
        guard = work / 'test-pairing-guard'
        run('cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-DGOODIX_UBUNTU_BLOCK_PAIRING',
            '-I' + str(ROOT / 'libfprint-driver'), ROOT / 'ubuntu/test_pairing_guard.c',
            *(ROOT / 'libfprint-driver' / (name + '.c') for name in
              ('goodix_a0_protocol', 'goodix_bb010002', 'goodix_pairing_provision')),
            *flags, '-o', guard)
        print(run(guard), flush=True)
        archive, includes, provenance = upstream.native.build_native(ROOT, work, run)
        cflags = ['-DGOODIX_PRODUCTION_FPRINTD_ACTION_PROFILE', '-DGOODIX_UBUNTU_READY_PROMPT', '-fstack-protector-strong',
                  '-D_FORTIFY_SOURCE=2', f'-ffile-prefix-map={ROOT}=/usr/src/goodix',
                  f'-ffile-prefix-map={work}=/usr/src/goodix-build']
        if not allow_pairing:
            cflags.append('-DGOODIX_UBUNTU_BLOCK_PAIRING')
        directory = work / 'meson'
        run('meson', 'setup', directory, ROOT / upstream.LIBFPRINT_SOURCE,
            '--prefix=/usr', '--libdir=lib/x86_64-linux-gnu', '--buildtype=release',
            '--wrap-mode=nodownload', '-Ddrivers=goodix_27c6_5125',
            '-Dintrospection=false', '-Ddoc=false', '-Dinstalled-tests=false',
            '-Dudev_rules=disabled', '-Dudev_hwdb=disabled',
            '-Dgoodix_production_minimal=true', '-Dgoodix_chicago=true',
            '-Dgoodix_chicago_native_archive=' + str(archive),
            '-Dgoodix_chicago_native_include=' + str(includes),
            '-Dc_args=' + json.dumps(cflags), '-Dc_link_args=-Wl,-z,relro,-z,now',
            env=dict(ENV, CC='cc'))
        run('meson', 'compile', '-C', directory, 'fprint-2', env=dict(ENV, CC='cc'))
        library = directory / 'libfprint/libfprint-2.so.2.0.0'
        require(upstream.soname(library) == 'libfprint-2.so.2', 'Unexpected SONAME.')
        symbols = run('nm', library)
        forbidden = set((ROOT / 'production/host-test-only-symbols.txt').read_text().splitlines())
        require(not set(re.findall(r'\b(goodix_\w+)$', symbols, re.M)) & forbidden,
                'Test-only symbols in library.')
        require(allow_pairing or b'Ubuntu preview blocks persistent pairing writes' in library.read_bytes(),
                'Pairing guard missing from preview library.')
        runtime = output / 'runtime'
        runtime.mkdir()
        shutil.copyfile(library, runtime / library.name)
        (runtime / 'libfprint-2.so.2').symlink_to(library.name)
        required = set(re.findall(r'\b(fp_\w+)@LIBFPRINT_2\.0\.0',
                                 run('nm', '-D', '--undefined-only', '/usr/libexec/fprintd')))
        provided = set(re.findall(r'\b(fp_\w+)@@LIBFPRINT_2\.0\.0',
                                 run('nm', '-D', '--defined-only', library)))
        require(required and required <= provided, 'Ubuntu fprintd ABI mismatch.')
        linked = run('ldd', '/usr/libexec/fprintd', env=dict(ENV, LD_LIBRARY_PATH=str(runtime)))
        require(str(runtime / 'libfprint-2.so.2') in linked and 'not found' not in linked,
                'Ubuntu fprintd library resolution failed.')
        require(not any(x in linked.lower() for x in ('opencv', 'libstdc++', 'libc++')),
                'Unexpected matcher dependencies.')
        dynamic = run('readelf', '-d', library)
        require('BIND_NOW' in dynamic and 'GNU_RELRO' in run('readelf', '-l', library),
                'RELRO/now hardening missing.')
        shutil.copyfile(ROOT / 'production/chicago-native/NOTICES.txt', runtime / 'ChicagoHS-NOTICES.txt')
        shutil.copytree(ROOT / 'LICENSES', runtime / 'licenses')
        shutil.copyfile(ROOT / 'docs/LICENSING_AND_PROVENANCE.md', runtime / 'LICENSING_AND_PROVENANCE.md')
        (output / 'abi-check.txt').write_text(linked + '\n')
        (output / 'source-files.sha256').write_text(before)
        require(upstream.source_inventory() == before and digest(Path(__file__)) == adapter_before,
                'Sources changed during build.')
        manifest = {'schema': 1, 'target': 'ubuntu-24.04-x86_64',
                    'upstream_commit': PIN, 'pairing_writes_enabled': allow_pairing,
                    'adapter_sha256': adapter_before, 'native': provenance,
                    'required_fprintd_symbols': sorted(required),
                    'packages': {p: run('dpkg-query', '-W', '-f=${Version}', p)
                                 for p in (*PACKAGES, 'fprintd')},
                    'files': {str(p.relative_to(output)): digest(p)
                              for p in output.rglob('*') if p.is_file() and not p.is_symlink()}}
        (output / 'ubuntu-build.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print('UBUNTU_BUILD=PASS')
    print('FPRINTD_ABI=PASS LINKING=PASS HARDENING=PASS')
    print('PAIRING_WRITES=' + ('ENABLED' if allow_pairing else 'BLOCKED'))
    print('INSTALLED=false SENSOR_ACCESSED=false PAM_CHANGED=false')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--allow-persistent-pairing', action='store_true',
                        help='Build an operational version permitting persistent sensor pairing; does not run it.')
    args = parser.parse_args()
    try:
        build(args.output, args.allow_persistent_pairing)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, str(error) + '\n')
