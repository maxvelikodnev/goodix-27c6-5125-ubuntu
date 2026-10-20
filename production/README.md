<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Milan source build

The root `install.sh` uses `build-public.py` for the Milan-only payload on this
branch. The canonical installer and standalone `goodix-uninstall` and
`goodix-force-remove` retain the existing Fedora integration and software-only
recovery paths. See [installation status](../docs/INSTALLATION.md).

## Source availability

`production/milan-native/` contains the pinned native C source set, original
notices, recipe and hashes. The repository owner's distribution decision accepts
the examined MIT/LGPL portions; see [provenance](../docs/LICENSING_AND_PROVENANCE.md).
`validate_milan_sources()` verifies completeness, pins, hashes and file types
before the installer requests sudo or installs packages.

The canonical build works from a fresh clone or source archive without Git
history, the private M4 binary, a reference checkout, `development/` or reader
data. The separate `build-milan-candidate.py` is the historical M4 builder and is
outside ordinary installation.

## Build and payload

`build-public.py` resolves its source root from its own location and builds as an
ordinary Fedora 44 x86_64 user. The pinned C1 native recipe is built as PIC C
objects in a static archive; Meson selects Milan. OpenCV, SIGFM and a C++ compiler
are not required by this payload. Fedora supplies the stock fprintd, libgusb,
OpenSSL, GLib, libdeflate, PAM and desktop consumers.

The payload contains libfprint, the existing Plasma Login selector/PAM entry,
the offline material checker, diagnostic client/preparer, native notices, source
hashes and build provenance. It uses the same libfprint SONAME and checks the
symbols required by stock fprintd, dependency resolution, payload inventory and
hashes. No calibration, key, biometric template, capture or OEM binary is shipped.

The public [source ledger](source-files.tsv) and [digests](source-files.sha256)
cover the local driver sources. The native source set has a pinned recipe,
per-file source digests and original notices, validated before building. See
[licensing and provenance](../docs/LICENSING_AND_PROVENANCE.md).

## Runtime boundary

Reproducible calibration acquisition and device binding remain under study.
Ordinary authentication stays closed until those requirements are qualified.
The explicit M5 diagnostic path allows protected calibration validation and
capture in a separate controlled process after live authorization; it cannot
activate ordinary enrollment, verify, identify or PAM authentication with
unqualified calibration. Stock libfprint may open a USB handle before a driver
action is refused.

The private M3 calibration is neither a payload asset nor an installation
requirement. A successful build and synthetic installation do not qualify live
authentication, Windows coexistence or biometric safety. Live work uses the
supported Fedora environment chosen by the operator, with exclusive access to
the real Goodix USB reader, and requires separate authorization.

The installed diagnostic helpers are
`/usr/local/lib64/goodix-27c6-5125/goodix-milan-diagnostic` and
`prepare-milan-diagnostic.py` in that same directory. `--help` and
`--offline-check` do not enumerate USB or open protected data. The live client
permits one capture with a 30-second acquisition deadline, then closes; it does
not retry or provision pairing. A new trusted session and explicit root action
are required. These helpers are covered by the ordinary runtime receipt and
removal tools.
