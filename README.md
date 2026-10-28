<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Goodix 27c6:5125 fingerprint driver for Ubuntu 24.04 LTS

Experimental Ubuntu adaptation for the **Goodix USB fingerprint reader `27c6:5125`**,
including the power-button fingerprint sensor found in some **HONOR MagicBook**
and **Huawei MateBook** laptops. Built around **libfprint**, **fprintd** and GNOME.

> **Based on [sorguido/goodix-27c6-5125](https://github.com/sorguido/goodix-27c6-5125)**
> by **Guido Sorarù (@sorguido)**, at commit
> [`4dff813`](https://github.com/sorguido/goodix-27c6-5125/commit/4dff813bb80ddedb2b801b89ced223e4e4594ca7).
> This fork adds Ubuntu build support and readiness-driven fingerprint prompts.
> The original protocol work, driver and ChicagoHS integration belong to upstream
> and its credited contributors; this is not a driver written from scratch.

## Status: tested on one Ubuntu laptop, still experimental

| Configuration | Observed result |
| --- | --- |
| Ubuntu 24.04 LTS, x86_64, GNOME; HONOR HYM-WXX | Source builds succeeded |
| Goodix `27c6:5125`, firmware `APP12508` | Pairing and image acquisition worked |
| Fingerprint registration | Completed using terminal and GNOME Settings |
| Registered finger | Successful verification observed after re-enrollment |
| Unregistered finger | One controlled rejection observed before re-enrollment |
| Local `sudo` authentication | Successful fingerprint authentication observed |
| Updated readiness prompt | Local `sudo` matched after the ready event |
| Lock screen, initial login, suspend/resume and Windows switching | Not yet qualified for this fork |

These are small functional checks, **not** biometric accuracy measurements.
False-accept/false-reject rates, spoof resistance and liveness have not been established.
Other HONOR/Huawei models, Ubuntu releases and APP12509 units remain unverified here.
An earlier enrollment did not match and had to be repeated; reliability still needs work.

## What changed from upstream?

- Ubuntu 24.04 dependency checks, build paths and checks against Ubuntu's fprintd ABI.
- A build-only workflow that uses normal user privileges and never opens the reader.
- A default build guard blocking new persistent pairing-write commands.
- A readiness event after baseline calibration and the first arm acknowledgement.
- A narrow fprintd 1.94.3 change: for this Goodix driver, the verification prompt
  waits for readiness instead of asking for a finger during preparation.
- Tests for readiness, cancellation and prompt ordering.

Preparation still takes time. The update removes the need to **count a manual
pause after the prompt**; it does not make calibration instantaneous. Keep the
finger away until requested. The graphical enrollment identify-to-enroll handoff
still needs qualification with the update.

See [Ubuntu changes and validation](ubuntu/CHANGES.md) and the detailed commit history.

## Build on Ubuntu 24.04

**Developer preview: source builds only. No signed Ubuntu package or general-purpose
Ubuntu installer is published yet. Do not run the Fedora RPM installer on Ubuntu.**

```sh
sudo apt-get install --no-install-recommends gcc meson ninja-build pkgconf \
  libglib2.0-dev libgusb-dev libusb-1.0-0-dev libssl-dev libjson-glib-dev binutils \
  fprintd libfprint-2-dev libpolkit-gobject-1-dev libdbus-1-dev gettext

git clone https://github.com/maxvelikodnev/goodix-27c6-5125-ubuntu.git
cd goodix-27c6-5125-ubuntu
python3 -B ubuntu/build-ready.py --output "$HOME/goodix-ubuntu-build"
```

Build as your normal user. Output goes to a new directory outside this checkout.
This command **does not install anything, alter PAM or access the sensor**.
New pairing writes are blocked by default. A first-time Linux pairing requires a
separate, explicitly enabled operational build and controlled installation.

[Build details and checks](ubuntu/README.md) · [Upstream Fedora README](docs/UPSTREAM-README.md)

## Persistent pairing and privacy

An operational build can write pairing material to the sensor. Uninstalling host
software does **not** guarantee restoration of the original sensor state. This fork
has no verified factory-reset or pairing-erasure procedure. The default guarded
build can reuse existing Linux pairing but fails instead of writing a new key;
Windows replacing pairing can therefore require separate investigation.

Never publish pairing-state directories, fingerprints, images, templates, raw USB
captures, credentials or private logs in an issue. Preserve password access when
experimenting. Do not relax matching thresholds to make a test pass.

## Credits and licensing

- **Guido Sorarù / @sorguido**: upstream Goodix `27c6:5125` project and integration.
- **Maksim Velikodnev / @maxvelikodnev**: this Ubuntu adaptation and supervised testing.
- **fprintd contributors**: fprintd 1.94.3, obtained from the Ubuntu source archive.
- Further protocol, libfprint and ChicagoHS contributors are preserved in
  [upstream acknowledgements](ACKNOWLEDGEMENTS.md) and
  [licensing and provenance](docs/LICENSING_AND_PROVENANCE.md).

Existing copyright and per-file license notices are retained. The upstream combined
Goodix-enabled library is conveyed under GPL-3.0-or-later; fprintd retains its
upstream GPL terms. See [LICENSE](LICENSE), [LICENSES](LICENSES), and
[fprintd source provenance](ubuntu/FPRINTD-PROVENANCE.md).

## Reporting an Ubuntu issue

Report Ubuntu release, laptop model, `lsusb` device ID, firmware version if known,
command or interface used, and redacted error text. State whether a finger was
already touching the sensor before the prompt. Do not attach biometric or pairing data.
