#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ctime>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

uint8_t filter_exclude_noisy(void *data, Logcie_Log *log) {
  (void) data;
  return std::strcmp(log->location.file, "noisy.c") != 0;
}

class TimeoutData {
public:
  uint64_t timeout_ms;
  uint64_t last_time;
};

bool filter_timeout(void *data, Logcie_Log &log) {
  TimeoutData *d = static_cast<TimeoutData*>(data);

  if (log.time - d->last_time > d->timeout_ms) {
    d->last_time = log.time;
    return true;
  }

  return false;
}

int main() {
  Logcie_Sink console = {
    .formatter = {logcie_printf_formatter, (void*)("[$M::$c$L$r] $m")},
    .writer = {logcie_printf_writer, stdout},
    .filter = logcie_filter_or(
      logcie_filter_level_min(LOGCIE_LEVEL_INFO),
      logcie_filter_message_contains("IMPORTANT")
    )
  };

  logcie_add_sink(&console);

  LOGCIE_INFO("Application starting");
  LOGCIE_VERBOSE("Initializing subsystems");
  LOGCIE_WARN("Warning: you are cool!");
  LOGCIE_DEBUG("Very IMPORTANT: active sinks: %zu", logcie_get_sink_count());

  return 0;
}
