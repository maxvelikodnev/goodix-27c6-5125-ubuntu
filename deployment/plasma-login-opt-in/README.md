<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Plasma Login fingerprint selection

The canonical builder compiles this small Linux-PAM module into the RPM payload.
Goodix Manager's explicit [Install/Apply Update](../../docs/INSTALLATION.md)
action applies it and its service entry through the shared installation backend;
installing the RPM alone does not activate it. Fedora continues to supply the login
daemon, greeter and current vendor authentication configuration.

A nonempty password selects normal Fedora password authentication immediately.
Submitting an empty field explicitly selects stock `pam_fprintd` with
`max-tries=3 timeout=30`. The series stops on a match. Account, password and session
processing include the current vendor PAM file instead of a frozen copy.

Installed project files are `/etc/pam.d/plasmalogin` and the module/metadata in
`/usr/local/lib64/goodix-plasma-login/`. The normal and emergency
[removal commands](../../docs/UNINSTALL.md) remove the service entry first and
reveal Fedora's current configuration without requiring the vendor file to exist.

`test_gate.c`, `test_dispatch.c` and `test_dispatch.py` exercise selection and
PAM dispatch with synthetic fixtures. The public builder runs these checks before
producing its installable module. They do not authenticate a real account.
See the [technical manual](../../TECHNICAL_MANUAL.md) for integration limitations.
