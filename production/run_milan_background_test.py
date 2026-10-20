#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build/run the synthetic background API proof using an existing native archive.

The executable also accepts --background-stdin for an independently authorized,
provenance-checked no-finger image; this runner only uses synthetic data.
"""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    native = root / 'milan-native/package/experiments/milan521d/native'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs',
                                     'glib-2.0', 'gobject-2.0'], text=True).split()
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(native),
                    str(root / 'test_milan_background.c'), str(args.native_archive),
                    '-o', str(args.output), *flags, '-l:libdeflate.so.0', '-lm'], check=True)
    subprocess.run([str(args.output.resolve())], check=True)
    for data in (b'', b'\x00' * 10239, b'\x00' * 10241, b'\xff' * 10240):
        result = subprocess.run([str(args.output.resolve()), '--background-stdin'],
                                input=data, capture_output=True)
        if result.returncode != 2:
            raise RuntimeError('Malformed background was not rejected')
    print('MALFORMED_BACKGROUND_REJECTED=PASS (missing, short, trailing, non-12-bit)')


if __name__ == '__main__':
    main()
