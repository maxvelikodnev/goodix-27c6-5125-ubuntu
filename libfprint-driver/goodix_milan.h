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
} GoodixMilanSample;

typedef enum {
  GOODIX_MILAN_ACCEPTED,
  GOODIX_MILAN_RETRY,
  GOODIX_MILAN_COMPLETE,
  GOODIX_MILAN_ERROR,
} GoodixMilanEnrollmentResult;

/* Inert payload only; caller retains its bytes. No file or device access. */
GoodixMilan *goodix_milan_new (const guint8 *payload, gsize size, GError **error);
/* Production fails closed without file access until source, envelope and
 * device binding are qualified. Never creates or provisions calibration. */
GoodixMilan *goodix_milan_load (GError **error);
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
