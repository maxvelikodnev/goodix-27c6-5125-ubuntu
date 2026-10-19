<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Contributing to Goodix USB 27c6:5125

Thank you for considering a contribution. Please read this policy before opening a pull request.

## Maintained scope

This repository maintains the Goodix USB `27c6:5125` driver for **Fedora KDE**. Only firmware identities explicitly supported by the current implementation are in scope; a matching USB ID alone does not establish compatibility.

Focused improvements to the driver, secure session, enrollment and matching, Fedora integration, installation and recovery, tests, and technical documentation are welcome within this scope.

**Other Linux distributions and desktop environments are outside this repository's maintenance and pull-request acceptance scope.** Independent ports and forks are welcome, but should be maintained separately. Questions and discussion about them belong in [GitHub Discussions](https://github.com/sorguido/goodix-27c6-5125/discussions), not in cross-distribution integration PRs.

## Before opening a pull request

- Keep changes small, focused, reviewable, and justified by a concrete problem.
- Discuss substantial changes to the USB/security protocol, PSK or pairing lifecycle, firmware coverage, biometric pipeline, fprintd/PAM integration, installation/recovery, or dependencies in Discussions before implementation.
- Avoid unrelated refactors, distribution auto-detection, or additional system-wide configuration changes.

## Non-negotiable safety requirements

- **Fail closed.** Reject unknown hardware or firmware identities, malformed input, mismatched state, failed integrity/security proofs, or unexpected protocol behavior. Do not bypass validation or fall back to an unverified path.
- **Preserve the device.** Do not add firmware flashing, IAP, ClearApp, OTP/factory-data writes, or persistent VID:PID/device-mode changes. Any permitted reader-persistent pairing action must stay within the documented, explicitly authorized and bounded path; changes to that path require focused review and qualification.
- **Preserve interoperability and recoverability.** Protect the established Windows/Linux pairing coexistence, host pairing state, fingerprint templates, password fallback, and safe installation, update, removal, and emergency recovery behavior.
- **Keep Fedora authentication integration minimal.** Retain the stock fprintd and PAM boundaries; do not replace distribution-wide PAM profiles, disable the stock fprintd PAM profile, or introduce invasive authentication changes.
- Keep cancellation, retries, timeouts, cleanup, lifecycle fencing, and negative/error paths bounded and auditable. Do not relax security checks or matching decisions merely to make a test pass.

## Evidence required for review

- Explain what changed, why, which components are affected, and the possible safety and compatibility impact.
- Report relevant build, offline regression, negative/fail-closed, and sanitizer checks with commands and actual results. Explain omissions or unavailable evidence.
- Explicitly distinguish **synthetic/offline tests** from **live hardware tests**. If live testing was performed, identify the tested hardware, firmware and operating environment and summarize the observed results. If it was not, state so; never claim broader qualification than the evidence establishes.
- Treat hardware access, privileged actions, and pairing-sensitive tests as separate, explicitly authorized activities. Do not ask other users to attempt unsafe or unqualified device operations.
- Never submit PSKs, passwords, host pairing state, fingerprint images/templates, firmware or factory dumps, raw USB captures, or other sensitive material. Share only redacted diagnostics; see [Security and privacy](../docs/SECURITY.md).
- Preserve SPDX identifiers, original copyright notices, licensing requirements, and provenance for reused code and new dependencies.

## Documentation discipline

[TECHNICAL_MANUAL.md](../TECHNICAL_MANUAL.md) records **durable technical knowledge** about the device and its Fedora integration. It is not a test diary, execution transcript, or running history of experiments. Keep temporary investigations and step-by-step test logs in development materials; update canonical documentation only with established technical facts and appropriately bounded evidence.

## Maintainer review

Target pull requests at `main`. The maintainer may request additional evidence, changes, or decline a contribution because of scope, safety, insufficient validation, regressions, or ongoing maintenance cost. A successful build or working independent fork does not imply acceptance into this repository.
