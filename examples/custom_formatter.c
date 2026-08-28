#include <stdio.h>
#include <time.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

size_t my_simple_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args) {
  (void)user_data;

  size_t     time_buf_len = 9;
  char       time_buf[time_buf_len];
  struct tm *tminfo = localtime(&log.time);
  strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tminfo);

  const char *level  = get_logcie_level_label_upper(log.level);
  const char *module = log.module ? log.module : "none";

  size_t output_len = snprintf(NULL, 0, "[%s] [%s] (%s) ", time_buf, level, module) + 1;
  char   output[output_len];
  snprintf(output, output_len, "[%s] [%s] (%s) ", time_buf, level, module);

  va_list args_copy;
  va_copy(args_copy, *args);

  size_t msg_len = vsnprintf(NULL, 0, log.msg, args_copy) + 1;
  char   msg[msg_len];
  va_copy(args_copy, *args);
  vsnprintf(msg, msg_len, log.msg, args_copy);

  writer->write(writer->data, output, output_len);
  writer->write(writer->data, msg, msg_len);
  writer->write(writer->data, "\n", 1);

  return 0;
}

int main(void) {
  Logcie_Sink my_sink = {
    .formatter = {my_simple_formatter, NULL},
    .writer    = {logcie_file_writer, stdout},
    .filter    = logcie_filter_level_min(LOGCIE_LEVEL_TRACE)
  };

  logcie_add_sink(&my_sink);

  LOGCIE_INFO("Hello %s!", "World");
  LOGCIE_WARN("Something seems wrong: code %d", 42);

  return 0;
}
