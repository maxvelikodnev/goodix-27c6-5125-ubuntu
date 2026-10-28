#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit activation of the packaged Goodix driver. Does not alter PAM."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import stat
import subprocess
import tempfile

BASE = Path('/usr/lib/goodix-5125-ubuntu')
DROP = Path('/etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf')
STATE = Path('/var/lib/fprint/goodix-5125-state-v2')
CONFIG = (b'[Service]\nEnvironment=LD_LIBRARY_PATH=/usr/lib/goodix-5125-ubuntu\n'
          b'ExecStart=\nExecStart=/usr/lib/goodix-5125-ubuntu/fprintd\n')
ENV = {'PATH': '/usr/sbin:/usr/bin:/sbin:/bin', 'LC_ALL': 'C'}

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def run(*args, check=True):
    result = subprocess.run(args, env=ENV, text=True, capture_output=True, timeout=40)
    require(not check or result.returncode == 0, result.stderr.strip() or 'Command failed: ' + args[0])
    return result

def trusted(path):
    for p in (path, *path.parents):
        s = p.lstat()
        require(not stat.S_ISLNK(s.st_mode) and s.st_uid == 0 and not s.st_mode & 0o022,
                'Untrusted path: ' + str(p))

def configured():
    if not DROP.exists() and not DROP.is_symlink():
        return False
    trusted(DROP)
    require(DROP.read_bytes() == CONFIG, 'Override modified; refusing to overwrite or remove it.')
    return True

def validate_payload():
    trusted(BASE / 'package.json')
    manifest = json.loads((BASE / 'package.json').read_text())
    require(manifest['pairing_writes_enabled'] is True, 'Operational package expected.')
    require(set(manifest['sha256']) == {'fprintd', 'libfprint-2.so.2.0.0'}, 'Unexpected payload inventory.')
    for name, digest in manifest['sha256'].items():
        p = BASE / name
        trusted(p)
        require(hashlib.sha256(p.read_bytes()).hexdigest() == digest, 'Payload checksum mismatch: ' + name)

def activate():
    release = platform.freedesktop_os_release()
    require(release.get('ID') == 'ubuntu' and release.get('VERSION_ID') == '24.04'
            and platform.machine() == 'x86_64', 'Ubuntu 24.04 amd64 required.')
    require(Path('/run/systemd/system').is_dir(), 'A running systemd host is required.')
    validate_payload()
    trusted(Path('/etc/systemd/system'))
    present = configured()
    unit = run('systemctl', 'show', 'fprintd.service', '-p', 'FragmentPath', '--value').stdout.strip()
    require(unit == '/usr/lib/systemd/system/fprintd.service', 'Stock Ubuntu fprintd unit required.')
    overrides = run('systemctl', 'show', 'fprintd.service', '-p', 'DropInPaths', '--value').stdout.split()
    require(all(x == str(DROP) or x.startswith('/usr/lib/systemd/system/') for x in overrides),
            'Foreign service override found. Restore stock fprintd before enabling this package.')
    environment = run('systemctl', 'show', 'fprintd.service', '-p', 'Environment', '--value').stdout.strip()
    expected = 'LD_LIBRARY_PATH=' + str(BASE) if present else ''
    require(environment == expected, 'Foreign service environment found.')
    profile = run('systemctl', 'show', 'fprintd.service', '-p', 'AppArmorProfile', '--value').stdout.strip()
    require(not profile, 'Custom AppArmor service profile requires separate review.')
    profiles = Path('/sys/kernel/security/apparmor/profiles')
    if profiles.exists():
        require('fprintd' not in profiles.read_text().lower(),
                'Loaded fprintd AppArmor profile requires separate review.')
    was_active = run('systemctl', 'is-active', 'fprintd.service', check=False).returncode == 0
    run('systemctl', 'stop', 'fprintd.service')
    created = False
    try:
        if not STATE.parent.exists():
            STATE.parent.mkdir(mode=0o700)
        trusted(STATE.parent)
        if not STATE.exists():
            STATE.mkdir(mode=0o700)
        trusted(STATE)
        require(stat.S_IMODE(STATE.stat().st_mode) == 0o700, 'Pairing directory must have mode 0700.')
        DROP.parent.mkdir(mode=0o755, exist_ok=True)
        trusted(DROP.parent)
        if not present:
            fd, temporary = tempfile.mkstemp(prefix='.goodix-', dir=DROP.parent)
            try:
                with os.fdopen(fd, 'wb') as stream:
                    stream.write(CONFIG)
                    stream.flush()
                    os.fchmod(stream.fileno(), 0o644)
                    os.fsync(stream.fileno())
                os.link(temporary, DROP, follow_symlinks=False)
                created = True
            finally:
                Path(temporary).unlink(missing_ok=True)
        run('systemctl', 'daemon-reload')
        run('systemctl', 'start', 'fprintd.service')
        require(run('systemctl', 'is-active', 'fprintd.service').stdout.strip() == 'active', 'Daemon did not start.')
    except BaseException:
        if created:
            run('systemctl', 'stop', 'fprintd.service', check=False)
            DROP.unlink(missing_ok=True)
        run('systemctl', 'daemon-reload', check=False)
        if was_active:
            run('systemctl', 'start', 'fprintd.service', check=False)
        raise
    print('Enabled. First fingerprint use may persist pairing. PAM was not changed.')

def deactivate():
    if not configured():
        print('Already disabled.')
        return
    was_active = run('systemctl', 'is-active', 'fprintd.service', check=False).returncode == 0
    run('systemctl', 'stop', 'fprintd.service')
    DROP.unlink()
    run('systemctl', 'daemon-reload')
    if was_active:
        run('systemctl', 'start', 'fprintd.service')
    print('Disabled; stock fprintd restored. Pairing and fingerprints preserved.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('status', 'enable', 'disable'))
    args = parser.parse_args()
    if args.action == 'status':
        print('enabled' if configured() else 'disabled')
        return
    require(os.geteuid() == 0, 'Run enable/disable with sudo.')
    trusted(Path('/run'))
    fd = os.open('/run/goodix-5125-ubuntu.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'a') as lock:
        s = os.fstat(lock.fileno())
        require(stat.S_ISREG(s.st_mode) and s.st_uid == 0 and not s.st_mode & 0o077, 'Unsafe lifecycle lock.')
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        (activate if args.action == 'enable' else deactivate)()

if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
