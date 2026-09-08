/* NOTE: the writer receives the log alongside the bytes so it can route on
 * metadata instead of parsing it back out of the text. This is the shape a
 * syslog or Android writer needs: severity as a value, not as characters. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <string.h>

static size_t to_stdout = 0;
static size_t to_stderr = 0;
static char   seen_module[32];
static char   seen_msg[64];

static size_t routing_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)bytes;

  if (log->level >= LOGCIE_LEVEL_WARN) {
    to_stderr++;
  } else {
    to_stdout++;
  }

  snprintf(seen_module, sizeof(seen_module), "%s", log->module ? log->module : "");

  /* NOTE: log->msg is the message text. logcie_log resolves the varargs before
   * any sink runs, so a writer never sees a conversion specifier. */
  snprintf(seen_msg, sizeof(seen_msg), "%s", log->msg);
  return len;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$m"},
  {routing_writer, logcie_file_flush, NULL},
  {NULL, NULL},
};

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_LOG_MOD("net", DEBUG, "quiet");
  LOGCIE_LOG_MOD("net", INFO, "also quiet");
  LOGCIE_LOG_MOD("core", WARN, "loud");
  LOGCIE_LOG_MOD("core", ERROR, "value=%d", 42);

  printf("stdout_bound=%zu\n", to_stdout);
  printf("stderr_bound=%zu\n", to_stderr);
  printf("last_module=%s\n", seen_module);
  printf("last_msg=%s\n", seen_msg);
  printf("last_msg_is_rendered=%d\n", strcmp(seen_msg, "value=42") == 0);
  return 0;
}
