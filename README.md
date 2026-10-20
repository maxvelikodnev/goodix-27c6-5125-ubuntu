<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Goodix 27c6:5125 — ChicagoHS branch

`porting_ChicagoHS` contains the ChicagoHS Type 24 driver for **Fedora 44 KDE
x86_64**, Goodix USB `27c6:5125` and APP12509/APP12508 firmware. The canonical
source build integrates with Fedora's stock libfprint/fprintd/PAM/KDE interfaces.
Live biometric and desktop qualification remains **NOT_RUN**; use an isolated
VM without USB passthrough for initial software validation.

ChicagoHS replaces the matcher and requires new twelve-stage enrollment for each
finger/user. Historical SIGFM and Milan templates are preserved but incompatible.
There is no alternative matcher or private diagnostic path in the payload.
`main` remains the SIGFM recovery baseline. See the precise
[qualification boundary](docs/VALIDATION.md).

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
password-encrypted KWallet. Hardware behavior must be qualified before relying
on this branch for authentication.

## Documentation

- [Installation and updates](docs/INSTALLATION.md)
- [Uninstall and emergency recovery](docs/UNINSTALL.md)
- [Validation and limitations](docs/VALIDATION.md)
- [Technical architecture](TECHNICAL_MANUAL.md)
- [Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md)
- [Security and privacy](docs/SECURITY.md)
