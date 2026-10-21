<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Acknowledgements

## Upstream projects

This ChicagoHS branch builds on libfprint, fprintd, Linux-PAM, KDE Plasma,
libgusb, OpenSSL and the open-source ChicagoHS backend, and on the work of
their contributors and Fedora maintainers. OpenCV belonged to the historical
SIGFM runtime and is not a dependency of the current ChicagoHS payload.

[Rockytkg's Goodix project](https://github.com/Rockytkg/goodix-linux-27c6-5125)
provided an **important** historical SIGFM implementation reference and the
reused device-configuration research described in
[Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md).

[Berke Kabagöz's ChicagoHS project](https://github.com/berkekbgz/libfprint-goodix-spi)
provides the current native biometric backend (calibration, preprocessing,
feature extraction, enrollment, and matching), imported at the pinned source
revision and retaining its original copyright notices.
The projects remain independently maintained and may differ in scope and
behavior. Original copyright notices are retained.

## Community contributions

Thank you to [@grold](https://github.com/grold) for two distinct contributions:

- [Issue #51](https://github.com/sorguido/goodix-27c6-5125/issues/51): reporting
  and analyzing the `IRQ 0x0080` problem when contact begins before or is already
  present during arming, affecting multi-finger enrollment and lock-screen use.
- [PR #49](https://github.com/sorguido/goodix-27c6-5125/pull/49): discovering,
  implementing, testing on real hardware, and sharing support for
  `GF_ST411SEC_APP_12508` on Goodix USB `27c6:5125`.

## Development tools

OpenAI AI and Qwen models assisted analysis, implementation, testing, documentation and
review. Human contributors retain responsibility for design decisions, hardware
validation and release approval. [Upstream references](docs/REFERENCES.md)
identify the source projects separately from those development tools.
