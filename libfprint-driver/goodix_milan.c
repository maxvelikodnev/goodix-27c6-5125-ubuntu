/* SPDX-License-Identifier: LGPL-2.1-or-later
 * 5125 adapter for the pinned FreshHomie C1 Type17 native API.
 * Enrollment admission/rollback and RAW FpPrint identity follow C1 bridge.py,
 * worker-api.c and goodix_worker_test.c; USB and worker transports are not used.
 */
#include "goodix_milan.h"
#include "goodix_config90.h"
#include "521d-calidata-image.h"
#include "521d-enrollment-session.h"
#include "fpi-image.h"
#include <openssl/crypto.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

#define MAX_TEMPLATE (384u * 1024u)
#define PROBE_ID "goodix-27c6-5125-milan-type17-probe-v1"
void *gx_worker_load (const uint8_t *, size_t);
void gx_worker_delete (void *);
size_t gx_worker_export (void *, uint8_t *, size_t);
int gx_worker_identify (void *, const uint8_t *, int, int, int, int32_t[4]);

struct _GoodixMilan {
  guint8 calibration[GX521D_CALIDATA_BYTES];
  Gx521dEnrollmentSession *enrollment;
  gboolean diagnostic, experimental, capture_started, capture_recorded, otp_bound;
  guint8 envelope_prefix[16];
  int directory_fd;
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
  milan->directory_fd = -1;
  memcpy (milan->calibration, payload, size);
  return milan;
}

static gboolean
same_snapshot (const struct stat *before, const struct stat *after)
{
  return before->st_dev == after->st_dev && before->st_ino == after->st_ino &&
    before->st_uid == after->st_uid && before->st_mode == after->st_mode &&
    before->st_nlink == after->st_nlink && before->st_size == after->st_size &&
    before->st_mtim.tv_sec == after->st_mtim.tv_sec && before->st_mtim.tv_nsec == after->st_mtim.tv_nsec &&
    before->st_ctim.tv_sec == after->st_ctim.tv_sec && before->st_ctim.tv_nsec == after->st_ctim.tv_nsec;
}

static int
open_private_directory (const gchar *directory, uid_t uid, GError **error)
{
  int fd = -1;
  struct stat st;
  g_auto(GStrv) components = NULL;
  if (!directory || !g_path_is_absolute (directory) || !strstr (directory, "/development/"))
    goto fail;
  components = g_strsplit (directory + 1, "/", -1);
  fd = open ("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) goto fail;
  /* Resolve each component with NOFOLLOW; only the final directory is a
   * trusted writer boundary. Home/repository ancestors may belong to the user;
   * the retained directory fd prevents later path swaps redirecting our I/O. */
  for (guint i = 0; components[i]; i++)
    {
      int next;
      if (!*components[i] || !strcmp (components[i], ".") || !strcmp (components[i], "..")) goto fail;
      next = openat (fd, components[i], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      close (fd);
      fd = next;
      if (fd < 0) goto fail;
    }
  if (fstat (fd, &st) || !S_ISDIR (st.st_mode) || st.st_uid != uid || (st.st_mode & 07777) != 0700)
    goto fail;
  return fd;
fail:
  if (fd >= 0) close (fd);
  invalid (error, "Milan diagnostic directory must be private, trusted and symlink-free inside development/");
  return -1;
}

static guint8 *
read_private_file (int directory_fd, const gchar *name, uid_t uid,
                   gsize minimum, gsize maximum, gsize *size, GError **error)
{
  struct stat before, after, named;
  int fd = openat (directory_fd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  guint8 *bytes = NULL;
  gsize offset = 0;
  if (fd < 0 || fstat (fd, &before) || !S_ISREG (before.st_mode) ||
      before.st_uid != uid || (before.st_mode & 07777) != 0600 || before.st_nlink != 1 ||
      before.st_size < 0 || (guint64) before.st_size < minimum || (guint64) before.st_size > maximum)
    goto fail;
  bytes = g_malloc0 ((gsize) before.st_size + 1);
  while (offset < (gsize) before.st_size)
    {
      ssize_t count = read (fd, bytes + offset, (gsize) before.st_size - offset);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) goto fail;
      offset += (gsize) count;
    }
  if (fstat (fd, &after) || fstatat (directory_fd, name, &named, AT_SYMLINK_NOFOLLOW) ||
      !same_snapshot (&before, &after) || !same_snapshot (&after, &named)) goto fail;
  close (fd);
  *size = offset;
  return bytes;
fail:
  if (fd >= 0) close (fd);
  if (bytes) { OPENSSL_cleanse (bytes, offset); g_free (bytes); }
  invalid (error, "Milan diagnostic file missing, untrusted, malformed or changed during read");
  return NULL;
}

static gboolean
receipt_value (GKeyFile *receipt, const gchar *key, const gchar *expected)
{
  g_autofree gchar *value = g_key_file_get_string (receipt, "M5B", key, NULL);
  return g_strcmp0 (value, expected) == 0;
}

static GoodixMilan *
load_experiment (const gchar *directory, uid_t uid, gboolean experimental, GError **error)
{
  GoodixMilan *milan = NULL;
  g_autoptr(GKeyFile) receipt = g_key_file_new ();
  g_autofree guint8 *metadata = NULL;
  guint8 *envelope = NULL;
  g_autofree gchar *envelope_hash = NULL, *payload_hash = NULL;
  gsize metadata_size = 0, size = 0;
  int fd = open_private_directory (directory, uid, error);
  if (fd < 0) return NULL;
  if (flock (fd, LOCK_EX | LOCK_NB))
    { invalid (error, "Milan private session is already in use"); close (fd); return NULL; }
  metadata = read_private_file (fd, "receipt.ini", uid, 1, 2048, &metadata_size, error);
  if (!metadata) goto out;
  if (memchr (metadata, 0, metadata_size) ||
      !g_key_file_load_from_data (receipt, (const gchar *) metadata, metadata_size, G_KEY_FILE_NONE, NULL) ||
      !receipt_value (receipt, "schema", "1") ||
      !receipt_value (receipt, "purpose", experimental ? "M5.C_PRIVATE_ENROLL_VERIFY" : "M5.B_CAPTURE_ONLY") ||
      !receipt_value (receipt, "source", "M3_USER_DECLARED_WINDOWS_COPY") ||
      !receipt_value (receipt, "device_binding", "UNKNOWN") ||
      !receipt_value (receipt, "prefix_semantics", "UNKNOWN"))
    { invalid (error, "Milan diagnostic provenance receipt invalid"); goto out; }
  envelope = read_private_file (fd, "calidata.dat", uid, GOODIX_MILAN_CALIDATA_ENVELOPE,
                                GOODIX_MILAN_CALIDATA_ENVELOPE, &size, error);
  if (!envelope) goto out;
  envelope_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA256, envelope, size);
  payload_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA256, envelope + 16, size - 16);
  if (!receipt_value (receipt, "envelope_sha256", envelope_hash) ||
      !receipt_value (receipt, "payload_sha256", payload_hash))
    { invalid (error, "Milan diagnostic calibration integrity or M3 provenance mismatch"); goto out; }
  /* Fixed envelope size and hash are checked before entering the pinned native
   * parser. Its import validates version, both map CRCs, sample_count 0..400,
   * optional classifier magic/version and classifier history 0..50. The prefix
   * is preserved byte-for-byte and deliberately never interpreted as an ID. */
  milan = goodix_milan_new (envelope + 16, size - 16, error);
  if (!milan) goto out;
  milan->diagnostic = !experimental;
  milan->experimental = experimental;
  memcpy (milan->envelope_prefix, envelope, sizeof milan->envelope_prefix);
  milan->directory_fd = fd;
  fd = -1;
out:
  if (envelope) { OPENSSL_cleanse (envelope, size); g_free (envelope); }
  if (fd >= 0) close (fd);
  return milan;
}

GoodixMilan *
goodix_milan_load (const gchar *directory, GError **error)
{
  if (!directory)
    {
      g_set_error_literal (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_NOT_SUPPORTED,
                           "Milan normal authentication requires qualified calibration source and binding; use explicit M5.B diagnostics first");
      return NULL;
    }
  if (geteuid () != 0)
    { invalid (error, "Milan diagnostic capture requires explicit root authorization"); return NULL; }
  return load_experiment (directory, 0, FALSE, error);
}

GoodixMilan *
goodix_milan_load_experimental (const gchar *directory, GError **error)
{
  if (geteuid () != 0 || !directory)
    { invalid (error, "M5.C requires explicit root authorization and private session"); return NULL; }
  return load_experiment (directory, 0, TRUE, error);
}

#ifdef GOODIX_ENABLE_TEST_SEAMS
GoodixMilan *goodix_milan_test_load_experimental (const gchar *directory, GError **error)
{ return load_experiment (directory, geteuid (), TRUE, error); }
GoodixMilan *goodix_milan_test_load (const gchar *directory, GError **error)
{ return load_experiment (directory, geteuid (), FALSE, error); }
#endif

gboolean goodix_milan_is_diagnostic (GoodixMilan *milan)
{ return milan && milan->diagnostic; }

gboolean goodix_milan_is_experimental (GoodixMilan *milan)
{ return milan && milan->experimental; }

gboolean goodix_milan_uses_directory (GoodixMilan *milan, int directory_fd)
{
  struct stat held, caller;
  return milan && milan->directory_fd >= 0 && directory_fd >= 0 &&
    !fstat (milan->directory_fd, &held) && !fstat (directory_fd, &caller) &&
    held.st_dev == caller.st_dev && held.st_ino == caller.st_ino;
}

void goodix_milan_clear_binding (GoodixMilan *milan)
{ if (milan) milan->otp_bound = FALSE; }

gboolean
goodix_milan_bind_to_otp (GoodixMilan *milan, const guint8 *otp, gsize size)
{
  goodix_milan_clear_binding (milan);
  /* Only the full, already validated preflight evidence can bind a loaded
   * envelope; a payload-only synthetic object has no device identity. */
  if (!milan || milan->directory_fd < 0 || !otp || size != GOODIX_CONFIG90_OTP_LENGTH)
    return FALSE;
  milan->otp_bound = CRYPTO_memcmp (milan->envelope_prefix, otp, sizeof milan->envelope_prefix) == 0;
  return milan->otp_bound;
}

const gchar *goodix_milan_device_binding (const GoodixMilan *milan)
{ return milan && milan->otp_bound ? "OTP_A6_PREFIX_MATCH" : "UNKNOWN"; }

static gboolean write_private_result (int directory_fd, const gchar *name,
                                       const guint8 *bytes, gsize size);

gboolean goodix_milan_begin_capture (GoodixMilan *milan, GError **error)
{
  static const guint8 marker[] = "M5.B capture budget consumed; no automatic retry\n";
  if (!goodix_milan_is_diagnostic (milan) || milan->capture_started)
    return invalid (error, "Milan diagnostic session permits exactly one explicit capture");
  /* Exclusive persistent session marker precedes all USB/resources. It also
   * fences a second client/process or a cancelled attempt after reopening. */
  if (!write_private_result (milan->directory_fd, "capture.started", marker, sizeof marker - 1) ||
      fsync (milan->directory_fd))
    return invalid (error, "Milan diagnostic capture budget already consumed or marker not durable");
  goodix_milan_clear_binding (milan);
  milan->capture_started = TRUE;
  return TRUE;
}

static gboolean
write_private_result (int directory_fd, const gchar *name, const guint8 *bytes, gsize size)
{
  int fd = openat (directory_fd, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  gsize offset = 0;
  gboolean ok;
  if (fd < 0) return FALSE;
  while (offset < size)
    {
      ssize_t count = write (fd, bytes + offset, size - offset);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) { close (fd); return FALSE; }
      offset += (gsize) count;
    }
  ok = fsync (fd) == 0;
  if (close (fd)) ok = FALSE;
  return ok;
}

/* Durable per-session budget, shared by verify and identify across processes.
 * A failed/cancelled action consumes its reservation. No automatic retries. */
gboolean
goodix_milan_begin_experimental_action (GoodixMilan *milan, FpiDeviceAction action, GError **error)
{
  static const guint8 marker[] = "M5.C explicit action consumed; never retry automatically\n";
  struct stat st;
  if (!goodix_milan_is_experimental (milan)) return invalid (error, "M5.C session required");
  goodix_milan_clear_binding (milan);
  if (fstatat (milan->directory_fd, "match.stop", &st, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT)
    return invalid (error, "M5.C stopped after MATCH or ambiguous session state");
  if (action == FPI_DEVICE_ACTION_ENROLL)
    {
      if (write_private_result (milan->directory_fd, "enroll.started", marker, sizeof marker - 1) &&
          fsync (milan->directory_fd) == 0) return TRUE;
    }
  else if (action == FPI_DEVICE_ACTION_VERIFY || action == FPI_DEVICE_ACTION_IDENTIFY)
    {
      for (guint attempt = 1; attempt <= 3; attempt++)
        {
          g_autofree gchar *name = g_strdup_printf ("probe-%u.started", attempt);
          if (fstatat (milan->directory_fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0) continue;
          if (errno != ENOENT) break;
          /* Do not retry even an exclusive-create race/partial write. */
          if (write_private_result (milan->directory_fd, name, marker, sizeof marker - 1) &&
              fsync (milan->directory_fd) == 0) return TRUE;
          break;
        }
    }
  return invalid (error, "M5.C action unsupported, budget exhausted or journal failed");
}

gboolean
goodix_milan_record_match (GoodixMilan *milan, GError **error)
{
  static const guint8 marker[] = "MATCH: M5.C session stopped\n";
  if (!goodix_milan_is_experimental (milan) || !milan->otp_bound ||
      !write_private_result (milan->directory_fd, "match.stop", marker, sizeof marker - 1) ||
      fsync (milan->directory_fd)) return invalid (error, "M5.C MATCH stop could not be recorded");
  return TRUE;
}

gboolean
goodix_milan_record_capture (GoodixMilan *milan, const uint16_t *raw,
                             const GoodixMilanSample *sample,
                             const GError *capture_error, GError **error)
{
  guint8 raw_le[GOODIX_MILAN_PIXELS * 2];
  g_autofree gchar *raw_hash = NULL, *image_hash = NULL, *metrics = NULL;
  gboolean ok = FALSE;
  if (!goodix_milan_is_diagnostic (milan) || !milan->capture_started || milan->capture_recorded || !raw)
    return invalid (error, "Milan diagnostic result state invalid");
  milan->capture_recorded = TRUE;
  for (guint i = 0; i < GOODIX_MILAN_PIXELS; i++)
    { raw_le[2 * i] = (guint8) (raw[i] & 0xff); raw_le[2 * i + 1] = (guint8) (raw[i] >> 8); }
  raw_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA256, raw_le, sizeof raw_le);
  image_hash = sample ? g_compute_checksum_for_data (G_CHECKSUM_SHA256, sample->pixels, sizeof sample->pixels) : g_strdup ("unavailable");
  metrics = g_strdup_printf ("[M5B]\ndevice_binding=%s\nprefix_semantics=UNKNOWN\nauthentication_qualified=false\nraw_sha256=%s\nimage_sha256=%s\nfeature_sha256=%s\npreprocess_sha256=%s\npreprocess=%s\nquality=%d\ncoverage=%d\nfeature_count=%d\ntag=%d\nerror_domain=%s\nerror_code=%d\n",
    goodix_milan_device_binding (milan), raw_hash, image_hash, sample ? sample->feature_sha256 : "unavailable", sample ? sample->preprocess_sha256 : "unavailable",
    sample ? "PASS" : "REJECTED", sample ? sample->quality : -1,
    sample ? sample->coverage : -1, sample ? sample->feature_count : -1, sample ? sample->tag : -1,
    capture_error ? g_quark_to_string (capture_error->domain) : "none", capture_error ? capture_error->code : 0);
  ok = write_private_result (milan->directory_fd, "capture.raw-u16le", raw_le, sizeof raw_le) &&
    (!sample || write_private_result (milan->directory_fd, "capture.preprocessed-u8", sample->pixels, sizeof sample->pixels)) &&
    write_private_result (milan->directory_fd, "metrics.ini", (const guint8 *) metrics, strlen (metrics)) &&
    fsync (milan->directory_fd) == 0;
  OPENSSL_cleanse (raw_le, sizeof raw_le);
  if (!ok) return invalid (error, "Milan diagnostic private result write failed; stop without retry");
  return TRUE;
}

void goodix_milan_enroll_clear (GoodixMilan *milan)
{
  if (milan) { gx521d_enrollment_finish (milan->enrollment); milan->enrollment = NULL; }
}
void goodix_milan_free (GoodixMilan *milan)
{
  if (!milan) return;
  goodix_milan_enroll_clear (milan);
  if (milan->directory_fd >= 0) close (milan->directory_fd);
  OPENSSL_cleanse (milan, sizeof *milan);
  g_free (milan);
}
gboolean goodix_milan_enroll_start (GoodixMilan *milan, GError **error)
{
  if (!milan || milan->diagnostic) return invalid (error, "Milan enrollment requires qualified calibration binding");
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
  Gx521dDefaultResult out = { 0 };
  Gx521dFeatureObject feature = { 0 };
  gboolean ok = FALSE;
  if (!milan || !raw || !sample || count != GOODIX_MILAN_PIXELS)
    return invalid (error, "Milan sample geometry invalid");
  if (milan->directory_fd >= 0 && !milan->otp_bound)
    return invalid (error, "Milan calibration has no current OTP binding");
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
  {
    g_autofree gchar *feature_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (const guint8 *) &feature, sizeof feature);
    g_autofree gchar *preprocess_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (const guint8 *) &out, sizeof out);
    g_strlcpy (sample->feature_sha256, feature_hash, sizeof sample->feature_sha256);
    g_strlcpy (sample->preprocess_sha256, preprocess_hash, sizeof sample->preprocess_sha256);
  }
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
