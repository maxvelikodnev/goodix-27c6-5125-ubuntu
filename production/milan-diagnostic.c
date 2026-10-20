/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Explicit private M5 client. Never an fprintd/PAM opt-in. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "fprint.h"
#include <glib-unix.h>
#include <errno.h>
#include <fcntl.h>
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

typedef struct {
  gboolean needed, present, enrolling;
  guint contacts;
  GCancellable *cancel;
} Prompts;

static void
finger_status (GObject *object, GParamSpec *property, gpointer data)
{
  Prompts *prompts = data;
  FpFingerStatusFlags status = fp_device_get_finger_status (FP_DEVICE (object));
  gboolean needed = (status & FP_FINGER_STATUS_NEEDED) != 0;
  gboolean present = (status & FP_FINGER_STATUS_PRESENT) != 0;
  (void) property;
  if (present && !prompts->present) prompts->contacts++;
  if (needed && !present && (!prompts->needed || prompts->present))
    {
      /* Enrollment only: bounded observation, without matcher threshold edits.
       * A probe action is always one call/contact, including quality rejection. */
      if (prompts->contacts >= (prompts->enrolling ? 36u : 1u))
        { g_print ("STOP: contact budget reached.\n"); g_cancellable_cancel (prompts->cancel); }
      else g_print ("APPOGGIA: %s\n", prompts->enrolling ? "same enrollment finger." : "one explicit probe/capture.");
    }
  if (present && !needed && (prompts->needed || !prompts->present))
    g_print ("SOLLEVA: release the reader.\n");
  prompts->needed = needed;
  prompts->present = present;
}

static void
enroll_progress (FpDevice *device, gint stages, FpPrint *print, gpointer data, GError *error)
{
  (void) print; (void) data;
  g_print ("M5C_ENROLL_PROGRESS=%d/%d%s\n", stages, fp_device_get_nr_enroll_stages (device),
           error ? " quality/diversity rejected; no stage accepted" : "");
}

/* No symlink ancestor; only private root-owned final directory is accepted. */
static int
open_session (const gchar *path)
{
  struct stat st;
  int fd = -1;
  g_auto(GStrv) parts = NULL;
  if (!g_path_is_absolute (path) || !strstr (path, "/development/")) return -1;
  parts = g_strsplit (path + 1, "/", -1);
  fd = open ("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  for (guint i = 0; fd >= 0 && parts[i]; i++)
    {
      int next;
      if (!*parts[i] || !strcmp (parts[i], ".") || !strcmp (parts[i], "..")) { close (fd); return -1; }
      next = openat (fd, parts[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
      close (fd); fd = next;
    }
  if (fd >= 0 && (fstat (fd, &st) || st.st_uid != 0 || (st.st_mode & 07777) != 0700))
    { close (fd); return -1; }
  return fd;
}

static gboolean
same_file (const struct stat *a, const struct stat *b)
{
  return a->st_dev == b->st_dev && a->st_ino == b->st_ino && a->st_mode == b->st_mode &&
    a->st_uid == b->st_uid && a->st_nlink == b->st_nlink && a->st_size == b->st_size &&
    a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
    a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static FpPrint *
read_template (int directory, GError **error)
{
  struct stat before, after, named;
  int fd = openat (directory, "milan-template.fp", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  FpPrint *print = NULL;
  guint8 *bytes = NULL;
  gsize offset = 0;
  if (fd < 0 || fstat (fd, &before) || !S_ISREG (before.st_mode) || before.st_uid != 0 ||
      (before.st_mode & 07777) != 0600 || before.st_nlink != 1 || before.st_size <= 0 || before.st_size > 512 * 1024)
    goto out;
  bytes = g_malloc ((gsize) before.st_size);
  while (offset < (gsize) before.st_size)
    {
      ssize_t count = read (fd, bytes + offset, (gsize) before.st_size - offset);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) goto out;
      offset += (gsize) count;
    }
  if (fstat (fd, &after) || fstatat (directory, "milan-template.fp", &named, AT_SYMLINK_NOFOLLOW) ||
      !same_file (&before, &after) || !same_file (&after, &named)) goto out;
  print = fp_print_deserialize (bytes, offset, error);
out:
  if (fd >= 0) close (fd);
  if (bytes) { explicit_bzero (bytes, offset); g_free (bytes); }
  if (!print && !*error) g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Private Milan template absent, corrupt or untrusted");
  return print;
}

static gboolean
save_template (int directory, FpPrint *print, GError **error)
{
  guint8 *bytes = NULL;
  gsize size = 0, offset = 0;
  int fd = -1;
  gboolean ok = FALSE;
  if (!fp_print_serialize (print, &bytes, &size, error)) return FALSE;
  fd = openat (directory, "milan-template.fp", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) goto out;
  while (offset < size)
    {
      ssize_t count = write (fd, bytes + offset, size - offset);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) goto out;
      offset += (gsize) count;
    }
  ok = fsync (fd) == 0 && fsync (directory) == 0;
out:
  if (fd >= 0 && close (fd)) ok = FALSE;
  explicit_bzero (bytes, size); g_free (bytes);
  if (!ok) g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Private template write failed; stop without retry");
  return ok;
}

int
main (int argc, char **argv)
{
  const struct rlimit no_core = { 0, 0 };
  g_autoptr(FpContext) context = NULL;
  FpDevice *device = NULL;
  g_autoptr(FpImage) image = NULL;
  g_autoptr(FpPrint) reference = NULL, enrolled = NULL, identified = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(GError) error = NULL;
  guint deadline = 0, interrupt = 0, terminate = 0;
  gboolean opened = FALSE, armed = FALSE, experimental = FALSE, match = FALSE;
  const gchar *operation = "capture", *directory = NULL;
  int result = 1, session_fd = -1;
  if (argc == 2 && (!strcmp (argv[1], "--help") || !strcmp (argv[1], "--offline-check")))
    {
      /* Before context creation: no enumeration, USB or protected-data I/O. */
      g_print (!strcmp (argv[1], "--help") ?
        "Usage: goodix-milan-diagnostic --offline-check | --session PRIVATE_DIRECTORY\n"
        "       goodix-milan-diagnostic --m5c enroll|verify|identify --session PRIVATE_DIRECTORY\n"
        "Explicit root/USB/biometric authorization required. M5.B: one capture/30s.\n"
        "M5.C: one enrollment (18 stages, at most 36 contacts/600s), one probe per invocation/30s.\n"
        "At most three verify/identify attempts per session combined; stop at first MATCH.\n"
        "Private Milan template only. No fprintd, PAM, pairing provisioning or automatic retry.\n" :
        "M5B_OFFLINE_CHECK=PASS M5C_CLI=AVAILABLE LIVE=NOT_RUN PROTECTED_DATA=NOT_OPENED USB=NOT_ENUMERATED\n");
      return 0;
    }
  if (argc == 3 && !strcmp (argv[1], "--session")) directory = argv[2];
  else if (argc == 5 && !strcmp (argv[1], "--m5c") && !strcmp (argv[3], "--session") &&
           (!strcmp (argv[2], "enroll") || !strcmp (argv[2], "verify") || !strcmp (argv[2], "identify")))
    { experimental = TRUE; operation = argv[2]; directory = argv[4]; }
  if (!directory || !g_path_is_absolute (directory))
    { g_printerr ("Use --help for explicit session options\n"); return 2; }
  if (geteuid () != 0 || setrlimit (RLIMIT_CORE, &no_core) || prctl (PR_SET_DUMPABLE, 0))
    { g_printerr ("M5 requires explicit root authorization and private process state\n"); return 2; }
  umask (0077);
  if (experimental && (session_fd = open_session (directory)) < 0)
    { g_printerr ("STOP: untrusted private session\n"); return 2; }
  cancellable = g_cancellable_new ();
  Prompts prompts = { .enrolling = !strcmp (operation, "enroll"), .cancel = cancellable };
  interrupt = g_unix_signal_add (SIGINT, cancel_capture, cancellable);
  terminate = g_unix_signal_add (SIGTERM, cancel_capture, cancellable);
  context = fp_context_new ();
  GPtrArray *devices = fp_context_get_devices (context);
  for (guint i = 0; i < devices->len; i++)
    {
      FpDevice *candidate = g_ptr_array_index (devices, i);
      if (strcmp (fp_device_get_driver (candidate), "goodix_27c6_5125_milan")) continue;
      if (device) { g_printerr ("STOP: multiple Milan readers\n"); goto out; }
      device = candidate;
    }
  const gchar *signal = experimental ? "goodix-milan-experimental-arm" : "goodix-milan-diagnostic-arm";
  if (!device || !g_signal_lookup (signal, G_OBJECT_TYPE (device)))
    { g_printerr ("STOP: canonical Milan reader/capability unavailable\n"); goto out; }
  if (!fp_device_open_sync (device, cancellable, &error)) goto out;
  opened = TRUE;
  if (experimental) g_signal_emit_by_name (device, signal, directory, session_fd, &armed);
  else g_signal_emit_by_name (device, signal, directory, &armed);
  if (!armed) { g_printerr ("STOP: private authorization/provenance rejected\n"); goto out; }
  if (experimental && !prompts.enrolling)
    {
      reference = read_template (session_fd, &error);
      if (!reference) goto out;
      if (!fp_print_compatible (reference, device))
        { g_printerr ("STOP: template is not from this Milan namespace/device\n"); goto out; }
    }
  g_signal_connect (device, "notify::finger-status", G_CALLBACK (finger_status), &prompts);
  g_print ("M5: wait for APPOGGIA; OTP prefix equality is mandatory. Authentication remains unqualified.\n");
  deadline = g_timeout_add_seconds (prompts.enrolling ? 600 : 30, cancel_capture, cancellable);
  if (!experimental)
    {
      image = fp_device_capture_sync (device, TRUE, cancellable, &error);
      if (!image) goto out;
      if (fp_image_get_width (image) != 80 || fp_image_get_height (image) != 64)
        { g_printerr ("STOP: unexpected image geometry\n"); goto out; }
      g_print ("M5B_CAPTURE=PASS OTP_PREFIX=MATCH AUTHENTICATION_QUALIFIED=false; private metrics recorded.\n");
    }
  else if (prompts.enrolling)
    {
      reference = g_object_ref_sink (fp_print_new (device));
      fp_print_set_finger (reference, FP_FINGER_RIGHT_INDEX);
      fp_print_set_username (reference, "m5c-private-experiment");
      enrolled = fp_device_enroll_sync (device, reference, cancellable, enroll_progress, NULL, &error);
      if (!enrolled || !save_template (session_fd, enrolled, &error)) goto out;
      g_print ("M5C_ENROLL=PASS OTP_PREFIX=MATCH CONTACTS=%u TEMPLATE=private AUTHENTICATION_QUALIFIED=false\n", prompts.contacts);
    }
  else
    {
      if (!strcmp (operation, "verify"))
        {
          if (!fp_device_verify_sync (device, reference, cancellable, NULL, NULL, &match, NULL, &error)) goto out;
        }
      else
        {
          g_autoptr(GPtrArray) gallery = g_ptr_array_new ();
          g_ptr_array_add (gallery, reference);
          if (!fp_device_identify_sync (device, gallery, cancellable, NULL, NULL, &identified, NULL, &error)) goto out;
          match = identified != NULL;
        }
      g_print ("M5C_PROBE=%s OTP_PREFIX=MATCH CONTACTS=%u AUTHENTICATION_QUALIFIED=false; no automatic retry.\n", match ? "MATCH_STOP" : "NO_MATCH", prompts.contacts);
    }
  result = 0;
out:
  if (deadline) g_source_remove (deadline);
  if (error) { g_printerr ("M5_ACTION=STOP %s\n", error->message); g_clear_error (&error); }
  if (opened && !fp_device_close_sync (device, NULL, &error))
    { g_printerr ("M5_CLOSE=STOP %s; use documented recovery\n", error->message); result = 1; }
  else if (opened) g_print ("M5_CLOSE=PASS\n");
  if (interrupt) g_source_remove (interrupt);
  if (terminate) g_source_remove (terminate);
  if (session_fd >= 0) close (session_fd);
  return result;
}
