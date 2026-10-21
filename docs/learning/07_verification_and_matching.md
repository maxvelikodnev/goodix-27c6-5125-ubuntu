<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 7. Verification and matching

## The question changes

Enrollment asks, "can we build a reference?"

Verification asks, "does this new touch match the reference already saved for
this user?"

```mermaid
flowchart TD
    A["New finger touch"] --> B["Detect and capture one image"]
    B --> C["Decode and preprocess"]
    C --> D["Extract one ChicagoHS native probe"]
    D --> E["Validate current sensor and Type 24 host template"]
    E --> F["Run original ChicagoHS matching policy"]
    F --> G{"Comparison outcome"}
    G -->|Valid native score greater than zero| H["MATCH"]
    G -->|Valid native score not positive| I["NO MATCH"]
    G -->|Capture, data, or processing failed| J["ERROR / RETRY STATUS"]
```

## MATCH

A MATCH means ChicagoHS has completed a valid native template comparison
and returned a positive final match score. Its pinned selector threshold is
207 and the final decision uses `score > 0`. Those values have not been
tuned to improve the project's observed results.

A successful verify stops the current authentication attempt after the match.
The driver then treats the result as terminal, drains the action and performs
cleanup. No hidden fourth physical attempt is started.

MATCH does not mean that the raw images were identical. It means the native
ChicagoHS feature representation and saved Type 24 subtemplates satisfied
the original matching policy on that probe.

## NO MATCH

NO MATCH means the system obtained a valid ChicagoHS probe, evaluated the
compatible saved template and returned a nonpositive final match score.

This is different from a broken USB transfer, malformed image, cancelled
operation, or feature-extraction failure.

After a clean NO MATCH and complete cleanup, a consumer may start another
explicit verification attempt.

## Failed capture and retry

A **retry** tells the user that no trustworthy match/no-match decision was
made. Perhaps the image could not be processed or the finger needs to be placed
again.

The exact retry behavior is shared between the consumer, `fprintd`,
`libfprint`, and driver. In this project, failures that could make the device
state ambiguous do not trigger hidden extra captures. They stop or fence the
operation so a fresh, explicit action is required.

That leads to an important rule:

```text
NO MATCH = a valid biometric comparison said no
ERROR    = the system could not safely make that comparison
```

## Where the attempt limit lives

One Goodix verification action captures one probe. The program using the
fingerprint stack decides whether to ask again.

On the tested Fedora PAM path, `pam_fprintd` uses three attempts by default.
After a clean NO MATCH, it can issue another explicit request. It stops on the
first MATCH or when the attempt series is exhausted.

The Plasma Login selector also explicitly bounds fingerprint selection to
three physical attempts:

```text
attempt 1: MATCH → stop and continue login
           NO MATCH → offer attempt 2

attempt 2: MATCH → stop and continue login
           NO MATCH → offer attempt 3

attempt 3: MATCH → stop and continue login
           NO MATCH → fingerprint series ends

attempt 4: not part of the series
```

There is no promise that every Linux distribution or every consumer uses the
same number. Three is the current Fedora/project behavior described here.

## Cancellation

Cancellation can happen because the user closes a prompt, the client goes
away, or the authentication action ends.

The driver then fences new work, cancels host-side transfers, waits for their
callbacks to drain, releases resources, and clears sensitive session state.
It does not invent an unproven device-side cancel command.

If safe device-side quiescence is uncertain, the open session is marked
unusable instead of pretending it can continue.

## Verify one finger or identify among several?

Two closely related operations exist:

- **verify:** compare the probe with one selected enrolled template;
- **identify:** compare the probe with a gallery of enrolled templates and
  report which one matched, if any.

`fprintd` can use identification when the user chooses "any" enrolled finger.
Both use the same native ChicagoHS capture/preprocessing and Type 24 matching
engine. One probe is extracted once per action and compared with the
compatible gallery. A different backend's saved templates are not converted
or silently accepted.

## ✅ What to remember

- Verification creates one new native ChicagoHS probe and compares it with
  the saved, device-bound Type 24 template.
- MATCH, NO MATCH, and ERROR are different outcomes.
- A clean NO MATCH may be followed by another explicit attempt after cleanup.
- The tested PAM and Plasma Login series allow at most three attempts and stop
  on the first MATCH.
- Cancellation cleans up host resources; uncertain protocol state fails closed.

---

[← Previous: Enrollment](06_enrollment.md) | [Up: Learning home](README.md) | [Next: Desktop authentication →](08_desktop_authentication.md)
