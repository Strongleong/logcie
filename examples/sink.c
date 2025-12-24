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

    Logcie_Sink file_sink = {0};
    file_sink.sink = logfile;
    file_sink.min_level = LOGCIE_LEVEL_DEBUG;
    file_sink.fmt = "$d $t [$L] ($M) $m"; // nice format: date, time, level, module, message
    file_sink.formatter = logcie_printf_formatter;
    logcie_add_sink(&file_sink);

    LOGCIE_INFO("Starting application");
    LOGCIE_WARN("Warning: low disk space");
    LOGCIE_ERROR("Error: can't save file");

    // Changing logs format in run time
    file_sink.fmt = "$f:$x [$L] ($M) $m";

    LOGCIE_INFO("New format");

    if (logfile) fclose(logfile);

    return 0;
}
