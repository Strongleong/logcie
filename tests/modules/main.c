#define LOGCIE_MODULE "core"
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

void other(void);

static Logcie_Sink sink = {
  .formatter = {logcie_token_formatter, (void *)"$M:$m"},
  .writer    = {logcie_file_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("from main");
  other();
  LOGCIE_LOG_MOD("explicit", INFO, "per call");
  return 0;
}
