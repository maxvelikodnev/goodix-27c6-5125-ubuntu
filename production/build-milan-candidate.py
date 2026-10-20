#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the private, noninstalled M4 Milan candidate; never an installer payload."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil

ROOT = Path(__file__).resolve().parents[1]
C1_PIN = '4f48c482f35976ea4fac27d5e598730581c4f380'
MILAN_PIN = 'e68eb725b1a7845b401b1a3c9b0790921f12133e'
REFERENCE = ROOT / 'development/reference/goodix521d-fprintd-v0.1.0-19'
PACKAGES = ('gcc', 'gcc-c++', 'meson', 'ninja-build', 'pkgconf-pkg-config',
            'glib2-devel', 'libgusb-devel', 'libusb1-devel', 'openssl-devel',
            'json-glib-devel', 'binutils', 'fprintd', 'libdeflate')

# Reuse the existing builder's command environment, hashing, ABI and source checks.
spec = importlib.util.spec_from_file_location('goodix_public_builder', ROOT / 'production/build-public.py')
public = importlib.util.module_from_spec(spec)
spec.loader.exec_module(public)
run, require, sha256 = public.run, public.require, public.sha256


def generate_enums(reference, include):
    """Same glib-mkenums templates used by C1/M1, with new private outputs."""
    headers = reference / 'libfprint/libfprint'
    public_headers = ['fp-context.h', 'fp-device.h', 'fp-image-device.h', 'fp-image.h', 'fp-print.h']
    private_headers = ['fpi-assembling.h', 'fpi-byte-reader.h', 'fpi-byte-utils.h',
                       'fpi-byte-writer.h', 'fpi-compat.h', 'fpi-context.h', 'fpi-device.h',
                       'fpi-image-device.h', 'fpi-image.h', 'fpi-log.h', 'fpi-minutiae.h',
                       'fpi-print.h', 'fpi-usb-transfer.h', 'fpi-ssm.h', 'fpi-spi-transfer.h']
    for name, sources in (('fp-enums.h', public_headers), ('fpi-enums.h', private_headers)):
        result = run('glib-mkenums', '--fhead', '#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
                     '--fprod', '\n/* enumerations from "@basename@" */\n', '--vhead',
                     '\nGType @enum_name@_get_type (void);\n#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type())',
                     '--ftail', '\nG_END_DECLS', *(headers / name for name in sources))
        (include / name).write_text(result + '\n')


def build_native(reference, output):
    require(run('git', '-C', reference, 'rev-parse', 'HEAD') == C1_PIN,
            'C1 reference must be at the pinned commit')
    require(not run('git', '-C', reference, 'status', '--porcelain', '--untracked-files=no'),
            'C1 tracked reference files must be unchanged')
    recipe_path = reference / 'package/native-engine/recipe.json'
    recipe = json.loads(recipe_path.read_bytes())
    for name, checksum in recipe['source_sha256'].items():
        path = reference / name
        require(path.resolve().is_relative_to(reference) and path.is_file() and
                sha256(path.read_bytes()) == checksum, 'native recipe input drift: ' + name)
    native = output / 'native'
    native.mkdir()
    include, objects = native / 'include', native / 'objects'
    include.mkdir()
    objects.mkdir()
    generate_enums(reference, include)
    commands, inputs, compiled = [], {recipe_path}, []
    # C1 owns the compiler arguments and source list. Redirect only output paths,
    # add generated enum headers/dependency records, and archive its PIC objects.
    for index, original in enumerate(recipe['commands']):
        if '-c' not in original:
            require('-shared' in original, 'unexpected noncompile native command')
            continue
        command = [part.replace('@ROOT@', str(reference)) for part in original]
        obj = objects / f'{index:03d}.o'
        depfile = obj.with_suffix('.d')
        command[command.index('-o') + 1] = str(obj)
        command += ['-I', str(include), '-MMD', '-MF', str(depfile)]
        run(*command)
        commands.append(command)
        compiled.append(obj)
        dependencies = shlex.split(depfile.read_text().replace('\\\n', ''))[1:]
        inputs.update(Path(name).resolve() for name in dependencies)
    archive = native / 'libgoodix521d-native.a'
    run('ar', 'rcsD', archive, *compiled)
    commands.append(['ar', 'rcsD', str(archive), *(str(obj) for obj in compiled)])
    (native / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    inventory = {str(path): sha256(path.read_bytes()) for path in sorted(inputs)}
    (native / 'source-files.json').write_text(json.dumps(inventory, indent=2, sort_keys=True) + '\n')
    return archive, inventory, sha256(recipe_path.read_bytes()), len(compiled)


def build_candidate(output, reference):
    require(os.geteuid() != 0, 'build as an ordinary user')
    release = platform.freedesktop_os_release()
    require(release.get('ID') == 'fedora' and release.get('VERSION_ID') == '44' and
            platform.machine() == 'x86_64', 'Fedora 44 x86_64 is required')
    run('rpm', '-q', *PACKAGES)
    output, reference = Path(output), Path(reference).resolve(strict=True)
    require(output.is_absolute() and not output.exists() and not output.is_symlink() and
            output.resolve().is_relative_to(ROOT / 'development'),
            'output must be a new absolute directory inside private development/')
    run('git', '-C', ROOT, 'check-ignore', str(output))
    run('git', '-C', ROOT, 'check-ignore', str(reference))
    require((ROOT / 'development').resolve() == ROOT / 'development', 'private area cannot be a symlink')
    inventory = public.source_inventory() + f'{sha256(Path(__file__).read_bytes())}  production/build-milan-candidate.py\n'
    os.umask(0o077)
    output.mkdir(mode=0o700, parents=True)
    archive, native_inputs, recipe_hash, count = build_native(reference, output)
    build, runtime = output / 'build', output / 'runtime'
    runtime.mkdir()
    flags = ['-DGOODIX_PRODUCTION_FPRINTD_ACTION_PROFILE',
             f'-ffile-prefix-map={ROOT}=/usr/src/goodix',
             f'-ffile-prefix-map={output}=/usr/src/goodix-milan-build']
    env = dict(public.BASE_ENV, CC='cc', CXX='c++')
    run('meson', 'setup', build, ROOT / public.LIBFPRINT_SOURCE, '--prefix=/usr', '--libdir=lib64',
        '--buildtype=release', '--wrap-mode=nodownload', '-Ddrivers=goodix_27c6_5125',
        '-Dintrospection=false', '-Ddoc=false', '-Dinstalled-tests=false',
        '-Dudev_rules=disabled', '-Dudev_hwdb=disabled', '-Dgoodix_production_minimal=true',
        '-Dgoodix_milan=true', '-Dgoodix_milan_native_archive=' + str(archive),
        '-Dgoodix_milan_native_include=' + str(reference / 'package/experiments/milan521d/native'),
        '-Dc_args=' + json.dumps(flags), '-Dcpp_args=' + json.dumps(flags), env=env)
    run('meson', 'compile', '-C', build, 'fprint-2', env=env)
    library = build / 'libfprint/libfprint-2.so.2.0.0'
    require(public.soname(library) == 'libfprint-2.so.2', 'unexpected library ABI')
    symbols = run('nm', library)
    forbidden = set((ROOT / 'production/host-test-only-symbols.txt').read_text().splitlines())
    require(not set(re.findall(r'\b(goodix_\w+)$', symbols, flags=re.M)) & forbidden,
            'host test entrypoints in candidate')
    require('sigfm' not in symbols.lower() and 'opencv' not in symbols.lower(),
            'SIGFM/OpenCV symbols in Milan candidate')
    shutil.copyfile(library, runtime / library.name)
    (runtime / 'libfprint-2.so.2').symlink_to(library.name)
    (runtime / 'libfprint-2.so').symlink_to('libfprint-2.so.2')
    required = set(re.findall(r'\b(fp_\w+)@LIBFPRINT_2\.0\.0',
                             run('nm', '-D', '--undefined-only', '/usr/libexec/fprintd')))
    provided = set(re.findall(r'\b(fp_\w+)@@LIBFPRINT_2\.0\.0',
                             run('nm', '-D', '--defined-only', library)))
    require(required and required <= provided, 'candidate does not provide stock fprintd ABI')
    dynamic = run('readelf', '-d', library)
    linked = run('ldd', '/usr/libexec/fprintd',
                 env=dict(public.BASE_ENV, LD_LIBRARY_PATH=str(runtime)))
    require(str(runtime / 'libfprint-2.so.2') in linked and 'not found' not in linked and
            'opencv' not in (dynamic + linked).lower(), 'stock fprintd dependency resolution failed')
    (output / 'stock-fprintd-ldd.txt').write_text(linked + '\n')
    (output / 'candidate-readelf.txt').write_text(dynamic + '\n')
    (output / 'source-files.sha256').write_text(inventory)
    current = public.source_inventory() + f'{sha256(Path(__file__).read_bytes())}  production/build-milan-candidate.py\n'
    require(current == inventory, 'project source content changed during the build')
    require(all(sha256(Path(name).read_bytes()) == digest for name, digest in native_inputs.items()),
            'native source content changed during the build')
    provenance = {'schema': 1, 'scope': 'private-M4-offline-noninstalled',
                  'c1_commit': C1_PIN, 'milan_commit': MILAN_PIN,
                  'native_recipe_sha256': recipe_hash, 'native_pic_objects': count,
                  'native_archive_sha256': sha256(archive.read_bytes()),
                  'library_sha256': sha256(library.read_bytes()),
                  'source_id': sha256(inventory.encode()),
                  'native_inputs_id': sha256(json.dumps(native_inputs, sort_keys=True).encode()),
                  'stock_fprintd_required_symbols': len(required),
                  'stock_fprintd_abi': 'PASS', 'sigfm_opencv_absent': True,
                  'libstdcxx_absent': 'libstdc++' not in dynamic + linked,
                  'packages': run('rpm', '-q', *PACKAGES).splitlines()}
    (output / 'build-provenance.json').write_text(json.dumps(provenance, indent=2, sort_keys=True) + '\n')
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference', type=Path, default=REFERENCE)
    args = parser.parse_args()
    provenance = build_candidate(args.output, args.reference)
    print('M4_CANDIDATE_BUILD=PASS LIBRARY_SHA256=' + provenance['library_sha256'])


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(f'M4_CANDIDATE_BUILD=STOP {error}')
