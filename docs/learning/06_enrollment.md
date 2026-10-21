<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 6. Enrollment

## 👀 What you see

KDE asks you to touch and lift the same finger several times. A progress
indicator advances until the finger is enrolled.

It can feel repetitive, but each useful touch sees a slightly different part
or pressure pattern.

## 🏠 A simple analogy

Imagine teaching someone to recognize a handwritten signature. One example is
helpful, but several natural examples show what changes and what stays stable.

Fingerprint enrollment does the same kind of job:

```text
one finger, several views → a reusable reference
```

It does **not** average the images into a prettier photograph. Each useful
view contributes original ChicagoHS features and a native subtemplate.
Some retained subtemplates do not advance the visible enrollment progress:
ChicagoHS has a separate position-guidance policy.

## 🗺️ The enrollment flow

```mermaid
flowchart TD
    A["KDE asks fprintd to enroll a finger"] --> B["fprintd claims the reader"]
    B --> C["Optional duplicate check on existing templates"]
    C --> D["Prepare sensor and secure session"]
    D --> E["Wait for finger"]
    E --> F["Detect, capture, decode, preprocess"]
    F --> G["Extract native ChicagoHS features"]
    G --> H{"Image and features usable?"}
    H -->|Poor or unusable| J["Ask for another touch"]
    H -->|Valid| I["Native subtemplate insertion"]
    I --> P{"ChicagoHS position policy"}
    P -->|Position tip / defer| J
    P -->|Advance| K{"12 accepted policy stages?"}
    J --> E
    K -->|Not yet| E
    K -->|Yes| L["Finish after the contact boundary is accepted"]
    L --> M["libfprint serializes template"]
    M --> N["fprintd saves it for the user"]
```

## How many touches?

The current source implements the **original ChicagoHS twelve-stage
enrollment policy**. Its rules are not the fixed-21 SIGFM policy:

- Enrollment completes only when twelve native **policy stages** have advanced.
- The backend can retain a subtemplate even when position guidance asks for
  another touch and the visible stage does not advance.
- Poor or unusable images can request another scan without advancing the
  policy. Invalid protocol, template or binding data instead fails closed.
- The policy includes original position tips and a controlled
  **defer/drop/restore** behavior, preserving the upstream ordering rather
  than forcing every image into the gallery.
- The number of physical contacts and the number of retained subtemplates
  therefore need not be twelve. More contacts may be required naturally.
- The driver does not bypass ChicagoHS quality or position rules to force
  a completed enrollment, nor does it reinterpret rejected images as valid.

The CH0 offline corpus completed twelve policy stages in twenty contacts.
The subsequent initial live test through KDE also completed the enrollment;
some strongly shifted touches produced normal requests to reposition.
These limited results do not prove that every possible finger will enroll
reliably or establish statistical biometric accuracy.

For historical context only, the prior **SIGFM** driver from older tagged
releases (such as `v2.2.1`) used a fixed-21 accepted-sample policy and
measured three all-zero scoring probes
among 100 in one qualification set (3.00%). That statistic does **not**
describe ChicagoHS and must not be used to estimate its error rate.

## Why lifting the finger matters

A physical touch is not finished the instant its image is available. In the
ordinary path, the driver observes the release boundary and completes the
protocol tail before arming the next stage.

Enrollment also handles a specific zero-mask sensor report by keeping the image
already captured and skipping the extra acquisition. Normal sample checks still
apply before another touch is requested; a final sample can complete without a
separate release notification. The [technical manual](../../TECHNICAL_MANUAL.md#74-enrollment-zero-mask-recovery)
describes this bounded rule and its remaining late-event limitation.

Keep following the touch-and-lift prompts. The driver does not silently take
unlimited pictures while you hold the finger in place.

## Who is responsible for what?

| Component | Enrollment responsibility |
| --- | --- |
| KDE settings | Shows the user interface and progress. |
| `fprintd` | Checks permission, claims the reader, starts/stops enrollment, labels the finger, and saves the completed record. |
| `libfprint` | Builds the host template, reports progress, serializes the result, and coordinates image processing. |
| Goodix driver | Prepares the reader and performs bounded finger detection, image capture, release, cancellation fences and native twelve-stage progress reporting. |
| ChicagoHS | Generates calibration from the no-finger image, preprocesses frames, extracts native features and enforces the original twelve-stage policy, including position tips and deferred subtemplates. |
| Sensor | Measures the finger and returns device data; it does not save the Linux user's final template in this project. |

## What happens when a sample is poor or repetitive?

Three outcomes are easy to confuse:

- **accepted:** the contact has usable ChicagoHS features and the native
  position policy advances one of the twelve stages;
- **position tip:** a usable native subtemplate may be retained without
  advancing the stage; the next touch should cover a different position;
- **retry:** the contact was consumed but the image or native policy needs
  another touch; no unsupported successful stage is fabricated;
- **fatal processing or protocol error:** enrollment stops safely rather than
  hiding another sensor acquisition.

The current driver deliberately fails closed after certain processing failures.
That prevents a completed sensor-side acquisition from silently turning into an
unexpected extra contact.

## When does saving happen?

The driver does not write the finished template into the sensor. It hands the
accepted samples to `libfprint`. `libfprint` serializes the biometric
representation, and `fprintd` stores it under the host's fingerprint storage
area for the selected user and finger label.

That division becomes important during uninstall: removing the driver can
preserve the user's templates for later reinstallation.

> 🔎 **Want to see this in the repository?**
> ChicagoHS enrollment integration is in
> [`goodix_chicago.c`](../../libfprint-driver/goodix_chicago.c), with the
> unchanged native position policy in
> [`goodix-chicago-enrollment.c`](../../production/chicago-native/chicago/goodix-chicago-enrollment.c).
> The device's protocol-stage model remains in
> [`goodix_enrollment_model.c`](../../libfprint-driver/goodix_enrollment_model.c);
> the historical SIGFM diversity helper is not the current ChicagoHS
> admission policy.

## ✅ What to remember

- Enrollment teaches the host using several views of one finger.
- Usable contacts contribute native ChicagoHS subtemplates to the gallery.
- The original ChicagoHS policy advances exactly twelve accepted stages;
  physical contacts and retained subtemplate counts can differ. Initial
  enrollment completed on one real reader; longer-term performance is open.
- Each stage follows its contact-completion rules before requesting another touch.
- `fprintd` saves the completed host template; the sensor does not become a
  database of Linux users.

---

[← Previous: From image to template](05_from_image_to_template.md) | [Up: Learning home](README.md) | [Next: Verification and matching →](07_verification_and_matching.md)
