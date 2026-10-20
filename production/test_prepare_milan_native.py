#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""M5.D preparer tests: temporary metadata only; virtual UID, never sudo/USB."""
import contextlib
import importlib.util
import io
from pathlib import Path
import stat
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('native_preparer', Path(__file__).with_name('prepare-milan-diagnostic.py'))
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class NativeSessionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='goodix-m5d-metadata-')
        self.root = Path(self.temp.name)
        self.parent = self.root / 'development' / 'private'
        self.parent.mkdir(parents=True, mode=0o700)
        self.session = self.parent / 'new-native-session'
        self.real_fstat = prepare.os.fstat

    def tearDown(self):
        self.temp.cleanup()

    def virtual_root_stat(self, fd):
        actual = self.real_fstat(fd)
        return types.SimpleNamespace(st_uid=0, st_mode=actual.st_mode)

    def invoke(self, extra=(), uid=0):
        output = io.StringIO()
        with patch.object(sys, 'argv', ['prepare-milan-diagnostic.py', '--linux-native', '--session', str(self.session), *extra]), \
             patch.object(prepare.os, 'geteuid', return_value=uid), \
             patch.object(prepare.os, 'fstat', side_effect=self.virtual_root_stat), \
             patch.object(prepare.resource, 'setrlimit'), \
             patch.object(prepare.os, 'umask'), \
             patch.object(prepare, 'read_source', side_effect=AssertionError('B must never read a source')) as source, \
             contextlib.redirect_stdout(output), contextlib.redirect_stderr(io.StringIO()):
            prepare.main()
            source.assert_not_called()
        return output.getvalue()

    def test_native_creates_only_private_metadata(self):
        self.assertIn('M5D_PREPARED=PASS', self.invoke())
        self.assertEqual([p.name for p in self.session.iterdir()], ['receipt.ini'])
        self.assertEqual(stat.S_IMODE(self.session.stat().st_mode), 0o700)
        receipt = self.session / 'receipt.ini'
        self.assertEqual(stat.S_IMODE(receipt.stat().st_mode), 0o600)
        text = receipt.read_text()
        self.assertIn('purpose=M5.D_PRIVATE_LINUX_NATIVE\n', text)
        self.assertIn('source=CURRENT_SESSION_FDT_B0\n', text)
        self.assertIn('device_binding=LIVE_PREFLIGHT_REQUIRED\n', text)
        self.assertIn('prefix_semantics=NOT_APPLICABLE\n', text)
        self.assertNotIn('sha256', text)

    def test_non_root_denied_before_creation(self):
        with self.assertRaises(SystemExit) as result:
            self.invoke(uid=1000)
        self.assertEqual(result.exception.code, 2)
        self.assertFalse(self.session.exists())

    def test_a_options_never_open_source(self):
        for args in (('--source', '/must/not/be/opened'), ('--experimental-enrollment',),
                     ('--ack-binding-unknown',), ('--expected-sha256', 'a' * 64),
                     ('--expected-payload-sha256', 'b' * 64)):
            with self.subTest(args=args), self.assertRaises(SystemExit):
                self.invoke(args)
            self.assertFalse(self.session.exists())

    def test_untrusted_parent_mode_rejected(self):
        self.parent.chmod(0o755)
        with self.assertRaisesRegex(ValueError, 'mode0700'):
            self.invoke()
        self.assertFalse(self.session.exists())

    def test_untrusted_parent_owner_rejected(self):
        # No root stat override: production validation observes real fixture UID.
        with patch.object(prepare.os, 'fstat', return_value=types.SimpleNamespace(st_uid=1000, st_mode=stat.S_IFDIR | 0o700)):
            with self.assertRaisesRegex(ValueError, 'root-owned'):
                prepare.prepare_native_session(self.session)
        self.assertFalse(self.session.exists())

    def test_symlink_ancestor_rejected(self):
        alias = self.parent.parent / 'alias'
        alias.symlink_to(self.parent, target_is_directory=True)
        self.session = alias / 'new-native-session'
        with self.assertRaises(OSError):
            self.invoke()
        self.assertFalse(self.session.exists())

    def test_existing_session_is_never_overwritten(self):
        self.invoke()
        receipt = self.session / 'receipt.ini'
        before = receipt.read_bytes()
        with self.assertRaises(FileExistsError):
            self.invoke()
        self.assertEqual(before, receipt.read_bytes())


if __name__ == '__main__':
    unittest.main()
