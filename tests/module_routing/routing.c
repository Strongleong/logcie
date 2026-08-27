/* NOTE: the scenario this exists for -- an app that wants its own logs at
 * DEBUG, one noisy library silenced, and everything else at INFO -- using
 * nothing but the built-in filters. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <string.h>

static Logcie_Sink sink = {
  .formatter = {logcie_printf_formatter, (void *)"$M/$L/$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

/* NOTE: file scope, so the compound literals inside these live as long as the
 * sink does. Building them inside a setup function would leave the sink
 * holding pointers into a dead stack frame. */
static Logcie_Filter under_net     = logcie_filter_module_prefix_eq("net");
static Logcie_Filter not_under_net = logcie_filter_not(logcie_filter_module_prefix_eq("net"));

static void emit_everything(void) {
  LOGCIE_LOG_MOD("net",          INFO, "root");
  LOGCIE_LOG_MOD("net.http",     INFO, "child");
  LOGCIE_LOG_MOD("net.http.tls", INFO, "grandchild");
  LOGCIE_LOG_MOD("network",      INFO, "not a child");
  LOGCIE_LOG_MOD("app",          INFO, "unrelated");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    return 2;
  }

  if (strcmp(argv[1], "under") == 0) {
    sink.filter = under_net;
  } else if (strcmp(argv[1], "not_under") == 0) {
    sink.filter = not_under_net;
  } else if (strcmp(argv[1], "empty_prefix") == 0) {
    sink.filter = logcie_filter_module_prefix_eq("");
  } else {
    return 2;
  }

  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  emit_everything();
  return 0;
}
