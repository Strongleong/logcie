#include <stdio.h>
#include <time.h>

#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

size_t my_simple_formatter(Logcie_Sink *sink, Logcie_Log log, va_list *args) {
    char time_buf[9];
    struct tm *tminfo = localtime(&log.time);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tminfo);

    fprintf(sink->sink, "[%s] [%s] (%s) ", time_buf, get_logcie_level_label_upper(log.level), log.module ? log.module : "none");

    vfprintf(sink->sink, log.msg, *args);
    fprintf(sink->sink, "\n");

    return 0;
}

int main(void) {
    Logcie_Sink my_sink = {
        .sink = stdout,
        .min_level = LOGCIE_LEVEL_TRACE,
        .formatter = my_simple_formatter,
        .fmt = NULL, // Not used by custom formatter
    };

    logcie_add_sink(&my_sink);

    logcie_module = "MainModule";
    LOGCIE_INFO("Hello %s!", "World");
    LOGCIE_WARN("Something seems wrong: code %d", 42);

    return 0;
}
