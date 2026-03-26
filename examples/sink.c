#include <stdio.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

static const char *logcie_module = "main";

int main(void) {
  // Open a file for logs
  FILE *logfile = fopen("out.log", "w");

  if (!logfile) {
    logfile = stdout;
  }

  Logcie_Sink file_sink = {
    .writer = {
      .write = logcie_printf_writer,
      .data  = logfile,
    },
    // nice format: date, time, level, module, message
    .formatter = {logcie_printf_formatter, "$d $t [$L] ($M) $m"},
    .filter    = logcie_filter_level_min(LOGCIE_LEVEL_DEBUG)
  };

  logcie_add_sink(&file_sink);

  LOGCIE_INFO("Starting application");
  LOGCIE_WARN("Warning: low disk space");
  LOGCIE_ERROR("Error: can't save file");

  // Changing logs format in run time
  file_sink.formatter.data = "$f:$x [$L] ($M) $m";

  LOGCIE_INFO("New format");

  if (logfile)
    fclose(logfile);

  return 0;
}
