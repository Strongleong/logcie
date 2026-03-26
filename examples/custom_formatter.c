#include <stdio.h>
#include <time.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

size_t my_simple_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args) {
  (void)user_data;

  char       time_buf[9];
  struct tm *tminfo = localtime(&log.time);
  strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tminfo);

  writer->write(writer->data, "[%s] [%s] (%s) ", NULL, time_buf, get_logcie_level_label_upper(log.level), log.module ? log.module : "none");
  writer->write(writer->data, log.msg, args);
  writer->write(writer->data, "\n", NULL);

  return 0;
}

int main(void) {
  Logcie_Sink my_sink = {
    .formatter = {my_simple_formatter, NULL},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_level_min(LOGCIE_LEVEL_TRACE)
  };

  logcie_add_sink(&my_sink);

  static const char *logcie_module = "MainModule";
  LOGCIE_INFO("Hello %s!", "World");
  LOGCIE_WARN("Something seems wrong: code %d", 42);

  return 0;
}
