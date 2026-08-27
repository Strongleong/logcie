/* NOTE: sinks live in a fixed array of LOGCIE_MAX_SINKS, so the interesting
 * cases are the boundary and one past it. This file lowers the limit to 3 so
 * the test does not depend on the default. */
#define LOGCIE_MAX_SINKS 3
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sinks[5];

int main(void) {
  logcie_remove_all_sinks();

  for (size_t i = 0; i < 5; i++) {
    sinks[i].formatter.format = logcie_printf_formatter;
    sinks[i].formatter.data   = (void *)"s:$m";
    sinks[i].writer.write     = logcie_printf_writer;
    sinks[i].writer.data      = stdout;
    sinks[i].filter.filter    = NULL;
    sinks[i].filter.data      = NULL;

    printf("add%zu=%d\n", i, logcie_add_sink(&sinks[i]));
  }

  printf("count=%zu\n", logcie_get_sink_count());

  /* NOTE: three sinks are registered, so one call produces three lines. */
  LOGCIE_INFO("once");

  logcie_remove_all_sinks();
  printf("after_remove_all=%zu\n", logcie_get_sink_count());

  /* NOTE: the default sink is back, so the next add must replace it again
   * rather than append to it. */
  printf("readd=%d\n", logcie_add_sink(&sinks[0]));
  printf("count_after_readd=%zu\n", logcie_get_sink_count());
  return 0;
}
