/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Public synthetic-only adapter tests, real libfprint serialization.
 */
#include "../goodix_chicago.h"
#include "../goodix_fpimage_device.h"
#include <string.h>
#ifdef GOODIX_CHICAGO_STORAGE_TEST
#include "file_storage.h"
#include <errno.h>
#include <glib/gstdio.h>
#endif

static GoodixChicago *context (guint8 change)
{
  GoodixChicago *c = goodix_chicago_new ();
  guint8 otp[64]; guint16 base[GOODIX_CHICAGO_PIXELS];
  GError *error = NULL;
  for (guint i = 0; i < 64; i++) otp[i] = (guint8) (i + 1);
  otp[63] ^= change;
  for (guint i = 0; i < GOODIX_CHICAGO_PIXELS; i++) base[i] = 2048;
  g_assert_true (goodix_chicago_bind_preflight (c, otp, sizeof otp, 1, &error));
  g_assert_true (goodix_chicago_native_background (c, base, G_N_ELEMENTS (base), 80, 64, 1, &error));
  g_assert_no_error (error);
  return c;
}
static GBytes *make_template (guint samples, guint features)
{
  GoodixChicagoEnrollment *e = goodix_chicago_enrollment_new ();
  GoodixChicagoFeatureRecord records[120] = { 0 };
  GoodixChicagoMetricData metric = { 0 };
  GoodixChicagoEnrollmentResult result;
  GError *error = NULL;
  g_assert_true (goodix_chicago_enrollment_insert_first (e, records, features, 1, 85, 100, &metric, &result, &error));
  for (guint i = 1; i < samples; i++)
    g_assert_true (goodix_chicago_enrollment_insert_next (e, records, features, 1, 85, 100, &metric, &result, &error));
  GBytes *bytes = goodix_chicago_enrollment_pack (e, &error);
  g_assert_no_error (error); g_assert_nonnull (bytes);
  goodix_chicago_enrollment_free (e);
  return bytes;
}
static void test_serialization (void)
{
  GoodixChicago *c = context (0);
  g_autoptr(GoodixFpImageDevice) dev = goodix_fpimage_device_new ();
  g_autoptr(GBytes) bytes = make_template (12, 1);
  g_autoptr(FpPrint) print = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), bytes));
  GError *error = NULL;
  g_assert_true (goodix_chicago_validate_print (print, &error));
  g_assert_no_error (error);
  guchar *serialized = NULL; gsize size = 0;
  g_assert_true (fp_print_serialize (print, &serialized, &size, &error));
  g_autoptr(FpPrint) restored = fp_print_deserialize (serialized, size, &error);
  g_free (serialized);
  g_assert_nonnull (restored); g_assert_no_error (error);
  g_assert_true (goodix_chicago_validate_print (restored, &error));
  g_assert_no_error (error);
  GoodixChicago *other = context (1);
  g_assert_false (goodix_chicago_validate_bound_print (other, restored, &error));
  g_clear_error (&error);
  goodix_chicago_free (other);
  guint8 otp[64]; guint16 changed_base[GOODIX_CHICAGO_PIXELS];
  for (guint i = 0; i < 64; i++) otp[i] = (guint8) (i + 1);
  for (guint i = 0; i < GOODIX_CHICAGO_PIXELS; i++) changed_base[i] = 2050;
  g_assert_true (goodix_chicago_bind_preflight (c, otp, 64, 2, &error));
  g_assert_true (goodix_chicago_native_background (c, changed_base, GOODIX_CHICAGO_PIXELS, 80, 64, 2, &error));
  g_assert_true (goodix_chicago_validate_bound_print (c, restored, &error));
  g_assert_no_error (error);
  otp[0] ^= 1;
  g_assert_true (goodix_chicago_bind_preflight (c, otp, 64, 3, &error));
  g_assert_false (goodix_chicago_validate_bound_print (c, restored, &error));
  g_clear_error (&error);
  goodix_chicago_free (c);
}
static void test_boundaries (void)
{
  GoodixChicago *c = goodix_chicago_new ();
  GError *error = NULL;
  guint16 raw[GOODIX_CHICAGO_PIXELS] = { 0 };
  GoodixChicagoSample sample;
  g_assert_false (goodix_chicago_bind_preflight (c, NULL, 0, 1, &error));
  g_clear_error (&error);
  g_assert_false (goodix_chicago_native_preprocess (c, raw, G_N_ELEMENTS (raw), 1, &sample, &error));
  g_clear_error (&error);
  goodix_chicago_free (c); c = context (0);
  g_assert_false (goodix_chicago_native_preprocess (c, raw, G_N_ELEMENTS (raw), 2, &sample, &error));
  g_clear_error (&error);
  raw[10] = 4096;
  g_assert_false (goodix_chicago_native_preprocess (c, raw, G_N_ELEMENTS (raw), 1, &sample, &error));
  g_clear_error (&error);
  goodix_chicago_clear_binding (c);
  g_assert_false (goodix_chicago_native_background (c, raw, G_N_ELEMENTS (raw), 80, 64, 1, &error));
  g_clear_error (&error); goodix_chicago_free (c);
}
static void test_corruption (void)
{
  GoodixChicago *c = context (0);
  g_autoptr(GoodixFpImageDevice) dev = goodix_fpimage_device_new ();
  g_autoptr(GBytes) bytes = make_template (12, 1);
  gsize size; const guint8 *data = g_bytes_get_data (bytes, &size);
  guint8 *bad = g_memdup2 (data, size); bad[size / 2] ^= 1;
  g_autoptr(GBytes) broken = g_bytes_new_take (bad, size);
  g_autoptr(FpPrint) print = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), broken));
  GError *error = NULL;
  g_assert_false (goodix_chicago_validate_print (print, &error)); g_assert_nonnull (error); g_clear_error (&error);
  g_object_set (print, "fpi-data", g_variant_new ("(su@ay)", "goodix-27c6-5125-milan-type17-e68eb725-v1", 1u,
    g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, size, 1)), NULL);
  g_assert_false (goodix_chicago_validate_print (print, &error)); g_clear_error (&error);
  g_object_set (print, "fpi-data", g_variant_new_string ("GSF1"), NULL);
  g_assert_false (goodix_chicago_validate_print (print, &error)); g_clear_error (&error);
  goodix_chicago_free (c);
}
static void test_envelope_negative (void)
{
  GoodixChicago *c = context (0);
  g_autoptr(GoodixFpImageDevice) dev = goodix_fpimage_device_new ();
  g_autoptr(GBytes) bytes = make_template (12, 1);
  gsize size; const guint8 *data = g_bytes_get_data (bytes, &size);
  guint8 sensor[16] = { 0 }, digest[32] = { 0 };
  GError *error = NULL;
  for (guint mode = 0; mode < 6; mode++)
    {
      GVariant *inner = g_variant_new ("(u@ay@ay@ay)", mode == 0 ? 2u : 1u,
        g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, sensor, mode == 1 ? 15u : 16u, 1),
        g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, digest, mode == 2 ? 31u : 32u, 1),
        g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, mode == 3 ? size - 1 : size, 1));
      g_autoptr(FpPrint) print = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), bytes));
      g_object_set (print, "fpi-data", g_variant_new ("(s@ay@v)", mode == 4 ? "wrong-backend" : GOODIX_CHICAGO_TEMPLATE_ID,
        g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, digest, mode == 5 ? 31u : 32u, 1),
        g_variant_new_variant (inner)), NULL);
      g_assert_false (goodix_chicago_validate_print (print, &error));
      g_assert_nonnull (error); g_clear_error (&error);
    }
  g_autoptr(GBytes) incomplete = make_template (1, 1);
  g_autoptr(FpPrint) probe = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), incomplete));
  g_assert_false (goodix_chicago_validate_print (probe, &error)); g_clear_error (&error);
  /* Exercise CRC rejection outside the first 64KiB of a valid native blob. */
  g_autoptr(GBytes) large = make_template (20, 120);
  data = g_bytes_get_data (large, &size);
  g_assert_cmpuint (size, >, 65536);
  guint8 *bad = g_memdup2 (data, size); bad[size - 17] ^= 1;
  g_autoptr(GBytes) broken = g_bytes_new_take (bad, size);
  g_autoptr(FpPrint) tail = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), broken));
  g_assert_false (goodix_chicago_validate_print (tail, &error)); g_clear_error (&error);
  goodix_chicago_free (c);
}
typedef struct { gboolean done; guint completions; FpPrint *print; GError *error; } Action;
static void enroll_done (GObject *object, GAsyncResult *result, gpointer data)
{
  Action *action = data;
  action->print = fp_device_enroll_finish (FP_DEVICE (object), result, &action->error);
  action->done = TRUE; action->completions++;
}
static void wait_action (Action *action)
{
  gint64 until = g_get_monotonic_time () + 3000000;
  while (!action->done && g_get_monotonic_time () < until)
    { while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  g_assert_true (action->done); g_assert_cmpuint (action->completions, ==, 1);
}
static void test_cancel_cleanup (void)
{
  for (guint pass = 0; pass < 2; pass++)
    {
      g_autoptr(GoodixFpImageDevice) dev = goodix_fpimage_device_new ();
      g_autoptr(GError) error = NULL;
      g_autoptr(GCancellable) cancel = g_cancellable_new ();
      g_assert_true (fp_device_open_sync (FP_DEVICE (dev), NULL, &error));
      GoodixDeviceContext *ctx = goodix_fpimage_device_get_context (dev);
      g_autoptr(FpPrint) print = g_object_ref_sink (fp_print_new (FP_DEVICE (dev)));
      Action action = { 0 };
      fp_device_enroll (FP_DEVICE (dev), print, cancel, NULL, NULL, NULL, enroll_done, &action);
      goodix_device_context_emit_arm_complete (ctx, NULL);
      if (pass)
        {
          guint8 otp[64] = { 1 }; guint16 base[5120];
          for (guint i = 0; i < 5120; i++) base[i] = 2048;
          g_assert_true (goodix_device_context_test_chicago_prepare (ctx, otp, base, &error));
          g_assert_no_error (error);
        }
      g_cancellable_cancel (cancel); wait_action (&action);
      g_assert_null (action.print); g_assert_error (action.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_clear_error (&action.error);
      guint commands = goodix_device_context_get_backend_command_count (ctx);
      guint16 late[5120] = { 0 };
      goodix_device_context_emit_image_ready (ctx, late, 5120);
      g_assert_cmpuint (goodix_device_context_get_backend_command_count (ctx), ==, commands);
      g_assert_cmpuint (action.completions, ==, 1);
      g_assert_true (fp_device_close_sync (FP_DEVICE (dev), NULL, &error));
      g_assert_no_error (error); g_assert_null (goodix_fpimage_device_get_context (dev));
    }
}
#ifdef GOODIX_CHICAGO_STORAGE_TEST
static void remove_fixture (const gchar *path)
{
  GDir *dir = g_dir_open (path, 0, NULL);
  if (!dir) { g_assert_cmpint (g_unlink (path), ==, 0); return; }
  const gchar *name;
  while ((name = g_dir_read_name (dir)))
    { gchar *child = g_build_filename (path, name, NULL); remove_fixture (child); g_free (child); }
  g_dir_close (dir); g_assert_cmpint (g_rmdir (path), ==, 0);
}
static void test_stock_storage (void)
{
  GoodixChicago *c = context (0);
  g_autoptr(GError) error = NULL;
  g_autoptr(GUsbDevice) usb = g_object_new (G_USB_TYPE_DEVICE, NULL);
  g_autoptr(GoodixFpImageDevice) dev = goodix_fpimage_device_new_for_usb (usb);
  g_autofree gchar *directory = g_dir_make_tmp ("goodix-chicago-storage-XXXXXX", &error);
  g_assert_no_error (error);
  g_setenv ("STATE_DIRECTORY", directory, TRUE);
  g_assert_cmpstr (fp_device_get_driver (FP_DEVICE (dev)), ==, "goodix_27c6_5125_chicago");
  const gchar *id = fp_device_get_device_id (FP_DEVICE (dev));
  g_autoptr(FpPrint) legacy = g_object_ref_sink (g_object_new (FP_TYPE_PRINT,
    "driver", "goodix_27c6_5125", "device-id", id, NULL));
  fpi_print_set_type (legacy, FPI_PRINT_RAW);
  g_object_set (legacy, "fpi-data", g_variant_new_string ("GSF1-synthetic"), NULL);
  fp_print_set_username (legacy, "user-a"); fp_print_set_finger (legacy, FP_FINGER_RIGHT_INDEX);
  g_assert_cmpint (file_storage_print_data_save (legacy), ==, 0);
  g_assert_false (fp_print_compatible (legacy, FP_DEVICE (dev)));
  g_assert_null (file_storage_discover_prints (FP_DEVICE (dev), "user-a"));
  g_autofree gchar *legacy_path = g_build_filename (directory, "user-a", "goodix_27c6_5125", id, "7", NULL);
  g_autofree gchar *before = NULL; gsize before_size;
  g_assert_true (g_file_get_contents (legacy_path, &before, &before_size, &error));
  g_autoptr(GBytes) bytes = make_template (12, 1);
  g_autoptr(FpPrint) first = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), bytes));
  g_autoptr(FpPrint) second = g_object_ref_sink (goodix_chicago_template_print (c, FP_DEVICE (dev), bytes));
  fp_print_set_username (first, "user-a"); fp_print_set_username (second, "user-b");
  fp_print_set_finger (first, FP_FINGER_RIGHT_INDEX); fp_print_set_finger (second, FP_FINGER_RIGHT_INDEX);
  g_assert_cmpint (file_storage_print_data_save (first), ==, 0);
  g_assert_cmpint (file_storage_print_data_save (second), ==, 0);
  g_autoptr(FpPrint) loaded = NULL;
  g_assert_cmpint (file_storage_print_data_load (FP_DEVICE (dev), FP_FINGER_RIGHT_INDEX, "user-a", &loaded), ==, 0);
  g_assert_cmpstr (fp_print_get_username (loaded), ==, "user-a");
  g_assert_true (goodix_chicago_validate_bound_print (c, loaded, &error)); g_clear_object (&loaded);
  g_assert_cmpint (file_storage_print_data_delete (FP_DEVICE (dev), FP_FINGER_RIGHT_INDEX, "user-a"), ==, 0);
  g_assert_cmpint (file_storage_print_data_load (FP_DEVICE (dev), FP_FINGER_RIGHT_INDEX, "user-a", &loaded), ==, -ENOENT);
  g_assert_null (loaded);
  g_assert_cmpint (file_storage_print_data_load (FP_DEVICE (dev), FP_FINGER_RIGHT_INDEX, "user-b", &loaded), ==, 0);
  g_assert_cmpstr (fp_print_get_username (loaded), ==, "user-b");
  g_assert_true (goodix_chicago_validate_bound_print (c, loaded, &error));
  g_autofree gchar *after = NULL; gsize after_size;
  g_assert_true (g_file_get_contents (legacy_path, &after, &after_size, &error));
  g_assert_cmpuint (before_size, ==, after_size);
  g_assert_cmpint (memcmp (before, after, before_size), ==, 0);
  g_assert_no_error (error);
  g_assert_cmpint (file_storage_deinit (), ==, 0);
  g_unsetenv ("STATE_DIRECTORY"); remove_fixture (directory); goodix_chicago_free (c);
}
#endif
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/chicago/serialization", test_serialization);
  g_test_add_func ("/chicago/boundaries", test_boundaries);
  g_test_add_func ("/chicago/corruption-legacy", test_corruption);
  g_test_add_func ("/chicago/envelope-negative", test_envelope_negative);
  g_test_add_func ("/chicago/cancel-cleanup", test_cancel_cleanup);
#ifdef GOODIX_CHICAGO_STORAGE_TEST
  g_test_add_func ("/chicago/stock-storage-multiuser-delete", test_stock_storage);
#endif
  return g_test_run ();
}
