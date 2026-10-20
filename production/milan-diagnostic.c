/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Explicit M5.B client of the canonical runtime. One CAPTURE, never auth. */
#include "fprint.h"
#include <glib-unix.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

static gboolean
cancel_capture (gpointer data)
{
  g_cancellable_cancel (G_CANCELLABLE (data));
  return G_SOURCE_CONTINUE;
}

typedef struct { gboolean touch, lift; } Prompts;
static void
finger_status (GObject *object, GParamSpec *property, gpointer data)
{
  Prompts *prompts = data;
  FpFingerStatusFlags status = fp_device_get_finger_status (FP_DEVICE (object));
  (void) property;
  if ((status & FP_FINGER_STATUS_NEEDED) && !(status & FP_FINGER_STATUS_PRESENT) && !prompts->touch)
    { prompts->touch = TRUE; g_print ("APPOGGIA: one diagnostic contact.\n"); }
  if ((status & FP_FINGER_STATUS_PRESENT) && !(status & FP_FINGER_STATUS_NEEDED) && !prompts->lift)
    { prompts->lift = TRUE; g_print ("SOLLEVA: release the reader; no further contact.\n"); }
}

int
main (int argc, char **argv)
{
  const struct rlimit no_core = { 0, 0 };
  FpContext *context = NULL;
  FpDevice *device = NULL;
  FpImage *image = NULL;
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  guint deadline = 0, interrupt = 0, terminate = 0;
  gboolean opened = FALSE, armed = FALSE;
  Prompts prompts = { 0 };
  int result = 1;
  if (argc == 2 && (!strcmp (argv[1], "--help") || !strcmp (argv[1], "--offline-check")))
    {
      /* Deliberately before fp_context_new(): no enumeration, USB, protected
       * files, calibration or runtime state is accessed in either mode. */
      g_print (!strcmp (argv[1], "--help") ?
        "Usage: goodix-milan-diagnostic --offline-check | --session PRIVATE_DIRECTORY\n"
        "M5.B requires explicit authorization and root. Exactly one capture, 30s deadline.\n"
        "No enrollment, verification, identification, pairing provisioning or automatic retry.\n" :
        "M5B_OFFLINE_CHECK=PASS LIVE=NOT_RUN PROTECTED_DATA=NOT_OPENED USB=NOT_ENUMERATED\n");
      return 0;
    }
  if (argc != 3 || strcmp (argv[1], "--session") || !g_path_is_absolute (argv[2]))
    { g_printerr ("Use --help or --session with an absolute private directory\n"); return 2; }
  if (geteuid () != 0 || setrlimit (RLIMIT_CORE, &no_core) || prctl (PR_SET_DUMPABLE, 0))
    { g_printerr ("M5.B requires explicit root authorization and private process state\n"); return 2; }
  umask (0077);
  cancellable = g_cancellable_new ();
  interrupt = g_unix_signal_add (SIGINT, cancel_capture, cancellable);
  terminate = g_unix_signal_add (SIGTERM, cancel_capture, cancellable);
  context = fp_context_new ();
  GPtrArray *devices = fp_context_get_devices (context);
  for (guint i = 0; i < devices->len; i++)
    {
      FpDevice *candidate = g_ptr_array_index (devices, i);
      if (strcmp (fp_device_get_driver (candidate), "goodix_27c6_5125_milan")) continue;
      if (device) { g_printerr ("STOP: multiple Milan readers; no capture\n"); goto out; }
      device = candidate;
    }
  if (!device || !g_signal_lookup ("goodix-milan-diagnostic-arm", G_OBJECT_TYPE (device)))
    { g_printerr ("STOP: canonical Milan reader/diagnostic capability unavailable\n"); goto out; }
  if (!fp_device_open_sync (device, cancellable, &error)) goto out;
  opened = TRUE;
  g_signal_emit_by_name (device, "goodix-milan-diagnostic-arm", argv[2], &armed);
  if (!armed) { g_printerr ("STOP: diagnostic authorization/provenance rejected; no capture\n"); goto out; }
  g_signal_connect (device, "notify::finger-status", G_CALLBACK (finger_status), &prompts);
  g_print ("M5.B: binding UNKNOWN; wait for APPOGGIA. One contact, no retry.\n");
  deadline = g_timeout_add_seconds (30, cancel_capture, cancellable);
  image = fp_device_capture_sync (device, TRUE, cancellable, &error);
  g_source_remove (deadline);
  deadline = 0;
  if (!image) goto out;
  if (fp_image_get_width (image) != 80 || fp_image_get_height (image) != 64)
    { g_printerr ("STOP: unexpected diagnostic image geometry\n"); goto out; }
  g_print ("M5B_CAPTURE=PASS AUTHENTICATION_QUALIFIED=false BINDING=UNKNOWN; private raw/preprocess/metrics recorded. Stop and review before any further contact.\n");
  result = 0;
out:
  if (deadline) g_source_remove (deadline);
  if (error) { g_printerr ("M5B_CAPTURE=STOP %s\n", error->message); g_clear_error (&error); }
  /* Closing waits for the driver's bounded cancellation/release/drain path.
   * There is no second CAPTURE or recovery command on any error path. */
  if (opened && !fp_device_close_sync (device, NULL, &error))
    { g_printerr ("M5B_CLOSE=STOP %s; stop and use documented recovery\n", error->message); g_clear_error (&error); result = 1; }
  if (interrupt) g_source_remove (interrupt);
  if (terminate) g_source_remove (terminate);
  g_clear_object (&image);
  g_clear_object (&context);
  g_clear_object (&cancellable);
  return result;
}
