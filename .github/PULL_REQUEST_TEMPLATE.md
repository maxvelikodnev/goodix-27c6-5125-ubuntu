<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
## Summary

Describe the problem, the minimal change proposed, and the affected components. Link any related Discussion or issue.

## Maintained scope and safety

Refer to [CONTRIBUTING.md](https://github.com/sorguido/goodix-27c6-5125/blob/main/.github/CONTRIBUTING.md). For non-applicable items, explain why under **Notes**; checked boxes are declarations, not substitutes for evidence.

- [ ] This contribution is within the **Fedora KDE** and Goodix USB `27c6:5125` maintained scope.
- [ ] Unknown/unqualified hardware, firmware, protocol responses, and failed security checks remain **fail-closed**.
- [ ] No firmware flashing, IAP, ClearApp, OTP/factory-data write, persistent USB identity change, or unreviewed pairing write is introduced.
- [ ] Windows/Linux coexistence, host pairing data, templates, password fallback, and installation/removal/recovery are preserved or the effects are explicitly evaluated.
- [ ] No invasive changes to Fedora's stock fprintd/PAM configuration are introduced.

## Validation evidence

| Check | Command, result, evidence, or N/A with reason |
| --- | --- |
| Build and relevant offline regression tests | |
| Negative/fail-closed tests | |
| Sanitizers (ASan/UBSan), where relevant | |
| Physical hardware tests (explicitly state **not performed** if none) | |
| Authentication, coexistence, and recovery checks if affected | |

**Live-test scope, if performed:** hardware model, firmware identity, operating environment, and exact operations/results. Do not represent offline checks as live qualification. Privileged or hardware-sensitive testing requires explicit authorization.

## Risk, documentation and provenance

Describe safety-sensitive paths and remaining known limitations. State whether documentation or dependencies changed.

- [ ] No secret, pairing state, biometric material, firmware dump, or raw USB capture is included.
- [ ] Relevant licensing/SPDX/provenance obligations are preserved.
- [ ] Any `TECHNICAL_MANUAL.md` update documents established technical knowledge only, **not** step-by-step testing or a development diary.

**Notes / N/A explanations:**
