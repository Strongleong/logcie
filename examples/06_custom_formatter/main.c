// A formatter turns a log into bytes:
//
//   size_t my_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args);
//
// It owns the serialization, so the $ token language belongs to
// logcie_token_formatter rather than to Logcie. This one emits JSON and ignores
// tokens entirely; user_data is whatever a given formatter wants, so it is
// unused here.
//
// The one rule: call writer->write once, with the whole line.
#include <stdio.h>
#include <stdlib.h>

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

static size_t json_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args) {
  (void)user_data;

  // Rendering log.msg means running it through vsnprintf with the arguments
  // that reached the macro. logcie_render_message does that, and copies the
  // va_list so it can be called again on a bigger buffer.
  //
  // Like snprintf it returns the length it wanted, so render into a buffer you
  // already have and only look further if it did not fit. Sizing first with
  // (NULL, 0) also works but always costs two passes.
  char   stack_msg[128];
  char  *msg    = stack_msg;
  char  *heap   = NULL;
  size_t needed = logcie_render_message(stack_msg, sizeof(stack_msg), &log, args);

  if (needed >= sizeof(stack_msg)) {
    heap = (char *)malloc(needed + 1);

    if (heap) {
      logcie_render_message(heap, needed + 1, &log, args);
      msg = heap;
    }
  }

  char line[512];
  int  n = snprintf(line, sizeof(line),
                    "{\"level\":\"%s\",\"module\":\"%s\",\"file\":\"%s\",\"line\":%u,\"msg\":\"%s\"}\n",
                    get_logcie_level_label(log.level),
                    log.module ? log.module : "",
                    log.location.file,
                    log.location.line,
                    msg);

  if (n < 0) {
    return 0;
  }

  size_t len = (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1;

  // One call, one line. A writer may treat a call as one record.
  writer->write(writer->data, &log, line, len);

  free(heap);
  return len;
}

static Logcie_Sink json_sink = {
  .formatter = {json_formatter, NULL},
  .writer    = {logcie_file_writer, NULL},
  .filter    = {NULL, NULL}
};

int main(void) {
  json_sink.writer.data = stdout;

  logcie_remove_sink(logcie_get_default_sink());
  logcie_add_sink(&json_sink);

  LOGCIE_INFO("Starting");
  LOGCIE_WARN("Disk at %d%%", 91);

  return 0;
}
