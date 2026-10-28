# Goodix fingerprint reader not working on Ubuntu: troubleshooting

This guide covers this fork on Ubuntu 24.04 amd64 with Goodix USB `27c6:5125`.
It does not establish support for another Goodix ID or laptop model.

## 1. Ubuntu does not detect the fingerprint reader

Run locally:

```sh
lsusb -d 27c6:5125
goodix-ubuntu status
systemctl show fprintd.service -p ExecStart -p Environment -p DropInPaths
```

- No matching USB device: inspect `lsusb`; this may be another reader or a device
  disabled at firmware level. Do not flash a different Goodix firmware.
- `goodix-ubuntu: command not found`: the `.deb` is not installed, or you are using
  the earlier manual build workflow.
- `disabled`: files are installed but the packaged driver is not activated.
  Review the pairing risks, then explicitly run `sudo goodix-ubuntu enable`.
- Foreign overrides: see the migration section below before changing anything.

`fprintd-list "$USER"` from the active local desktop can confirm whether a device
is exported. `No devices available` alone does not prove the hardware is broken.

## 2. PermissionDenied or Not Authorized over SSH

GNOME and fprintd use PolicyKit to distinguish local active sessions from remote
sessions. Start with the terminal on the notebook's active GNOME desktop, or its
fingerprint settings. A refusal over SSH is not evidence that the USB protocol
failed. Do not disable PolicyKit or grant fingerprint access globally as a fix.

## 3. Registration fails with an FDT or baseline error

The driver first measures the empty sensor. Touching during that preparation can
produce unstable readings. Remove the finger and stop the failed operation.
Check the journal before another controlled test:

```sh
journalctl -u fprintd.service -n 30 --no-pager
```

The readiness update delays **verification** prompts until calibration and the
initial arm acknowledgement. The graphical registration flow also includes a
duplicate-finger check and an identify-to-enroll transition; that updated path is
not yet qualified. Do not assume every registration error is caused by timing.
If the error occurs with no finger touching, retain the diagnostic error and
investigate instead of repeating resets or pairing writes.

## 4. verify-no-match or verify-unknown-error

`verify-no-match` means that this scan did not match the enrolled reference. Check
that you selected the same finger and wait for the actual request before touching.
An early enrollment on our test unit failed genuine matching; re-enrollment later
produced a match. That observation is not a general accuracy guarantee.

`verify-unknown-error` is not a normal non-match. Inspect the specific protocol or
preparation error. Stop on an error, timeout or ambiguous state. For a controlled
verification series allow at most three physical attempts, stop at the first
match, and do not hide additional retries in a loop.

Do not lower matcher thresholds, use dummy keys, delete pairing state, flash
firmware or repeatedly re-pair the device to obtain a passing test.

## 5. Fingerprint works in a test but not for sudo or login

The package does not enable system authentication automatically. After testing
genuine matching, unregistered-finger rejection and password fallback, use
Ubuntu's `sudo pam-auth-update` to select Fingerprint authentication if desired.
Keep password access and a working local session.

Local sudo has been observed working. Initial graphical login, lock screen and
suspend/resume have not been qualified for this fork. An enabled desktop setting
is not a successful test of those paths. Password-encrypted keyrings can still
require their password.

## 6. Foreign service override found or migration from a manual install

The packaged helper refuses unrelated fprintd overrides or environments. This is
intentional: combining private libraries and daemons from different installations
can produce a configuration that neither project tested.

Use the **original installation's rollback procedure** first. Confirm stock
fprintd is restored, then activate the `.deb`. Do not blindly delete all files
under `fprintd.service.d`, and do not remove `/var/lib/fprint` or the protected
`goodix-5125-state-v2` directory. The package does not automatically migrate
earlier hand-installed test versions.

## 7. Upgrade refused while the driver is enabled

```sh
sudo goodix-ubuntu disable
sudo apt install ./goodix-5125-ubuntu_0.1.0_amd64.deb
sudo goodix-ubuntu enable
```

Replace the example filename with the intended version. Disable-before-upgrade
prevents a running private daemon from loading files while they are replaced.
For a downgrade, use the same sequence with the older package.

## 8. The reader stops working after Windows

Windows can replace pairing. The regular package permits bounded restoration,
but this fork's Windows/Ubuntu coexistence is not qualified. A build made without
`--allow-persistent-pairing` deliberately cannot create or restore pairing.
Inspect the reported state instead of guessing which key or slot to overwrite.
Uninstalling Linux software is not a verified sensor factory-reset procedure.

## Information for a useful bug report

Include Ubuntu version, kernel version, laptop model, exact USB ID, firmware if
known, package/build version, interface used, and redacted error text. State which
finger was selected and whether it touched before the request. Describe the
expected result and actual result separately.

**Do not upload** passwords, pairing files, keys, raw USB captures, fingerprint
images, enrolled templates or a complete unredacted journal. Our CI uses synthetic
fixtures and cannot reproduce your physical fingerprint or certify the sensor.

[Compatibility and quick start](../README.md) · [Package installation and rollback](RELEASES.md)
