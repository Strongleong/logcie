#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  .formatter = {logcie_token_formatter, (void *)"$M:$m"},
  .writer    = {logcie_file_writer, logcie_file_flush, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);
  LOGCIE_INFO("undeclared");
  return 0;
}
