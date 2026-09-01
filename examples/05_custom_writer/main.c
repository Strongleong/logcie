// A writer puts one finished line somewhere:
//
//   size_t my_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len);
//
// Three things make this contract worth knowing:
//
//   - one call is one complete line, newline included, so a sink that treats a
//     call as a record -- syslog, a socket -- is safe
//   - the log comes along, so a transport can use level or module as a value
//     instead of parsing them back out of the text
//   - log->msg is the format string from the call site, not the text. The
//     rendered line is bytes, and it is not NUL terminated, so use len
#include <stdio.h>

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

// Warnings and worse go to stderr, everything else to stdout.
static size_t console_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;

  FILE *out = log->level >= LOGCIE_LEVEL_WARN ? stderr : stdout;
  return fwrite(bytes, 1, len, out);
}

static Logcie_Sink console = {
  .formatter = {logcie_token_formatter, "[$L] $m"},
  .writer    = {console_writer, logcie_file_flush, NULL},
  .filter    = {NULL, NULL}
};

int main(void) {
  logcie_remove_sink(logcie_get_default_sink());
  logcie_add_sink(&console);

  LOGCIE_INFO("goes to stdout");
  LOGCIE_ERROR("goes to stderr");

  // A writer with no destination discards. Cheapest way to mute a sink.
  console.writer.write = logcie_file_writer;
  console.writer.data  = NULL;
  LOGCIE_INFO("goes nowhere");

  return 0;
}
