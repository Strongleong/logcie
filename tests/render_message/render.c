/* NOTE: logcie_render_message is the sanctioned way for a custom formatter to
 * get the message text, so its snprintf-like contract needs pinning: it
 * reports the length it wanted even when truncating, and (NULL, 0) sizes
 * without writing anywhere. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <string.h>

static size_t probe(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args) {
  (void)writer;
  (void)user_data;

  printf("sizing=%zu\n", logcie_render_message(NULL, 0, &log, args));

  char small[8];
  printf("truncated_wants=%zu\n", logcie_render_message(small, sizeof(small), &log, args));
  printf("truncated_text=%s\n", small);

  char full[64];
  printf("full_wants=%zu\n", logcie_render_message(full, sizeof(full), &log, args));
  printf("full_text=%s\n", full);

  /* NOTE: repeated calls must agree, which is what proves the va_list is being
   * copied rather than consumed. */
  printf("repeatable=%d\n", logcie_render_message(NULL, 0, &log, args) == strlen(full));
  return 0;
}

static Logcie_Sink sink = {{probe, NULL}, {logcie_file_writer, NULL}, {NULL, NULL}};

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("value=%d text=%s", 42, "hello");
  return 0;
}
