<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ChicagoHS validation and limitations

ChicagoHS has passed an **initial operator-supervised live functional check**
on one Goodix USB `27c6:5125` reader in a Fedora 44 KDE virtual machine with USB
passthrough. These observations supplement the offline build and corpus evidence;
they do not establish FAR/FRR, liveness, multi-device support, or exhaustive
fault tolerance. The SIGFM results on `main` remain a separate recovery
reference, not evidence of ChicagoHS performance.

## Evidence boundaries

| Area | Offline verification | Initial live functional observation | Remaining qualification |
| --- | --- | --- | --- |
| Native backend | Pinned original ChicagoHS C sources; calibration, features, twelve-stage enrollment, Type 24 templates, verify and identify | KDE enrollment complete using native no-finger calibration; registered finger MATCH; different finger NO_MATCH | Exact physical equivalence of USB FDT_B0 and SPI T0/ImageBase, repeated reliability |
| Sensor binding | CRC-checked A6 OTP source; opaque 16-byte identity and full OTP SHA-256 envelope; malformed/mismatch rejection | Enrollment, reboot and limited Windows/Linux switching operated on one reader | Cross-device uniqueness, stability across more units and failure histories |
| Templates and users | Backend identity separation; stock fprintd per-user galleries; historical files preserved by recovery | One local user/finger was enrolled and verified after reboot | Multiple real users/fingers, corrupted gallery live paths, partial-input behavior |
| USB/TLS and pairing | Existing APP12509/APP12508 transport contracts and offline error/cleanup tests | First-use enrollment from empty host pairing state, reboot and bounded Windows/Linux ping-pong passed as reported by operator | Review live audit for E0/readback/TLS invariants; fault/recovery/timeout and APP12508 ChicagoHS |
| Installer | Canonical source build and payload validation; simulated install/update/reinstall/rollback | FIRST_INSTALL, UPDATE, reinstall, source identity and SELinux Enforcing verified in Fedora VM | Failed real installation/rollback and upgrades over legacy runtime |
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

## Reproducible software checks

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

## VM observations and subsequent live checks

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
action. Rollback uses the two ordinary recovery commands and a separate SIGFM
`main` installation, without deleting templates, host state or factory data.
