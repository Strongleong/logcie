// NOTE: logcie.h wraps its declarations in extern "C" and swaps
// logcie_filter_and/or for lambda-based versions under __cplusplus. Nothing
// else in the suite compiles that path.
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  {logcie_printf_formatter, (void *)"$L/$M/$m"},
  {logcie_printf_writer, NULL},
  logcie_filter_and(
    logcie_filter_level_min(LOGCIE_LEVEL_INFO),
    logcie_filter_module_eq("core")
  ),
};

int main() {
  sink.writer.data = stdout;
  logcie_add_sink(&sink);

  LOGCIE_LOG_MOD("core",  DEBUG, "dropped by level");
  LOGCIE_LOG_MOD("other", INFO,  "dropped by module");
  LOGCIE_LOG_MOD("core",  INFO,  "kept");
  return 0;
}
