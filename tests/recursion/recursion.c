#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static size_t reentrant_writer(void *user_data, const char *fmt, va_list *va, ...) {
  /* NOTE: logging from inside a writer. The guard must suppress it rather
   * than recurse forever. */
  LOGCIE_INFO("from inside the writer");

  va_list args;

  if (va != NULL) {
    va_copy(args, *va);
  } else {
    va_start(args, va);
  }

  size_t written = vfprintf((FILE *)user_data, fmt, args);
  va_end(args);
  return written;
}

static Logcie_Sink sink = {
  .formatter = {logcie_printf_formatter, (void *)"$m"},
  .writer    = {reentrant_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_add_sink(&sink);

  LOGCIE_INFO("outer");
  printf("survived\n");
  return 0;
}
