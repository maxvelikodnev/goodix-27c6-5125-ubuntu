#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Package an operational build-ready.py output. Never installs or opens USB."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
NAME = 'goodix-5125-ubuntu'
FILES = {'fprintd': 'daemon/src/fprintd',
         'libfprint-2.so.2.0.0': 'library/runtime/libfprint-2.so.2.0.0'}

def run(*args, **kwargs):
    return subprocess.check_output([str(x) for x in args], text=True, **kwargs).strip()

def write(path, text, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    path.chmod(mode)

def build(source, output, version):
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version):
        raise ValueError('Use a numeric major.minor.patch version.')
    if run('dpkg', '--print-architecture') != 'amd64':
        raise ValueError('Build on Ubuntu amd64.')
    record = json.loads((source / 'ready-build.json').read_text())
    if record.get('target') != 'ubuntu-24.04-x86_64' or record.get('pairing_writes_enabled') is not True:
        raise ValueError('Build with build-ready.py --allow-persistent-pairing first.')
    for relative in FILES.values():
        p = source / relative
        if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest() != record['sha256'][relative]:
            raise ValueError('Payload checksum mismatch: ' + relative)
    output.mkdir(parents=True, exist_ok=True)
    destination = output / f'{NAME}_{version}_amd64.deb'
    if destination.exists():
        raise ValueError('Refusing to overwrite existing package.')
    with tempfile.TemporaryDirectory(prefix='goodix-deb-') as name:
        work = Path(name)
        stage = work / 'package'
        private = stage / 'usr/lib' / NAME
        private.mkdir(parents=True)
        hashes = {}
        for filename, relative in FILES.items():
            target = private / filename
            shutil.copyfile(source / relative, target)
            target.chmod(0o755 if filename == 'fprintd' else 0o644)
            hashes[filename] = hashlib.sha256(target.read_bytes()).hexdigest()
        (private / 'libfprint-2.so.2').symlink_to('libfprint-2.so.2.0.0')
        write(private / 'package.json', json.dumps({'schema': 1, 'version': version,
              'pairing_writes_enabled': True, 'sha256': hashes}, sort_keys=True, indent=2) + '\n')
        helper = stage / 'usr/sbin/goodix-ubuntu'
        helper.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / 'ubuntu/deb/goodix-ubuntu.py', helper)
        helper.chmod(0o755)
        docs = stage / 'usr/share/doc' / NAME
        docs.mkdir(parents=True)
        for original, target in [('README.md', 'README.md'), ('ubuntu/RELEASES.md', 'RELEASES.md'),
                                 ('ubuntu/FPRINTD-PROVENANCE.md', 'FPRINTD-PROVENANCE.md'),
                                 ('docs/LICENSING_AND_PROVENANCE.md', 'LICENSING_AND_PROVENANCE.md'),
                                 ('ACKNOWLEDGEMENTS.md', 'ACKNOWLEDGEMENTS.md'),
                                 ('ubuntu/fprintd-source/COPYING', 'fprintd-COPYING'),
                                 ('production/chicago-native/NOTICES.txt', 'ChicagoHS-NOTICES.txt')]:
            shutil.copyfile(ROOT / original, docs / target)
        shutil.copytree(ROOT / 'LICENSES', docs / 'licenses')
        write(work / 'debian/control', f'Source: {NAME}\nSection: admin\nPriority: optional\nMaintainer: Maksim Velikodnev <maxvelikodnev@users.noreply.github.com>\n\nPackage: {NAME}\nArchitecture: amd64\nDescription: Experimental Ubuntu Goodix fingerprint driver\n')
        write(work / 'debian/shlibs.local', f'libfprint-2 2 {NAME}\n')
        dependencies = run('dpkg-shlibdeps', '-O', '-l' + str(private),
                           '-e' + str(private / 'fprintd'), '-e' + str(private / 'libfprint-2.so.2.0.0'), cwd=work)
        dependencies = dependencies.removeprefix('shlibs:Depends=')
        dependencies = ', '.join(x for x in dependencies.split(', ') if x.split(' ')[0] != NAME)
        debian = stage / 'DEBIAN'
        write(debian / 'control', f'''Package: {NAME}
Version: {version}
Architecture: amd64
Installed-Size: {sum(p.stat().st_size for p in stage.rglob('*') if p.is_file() and not p.is_symlink()) // 1024 + 1}
Section: admin
Priority: optional
Maintainer: Maksim Velikodnev <maxvelikodnev@users.noreply.github.com>
Depends: fprintd, python3, {dependencies}
Recommends: libpam-fprintd
Homepage: https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu
Description: Experimental Goodix 27c6:5125 fingerprint support for Ubuntu 24.04
 Private libfprint and readiness-aware fprintd based on sorguido's project.
 Installation does not activate the driver or alter PAM. Enable explicitly
 with goodix-ubuntu enable. First fingerprint use may persist pairing.
 Disable before upgrading. Removal preserves pairing and enrolled prints.
''')
        write(debian / 'preinst', '''#!/bin/sh
set -eu
case "$1" in
 install|upgrade)
  . /etc/os-release
  [ "${ID:-}" = ubuntu ] && [ "${VERSION_ID:-}" = 24.04 ] || { echo 'Ubuntu 24.04 required.' >&2; exit 1; }
  if [ -e /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf ] || [ -L /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf ]; then
   echo 'Run sudo goodix-ubuntu disable before installing or upgrading.' >&2
   exit 1
  fi
 ;;
esac
exit 0
''', 0o755)
        write(debian / 'postinst', '''#!/bin/sh
set -eu
case "$1" in
 configure) echo 'Goodix files installed, driver disabled. Run sudo goodix-ubuntu enable after reviewing /usr/share/doc/goodix-5125-ubuntu/RELEASES.md.' ;;
esac
exit 0
''', 0o755)
        write(debian / 'prerm', '''#!/bin/sh
set -eu
case "$1" in
 remove|deconfigure) /usr/sbin/goodix-ubuntu disable ;;
esac
exit 0
''', 0o755)
        write(debian / 'postrm', '''#!/bin/sh
set -eu
case "$1" in
 remove|purge)
  if [ -d /run/systemd/system ]; then systemctl daemon-reload; fi
 ;;
esac
exit 0
''', 0o755)
        write(debian / 'md5sums', ''.join(hashlib.md5(p.read_bytes()).hexdigest() + '  ' + str(p.relative_to(stage)) + '\n'
              for p in sorted(stage.rglob('*')) if p.is_file() and not p.is_symlink() and 'DEBIAN' not in p.parts))
        # Never inherit a developer's group-writable umask into root-loaded code.
        for p in stage.rglob('*'):
            if p.is_symlink():
                continue
            executable = p == helper or p == private / 'fprintd' or (p.parent == debian and p.name in ('preinst', 'postinst', 'prerm', 'postrm'))
            p.chmod(0o755 if p.is_dir() or executable else 0o644)
        stage.chmod(0o755)
        run('dpkg-deb', '--root-owner-group', '--build', stage, destination)
    (output / (destination.name + '.sha256')).write_text(hashlib.sha256(destination.read_bytes()).hexdigest() + '  ' + destination.name + '\n')
    print(destination)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    build(args.build.resolve(), args.output.resolve(), args.version)
