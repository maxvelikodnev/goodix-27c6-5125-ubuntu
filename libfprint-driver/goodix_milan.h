/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "fpi-image-device.h"
#include <stdint.h>

#define GOODIX_MILAN_ENROLL_STAGES 18u
#define GOODIX_MILAN_PIXELS 5120u
#define GOODIX_MILAN_CALIDATA_ENVELOPE 140480u
#define GOODIX_MILAN_TEMPLATE_ID "goodix-27c6-5125-milan-type17-e68eb725-v1"
#define GOODIX_MILAN_CALIDATA_PATH "/var/lib/fprint/goodix-5125-state-v2/milan-calidata.dat"

typedef struct _GoodixMilan GoodixMilan;
typedef struct {
  guint8 pixels[GOODIX_MILAN_PIXELS];
  gint quality, coverage;
  gint feature_count, tag;
  gchar feature_sha256[65], preprocess_sha256[65];
} GoodixMilanSample;

typedef enum {
  GOODIX_MILAN_ACCEPTED,
  GOODIX_MILAN_RETRY,
  GOODIX_MILAN_COMPLETE,
  GOODIX_MILAN_ERROR,
} GoodixMilanEnrollmentResult;

/* Inert payload only; caller retains its bytes. No file or device access. */
GoodixMilan *goodix_milan_new (const guint8 *payload, gsize size, GError **error);
/* NULL means normal operation: no source/device association is qualified yet.
 * A private directory is accepted only for explicit root-only M5.B capture.
 * This reads the original envelope unchanged; it never provisions the sensor. */
GoodixMilan *goodix_milan_load (const gchar *directory, GError **error);
/* Separate private M5.C opt-in. Ordinary load(NULL) always stays closed. */
GoodixMilan *goodix_milan_load_experimental (const gchar *directory, GError **error);
/* M5.D owns only the current epoch's decoded no-finger raster, never calidata. */
GoodixMilan *goodix_milan_load_native (const gchar *directory, GError **error);
gboolean goodix_milan_is_native (GoodixMilan *milan);
gboolean goodix_milan_native_preflight (GoodixMilan *milan, guint64 generation, GError **error);
gboolean goodix_milan_native_background (GoodixMilan *milan, const uint16_t *background,
                                         gsize count, guint width, guint height,
                                         guint64 generation, GError **error);
gboolean goodix_milan_native_preprocess (GoodixMilan *milan, const uint16_t *raw,
                                         gsize count, guint64 generation,
                                         GoodixMilanSample *sample, GError **error);
gboolean goodix_milan_is_diagnostic (GoodixMilan *milan);
gboolean goodix_milan_is_experimental (GoodixMilan *milan);
gboolean goodix_milan_uses_directory (GoodixMilan *milan, int directory_fd);
gboolean goodix_milan_bind_to_otp (GoodixMilan *milan, const guint8 *otp, gsize size);
void goodix_milan_clear_binding (GoodixMilan *milan);
const gchar *goodix_milan_device_binding (const GoodixMilan *milan);
gboolean goodix_milan_begin_experimental_action (GoodixMilan *milan, FpiDeviceAction action, GError **error);
gboolean goodix_milan_record_match (GoodixMilan *milan, GError **error);
gboolean goodix_milan_begin_capture (GoodixMilan *milan, GError **error);
gboolean goodix_milan_record_capture (GoodixMilan *milan, const uint16_t *raw,
                                     const GoodixMilanSample *sample,
                                     const GError *capture_error, GError **error);
#ifdef GOODIX_ENABLE_TEST_SEAMS
/* Same validator and inode-safe reader, current UID and synthetic hashes only. */
GoodixMilan *goodix_milan_test_load (const gchar *directory, GError **error);
GoodixMilan *goodix_milan_test_load_experimental (const gchar *directory, GError **error);
GoodixMilan *goodix_milan_test_load_native (const gchar *directory, GError **error);
#endif
void goodix_milan_free (GoodixMilan *milan);
gboolean goodix_milan_enroll_start (GoodixMilan *milan, GError **error);
void goodix_milan_enroll_clear (GoodixMilan *milan);
guint goodix_milan_enroll_progress (GoodixMilan *milan);
gboolean goodix_5125_milan_preprocess (GoodixMilan *milan, const uint16_t *raw,
                                  gsize count, GoodixMilanSample *sample, GError **error);
GoodixMilanEnrollmentResult goodix_milan_enroll (GoodixMilan *milan,
                                               const GoodixMilanSample *sample,
                                               GBytes **template_bytes,
                                               GError **error);
FpImage *goodix_milan_sample_image (const GoodixMilanSample *sample);
FpPrint *goodix_milan_probe_print (FpDevice *device, const GoodixMilanSample *sample);
FpPrint *goodix_milan_template_print (FpDevice *device, GBytes *template_bytes);
FpiMatchResult goodix_milan_match (FpImageDevice *device, FpPrint *template_print,
                                  FpPrint *probe, GError **error);
gboolean goodix_milan_validate_print (FpPrint *print, GError **error);
