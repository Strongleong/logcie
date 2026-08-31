// Strict ISO C99 has no way to write a variadic macro with zero arguments, so
// the usual LOGCIE_INFO("text") would be a constraint violation. GCC and Clang
// have an extension for it and Logcie uses that by default; LOGCIE_PEDANTIC
// turns it off.
//
// In that mode the classic macros take a message and nothing else, and every
// call with arguments uses the _VA form. Logcie defines LOGCIE_VA_LOGS when
// this is in effect, so portable code can branch on it.
//
// Built with -pedantic -DLOGCIE_PEDANTIC, from this directory's build.flags.
#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

int main(void) {
  // No arguments: the classic macros still work
  LOGCIE_INFO("starting");
  LOGCIE_WARN("no arguments needed here");

  // With arguments: the _VA form
  LOGCIE_INFO_VA("value is %d", 42);
  LOGCIE_ERROR_VA("failed: %s", "no such file");

  // The same split applies to the explicit-module macros
  LOGCIE_LOG_MOD("net", INFO, "connected");
  LOGCIE_LOG_MOD_VA("net", WARN, "retry %d of %d", 2, 5);

#ifdef LOGCIE_VA_LOGS
  LOGCIE_INFO("LOGCIE_VA_LOGS is defined, so the _VA macros exist");
#endif

  // A combinator filter cannot sit in a file-scope initializer here: ISO C does
  // not accept the address of a compound literal as a constant expression, and
  // the other examples rely on a GCC and Clang extension for that. Building it
  // at run time works everywhere.
  logcie_get_default_sink()->filter = logcie_filter_level_min(LOGCIE_LEVEL_INFO);
  LOGCIE_DEBUG("filtered out");

  return 0;
}
