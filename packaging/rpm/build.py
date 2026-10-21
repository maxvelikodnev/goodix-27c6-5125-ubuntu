#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the local RPM/SRPM, optionally sign with an existing personal key."""
import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'development/rpm'
ENV = dict(os.environ, LC_ALL='C', PYTHONDONTWRITEBYTECODE='1')
SOURCE_DIRS = ('production', 'deployment', 'libfprint-driver',
               'reference/libfprint-fedora44-1.94.100', 'LICENSES', 'packaging/rpm', 'docs')
SOURCE_FILES = ('VERSION', 'install.sh', 'README.md', 'DOCUMENTATION.md', 'ACKNOWLEDGEMENTS.md')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, env=ENV, **kwargs)


def source_files(root):
    files = [root / name for name in SOURCE_FILES]
    for name in SOURCE_DIRS:
        require((root / name).resolve().is_relative_to(root.resolve())
                and all(not p.is_symlink() for p in (root / name, *(root / name).parents) if p != root.parent),
                f'redirected source directory: {name}')
        files.extend(p for p in (root / name).rglob('*') if not p.is_dir()
                     and '__pycache__' not in p.parts and p.suffix not in ('.pyc', '.pyo'))
    for path in sorted(set(files)):
        require(path.is_file() and not path.is_symlink(), f'not a regular source: {path}')
        require(path.resolve().is_relative_to(root.resolve())
                and all(not p.is_symlink() for p in path.parents), f'redirected source: {path}')
        require(path.suffix not in ('.rpm', '.so', '.o', '.a', '.bin', '.dll', '.pem', '.key'),
                f'non-source artifact in source tree: {path}')
        yield path


def archive_bytes(root, version):
    """Fixed metadata and ordering; never archive Git, development or home files."""
    out = io.BytesIO()
    with gzip.GzipFile(fileobj=out, mode='wb', mtime=0, filename='') as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as archive:
            for path in source_files(root):
                data = path.read_bytes()
                item = tarfile.TarInfo(f'goodix-manager-{version}/{path.relative_to(root).as_posix()}')
                item.size, item.mode = len(data), 0o755 if path.name == 'install.sh' else 0o644
                archive.addfile(item, io.BytesIO(data))
    return out.getvalue()


def primary_fingerprint(public_key):
    result = run('gpg', '--batch', '--with-colons', '--show-keys', public_key,
                 capture_output=True, text=True).stdout
    primary = []
    want = False
    for line in result.splitlines():
        fields = line.split(':')
        if fields[0] == 'pub':
            want = True
        elif fields[0] == 'fpr' and want:
            primary.append(fields[9].upper())
            want = False
    require(len(primary) == 1, 'public certificate must contain exactly one primary key')
    return primary[0]


def sign_and_verify(rpms, fingerprint, directory):
    fingerprint = fingerprint.upper()
    require(re.fullmatch(r'[A-F0-9]{40}|[A-F0-9]{64}', fingerprint), 'use the full public fingerprint')
    public = directory / 'Guido-Soraru-public.asc'
    exported = run('gpg', '--batch', '--armor', '--export', fingerprint, capture_output=True).stdout
    require(exported, 'selected public key is unavailable; set up the personal key first')
    public.write_bytes(exported)
    require(primary_fingerprint(public) == fingerprint, 'public fingerprint differs from requested identity')
    # The user enters the passphrase through GnuPG pinentry; never capture it.
    for rpm in rpms:
        run('rpmsign', '--define', '_openpgp_sign gpg', '--key-id', fingerprint, '--addsign', rpm)
    with tempfile.TemporaryDirectory(prefix='verify-rpmdb-', dir=directory) as temporary:
        options = ('--dbpath', temporary, '--define', '_keyring rpmdb',
                   '--define', '_pkgverify_level all', '--define', '_pkgverify_flags 0')
        run('rpmkeys', *options, '--import', public)
        keys = run('rpmkeys', *options, '--list', capture_output=True, text=True).stdout
        require(fingerprint.lower() in keys.lower(), 'isolated RPM keyring fingerprint mismatch')
        for rpm in rpms:
            run('rpmkeys', *options, '--checksig', '--verbose', rpm)
    return fingerprint


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', required=True, help='positive packaging revision for VERSION')
    parser.add_argument('--sign', metavar='FULL_FINGERPRINT', help='existing protected personal key')
    parser.add_argument('--sign-existing', type=Path, help='sign a previous successful build directory only')
    args = parser.parse_args()
    require(os.geteuid() != 0, 'build and sign as your ordinary user')
    require(re.fullmatch(r'[1-9][0-9]*', args.release), 'release must be a positive integer')
    version = (ROOT / 'VERSION').read_text().strip()
    require(re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version), 'invalid VERSION')
    require(platform.freedesktop_os_release().get('VERSION_ID') == '44'
            and platform.freedesktop_os_release().get('ID') == 'fedora'
            and platform.machine() == 'x86_64', 'only Fedora 44 x86_64 is qualified for this build')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    directory = OUTPUT / 'builds' / f'{version}-{args.release}.fc44'
    if args.sign_existing:
        require(args.sign and args.sign_existing.resolve() == directory.resolve(),
                '--sign-existing must select this VERSION/release build directory')
        record = json.loads((directory / 'result.json').read_text())
        require(record['version'] == version and record['release'] == args.release,
                'existing build identity mismatch')
        rpms = [directory / name for name in record['sha256']]
        expected = {f'rpmbuild/RPMS/x86_64/goodix-manager-{version}-{args.release}.fc44.x86_64.rpm',
                    f'rpmbuild/SRPMS/goodix-manager-{version}-{args.release}.fc44.src.rpm'}
        require(set(record['sha256']) == expected, 'expected exact binary and source RPM artifacts')
        require(all(p.resolve().is_relative_to(directory.resolve()) and p.is_file()
                    and hashlib.sha256(p.read_bytes()).hexdigest() == record['sha256'][str(p.relative_to(directory))]
                    for p in rpms), 'existing artifacts changed; refuse signing')
    else:
        require(shutil.which('rpmbuild'), 'install rpm-build first')
        require(not directory.exists(), 'build identity already reserved; increment --release, never overwrite an RPM')
        directory.mkdir(parents=True)
        top = directory / 'rpmbuild'
        for name in ('BUILD', 'BUILDROOT', 'RPMS', 'SOURCES', 'SPECS', 'SRPMS', 'tmp'):
            (top / name).mkdir(parents=True)
        data = archive_bytes(ROOT, version)
        (top / 'SOURCES' / f'goodix-manager-{version}.tar.gz').write_bytes(data)
        spec = (ROOT / 'packaging/rpm/goodix-manager.spec.in').read_text()
        spec = spec.replace('@VERSION@', version).replace('@RELEASE@', args.release)
        spec = spec.replace('@DATE@', datetime.now(timezone.utc).strftime('%a %b %d %Y'))
        specpath = top / 'SPECS/goodix-manager.spec'
        specpath.write_text(spec)
        with (directory / 'build.log').open('w') as log:
            run('rpmbuild', '-ba', '--define', f'_topdir {top}', '--define', 'dist .fc44',
                '--define', f'_tmppath {top / "tmp"}', specpath, stdout=log, stderr=subprocess.STDOUT)
        rpms = sorted((top / 'RPMS').rglob('*.rpm')) + sorted((top / 'SRPMS').glob('*.rpm'))
        require(len(rpms) == 2, 'expected exactly one binary RPM and its corresponding SRPM')
        record = dict(version=version, release=args.release,
                      source_archive_sha256=hashlib.sha256(data).hexdigest(), signature='UNSIGNED')
    record['sha256'] = {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest() for p in rpms}
    (directory / 'result.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')
    if args.sign:
        # Keep the unsigned artifacts/result intact if pinentry is cancelled or
        # any verification fails. Only fully verified signatures are published.
        with tempfile.TemporaryDirectory(prefix='signing-', dir=directory) as temporary:
            staged = [Path(temporary) / rpm.name for rpm in rpms]
            for rpm, target in zip(rpms, staged):
                shutil.copyfile(rpm, target)
            fingerprint = sign_and_verify(staged, args.sign, Path(temporary))
            for target, rpm in zip(staged, rpms):
                os.replace(target, rpm)
            shutil.copyfile(Path(temporary) / 'Guido-Soraru-public.asc', directory / 'Guido-Soraru-public.asc')
        record.update(signature='VERIFIED', fingerprint=fingerprint)
        record['sha256'] = {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest() for p in rpms}
        (directory / 'result.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')
    print(json.dumps(dict(directory=str(directory), **record), indent=2))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f'GOODIX_RPM=STOP {error}')
