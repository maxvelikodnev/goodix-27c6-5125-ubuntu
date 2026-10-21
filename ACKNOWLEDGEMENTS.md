<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Acknowledgements

I maintain this Goodix USB `27c6:5125` project as an individual open-source
developer. I owe much of its progress to people who published difficult
research, inspectable code and patient technical explanations. Their work
deserves recognition in its own right, independently of which components
remain in the current driver.

## The foundational contribution — Rockytkg

I owe a particularly substantial debt of gratitude to
**[Rockytkg (@Rockytkg)](https://github.com/Rockytkg)** for
**[goodix-linux — Goodix 27c6:5125/5135](https://github.com/Rockytkg/goodix-linux-27c6-5125)**.

This was **the essential starting point** for my work on the 5125, not merely
a historical reference to the SIGFM matcher. Rockytkg made a working and
readable Linux implementation of the reader's complicated protocol publicly
available when the device was still largely unexplored in my own project.
The published implementation and
[protocol documentation](https://github.com/Rockytkg/goodix-linux-27c6-5125/blob/227eba219fa9e3fbac5bd59aca79f624f67cd11b/docs/protocol.md)
gave me a concrete foundation for understanding the sequence from
`A0`/`B0` messages through initialization, OTP/device configuration,
PSK-based secure communication, TLS, finger detection (FDT), protected image
capture, image decoding and integration with libfprint.

**Without that foundation, I would likely still be trying to get beyond the
first `A0` exchanges.** It greatly shortened the distance between recognizing
the device and building a driver capable of actual fingerprint enrollment
and authentication.

The original project's SIGFM/libfprint integration also made it possible for
me to establish the earlier working Linux baseline and to learn where image
acquisition ends and host-side biometric recognition begins. Even after
replacing SIGFM with ChicagoHS, I continue to use Rockytkg's research as an
important historical and differential reference. The current project also
retains a specifically attributed adaptation of the APP12509/ChicagoHS
Type 12 `CONFIG90`/OTP configuration derivation, from
[`src/goodix_init.c` and `src/goodix_otp.c` at `227eba21`](https://github.com/Rockytkg/goodix-linux-27c6-5125/tree/227eba219fa9e3fbac5bd59aca79f624f67cd11b/src),
under its applicable GPL terms; see
[Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md).

My driver is an independently integrated and more narrowly constrained
implementation. I do **not** adopt the upstream firmware-update, forced
PSK-reset or other unrestricted persistent-device procedures. The factory-
preserving, fail-closed behavior, pairing recovery and Fedora KDE integration
have their own evidence and limits. That distinction does not diminish the
importance of Rockytkg's original breakthrough.

I am deeply grateful for the decision to publish this research and source.
It made the entire project possible.

## Native ChicagoHS biometrics — Berke Kabagöz

My special thanks to
**[Berke Kabagöz (@berkekbgz)](https://github.com/berkekbgz)**
for publishing
**[libfprint-goodix-spi](https://github.com/berkekbgz/libfprint-goodix-spi)**,
including an inspectable native ChicagoHS engine and extensive supporting
Windows-driver reverse-engineering work.

Its reconstruction, parity documentation, source provenance and differential
tests gave me a technically defensible way to replace the previous SIGFM
matcher with the biometric engine actually selected by Windows for the
tested 5125. I directly incorporate the original
**ChicagoHS computational sources**, pinned to
[`010a665f54089a1632b1ab7be588b316ace934e2`](https://github.com/berkekbgz/libfprint-goodix-spi/tree/010a665f54089a1632b1ab7be588b316ace934e2).
These cover native no-finger calibration generation, preprocessing, feature
extraction, the twelve-stage enrollment and position policy, Type 24 template
handling, matching and associated computational helpers.

I integrate those sources with my separate USB/TLS transport, controlled
pairing lifecycle and Fedora/libfprint authentication path. I do **not**
import the upstream SPI transport, its fprintd modifications, proprietary
Windows binaries or firmware. Original copyright and LGPL notices remain
authoritative and are preserved; see
[Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md).

I am especially grateful that Berke made this difficult biometric work
available as source code with sufficiently detailed provenance to support
careful reuse rather than speculative imitation. His device qualification
and my 5125 testing remain distinct and should not be conflated.

## Milan Type 17 research and guidance — FreshHomie

I also sincerely thank
**[FreshHomie (@FreshHomie)](https://github.com/FreshHomie)**
for publishing
**[goodix521d-fprintd](https://github.com/FreshHomie/goodix521d-fprintd)**,
its native Milan Type 17 implementation and the accompanying research into
preprocessing, feature extraction, enrollment, recognition and templates.

I particularly appreciate the time and technical care invested in answering
my questions in
**[the cross-device feasibility issue](https://github.com/FreshHomie/goodix521d-fprintd/issues/1)**.
The explanations about Windows-selected algorithm profiles, parameter tables,
classification metadata, matching modes and the importance of comparing
complete intermediate results shaped my experimental method and strengthened
the project's fail-closed compatibility criteria.

My Milan investigation produced meaningful offline results and a successful
live path with Windows-derived calibration, but the Linux-native calibration
path on my 5125 did not qualify. I selected ChicagoHS because of the actual
Windows engine profile and evidence for this particular reader, **not**
because FreshHomie's Milan implementation was defective. No Milan Type 17
runtime code is included in the current ChicagoHS payload.

FreshHomie's contribution to my work is therefore indirect but substantial:
open-source research, engineering guidance and a practical demonstration of
how to investigate a difficult Goodix matcher rigorously. I am grateful for
both the work and the willingness to share it.

## Linux and Fedora upstream

My driver builds on the work of the
[libfprint](https://gitlab.freedesktop.org/libfprint/libfprint),
[fprintd](https://gitlab.freedesktop.org/libfprint/fprintd),
[Linux-PAM](https://github.com/linux-pam/linux-pam) and
[KDE Plasma](https://kde.org/) contributors, as well as the maintainers of
libgusb, OpenSSL and Fedora. I thank them for maintaining the libraries,
authentication framework and desktop integration on which this project
depends. OpenCV was part of the earlier SIGFM implementation; it is not a
dependency of the current ChicagoHS runtime.

## Community contributions

I thank **[@grold](https://github.com/grold)** for two distinct contributions:

- [Issue #51](https://github.com/sorguido/goodix-27c6-5125/issues/51):
  reporting and analyzing the `IRQ 0x0080` problem when finger contact begins
  before or during arming, affecting enrollment and lock-screen operation.
- [PR #49](https://github.com/sorguido/goodix-27c6-5125/pull/49):
  discovering, implementing, testing on real hardware and sharing support
  for `GF_ST411SEC_APP_12508` on Goodix USB `27c6:5125`.

## Development tools

OpenAI AI and Qwen models assisted me with analysis, implementation, testing,
documentation and review. I remain responsible for design decisions,
hardware qualification and release approval. These tools are not substitutes
for the cited upstream authors' original work.
[Upstream references](docs/REFERENCES.md) and
[Licensing and provenance](docs/LICENSING_AND_PROVENANCE.md)
record the technical sources, copyrights and licensing separately.
