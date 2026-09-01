#define LOGCIE_IMPLEMENTATION
#include <string.h>

#include "logcie.h"

#define BUFFER_CAP 64

// NOTE: a buffer of its own rather than stdio's, so a flush is observable
// without fflush(NULL) reaching into streams this sink does not own.
static char   buffer[BUFFER_CAP];
static size_t buffer_filled  = 0;
static int    writes         = 0;
static int    flushes        = 0;
static int    plain_writes   = 0;
static int    second_writes  = 0;
static int    second_flushes = 0;

static void buffered_flush(void *user_data) {
  (void)user_data;

  flushes++;
  buffer_filled = 0;
}

static size_t buffered_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)log;
  writes++;

  if (buffer_filled + len > BUFFER_CAP) {
    buffered_flush(user_data);
  }

  if (len > BUFFER_CAP) {
    len = BUFFER_CAP;
  }

  memcpy(buffer + buffer_filled, bytes, len);
  buffer_filled += len;
  return len;
}

// NOTE: this sink has no flusher at all, which is legal. The autoflush path
// used to call through the NULL.
static size_t plain_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;
  (void)bytes;
  plain_writes++;
  return len;
}

static size_t second_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;
  (void)bytes;
  second_writes++;
  return len;
}

static void second_flush(void *user_data) {
  (void)user_data;
  second_flushes++;
}

static Logcie_Sink buffered = {
  {logcie_token_formatter, (void *)"$m"},
  {buffered_writer, buffered_flush, NULL},
  {NULL, NULL},
};

static Logcie_Sink plain = {
  {logcie_token_formatter, (void *)"$m"},
  {plain_writer, NULL, NULL},
  {NULL, NULL},
};

static Logcie_Sink second = {
  {logcie_token_formatter, (void *)"$m"},
  {second_writer, second_flush, NULL},
  {NULL, NULL},
};

static void report(const char *stage) {
  printf("%s: w=%d f=%d plain_w=%d second_w=%d second_f=%d\n", stage, writes, flushes, plain_writes, second_writes, second_flushes);
}

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&buffered);
  logcie_add_sink(&plain);
  logcie_add_sink(&second);

  LOGCIE_INFO("short");
  report("below_level");

  logcie_flush();
  report("explicit");

  LOGCIE_ERROR("at level");
  report("autoflush_error");

  LOGCIE_FATAL("above level");
  report("autoflush_fatal");

  buffered.filter = logcie_filter_module_eq("nothing");
  LOGCIE_ERROR("rejected");
  report("filtered");

  buffered.filter.filter = NULL;
  buffered.filter.data   = NULL;

  LOGCIE_INFO("a line long enough on its own to push that sixty four byte buffer past its limit");
  report("self_flush");

  logcie_remove_all_sinks();
  logcie_flush();
  report("no_sinks");

  return 0;
}
