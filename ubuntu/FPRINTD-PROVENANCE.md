# fprintd source provenance

The `fprintd-source/` directory contains the fprintd 1.94.3 source release from
the Ubuntu archive, retaining its copyright and license notices.

- Archive: https://archive.ubuntu.com/ubuntu/pool/main/f/fprintd/fprintd_1.94.3.orig.tar.bz2
- SHA-256: `969777bacf353706747998e50e5d55050e6cec09117b2565a2e8681eba094a82`
- Upstream project: https://fprint.freedesktop.org/
- License text: [COPYING](fprintd-source/COPYING), plus original per-file notices.

Local modifications are restricted to:

1. `src/device.c`: delay `VerifyFingerSelected` for the exact
   `goodix_27c6_5125_chicago` device exposing `goodix-capture-ready`; emit it only
   after readiness and the VerifyStart reply; clear pending state at completion.
2. `src/goodix-prompt.h`: a small one-shot informational prompt-ordering helper.

No PAM authentication logic, biometric matching, access-control decision, print
storage format or PolicyKit rule is changed in this vendored daemon.
Other devices keep upstream prompt timing.

The build keeps the installed distro daemon untouched. Running this private
daemon requires a separately reviewed service override; merely building does
not activate it. Do not install the vendored service/PAM files over Ubuntu's.
