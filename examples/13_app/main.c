// Everything at once, laid out the way a real program would.
//
// Two sinks over the same call sites: a console filtered down to what a person
// watching the terminal wants, and a file that keeps everything as JSON for
// afterwards. The subsystems in api.c and storage.c log under their own modules
// and know nothing about either sink -- turning storage down is a change here,
// not there. That separation is the whole point of modules.
//
// The console uses the built-in token formatter; the file uses one written
// here. Both use writers written here. Requests are served on threads, which is
// what LOGCIE_THREAD_SAFE is for.
//
// If a sink of yours would block -- a socket, a disk that stalls -- give it a
// thread of its own instead. See examples/10_async_sink.

// $N needs a sub-second clock. C11 has one; on C99 you opt into the POSIX one
// before any header, which is why this comes first.
#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define LOGCIE_MODULE "app"
#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

#include "app.h"

#define WORKERS 3

// Warnings and worse belong on stderr so a shell can separate them.
static size_t console_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;

  FILE *out = log->level >= LOGCIE_LEVEL_WARN ? stderr : stdout;
  return fwrite(bytes, 1, len, out);
}

static void console_flush(void *user_data) {
  (void)user_data;
  fflush(stdout);
  fflush(stderr);
}

// A formatter turns a log into bytes and hands them to the writer in one call.
// This one emits JSON, which the token formatter cannot: a machine reading the
// file wants fields, not a sentence.
static size_t json_escape(char *out, size_t cap, const char *text) {
  size_t used = 0;

  for (size_t i = 0; text[i] != '\0' && used + 2 < cap; i++) {
    if (text[i] == '"' || text[i] == '\\') {
      out[used++] = '\\';
    }

    out[used++] = text[i];
  }

  out[used] = '\0';
  return used;
}

static size_t json_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log) {
  (void)user_data;

  char message[512];
  char escaped[1024];
  char line[1536];

  // log.msg is the finished message: logcie_log applied the printf arguments
  // before any sink ran. logcie_render_message copies it out.
  //
  // This file is the one that defines LOGCIE_IMPLEMENTATION, so logcie's own
  // statics are in scope. get_logcie_level_label is the same one $l uses.
  logcie_render_message(message, sizeof(message), &log);
  json_escape(escaped, sizeof(escaped), message);

  int written = snprintf(line, sizeof(line), "{\"level\":\"%s\",\"module\":\"%s\",\"file\":\"%s\",\"line\":%u,\"msg\":\"%s\"}\n", get_logcie_level_label(log.level), log.module ? log.module : "", log.location.file, log.location.line, escaped);

  if (written < 0) {
    return 0;
  }

  size_t len = (size_t)written < sizeof(line) ? (size_t)written : sizeof(line) - 1;

  return writer->write(writer->data, &log, line, len);
}

// Console: INFO and up, but never the storage chatter. Neither storage.c nor
// api.c is aware of this.
static Logcie_Sink console = {
  .formatter = {logcie_token_formatter, "$c$L$<6$r ($M) $m"},
  .writer    = {console_writer, console_flush, NULL},
  .filter    = logcie_filter_and(
    logcie_filter_level_min(LOGCIE_LEVEL_INFO),
    logcie_filter_not(logcie_filter_module_prefix_eq("app.storage"))
  )
};

// File: everything, as one JSON object per line.
static Logcie_Sink logfile = {
  .formatter = {json_formatter, NULL},
  .writer    = {logcie_file_writer, logcie_file_flush, NULL},
  .filter    = {NULL, NULL}
};

static void *worker_main(void *arg) {
  api_serve((int)(intptr_t)arg, 4);
  return NULL;
}

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

  // Sinks are registered before the threads start and removed after they join.
  // LOGCIE_THREAD_SAFE covers logging, not reconfiguration.
  pthread_t workers[WORKERS];

  for (long i = 0; i < WORKERS; i++) {
    if (pthread_create(&workers[i], NULL, worker_main, (void *)i) != 0) {
      LOGCIE_ERROR("cannot start worker %ld", i);
      return 1;
    }
  }

  for (int i = 0; i < WORKERS; i++) {
    pthread_join(workers[i], NULL);
  }

  storage_close();
  LOGCIE_INFO("done, see app.log for the full trace");

  logcie_flush();

  logcie_remove_sink(&logfile);
  fclose((FILE *)logfile.writer.data);
  return 0;
}
