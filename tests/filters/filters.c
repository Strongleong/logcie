#define LOGCIE_IMPLEMENTATION
#include "logcie.h"
#include <string.h>

static Logcie_Sink sink = {
  .formatter = {logcie_printf_formatter, (void *)"$L/$M/$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

/* NOTE: file-scope compound literals so the filter data outlives the call. */
static Logcie_Filter min_warn      = logcie_filter_level_min(LOGCIE_LEVEL_WARN);
static Logcie_Filter max_info      = logcie_filter_level_max(LOGCIE_LEVEL_INFO);
static Logcie_Filter only_net      = logcie_filter_module_eq("net");
static Logcie_Filter has_needle    = logcie_filter_message_contains("needle");
static Logcie_Filter warn_and_net  = logcie_filter_and(logcie_filter_level_min(LOGCIE_LEVEL_WARN), logcie_filter_module_eq("net"));
static Logcie_Filter warn_or_net   = logcie_filter_or(logcie_filter_level_min(LOGCIE_LEVEL_WARN), logcie_filter_module_eq("net"));
static Logcie_Filter not_net       = logcie_filter_not(only_net);

int main(int argc, char **argv) {
  if (argc != 2) {
    return 2;
  }

  if (strcmp(argv[1], "none") == 0)               sink.filter = (Logcie_Filter){NULL, NULL};
  else if (strcmp(argv[1], "min_warn") == 0)      sink.filter = min_warn;
  else if (strcmp(argv[1], "max_info") == 0)      sink.filter = max_info;
  else if (strcmp(argv[1], "module_eq") == 0)     sink.filter = only_net;
  else if (strcmp(argv[1], "contains") == 0)      sink.filter = has_needle;
  else if (strcmp(argv[1], "and") == 0)           sink.filter = warn_and_net;
  else if (strcmp(argv[1], "or") == 0)            sink.filter = warn_or_net;
  else if (strcmp(argv[1], "not") == 0)           sink.filter = not_net;
  else return 2;

  sink.writer.data = stdout;
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_LOG_MOD("core", INFO,  "plain");
  LOGCIE_LOG_MOD("net",  INFO,  "needle here");
  LOGCIE_LOG_MOD("core", ERROR, "boom");
  LOGCIE_LOG_MOD("net",  ERROR, "net boom");
  return 0;
}
