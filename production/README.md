<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Canonical ChicagoHS source build

`./install.sh` delegates to `deployment/install.py` and `production/build-public.py`.
The builder validates the pinned `production/chicago-native/source-manifest.json`,
compiles the necessary C units into a private static archive and links the
ChicagoHS-only libfprint shared library. It validates SONAME, absence of private
RPATH/RUNPATH and forbidden matcher dependencies, stock fprintd ABI/resolution,
PAM selector tests and the material checker before publishing a hashed payload.

Build from a complete source clone or snapshot, as an ordinary user on Fedora
44 x86_64 with the documented Fedora packages and stock fprintd:

```text
python3 production/build-public.py --output /tmp/goodix-chicago-payload
```

The output must be new and outside the source tree. No `development/`, external
matcher checkout, USB, protected biometric input or Windows calibration is used.
The payload contains libfprint, native notices/licenses, provenance, the login
selector and installer material checker. It contains no Milan diagnostic tool.
This command builds only; the canonical installer owns deployment and rollback.
