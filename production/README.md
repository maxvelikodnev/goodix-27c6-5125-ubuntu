<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Milan source build

The root `install.sh` uses `build-public.py` for the Milan-only payload on this
branch. The canonical installer and standalone `goodix-uninstall` and
`goodix-force-remove` retain the existing Fedora integration and software-only
recovery paths. See [installation status](../docs/INSTALLATION.md).

## Source availability

The native MIT/RVA publication gate is still open. `production/milan-native/`
is therefore absent from the public tree. `validate_milan_sources()` rejects
that incomplete source set before the installer requests sudo or installs
packages. It does not fall back to SIGFM, download code, or search `development/`.
The separate `build-milan-candidate.py` remains the historical private M4 builder;
it is not part of ordinary installation.

A complete source-only staging tree can exercise the canonical builder privately
without Git history, the M4 binary, a reference checkout or reader data. Closing
the publication decision is required before promoting those native sources and
claiming that a public clone is a self-contained RC.

## Build and payload

`build-public.py` resolves its source root from its own location and builds as an
ordinary Fedora 44 x86_64 user. The pinned C1 native recipe is built as PIC C
objects in a static archive; Meson selects Milan. OpenCV, SIGFM and a C++ compiler
are not required by this payload. Fedora supplies the stock fprintd, libgusb,
OpenSSL, GLib, libdeflate, PAM and desktop consumers.

The payload contains libfprint, the existing Plasma Login selector/PAM entry,
the offline material checker, original native notices/license texts, source
hashes and build provenance. It uses the same libfprint SONAME and checks the
symbols required by stock fprintd, dependency resolution, payload inventory and
hashes. No calibration, key, biometric template, capture or OEM binary is shipped.

The public [source ledger](source-files.tsv) and [digests](source-files.sha256)
cover the local driver sources. The native source set has a pinned recipe,
per-file source digests and original notices, validated before building. See
[licensing and provenance](../docs/LICENSING_AND_PROVENANCE.md).

## Runtime boundary

Milan calibration acquisition and device binding remain unqualified. Runtime
activation fails closed before reading protected calibration, claiming the USB
interface or sending protocol/pairing commands. Stock libfprint may already have
opened a USB handle.
A successful offline build and synthetic installation transaction do not qualify
live authentication, Windows coexistence or sensor safety. M5 live work requires
separate authorization on the physical Fedora host.
