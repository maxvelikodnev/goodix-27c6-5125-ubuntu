/* SPDX-License-Identifier: LGPL-2.1-or-later
 * USB 5125 adapter for the pinned, unchanged ChicagoHS computational engine.
 * No hardware I/O, calibration files, templateStudy, or alternate matcher.
 */
#include "goodix_chicago.h"
#include "goodix_config90.h"
#include "fpi-image.h"
#include <openssl/crypto.h>
#include <string.h>

#define MAX_TEMPLATE (1024u * 1024u)
#define PROBE_KEY "goodix-chicago-transient-probe"
struct _GoodixChicago {
  guint8 sensor_id[16];
  guint8 otp_digest[32];
  guint64 generation;
  gboolean bound, background_valid;
  guint16 background[GOODIX_CHICAGO_PIXELS];
  GBytes *calibration;
  GoodixChicagoPreprocessor *preprocessor;
  GoodixChicagoEnrollment *enrollment;
  GoodixChicagoRuntimeProbe *deferred;
  GoodixChicagoEngineEnrollmentPolicy policy;
};
static gboolean invalid (GError **error, const gchar *message)
{
  g_set_error_literal (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID, message);
  return FALSE;
}
void goodix_chicago_bytes_free (GoodixChicagoBytes *bytes)
{
  if (!bytes) return;
  gsize size;
  gpointer data = g_bytes_unref_to_data (bytes, &size);
  OPENSSL_cleanse (data, size); g_free (data);
}
static void clear_bytes (GBytes **bytes)
{
  if (!*bytes) return;
  gsize size;
  gpointer data = g_bytes_unref_to_data (*bytes, &size);
  *bytes = NULL;
  OPENSSL_cleanse (data, size);
  g_free (data);
}
GoodixChicago *goodix_chicago_new (void) { return g_new0 (GoodixChicago, 1); }
void goodix_chicago_clear_binding (GoodixChicago *c)
{
  if (!c) return;
  c->bound = c->background_valid = FALSE;
  c->generation = 0;
  goodix_chicago_preprocessor_free (c->preprocessor);
  c->preprocessor = NULL;
  clear_bytes (&c->calibration);
  OPENSSL_cleanse (c->sensor_id, sizeof c->sensor_id);
  OPENSSL_cleanse (c->otp_digest, sizeof c->otp_digest);
  OPENSSL_cleanse (c->background, sizeof c->background);
}
void goodix_chicago_enroll_clear (GoodixChicago *c)
{
  if (!c) return;
  goodix_chicago_enrollment_free (c->enrollment);
  c->enrollment = NULL;
  goodix_chicago_runtime_probe_free (c->deferred);
  c->deferred = NULL;
  OPENSSL_cleanse (&c->policy, sizeof c->policy);
}
void goodix_chicago_free (GoodixChicago *c)
{
  if (!c) return;
  goodix_chicago_enroll_clear (c);
  goodix_chicago_clear_binding (c);
  OPENSSL_cleanse (c, sizeof *c);
  g_free (c);
}
gboolean goodix_chicago_bind_preflight (GoodixChicago *c, const guint8 *otp,
                                       gsize size, guint64 generation, GError **error)
{
  goodix_chicago_clear_binding (c);
  if (!c || !otp || size != GOODIX_CONFIG90_OTP_LENGTH || !generation)
    return invalid (error, "Chicago preflight evidence missing or malformed");
  /* Upstream sensorID is exactly the opaque first 16 OTP bytes, not a
   * decoded universal serial. Retain that byte-for-byte API contract and
   * additionally bind our envelope to the complete immutable factory record.
   * Caller is the successful CRC-validated A6/CONFIG90 preflight, never a
   * Windows calibration prefix or a synthesized identity. */
  memcpy (c->sensor_id, otp, sizeof c->sensor_id);
  GChecksum *checksum = g_checksum_new (G_CHECKSUM_SHA256);
  gsize digest_size = sizeof c->otp_digest;
  g_checksum_update (checksum, otp, (gssize) size);
  g_checksum_get_digest (checksum, c->otp_digest, &digest_size);
  g_checksum_free (checksum);
  c->bound = TRUE; c->generation = generation;
  return TRUE;
}
#ifdef GOODIX_ENABLE_TEST_SEAMS
gboolean goodix_chicago_test_bind (GoodixChicago *c, const guint8 id[16], guint64 generation)
{
  goodix_chicago_clear_binding (c);
  if (!c || !id || !generation) return FALSE;
  memcpy (c->sensor_id, id, 16);
  c->bound = TRUE; c->generation = generation;
  return TRUE;
}
#endif
gboolean goodix_chicago_native_background (GoodixChicago *c, const guint16 *background,
  gsize count, guint width, guint height, guint64 generation, GError **error)
{
  if (!c || !c->bound || !generation || generation != c->generation || !background ||
      count != GOODIX_CHICAGO_PIXELS || width != 80 || height != 64)
    return invalid (error, "Chicago background provenance, generation or geometry invalid");
  for (gsize i = 0; i < count; i++)
    if (background[i] > 4095) return invalid (error, "Chicago background exceeds 12 bits");
  if (c->background_valid)
    return memcmp (c->background, background, sizeof c->background) == 0 ||
      invalid (error, "Chicago background changed within one generation");
  c->calibration = goodix_chicago_calibration_generate (background);
  if (!c->calibration) return invalid (error, "Chicago calibration generation failed");
  c->preprocessor = goodix_chicago_preprocessor_new (c->calibration, background, error);
  if (!c->preprocessor) { clear_bytes (&c->calibration); return FALSE; }
  memcpy (c->background, background, sizeof c->background);
  c->background_valid = TRUE;
  return TRUE;
}
gboolean goodix_chicago_native_preprocess (GoodixChicago *c, const guint16 *raw,
  gsize count, guint64 generation, GoodixChicagoSample *sample, GError **error)
{
  GoodixChicagoRuntimeReject reject;
  if (sample) memset (sample, 0, sizeof *sample);
  if (!c || !sample || !raw || count != GOODIX_CHICAGO_PIXELS || !c->bound ||
      !c->background_valid || !generation || generation != c->generation)
    return invalid (error, "Chicago capture requires current sensor and no-finger background");
  for (gsize i = 0; i < count; i++)
    if (raw[i] > 4095) return invalid (error, "Chicago frame exceeds 12 bits");
  /* Enrollment retains temporal state. Independent probes get fresh state,
   * matching upstream CH0: gallery iteration never preprocesses twice. */
  GoodixChicagoPreprocessor *p = c->enrollment ? c->preprocessor :
    goodix_chicago_preprocessor_new (c->calibration, c->background, error);
  if (!p) return FALSE;
  sample->probe = goodix_chicago_runtime_prepare_probe (p, raw, &reject, error);
  if (p != c->preprocessor) goodix_chicago_preprocessor_free (p);
  if (!sample->probe)
    {
      if (error && *error) return FALSE;
      if (reject == GOODIX_CHICAGO_RUNTIME_REJECT_BAD_INPUT)
        return invalid (error, "Chicago preprocessing rejected invalid input");
      g_set_error_literal (error, FP_DEVICE_RETRY, FP_DEVICE_RETRY_GENERAL, "Chicago capture quality rejected");
      return FALSE;
    }
  const GoodixChicagoSubtemplateView *v = goodix_chicago_runtime_probe_get_view (sample->probe);
  sample->quality = (gint) v->quality; sample->coverage = (gint) v->coverage;
  /* Display-only original raster. The matcher consumes the native probe,
   * never this 8-bit rendering and never a second preprocessing pass. */
  for (gsize i = 0; i < count; i++) sample->pixels[i] = (guint8) (raw[i] >> 4);
  return TRUE;
}
void goodix_chicago_sample_clear (GoodixChicagoSample *sample)
{
  if (!sample) return;
  goodix_chicago_runtime_probe_free (sample->probe);
  OPENSSL_cleanse (sample, sizeof *sample);
}
gboolean goodix_chicago_enroll_start (GoodixChicago *c, GError **error)
{
  if (!c) return invalid (error, "Chicago context missing");
  goodix_chicago_enroll_clear (c);
  c->enrollment = goodix_chicago_enrollment_new ();
  goodix_chicago_engine_enrollment_policy_init (&c->policy);
  return TRUE;
}
guint goodix_chicago_enroll_progress (GoodixChicago *c) { return c ? c->policy.used : 0; }
static gboolean insert (GoodixChicagoEnrollment *e, GoodixChicagoRuntimeProbe *probe,
                         GoodixChicagoEnrollmentResult *result, GError **error)
{
  const GoodixChicagoSubtemplateView *v = goodix_chicago_runtime_probe_get_view (probe);
  if (!goodix_chicago_enrollment_get_count (e))
    return goodix_chicago_enrollment_insert_first (e, v->records, v->record_count, v->active_count,
      v->quality, v->coverage, v->metric_data, result, error);
  return goodix_chicago_enrollment_insert_next (e, v->records, v->record_count, v->active_count,
    v->quality, v->coverage, v->metric_data, result, error);
}
GoodixChicagoAdmission goodix_chicago_enroll (GoodixChicago *c,
  GoodixChicagoSample *sample, GBytes **bytes, GError **error)
{
  GoodixChicagoEnrollmentResult result;
  guint reject;
  if (!c || !c->bound || !c->enrollment || !sample || !sample->probe || !bytes || *bytes ||
      goodix_chicago_engine_enrollment_policy_complete (&c->policy))
    { invalid (error, "Chicago enrollment state invalid"); return GOODIX_CHICAGO_ERROR; }
  if (!insert (c->enrollment, sample->probe, &result, error)) return GOODIX_CHICAGO_ERROR;
  gboolean accepted = goodix_chicago_engine_enrollment_policy_accept (&c->policy, result.position_x, result.position_y, &reject);
  if (c->policy.defer_current_sample)
    {
      if (c->deferred || !goodix_chicago_enrollment_drop_last (c->enrollment, error))
        { if (!error || !*error) invalid (error, "Chicago deferred enrollment invalid"); return GOODIX_CHICAGO_ERROR; }
      c->deferred = g_steal_pointer (&sample->probe);
    }
  if (c->policy.restore_deferred_sample)
    {
      if (!c->deferred || !insert (c->enrollment, c->deferred, &result, error))
        { if (!error || !*error) invalid (error, "Chicago deferred enrollment missing"); return GOODIX_CHICAGO_ERROR; }
      goodix_chicago_runtime_probe_free (c->deferred); c->deferred = NULL;
    }
  if (goodix_chicago_engine_enrollment_policy_complete (&c->policy))
    {
      if (c->deferred) { invalid (error, "Chicago deferred sample at completion"); return GOODIX_CHICAGO_ERROR; }
      *bytes = goodix_chicago_enrollment_pack (c->enrollment, error);
      return *bytes ? GOODIX_CHICAGO_COMPLETE : GOODIX_CHICAGO_ERROR;
    }
  return accepted ? GOODIX_CHICAGO_ACCEPTED : GOODIX_CHICAGO_RETRY;
}
static FpPrint *raw_print (FpDevice *device, GVariant *data)
{
  FpPrint *print = fp_print_new (device);
  fpi_print_set_type (print, FPI_PRINT_RAW);
  fpi_print_set_device_stored (print, FALSE);
  if (data) g_object_set (print, "fpi-data", data, NULL);
  return print;
}
FpImage *goodix_chicago_sample_image (const GoodixChicagoSample *sample)
{
  FpImage *image = fp_image_new (80, 64);
  memcpy (image->data, sample->pixels, sizeof sample->pixels);
  return image;
}
FpPrint *goodix_chicago_probe_print (FpDevice *device, GoodixChicagoSample *sample)
{
  FpPrint *print = raw_print (device, NULL);
  g_object_set_data_full (G_OBJECT (print), PROBE_KEY, g_steal_pointer (&sample->probe),
                         (GDestroyNotify) goodix_chicago_runtime_probe_free);
  return print;
}
FpPrint *goodix_chicago_template_print (GoodixChicago *c, FpDevice *device, GBytes *bytes)
{
  if (!c || !c->bound || !c->calibration || !bytes) return NULL;
  g_autoptr(GVariant) inner = goodix_chicago_print_data_build (c->sensor_id, c->calibration, bytes);
  return raw_print (device, g_variant_new ("(s@ay@v)", GOODIX_CHICAGO_TEMPLATE_ID,
    g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, c->otp_digest, sizeof c->otp_digest, 1),
    g_variant_new_variant (inner)));
}
static GVariant *print_inner (FpPrint *print, const guint8 *expected_digest, GError **error)
{
  g_autoptr(GVariant) data = NULL, child = NULL, digest = NULL;
  const gchar *identity;
  if (!print || fpi_print_get_type (print) != FPI_PRINT_RAW) goto fail;
  g_object_get (print, "fpi-data", &data, NULL);
  if (!data || g_variant_get_size (data) > MAX_TEMPLATE + 1024 || !g_variant_is_of_type (data, G_VARIANT_TYPE ("(sayv)"))) goto fail;
  g_variant_get (data, "(&s@ay@v)", &identity, &digest, &child);
  gsize size; const guint8 *bytes = g_variant_get_fixed_array (digest, &size, 1);
  if (size != 32 || (expected_digest && CRYPTO_memcmp (bytes, expected_digest, size))) goto fail;
  if (strcmp (identity, GOODIX_CHICAGO_TEMPLATE_ID)) goto fail;
  return g_variant_get_variant (child);
fail:
  invalid (error, "Print is not a USB 5125 ChicagoHS template"); return NULL;
}
static gboolean validate_inner (GVariant *inner, const guint8 *id, GBytes *calibration, GError **error)
{
  g_autoptr(GoodixChicagoBytes) packed = NULL;
  if (!goodix_chicago_print_data_parse (inner, id, calibration, &packed, error)) return FALSE;
  gsize size; const guint8 *data = g_bytes_get_data (packed, &size);
  if (size > MAX_TEMPLATE) return invalid (error, "Chicago template too large");
  GoodixChicagoEnrollment *e = goodix_chicago_enrollment_unpack (data, size, error);
  if (!e) return FALSE;
  gboolean complete = goodix_chicago_enrollment_get_count (e) >= GOODIX_CHICAGO_ENGINE_REQUIRED_SAMPLES;
  goodix_chicago_enrollment_free (e);
  return complete || invalid (error, "Chicago template has fewer than 12 enrollment samples");
}
gboolean goodix_chicago_validate_print (FpPrint *print, GError **error)
{
  g_autoptr(GVariant) inner = print_inner (print, NULL, error);
  return inner && validate_inner (inner, NULL, NULL, error);
}
gboolean goodix_chicago_validate_bound_print (GoodixChicago *c, FpPrint *print, GError **error)
{
  if (!c || !c->bound) return invalid (error, "Chicago current sensor binding required");
  g_autoptr(GVariant) inner = print_inner (print, c->otp_digest, error);
  return inner && validate_inner (inner, c->sensor_id, c->calibration, error);
}
FpiMatchResult goodix_chicago_match (GoodixChicago *c, FpPrint *reference, FpPrint *probe, GError **error)
{
  if (!c || !c->bound || !c->background_valid || !probe)
    { invalid (error, "Chicago current sensor binding required"); return FPI_MATCH_ERROR; }
  GoodixChicagoRuntimeProbe *p = g_object_get_data (G_OBJECT (probe), PROBE_KEY);
  if (!p) { invalid (error, "Chicago probe is not transient runtime data"); return FPI_MATCH_ERROR; }
  g_autoptr(GVariant) inner = print_inner (reference, c->otp_digest, error);
  if (!inner || !validate_inner (inner, c->sensor_id, c->calibration, error)) return FPI_MATCH_ERROR;
  GoodixChicagoMatchTemplateResult result;
  if (!goodix_chicago_runtime_match_print_data (p, inner, c->sensor_id, c->calibration, &result, error))
    return FPI_MATCH_ERROR;
  gboolean matched = result.score > 0;
  OPENSSL_cleanse (&result, sizeof result);
  return matched ? FPI_MATCH_SUCCESS : FPI_MATCH_FAIL;
}
