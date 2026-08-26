#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

static Logcie_Sink sink = {
  .formatter = {logcie_printf_formatter, (void *)"$c$L$r $m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

static const char *custom_colors[Count_LOGCIE_LEVEL] = {
  "<trace>", "<debug>", "<verb>", "<info>", "<warn>", "<error>", "<fatal>",
};

int main(void) {
  sink.writer.data = stdout;

  /* NOTE: index 0 is the built-in stdout sink before anything is added. */
  printf("default_sink_exists=%d\n", logcie_get_sink(0) != NULL);
  printf("out_of_range_is_null=%d\n", logcie_get_sink(99) == NULL);

  logcie_add_sink(&sink);
  printf("get_sink_returns_ours=%d\n", logcie_get_sink(0) == &sink);

  logcie_set_colors(custom_colors);
  LOGCIE_INFO("custom");

  logcie_set_colors(NULL);
  LOGCIE_INFO("restored");

  printf("remove_by_index=%d\n", logcie_remove_sink_by_index(0));
  printf("count_after=%zu\n", logcie_get_sink_count());
  return 0;
}
