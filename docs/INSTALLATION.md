<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ChicagoHS: installation and updates

The canonical Goodix driver targets **Fedora 44 KDE x86_64**, Goodix USB
`27c6:5125` and APP12509/APP12508 firmware. The ChicagoHS engine supersedes
the earlier SIGFM engine in the planned 3.0.0 release; it is **not** a
separately maintained edition. An initial supervised live functional check
passed on one reader in a Fedora KDE VM; see the precise evidence and
remaining limitations in [Validation](VALIDATION.md). Start software-only
installation and recovery checks without USB passthrough.

## Install, update or reinstall

Use the same command for first installation, update and reinstall **after
the 3.0.0 ChicagoHS code has been merged into `main`**. The stable public
installation source is `main`; the temporary porting branch is not a user
installation or maintenance channel. Existing clones must select `main`;
the block never switches a user's working branch automatically. Run as
your ordinary administrative user. Git must already be available.

```bash
(
set -e
REPO="$HOME/goodix-27c6-5125"

if [ -d "$REPO/.git" ] && [ ! -L "$REPO" ]; then
    case "$(git -C "$REPO" remote get-url origin)" in
        https://github.com/sorguido/goodix-27c6-5125.git|git@github.com:sorguido/goodix-27c6-5125.git) ;;
        *) echo "STOP: $REPO is not the Goodix Git repository."; exit 1 ;;
    esac
    if [ "$(git -C "$REPO" branch --show-current)" != "main" ]; then
        echo "STOP: use a clean main clone for canonical installation; the current branch is preserved."
        exit 1
    fi
    git -C "$REPO" pull --ff-only
elif [ ! -e "$REPO" ] && [ ! -L "$REPO" ]; then
    git clone --branch main --single-branch https://github.com/sorguido/goodix-27c6-5125.git "$REPO"
else
    echo "STOP: $REPO exists but is not the Goodix Git repository."
    exit 1
fi

cd "$REPO"
./install.sh
)
```

The subshell stops on errors without closing your terminal. A failed clone or
fast-forward pull does not launch installation. `./install.sh` calls
`deployment/install.py`, validates the pinned ChicagoHS source inventory, checks
host prerequisites, installs Fedora build dependencies when needed, builds with
`production/build-public.py` as the ordinary user, and requests sudo only for
host checks and installation. No external matcher checkout, Windows calibration,
DLL, manual library copy or `development/` directory is required.

The installer selects `FIRST_INSTALL` or `UPDATE`, retains stock Fedora fprintd,
PAM and desktop packages, installs its private library and Plasma login selector,
and installs `goodix-uninstall` and `goodix-force-remove`. It checks the stock
fprintd ABI and dependency resolution. Installation never opens USB or starts an
authentication action. After success the source clone may be removed.

### Upgrade from SIGFM 2.x to ChicagoHS 3.0.0

The major upgrade changes the biometric engine and stored template format.
Every user will have to **enroll each desired finger again**. Existing
SIGFM/GSF1 templates remain on disk but are incompatible with ChicagoHS;
the installer does not convert or delete them. The pairing state and
password fallback remain preserved.

An existing **recognized** SIGFM or Milan installation is designed to be
removed automatically inside the same controlled, transactional update before
the ChicagoHS software is installed. Failure restores the previous project
software when possible. Therefore **manual uninstallation is not normally
required**, but a populated real SIGFM → ChicagoHS upgrade and rollback are
still pending live qualification before the 3.0.0 release. An unrecognized,
partial or drifted installation must stop rather than be forcibly overwritten;
use the documented emergency recovery path as appropriate.

The installer preserves fingerprint templates and
`/var/lib/fprint/goodix-5125-state-v2/`, including host pairing state.
Reinstallation also preserves them. Historical SIGFM can be obtained from
[versioned releases](https://github.com/sorguido/goodix-27c6-5125/tree/v2.2.1),
**not** from the post-merge `main` branch. The downgrade procedure is in
[Uninstall and emergency recovery](UNINSTALL.md).

## Checks and software-only VM qualification

`./install.sh --help` and `./install.sh --check` do not install packages or request
privileged changes. `--check` validates prerequisites; it does not certify a live
reader or existing protected state. A missing package can therefore cause the
check to stop before a regular installation installs dependencies.

For software-only VM validation, leave USB passthrough disabled, run the
canonical installation block, then repeat `./install.sh` for UPDATE. Check
password login remains available and exercise both documented removal paths
separately, including `goodix-force-remove` from TTY when appropriate.
Require `GOODIX_INSTALL=PASS` and `GOODIX_REMOVAL=PASS`; on failure preserve the
error and follow recovery. The initial clean-VM sequence has been observed PASS,
but such software checks alone never qualify authentication or protection of
previously populated biometric data.

## Enrollment and authentication

ChicagoHS implements twelve accepted enrollment stages and requires a new
enrollment for every finger and user. SIGFM/`GSF1` and Milan templates use different
identities and are not accepted or converted. Stock fprintd isolates users'
galleries. Manage fingerprints through KDE's normal fingerprint settings;
explicit user-requested deletion uses the stock database.

Use KDE's normal place/lift prompts and lift between contacts. Do not run another
fingerprint client simultaneously. Calibration is derived from the no-finger
background; there is no private diagnostic enrollment path or Windows file
prerequisite. Hardware binding and calibration must pass the driver's checks;
unsafe or unqualified binding fails closed.

At Plasma Login, a nonempty password uses password authentication. Submitting an
empty password explicitly selects fingerprint authentication, with at most three
attempts and a stop on the first match. Keep password access available; fingerprint
authentication does not unlock a password-encrypted KWallet. The first supervised live session has exercised enrollment, selected-finger
verification, a different-finger negative result, KScreenLocker, sudo, PolicyKit,
Plasma Login, reboot login, limited Windows coexistence and password fallback.
Further live qualification, including multiuser and failure-path coverage,
remains necessary. See [validation](VALIDATION.md) and
[removal/recovery](UNINSTALL.md).
