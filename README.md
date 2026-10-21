<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Goodix 27c6:5125 — Fedora KDE fingerprint driver

The canonical Goodix USB `27c6:5125` driver uses the **ChicagoHS Type 24**
biometric engine on **Fedora 44 KDE x86_64**, recognizing APP12509/APP12508
firmware. The source build integrates with Fedora's stock
libfprint/fprintd/PAM/KDE interfaces. ChicagoHS supersedes the earlier SIGFM
engine in the planned major **3.0.0** release; it is not a parallel long-term
driver edition.
An initial operator-supervised Fedora KDE VM qualification with USB passthrough
has completed enrollment, genuine/negative verification, desktop authentication,
reboot and a bounded Windows/Linux coexistence check. Broader reliability,
additional hardware profiles and fault scenarios are not yet qualified;
see [validation evidence and limits](docs/VALIDATION.md).

ChicagoHS replaces the matcher and requires new twelve-stage enrollment for each
finger/user. Historical SIGFM and Milan templates are preserved but incompatible.
There is no alternative matcher or private diagnostic path in the payload.
The former SIGFM implementation remains available through
[historical releases such as v2.2.1](https://github.com/sorguido/goodix-27c6-5125/tree/v2.2.1),
**not** through the canonical `main` branch after the ChicagoHS merge.
See the precise [qualification boundary](docs/VALIDATION.md).

## Upgrade from SIGFM 2.x to ChicagoHS 3.0.0

ChicagoHS changes the enrolled-print format and the driver identity. Existing
SIGFM templates are kept on disk, but **every user must enroll each desired
finger again** after upgrading. Host pairing state and password authentication
are preserved. The canonical installer is designed to remove a recognized
older project runtime and install ChicagoHS transactionally; manual
pre-uninstallation is not normally required. The direct migration from a
real populated SIGFM installation must still pass its final live qualification
before release. Do not assume a clean-VM install is that migration test.
An optional downgrade to the historical SIGFM release requires removing
ChicagoHS first; see [recovery](docs/UNINSTALL.md).

## Installation and recovery

Follow the [canonical installation block](docs/INSTALLATION.md). `./install.sh`
automatically selects first installation or update, builds from this source tree,
and installs the login selector and two recovery commands. No private source,
Windows file or upstream download is needed for the build. Fedora development
packages may be installed through the system package manager.

Use **`goodix-uninstall`** for ordinary removal or **`goodix-force-remove`** from
a TTY for damaged installations. Both work without the clone, network or reader,
preserve host pairing state and fingerprint templates, and remove project
software without replacing Fedora vendor files. Follow the
[recovery instructions](docs/UNINSTALL.md), including the restart instruction.

## Authentication

Manage fingerprints through KDE's fingerprint settings. A nonempty password at
Plasma Login selects password authentication; an empty submission selects the
fingerprint path, with at most three attempts and stop on the first match.
Keep password access available. Fingerprint login does not unlock a
password-encrypted KWallet. Initial live authentication paths passed on one
reader; do not infer general FAR/FRR, liveness or multi-device reliability
from these limited trials.

## Documentation

- [Installation and updates](docs/INSTALLATION.md)
- [Uninstall and emergency recovery](docs/UNINSTALL.md)
- [Validation and limitations](docs/VALIDATION.md)
- [Technical architecture](TECHNICAL_MANUAL.md)
- [Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md)
- [Security and privacy](docs/SECURITY.md)
