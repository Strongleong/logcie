#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <string.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

// Imagine if this is backend for web application, ok, yes?

typedef struct User {
  uint32_t    id;
  const char *name;
  uint8_t     is_invisible;
} User;

static User normal_user = (User){
    .id           = 1,
    .name         = "John",
    .is_invisible = 0,
};

static User invisible_user = (User){
    .id           = 2,
    .name         = "Dave",
    .is_invisible = 1,
};

static User *current_user;

// Set module name for this file
const char *logcie_module = "main";

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

uint8_t file_filter(Logcie_Sink *sink, Logcie_Log *log) {
  return filter_important_only(sink, log) && filter_exclude_file(sink, log);
}

uint8_t console_filter(Logcie_Sink *sink, Logcie_Log *log) {
  (void) sink; (void) log;
  return current_user ? !current_user->is_invisible : 1;
}

int main() {
  FILE *logfile = fopen("app.log", "w");

  Logcie_Sink file_sink = (Logcie_Sink){
      .sink      = logfile,
      .min_level = LOGCIE_LEVEL_VERBOSE,
      .fmt       = "$d $t $f:$x [$M::$L] $m",
      .formatter = logcie_printf_formatter,
      .filter    = file_filter,
  };

  logcie_add_sink(&file_sink);

  // Create and add a filtered console sink (stack allocated)
  Logcie_Sink console_sink = {
      .sink      = stdout,
      .min_level = LOGCIE_LEVEL_INFO,
      .fmt       = "$c[$L]$r $M $t - $m",
      .formatter = logcie_printf_formatter,
      .filter    = console_filter,
  };

  logcie_add_sink(&console_sink);

  // Log some messages
  LOGCIE_INFO("Application starting");
  LOGCIE_VERBOSE("Initializing subsystems");
  LOGCIE_WARN("This is an important warning about memory");
  LOGCIE_DEBUG("Active sinks: %zu", logcie_get_sink_count());

  // A another user logs in
  current_user = &normal_user;
  LOGCIE_INFO("User %s(%u) logged in", current_user->name, current_user->id);

  // Another user logs in
  current_user = &invisible_user;
  LOGCIE_INFO("User %s(%u) logged in", current_user->name, current_user->id);

  // Remove file sink and clean up
  logcie_remove_sink_and_close(&file_sink);

  // Remove console sink (stack-allocated, no free needed)
  logcie_remove_sink(&console_sink);

  // Remove all sinks (back to default only)
  logcie_remove_all_sinks();

  LOGCIE_INFO("Shutdown");

  return 0;
}
