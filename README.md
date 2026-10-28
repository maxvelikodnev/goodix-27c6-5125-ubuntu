<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Goodix 27c6:5125 Fingerprint Driver for Ubuntu 24.04 LTS

[![Ubuntu package build](https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu/actions/workflows/ubuntu-release.yml/badge.svg)](https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu/actions/workflows/ubuntu-release.yml)

**Experimental Linux fingerprint support for the Goodix USB reader `27c6:5125`
on Ubuntu 24.04 LTS (amd64), with GNOME enrollment, verified local `sudo`
authentication, `.deb` packaging and prompts that wait for sensor readiness.**

This power-button fingerprint sensor is found in some **HONOR MagicBook** and
**Huawei MateBook** laptops. Check the USB ID before trying the driver: the laptop
brand alone does not establish compatibility. Hardware testing covers **one HONOR
HYM-WXX with APP12508 firmware**; broad model support is not claimed.

**Based on [sorguido/goodix-27c6-5125](https://github.com/sorguido/goodix-27c6-5125)
by Guido Sorarù (@sorguido).** This fork adds Ubuntu builds, packaging and prompt
synchronization to the author's existing driver and ChicagoHS integration.
Original authorship, upstream history and per-file licenses are preserved.

[Check your sensor](#check-your-goodix-fingerprint-sensor) ·
[Compatibility](#ubuntu-and-laptop-compatibility) ·
[Install](#install-the-ubuntu-deb-package) ·
[Build](#build-from-source-on-ubuntu-2404) ·
[Troubleshooting](ubuntu/TROUBLESHOOTING.md) ·
[FAQ](#frequently-asked-questions) ·
[По-русски](#драйвер-отпечатка-goodix-для-ubuntu)

## Check your Goodix fingerprint sensor

If your **fingerprint reader is not working on Ubuntu**, first identify it:

```sh
lsusb -d 27c6:5125
```

A matching device typically appears as:

```text
ID 27c6:5125 Shenzhen Goodix Technology Co.,Ltd. Goodix Fingerprint Device
```

No output? Run `lsusb` and check the complete list. A different Goodix USB ID
requires its own driver investigation. Do not change IDs or flash firmware to
make another reader match this project.

## Ubuntu and laptop compatibility

| Component or feature | Status in this Ubuntu fork |
| --- | --- |
| Operating system | Ubuntu **24.04 LTS**, x86_64 / amd64 |
| Desktop tested | GNOME |
| Laptop tested | HONOR **HYM-WXX** |
| Fingerprint reader | Goodix USB **`27c6:5125`** |
| Firmware tested | **APP12508** (`GF_ST411SEC_APP_12508`) |
| Register a fingerprint | Completed through the terminal and GNOME Settings |
| Verify an enrolled finger | Successful match observed after re-enrollment |
| Reject an unregistered finger | One rejection observed before that re-enrollment |
| `sudo` fingerprint authentication | Successful local authentication observed |
| Request finger only after readiness | Successful local `sudo` test with the updated daemon |
| `.deb` installation and removal | Checked in disposable Ubuntu containers with simulated service control |
| Initial login, lock screen, suspend/resume, Windows dual boot | **Not yet qualified for this fork** |
| Other laptop models, distributions and APP12509 units | **Not verified here** |

The upstream driver recognizes APP12508 and APP12509, but recognition in code is
not the same as testing this fork on both revisions. An earlier enrollment did
not match and had to be repeated. These limited functional checks do not establish
false-accept/false-reject rates, liveness detection or spoof resistance.

## Install the Ubuntu .deb package

The regular package permits initial Linux pairing. **First fingerprint use can
write pairing material to the reader; uninstalling does not restore factory
pairing.** The package itself installs without accessing the sensor, automatically
enabling the driver, enrolling fingers or modifying PAM.

1. Obtain a package and checksum from [GitHub Releases](https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu/releases)
   when a release is published. If there is no release yet, download the
   `ubuntu-package` artifact from a successful [Actions build](https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu/actions/workflows/ubuntu-release.yml)
   or build locally. GitHub artifact downloads require signing in; artifacts expire.
2. Extract the artifact if needed. Verify its checksum, then install the `.deb`.
3. Enable explicitly after reviewing the pairing and compatibility limits.

For example, for version `0.1.0` (replace filenames with the actual version):

```sh
sha256sum -c goodix-5125-ubuntu_0.1.0_amd64.deb.sha256
sudo apt install ./goodix-5125-ubuntu_0.1.0_amd64.deb
goodix-ubuntu status
sudo goodix-ubuntu enable
```

Checksums detect corruption; they are not an independent signature. Packages are
currently unsigned. Existing hand-installed drivers or foreign fprintd overrides
must be rolled back with their original tools first. Do not delete pairing files
to migrate. **The Fedora RPM installer and Goodix Manager are not Ubuntu installers.**

See [installation, upgrades, rollback and release details](ubuntu/RELEASES.md).

### Register and verify a fingerprint

On a local GNOME desktop, open **Settings → System → Users → Fingerprint Login**,
or use the terminal:

```sh
fprintd-enroll -f right-index-finger
fprintd-verify -f right-index-finger
```

Keep your finger away until it is requested. During verification, the paired
Ubuntu daemon waits for the driver's readiness signal before issuing the request;
there is no manual two-second countdown after that request. The updated graphical
enrollment handoff still needs qualification.

A successful check reports `verify-match`. Verify both your enrolled finger and
an unregistered finger before relying on fingerprint authentication. Keep password
access. To opt into Ubuntu's system authentication after testing, use
`sudo pam-auth-update` and select **Fingerprint authentication**. This fork has
confirmed local `sudo`; initial graphical login and lock screen remain unqualified.

### Disable, uninstall or upgrade

```sh
sudo goodix-ubuntu disable
sudo apt remove goodix-5125-ubuntu
```

Disabling restores stock fprintd. Removal preserves pairing, enrolled prints and
PAM settings. For an upgrade or downgrade: **disable → install the desired package
→ enable**. Enabled package upgrades are refused before replacing the payload.

## Build from source on Ubuntu 24.04

```sh
sudo apt-get install --no-install-recommends gcc meson ninja-build pkgconf \
  libglib2.0-dev libgusb-dev libusb-1.0-0-dev libssl-dev libjson-glib-dev binutils \
  fprintd libfprint-2-dev libpolkit-gobject-1-dev libdbus-1-dev gettext dpkg-dev

git clone https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu.git
cd goodix-27c6-5125-ubuntu

python3 -B ubuntu/build-ready.py \
  --output "$HOME/goodix-release-build" --allow-persistent-pairing
python3 -B ubuntu/build-deb.py \
  --build "$HOME/goodix-release-build" --output "$HOME/goodix-dist" --version 0.1.0
```

Run the build as your normal user with a **new output directory outside the
checkout**. It does not install, alter PAM or communicate with the reader.
`--allow-persistent-pairing` creates the regular operational build. Omitting it
builds a guarded library that cannot create new sensor pairing; that guarded
output is not accepted by the regular `.deb` packager.

[Build checks](ubuntu/README.md) · [Release workflow](.github/workflows/ubuntu-release.yml)

## Fingerprint not working? Common Ubuntu errors

| Symptom | What to check next |
| --- | --- |
| `No devices available` | Confirm `27c6:5125`, Ubuntu version and whether the packaged driver is enabled. |
| `Not Authorized` / `PermissionDenied` over SSH | Try from the active local GNOME session; remote authorization differs from local authorization. |
| `verify-no-match` | The image did not match. Stop the test series and check enrollment; do not lower matching thresholds. |
| `verify-unknown-error` or FDT/baseline error | Remove the finger and inspect the preparation error. Do not reset or re-pair repeatedly. |
| `Foreign service override found` | Roll back the previous manual driver using its original procedure. |
| Upgrade says to disable first | Run `sudo goodix-ubuntu disable`, upgrade, then enable again. |
| No fingerprint option in GNOME Settings | Check detection and activation first; installed files alone do not enable the reader. |

[Full troubleshooting guide and diagnostic commands](ubuntu/TROUBLESHOOTING.md).

## What this fork changes from upstream

- Ubuntu 24.04 package checks, paths, ABI/dependency validation and source-build manifests.
- An operational `.deb` with explicit activation, rollback and retained password settings.
- Optional build-time blocking of persistent pairing writes.
- A readiness event after baseline calibration and the first sensor-arm acknowledgement.
- A narrow fprintd 1.94.3 change: for this Goodix driver, verification prompts wait
  for readiness. The original ChicagoHS matcher and thresholds are retained.
- Offline cancellation, prompt-ordering and package-lifecycle checks, plus GitHub
  Actions builds and prereleases for deliberate `vX.Y.Z-ubuntu` tag pushes.

[Detailed changes and test observations](ubuntu/CHANGES.md) ·
[Original Fedora README](docs/UPSTREAM-README.md) ·
[Original upstream base commit](https://github.com/sorguido/goodix-27c6-5125/commit/4dff813bb80ddedb2b801b89ced223e4e4594ca7)

## Frequently asked questions

### Does this support every HONOR MagicBook or Huawei MateBook?

No. The exact reader must be `27c6:5125`; only one HONOR HYM-WXX / APP12508 unit
has been tested here. Similar laptop names can contain different sensors.

### Does it work with Windows Hello and Ubuntu dual boot?

That has not been qualified for this fork. The upstream protocol supports
replacement/restoration of pairing, but Windows compatibility cannot be guaranteed
from our Ubuntu tests. No factory-reset or pairing-erasure procedure is verified.

### Is it a kernel module? Do I need to disable Secure Boot?

It is a userspace libfprint/fprintd implementation, not an out-of-tree kernel
module. Disabling Secure Boot is not part of these installation instructions.

### Why did the original driver require a pause before touching the sensor?

Calibration and the protected connection were still being prepared after the
initial request. This fork synchronizes verification prompts with actual readiness.
Preparation still takes time; keep the finger away until requested.

### Is fingerprint login as safe as a password?

This project has not measured biometric security or spoof resistance. A successful
match is a functional observation, not a security certification. Retain a working
password and do not treat the reader as proven protection against impersonation.

## Драйвер отпечатка Goodix для Ubuntu

Экспериментальная поддержка сканера **Goodix `27c6:5125`** для **Ubuntu 24.04 LTS**.
Проверено на одном ноутбуке **HONOR HYM-WXX**, прошивка **APP12508**: регистрация
пальца, совпадение отпечатка и подтверждение `sudo`. Поддержка всех моделей
HONOR MagicBook и Huawei MateBook не заявляется — сначала проверьте USB ID.

Если Ubuntu не видит сканер отпечатка в кнопке питания или показывает
`No devices available`, начните с `lsusb -d 27c6:5125` и раздела ошибок выше.
Пакет `.deb` требует отдельного включения: `sudo goodix-ubuntu enable`.
Для отключения: `sudo goodix-ubuntu disable`. Первое использование может записать
сопряжение в сенсор; удаление пакета не возвращает его заводское состояние.

За основу взят [проект Guido Sorarù (@sorguido)](https://github.com/sorguido/goodix-27c6-5125).
Наши дополнения: сборка под Ubuntu, пакет, тесты и запрос пальца после готовности.

## Contributing and reporting a problem

Include Ubuntu release, laptop model, USB ID, firmware version if known, interface
used and the exact redacted error. State whether the finger was already touching
before the request. Keep reports reproducible and distinguish observed results
from assumptions. Ubuntu-specific results belong to this fork; do not assume the
original author supports our changes.

**Never upload pairing keys/state, fingerprint images/templates, raw USB captures,
credentials or unredacted personal journals.**

## Credits and license

- **Guido Sorarù (@sorguido)** — original Goodix `27c6:5125` project and integration.
- **Maksim Velikodnev (@maxvelikodnev)** — Ubuntu adaptation and supervised testing.
- **fprintd contributors** — fprintd 1.94.3 from the Ubuntu source archive.
- Protocol, libfprint and ChicagoHS contributors — see the preserved
  [acknowledgements](ACKNOWLEDGEMENTS.md) and [licensing/provenance](docs/LICENSING_AND_PROVENANCE.md).

Original per-file copyright and license notices remain. The upstream combined
Goodix-enabled library is conveyed under GPL-3.0-or-later; fprintd retains its
upstream GPL terms. [LICENSE](LICENSE) · [LICENSES](LICENSES) ·
[fprintd source provenance](ubuntu/FPRINTD-PROVENANCE.md).
