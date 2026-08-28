/* NOTE: $d, $t and $z cannot be asserted from the outside because their value
 * depends on when the test runs. The fixture therefore computes what logcie
 * should have produced and compares, so the .tspec only has to assert a fixed
 * verdict line. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <string.h>
#include <time.h>

static char captured[256];

/* NOTE: logcie_token_formatter asserts writer->data is non-NULL even though
 * a custom writer need not use it, so the buffer is passed through it. */
static size_t capture_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)log;

  char  *out = (char *)user_data;
  size_t at  = strlen(out);

  /* NOTE: bytes is not NUL terminated, so the length has to come from len. */
  int written = snprintf(out + at, sizeof(captured) - at, "%.*s", (int)len, bytes);
  return written > 0 ? (size_t)written : 0;
}

static Logcie_Sink sink = {
  .formatter = {logcie_token_formatter, NULL},
  .writer    = {capture_writer, captured},
  .filter    = {NULL, NULL},
};

static const char *emit(const char *format) {
  captured[0]         = '\0';
  sink.formatter.data = (void *)format;

  LOGCIE_INFO("x");

  /* NOTE: the formatter always appends a newline; drop it before comparing. */
  size_t len = strlen(captured);

  if (len > 0 && captured[len - 1] == '\n') {
    captured[len - 1] = '\0';
  }

  return captured;
}

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  time_t    now   = time(NULL);
  struct tm local = *localtime(&now);

  char expected_date[32];
  char expected_time[32];

  strftime(expected_date, sizeof(expected_date), "%Y-%m-%d", &local);
  strftime(expected_time, sizeof(expected_time), "%H:%M:%S", &local);

  printf("date=%s\n", strcmp(emit("$d"), expected_date) == 0 ? "ok" : emit("$d"));
  printf("time=%s\n", strcmp(emit("$t"), expected_time) == 0 ? "ok" : emit("$t"));

  /* NOTE: the offset is whole hours with an explicit sign, e.g. "+0" in UTC. */
  const char *zone = emit("$z");
  printf("zone=%s\n", (zone[0] == '+' || zone[0] == '-') ? "ok" : zone);

  return 0;
}
