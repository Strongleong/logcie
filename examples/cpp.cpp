#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

class TimeoutData {
 public:
  uint64_t timeout_ms = 0;
  uint64_t last_time  = 0;
};

int main() {
  Logcie_Sink sink = {
    .formatter = {logcie_printf_formatter, (void *)("[$M::$c$L$r] $m")},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_and(
      (Logcie_Filter{
        .filter = [](const void *data, Logcie_Log *log) -> uint8_t {
          TimeoutData *d = (TimeoutData*)(data);

          if (log->time - d->last_time > d->timeout_ms) {
            d->last_time = log->time;
            return true;
          }

          return false;
        },
        .data = new TimeoutData(),
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
