# Ubuntu 24.04 build instructions

This developer preview builds a private libfprint library and a narrowly modified
fprintd 1.94.3 daemon. Experimental `.deb` packaging is described in
[RELEASES.md](RELEASES.md); it is not a generally qualified hardware release.
The original Fedora deployment scripts are not used for Ubuntu.

Follow the [root README](../README.md) for apt dependencies, then run as an
ordinary user:

```sh
python3 -B ubuntu/build-ready.py --output "$HOME/goodix-ubuntu-build"
```

Output must be a new directory outside this checkout. Artifacts are
`library/runtime/libfprint-2.so.2.0.0`, `daemon/src/fprintd` and checksum manifests.
No reader is opened, no service is started, and no PAM configuration is changed.
Builds default to blocking persistent pairing writes. To build an operational
version permitting initial pairing, explicitly add `--allow-persistent-pairing`;
this flag is not an installation command or a promise of reversibility.

## Offline checks

```sh
cc -std=c11 -Wall -Wextra -Werror ubuntu/test_prompt.c -o /tmp/goodix-prompt-test
/tmp/goodix-prompt-test
sh libfprint-driver/tests/run_goodix_post_tls_test.sh
sh libfprint-driver/tests/run_goodix_pairing_crypto_test.sh
sh libfprint-driver/tests/run_goodix_self_state_test.sh
sh libfprint-driver/tests/run_goodix_pairing_provision_test.sh
```

The build also runs the synthetic pairing-write guard test. These checks do not
access USB or use real fingerprint data. Full daemon integration tests remain work to do. The package lifecycle has
separate mocked and disposable-container checks.

## Controlled integration boundary

The supervised test used private root-owned directories under `/opt` and a
fprintd-only systemd override. Stock Ubuntu binaries were preserved. Keep the
existing service sandbox and password path. Never load a privileged daemon or
library directly from a user-writable build directory, and never replace distro
PAM files with the vendored fprintd files. Installation should verify ownership,
artifact checksums, existing configuration and rollback before activation.

For initial pairing, a persistent reader write may be necessary. Removal of host
software does not prove restoration of the original sensor state. Preserve
protected pairing files and enrolled templates during updates and rollback.
Keep any such files outside the source checkout and out of issues and commits.

Only after successful genuine/negative tests and a password-fallback check should
fingerprint login be enabled using Ubuntu's normal administration tools. On errors,
stop instead of flashing, resetting, deleting pairing state or loosening matching.

[Changes and observed validation](CHANGES.md) · [fprintd provenance](FPRINTD-PROVENANCE.md)
