<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Milan experimental branch: installation and updates

This page describes the experimental, installable Milan-only RC on
`porting_milan_type_17`; `main` retains SIGFM. The canonical installer builds from
versioned sources without private development files, reader data or prebuilt
libraries. It validates source completeness and integrity before requesting sudo.

Milan calibration and live authentication remain experimental. The M5.B capture
client and the separate M5.C enrollment/verify/identify mode require explicit
authorization; ordinary authentication stays closed until it is qualified. The private M3 blob
is not shipped or required to install the software. Host installation and
real-reader testing require separate consent for M5. SIGFM hardware results in
[validation](VALIDATION.md) do not qualify Milan.

## Supported configuration

Fedora 44 KDE on x86_64, a local user account, Goodix USB `27c6:5125` running
`GF_ST411SEC_APP_12509` or `GF_ST411SEC_APP_12508`, Fedora's stock fprintd, PAM
and Plasma Login, and SELinux Enforcing. Fedora qualification concerns one
APP12509 reader; APP12508 recognition is backed by contributor hardware evidence
on Ubuntu GNOME, not a complete Fedora authentication/lifecycle qualification.
Other hardware, unlisted firmware, distributions and network accounts are
unsupported. See [validation](VALIDATION.md) for the qualification boundary.

## Prerequisites

Use a working, supported Fedora installation with SELinux Enforcing, password
login and ordinary sudo access. Finish authentication dialogs and close fingerprint settings.
The reader may be absent during installation, update, reinstall and removal.
When it is present, keep your finger off it during administrative prompts.

You need Git, network access for the public clone and Fedora packages, and disk
space for a native source build. The software builder needs no private development tree, precompiled library,
Windows files, PSK, calibration or template. It builds only Milan C sources and
does not require OpenCV or a C++ compiler. Reader-specific Milan calibration is
a separate runtime prerequisite. The experimental M5.C path uses the qualified
Windows snapshot and requires its envelope prefix to match the current preflight
OTP. Background initialization is a separate offline investigation; do not
substitute it for the snapshot or provision private calibration through ordinary
installation. The protected M5 procedure is separate from installation. The installer does not
open USB.

## Install, update or reinstall

After host installation is separately authorized for M5, use this block for first installation, update or reinstall. It
requires the Milan branch in an existing clone and never switches branches:

```bash
(
set -e
REPO="$HOME/goodix-27c6-5125"

if [ -d "$REPO/.git" ] && [ ! -L "$REPO" ]; then
    case "$(git -C "$REPO" remote get-url origin)" in
        https://github.com/sorguido/goodix-27c6-5125.git|git@github.com:sorguido/goodix-27c6-5125.git) ;;
        *) echo "STOP: $REPO is not the Goodix Git repository."; exit 1 ;;
    esac
    if [ "$(git -C "$REPO" branch --show-current)" != "porting_milan_type_17" ]; then
        echo "STOP: select a separate Milan clone; the current branch is preserved."
        exit 1
    fi
    git -C "$REPO" pull --ff-only
elif [ ! -e "$REPO" ] && [ ! -L "$REPO" ]; then
    git clone --branch porting_milan_type_17 --single-branch https://github.com/sorguido/goodix-27c6-5125.git "$REPO"
else
    echo "STOP: $REPO exists but is not the Goodix Git repository."
    exit 1
fi

cd "$REPO"
./install.sh
)
```

The subshell stops on errors without closing your terminal. A failed clone or
fast-forward pull does not launch the installer.

`install.sh` checks the complete native source set first, then automatically selects `FIRST_INSTALL` or `UPDATE`.
It handles the host state check, supported Fedora prerequisites, source
build, installation and failure reporting. All package-manager,
build and installer output remains visible in the same terminal; the script does
not clear or replace the terminal, redirect the user-visible output, or run the
installation in the background.

Fedora may ask for your sudo password and confirmation before installing packages.
The installer first requests sudo for a read-only host state check, and later for
packages and system changes. If Fedora offers fingerprint first, leave the reader
untouched and wait about 30 seconds for the
password prompt. If password authentication is unavailable, stop; do not bypass it.

The installer builds the Goodix library and Plasma Login selector as your normal
user. It always creates or preserves
`/var/lib/fprint/goodix-5125-state-v2/` as the root-only host pairing state
location, labels it and verifies its permissions. It does not
open the reader or initialize pairing.
It installs the runtime and login entry, and
`goodix-uninstall` and `goodix-force-remove` in the normal command search path.
It quiesces fprintd during replacement and does not start a fingerprint test.
Fedora's daemon, greeter and vendor authentication files remain package-owned.

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

Expect **`GOODIX_BUILD=PASS`**, a **`GOODIX_INSTALL_MODE`** of **`FIRST_INSTALL`**
or **`UPDATE`**, then **`GOODIX_PAIRING_STATE=READY`** confirming the
host pairing state root is ready, followed by
**`GOODIX_INSTALL=PASS`** with **`READER_PRESENT_ALLOWED=true`**, and finally
**`GOODIX_INSTALL_BLOCK=PASS`**. Any error or missing final success marker means
installation did not complete. Neither runtime nor either removal command needs the
clone after successful installation. The repository clone may be removed; the
same bootstrap block clones it again automatically when needed.

A fresh install creates only the empty host pairing state root. Pairing uses
the same lifecycle as SIGFM on `main`: initial provisioning when necessary,
reuse of a valid Linux key, and qualified restoration after external replacement.
An ordinary reopen with a valid Linux pairing performs no `E0`. Every qualified
transaction retains the one-write limit, durable journal, byte-for-byte
`BB010002` preservation, readback/hash/TLS proof and fail-closed behavior.
Ordinary Milan authentication stops while calibration remains unqualified.
The separately authorized M5.B diagnostic path permits only controlled capture;
it does not grant enrollment, verify, identify or PAM access. Stock libfprint
may already have opened a USB handle before an ordinary action is refused.

Advanced users may clone elsewhere: the installer resolves its own location.
The standard procedure above needs no override.

### Private M5.C experiment

After a separately approved operator gate, the installed
`goodix-milan-diagnostic --m5c enroll|verify|identify --session PRIVATE_DIRECTORY`
uses an explicit process-local experimental capability. Prepare a new root-only
session with `prepare-milan-diagnostic.py --experimental-enrollment` and the
qualified snapshot's expected hashes. A consumed M5.B session is not an M5.C
authorization. The preparation help describes the required arguments.

The driver validates private file ownership/modes, calibration structure and
the envelope's 16-byte equality with the existing live OTP preflight. Any failure
stops the action. This association does not establish universal OTP uniqueness
or biometric safety. Existing active pairing is required; this private mode
cannot provision pairing. The session allows one enrollment and at most three
verify/identify attempts combined, stopping at the first match. Its new Milan
template remains in the private session, outside the stock fprintd gallery.
This mode does not enable KDE/PAM/login authentication. Both helpers support
`--offline-check` without opening protected data or enumerating USB.

## Enrollment and basic verification

After calibration and live use are separately qualified, use KDE's normal
fingerprint settings. **Milan requires a new enrollment for every finger and
user.** SIGFM/`GSF1` templates cannot verify or identify with Milan and are never
converted. The separate driver identity keeps historical SIGFM files outside
the Milan gallery; they need not be deleted to enroll with Milan. Do not open
simultaneous fingerprint applications.

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

Milan requires 18 accepted samples; build coverage across those samples.

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
prerequisite does not authorize bypassing a safety check. The installer reports
rollback if a system change fails; incomplete rollback is an error, not success.

If project software remains installed or authentication regresses, use
**`goodix-uninstall`** from the working desktop. If normal removal refuses damaged
project state or the desktop is unavailable, use **`goodix-force-remove`** according
to [the emergency instructions](UNINSTALL.md). Follow its final restart instruction
and verify password login. Do not delete templates or host pairing state, change
Fedora PAM by hand, disable SELinux or hide the reader.

Report the command, error, failure point and whether password/desktop access works.
Never attach host pairing state, templates, fingerprint images or raw USB captures.

## What updates and reinstalls preserve

The same bootstrap block checks dependencies and rebuilds from current source.
Existing project software is replaced while fprintd is quiescent. The software transaction preserves host pairing state and existing template
files. It does not access the reader. Subsequent reader use follows the ordinary
pairing lifecycle described above.

After normal or emergency removal, the block reinstalls the software and
restores its SELinux labels. Existing Milan templates remain available to Milan; SIGFM templates remain incompatible. Incompatible
Fedora changes may disable fingerprint functionality; report them or remove the
project instead of replacing Fedora's own authentication components.

## Using the reader with Windows

The inherited pairing lifecycle supports sharing the reader with Windows. Windows may replace the reader's
pairing when it initializes the device; Linux classifies that change read-only,
preserves the Windows record byte-for-byte and restores its own stored key with
one qualified write on the next fingerprint action. No reinstall, re-enrollment
or manual key handling is needed, and no key is shared between the two operating
systems. That coexistence evidence belongs to SIGFM. Milan has not yet been qualified
live with Windows; see [validation](VALIDATION.md).
