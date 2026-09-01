/* NOTE: a line that fits LOGCIE_MAX_LINE never touches the allocator. A longer
 * one is rendered a second time into an exact-sized buffer, so it arrives
 * whole. Either way the writer is called exactly once per line -- that is what
 * lets a sink treat a call as a record. */
#define LOGCIE_MAX_LINE 128
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <string.h>

static size_t calls    = 0;
static size_t last_len = 0;
static char   tail[8];

static size_t counting_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;

  calls++;
  last_len = len;

  size_t from = len > 4 ? len - 4 : 0;
  memcpy(tail, bytes + from, len - from);
  tail[len - from] = '\0';
  return len;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$m"},
  {counting_writer, logcie_file_flush, NULL},
  {NULL, NULL},
};

int main(void) {
  static char big[400];
  memset(big, 'x', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';

  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("short");
  printf("short_calls=%zu short_len=%zu\n", calls, last_len);

  calls = 0;
  LOGCIE_INFO("%s", big);
  printf("long_calls=%zu long_len=%zu wanted=%zu tail=%s\n",
         calls, last_len, strlen(big) + 1, tail);
  return 0;
}
