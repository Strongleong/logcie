#include <logcie.h>
#include <stdio.h>

static const char *logcie_module = "main";

void module_stuff(void);

void yet_another_module(void) {
  static const char *logcie_module = "yet_another_module";
  LOGCIE_TRACE("you can have as many as you want");
}

int main(void) {
  Logcie_Sink stdout_sink = {
    .sink = stdout,
    .level = LOGCIE_LEVEL_INFO,
    .formatter = logcie_printf_formatter,
    .color = true,
    .fmt = "$L $f:$x: [$M] $m"
  };

  // TODO: Think how to make automatic hadling of file handlers
  FILE *out_log = fopen("out.log", "w");
  Logcie_Sink file_sink = {
    .sink = out_log,
    .level = LOGCIE_LEVEL_TRACE,
    .formatter = logcie_printf_formatter,
    .color = false,
    .fmt = "$f:$x:$L [$M] $d $t (GMT $z) $m"
  };

  logcie_add_sink(stdout_sink);
  logcie_add_sink(file_sink);

  LOGCIE_TRACE("Too much log levels");
  LOGCIE_DEBUG("Can debug a lot");
  LOGCIE_VERBOSE("Format strings are %%s %s", "supported");
  LOGCIE_INFO("Colored ouput");

  stdout_sink.fmt = "$L $f:$x: [$M] (updated output format in runtime) $m";

  module_stuff();
  yet_another_module();

  LOGCIE_WARN("warnny loggy %d %s", 4, "asd");
  LOGCIE_ERROR("errorry loggy %d", 5);
  LOGCIE_FATAL("fatallyly loggy %d", 6);

  fclose(out_log);

  return 0;
}
