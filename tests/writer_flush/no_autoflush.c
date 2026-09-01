#define LOGCIE_AUTOFLUSH_DISABLE
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static int flushes = 0;

static size_t counting_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;
  (void)bytes;
  return len;
}

static void counting_flush(void *user_data) {
  (void)user_data;
  flushes++;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$m"},
  {counting_writer, counting_flush, NULL},
  {NULL, NULL},
};

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_ERROR("would autoflush by default");
  LOGCIE_FATAL("so would this");
  printf("after_logging=%d\n", flushes);

  logcie_flush();
  printf("after_explicit=%d\n", flushes);

  return 0;
}
