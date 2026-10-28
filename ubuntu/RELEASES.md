# Ubuntu .deb packages and releases

Target: Ubuntu 24.04 amd64, experimental Goodix USB `27c6:5125` support.
The regular package permits initial Linux pairing and bounded restoration after
Windows replaces it. First fingerprint use can persist pairing; installing the
package does not access the reader. Uninstall is not a factory reset.

## Install and activate

Download the `.deb` and its `.sha256` from this fork's GitHub Releases, then:

```sh
sha256sum -c goodix-5125-ubuntu_1.0.0_amd64.deb.sha256
sudo apt install ./goodix-5125-ubuntu_1.0.0_amd64.deb
goodix-ubuntu status
sudo goodix-ubuntu enable
```

Use the actual release version in these filenames. Checksums detect corruption;
they are not an independent publisher signature. These packages are not signed.

The package puts private binaries under `/usr/lib/goodix-5125-ubuntu`, preserving
stock binaries. Activation adds one service override, retains the Ubuntu sandbox,
creates root-only pairing storage if absent, and starts fprintd. It does not enroll
a finger or change PAM. Review persistent-pairing risks before enrollment.

Foreign overrides, including earlier hand-installed test versions, must be
removed using their original rollback procedures first. Do not delete pairing
state to migrate. This package deliberately refuses to compose unknown overrides.

Register and verify using GNOME Settings or fprintd. Keep the finger away until
requested. Keep password access; only after successful matching and negative tests
consider enabling Ubuntu's fingerprint PAM profile with `sudo pam-auth-update`.

## Disable, remove, upgrade or downgrade

```sh
sudo goodix-ubuntu disable
sudo apt remove goodix-5125-ubuntu
```

Disable restores stock fprintd. Removal also disables the package if active.
Neither operation erases pairing keys, fingerprint templates or PAM settings.
If the private daemon fails during initial activation, the helper removes its
new override and restores the previously running stock service.

For an upgrade or downgrade, disable first, install the desired `.deb`, then
enable again. Enabled upgrades are rejected before payload replacement. Keep the
previous package for rollback. This is explicit rollback, not an automatic apt
transaction rollback. Do not remove `fprintd` itself to uninstall this package.

## Build

Install the README build dependencies plus `dpkg-dev`, then as an ordinary user:

```sh
python3 -B ubuntu/build-ready.py --output "$HOME/goodix-release-build" --allow-persistent-pairing
python3 -B ubuntu/build-deb.py --build "$HOME/goodix-release-build" --output "$HOME/goodix-dist" --version 1.0.0
```

The build verifies the operational flag and binary hashes, computes runtime
dependencies with dpkg-shlibdeps, and packages licensing notices. It does not
install, activate or communicate with USB.

## GitHub Actions

Pushes and pull requests run the Ubuntu build and offline tests. A manual run
creates downloadable artifacts without publishing a release. A tag such as
`v1.0.0-ubuntu` builds that exact commit and publishes a **prerelease**, after checks,
with `.deb`, checksum and source archive. A tag must be created deliberately;
adding the workflow does not create one.

Only the release job gets `contents: write`. Pull requests and builds use read-only
permissions; pull_request_target is not used. No personal PAT, SSH key or sensor
credential is needed by CI. GitHub's job-scoped token uploads the release.

The source remains experimental. CI cannot prove biometric accuracy, hardware
compatibility, Windows coexistence, login reliability or resistance to spoofing.
