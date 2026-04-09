#include <iso646.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

uint8_t filter_exclude_noisy(void *data, Logcie_Log *log) {
  (void)data;
  return std::strcmp(log->location.file, "noisy.c") != 0;
}

class TimeoutData {
 public:
  uint64_t timeout_ms;
  uint64_t last_time;
};

bool filter_timeout(void *data, Logcie_Log &log) {
  TimeoutData *d = static_cast<TimeoutData *>(data);

  if (log.time - d->last_time > d->timeout_ms) {
    d->last_time = log.time;
    return true;
  }

  return false;
}

int main() {
  Logcie_Sink sink = {
    .formatter = {logcie_printf_formatter, (void *)("[$M::$c$L$r] $m")},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_and(
      (Logcie_Filter{
        .filter = [](const void *data, Logcie_Log *log) -> uint8_t {
          (void)data;
          (void)log;
          return true;
        },
        .data = NULL,
      }),
      logcie_filter_or(
        logcie_filter_level_min(LOGCIE_LEVEL_INFO),
        logcie_filter_message_contains("IMPORTANT")
      )
    ),
  };

  logcie_add_sink(&sink);

  LOGCIE_INFO("Application starting");
  LOGCIE_VERBOSE("Initializing subsystems");
  LOGCIE_WARN("Warning: you are cool!");
  LOGCIE_DEBUG("Very IMPORTANT: active sinks: %zu", logcie_get_sink_count());

  return 0;
}
