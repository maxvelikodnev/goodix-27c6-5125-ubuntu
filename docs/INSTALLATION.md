<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Installation and updates

Install the signed RPM with KDE Discover, then use Goodix Manager to install,
update or remove the driver. Installing the package and activating the driver
are separate actions.

## Supported configuration

Fedora 44 KDE on x86_64, a local user account, Goodix USB `27c6:5125` running
`GF_ST411SEC_APP_12509` or `GF_ST411SEC_APP_12508`, Fedora's stock fprintd, PAM
and Plasma Login, and SELinux Enforcing. Fedora qualification concerns one
APP12509 reader, where the current biometric implementation has also passed
initial supervised VM/live checks. APP12508 recognition has contributor hardware
evidence from the earlier driver on Ubuntu GNOME, not a complete qualification
of the current biometric implementation on Fedora.
Other hardware, unlisted firmware, distributions and network accounts are
unsupported. See [validation](VALIDATION.md) for the qualification boundary.

## Prerequisites

Use a working, supported Fedora installation with SELinux Enforcing, password
login and ordinary sudo access. Finish authentication dialogs and close fingerprint settings.
The reader may be absent during installation, update, reinstall and removal.
When it is present, keep your finger off it during administrative prompts.

You need the signed RPM for Fedora 44 x86_64, its verified public signing key,
and administrative access. Discover handles the Fedora runtime dependencies;
network access may be needed to obtain those packages. Once the RPM and its
dependencies are installed, driver installation, update and removal work offline.
No Git checkout, compiler, Windows machine, capture tooling or reader-specific
file is required.

## Install, update or reinstall

Download the matching `goodix-manager-…fc44.x86_64.rpm` from the
[official releases](https://github.com/sorguido/goodix-27c6-5125/releases).
Use a stable release with a compatible RPM asset; if none is available, do not
substitute a package for another Fedora version or architecture. The RPM/Discover
qualification boundary is recorded in [validation](VALIDATION.md).

Before the first installation, download the
[public certificate](../packaging/rpm/keys/Guido-Soraru.asc) as `Guido-Soraru.asc`.
Inspect it and compare the full fingerprint with an independently trusted copy:

```bash
gpg --show-keys --with-fingerprint Guido-Soraru.asc
```

```text
5FB2 0F58 3C83 1729 D5FF 073F E99A DAE5 B7EA 4918
```

Only after that comparison, import the public key and verify the RPM. Replace
`VERSION-RELEASE` below with the downloaded package's version and revision:

```bash
sudo rpmkeys --import Guido-Soraru.asc
rpmkeys --define '_pkgverify_level all' --checksig --verbose goodix-manager-VERSION-RELEASE.fc44.x86_64.rpm
```

Require the signature and digest checks to report `OK`, and the signature's
`key fingerprint` to match the full fingerprint above. Stop on a different signer,
missing signature or verification failure; a signature trusted by another key
is not sufficient. Do not disable signature checking. The key is reused for
subsequent releases; a key change requires a new fingerprint comparison, not an
automatic import.

Open the verified RPM with **KDE Discover** and complete its installation. This
installs **Goodix Manager** and a compiled local payload. It does not activate the
driver, modify PAM or pairing state, or start a fingerprint action.

Discover 6.7.5's local-RPM page may show a generic icon and “Unknown author” even for
this correctly signed package; its local-file view does not use the embedded
application metadata. See the [Discover metadata limitation](../packaging/rpm/README.md#discover-metadata-and-system-appearance).
The KDE menu and Manager use the installed Goodix icon. On the observed KDE
configuration, the application menu's **Uninstall** action could not resolve
the locally installed package in Discover. To remove only the Manager RPM,
use `sudo dnf remove goodix-manager`; this does not remove an activated
Goodix driver. To remove the driver, use **Remove** in Goodix Manager or the
normal `goodix-uninstall` command. Do not confuse the two operations.

Open **Goodix Manager** from the KDE application menu. With no driver installed,
choose **Install** and approve the PolicyKit authorization. For an update, install
the newer signed RPM with Discover, reopen the Manager and choose **Apply Update**
when the local payload is newer than the installed driver. Updating the RPM alone
leaves the previous runtime in place. A packaging-only revision does not require
reapplying an unchanged driver. Downgrades are not offered.

For an intact, recognized 2.x/SIGFM installation, choose **Upgrade to 3.0.0**.
Read the format-change warning and confirm the direct transactional upgrade;
no preliminary uninstall is required. Restart when requested and enroll your
fingers again. The historical files and host pairing state are preserved.
The populated-installation VM qualification remains a release gate in
[validation](VALIDATION.md).

The Manager checks official public releases in the background and links to a
compatible RPM when one is available. Download and verify it, install it with
Discover, then return to the Manager. **Check unavailable** means the public
release check could not complete; local operations remain available offline.
**No compatible public release** is distinct from a failed check or a current
package. A newer public RPM remains visible even if a local runtime integrity
or version check blocks Apply Update. Downloading an RPM does not bypass that
local check, and its signature must still be verified before installation.

After normal or emergency driver removal, reopen the Manager and choose
**Install** to reinstall from its local payload. If the Manager itself was
removed, reinstall the signed RPM first. A failed integrity or provenance check
blocks Update: keep the diagnostics and review the explanation. Do not force
a migration past a failed check; [removal and recovery](UNINSTALL.md) remains
available when you need to remove the installation.

Earlier ChicagoHS CLI installations may lack a version field. The Manager can
recognize a verified source identity from a pinned historical build without
rewriting its receipts or reinstalling the current driver. If no exact version
can be established, it reports **Unknown version**; public release discovery
continues, while local mutation remains subject to the provenance checks.

The Manager stays unprivileged and requests authorization only through its fixed
installed helper. Keep password access available throughout. Wait for the final
operation result, follow any restart or enrollment instruction and use
**Copy diagnostics** if an error needs reporting.

The installation backend creates or preserves the root-only host pairing state
location, installs the runtime and login entry, and copies standalone
`goodix-uninstall` and `goodix-force-remove` commands into the normal command
search path. It quiesces fprintd during replacement without opening the reader
or initializing pairing. Fedora's daemon, greeter and vendor authentication
files remain package-owned.

Source-based CLI installation remains an
[advanced alternative](../production/README.md#source-installation-advanced).
For package building and future releases, see the
[RPM maintainer instructions](../packaging/rpm/README.md).

### Fingerprint temporary suspension

When a temporary password-only session is needed, for example during remote access
where the physical reader cannot be used, fingerprint authentication can be
suspended without uninstalling the driver, deleting templates, changing host
pairing state, or rewriting PAM/authselect policy:

```bash
sudo systemctl mask --runtime --now fprintd.service
```

The runtime mask prevents stock fingerprint consumers from activating `fprintd`
for the current boot, so authentication falls back to the remaining Fedora policy
instead of waiting for an unavailable fingerprint interaction. Restore fingerprint
availability at any time, without rebooting, with:

```bash
sudo systemctl unmask --runtime fprintd.service
```

There is no need to start `fprintd` manually after the unmask: the next normal
fingerprint consumer can activate it through the stock D-Bus/systemd path. Because
the mask is runtime-only, a reboot also clears it automatically; reboot is not
required for either suspension or restoration.

A fresh install creates only the empty host pairing state root. The runtime then
initializes the reader itself on the first fingerprint action: it confirms the
target identity, derives the configuration from the reader's own OTP, stores a
locally generated pairing key and records it with a single bounded pairing write.
Ordinary later use reuses that stored key and writes nothing to the reader.
Password and desktop access remain available throughout.

## Enrollment and basic verification

After successful installation, use KDE's normal fingerprint settings to manage
your enrolled fingers. The first fingerprint action takes slightly longer while
the reader is initialized; later actions do not repeat that step. If already
enrolled with the current driver, the existing compatible template remains
available. **Fingerprints enrolled with the earlier 2.x/SIGFM engine must be
enrolled again** after the major upgrade: the old format is preserved but cannot
be read by the new matcher. Otherwise enroll one finger following the normal
prompts, without opening simultaneous fingerprint applications.

### Recommended enrollment practice

For reliable recognition, do not place your finger in exactly the same position
for all enrollment samples.

During enrollment:

- lift your finger completely after each accepted sample;
- place it again with a slightly different position or angle;
- include the center of the fingertip as well as slightly higher, lower, left
  and right areas;
- vary the angle naturally so that the enrollment represents different parts
  of the fingerprint;
- keep the finger reasonably flat on the sensor and avoid deliberately
  repeating the same contact area.

The goal is not to repeat one identical touch. The native biometric engine
advances through twelve accepted enrollment stages, but physical contacts and
retained subtemplates may exceed that number because of quality or position
prompts. Good coverage remains more important than repeating one position.

Windows Hello follows a similar principle and explicitly encourages capturing
the finger from different angles. KDE's standard enrollment dialog may provide
less positional guidance, so following these recommendations manually can
improve day-to-day recognition reliability.

Check ordinary password login first. For the fingerprint check at Plasma Login,
submit the empty password field once and follow its prompts with the enrolled
finger. Stop on the first successful match. If it does not match, allow at most
three explicit physical attempts in that series; after the third failure use
password and report the result. Do not start repeated test series or add a fourth
attempt. A password-encrypted KWallet may ask for its own password after login.
Other consumers offer fingerprint where the current Fedora authentication
configuration enables it. The installer does not modify authselect or global PAM
policy. The supported Fedora configuration uses authselect `with-fingerprint`;
systems with an altered Fedora authentication policy may not offer fingerprint
to those consumers until that host policy is corrected.

## If installation or authentication fails

Stop at the first error and keep its exact non-secret message. A failed
prerequisite does not authorize bypassing a safety check. The Manager reports
failed operations and verified rollback; incomplete recovery is an error, not
success.

If project software remains installed or authentication regresses, use
**Remove** in Goodix Manager, or `goodix-uninstall` from a working terminal. If
normal removal refuses damaged project state or the desktop is unavailable,
use **`goodix-force-remove`** according
to [the emergency instructions](UNINSTALL.md). Follow its final restart instruction
and verify password login. Do not delete templates or host pairing state, change
Fedora PAM by hand, disable SELinux or hide the reader.

Report the Manager diagnostics or CLI error, failure point and whether
password/desktop access works.
Never attach host pairing state, templates, fingerprint images or raw USB captures.

## What updates and reinstalls preserve

The Manager applies the verified local payload through the same transactional
backend used by the source installer. Direct migration is offered for recognized,
intact 2.x installations; its populated-installation VM qualification is separate
from the offline software checks.
**New fingerprint enrollment is required** with the 3.x engine because historical
templates are incompatible; the old files are preserved. A failed ownership,
receipt or state check must stop instead of guessing how to migrate. A deliberate
downgrade uses a pinned historical version (see [removal and recovery](UNINSTALL.md)).

Existing project software is replaced while fprintd is quiescent. Host pairing
state and existing templates are preserved; a reader's pairing identity is never
changed by an update, and reinstallation after removal does not re-pair it.

After normal or emergency removal, **Install** in Goodix Manager reinstalls
the software and restores its SELinux labels. Existing templates remain available.
Incompatible Fedora changes may disable fingerprint functionality; report them
or remove the project instead of replacing Fedora's own authentication components.

## Using the reader with Windows

The same reader can be shared with Windows. Windows may replace the reader's
pairing when it initializes the device; Linux classifies that change read-only,
preserves the Windows record byte-for-byte and restores its own stored key with
one qualified write on the next fingerprint action. No reinstall, re-enrollment
or manual key handling is needed, and no key is shared between the two operating
systems. Initial Windows/Linux switching with the current driver was observed
to succeed on one reader. The exact pairing-write/readback/TLS audit remains
separate from that functional result; see [validation](VALIDATION.md).
