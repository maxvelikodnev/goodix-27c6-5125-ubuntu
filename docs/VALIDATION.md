<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ChicagoHS validation and limitations

ChicagoHS live qualification is **NOT_RUN**. Offline build, fixture, synthetic
and corpus evidence cannot establish live authentication, biometric security,
Windows coexistence or Fedora desktop behavior. The SIGFM results on `main`
are a regression reference, not ChicagoHS qualification.

## Evidence boundaries

| Area | Offline verification | Remaining qualification |
| --- | --- | --- |
| Native backend | Pinned original ChicagoHS C sources; calibration, feature, twelve-stage enrollment, Type 24 templates, verify and identify | Real USB no-finger background equivalence to SPI T0/ImageBase |
| Sensor binding | CRC-checked A6 OTP source; upstream opaque 16-byte identity plus full OTP SHA-256 binding; structural/version/CRC/mismatch rejection | Physical cross-device uniqueness and live stability |
| Templates and users | Backend identity separation; stock fprintd per-user galleries; historical data preserved | Multiple real users and fingers, genuine/impostor/partial behavior |
| USB/TLS and pairing | Existing APP12509/APP12508 transport contracts and offline error/cleanup tests | Reader reopen, timeout, cancellation, Windows recovery and reboot |
| Installer | Canonical source build and payload validation; simulated first-install/update/reinstall/rollback | Real isolated Fedora KDE VM installation |
| Removal | Temporary-filesystem normal and emergency removal, damaged/missing receipts, state/template preservation | TTY recovery and password login on Fedora VM |
| Desktop integration | PAM selector and stock fprintd ABI/dependency checks | Plasma Login, KScreenLocker, sudo, PolicyKit and password fallback |

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
Current integration limitations must be read alongside the technical manual and
must not be interpreted as a live PASS.

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

## First VM and later live checks

First qualify [installation/update/reinstall](INSTALLATION.md) in an isolated
Fedora 44 KDE VM without USB passthrough, followed by both documented
[removal/recovery](UNINSTALL.md) paths. Require preserved password login and
byte-identical preexisting host state/templates. Keep the reader disconnected
from the VM for software-only testing.

Live testing requires supervised authorization and must check sensor binding
stability and cross-device separation.
Then exercise enrollment, verify and identify, negative and wrong-user cases,
errors/cancellation, desktop entry points and Windows/reboot recovery. For each
verify/match sequence, allow at most three physical attempts, stop at the first
match and do not hide retries. A timeout, corrupted template, binding mismatch or
ambiguous state is a STOP, not an instruction to provision or reset the sensor.
Rollback uses the two ordinary recovery commands and a separate SIGFM `main`
installation, without deleting templates, host state or factory data.
