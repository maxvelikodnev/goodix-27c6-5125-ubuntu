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
#include <math.h>
#include <unistd.h>
#include "521d-calidata.h"

typedef struct {
  gchar *root, *development, *directory, *data_path, *receipt_path;
  guint8 *envelope;
} SyntheticSession;

static void
write_synthetic_session (SyntheticSession *session)
{
  g_autofree gchar *full = g_compute_checksum_for_data (G_CHECKSUM_SHA256, session->envelope, GOODIX_MILAN_CALIDATA_ENVELOPE);
  g_autofree gchar *payload = g_compute_checksum_for_data (G_CHECKSUM_SHA256, session->envelope + 16, GOODIX_MILAN_CALIDATA_ENVELOPE - 16);
  g_autofree gchar *receipt = g_strdup_printf (
    "[M5B]\nschema=1\npurpose=M5.B_CAPTURE_ONLY\nsource=M3_USER_DECLARED_WINDOWS_COPY\n"
    "device_binding=UNKNOWN\nprefix_semantics=UNKNOWN\nenvelope_sha256=%s\npayload_sha256=%s\n", full, payload);
  g_assert_true (g_file_set_contents (session->data_path, (const gchar *) session->envelope, GOODIX_MILAN_CALIDATA_ENVELOPE, NULL));
  g_assert_cmpint (g_chmod (session->data_path, 0600), ==, 0);
  g_assert_true (g_file_set_contents (session->receipt_path, receipt, -1, NULL));
  g_assert_cmpint (g_chmod (session->receipt_path, 0600), ==, 0);
}

static SyntheticSession
synthetic_session (void)
{
  SyntheticSession session = { 0 };
  Gx521dCalidata *fields = g_new0 (Gx521dCalidata, 1);
  session.root = g_dir_make_tmp ("goodix-m5b-synthetic-XXXXXX", NULL);
  session.development = g_build_filename (session.root, "development", NULL);
  session.directory = g_build_filename (session.development, "session", NULL);
  g_assert_cmpint (g_mkdir (session.development, 0700), ==, 0);
  g_assert_cmpint (g_mkdir (session.directory, 0700), ==, 0);
  session.data_path = g_build_filename (session.directory, "calidata.dat", NULL);
  session.receipt_path = g_build_filename (session.directory, "receipt.ini", NULL);
  session.envelope = g_malloc0 (GOODIX_MILAN_CALIDATA_ENVELOPE);
  fields->sample_count = 400;
  for (guint i = 0; i < GOODIX_MILAN_PIXELS; i++) fields->calibration[i] = fields->setup[i] = 2000;
  g_assert_cmpint (gx521d_calidata_store (fields, session.envelope + 16, GX521D_CALIDATA_BYTES), ==, 0);
  g_free (fields);
  write_synthetic_session (&session);
  return session;
}

static void
clear_synthetic_session (SyntheticSession *session)
{
  const gchar *names[] = { "calidata.dat", "receipt.ini", "capture.started", "capture.raw-u16le", "capture.preprocessed-u8", "metrics.ini" };
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    { g_autofree gchar *path = g_build_filename (session->directory, names[i], NULL); g_unlink (path); }
  g_assert_cmpint (g_rmdir (session->directory), ==, 0);
  g_assert_cmpint (g_rmdir (session->development), ==, 0);
  g_assert_cmpint (g_rmdir (session->root), ==, 0);
  g_free (session->root); g_free (session->development); g_free (session->directory);
  g_free (session->data_path); g_free (session->receipt_path); g_free (session->envelope);
}

static void
assert_synthetic_rejected (SyntheticSession *session)
{
  g_autoptr(GError) error = NULL;
  g_assert_null (goodix_milan_test_load (session->directory, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
}

static void
test_experimental_loader (void)
{
  SyntheticSession session = synthetic_session ();
  g_autoptr(GError) error = NULL;
  GoodixMilan *milan = goodix_milan_test_load (session.directory, &error);
  g_assert_nonnull (milan); g_assert_no_error (error);
  g_assert_true (goodix_milan_is_diagnostic (milan));
  g_assert_false (goodix_milan_enroll_start (milan, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID); g_clear_error (&error);
  goodix_milan_free (milan);
  g_assert_cmpint (g_chmod (session.directory, 0755), ==, 0); assert_synthetic_rejected (&session);
  g_assert_cmpint (g_chmod (session.directory, 0700), ==, 0);
  g_assert_cmpint (g_chmod (session.data_path, 0644), ==, 0); assert_synthetic_rejected (&session);
  g_assert_cmpint (g_chmod (session.data_path, 0600), ==, 0);
  g_assert_cmpint (g_chmod (session.receipt_path, 0644), ==, 0); assert_synthetic_rejected (&session);
  g_assert_cmpint (g_chmod (session.receipt_path, 0600), ==, 0);
  g_autofree gchar *saved = g_strconcat (session.data_path, ".saved", NULL);
  g_assert_cmpint (g_rename (session.data_path, saved), ==, 0);
  g_assert_cmpint (symlink (saved, session.data_path), ==, 0); assert_synthetic_rejected (&session);
  g_assert_cmpint (g_unlink (session.data_path), ==, 0);
  g_assert_cmpint (g_rename (saved, session.data_path), ==, 0);
  g_assert_cmpint (link (session.data_path, saved), ==, 0); assert_synthetic_rejected (&session);
  g_assert_cmpint (g_unlink (saved), ==, 0);
  g_assert_cmpint (truncate (session.data_path, GOODIX_MILAN_CALIDATA_ENVELOPE - 1), ==, 0); assert_synthetic_rejected (&session);
  write_synthetic_session (&session);
  session.envelope[0] = 1;
  g_assert_true (g_file_set_contents (session.data_path, (const gchar *) session.envelope, GOODIX_MILAN_CALIDATA_ENVELOPE, NULL));
  assert_synthetic_rejected (&session); session.envelope[0] = 0;
  /* Rehash malformed inputs, proving native format checks are independently
   * required even when a trusted receipt agrees with the envelope. */
  const guint offsets[] = { 16, 20, 16 + 0x22490, 16 + 0x18488 + 3, 16 + 0x1848c };
  for (guint i = 0; i < G_N_ELEMENTS (offsets); i++)
    {
      session.envelope[offsets[i]] ^= 0xff; write_synthetic_session (&session);
      assert_synthetic_rejected (&session); session.envelope[offsets[i]] ^= 0xff;
    }
  write_synthetic_session (&session);
  g_assert_true (g_file_set_contents (session.receipt_path, "[M5B]\nschema=1\ndevice_binding=QUALIFIED\n", -1, NULL));
  assert_synthetic_rejected (&session);
  write_synthetic_session (&session);
  if (geteuid () != 0) { g_assert_null (goodix_milan_load (session.directory, &error)); g_assert_nonnull (error); g_clear_error (&error); }
  clear_synthetic_session (&session);
  g_print ("MILAN_M5B_LOADER synthetic_valid=1 permissions=1 symlink=1 hardlink=1 truncation=1 drift_hash=1 version=1 both_CRC=1 range=1 classifier=1 provenance=1 no_private_data=1\n");
}

typedef struct { gboolean done; FpImage *image; GError *error; } Capture;
static void capture_done (GObject *object, GAsyncResult *result, gpointer data)
{
  Capture *capture = data;
  capture->image = fp_device_capture_finish (FP_DEVICE (object), result, &capture->error);
  capture->done = TRUE;
}
static void pump_until_capture (Capture *capture)
{
  gint64 deadline = g_get_monotonic_time () + 3000000;
  while (!capture->done && g_get_monotonic_time () < deadline)
    { while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  g_assert_true (capture->done);
}

static void
test_experimental_capture_and_auth_gate (void)
{
  SyntheticSession session = synthetic_session ();
  g_autoptr(GError) error = NULL;
  g_autoptr(GoodixFpImageDevice) device = goodix_fpimage_device_new ();
  g_assert_true (fp_device_open_sync (FP_DEVICE (device), NULL, &error));
  GoodixDeviceContext *ctx = goodix_fpimage_device_get_context (device);
  gboolean armed = TRUE;
  g_signal_emit_by_name (device, "goodix-milan-diagnostic-arm", session.directory, &armed);
  g_assert_false (armed); /* Real entrypoint refuses host-only/fake transports. */
  g_assert_true (goodix_device_context_test_milan_diagnostic_arm (ctx, session.directory, &error));
  g_assert_false (goodix_device_context_test_milan_diagnostic_arm (ctx, session.directory, &error));
  g_autoptr(FpPrint) print = g_object_ref_sink (fp_print_new (FP_DEVICE (device)));
  g_autoptr(FpPrint) result = fp_device_enroll_sync (FP_DEVICE (device), print, NULL, NULL, NULL, &error);
  g_assert_null (result); g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED); g_clear_error (&error);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (ctx), ==, 0);
  gboolean match = FALSE;
  g_autoptr(GBytes) marker = g_bytes_new_static ("synthetic-raw", 13);
  g_autoptr(FpPrint) reference = g_object_ref_sink (goodix_milan_template_print (FP_DEVICE (device), marker));
  /* The stock image core retains ACTIVATING after an early refusal. Its
   * warning on a second refused action is expected; no resources were armed. */
  g_test_expect_message ("libfprint-image_device", G_LOG_LEVEL_WARNING, "*ACTIVATING to FPI_IMAGE_DEVICE_STATE_ACTIVATING*");
  g_assert_false (fp_device_verify_sync (FP_DEVICE (device), reference, NULL, NULL, NULL, &match, NULL, &error));
  g_test_assert_expected_messages ();
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED); g_clear_error (&error);
  g_autoptr(GPtrArray) gallery = g_ptr_array_new ();
  g_ptr_array_add (gallery, reference);
  g_test_expect_message ("libfprint-image_device", G_LOG_LEVEL_WARNING, "*ACTIVATING to FPI_IMAGE_DEVICE_STATE_ACTIVATING*");
  g_assert_false (fp_device_identify_sync (FP_DEVICE (device), gallery, NULL, NULL, NULL, NULL, NULL, &error));
  g_test_assert_expected_messages ();
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED); g_clear_error (&error);
  g_assert_false (goodix_device_context_test_milan_pairing_activation (ctx, &error));
  g_assert_nonnull (strstr (error->message, "active pairing")); g_clear_error (&error);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (ctx), ==, 0);
  Capture capture = { 0 };
  g_test_expect_message ("libfprint-image_device", G_LOG_LEVEL_WARNING, "*ACTIVATING to FPI_IMAGE_DEVICE_STATE_ACTIVATING*");
  fp_device_capture (FP_DEVICE (device), TRUE, NULL, capture_done, &capture);
  g_test_assert_expected_messages ();
  goodix_device_context_emit_arm_complete (ctx, NULL);
  uint16_t raw[GOODIX_MILAN_PIXELS];
  for (guint i = 0; i < GOODIX_MILAN_PIXELS; i++)
    raw[i] = (uint16_t) (2000.0 + 250.0 * sin ((double) (i % 80) * 0.75) + 70.0 * sin ((double) (i / 80) * 0.29));
  goodix_device_context_emit_finger_down (ctx);
  goodix_device_context_emit_image_ready (ctx, raw, GOODIX_MILAN_PIXELS);
  goodix_device_context_emit_release_tail_complete (ctx);
  goodix_device_context_emit_finger_up_ready (ctx);
  pump_until_capture (&capture);
  /* A synthetic image exercises successful preprocessing/capture and release;
   * its quality or features are not biometric evidence. */
  g_autofree gchar *raw_path = g_build_filename (session.directory, "capture.raw-u16le", NULL);
  g_autofree gchar *metrics_path = g_build_filename (session.directory, "metrics.ini", NULL);
  g_assert_true (g_file_test (raw_path, G_FILE_TEST_IS_REGULAR));
  g_assert_true (g_file_test (metrics_path, G_FILE_TEST_IS_REGULAR));
  g_assert_nonnull (capture.image); g_assert_no_error (capture.error);
  g_assert_cmpint (fp_image_get_width (capture.image), ==, 80);
  g_clear_object (&capture.image); g_clear_error (&capture.error);
  g_assert_cmpuint (goodix_in_memory_backend_get_rearm_count (ctx), ==, 0);
  guint commands = goodix_device_context_get_backend_command_count (ctx);
  capture = (Capture) { 0 };
  fp_device_capture (FP_DEVICE (device), TRUE, NULL, capture_done, &capture);
  pump_until_capture (&capture); g_assert_null (capture.image); g_assert_nonnull (capture.error); g_clear_error (&capture.error);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (ctx), ==, commands);
  g_assert_true (fp_device_close_sync (FP_DEVICE (device), NULL, &error)); g_assert_no_error (error);
  g_assert_null (goodix_fpimage_device_get_context (device));
  GoodixMilan *another = goodix_milan_test_load (session.directory, &error);
  g_assert_nonnull (another); g_assert_no_error (error);
  g_assert_false (goodix_milan_begin_capture (another, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID); g_clear_error (&error);
  goodix_milan_free (another);
  g_assert_true (fp_device_open_sync (FP_DEVICE (device), NULL, &error));
  g_clear_object (&result);
  g_clear_object (&print);
  print = g_object_ref_sink (fp_print_new (FP_DEVICE (device)));
  result = fp_device_enroll_sync (FP_DEVICE (device), print, NULL, NULL, NULL, &error);
  g_assert_null (result); g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED); g_clear_error (&error);
  ctx = goodix_fpimage_device_get_context (device);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (ctx), ==, 0);
  g_assert_true (fp_device_close_sync (FP_DEVICE (device), NULL, &error)); g_assert_no_error (error);
  clear_synthetic_session (&session);
  g_print ("MILAN_M5B_ACTION explicit_signal_no_fake=1 auth_blocked_before_commands=1 pairing_guard_before_factory=1 one_capture=1 cross_instance_budget=1 private_results=1 no_rearm=1 close_reopen_sticky=1\n");
}

static void
test_experimental_cancel (void)
{
  SyntheticSession session = synthetic_session ();
  g_autoptr(GError) error = NULL;
  g_autoptr(GoodixFpImageDevice) device = goodix_fpimage_device_new ();
  g_autoptr(GCancellable) cancel = g_cancellable_new ();
  g_assert_true (fp_device_open_sync (FP_DEVICE (device), NULL, &error));
  GoodixDeviceContext *ctx = goodix_fpimage_device_get_context (device);
  g_assert_true (goodix_device_context_test_milan_diagnostic_arm (ctx, session.directory, &error));
  Capture capture = { 0 };
  fp_device_capture (FP_DEVICE (device), TRUE, cancel, capture_done, &capture);
  g_cancellable_cancel (cancel);
  pump_until_capture (&capture);
  g_assert_null (capture.image); g_assert_error (capture.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&capture.error);
  g_assert_cmpuint (goodix_in_memory_backend_get_rearm_count (ctx), ==, 0);
  g_assert_true (fp_device_close_sync (FP_DEVICE (device), NULL, &error));
  g_assert_no_error (error); g_assert_null (goodix_fpimage_device_get_context (device));
  clear_synthetic_session (&session);
  g_print ("MILAN_M5B_CANCEL terminal_once=1 no_rearm=1 close_released=1\n");
}

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
  g_assert_null (goodix_milan_load (NULL, &error));
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
  const FpiDeviceAction actions[] = { FPI_DEVICE_ACTION_ENROLL, FPI_DEVICE_ACTION_VERIFY, FPI_DEVICE_ACTION_IDENTIFY };
  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    {
      g_assert_true (goodix_fpimage_device_test_action_may_start_without_material (actions[i], GOODIX_RUNTIME_COORDINATOR_DECISION_NEEDS_INITIALIZATION));
      g_assert_true (goodix_fpimage_device_test_action_may_start_without_material (actions[i], GOODIX_RUNTIME_COORDINATOR_DECISION_NEEDS_LIVE_PREFLIGHT));
    }
  g_assert_false (goodix_fpimage_device_test_action_may_start_without_material (FPI_DEVICE_ACTION_NONE, GOODIX_RUNTIME_COORDINATOR_DECISION_NEEDS_INITIALIZATION));
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
  g_test_add_func ("/milan-contract/experimental-loader", test_experimental_loader);
  g_test_add_func ("/milan-contract/experimental-capture-auth-gate", test_experimental_capture_and_auth_gate);
  g_test_add_func ("/milan-contract/experimental-cancel", test_experimental_cancel);
  return g_test_run ();
}
