// A sink is where a log ends up, and it is three things:
//
//   formatter  turns the log into bytes    (here: $ tokens -> text)
//   writer     puts those bytes somewhere  (here: a FILE *)
//   filter     decides if it belongs here  (here: none, so everything passes)
//
// Every registered sink sees every log, so a log can go to several places at
// once in different formats. Logcie does not own your sink: the struct must
// outlive its registration, which is why sinks usually live at file scope or in
// main.
#include <stdio.h>

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

int main(void) {
  FILE *logfile = fopen("out.log", "w");

  if (!logfile) {
    return 1;
  }

  Logcie_Sink file_sink = {
    .formatter = {logcie_token_formatter, "$d $t [$L] ($M) $m"},
    .writer    = {logcie_file_writer, logcie_file_flush, logfile},
    .filter    = {NULL, NULL}
  };

  logcie_add_sink(&file_sink);

  // Both sinks get every log: the built-in one to stdout, this one to the file.
  // Adding a sink does not displace the built-in one.
  LOGCIE_INFO("Starting application");
  LOGCIE_WARN("Low disk space");

  // Formats can change at run time
  file_sink.formatter.data = "$f:$x [$L] $m";
  LOGCIE_INFO("New format");

  // The built-in sink is an ordinary sink. Remove it and the console goes quiet
  logcie_remove_sink(logcie_get_default_sink());
  LOGCIE_INFO("This one only reaches the file");

  fclose(logfile);
  return 0;
}
