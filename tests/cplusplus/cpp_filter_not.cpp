// NOTE: logcie_filter_not stores &f, and a compound literal is an lvalue in C
// but an rvalue in C++, so this does not compile. logcie_filter_and and
// logcie_filter_or already have C++ lambda versions; not does not.
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$M/$m"},
  {logcie_file_writer, logcie_file_flush, NULL},
  logcie_filter_not(logcie_filter_module_eq("quiet")),
};

int main() {
  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_LOG_MOD("quiet", INFO, "dropped");
  LOGCIE_LOG_MOD("loud",  INFO, "kept");
  return 0;
}
