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
static const char *logcie_module = "main";

uint8_t console_filter(const void *data, Logcie_Log *log) {
  (void)log;
  User *user = (User *)data;
  return user ? !user->is_invisible : 1;
}

// Custom filter: exclude debug messages from a specific file
uint8_t filter_exclude_file(const void *data, Logcie_Log *log) {
  (void) data;
  return strstr(log->location.file, "noisy.c") == NULL;
}

int main() {
  FILE *logfile = fopen("app.log", "w");

  Logcie_Sink file_sink = (Logcie_Sink){
    .formatter = {logcie_printf_formatter, "$d $t $f:$x [$M::$L] $m"},
    .writer    = {logcie_printf_writer, logfile},
    // Filter our logs with level less that verbose
    // OR
    // every "importand" log, except from that one file
    .filter    = logcie_filter_or(
      logcie_filter_message_contains("important"),
      logcie_filter_and(
        logcie_filter_level_min(LOGCIE_LEVEL_VERBOSE),
        ((Logcie_Filter){
          .filter = filter_exclude_file,
          .data = NULL,
        })
      )
    )
  };

  logcie_add_sink(&file_sink);

  // Create and add a filtered console sink (stack allocated)
  Logcie_Sink console_sink = {
    .formatter = {logcie_printf_formatter, "$c[$L]$r $t - $m"},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_and(
      logcie_filter_level_min(LOGCIE_LEVEL_INFO),
      ((Logcie_Filter){console_filter, current_user})
    )
  };

  logcie_add_sink(&console_sink);

  // Log some messages
  LOGCIE_INFO("Application starting");
  LOGCIE_VERBOSE("Initializing subsystems");
  LOGCIE_DEBUG("Active sinks: %zu", logcie_get_sink_count());

  // A another user logs in
  current_user = &normal_user;
  LOGCIE_INFO("User %s(%u) logged in", current_user->name, current_user->id);

  // Another user logs in
  current_user = &invisible_user;
  LOGCIE_INFO("User %s(%u) logged in", current_user->name, current_user->id);

  // Remove file sink and clean up
  logcie_remove_sink(&file_sink);

  fclose(logfile);

  // Remove console sink (stack-allocated, no free needed)
  logcie_remove_sink(&console_sink);

  // Remove all sinks (back to default only)
  logcie_remove_all_sinks();

  LOGCIE_INFO("Shutdown");

  return 0;
}
