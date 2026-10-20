/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Synthetic boundary tests, using real libfprint and stock fprintd file storage.
 * No sensor, privileged path, calibration, native template or biometric corpus.
 */
#include "../goodix_fpimage_device.h"
#include "../goodix_milan.h"
#include "fp-print.h"
#include "fpi-print.h"
#include "file_storage.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

static void
test_identity_and_storage (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GUsbDevice) usb = g_object_new (G_USB_TYPE_DEVICE, NULL);
  g_autoptr(GoodixFpImageDevice) device = goodix_fpimage_device_new_for_usb (usb);
  g_autofree gchar *directory = g_dir_make_tmp ("goodix-milan-contract-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (directory);
  g_assert_true (g_setenv ("STATE_DIRECTORY", directory, TRUE));
  g_assert_cmpstr (fp_device_get_driver (FP_DEVICE (device)), ==, "goodix_27c6_5125_milan");
  g_assert_cmpuint (fpi_device_goodix_27c6_5125_get_type (), ==, G_OBJECT_TYPE (device));

  const gchar *id = fp_device_get_device_id (FP_DEVICE (device));
  g_autoptr(FpPrint) legacy = g_object_ref_sink (g_object_new (FP_TYPE_PRINT,
      "driver", "goodix_27c6_5125", "device-id", id, NULL));
  fpi_print_set_type (legacy, FPI_PRINT_RAW);
  g_object_set (legacy, "fpi-data", g_variant_new_string ("GSF1-synthetic-historical"), NULL);
  fp_print_set_username (legacy, "synthetic");
  fp_print_set_finger (legacy, FP_FINGER_RIGHT_INDEX);
  g_assert_false (fp_print_compatible (legacy, FP_DEVICE (device)));
  g_assert_cmpint (file_storage_print_data_save (legacy), ==, 0);
  g_autofree gchar *old_path = g_build_filename (directory, "synthetic",
      "goodix_27c6_5125", id, "7", NULL);
  g_autofree gchar *before = NULL;
  gsize before_size;
  g_assert_true (g_file_get_contents (old_path, &before, &before_size, &error));
  g_assert_no_error (error);
  g_assert_null (file_storage_discover_prints (FP_DEVICE (device), "synthetic"));
  g_autoptr(FpPrint) loaded = NULL;
  g_assert_cmpint (file_storage_print_data_load (FP_DEVICE (device),
      FP_FINGER_RIGHT_INDEX, "synthetic", &loaded), ==, -ENOENT);
  g_assert_null (loaded);

  /* Only a storage fixture, not a native template or successful enrollment. */
  g_autoptr(GBytes) marker = g_bytes_new_static ("synthetic-milan-storage", 23);
  g_autoptr(FpPrint) milan = g_object_ref_sink (goodix_milan_template_print (FP_DEVICE (device), marker));
  fp_print_set_username (milan, "synthetic");
  fp_print_set_finger (milan, FP_FINGER_RIGHT_INDEX);
  g_assert_true (fp_print_compatible (milan, FP_DEVICE (device)));
  g_assert_cmpint (file_storage_print_data_save (milan), ==, 0);
  g_autoptr(GSList) fingers = file_storage_discover_prints (FP_DEVICE (device), "synthetic");
  g_assert_cmpuint (g_slist_length (fingers), ==, 1);
  g_assert_cmpuint (GPOINTER_TO_UINT (fingers->data), ==, FP_FINGER_RIGHT_INDEX);
  g_assert_cmpint (file_storage_print_data_load (FP_DEVICE (device),
      FP_FINGER_RIGHT_INDEX, "synthetic", &loaded), ==, 0);
  g_assert_true (fp_print_equal (loaded, milan));
  g_assert_true (fp_print_compatible (loaded, FP_DEVICE (device)));

  g_autofree gchar *after = NULL;
  gsize after_size;
  g_assert_true (g_file_get_contents (old_path, &after, &after_size, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (before_size, ==, after_size);
  g_assert_cmpint (memcmp (before, after, before_size), ==, 0);
  g_assert_false (goodix_milan_validate_print (legacy, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  g_clear_error (&error);

  /* All fixture files are synthetic and confined to our fresh temp tree. */
  g_assert_cmpint (file_storage_print_data_delete (FP_DEVICE (device),
      FP_FINGER_RIGHT_INDEX, "synthetic"), ==, 0);
  g_assert_true (g_file_test (old_path, G_FILE_TEST_EXISTS));
  g_assert_cmpint (g_unlink (old_path), ==, 0);
  g_autofree gchar *old_device = g_path_get_dirname (old_path);
  g_autofree gchar *old_driver = g_path_get_dirname (old_device);
  g_autofree gchar *user = g_path_get_dirname (old_driver);
  g_assert_cmpint (g_rmdir (old_device), ==, 0);
  g_assert_cmpint (g_rmdir (old_driver), ==, 0);
  g_assert_cmpint (g_rmdir (user), ==, 0);
  g_assert_cmpint (g_rmdir (directory), ==, 0);
  g_assert_cmpint (file_storage_deinit (), ==, 0);
  g_unsetenv ("STATE_DIRECTORY");
  g_print ("MILAN_STORAGE stock_fprintd=1 legacy_ignored=1 same_finger_new_namespace=1 reload=1 legacy_unchanged=1\n");
}

static void
test_fail_closed_calibration (void)
{
  g_autoptr(GError) error = NULL;
  g_assert_null (goodix_milan_load (&error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED);
  g_assert_nonnull (strstr (error->message, "binding"));
  g_clear_error (&error);
  g_assert_null (goodix_milan_new (NULL, 0, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  g_clear_error (&error);
  const guint8 invalid[] = { 0 };
  g_assert_null (goodix_milan_new (invalid, sizeof invalid, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  g_print ("MILAN_CALIBRATION unqualified_source_binding_rejected=1 missing_rejected=1 invalid_size_rejected=1\n");
}

static void
test_pairing_shared_validator (void)
{
  g_autoptr(GoodixFpImageDevice) device = goodix_fpimage_device_new ();
  GoodixDeviceContext *ctx = goodix_fpimage_device_get_context (device);
  g_autoptr(GError) error = NULL;
  /* This seam supplies zero evidence. The common factory rejects it before
   * opening paths, generating keys, writing a journal or submitting USB. */
  g_assert_false (goodix_device_context_test_milan_pairing_activation (ctx, &error));
  g_assert_nonnull (error);
  g_assert_cmpstr (g_quark_to_string (error->domain), ==, "goodix-pairing-activation-error");
  g_assert_cmpstr (error->message, ==, "pairing activation inputs are invalid");
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (ctx), ==, 0);
  g_print ("MILAN_PAIRING shared_validator_reached=1 invalid_evidence_no_command=1\n");
}

static void
test_missing_calibration_action (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GoodixFpImageDevice) device = goodix_fpimage_device_new ();
  g_assert_true (fp_device_open_sync (FP_DEVICE (device), NULL, &error));
  g_assert_no_error (error);
  g_autoptr(FpPrint) print = g_object_ref_sink (fp_print_new (FP_DEVICE (device)));
  g_autoptr(FpPrint) result = fp_device_enroll_sync (FP_DEVICE (device), print,
      NULL, NULL, NULL, &error);
  g_assert_null (result);
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (
      goodix_fpimage_device_get_context (device)), ==, 0);
  g_clear_error (&error);
  g_assert_true (fp_device_close_sync (FP_DEVICE (device), NULL, &error));
  g_assert_no_error (error);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/milan-contract/storage-identity", test_identity_and_storage);
  g_test_add_func ("/milan-contract/calibration", test_fail_closed_calibration);
  g_test_add_func ("/milan-contract/pairing-shared-validator", test_pairing_shared_validator);
  g_test_add_func ("/milan-contract/missing-calibration-action", test_missing_calibration_action);
  return g_test_run ();
}
