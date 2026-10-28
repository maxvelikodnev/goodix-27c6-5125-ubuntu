#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Host operations are mocked; fixtures contain no real keys or biometric data."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('helper', Path(__file__).with_name('goodix-ubuntu.py'))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)

class Lifecycle(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base = self.root / 'lib'
        self.base.mkdir()
        self.drop = self.root / 'systemd/fprintd.service.d/92-goodix.conf'
        self.drop.parent.parent.mkdir()
        self.state = self.root / 'fprint/state'
        self.state.mkdir(parents=True, mode=0o700)
        (self.state / 'preserve').write_text('synthetic sentinel')
        hashes = {}
        for name in ('fprintd', 'libfprint-2.so.2.0.0'):
            (self.base / name).write_text('synthetic ELF placeholder')
            hashes[name] = hashlib.sha256((self.base / name).read_bytes()).hexdigest()
        (self.base / 'package.json').write_text(json.dumps({'pairing_writes_enabled': True, 'sha256': hashes}))
        self.calls = []
        self.fail_start = False
        self.foreign = False
        for attr, value in [('BASE', self.base), ('DROP', self.drop), ('STATE', self.state)]:
            p = patch.object(h, attr, value)
            p.start()
            self.addCleanup(p.stop)
        p = patch.object(h, 'trusted', lambda p: None)
        p.start(); self.addCleanup(p.stop)
        p = patch.object(h, 'run', self.command)
        p.start(); self.addCleanup(p.stop)
        p = patch.object(h.platform, 'freedesktop_os_release', return_value={'ID':'ubuntu','VERSION_ID':'24.04'})
        p.start(); self.addCleanup(p.stop)
        p = patch.object(h.platform, 'machine', return_value='x86_64')
        p.start(); self.addCleanup(p.stop)
        self.systemd = self.root / 'running-systemd'
        self.systemd.mkdir()
        self.apparmor = self.root / 'apparmor-profiles'
        p = patch.object(h, 'Path', side_effect=lambda x: self.systemd if x == '/run/systemd/system' else self.apparmor if x == '/sys/kernel/security/apparmor/profiles' else Path(x))
        p.start(); self.addCleanup(p.stop)

    def command(self, *args, check=True):
        self.calls.append(args)
        if args[1] == 'show':
            field = args[args.index('-p') + 1]
            text = {'AppArmorProfile':'', 'FragmentPath':'/usr/lib/systemd/system/fprintd.service',
                    'DropInPaths': '/foreign.conf' if self.foreign else (str(self.drop) if self.drop.exists() else ''),
                    'Environment': 'LD_LIBRARY_PATH=' + str(self.base) if self.drop.exists() else ''}[field]
            return SimpleNamespace(returncode=0, stdout=text)
        if args[1] == 'start' and self.fail_start:
            self.fail_start = False
            raise RuntimeError('synthetic start failure')
        return SimpleNamespace(returncode=0, stdout='active')

    def test_enable_disable_preserves_data_and_payload(self):
        h.activate()
        self.assertEqual(self.drop.read_bytes(), h.CONFIG)
        h.deactivate()
        self.assertFalse(self.drop.exists())
        self.assertEqual((self.state / 'preserve').read_text(), 'synthetic sentinel')
        self.assertTrue((self.base / 'fprintd').exists())

    def test_failed_start_rolls_back_new_override(self):
        self.fail_start = True
        with self.assertRaisesRegex(RuntimeError, 'synthetic start failure'):
            h.activate()
        self.assertFalse(self.drop.exists())
        self.assertEqual(sum(c[1] == 'start' for c in self.calls), 2)
        self.assertTrue((self.state / 'preserve').exists())

    def test_foreign_override_is_not_stopped_or_overwritten(self):
        self.foreign = True
        with self.assertRaisesRegex(RuntimeError, 'Foreign service override'):
            h.activate()
        self.assertFalse(any(c[1] == 'stop' for c in self.calls))

    def test_modified_owned_override_is_preserved(self):
        self.drop.parent.mkdir(parents=True)
        self.drop.write_text('foreign contents')
        with self.assertRaisesRegex(RuntimeError, 'modified'):
            h.deactivate()
        self.assertEqual(self.drop.read_text(), 'foreign contents')

    def test_corrupt_payload_refuses_before_stop(self):
        (self.base / 'fprintd').write_text('changed')
        with self.assertRaisesRegex(RuntimeError, 'checksum'):
            h.activate()
        self.assertFalse(self.calls)

    def test_custom_apparmor_profile_refuses_before_stop(self):
        self.apparmor.write_text('/usr/libexec/fprintd (enforce)')
        with self.assertRaisesRegex(RuntimeError, 'AppArmor profile'):
            h.activate()
        self.assertFalse(any(c[1] == 'stop' for c in self.calls))

    def test_disabled_remove_is_idempotent(self):
        h.deactivate(); h.deactivate()
        self.assertFalse(self.calls)

if __name__ == '__main__':
    unittest.main()
