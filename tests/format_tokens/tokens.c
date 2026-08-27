/* NOTE: the log call below is on a fixed line. tests/format_tokens/test.tspec
 * asserts the $x token, so moving it breaks that test on purpose. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  .formatter = {logcie_printf_formatter, NULL},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(int argc, char **argv) {
  if (argc != 2) {
    return 2;
  }

  sink.formatter.data = argv[1];
  sink.writer.data    = stdout;

  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_LOG_MOD("net", INFO, "msg=%d", 7); /* LINE 23 - DO NOT MOVE */
  return 0;
}
