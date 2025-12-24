#include <stdio.h>
#include <string.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

// Set module name for this file
static const char *logcie_module = "main";

// Custom filter: only log messages containing "important"
uint8_t filter_important_only(Logcie_Sink *sink, Logcie_Log *log) {
  (void)sink;  // Unused parameter
  return strstr(log->msg, "important") != NULL;
}

// Custom filter: exclude debug messages from a specific file
uint8_t filter_exclude_file(Logcie_Sink *sink, Logcie_Log *log) {
  (void)sink;  // Unused parameter
  return strstr(log->location.file, "noisy.c") == NULL;
}

int main() {
  FILE *logfile = fopen("app.log", "w");

  Logcie_Sink file_sink = (Logcie_Sink){.sink = logfile, .min_level = LOGCIE_LEVEL_INFO, .fmt = "$d $t [$M::$L] $m\n", .formatter = logcie_printf_formatter, .filter = NULL, .userdata = NULL};

  logcie_add_sink(&file_sink);

  // Create and add a filtered console sink (stack allocated)
  Logcie_Sink console_sink = {.sink = stdout, .min_level = LOGCIE_LEVEL_VERBOSE, .fmt = "$c[$L]$r $t - $m\n", .formatter = logcie_printf_formatter, .filter = NULL, .userdata = NULL};

  logcie_add_sink(&console_sink);

  // Create filter contexts
  static Logcie_CombinedFilterContext and_ctx;

  // Apply combined filter to console sink
  logcie_set_filter_and(&console_sink, filter_important_only, filter_exclude_file, &and_ctx);

  // Log some messages
  LOGCIE_INFO("Application starting");
  LOGCIE_VERBOSE("Initializing subsystems");
  LOGCIE_WARN("This is an important warning about memory");
  LOGCIE_DEBUG("Debug data: x=%d, y=%d", 10, 20);

  // Check sink count
  printf("Active sinks: %zu\n", logcie_get_sink_count());

  // Get and inspect first user-added sink
  Logcie_Sink *first_sink = logcie_get_sink(1);  // Index 0 is default stdout
  if (first_sink) {
    printf("First user sink min_level: %d\n", first_sink->min_level);
  }

  // Remove file sink and clean up
  logcie_remove_sink_and_close(&file_sink);

  // Remove console sink (stack-allocated, no free needed)
  logcie_remove_sink(&console_sink);

  // Remove all sinks (back to default only)
  logcie_remove_all_sinks();

  LOGCIE_INFO("Back to default sink configuration");

  return 0;
}
