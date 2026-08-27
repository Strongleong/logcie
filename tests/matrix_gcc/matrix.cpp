/* NOTE: LOGCIE_INFO_VA only exists when LOGCIE_VA_LOGS is defined, which is not
 * the case on GCC/Clang unless LOGCIE_PEDANTIC is set. Portable code therefore
 * has to branch, exactly as the README's library snippet does. Every
 * combination in test.tspec compiles this one file. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#ifdef LOGCIE_VA_LOGS
#define LOG_INFO(...) LOGCIE_INFO_VA(__VA_ARGS__)
#define LOG_MOD(mod, level, ...) LOGCIE_LOG_MOD_VA(mod, level, __VA_ARGS__)
#else
#define LOG_INFO(...) LOGCIE_INFO(__VA_ARGS__)
#define LOG_MOD(mod, level, ...) LOGCIE_LOG_MOD(mod, level, __VA_ARGS__)
#endif

static Logcie_Sink sink = {
  {logcie_printf_formatter, (void *)"$L $m"},
  {logcie_printf_writer, NULL},
  {NULL, NULL},
};

int main(void) {
  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOG_INFO("n=%d", 1);
  LOG_MOD("mod", WARN, "s=%s", "x");
  return 0;
}
