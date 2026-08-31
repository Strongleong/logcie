// Everything at once, laid out the way a real program would.
//
// Two sinks over the same call sites: a console filtered down to what a person
// watching the terminal wants, and a file that keeps everything for afterwards.
// The subsystems in api.c and storage.c log under their own modules and know
// nothing about either sink -- turning storage down is a change here, not
// there. That separation is the whole point of modules.

// $N needs a sub-second clock. C11 has one; on C99 you opt into the POSIX one
// before any header, which is why this comes first.
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

#include "app.h"

// Warnings and worse belong on stderr so a shell can separate them.
static size_t console_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;

  FILE *out = log->level >= LOGCIE_LEVEL_WARN ? stderr : stdout;
  return fwrite(bytes, 1, len, out);
}

// Console: INFO and up, but never the storage chatter. Neither storage.c nor
// api.c is aware of this.
static Logcie_Sink console = {
  .formatter = {logcie_token_formatter, "$c$L$<6$r ($M) $m"},
  .writer    = {console_writer, NULL},
  .filter    = logcie_filter_and(
    logcie_filter_level_min(LOGCIE_LEVEL_INFO),
    logcie_filter_not(logcie_filter_module_prefix_eq("app.storage"))
  )
};

// File: everything, with enough detail to debug from afterwards.
static Logcie_Sink logfile = {
  .formatter = {logcie_token_formatter, "$d $t.$N [$l] $M $f:$x $m"},
  .writer    = {logcie_file_writer, NULL},
  .filter    = {NULL, NULL}
};

int main(void) {
  logfile.writer.data = fopen("app.log", "w");

  if (!logfile.writer.data) {
    fprintf(stderr, "cannot open app.log\n");
    return 1;
  }

  logcie_remove_sink(logcie_get_default_sink());
  logcie_add_sink(&console);
  logcie_add_sink(&logfile);

  LOGCIE_INFO("starting");

  storage_open("/var/lib/app");
  api_serve(4);
  storage_close();

  LOGCIE_INFO("done, see app.log for the full trace");

  fclose((FILE *)logfile.writer.data);
  return 0;
}
