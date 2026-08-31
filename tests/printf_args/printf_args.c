#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  .formatter = {logcie_token_formatter, (void *)"$m"},
  .writer    = {logcie_file_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("no args at all");
  LOGCIE_INFO("int=%d uint=%u", -7, 7u);
  LOGCIE_INFO("str=%s char=%c", "text", 'x');
  LOGCIE_INFO("hex=%x oct=%o", 255, 8);
  LOGCIE_INFO("float=%.2f", 1.5);
  LOGCIE_INFO("width=%5d left=%-5d|", 42, 42);
  LOGCIE_INFO("percent=%%");
  LOGCIE_INFO("many=%d %d %d %d %d", 1, 2, 3, 4, 5);
  return 0;
}
