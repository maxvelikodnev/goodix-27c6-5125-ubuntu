/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "fpi-image-device.h"
#include "goodix-chicago-runtime.h"
#include "goodix-chicago-template.h"
#define GOODIX_CHICAGO_ENROLL_STAGES 12u
#define GOODIX_CHICAGO_TEMPLATE_ID "goodix-27c6-5125-chicagohs-type24-010a665f-v1"
typedef struct _GoodixChicago GoodixChicago;
typedef GBytes GoodixChicagoBytes;
void goodix_chicago_bytes_free (GoodixChicagoBytes *bytes);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GoodixChicagoBytes, goodix_chicago_bytes_free)
typedef struct {
  GoodixChicagoRuntimeProbe *probe;
  guint8 pixels[GOODIX_CHICAGO_PIXELS];
  gint quality, coverage;
} GoodixChicagoSample;
typedef enum { GOODIX_CHICAGO_ACCEPTED, GOODIX_CHICAGO_RETRY,
  GOODIX_CHICAGO_COMPLETE, GOODIX_CHICAGO_ERROR } GoodixChicagoAdmission;
GoodixChicago *goodix_chicago_new (void);
void goodix_chicago_free (GoodixChicago *chicago);
void goodix_chicago_clear_binding (GoodixChicago *chicago);
/* Only validated A6/CONFIG90 preflight evidence may enter here. Uses exact
 * upstream opaque OTP[0:16], plus full-OTP SHA256 as an additional binding. */
gboolean goodix_chicago_bind_preflight (GoodixChicago *chicago, const guint8 *otp,
                                       gsize size, guint64 generation, GError **error);
gboolean goodix_chicago_native_background (GoodixChicago *chicago, const guint16 *background,
  gsize count, guint width, guint height, guint64 generation, GError **error);
gboolean goodix_chicago_native_preprocess (GoodixChicago *chicago, const guint16 *raw,
  gsize count, guint64 generation, GoodixChicagoSample *sample, GError **error);
void goodix_chicago_sample_clear (GoodixChicagoSample *sample);
gboolean goodix_chicago_enroll_start (GoodixChicago *chicago, GError **error);
void goodix_chicago_enroll_clear (GoodixChicago *chicago);
guint goodix_chicago_enroll_progress (GoodixChicago *chicago);
GoodixChicagoAdmission goodix_chicago_enroll (GoodixChicago *chicago,
  GoodixChicagoSample *sample, GBytes **bytes, GError **error);
FpImage *goodix_chicago_sample_image (const GoodixChicagoSample *sample);
FpPrint *goodix_chicago_probe_print (FpDevice *device, GoodixChicagoSample *sample);
FpPrint *goodix_chicago_template_print (GoodixChicago *chicago, FpDevice *device, GBytes *bytes);
FpiMatchResult goodix_chicago_match (GoodixChicago *chicago, FpPrint *reference, FpPrint *probe, GError **error);
gboolean goodix_chicago_validate_print (FpPrint *print, GError **error);
gboolean goodix_chicago_validate_bound_print (GoodixChicago *chicago, FpPrint *print, GError **error);
#ifdef GOODIX_ENABLE_TEST_SEAMS
/* Synthetic identity injection is absent from production builds. */
gboolean goodix_chicago_test_bind (GoodixChicago *chicago, const guint8 id[16], guint64 generation);
#endif
