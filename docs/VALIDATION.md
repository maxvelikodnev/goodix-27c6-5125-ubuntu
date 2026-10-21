<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Validation scope and known limitations

## Tested configuration

Initial operator-supervised functional testing concerns one Goodix USB
`27c6:5125` reader, Fedora 44 KDE x86_64 with SELinux Enforcing and local
accounts, using stock fprintd, PAM and KDE authentication consumers. The
reader was passed through to a Fedora VM and also used with a Windows VM.
Software installation and removal were separately exercised on an isolated
Fedora VM without reader access.

These results supplement offline build and biometric corpus checks. They
do not establish FAR/FRR, liveness, multi-reader compatibility or exhaustive
fault tolerance. Firmware APP12508 is recognized by the software but has not
undergone this reader's full biometric qualification.

## Functional qualification and remaining limits

| Area | Offline verification | Initial live functional observation | Remaining qualification |
| --- | --- | --- | --- |
| Native backend | Pinned original ChicagoHS C sources; calibration, features, twelve-stage enrollment, Type 24 templates, verify and identify | KDE enrollment complete using native no-finger calibration; registered finger MATCH; different finger NO_MATCH | Exact physical equivalence of USB FDT_B0 and SPI T0/ImageBase, repeated reliability |
| Sensor binding | CRC-checked A6 OTP source; opaque 16-byte identity and full OTP SHA-256 envelope; malformed/mismatch rejection | Enrollment, reboot and limited Windows/Linux switching operated on one reader | Cross-device uniqueness, stability across more units and failure histories |
| Templates and users | Backend identity separation; stock fprintd per-user galleries; historical files preserved by recovery | One local user/finger was enrolled and verified after reboot | Multiple real users/fingers, corrupted gallery live paths, partial-input behavior |
| USB/TLS and pairing | Existing APP12509/APP12508 transport contracts and offline error/cleanup tests | First-use enrollment from empty host pairing state, reboot and bounded Windows/Linux ping-pong passed as reported by operator | Review live audit for E0/readback/TLS invariants; fault/recovery/timeout and APP12508 ChicagoHS |
| Installer | Canonical source build and payload validation; simulated install/update/reinstall/rollback, including SIGFM receipt migration | FIRST_INSTALL, UPDATE of ChicagoHS, reinstall, source identity and SELinux Enforcing verified in clean Fedora VM | Real SIGFM 2.x → ChicagoHS 3.0.0 update with preexisting templates/pairing material, failed update and rollback |
| Removal | Simulated normal and emergency removal, damaged/missing receipts, state/template preservation | Normal remove and recovery from TTY succeeded; subsequent password login and vendor fprintd restored | Real damaged-install recovery; byte-for-byte before/after comparison of preexisting templates/host state |
| Desktop integration | PAM selector and stock fprintd ABI/dependency checks | KScreenLocker, sudo, PolicyKit, Plasma Login, reboot login, three failed fingerprint attempts then password fallback PASS | Broader policy/system versions and repeated cold-start latency/reliability |

The existing private CH0 corpus established 35/35 admissions, twelve enrollment
stages in twenty contacts, three genuine matches, three impostor non-matches and
two partial non-matches with the original pinned backend. That small offline set
is neither an FAR/FRR estimate nor liveness or live-device qualification. The
public tree contains no real corpus, biometric template, sensorID, OTP or PSK fixture;
public unit tests use synthetic data.

The runtime obtains the opaque 16-byte upstream identity from CRC-checked A6
OTP and also binds the print envelope to the full OTP SHA-256. This is not a
claim that a sixteen-byte prefix alone proves physical uniqueness. Missing,
invalid or mismatched binding fails closed; no synthetic sensorID, Windows file,
alternate matcher or diagnostic-only flag is accepted in production.
The table distinguishes reported, bounded live observations from contracts
supported only by software checks. A successful initial live path is not a
general release qualification or proof of every fail-closed recovery path.

## Offline evidence and reproducible checks

From a complete source tree, the public tests use no hardware:

```text
python3 -B production/test_build_public.py
python3 -B deployment/test_install.py
python3 -B deployment/test_install_selection.py
python3 -B deployment/test_bootstrap.py
python3 -B deployment/recovery/test_remove.py
```

Builder tests use synthetic compiler/package output to exercise contract errors.
Deployment/recovery tests use temporary directories and mocked host operations;
they neither install on the owner host nor prove Fedora runtime behavior. A real
canonical build validates actual C compilation, shared-library ABI and dependency
resolution independently of those fixtures. See [production](../production/README.md).

## Installation and live-device qualification

On 21 October 2026 the operator reported the following **initial bounded
functional observations** from a clean Fedora 44 KDE VM. Without USB passthrough,
installation, update, reinstall, normal removal, emergency TTY removal and
password recovery succeeded; the obsolete `dontaudit` module was absent and
SELinux remained Enforcing. With the Goodix assigned exclusively to the VM,
KDE enrollment from an initially empty host pairing state completed; a
registered finger returned `verify-match` and a different finger returned
`verify-no-match`. KScreenLocker, sudo, PolicyKit and Plasma Login succeeded.
Login after reboot, Windows/Linux switching, and password fallback after three
failed fingerprint attempts were also reported PASS. These are operator
observations; raw audit traces for E0/readback/TLS and an independent repeat
of the hardware tests were not supplied with this documentation revision.

For reproducible software-only validation, leave USB passthrough disabled,
install from the [documented canonical block](INSTALLATION.md), then exercise
update, reinstall and both [removal/recovery](UNINSTALL.md) paths. Preserve
password access. For preservation claims involving previously populated
templates or host state, collect and compare authorized protected before/after
evidence; a clean VM does not demonstrate byte-level preservation.

Additional live qualification requires supervised authorization and should
cover sensor binding stability and cross-device separation, multiple users and
fingers, bad captures and false-positive risk, errors/cancellation, explicit
reopen, cold starts, and Windows/reboot recovery. For each verify/match series,
allow at most three physical attempts and stop at the first match. A timeout,
corrupted template, binding mismatch or ambiguous state is a STOP, not an
instruction to provision or reset the sensor. The pairing journal/readback/E0
contract must be audited separately from the observed success of a fingerprint
action. ChicagoHS is the canonical engine intended for `main`; it is not an
alternative branch to SIGFM. The normal installer is designed to upgrade a
recognized SIGFM installation transactionally, retaining pairing state and
historical templates but requiring new ChicagoHS enrollment. This direct
migration still requires a populated real-installation validation before
3.0.0. An optional return to SIGFM uses the two ordinary recovery commands
and a separate pinned historical checkout such as `v2.2.1`, **not `main`**,
without deleting templates, host state or factory data.

## Earlier driver qualification

The full test and integration record of the former SIGFM-based Goodix driver
is preserved in the [historical validation archive](VALIDATION_SIGFM_LEGACY.md).
That record documents the preceding implementation; its recognition statistics,
enrollment acceptance and pairing observations must not be represented as
biometric qualification of the current engine.

The change in template format introduced for version 3.0.0 requires new
fingerprint enrollment. A direct upgrade from a real, populated 2.x
installation, including fail-closed rollback and preservation checks, remains
a release gate. The reference for an optional older-version installation is
the historical `v2.2.1` tag, not the current `main` branch.

