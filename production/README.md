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

Linux-native calibration quality/matching remain to be qualified on 5125.
Ordinary authentication stays closed until those requirements are qualified.
The explicit M5 diagnostic path allows protected calibration validation and
capture in a separate controlled process after live authorization; it cannot
activate ordinary enrollment, verify, identify or PAM authentication. The separate
M5.C private opt-in permits experimental enrollment and matching with the existing
qualified snapshot after equality against the current preflight OTP prefix;
prefix semantics and universal uniqueness remain unknown. Stock libfprint may open a USB handle before a driver
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

## Private M5.C operator gate (not executed by the build)

After explicit USB/root/biometric authorization, prepare a **new** session using
`prepare-milan-diagnostic.py --experimental-enrollment --ack-binding-unknown`
with the existing `--source`, `--session` and both qualified expected SHA256
options. The source must already be root-owned mode0600. The existing snapshot
is unchanged; no new M5.B capture is required. The session parent is root-owned
mode0700 inside private `development/`, and source/session paths are symlink-free.

Use the canonical RC's colocated `goodix-milan-diagnostic`:

```text
goodix-milan-diagnostic --m5c enroll --session ABSOLUTE_PRIVATE_SESSION
goodix-milan-diagnostic --m5c verify --session ABSOLUTE_PRIVATE_SESSION
```

`identify` is also available as a single-probe alternative to `verify`. Each
invocation closes the device. The driver permits one enrollment and at most
three verify/identify attempts **combined across processes**, counting failures
and cancellation; a successfully journaled MATCH prevents further attempts.
Any journal/cleanup error requires STOP without relaunch, even if no stop marker
could be created. The client never
retries. Enrollment uses unchanged 18-stage admission, at most 36 contacts and
600 seconds; a probe has one contact and 30 seconds. Wait for APPOGGIA/SOLLEVA.
The dedicated `milan-template.fp` is mode0600 in that session, never in stock
fprintd storage. An exclusive session lock prevents simultaneous clients, and
template I/O uses the same directory inode retained by the driver.

PASS requires enrollment, a later probe MATCH with `OTP_PREFIX=MATCH`, and
`M5_CLOSE=PASS`. On mismatch, missing/corrupt/untrusted calibration or template,
USB error, ambiguous pairing, cancellation or timeout: STOP, keep the consumed
budget, inspect only non-sensitive results, and do not auto-repeat. A quality
rejection consumes a probe attempt. An error may leave **no** `metrics.ini`;
M5.C never writes capture raw/image files. The private path requires already
active pairing and cannot issue E0. A fresh process without the explicit M5.C
signal cannot authenticate; fprintd/PAM and password fallback are unchanged.

Rollback is to close the client and use the canonical software removal/SIGFM
reinstallation procedures, with no factory reset or automatic biometric deletion.
A successful M5.C gate is experimental evidence, not ordinary-auth qualification.
