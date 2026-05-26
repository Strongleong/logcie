#include <stdint.h>
#include <time.h>

#define LOGCIE_MODULE "core"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

void network_module() {
  static const char *module = "network";
  LOGCIE_TRACE_MOD(module, "Network module started");
  LOGCIE_ERROR_MOD(module, "Uselss SSL error");
  LOGCIE_WARN_MOD(module, "Can not do safe connection, doing unsafe");
  LOGCIE_VERBOSE_MOD(module, "Redirecting");
  LOGCIE_INFO_MOD(module, "Connection is good");
}

void auth_module() {
  static const char *module = "auth";

  LOGCIE_TRACE_MOD(module, "Auth module started");
  LOGCIE_VERBOSE_MOD(module, "Authintificating...");
  LOGCIE_TRACE_MOD(module, "Calling very CRITICAL network_module");

  network_module();

  LOGCIE_DEBUG_MOD(module, "Network module finished");
  LOGCIE_DEBUG_MOD(module, "User is %s", "authentificated");
}

uint8_t filter_work_hours(Logcie_Log *log) {
  struct tm *tm = localtime(&log->time);
  return tm->tm_hour >= 8 && tm->tm_hour < 18;
}

typedef struct TimeoutFilterData {
  uint64_t timeout_ms;
  uint64_t last_time;
} TimeoutFilterData;

uint8_t filter_timout(const void *data, Logcie_Log *log) {
  TimeoutFilterData *d = (TimeoutFilterData *)data;

  if (log->time - d->last_time >= d->timeout_ms) {
    d->last_time = log->time;
    return 1;
  }

  return 0;
}

int main(void) {
  Logcie_Sink prod_console = {
    .formatter = {logcie_printf_formatter, "$c$L$r $m"},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_or(
      logcie_filter_level_min(LOGCIE_LEVEL_WARN),
      logcie_filter_message_contains("CRITICAL")
    )
  };

  Logcie_Sink faulty_module_supressor = {
    .formatter = {logcie_printf_formatter, "$L [$M] $m"},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_or(
      logcie_filter_not(logcie_filter_module_eq("network")),
      logcie_filter_level_min(LOGCIE_LEVEL_ERROR)
    )
  };

  Logcie_Sink debug_specific_module = {
    .formatter = {logcie_printf_formatter, LOGCIE_COLOR_GRAY "$f:$x$r [$M:$c$L$r]  $m"},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_or(
      logcie_filter_and(
        logcie_filter_module_eq("auth"),
        logcie_filter_level_min(LOGCIE_LEVEL_DEBUG)
      ),
      logcie_filter_level_min(LOGCIE_LEVEL_INFO)
    )
  };

  Logcie_Sink max_one_log_per_second = {
    .formatter = {logcie_printf_formatter, LOGCIE_COLOR_GRAY "(throttled) $f:$x$r [$M:$c$L$r]  $m"},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = {filter_timout, &(TimeoutFilterData){.last_time = 0, .timeout_ms = 1000}}
  };

  logcie_add_sink(&prod_console);
  logcie_add_sink(&faulty_module_supressor);
  logcie_add_sink(&debug_specific_module);
  logcie_add_sink(&max_one_log_per_second);

  LOGCIE_TRACE("Logcie inited");
  LOGCIE_INFO("App started");
  LOGCIE_TRACE("Starting auth module");

  auth_module();

  LOGCIE_DEBUG("Auth module finihsed");
  LOGCIE_INFO("Working good");
  LOGCIE_INFO("Shutdown");

  return 0;
}
