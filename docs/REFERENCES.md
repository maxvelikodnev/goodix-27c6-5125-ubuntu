<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Upstream references

## Fingerprint stack

- [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint): library base 1.94.100.
- [fprintd](https://gitlab.freedesktop.org/libfprint/fprintd): the tested Fedora
  package is 1.94.5-5.fc44; Fedora supplies the service and PAM module.
- [Linux-PAM](https://github.com/linux-pam/linux-pam): public module and dispatch APIs.
- [Plasma Login Manager](https://invent.kde.org/plasma/plasma-login-manager):
  Fedora supplies the login daemon and greeter.

The project contains the libfprint source base required for its library build.
It does not build a replacement Fedora authentication daemon or desktop.

## Matching and preprocessing

[berkekbgz/libfprint-goodix-spi](https://github.com/berkekbgz/libfprint-goodix-spi)
is the current ChicagoHS computational source, pinned at
`010a665f54089a1632b1ab7be588b316ace934e2`. The imported backend contains
calibration, preprocessing, feature extraction, enrollment, Type 24 templates,
and matching, without copying its SPI transport.

[Rockytkg's Goodix project](https://github.com/Rockytkg/goodix-linux-27c6-5125)
is a historical SIGFM/matcher reference and a continuing provenance source for
target-specific device-configuration derivation:

- preprocessing origin: `227eba219fa9e3fbac5bd59aca79f624f67cd11b`;
- materialized libfprint/SIGFM origin: `7ebe0c809b4d1df3400e84299a4ec4acdea84590`.

Reuse does not include firmware updates, provisioning, PSK replacement or the
upstream USB control path. Its behavior is not evidence for every local device.
See [Licensing and provenance](LICENSING_AND_PROVENANCE.md).

## Protocol and build references

- [RFC 5246: TLS 1.2](https://www.rfc-editor.org/rfc/rfc5246)
- [RFC 5487: PSK cipher suites](https://www.rfc-editor.org/rfc/rfc5487)
- [RFC 5288: AES-GCM cipher suites](https://www.rfc-editor.org/rfc/rfc5288)
- [OpenCV](https://opencv.org/): historical SIGFM image-processing dependency;
  **not** shipped or required by the ChicagoHS driver.
- [Fedora libfprint source provenance](../reference/libfprint-fedora44-1.94.100/PROVENANCE.md)

Source versions identify tested/build inputs. They are not runtime version pins
for password login or guarantees about later Fedora updates.
