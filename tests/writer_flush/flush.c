#define LOGCIE_IMPLEMENTATION
#include <string.h>

#include "logcie.h"

static char    buffer[64]    = {0};
static uint8_t buffer_filled = 0;
static uint8_t writes        = 0;
static uint8_t flushes       = 0;

static size_t buffered_writer_flush(void *user_data) {
  (void)user_data;

  flushes++;
  size_t res = 0;
  buffer[63] = '\0';

  res += snprintf(NULL, 0, "%s", buffer);
  res += fflush(NULL);

  buffer_filled = 0;
  return res;
}

// NOTE: printf is still buffered, but we have another buffer here just to actually see flusher working
static size_t buffered_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)log;
  writes++;

  if (buffer_filled + len > 63) {
    buffered_writer_flush(user_data);
  }

  memcpy(buffer + buffer_filled, bytes, len);
  buffer_filled += len;
  return len;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$m"},
  {buffered_writer, buffered_writer_flush, NULL},
  {NULL, NULL},
};

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("Writing log that not that long enough");
  LOGCIE_INFO("A little bit more");
  LOGCIE_INFO("A little bit more x2");

  printf("before: flushes: %d\n", flushes);
  printf("before: writes: %d\n", writes);

  LOGCIE_INFO("Writing more but not enought");

  printf("more: flushes: %d\n", flushes);
  printf("more: writes: %d\n", writes);

  logcie_flush();

  printf("after: flushes: %d\n", flushes);
  printf("after: writes: %d\n", writes);

  LOGCIE_ERROR("Still enough space in buffer but it should autoflush");

  printf("autoflush: flushes: %d\n", flushes);
  printf("autoflush: writes: %d\n", writes);

  return 0;
}
