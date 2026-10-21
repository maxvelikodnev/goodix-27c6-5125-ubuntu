<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Public source build

Ordinary users install and update through the root `install.sh` using the
[documented installation procedure](../docs/INSTALLATION.md). No separate build
command, payload selection or maintainer tooling is required.

## Build architecture

`build-public.py` resolves the source root from its own location and builds as
an ordinary user on Fedora 44 x86_64. It uses the public libfprint base, Goodix
driver and a pinned native biometric backend together with supported Fedora
development packages. The native engine's original C sources are included in
the public tree under `production/chicago-native/`.
It does not require Git history, a branch name, a particular clone directory or
pre-existing build output.

The generated payload contains the Goodix-enabled libfprint runtime, the
native biometric code linked from a private static PIC archive, the Plasma Login
selector and PAM entry, the offline material checker, license notices and
source/build provenance. It does **not** contain the previous SIGFM/OpenCV
runtime or Milan diagnostic tools. Fedora supplies fprintd,
libgusb, OpenSSL, PAM, Plasma and the ordinary authentication consumers.

The builder verifies the library ABI and SONAME, stock fprintd resolution,
absence of private RPATH/RUNPATH and forbidden matcher dependencies, PAM selector
checks, source inventories and content hashes before installation. It validates
the pinned source manifest for the native algorithm and retains all applicable
upstream copyright and licensing notices. Build provenance records the
source-content digest and package versions rather than depending on Git objects.

The [source ledger](source-files.tsv) and [matching digests](source-files.sha256)
record the public driver sources and their provenance. See
[Licensing and provenance](../docs/LICENSING_AND_PROVENANCE.md) for the combined
library's terms.

## Standalone build (maintainer use)

The canonical installer performs this build automatically. A standalone build,
without deployment, can be used to examine its generated payload:

```text
python3 production/build-public.py --output /tmp/goodix-build-payload
```

The output location must be new and outside the source tree. No `development/`,
external matcher checkout, USB access, protected biometric input or Windows
calibration is required. The standalone command does not install software,
modify Fedora authentication or touch the reader.

## Validation boundary

The public installer performs the checks required for the supported installation
path. Additional maintainer verification is separate from the end-user procedure
and is not required to install, use, update or remove the driver.

A successful source build does not by itself establish hardware behavior or every
Fedora authentication/SELinux path. The qualified scope and remaining limits are
documented in [Validation](../docs/VALIDATION.md).
