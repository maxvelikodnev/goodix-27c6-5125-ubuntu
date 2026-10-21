#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
"""Fixed-path PolicyKit adapter for the canonical Goodix lifecycle backend.

The JSON boundary exposes only software status and fixed diagnostic codes.
It never returns backend output, protected material, or exception strings.
"""
from contextlib import contextmanager, redirect_stdout, redirect_stderr
import fcntl
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import resource
import signal
import stat
import subprocess
import sys

BASE = Path('/usr/lib/goodix-manager')
HELPER = Path('/usr/libexec/goodix-manager-helper')
PAYLOAD = BASE / 'payload'
BACKEND = BASE / 'backend'
PACKAGE = 'goodix-manager'
ENV = {'PATH': '/usr/sbin:/usr/bin:/sbin:/bin', 'LC_ALL': 'C'}
OPERATIONS = {'status', 'install', 'update', 'remove'}
VERSION = r'(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)'
CODES = {'ready', 'not_installed', 'legacy_unqualified', 'version_unknown',
         'up_to_date', 'update_available', 'downgrade_blocked', 'integrity_failed',
         'authorization_required', 'invalid_request', 'busy', 'operation_failed',
         'rollback_complete', 'installed', 'removed', 'runtime_changed',
         'missing_dependencies', 'source_mismatch'}


class Blocked(RuntimeError):
    pass


def require(ok, code='integrity_failed'):
    if not ok:
        raise Blocked(code)


def trusted(path, *, directory=False):
    """Every component is root-owned and cannot be redirected by a user."""
    for parent in reversed(path.parents):
        info = parent.lstat()
        require(stat.S_ISDIR(info.st_mode) and info.st_uid == 0 and info.st_gid == 0
                and not info.st_mode & 0o022)
    info = path.lstat()
    require((stat.S_ISDIR if directory else stat.S_ISREG)(info.st_mode)
            and info.st_uid == 0 and info.st_gid == 0 and not info.st_mode & 0o022)


def read_json(path):
    trusted(path)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode))
        data = stream.read(1024 * 1024 + 1)
    require(len(data) <= 1024 * 1024)
    value = json.loads(data)
    require(isinstance(value, dict))
    return value


def verified_package():
    require(Path(__file__).absolute() == HELPER)
    trusted(HELPER)
    trusted(BASE, directory=True)
    trusted(BACKEND, directory=True)
    trusted(PAYLOAD, directory=True)
    result = subprocess.run(['/usr/bin/rpm', '-V', '--noscripts', '--nodeps', PACKAGE],
                            env=ENV, capture_output=True, timeout=60)
    require(result.returncode == 0 and not result.stdout and not result.stderr)
    metadata = read_json(BASE / 'package.json')
    require(metadata.get('schema') == 1 and type(metadata.get('schema')) is int
            and re.fullmatch(VERSION, str(metadata.get('version', '')))
            and type(metadata.get('release')) is int and metadata['release'] > 0
            and metadata.get('fedora') == 44 and metadata.get('arch') == 'x86_64'
            and re.fullmatch('[0-9a-f]{64}', str(metadata.get('source_id', ''))))
    return metadata


def load_backend():
    path = BACKEND / 'deployment/install.py'
    trusted(path)
    # Explicit loading from the verified package; no cwd or user import path.
    spec = importlib.util.spec_from_file_location('goodix_rpm_install', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def version(value):
    require(isinstance(value, str) and re.fullmatch(VERSION, value), 'version_unknown')
    return tuple(int(part) for part in value.split('.'))


def sensor_present():
    # Kernel enumeration only: never open a USB device or issue a command.
    try:
        for device in Path('/sys/bus/usb/devices').iterdir():
            try:
                vendor = (device / 'idVendor').read_text().strip()
                product = (device / 'idProduct').read_text().strip()
            except FileNotFoundError:
                continue
            if (vendor, product) == ('27c6', '5125'):
                return True
        return False
    except OSError:
        return None


def response(metadata=None, **changes):
    metadata = metadata or {}
    result = {'schema': 1, 'state': 'blocked', 'runtime_version': None,
              'payload_version': metadata.get('version', ''),
              'package_version': (f"{metadata['version']}-{metadata['release']}.fc{metadata['fedora']}"
                                  if metadata else ''),
              'sensor_present': None, 'message_code': 'integrity_failed',
              'reboot_required': False, 'enrollment_required': False, 'result': 'blocked'}
    result.update(changes)
    return result


@contextmanager
def status_lock(backend):
    """Share the existing lifecycle lock without creating files or service work."""
    parent = backend.r.RUNTIME.parent
    fd = None
    try:
        if parent.exists():
            trusted(parent, directory=True)
            fd = os.open(parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            try:
                fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise Blocked('busy') from error
        yield
    finally:
        if fd is not None:
            os.close(fd)


def inspect_runtime(backend, metadata):
    """Read only ordinary software receipts/hashes; never pairing or templates."""
    r = backend.r
    with status_lock(backend):
        if not any(r.present(path) for path in backend.software_paths()):
            return response(metadata, state='absent', result='success', message_code='not_installed'), None
        r.normal_preflight()
        require(r.present(r.RUNTIME), 'integrity_failed')
        receipt = json.loads(r.read(r.RUNTIME / 'installation.json'))
        if receipt.get('matcher', 'sigfm') != 'chicago-type24':
            # Recognized software remains removable; migration needs qualification.
            return response(metadata, state='legacy', result='success',
                            message_code='legacy_unqualified', enrollment_required=True), receipt.get('source_id')
        provenance = json.loads(r.read(r.RUNTIME / 'build-provenance.json'))
        require(provenance.get('source_id') == receipt.get('source_id'))
        for record in (receipt, provenance):
            if 'version' in record:
                version(record['version'])
        runtime_version = receipt.get('version', provenance.get('version'))
        if runtime_version is None:
            return response(metadata, state='blocked', message_code='version_unknown'), receipt.get('source_id')
        require('version' not in provenance or provenance['version'] == runtime_version)
        if (runtime_version == metadata['version']
                and receipt.get('source_id') != metadata['source_id']):
            return response(metadata, state='blocked', runtime_version=runtime_version,
                            message_code='source_mismatch'), receipt.get('source_id')
        if version(runtime_version) > version(metadata['version']):
            code = 'downgrade_blocked'
        elif version(runtime_version) < version(metadata['version']):
            code = 'update_available'
        else:
            code = 'up_to_date'
        return response(metadata, state='installed', runtime_version=runtime_version,
                        result='success', message_code=code), receipt.get('source_id')


def runtime_prerequisites(backend):
    for name in backend.RUNTIME_PACKAGES:
        result = subprocess.run(['/usr/bin/rpm', '-q', name], env=ENV,
                                capture_output=True, timeout=30)
        require(result.returncode == 0, 'missing_dependencies')


def execute(operation, backend, metadata):
    current, source_id = inspect_runtime(backend, metadata)
    current['sensor_present'] = sensor_present()
    if operation == 'status':
        return current
    # Remove always invokes the normal canonical entry point, even for legacy
    # or unknown-version receipts. That entry point owns its lock and checks.
    if operation == 'remove':
        require(current['state'] != 'absent', 'not_installed')
    else:
        require(current['state'] != 'legacy', 'legacy_unqualified')
        require(current['state'] != 'blocked', current['message_code'])
        require((operation == 'install' and current['state'] == 'absent') or
                (operation == 'update' and current['state'] == 'installed'), 'runtime_changed')
        if operation == 'update':
            require(version(current['runtime_version']) < version(metadata['version']),
                    'downgrade_blocked' if version(current['runtime_version']) > version(metadata['version'])
                    else 'up_to_date')
        runtime_prerequisites(backend)
        manifest = backend.builder.validate_payload(PAYLOAD)
        require(manifest['source_id'] == metadata['source_id'])
        provenance = read_json(PAYLOAD / 'runtime/build-provenance.json')
        require(provenance.get('version') == metadata['version'])
    output, errors = io.StringIO(), io.StringIO()
    try:
        with redirect_stdout(output), redirect_stderr(errors):
            if operation == 'remove':
                original = sys.argv
                try:
                    sys.argv = [str(backend.r.NORMAL)]
                    require(backend.r.main() == 0, 'operation_failed')
                finally:
                    sys.argv = original
            else:
                # A fixed already-installed legacy location is the only possible
                # material source. No caller-supplied path or home discovery.
                backend.apply(PAYLOAD, backend.r.MATERIAL, 0, expected_source_id=source_id)
    except (Exception, KeyboardInterrupt) as error:
        # The backend emits this marker only after successful restoration.
        rolled_back = ('GOODIX_ROLLBACK=PASS PRIOR_PROJECT_SOFTWARE_RESTORED=true' in errors.getvalue()
                       and not str(error).startswith('final service cleanup incomplete'))
        try:
            after, after_source = inspect_runtime(backend, metadata)
            restored = (after['state'], after_source) == (current['state'], source_id)
        except Exception:
            restored = False
        return response(metadata, result='rolled_back' if rolled_back and restored else 'failed',
                        message_code='rollback_complete' if rolled_back and restored else 'operation_failed')
    after, _ = inspect_runtime(backend, metadata)
    require(after['state'] == ('absent' if operation == 'remove' else 'installed'), 'operation_failed')
    after.update(message_code='removed' if operation == 'remove' else 'installed',
                 reboot_required=True, enrollment_required=operation == 'install',
                 sensor_present=current['sensor_present'])
    return after


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    metadata = None
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    sys.dont_write_bytecode = True
    os.umask(0o077)
    # Convert termination into the canonical backend's rollback/cleanup path.
    def interrupted(_signal, _frame):
        raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM, interrupted)
    try:
        require(len(argv) == 1 and argv[0] in OPERATIONS, 'invalid_request')
        require(os.geteuid() == 0, 'authorization_required')
        metadata = verified_package()
        backend = load_backend()
        value = execute(argv[0], backend, metadata)
    except Blocked as error:
        code = str(error)
        value = response(metadata, message_code=code if code in CODES else 'integrity_failed')
    except (Exception, KeyboardInterrupt):
        value = response(metadata)
    print(json.dumps(value, sort_keys=True))
    return 0 if value['result'] == 'success' else 1


if __name__ == '__main__':
    sys.exit(main())
