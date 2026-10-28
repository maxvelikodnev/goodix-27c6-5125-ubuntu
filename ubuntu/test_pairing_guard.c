/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Reuse only the upstream synthetic fixture; this executable has no USB code. */
#define main upstream_pairing_test_main
#include "../libfprint-driver/tests/test_goodix_pairing_provision.c"
#undef main

int main (void)
{
  guint8 bb2[GOODIX_PAIRING_PROVISION_BB010002_LENGTH];
  guint8 wb[GOODIX_PAIRING_PROVISION_ENVELOPE_LENGTH];
  guint8 validator[GOODIX_PAIRING_PROVISION_VALIDATOR_LENGTH];
  GoodixPairingProvisionAudit audit = { 0 };
  g_autoptr(GoodixPairingProvision) provision = fixture (bb2, wb, validator, &audit);
  g_assert_nonnull (provision);
  for (guint attempt = 0; attempt < 2; attempt++)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GBytes) frame = goodix_pairing_provision_begin_e0 (provision, &error);
      g_assert_null (frame);
      g_assert_error (error, GOODIX_PAIRING_PROVISION_ERROR,
                      GOODIX_PAIRING_PROVISION_ERROR_STATE);
      g_assert_cmpstr (error->message, ==, "Ubuntu preview blocks persistent pairing writes");
    }
  g_assert_cmpuint (audit.logical_e0_count, ==, 0);
  g_assert_cmpuint (audit.persistent_write_count, ==, 0);
  g_assert_false (goodix_pairing_provision_is_complete (provision));
  g_print ("UBUNTU_PAIRING_GUARD=PASS SYNTHETIC_ONLY=true\n");
  return 0;
}
