# Ubuntu fork: changes from sorguido/goodix-27c6-5125

Base commit: `4dff813bb80ddedb2b801b89ced223e4e4594ca7`.
Maintainer of the original Goodix project: Guido Sorarù (@sorguido).
Ubuntu adaptation: Maksim Velikodnev (@maxvelikodnev).

## Build and integration

- `build.py` validates Ubuntu 24.04 x86_64 packages, builds without root or USB
  access, checks libfprint's SONAME and the installed Ubuntu fprintd ABI, verifies
  dependency resolution, and records package/source provenance.
- Builds retain the pinned upstream ChicagoHS computation sources and matcher.
- `build-ready.py` builds the private library and fprintd 1.94.3 daemon together.
- The default library blocks construction of E0 pairing writes. Explicit
  `--allow-persistent-pairing` produces an operational build; building still
  does not run it. The guarded version can reuse an existing valid Linux key.

## Prompt timing

Previously, the daemon emitted `VerifyFingerSelected` immediately after starting
verification while the sensor was still collecting a no-finger baseline. Early
touch could cause a baseline rejection. The Ubuntu build now completes activation
and emits `goodix-capture-ready` after calibration and first-arm ACK. The paired
daemon sends the verification prompt only after that signal and its method reply.
The event is one-shot; cancellation does not produce a delayed prompt.

This is readiness synchronization, not a fixed sleep and not a change to the
matching threshold. Hardware initialization latency remains.

## Observed checks on 2026-10-28

- Ubuntu 24.04 LTS x86_64, GNOME, HONOR HYM-WXX, USB `27c6:5125`, APP12508.
- Registration completed; successful genuine verification observed after
  re-enrollment. One earlier enrollment failed genuine matching.
- One unregistered-finger rejection observed before that re-enrollment.
- Local sudo succeeded using the initial driver and again using the readiness
  update. For the latter, the ready event preceded the successful match.
- 64 post-TLS tests passed normally and under ASan/UBSan; new coverage checks
  baseline/ACK readiness, single delivery and cancellation before readiness.
- Prompt-state ordering tests passed normally and under ASan/UBSan.
- No new key write was needed for the readiness update.

These are operator-supervised functional observations on one reader. They do
not establish FAR/FRR, liveness, long-term reliability, full D-Bus integration,
graphical login, updated enrollment handoff, suspend/resume or Windows coexistence.
No raw capture, template, pairing key or personal journal is included.

## Upstream boundaries retained

No firmware flashing, OTP/factory-data writes, relaxed matching threshold,
replacement Windows key export, or automatic pairing-write retry is added.
Original copyright and per-file licenses remain intact. Fedora installers,
Goodix Manager and Fedora documentation remain upstream reference material;
they are not Ubuntu installation instructions.
