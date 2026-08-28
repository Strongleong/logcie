#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static size_t reentrant_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)log;

  /* NOTE: logging from inside a writer. The guard must suppress it rather
   * than recurse forever. */
  LOGCIE_INFO("from inside the writer");

  size_t written = fprintf((FILE *)user_data, "%.*s", (int)len, bytes);
  return written;
}

static Logcie_Sink sink = {
  .formatter = {logcie_token_formatter, (void *)"$m"},
  .writer    = {reentrant_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("outer");
  printf("survived\n");
  return 0;
}
