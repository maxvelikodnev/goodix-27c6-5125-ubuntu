#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the Ubuntu library and readiness-aware daemon, without installing either."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def run(*args):
    subprocess.run([str(x) for x in args], check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--allow-persistent-pairing', action='store_true',
                        help='Permit persistent pairing commands in the built library. Does not execute them.')
    args = parser.parse_args()
    output = args.output.expanduser().absolute()
    if output.exists() or output.is_symlink() or output.resolve().is_relative_to(ROOT):
        parser.error('Output must be a new directory outside this checkout.')
    output.mkdir(mode=0o700)
    library = output / 'library'
    command = [sys.executable, '-B', ROOT / 'ubuntu/build.py', '--output', library]
    if args.allow_persistent_pairing:
        command.append('--allow-persistent-pairing')
    run(*command)
    daemon = output / 'daemon'
    run('meson', 'setup', daemon, ROOT / 'ubuntu/fprintd-source',
        '--prefix=/usr', '--sysconfdir=/etc', '--localstatedir=/var',
        '--buildtype=release', '--wrap-mode=nodownload', '-Dpam=false',
        '-Dman=false', '-Dgtk_doc=false', '-Dsystemd=false')
    run('meson', 'compile', '-C', daemon)
    prompt_test = output / 'test-prompt'
    run('cc', '-std=c11', '-Wall', '-Wextra', '-Werror', ROOT / 'ubuntu/test_prompt.c', '-o', prompt_test)
    run(prompt_test)
    paths = [library / 'runtime/libfprint-2.so.2.0.0', daemon / 'src/fprintd']
    record = {'schema': 1, 'target': 'ubuntu-24.04-x86_64',
              'pairing_writes_enabled': args.allow_persistent_pairing,
              'installed': False, 'sensor_accessed': False,
              'sha256': {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in paths}}
    (output / 'ready-build.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')
    print('UBUNTU_READY_BUILD=PASS INSTALLED=false SENSOR_ACCESSED=false')

if __name__ == '__main__':
    main()
