/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Real libfprint public actions, existing in-memory 5125 backend, pinned C1.
 * Explicit private M3 file routes only; never opens USB or the production path.
 */
#include "../goodix_fpimage_device.h"
#include "../goodix_milan.h"
#include "fp-device.h"
#include "fp-print.h"
#include "fpi-print.h"
#include <glib.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <string.h>

typedef struct {
  gchar *role, *id, *path, *image_sha256;
  gint quality, coverage, feature_count, tag;
} Frame;
typedef struct {
  GoodixFpImageDevice *device;
  GoodixDeviceContext *ctx;
  gboolean done, success, match;
  guint completions, progress, retries, reports;
  FpPrint *print;
  GError *error;
} Fixture;
static GPtrArray *frames;
static gchar *calibration;
static gsize calibration_size;
static FpPrint *enrolled;
static guint accepted_attempts;

static void pump (void) { while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, FALSE); }
static void wait_done (Fixture *f)
{
  gint64 deadline = g_get_monotonic_time () + 5000000;
  while (!f->done && g_get_monotonic_time () < deadline) { pump (); g_usleep (1000); }
  g_assert_true (f->done); g_assert_cmpuint (f->completions, ==, 1);
}
static void enroll_done (GObject *object, GAsyncResult *result, gpointer data)
{
  Fixture *f = data;
  f->print = fp_device_enroll_finish (FP_DEVICE (object), result, &f->error);
  f->success = f->print != NULL; f->done = TRUE; f->completions++;
}
static void verify_done (GObject *object, GAsyncResult *result, gpointer data)
{
  Fixture *f = data;
  g_autoptr(FpPrint) scanned = NULL;
  f->success = fp_device_verify_finish (FP_DEVICE (object), result, &f->match, &scanned, &f->error);
  f->done = TRUE; f->completions++;
}
static void identify_done (GObject *object, GAsyncResult *result, gpointer data)
{
  Fixture *f = data;
  g_autoptr(FpPrint) scanned = NULL;
  g_autoptr(FpPrint) matched = NULL;
  f->success = fp_device_identify_finish (FP_DEVICE (object), result, &matched, &scanned, &f->error);
  f->match = matched != NULL;
  if (matched) g_assert_true (matched == enrolled);
  f->done = TRUE; f->completions++;
}
static void progress (FpDevice *device, gint stages, FpPrint *print, gpointer data, GError *error)
{
  Fixture *f = data;
  (void) device; (void) print;
  if (error) { g_assert_error (error, FP_DEVICE_RETRY, error->code); f->retries++; g_assert_cmpint (stages, ==, (gint) f->progress); }
  else { g_assert_cmpint (stages, ==, (gint) f->progress + 1); f->progress++; }
}
static Fixture fixture (gboolean with_calibration)
{
  Fixture f = { 0 };
  g_autoptr(GError) error = NULL;
  f.device = goodix_fpimage_device_new ();
  g_assert_true (fp_device_open_sync (FP_DEVICE (f.device), NULL, &error));
  g_assert_no_error (error);
  f.ctx = goodix_fpimage_device_get_context (f.device);
  if (with_calibration) {
    g_assert_true (goodix_device_context_set_milan_calibration (f.ctx,
        (const guint8 *) calibration + 16, calibration_size - 16, &error));
    g_assert_no_error (error);
  }
  return f;
}
static void clear_fixture (Fixture *f)
{
  g_autoptr(GError) error = NULL;
  g_assert_true (fp_device_close_sync (FP_DEVICE (f->device), NULL, &error));
  g_assert_no_error (error);
  g_assert_null (goodix_fpimage_device_get_context (f->device));
  g_clear_object (&f->device); /* Regression: finalize after close has no context. */
  g_clear_object (&f->print); g_clear_error (&f->error);
}
static void begin_enroll (Fixture *f, GCancellable *cancel)
{
  g_autoptr(FpPrint) print = g_object_ref_sink (fp_print_new (FP_DEVICE (f->device)));
  fp_device_enroll (FP_DEVICE (f->device), print, cancel, progress, f, NULL, enroll_done, f);
  goodix_device_context_emit_arm_complete (f->ctx, NULL);
}
static void feed (Fixture *f, Frame *frame, gboolean release)
{
  g_autofree gchar *raw = NULL;
  gsize size;
  g_autoptr(GError) error = NULL;
  uint16_t samples[GOODIX_MILAN_PIXELS];
  g_assert_true (g_file_get_contents (frame->path, &raw, &size, &error));
  g_assert_no_error (error); g_assert_cmpuint (size, ==, sizeof samples);
  for (guint i = 0; i < GOODIX_MILAN_PIXELS; i++) {
    guint16 word; memcpy (&word, raw + i * 2, 2); samples[i] = GUINT16_FROM_LE (word);
  }
  goodix_device_context_emit_finger_down (f->ctx);
  goodix_device_context_emit_image_ready (f->ctx, samples, GOODIX_MILAN_PIXELS);
  if (release) {
    goodix_device_context_emit_release_tail_complete (f->ctx);
    goodix_device_context_emit_finger_up_ready (f->ctx);
  }
  pump ();
}
static gboolean enrollment_frame (Frame *frame)
{ return !strcmp (frame->role, "enroll") || !strcmp (frame->role, "reserve"); }
static void test_enrollment_and_reload (void)
{
  Fixture f = fixture (TRUE);
  begin_enroll (&f, NULL);
  for (guint i = 0; i < frames->len && !f.done; i++) {
    Frame *frame = g_ptr_array_index (frames, i);
    if (!enrollment_frame (frame)) continue;
    accepted_attempts++; feed (&f, frame, TRUE);
  }
  wait_done (&f); g_assert_true (f.success); g_assert_no_error (f.error);
  g_assert_cmpuint (f.progress, ==, 18); g_assert_cmpuint (f.retries, >, 0);
  g_assert_cmpuint (accepted_attempts, >, f.progress);
  g_autofree guchar *serialized = NULL; gsize size;
  g_autoptr(GError) error = NULL;
  g_assert_true (fp_print_serialize (f.print, &serialized, &size, &error));
  enrolled = fp_print_deserialize (serialized, size, &error);
  g_assert_no_error (error); g_assert_nonnull (enrolled); g_assert_true (fp_print_equal (enrolled, f.print));
  g_assert_true (goodix_milan_validate_print (enrolled, &error)); g_assert_no_error (error);
  g_autoptr(GVariant) data = NULL, packet = NULL;
  g_object_get (enrolled, "fpi-data", &data, NULL);
  const gchar *identity; guint version;
  g_variant_get (data, "(&su@ay)", &identity, &version, &packet);
  gsize packet_size; const guint8 *packet_data = g_variant_get_fixed_array (packet, &packet_size, 1);
  g_autofree gchar *expected = NULL; gsize expected_size;
  g_assert_true (g_file_get_contents (g_getenv ("GOODIX_MILAN_EXPECTED_TEMPLATE"), &expected, &expected_size, &error));
  g_assert_no_error (error); g_assert_cmpuint (packet_size, ==, expected_size);
  g_assert_cmpint (memcmp (packet_data, expected, expected_size), ==, 0);
  g_print ("MILAN_ENROLL accepted=%u attempts=%u retries=%u native_M3_template_equal=1 FpPrint_reload_equal=1\n",
      f.progress, accepted_attempts, f.retries);
  clear_fixture (&f);
}
static void test_real_corpus_actions (void)
{
  g_assert_nonnull (enrolled);
  guint genuine = 0, negative = 0, partial = 0;
  for (guint i = 0; i < frames->len; i++) {
    Frame *frame = g_ptr_array_index (frames, i);
    if (enrollment_frame (frame)) continue;
    for (guint identify = 0; identify < 2; identify++) {
      Fixture f = fixture (TRUE);
      g_autoptr(GPtrArray) gallery = g_ptr_array_new ();
      g_ptr_array_add (gallery, enrolled);
      g_autofree guchar *before = NULL, *after = NULL; gsize n_before, n_after;
      g_autoptr(GError) error = NULL;
      g_assert_true (fp_print_serialize (enrolled, &before, &n_before, &error));
      if (identify)
        fp_device_identify (FP_DEVICE (f.device), gallery, NULL, NULL, NULL, NULL, identify_done, &f);
      else fp_device_verify (FP_DEVICE (f.device), enrolled, NULL, NULL, NULL, NULL, verify_done, &f);
      goodix_device_context_emit_arm_complete (f.ctx, NULL);
      feed (&f, frame, TRUE); wait_done (&f);
      g_assert_true (f.success); g_assert_no_error (f.error);
      g_assert_cmpint (f.match, ==, !strcmp (frame->role, "genuine"));
      g_assert_true (fp_print_serialize (enrolled, &after, &n_after, &error));
      g_assert_no_error (error); g_assert_cmpuint (n_after, ==, n_before); g_assert_cmpint (memcmp (before, after, n_before), ==, 0);
      clear_fixture (&f);
    }
    if (!strcmp (frame->role, "genuine")) genuine++;
    else if (!strcmp (frame->role, "negative")) negative++;
    else partial++;
  }
  g_print ("MILAN_ACTIONS genuine=%u negative=%u partial_no_match=%u each_verify_and_identify=1 FpPrint_unchanged=1\n", genuine, negative, partial);
}
static FpPrint *bad_print (FpDevice *device, const gchar *identity, guint version, const guint8 *bytes, gsize size)
{
  FpPrint *print = fp_print_new (device); fpi_print_set_type (print, FPI_PRINT_RAW);
  g_object_set (print, "fpi-data", g_variant_new ("(su@ay)", identity, version,
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, bytes, size, 1)), NULL);
  return print;
}
static void test_invalid_templates_and_calibration (void)
{
  const guint8 gsf[] = "GSF1not-a-milan-template";
  Fixture f = fixture (TRUE);
  g_autoptr(GVariant) data = NULL, packet = NULL;
  g_object_get (enrolled, "fpi-data", &data, NULL);
  const gchar *identity; guint version; gsize size;
  g_variant_get (data, "(&su@ay)", &identity, &version, &packet);
  const guint8 *bytes = g_variant_get_fixed_array (packet, &size, 1);
  for (guint i = 0; i < 5; i++) {
    g_autoptr(GError) error = NULL;
    g_autoptr(FpPrint) bad = bad_print (FP_DEVICE (f.device), i == 1 ? "GSF1" : identity,
        i == 2 ? 2 : 1, i == 0 ? gsf : bytes, i == 0 ? sizeof gsf : i == 3 ? size - 1 : size);
    if (i == 4) {
      g_clear_object (&bad); bad = fp_print_new (FP_DEVICE (f.device));
      fpi_print_set_type (bad, FPI_PRINT_NBIS);
    }
    g_assert_false (goodix_milan_validate_print (bad, &error));
    g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  }
  g_autoptr(FpPrint) bad = bad_print (FP_DEVICE (f.device), identity, 1, gsf, sizeof gsf);
  g_autoptr(GPtrArray) gallery = g_ptr_array_new ();
  g_ptr_array_add (gallery, enrolled); g_ptr_array_add (gallery, bad);
  fp_device_identify (FP_DEVICE (f.device), gallery, NULL, NULL, NULL, NULL, identify_done, &f);
  wait_done (&f); g_assert_false (f.success); g_assert_error (f.error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (f.ctx), ==, 0);
  clear_fixture (&f);
  f = fixture (FALSE);
  g_autoptr(FpPrint) print = g_object_ref_sink (fp_print_new (FP_DEVICE (f.device)));
  fp_device_enroll (FP_DEVICE (f.device), print, NULL, progress, &f, NULL, enroll_done, &f);
  wait_done (&f); g_assert_false (f.success); g_assert_error (f.error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (f.ctx), ==, 0);
  clear_fixture (&f);
  g_print ("MILAN_REJECTIONS GSF1=1 identity=1 version=1 truncated=1 legacy_type=1 mixed_gallery_before_acquire=1 missing_cal_before_acquire=1\n");
}
static void test_cancel_and_hold (void)
{
  for (guint cancelled = 0; cancelled < 2; cancelled++) {
    Fixture f = fixture (TRUE);
    g_autoptr(GCancellable) cancel = g_cancellable_new ();
    begin_enroll (&f, cancel);
    guint attempts = 0;
    for (guint i = 0; i < frames->len && attempts < accepted_attempts; i++) {
      Frame *frame = g_ptr_array_index (frames, i); if (!enrollment_frame (frame)) continue;
      attempts++;
      if (attempts == accepted_attempts) fpi_image_device_hold_enroll_completion (FP_IMAGE_DEVICE (f.device));
      feed (&f, frame, TRUE);
    }
    g_assert_false (f.done); g_assert_cmpuint (f.progress, ==, 18);
    if (cancelled) g_cancellable_cancel (cancel);
    else fpi_image_device_release_enroll_completion (FP_IMAGE_DEVICE (f.device));
    wait_done (&f);
    if (cancelled) { g_assert_false (f.success); g_assert_error (f.error, G_IO_ERROR, G_IO_ERROR_CANCELLED); g_assert_null (f.print); }
    else { g_assert_true (f.success); g_assert_no_error (f.error); }
    guint commands = goodix_device_context_get_backend_command_count (f.ctx);
    uint16_t late[GOODIX_MILAN_PIXELS] = { 0 };
    goodix_device_context_emit_image_ready (f.ctx, late, GOODIX_MILAN_PIXELS);
    g_assert_cmpuint (goodix_device_context_get_backend_command_count (f.ctx), ==, commands);
    g_assert_cmpuint (f.completions, ==, 1);
    clear_fixture (&f);
  }
  g_print ("MILAN_LIFECYCLE terminal_hold_release=1 terminal_hold_cancel=1 stale_image_fenced=1 close_finalize=1\n");
}
static void test_synthetic_contracts (void)
{
  g_autoptr(GError) error = NULL;
  GoodixMilan *milan = goodix_milan_new ((const guint8 *) calibration + 16, calibration_size - 16, &error);
  g_assert_nonnull (milan); g_assert_no_error (error);
  GoodixMilanSample sample = { .quality = 80, .coverage = 100, .feature_count = 0 };
  g_assert_true (goodix_milan_enroll_start (milan, &error));
  g_autoptr(GBytes) bytes = NULL;
  g_assert_cmpint (goodix_milan_enroll (milan, &sample, &bytes, &error), ==, GOODIX_MILAN_RETRY);
  sample.feature_count = 10; sample.tag = 1;
  g_assert_cmpint (goodix_milan_enroll (milan, &sample, &bytes, &error), ==, GOODIX_MILAN_RETRY);
  sample.tag = 0; sample.quality = 24;
  g_assert_cmpint (goodix_milan_enroll (milan, &sample, &bytes, &error), ==, GOODIX_MILAN_RETRY);
  sample.quality = 80; sample.coverage = 64;
  g_assert_cmpint (goodix_milan_enroll (milan, &sample, &bytes, &error), ==, GOODIX_MILAN_RETRY);
  g_assert_cmpuint (goodix_milan_enroll_progress (milan), ==, 0);
  uint16_t raw[GOODIX_MILAN_PIXELS] = { 0 }; raw[0] = 4096;
  g_assert_false (goodix_5125_milan_preprocess (milan, raw, GOODIX_MILAN_PIXELS, &sample, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID); g_clear_error (&error);
  goodix_milan_free (milan);
  g_print ("MILAN_SYNTHETIC empty_tag_quality_coverage_retry=1 progress_unchanged=1 range_fail_closed=1\n");
}
static void test_all_preprocessing (void)
{
  g_autoptr(GError) error = NULL;
  GoodixMilan *milan = goodix_milan_new ((const guint8 *) calibration + 16, calibration_size - 16, &error);
  g_assert_nonnull (milan); g_assert_no_error (error);
  for (guint i = 0; i < frames->len; i++) {
    Frame *frame = g_ptr_array_index (frames, i);
    g_autofree gchar *raw = NULL; gsize size;
    g_assert_true (g_file_get_contents (frame->path, &raw, &size, &error)); g_assert_no_error (error);
    uint16_t samples[GOODIX_MILAN_PIXELS]; g_assert_cmpuint (size, ==, sizeof samples);
    for (guint n = 0; n < GOODIX_MILAN_PIXELS; n++) { guint16 word; memcpy (&word, raw + 2 * n, 2); samples[n] = GUINT16_FROM_LE (word); }
    GoodixMilanSample sample;
    g_assert_true (goodix_5125_milan_preprocess (milan, samples, GOODIX_MILAN_PIXELS, &sample, &error)); g_assert_no_error (error);
    g_assert_cmpint (sample.quality, ==, frame->quality); g_assert_cmpint (sample.coverage, ==, frame->coverage);
    g_assert_cmpint (sample.feature_count, ==, frame->feature_count); g_assert_cmpint (sample.tag, ==, frame->tag);
    g_autofree gchar *sha = g_compute_checksum_for_data (G_CHECKSUM_SHA256, sample.pixels, sizeof sample.pixels);
    g_assert_cmpstr (sha, ==, frame->image_sha256);
    g_assert_cmpint (memcmp (raw, samples, sizeof samples), ==, 0);
  }
  goodix_milan_free (milan);
  g_print ("MILAN_PREPROCESS all35_M3_pixel_hash_quality_coverage_features_tag_equal=1 raw_unchanged=1\n");
}
static void test_ordinary_retry_actions (void)
{
  Fixture f = fixture (TRUE);
  g_autoptr(GCancellable) cancel = g_cancellable_new ();
  begin_enroll (&f, cancel);
  uint16_t samples[GOODIX_MILAN_PIXELS] = { 0 };
  for (guint n = 0; n < 2; n++) {
    if (n) for (guint i = 0; i < GOODIX_MILAN_PIXELS; i++) samples[i] = 4095;
    goodix_device_context_emit_finger_down (f.ctx);
    goodix_device_context_emit_image_ready (f.ctx, samples, GOODIX_MILAN_PIXELS);
    goodix_device_context_emit_release_tail_complete (f.ctx);
    goodix_device_context_emit_finger_up_ready (f.ctx);
    pump (); g_assert_false (f.done); g_assert_cmpuint (f.progress, ==, 0);
  }
  g_assert_cmpuint (f.retries, ==, 2);
  g_cancellable_cancel (cancel); wait_done (&f); g_assert_false (f.success);
  g_assert_error (f.error, G_IO_ERROR, G_IO_ERROR_CANCELLED); clear_fixture (&f);
  f = fixture (TRUE);
  fp_device_verify (FP_DEVICE (f.device), enrolled, NULL, NULL, NULL, NULL, verify_done, &f);
  goodix_device_context_emit_arm_complete (f.ctx, NULL);
  memset (samples, 0, sizeof samples);
  goodix_device_context_emit_finger_down (f.ctx);
  goodix_device_context_emit_image_ready (f.ctx, samples, GOODIX_MILAN_PIXELS);
  g_assert_false (f.done); /* Even a retry observes the release boundary. */
  goodix_device_context_emit_release_tail_complete (f.ctx);
  goodix_device_context_emit_finger_up_ready (f.ctx);
  wait_done (&f); g_assert_false (f.success); g_assert_false (f.match);
  g_assert_error (f.error, FP_DEVICE_RETRY, FP_DEVICE_RETRY_GENERAL); clear_fixture (&f);
  f = fixture (TRUE);
  g_autoptr(GError) error = NULL;
  g_assert_false (goodix_device_context_test_milan_pairing_activation (f.ctx, &error));
  g_assert_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED);
  g_assert_cmpuint (goodix_device_context_get_backend_command_count (f.ctx), ==, 0);
  clear_fixture (&f);
  g_print ("MILAN_RETRY_ACTIONS zero_saturated_enroll_retry=1 no_progress=1 verify_retry_release=1 pairing_refused_before_command=1\n");
}
static void count_match_report (FpDevice *device, FpPrint *match, FpPrint *print,
                                gpointer data, GError *error)
{
  Fixture *f = data;
  (void) device; (void) match; (void) print; (void) error;
  f->reports++;
}
static FpiMatchResult cancel_inside_match (FpImageDevice *device, FpPrint *template_print,
                                             FpPrint *probe, GError **error)
{
  (void) template_print; (void) probe; (void) error;
  g_cancellable_cancel (fpi_device_get_cancellable (FP_DEVICE (device)));
  return FPI_MATCH_SUCCESS;
}
static void test_cancel_during_match (void)
{
  for (guint identify = 0; identify < 2; identify++) {
    Fixture f = fixture (TRUE);
    g_autoptr(GCancellable) cancel = g_cancellable_new ();
    g_autoptr(GPtrArray) gallery = g_ptr_array_new ();
    g_ptr_array_add (gallery, enrolled);
    FpImageDeviceClass *klass = FP_IMAGE_DEVICE_GET_CLASS (f.device);
    FpiMatchResult (*original) (FpImageDevice *, FpPrint *, FpPrint *, GError **) = klass->raw_match;
    klass->raw_match = cancel_inside_match;
    if (identify) fp_device_identify (FP_DEVICE (f.device), gallery, cancel, count_match_report, &f, NULL, identify_done, &f);
    else fp_device_verify (FP_DEVICE (f.device), enrolled, cancel, count_match_report, &f, NULL, verify_done, &f);
    goodix_device_context_emit_arm_complete (f.ctx, NULL);
    Frame *frame = g_ptr_array_index (frames, 0); feed (&f, frame, FALSE);
    wait_done (&f); g_assert_false (f.success); g_assert_false (f.match);
    g_assert_error (f.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_cmpuint (f.reports, ==, 0);
    klass->raw_match = original;
    clear_fixture (&f);
  }
  g_print ("MILAN_MATCH_CANCEL verify_and_identify_cancel_after_native_result=1 no_match_report=1 cleanup_once=1\n");
}
int main (int argc, char **argv)
{
  struct rlimit no_core = {0, 0};
  g_assert_cmpint (prctl (PR_SET_DUMPABLE, 0), ==, 0);
  g_assert_cmpint (setrlimit (RLIMIT_CORE, &no_core), ==, 0);
  g_test_init (&argc, &argv, NULL);
  const gchar *cal = g_getenv ("GOODIX_MILAN_CALIDATA_FILE");
  const gchar *routes = g_getenv ("GOODIX_MILAN_FRAME_LIST");
  g_assert_nonnull (cal); g_assert_nonnull (routes);
  g_autoptr(GError) error = NULL;
  g_assert_true (g_file_get_contents (cal, &calibration, &calibration_size, &error));
  g_assert_no_error (error); g_assert_cmpuint (calibration_size, ==, GOODIX_MILAN_CALIDATA_ENVELOPE);
  g_autofree gchar *text = NULL; gsize size;
  g_assert_true (g_file_get_contents (routes, &text, &size, &error)); g_assert_no_error (error);
  g_auto(GStrv) lines = g_strsplit (text, "\n", -1);
  frames = g_ptr_array_new ();
  for (guint i = 0; lines[i] && *lines[i]; i++) {
    g_auto(GStrv) fields = g_strsplit (lines[i], "\t", 8); g_assert_cmpuint (g_strv_length (fields), ==, 8);
    Frame *frame = g_new0 (Frame, 1); frame->role = g_strdup (fields[0]); frame->id = g_strdup (fields[1]); frame->path = g_strdup (fields[2]);
    frame->quality = (gint) g_ascii_strtoll (fields[3], NULL, 10);
    frame->coverage = (gint) g_ascii_strtoll (fields[4], NULL, 10);
    frame->feature_count = (gint) g_ascii_strtoll (fields[5], NULL, 10);
    frame->tag = (gint) g_ascii_strtoll (fields[6], NULL, 10);
    frame->image_sha256 = g_strdup (fields[7]);
    g_ptr_array_add (frames, frame);
  }
  g_assert_cmpuint (frames->len, ==, 35);
  g_test_add_func ("/milan/01-enroll-reload", test_enrollment_and_reload);
  g_test_add_func ("/milan/02-corpus-actions", test_real_corpus_actions);
  g_test_add_func ("/milan/03-template-calibration-rejections", test_invalid_templates_and_calibration);
  g_test_add_func ("/milan/04-cancel-hold-cleanup", test_cancel_and_hold);
  g_test_add_func ("/milan/05-synthetic-contracts", test_synthetic_contracts);
  g_test_add_func ("/milan/06-preprocess-all35", test_all_preprocessing);
  g_test_add_func ("/milan/07-ordinary-retry-pairing-refusal", test_ordinary_retry_actions);
  g_test_add_func ("/milan/08-cancel-during-match", test_cancel_during_match);
  int result = g_test_run ();
  g_clear_object (&enrolled);
  for (guint i = 0; i < frames->len; i++) { Frame *frame = g_ptr_array_index (frames, i); g_free (frame->role); g_free (frame->id); g_free (frame->path); g_free (frame->image_sha256); g_free (frame); }
  g_ptr_array_unref (frames);
  memset (calibration, 0, calibration_size); g_free (calibration);
  return result;
}
