/* NOTE: $N is fed from a hand-built Logcie_Log rather than from a live clock,
 * so the expected bytes in test.tspec are exact. The point of the test is the
 * token's width and that it stops where it should - $N used to fall through
 * into $z and to corrupt the $<n that followed it. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Writer writer = {logcie_printf_writer, NULL};

static void render(const char *fmt, Logcie_Log log) {
  logcie_printf_formatter(&writer, (void *)fmt, log, NULL);
}

int main(void) {
  Logcie_Log log = logcie_make_log("mod", LOGCIE_LEVEL_INFO, "msg", "f.c", 7);

  writer.data = stdout;
  log.time    = 0;
  log.nanos   = 42;

  render("[$N]", log);

  log.nanos = 987654321u;
  render("[$N]", log);

  /* NOTE: $N followed by padding. A `+=` on the padding accumulator instead of
   * a `=` shows up here and nowhere else. */
  render("[$N$<12|]", log);
  render("[$L$<6|$N]", log);

  return 0;
}
