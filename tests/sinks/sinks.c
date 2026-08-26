#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink a = {
  .formatter = {logcie_printf_formatter, (void *)"a:$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

static Logcie_Sink b = {
  .formatter = {logcie_printf_formatter, (void *)"b:$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

int main(void) {
  a.writer.data = stdout;
  b.writer.data = stdout;

  printf("initial=%zu\n", logcie_get_sink_count());

  logcie_add_sink(&a);
  printf("after_first_add=%zu\n", logcie_get_sink_count());
  LOGCIE_INFO("one");

  logcie_add_sink(&b);
  printf("after_second_add=%zu\n", logcie_get_sink_count());
  LOGCIE_INFO("two");

  logcie_remove_sink(&a);
  printf("after_remove=%zu\n", logcie_get_sink_count());
  LOGCIE_INFO("three");

  logcie_remove_all_sinks();
  printf("after_remove_all=%zu\n", logcie_get_sink_count());
  return 0;
}
