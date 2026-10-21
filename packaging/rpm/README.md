<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Goodix Manager and native RPM

Goodix Manager installs the Goodix fingerprint driver from a local, verified
binary payload. I support Fedora **44 KDE, x86_64**. Fedora 45 is not qualified
and the package and helper refuse it. No branch name is part of the build or
runtime contract. The driver version comes exclusively from root `VERSION`;
the packaging revision is the positive `--release` argument.

## Two explicit installation steps

1. Verify the publisher's full public key fingerprint through an independently
   trusted channel, import that public key, and verify the downloaded RPM.
   Open the local RPM with KDE Discover and complete its package transaction.
2. Open **Goodix Manager** from the application menu and select **Install**.
   PolicyKit authorizes the fixed installed helper. The GUI stays unprivileged.
   Installation uses the packaged payload without cloning, downloading or
   compiling. A connected reader is not required.

Installing, upgrading or removing the Manager RPM does not change PAM, activate
the driver, start fingerprint actions or remove pairing data. A package upgrade
leaves the previous driver running until **Apply Update** succeeds. A change to
packaging `Release` alone does not request a driver update. Same-version payloads
with a different source identity are refused: bump `VERSION` for driver changes.
Password access must already work. Read [installation](../../docs/INSTALLATION.md)
and [recovery](../../docs/UNINSTALL.md) before activation.

**Remove** performs the ordinary `goodix-uninstall` operation with the original
receipt, ownership and hash checks. The Manager remains installed. Reboot as
instructed after removal, then check password login. Reinstallation preserves
host pairing state and historical templates; there is no implicit re-pairing.

Removing the **RPM itself** leaves any activated runtime and its standalone
`/usr/local/bin/goodix-uninstall` and `goodix-force-remove` commands in place.
Those files are installed by the existing backend and are never RPM-owned.
Use the normal CLI to remove the remaining driver, or reinstall the Manager.
Emergency `goodix-force-remove` remains a TTY recovery command; it is not a GUI
action. No RPM scriptlet attempts authentication changes or irreversible cleanup.

Unknown or damaged software stops safely. Recognized 2.x legacy installations
are shown and can be removed normally, but direct Manager migration is blocked
pending populated-installation VM qualification. Old fingerprints require **new
enrollment** with the 3.x engine; historical files are preserved. A prior 3.x CLI
receipt without a verifiable driver version is shown as version unknown, rather
than guessed. Use ordinary removal then install for such a transition, preserving
all protected data. The classic `install.sh` path remains available.

## Updates and privacy

At startup the GUI asynchronously checks the official GitHub stable Release API.
It compares runtime, local payload and public versions numerically. Only a
matching Fedora/architecture RPM asset produces a download action. The flow is
**Download update → Discover → reopen Manager → Apply Update**; no remote code
is executed by the helper. Prereleases, malformed replies, missing compatible
RPMs, rate limits and offline failures do not disable local operations.

The read-only status helper checks installed package files and runtime receipts
without opening USB or reading pairing/template contents. A narrow PolicyKit
action permits status to the active local session; each mutation requires
administrator authorization. Copyable diagnostics contain fixed software status
codes and versions, never raw backend output or biometric journal entries.
There is no telemetry. GitHub receives the normal update HTTP request.

## Build the next release

Install the build prerequisites on Fedora 44 (network is needed only to obtain
Fedora packages, not by the build):

```sh
sudo dnf install rpm-build rpm-sign rpmlint desktop-file-utils appstream \
  gcc meson ninja-build pkgconf-pkg-config glib2-devel libgusb-devel \
  libusb1-devel openssl-devel json-glib-devel pam-devel binutils fprintd
sudo dnf install python3-pyside6 qt6-qtdeclarative kf6-kirigami
python3 packaging/rpm/build.py --release 1
```

For a driver release change `VERSION` once; for packaging-only changes increment
`--release`. Build outputs, exact source archive, SRPM, build log, hashes and
signature result are exclusively in `development/rpm/builds/VERSION-RELEASE.fc44/`.
An existing build identity is never overwritten, including after failed builds;
use the next release number. The archive has deterministic ordering and metadata;
bit-for-bit RPM reproducibility across differing toolchains is not claimed.
The build calls `production/build-public.py`, including its native tests, ABI,
stock fprintd resolution and manifest checks. RPM postprocessing does not alter
the already hashed payload. Automatic library provides are filtered so this
private inactive library cannot satisfy Fedora's stock libfprint dependency.
The complete corresponding source travels in the SRPM, retaining per-file notices.

## Personal signing identity and trust

The selected personal public certificate is [Guido-Soraru.asc](keys/Guido-Soraru.asc).
Its full fingerprint is **5FB2 0F58 3C83 1729 D5FF 073F E99A DAE5 B7EA 4918**.
This is public verification material, never the private key. Authenticate this
fingerprint independently before trusting a downloaded package.

Set up the reusable personal key **once**, in your own terminal, outside Git:

```sh
gpg --quick-generate-key 'Guido Sorarù' rsa3072 cert,sign 2y
gpg --fingerprint 'Guido Sorarù'
```

Reuse an existing appropriate personal key instead of regenerating it. Set a
nonempty passphrase in protected pinentry; never put it in commands, environment
variables, chat, build scripts or logs. No email address is invented or required.
GnuPG stores private material in its personal home with restrictive permissions.
Record the full public fingerprint, and then build and sign with one command:

```sh
python3 packaging/rpm/build.py --release 2 --sign FULL_PUBLIC_FINGERPRINT
```

To sign an already built, unchanged artifact after key setup:

```sh
python3 packaging/rpm/build.py --release 1 --sign FULL_PUBLIC_FINGERPRINT \
  --sign-existing development/rpm/builds/3.0.0-1.fc44
```

Signing uses pinentry and the same key on every release. Verification uses an
isolated RPM database containing only the exported certificate whose primary
fingerprint equals the explicitly requested fingerprint, and requires signatures
and digests. `result.json` records the final signed hashes. The exported `.asc`
contains only public material. See upstream [rpmsign](https://rpm.org/docs/6.0.x/man/rpmsign.1)
and [rpmkeys](https://rpm.org/docs/6.0.x/man/rpmkeys.8).

On another Fedora machine, first compare the complete fingerprint with the
publisher through an independent trusted channel:

```sh
gpg --show-keys --with-fingerprint Guido-Soraru-public.asc
sudo rpmkeys --import Guido-Soraru-public.asc
rpmkeys --define '_pkgverify_level all' --checksig --verbose goodix-manager-VERSION-RELEASE.fc44.x86_64.rpm
```

Only then open the RPM in Discover. A valid signature does not establish trust
by itself. No automatic key import or disabled signature checking is provided.
Discover/PackageKit behavior must be qualified on a clean Fedora 44 KDE VM;
successful command-line signature verification alone is not that qualification.

For backup, use a private encrypted removable volume under your control, with
mode 0700 on the destination directory and `umask 077`. Export the passphrase
protected secret key directly there with `gpg --export-secret-keys --armor
FULL_PUBLIC_FINGERPRINT > /your/encrypted/volume/private-key.asc`; never use the
repository, a shared folder or a build directory. Preserve the GnuPG revocation
certificate from your personal `openpgp-revocs.d` there too. Verify restoration
in a separate private GnuPG home under your control. For rotation, publish a
revocation notice and authenticate the replacement full fingerprint independently;
do not silently regenerate the signing key in a build.

## Qualification before distribution

Run public fixture tests and inspect the RPM before a VM transaction:

```sh
python3 packaging/rpm/test_packaging.py
python3 packaging/rpm/test_helper.py
python3 packaging/rpm/manager/test_model.py
python3 production/test_build_public.py
python3 deployment/test_install.py
python3 deployment/test_install_selection.py
python3 deployment/recovery/test_remove.py
rpmlint -c packaging/rpm/rpmlint.toml development/rpm/builds/VERSION-RELEASE.fc44/rpmbuild/RPMS/x86_64/*.rpm
```

On a disposable Fedora 44 KDE VM with password login confirmed and USB passthrough
disabled, record Discover install, first activation, package upgrade followed by
Apply Update, packaging-only upgrade, ordinary remove/reboot/reinstall, and RPM
removal with active runtime followed by CLI recovery. Disconnect networking for
activation and update. Compare PAM/drop-ins before/after RPM-only transactions;
they must be unchanged. Inject backend failures using the public fixture tests;
real rollback and recovery require separate VM observation. A populated legacy
2.x migration and protected-data preservation checks remain explicit gates.

PASS requires actual observations at each layer. STOP for missing password
access, unexpected file ownership, receipt drift, failed rollback or unexplained
state. VM rollback is its known-good snapshot; runtime rollback uses the canonical
backend, ordinary CLI removal or documented TTY recovery. Never bypass checks to
make a test pass. Record unavailable Discover, PolicyKit, Fedora 45, USB and login
tests as NOT_TESTED. No release publication is part of this build command.

The lint configuration preserves original upstream license text (including its
historical FSF address). Remaining unstripped/private-library warnings reflect
the canonical hashed payload and its required library links. The fixed TLS
cipher selection is the existing reader protocol, not a new host crypto policy.
