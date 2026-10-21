#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Public, synthetic Chicago tests; never opens USB or protected data."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('chicago_native', ROOT / 'production/build-chicago-native.py')
native = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(native)

def run(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True)

def main():
    native.validate_sources(ROOT)
    with tempfile.TemporaryDirectory(prefix='goodix-chicago-test-') as directory:
        work = Path(directory)
        archive, include, provenance = native.build_native(ROOT, work, run)
        flags = run('pkg-config', '--cflags', '--libs', 'gio-2.0').split()
        count = skipped = 0
        for source in sorted((ROOT / native.NATIVE_SOURCE / 'tests').glob('*.c')):
            binary = work / source.stem
            run('cc', '-std=c11', '-O2', '-I', include, '-I', include.parent / 'common',
                source, archive, *flags, '-lm', '-o', binary)
            output = run(binary)
            count += sum(line.startswith('ok ') for line in output.splitlines())
            skipped += sum(line.startswith('ok ') and '# SKIP' in line for line in output.splitlines())
        print(f'CHICAGO_UPSTREAM_SYNTHETIC=PASS executed={count - skipped} optional_oracles_skipped={skipped}')
        env = os.environ.copy()
        env.update(GOODIX_CHICAGO_TEST='1', GOODIX_CHICAGO_NATIVE_ARCHIVE=str(archive))
        subprocess.run(['sh', str(ROOT / 'libfprint-driver/tests/run_goodix_fpimage_device_test_inner.sh'),
                        str(ROOT), str(work)], env=env, check=True)
        print('CHICAGO_ADAPTER_SYNTHETIC=PASS PRIVATE_DATA_READ=NO USB=NO')

if __name__ == '__main__':
    main()
