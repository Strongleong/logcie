#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  .formatter = {logcie_printf_formatter, (void *)"$L $m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_add_sink(&sink);

  LOGCIE_TRACE("trace");
  LOGCIE_DEBUG("debug");
  LOGCIE_VERBOSE("verbose");
  LOGCIE_INFO("info");
  LOGCIE_WARN("warn");
  LOGCIE_ERROR("error");
  LOGCIE_FATAL("fatal");
  return 0;
}
