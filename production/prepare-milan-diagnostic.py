#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare a private M5.B capture or M5.C enrollment session; never USB or pairing."""
import argparse
import hashlib
import os
from pathlib import Path
import resource
import stat
import struct
import zlib

def validate(data, expected_hash=None, expected_payload_hash=None):
    if len(data) != 140480:
        raise ValueError('exact 16 + 140464 byte envelope required')
    payload = data[16:]
    full_hash = hashlib.sha256(data).hexdigest()
    payload_hash = hashlib.sha256(payload).hexdigest()
    if expected_hash and (full_hash != expected_hash or payload_hash != expected_payload_hash):
        raise ValueError('existing M3 envelope/payload provenance hash mismatch')
    if payload[0x22490:0x22490 + 20] != b'Preprocess_v_1.01.01':
        raise ValueError('calibration version mismatch')
    for field, start in ((0, 8), (4, 0x9928)):
        if struct.unpack_from('<I', payload, field)[0] != (zlib.crc32(payload[start:start + 10240]) ^ 0xffffffff):
            raise ValueError('calibration map CRC mismatch')
    if not 0 <= struct.unpack_from('<i', payload, 0x18488)[0] <= 400:
        raise ValueError('calibration sample count range invalid')
    classifier = payload[0x1848c:0x2248c]
    if any(classifier) and (classifier[:8] != bytes.fromhex('f4f628f7f47bcc24') or
                            struct.unpack_from('<i', classifier, 8)[0] != 10001 or
                            not 0 <= struct.unpack_from('<i', classifier, 20)[0] <= 50):
        raise ValueError('classifier magic/version/history invalid')
    return full_hash, payload_hash


def open_directory(path):
    if not path.is_absolute() or '..' in path.parts or '.' in path.parts:
        raise ValueError('absolute symlink-free path required')
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    try:
        for component in path.parts[1:]:
            child = os.open(component, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=fd)
            os.close(fd)
            fd = child
        return fd
    except BaseException:
        os.close(fd)
        raise


def read_source(path, require_private=False):
    directory_fd = open_directory(path.parent)
    try:
        fd = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK, dir_fd=directory_fd)
        try:
            before = os.fstat(fd)
            if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1 or before.st_size != 140480:
                raise ValueError('source must be a single regular exact-size M3 envelope')
            if require_private and (before.st_uid != 0 or stat.S_IMODE(before.st_mode) != 0o600):
                raise ValueError('M5.C source must already be root-owned mode0600')
            data = bytearray()
            while len(data) < 140481:
                part = os.read(fd, 140481 - len(data))
                if not part:
                    break
                data.extend(part)
            after = os.fstat(fd)
            named = os.stat(path.name, dir_fd=directory_fd, follow_symlinks=False)
            fields = lambda value: (value.st_dev, value.st_ino, value.st_size, value.st_mtime_ns,
                                    value.st_ctime_ns, value.st_uid, value.st_mode, value.st_nlink)
            if fields(before) != fields(after) or fields(after) != fields(named):
                raise ValueError('source changed while reading')
            return data
        finally:
            os.close(fd)
    finally:
        os.close(directory_fd)


def write_private(fd, name, data):
    output = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600, dir_fd=fd)
    try:
        view = memoryview(data)
        while view:
            written = os.write(output, view)
            if written <= 0:
                raise OSError('short private write')
            view = view[written:]
        os.fsync(output)
    finally:
        os.close(output)


def prepare_native_session(path):
    """Only authorization metadata; no calibration/background source is opened."""
    if not path.is_absolute() or 'development' not in path.parts:
        raise ValueError('native session must be absolute inside private development/')
    parent_fd = open_directory(path.parent)
    try:
        parent = os.fstat(parent_fd)
        if parent.st_uid != 0 or stat.S_IMODE(parent.st_mode) != 0o700:
            raise ValueError('native session parent must already be root-owned mode0700')
        os.mkdir(path.name, 0o700, dir_fd=parent_fd)
        fd = os.open(path.name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=parent_fd)
        try:
            write_private(fd, 'receipt.ini',
                          b'[M5B]\nschema=1\npurpose=M5.D_PRIVATE_LINUX_NATIVE\n'
                          b'source=CURRENT_SESSION_FDT_B0\ndevice_binding=LIVE_PREFLIGHT_REQUIRED\n'
                          b'prefix_semantics=NOT_APPLICABLE\n')
            os.fsync(fd)
        finally:
            os.close(fd)
        os.fsync(parent_fd)
    finally:
        os.close(parent_fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline-check', action='store_true', help='synthetic checks only, no source files or USB')
    parser.add_argument('--source', type=Path)
    parser.add_argument('--session', type=Path)
    parser.add_argument('--expected-sha256', help='M3 envelope hash from the private qualification report')
    parser.add_argument('--expected-payload-sha256', help='M3 payload hash from the private qualification report')
    parser.add_argument('--ack-binding-unknown', action='store_true')
    parser.add_argument('--linux-native', action='store_true', help='M5.D metadata-only session; no Windows source')
    parser.add_argument('--experimental-enrollment', action='store_true',
                        help='explicit M5.C private session; current OTP equality remains mandatory at runtime')
    args = parser.parse_args()
    if args.offline_check:
        if args.source or args.session or args.ack_binding_unknown or args.experimental_enrollment or args.linux_native or args.expected_sha256 or args.expected_payload_sha256:
            parser.error('--offline-check does not accept protected-data options')
        payload = bytearray(140464)
        payload[0x22490:0x22490 + 20] = b'Preprocess_v_1.01.01'
        for field, start in ((0, 8), (4, 0x9928)):
            struct.pack_into('<I', payload, field, zlib.crc32(payload[start:start + 10240]) ^ 0xffffffff)
        validate(bytes(16) + payload)
        for offset in (16, 16 + 4, 16 + 0x22490, 16 + 0x18488 + 3, 16 + 0x1848c):
            invalid = bytearray(bytes(16) + payload)
            invalid[offset] ^= 0xff
            try:
                validate(invalid)
            except ValueError:
                continue
            raise ValueError('synthetic invalid input was accepted')
        print('M5B_PREPARE_OFFLINE=PASS PROTECTED_DATA=NOT_OPENED USB=NOT_USED')
        return
    if args.linux_native:
        if (os.geteuid() != 0 or not args.session or args.source or args.experimental_enrollment or
                args.ack_binding_unknown or args.expected_sha256 or args.expected_payload_sha256):
            parser.error('--linux-native requires root authorization and --session only; no A/source options')
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        os.umask(0o077)
        prepare_native_session(args.session)
        print('M5D_PREPARED=PASS WINDOWS_CALIDATA=NOT_USED BACKGROUND=CURRENT_SESSION_REQUIRED ENROLL_BUDGET=1 PROBE_BUDGET=3')
        return
    if (os.geteuid() != 0 or not args.source or not args.session or not args.ack_binding_unknown or
            not args.expected_sha256 or not args.expected_payload_sha256 or
            any(len(value) != 64 or any(letter not in '0123456789abcdef' for letter in value)
                for value in (args.expected_sha256, args.expected_payload_sha256))):
        parser.error('root authorization, source/session, both explicit expected SHA256 hashes and --ack-binding-unknown required')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.umask(0o077)
    if not args.source.is_absolute() or not args.session.is_absolute() or 'development' not in args.session.parts:
        parser.error('source/session must be absolute; session must be inside private development/')
    data = read_source(args.source, require_private=args.experimental_enrollment)
    try:
        full_hash, payload_hash = validate(data, args.expected_sha256, args.expected_payload_sha256)
        parent_fd = open_directory(args.session.parent)
        try:
            parent = os.fstat(parent_fd)
            if parent.st_uid != 0 or stat.S_IMODE(parent.st_mode) != 0o700:
                raise ValueError('session parent must already be root-owned mode0700 in private development/')
            os.mkdir(args.session.name, 0o700, dir_fd=parent_fd)
            fd = os.open(args.session.name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=parent_fd)
            try:
                write_private(fd, 'calidata.dat', data)
                purpose = 'M5.C_PRIVATE_ENROLL_VERIFY' if args.experimental_enrollment else 'M5.B_CAPTURE_ONLY'
                receipt = (f'[M5B]\nschema=1\npurpose={purpose}\n'
                           'source=M3_USER_DECLARED_WINDOWS_COPY\ndevice_binding=UNKNOWN\nprefix_semantics=UNKNOWN\n'
                           f'envelope_sha256={full_hash}\npayload_sha256={payload_hash}\n')
                write_private(fd, 'receipt.ini', receipt.encode())
                os.fsync(fd)
            finally:
                os.close(fd)
            os.fsync(parent_fd)
        finally:
            os.close(parent_fd)
    finally:
        data[:] = bytes(len(data))
    print('M5C_PREPARED=PASS OTP_BINDING=REQUIRED ENROLL_BUDGET=1 PROBE_BUDGET=3 SOURCE_UNCHANGED=true'
          if args.experimental_enrollment else
          'M5B_PREPARED=PASS BINDING=UNKNOWN CAPTURE_BUDGET=1 SOURCE_UNCHANGED=true')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as error:
        raise SystemExit('M5B_PREPARE=STOP ' + str(error))
