/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Offline API proof only. No device, Windows calidata or fingerprint template.
 * Optional stdin contains exactly one already-authorized no-finger U16LE image.
 * Neither images nor the derived calibration state are written to output. */
#include "521d-wrapper.h"
#include "521d-checkpoint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
clear_memory (void *data, size_t size)
{
  volatile unsigned char *bytes = data;
  while (size--) *bytes++ = 0;
}

int
main (int argc, char **argv)
{
  uint16_t background[GX521D_PIXELS], original[GX521D_PIXELS];
  Gx521dPreprocessor first = {0}, second = {0};
  Gx521dImageCheckpoint a = {0}, b = {0};
  int from_stdin = argc == 2 && strcmp (argv[1], "--background-stdin") == 0;
  if (argc != 1 && !from_stdin) return 2;
  for (size_t i = 0; i < GX521D_PIXELS; i++)
    {
      if (from_stdin)
        {
          int lo = getchar (), hi = getchar ();
          if (lo == EOF || hi == EOF) return 2;
          background[i] = (uint16_t) ((unsigned) lo | ((unsigned) hi << 8));
          if (background[i] > 4095) return 2;
        }
      else
        background[i] = (uint16_t) (1800 + (i % 80) * 2 + (i / 80));
    }
  if (from_stdin && (getchar () != EOF || ferror (stdin))) return 2;
  memcpy (original, background, sizeof original);
  assert (gx521d_preprocessor_init (&first, background) == 0);
  assert (first.parameters_ready == 1 && first.calibrated == 1);
  assert (first.image != NULL && first.loaded == NULL);
  assert (memcmp (original, background, sizeof original) == 0);
  assert (gx521d_image_checkpoint (first.image, &a) == 0);
  assert (a.version == 1 && a.rows == GX521D_ROWS && a.columns == GX521D_COLUMNS);
  assert (gx521d_preprocessor_init (&second, background) == 0);
  assert (gx521d_image_checkpoint (second.image, &b) == 0);
  assert (memcmp (&a, &b, sizeof a) == 0);
  gx521d_preprocessor_clear (&first);
  gx521d_preprocessor_clear (&second);
  assert (!first.parameters_ready && !first.calibrated && !first.image && !first.loaded);
  assert (gx521d_preprocessor_init (&first, NULL) != 0);
  assert (!first.calibrated && !first.image && !first.loaded);
  gx521d_preprocessor_clear (&first);
  clear_memory (background, sizeof background);
  clear_memory (original, sizeof original);
  clear_memory (&a, sizeof a);
  clear_memory (&b, sizeof b);
  puts ("BACKGROUND_INIT=PASS WINDOWS_CALIDATA=NOT_USED INPUT_UNCHANGED=PASS "
        "DETERMINISTIC_STATE=PASS NULL_REJECTED=PASS CLEANUP=PASS "
        "BIOMETRIC_QUALIFICATION=NOT_TESTED");
  return 0;
}
