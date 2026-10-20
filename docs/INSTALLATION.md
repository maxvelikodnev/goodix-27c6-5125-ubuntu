<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# ChicagoHS: installation and updates

This branch targets **Fedora 44 KDE x86_64**, Goodix USB `27c6:5125`, and
APP12509/APP12508 firmware. ChicagoHS live biometric and desktop qualification
has not been performed. Start with an isolated Fedora KDE VM without USB
passthrough to qualify software installation and recovery. The SIGFM driver on
`main` remains the recovery baseline.

## Install, update or reinstall

Use the same command for first installation, update and reinstall. Existing
clones must already select `porting_ChicagoHS`; the block preserves other branches.
Run as your ordinary administrative user. Git must already be available.

```bash
(
set -e
REPO="$HOME/goodix-27c6-5125"

if [ -d "$REPO/.git" ] && [ ! -L "$REPO" ]; then
    case "$(git -C "$REPO" remote get-url origin)" in
        https://github.com/sorguido/goodix-27c6-5125.git|git@github.com:sorguido/goodix-27c6-5125.git) ;;
        *) echo "STOP: $REPO is not the Goodix Git repository."; exit 1 ;;
    esac
    if [ "$(git -C "$REPO" branch --show-current)" != "porting_ChicagoHS" ]; then
        echo "STOP: select a separate ChicagoHS clone; the current branch is preserved."
        exit 1
    fi
    git -C "$REPO" pull --ff-only
elif [ ! -e "$REPO" ] && [ ! -L "$REPO" ]; then
    git clone --branch porting_ChicagoHS --single-branch https://github.com/sorguido/goodix-27c6-5125.git "$REPO"
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

An existing recognized SIGFM or Milan installation is removed through the same
owned-software transaction before ChicagoHS installation. Failure restores the
previous project software when possible. A damaged or unrecognized receipt fails
closed; use `goodix-force-remove`, follow its restart instruction and reinstall.
The installer preserves fingerprint templates and
`/var/lib/fprint/goodix-5125-state-v2/`, including host pairing state. Reinstall
also preserves them. No automatic deletion or conversion of historical templates
is performed.

## Checks and software-only VM qualification

`./install.sh --help` and `./install.sh --check` do not install packages or request
privileged changes. `--check` validates prerequisites; it does not certify a live
reader or existing protected state. A missing package can therefore cause the
check to stop before a regular installation installs dependencies.

In the VM, leave USB passthrough disabled, run the canonical installation block,
then reinstall with `./install.sh`. Check password login remains available and
exercise both documented removal paths separately. Require `GOODIX_INSTALL=PASS`
and `GOODIX_REMOVAL=PASS`; on any failure preserve the error and use the recovery
procedure. A successful software-only VM test does not qualify authentication.

## Enrollment and authentication

ChicagoHS implements twelve accepted enrollment stages and requires a new
enrollment for every finger and user. SIGFM/`GSF1` and Milan templates use different
identities and are not accepted or converted. Stock fprintd isolates users'
galleries. Manage fingerprints through KDE's normal fingerprint settings;
explicit user-requested deletion uses the stock database.

Use normal APPOGGIA/SOLLEVA prompts and lift between contacts. Do not run another
fingerprint client simultaneously. Calibration is derived from the no-finger
background; there is no private diagnostic enrollment path or Windows file
prerequisite. Hardware binding and calibration must pass the driver's checks;
unsafe or unqualified binding fails closed.

At Plasma Login, a nonempty password uses password authentication. Submitting an
empty password explicitly selects fingerprint authentication, with at most three
attempts and a stop on the first match. Keep password access available; fingerprint
authentication does not unlock a password-encrypted KWallet. Live reader testing,
including enrollment, verify, identify, screen unlock, sudo, PolicyKit and Windows
coexistence, requires separate supervised qualification. See
[validation](VALIDATION.md) and [removal/recovery](UNINSTALL.md).
