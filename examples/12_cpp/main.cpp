// Logcie is a C library that is usable from C++: its declarations are wrapped
// in extern "C", and the filter combinators have separate C++ implementations
// because the C ones are built on compound literals, which C++ does not have.
//
// Everything else is the same interface. The build system compiles this as C++
// because of the .cpp extension.
#include <cstdio>
#include <string>

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

// A writer is a plain function pointer, so it cannot be a capturing lambda or a
// non-static member function.
static size_t console_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;

  std::FILE *out = log->level >= LOGCIE_LEVEL_WARN ? stderr : stdout;
  return std::fwrite(bytes, 1, len, out);
}

int main() {
  // No designated initializers: C++ only gained them in C++20 and this is
  // built as C++11
  Logcie_Sink console = {
    {logcie_token_formatter, (void *)"[$L] ($M) $m"},
    {console_writer, logcie_file_flush, NULL},
    logcie_filter_and(
      logcie_filter_level_min(LOGCIE_LEVEL_INFO),
      logcie_filter_not(logcie_filter_module_eq("noisy"))
    )
  };

  logcie_remove_sink(logcie_get_default_sink());
  logcie_add_sink(&console);

  LOGCIE_INFO("hello from C++");

  // The message is still a printf format string, so a std::string goes in as
  // %s and c_str()
  std::string who = "std::string";
  LOGCIE_INFO("printf formatting still applies: %s", who.c_str());

  LOGCIE_LOG_MOD("noisy", ERROR, "filtered out by module");
  LOGCIE_WARN("this one reaches stderr");

  return 0;
}
