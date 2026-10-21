<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Acknowledgements

## Upstream projects

This ChicagoHS branch builds on libfprint, fprintd, Linux-PAM, KDE Plasma,
libgusb, OpenSSL and independent open-source fingerprint research. We thank
their contributors and Fedora maintainers for the software and interfaces
that make integration with the stock authentication stack possible.
OpenCV belonged to the historical SIGFM runtime; it is not a dependency of
the current ChicagoHS payload.

### ChicagoHS — Berke Kabagöz

Special thanks to **[Berke Kabagöz (@berkekbgz)](https://github.com/berkekbgz)**
for publishing **[libfprint-goodix-spi](https://github.com/berkekbgz/libfprint-goodix-spi)**,
including an inspectable, independently implemented ChicagoHS biometric
backend. The project's reconstruction of the original Windows algorithm,
its source-level documentation, provenance records, and differential
verification gave this port a practical and technically grounded
starting point.

The current Goodix USB `27c6:5125` ChicagoHS branch directly incorporates
the upstream **computational backend**, pinned to commit
[`010a665f`](https://github.com/berkekbgz/libfprint-goodix-spi/tree/010a665f54089a1632b1ab7be588b316ace934e2).
This includes native no-finger calibration generation, preprocessing,
feature extraction, the original twelve-stage enrollment/position policy,
Type 24 templates, matching and relevant computing helpers. Our project
integrates those sources with its separately developed USB/TLS, secure
pairing and Fedora/libfprint lifecycle. It does **not** import the
upstream SPI transport, proprietary Windows binaries, firmware, or
upstream fprintd modifications.

We are particularly grateful that the source and licensing/provenance
information are public and detailed enough to permit careful adaptation
rather than speculative reimplementation. Upstream copyright and license
notices remain authoritative and are retained in the imported files;
see [Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md).
The Linux implementations remain separate projects with distinct
hardware-validation boundaries; successful tests on one device are not
attributed to the other author's testing.

### Milan Type 17 — FreshHomie

Special thanks also to **[FreshHomie (@FreshHomie)](https://github.com/FreshHomie)**
for publishing **[goodix521d-fprintd](https://github.com/FreshHomie/goodix521d-fprintd)**,
the buildable native Milan Type 17 engine and the extensive research
supporting its preprocessing, enrollment, matching and template model.
That work enabled a serious investigation of a native alternative to
the original SIGFM matcher on our `27c6:5125`.

We especially appreciate the detailed, candid and technically
specific guidance provided in
**[our cross-device feasibility discussion](https://github.com/FreshHomie/goodix521d-fprintd/issues/1)**.
The explanation of Windows engine profiles, limits of parameter-row
and algorithm-type identification, comparison methodology, preprocessing
metadata, enrollment policies and the dangers of assuming cross-sensor
equivalence helped define evidence-driven experiments and strict
fail-closed boundaries.

The Milan investigation yielded meaningful offline results and a
working live experimental path using a Windows-derived calibration.
It also exposed important limits of Linux-native calibration on
this particular reader. The subsequent decision to use ChicagoHS
reflects the actual Windows engine profile and our device-specific
evidence—not a defect claim about FreshHomie's implementation, whose
qualified scope is the 521d. No Milan runtime or Type 17 code is
included in the current ChicagoHS payload. FreshHomie's contribution
to the project is therefore **indirect but technically important**:
research, freely shared source, and unusually helpful engineering
guidance.

Both authors deserve explicit recognition for turning difficult Goodix
biometric research into inspectable, reusable open-source knowledge.

### Historical Goodix/SIGFM foundation

[Rockytkg's Goodix project](https://github.com/Rockytkg/goodix-linux-27c6-5125)
provided an important historical SIGFM implementation reference and
reused device-configuration research, as detailed in
[Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md).
The SIGFM driver remains available on this repository's `main` branch
as the documented recovery baseline. These separately maintained
projects may differ in hardware scope and behavior.

## Community contributions

Thank you to [@grold](https://github.com/grold) for two distinct contributions:

- [Issue #51](https://github.com/sorguido/goodix-27c6-5125/issues/51): reporting
  and analyzing the `IRQ 0x0080` problem when contact begins before or is already
  present during arming, affecting multi-finger enrollment and lock-screen use.
- [PR #49](https://github.com/sorguido/goodix-27c6-5125/pull/49): discovering,
  implementing, testing on real hardware, and sharing support for
  `GF_ST411SEC_APP_12508` on Goodix USB `27c6:5125`.

## Development tools

OpenAI AI and Qwen models assisted analysis, implementation, testing,
documentation and review. Human contributors retain responsibility for
design decisions, hardware validation and release approval.
[Upstream references](docs/REFERENCES.md) identify the source projects
separately from those development tools.
