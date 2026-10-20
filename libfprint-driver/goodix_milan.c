/* SPDX-License-Identifier: LGPL-2.1-or-later
 * 5125 adapter for the pinned FreshHomie C1 Type17 native API.
 * Enrollment admission/rollback and RAW FpPrint identity follow C1 bridge.py,
 * worker-api.c and goodix_worker_test.c; USB and worker transports are not used.
 */
#include "goodix_milan.h"
#include "521d-calidata-image.h"
#include "521d-enrollment-session.h"
#include "fpi-image.h"
#include <openssl/crypto.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define MAX_TEMPLATE (384u * 1024u)
#define PROBE_ID "goodix-27c6-5125-milan-type17-probe-v1"
void *gx_worker_load (const uint8_t *, size_t);
void gx_worker_delete (void *);
size_t gx_worker_export (void *, uint8_t *, size_t);
int gx_worker_identify (void *, const uint8_t *, int, int, int, int32_t[4]);

struct _GoodixMilan {
  guint8 calibration[GX521D_CALIDATA_BYTES];
  Gx521dEnrollmentSession *enrollment;
};

static gboolean invalid (GError **error, const gchar *message)
{
  g_set_error_literal (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID, message);
  return FALSE;
}

GoodixMilan *
goodix_milan_new (const guint8 *payload, gsize size, GError **error)
{
  Gx521dCalidataImage *context = NULL;
  GoodixMilan *milan;
  if (!payload || size != GX521D_CALIDATA_BYTES ||
      gx521d_calidata_image_new (payload, size, &context) != 0 || !context) {
    invalid (error, "Milan calibration invalid or missing");
    return NULL;
  }
  gx521d_calidata_image_free (context);
  milan = g_new0 (GoodixMilan, 1);
  memcpy (milan->calibration, payload, size);
  return milan;
}

GoodixMilan *
goodix_milan_load (GError **error)
{
  int directory = -1, fd = -1;
  struct stat info, before;
  guint8 bytes[GOODIX_MILAN_CALIDATA_ENVELOPE];
  gsize offset = 0;
  GoodixMilan *result = NULL;
  /* Walk every component; a trusted leaf must not hide a symlinked ancestor. */
  directory = open ("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  const gchar *components[] = {"var", "lib", "fprint", "goodix-5125-state-v2"};
  for (guint i = 0; directory >= 0 && i < G_N_ELEMENTS (components); i++) {
    if (fstat (directory, &info) || !S_ISDIR (info.st_mode) || info.st_uid != 0 ||
        info.st_gid != 0 || (info.st_mode & 0022)) goto failed;
    int next = openat (directory, components[i], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    close (directory); directory = next;
  }
  if (directory < 0 || fstat (directory, &info) || !S_ISDIR (info.st_mode) ||
      info.st_uid != 0 || info.st_gid != 0 || (info.st_mode & 07777) != 0700)
    goto failed;
  fd = openat (directory, "milan-calidata.dat", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0 || fstat (fd, &info) || !S_ISREG (info.st_mode) ||
      info.st_uid != 0 || info.st_gid != 0 || (info.st_mode & 07777) != 0600 ||
      info.st_nlink != 1 || info.st_size != GOODIX_MILAN_CALIDATA_ENVELOPE)
    goto failed;
  before = info;
  while (offset < sizeof bytes) {
    ssize_t n = read (fd, bytes + offset, sizeof bytes - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) goto failed;
    offset += (gsize) n;
  }
  { guint8 extra; ssize_t n;
    do { n = read (fd, &extra, 1); } while (n < 0 && errno == EINTR);
    if (n != 0) goto failed;
  }
  if (fstat (fd, &info) || before.st_dev != info.st_dev || before.st_ino != info.st_ino ||
      before.st_mode != info.st_mode || before.st_uid != info.st_uid || before.st_gid != info.st_gid ||
      before.st_nlink != info.st_nlink || before.st_size != info.st_size ||
      before.st_mtim.tv_sec != info.st_mtim.tv_sec || before.st_mtim.tv_nsec != info.st_mtim.tv_nsec ||
      before.st_ctim.tv_sec != info.st_ctim.tv_sec || before.st_ctim.tv_nsec != info.st_ctim.tv_nsec)
    goto failed;
  result = goodix_milan_new (bytes + 16, GX521D_CALIDATA_BYTES, error);
  goto out;
failed:
  invalid (error, "Milan requires a trusted private calibration file");
out:
  OPENSSL_cleanse (bytes, sizeof bytes);
  if (fd >= 0) close (fd);
  if (directory >= 0) close (directory);
  return result;
}

void goodix_milan_enroll_clear (GoodixMilan *milan)
{
  if (milan) { gx521d_enrollment_finish (milan->enrollment); milan->enrollment = NULL; }
}
void goodix_milan_free (GoodixMilan *milan)
{
  if (!milan) return;
  goodix_milan_enroll_clear (milan);
  OPENSSL_cleanse (milan, sizeof *milan);
  g_free (milan);
}
gboolean goodix_milan_enroll_start (GoodixMilan *milan, GError **error)
{
  if (!milan) return invalid (error, "Milan calibration missing");
  goodix_milan_enroll_clear (milan);
  milan->enrollment = gx521d_enrollment_start (GOODIX_MILAN_ENROLL_STAGES);
  if (!milan->enrollment) return invalid (error, "Milan enrollment allocation failed");
  return TRUE;
}
guint goodix_milan_enroll_progress (GoodixMilan *milan)
{
  return milan && milan->enrollment ? milan->enrollment->accepted : 0;
}

gboolean goodix_5125_milan_preprocess (GoodixMilan *milan, const uint16_t *raw,
                                  gsize count, GoodixMilanSample *sample, GError **error)
{
  Gx521dCalidataImage *context = NULL;
  Gx521dDefaultResult out;
  Gx521dFeatureObject feature;
  gboolean ok = FALSE;
  if (!milan || !raw || !sample || count != GOODIX_MILAN_PIXELS)
    return invalid (error, "Milan sample geometry invalid");
  for (gsize i = 0; i < count; i++)
    if (raw[i] > 4095) return invalid (error, "Milan sample range invalid");
  memset (sample, 0, sizeof *sample);
  if (gx521d_calidata_image_new (milan->calibration, sizeof milan->calibration, &context) != 0 || !context)
    goto out;
  /* Exact M3 policy: fresh import and mode0 reset for every contact. */
  gx521d_calidata_image_reset_entry_selection (context);
  int code = gx521d_calidata_image_step (context, raw, &out, NULL);
  /* C1 prepare-input raw rejection and its two ordinary capture rejections.
   * SETUP/background and every unknown code remain fatal in this restore path. */
  if (code == 0x29aa || (!code && (out.status == 0xc351 || out.status == 0x7531)))
    {
      g_set_error_literal (error, FP_DEVICE_RETRY, FP_DEVICE_RETRY_GENERAL, "Milan capture quality rejected");
      goto out;
    }
  if (code || out.status != 0 || out.quality < 0 || out.quality > 100 || out.coverage < 0 || out.coverage > 100 ||
      gx521d_feature_extract (out.image, out.quality, out.coverage, 0, &feature) != 0)
    goto out;
  memcpy (&sample->feature_count, feature.scalar_header + 0xf0, 4);
  memcpy (&sample->tag, feature.scalar_header + 0x150, 4);
  if (sample->feature_count < 0 || sample->feature_count > 120) goto out;
  sample->quality = out.quality; sample->coverage = out.coverage;
  memcpy (sample->pixels, out.image, sizeof sample->pixels);
  ok = TRUE;
out:
  gx521d_calidata_image_free (context);
  OPENSSL_cleanse (&out, sizeof out);
  OPENSSL_cleanse (&feature, sizeof feature);
  if (!ok) { OPENSSL_cleanse (sample, sizeof *sample); if (!error || !*error) invalid (error, "Milan preprocessing failed"); }
  return ok;
}

/* C1 load/export is the parser. Require canonical stable reload at this boundary. */
static GBytes *canonical_template (const guint8 *bytes, gsize size, GError **error)
{
  void *owner = NULL;
  guint8 *output = NULL;
  gsize length;
  guint32 declared = 0;
  if (bytes && size >= 10) memcpy (&declared, bytes + 6, 4);
  GBytes *result = NULL;
  if (!bytes || size < 15 || size > MAX_TEMPLATE || !memcmp (bytes, "GSF1", 4) ||
      bytes[0] != 0x87 || bytes[5] != 0x86 || bytes[10] != 0x81 ||
      (gsize) GUINT32_FROM_LE (declared) != size - 10)
    goto out;
  owner = gx_worker_load (bytes, size);
  if (!owner) goto out;
  output = g_malloc (MAX_TEMPLATE);
  length = gx_worker_export (owner, output, MAX_TEMPLATE);
  if (length < 15 || length > MAX_TEMPLATE) goto out;
  gx_worker_delete (owner);
  owner = gx_worker_load (output, length);
  if (!owner) goto out;
  { guint8 *again = g_malloc (MAX_TEMPLATE);
    gsize again_size = gx_worker_export (owner, again, MAX_TEMPLATE);
    gboolean stable = again_size == length && !memcmp (again, output, length);
    OPENSSL_cleanse (again, MAX_TEMPLATE); g_free (again);
    if (!stable) goto out;
  }
  result = g_bytes_new (output, length);
out:
  gx_worker_delete (owner);
  if (output) { OPENSSL_cleanse (output, MAX_TEMPLATE); g_free (output); }
  if (!result) invalid (error, "Milan template invalid");
  return result;
}

GoodixMilanEnrollmentResult
goodix_milan_enroll (GoodixMilan *milan, const GoodixMilanSample *sample,
                     GBytes **template_bytes, GError **error)
{
  Gx521dEnrollmentSession *s = milan ? milan->enrollment : NULL;
  uint32_t status = 0;
  int32_t cq[2];
  guint accepted;
  if (!s || !sample || !template_bytes || *template_bytes || s->accepted >= GOODIX_MILAN_ENROLL_STAGES ||
      sample->quality < 0 || sample->quality > 100 || sample->coverage < 0 || sample->coverage > 100 ||
      sample->feature_count < 0 || sample->feature_count > 120) {
    invalid (error, "Milan enrollment state invalid"); return GOODIX_MILAN_ERROR;
  }
  if (sample->quality < 25 || sample->coverage < 65 || sample->tag != 0 || sample->feature_count == 0)
    return GOODIX_MILAN_RETRY;
  accepted = s->accepted;
  if (gx521d_enrollment_add (s, sample->pixels, sample->quality, sample->coverage, &status, cq) != 0 ||
      (status != 0 && status != 0x80000006u)) goto failed;
  /* Direct C1 worker/bridge overlap rollback, retaining native session state. */
  if (!status && s->overlap > 85 && (s->accepted < 6 || s->previous_overlap > 75)) {
    if (gx521d_enrollment_delete_image (s) != 0 || s->accepted != accepted) goto failed;
    return GOODIX_MILAN_RETRY;
  }
  if (status) {
    if (s->accepted != accepted) goto failed;
    return GOODIX_MILAN_RETRY;
  }
  if (s->accepted != accepted + 1) goto failed;
  if (s->accepted < GOODIX_MILAN_ENROLL_STAGES) return GOODIX_MILAN_ACCEPTED;
  { guint8 *packed = g_malloc (MAX_TEMPLATE);
    gsize size = gx521d_template_export (s->template, packed, MAX_TEMPLATE);
    *template_bytes = canonical_template (packed, size, error);
    OPENSSL_cleanse (packed, MAX_TEMPLATE); g_free (packed);
    return *template_bytes ? GOODIX_MILAN_COMPLETE : GOODIX_MILAN_ERROR;
  }
failed:
  invalid (error, "Milan enrollment failed closed"); return GOODIX_MILAN_ERROR;
}

FpImage *goodix_milan_sample_image (const GoodixMilanSample *sample)
{
  FpImage *image = fp_image_new (80, 64);
  memcpy (image->data, sample->pixels, GOODIX_MILAN_PIXELS);
  return image;
}
static FpPrint *raw_print (FpDevice *device, GVariant *data)
{
  FpPrint *print = fp_print_new (device);
  fpi_print_set_type (print, FPI_PRINT_RAW);
  fpi_print_set_device_stored (print, FALSE);
  g_object_set (print, "fpi-data", data, NULL);
  return print;
}
FpPrint *goodix_milan_probe_print (FpDevice *device, const GoodixMilanSample *sample)
{
  return raw_print (device, g_variant_new ("(sii@ay)", PROBE_ID, sample->quality, sample->coverage,
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, sample->pixels, GOODIX_MILAN_PIXELS, 1)));
}
FpPrint *goodix_milan_template_print (FpDevice *device, GBytes *bytes)
{
  gsize size; const guint8 *data = g_bytes_get_data (bytes, &size);
  return raw_print (device, g_variant_new ("(su@ay)", GOODIX_MILAN_TEMPLATE_ID, 1u,
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, size, 1)));
}
static GBytes *template_from_print (FpPrint *print, GError **error)
{
  g_autoptr(GVariant) data = NULL;
  g_autoptr(GVariant) packet = NULL;
  const gchar *identity; guint version; gsize size;
  if (!print || fpi_print_get_type (print) != FPI_PRINT_RAW) goto failed;
  g_object_get (print, "fpi-data", &data, NULL);
  if (!data || !g_variant_is_of_type (data, G_VARIANT_TYPE ("(suay)"))) goto failed;
  g_variant_get (data, "(&su@ay)", &identity, &version, &packet);
  if (strcmp (identity, GOODIX_MILAN_TEMPLATE_ID) || version != 1u) goto failed;
  { const guint8 *bytes = g_variant_get_fixed_array (packet, &size, 1);
    GBytes *canonical = canonical_template (bytes, size, error);
    gsize canonical_size = 0;
    const guint8 *canonical_data = canonical ? g_bytes_get_data (canonical, &canonical_size) : NULL;
    if (canonical && (canonical_size != size || memcmp (canonical_data, bytes, size))) {
      g_bytes_unref (canonical); goto failed;
    }
    return canonical;
  }
failed:
  invalid (error, "Print is not a canonical Milan Type17 template"); return NULL;
}
gboolean goodix_milan_validate_print (FpPrint *print, GError **error)
{
  GBytes *bytes = template_from_print (print, error);
  if (!bytes) return FALSE;
  g_bytes_unref (bytes); return TRUE;
}
/* Literal outputs of pinned recognition.c/recognition-rescue.c, not a
 * threshold change. Unexpected ABI outputs must never become a positive match. */
static gboolean supported_decision (gint decision)
{
  switch (decision) {
    case 0: case 101: case 104: case 111: case 501: case 701: case 702:
    case -100: case -101: case -102: case -104: case -701: case -702:
      return TRUE;
    default: return FALSE;
  }
}

FpiMatchResult goodix_milan_match (FpImageDevice *device, FpPrint *template_print,
                                  FpPrint *probe, GError **error)
{
  g_autoptr(GBytes) packed = template_from_print (template_print, error);
  g_autoptr(GVariant) data = NULL;
  g_autoptr(GVariant) image = NULL;
  const gchar *identity; gint q, cov; gsize size, length;
  void *owner = NULL;
  FpiMatchResult result = FPI_MATCH_ERROR;
  int32_t decision[4];
  (void) device;
  if (!packed) return result;
  if (!probe || fpi_print_get_type (probe) != FPI_PRINT_RAW) goto failed;
  g_object_get (probe, "fpi-data", &data, NULL);
  if (!data || !g_variant_is_of_type (data, G_VARIANT_TYPE ("(siiay)"))) goto failed;
  g_variant_get (data, "(&sii@ay)", &identity, &q, &cov, &image);
  const guint8 *pixels = g_variant_get_fixed_array (image, &size, 1);
  if (strcmp (identity, PROBE_ID) || q < 0 || q > 100 || cov < 0 || cov > 100 || size != GOODIX_MILAN_PIXELS)
    goto failed;
  const guint8 *bytes = g_bytes_get_data (packed, &length);
  owner = gx_worker_load (bytes, length);
  /* Every template comparison gets a new owner/cache and flags=0. Never learn. */
  if (!owner || gx_worker_identify (owner, pixels, q, cov, 0, decision) != 0 ||
      decision[2] != 0 || (decision[0] != 0 && decision[0] != 131)) goto failed;
  if ((!decision[0] && (decision[3] != -1 || !supported_decision (decision[1]))) ||
      (decision[0] == 131 && (decision[1] != 0 || (decision[3] != -1 && decision[3] != -2))))
    goto failed;
  if (decision[0] == 131) {
    g_set_error_literal (error, FP_DEVICE_RETRY, FP_DEVICE_RETRY_GENERAL, "Milan probe unsupported or empty");
  } else result = decision[1] > 0 ? FPI_MATCH_SUCCESS : FPI_MATCH_FAIL;
  gx_worker_delete (owner);
  return result;
failed:
  gx_worker_delete (owner); invalid (error, "Milan comparison failed closed"); return FPI_MATCH_ERROR;
}
