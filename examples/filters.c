#include <string.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

static Logcie_CombinedFilterContext ctx_and;
static Logcie_NotFilterContext ctx_not;

uint8_t min_info_filter(Logcie_Sink *sink, Logcie_Log *log) {
    (void)sink;
    return log->level >= LOGCIE_LEVEL_INFO;
}

uint8_t module_filter(Logcie_Sink *sink, Logcie_Log *log) {
    (void)sink;
    return log->module && strcmp(log->module, "module") == 0;
}

void module(void) {
    static const char *logcie_module = "module";
    LOGCIE_DEBUG("debug from module - should appear only in module logger");
    LOGCIE_INFO("info from module - should appear in every logger");
}

int main(void) {
    Logcie_Sink info_sink = {
        .min_level = LOGCIE_LEVEL_TRACE,
        .sink = stdout,
        .fmt = "$c$L (Info)$r $m",
        .formatter = logcie_printf_formatter,
        .filter = min_info_filter, // Use info filter
    };

    Logcie_Sink module_sink = {
        .min_level = LOGCIE_LEVEL_TRACE,
        .sink = stdout,
        .fmt = "$c$L (Module)$r $m",
        .formatter = logcie_printf_formatter,
        .filter = module_filter, // Use module filter
    };

    Logcie_Sink common_sink = {
        .min_level = LOGCIE_LEVEL_TRACE,
        .sink = stdout,
        .fmt = "$c$L (Common)$r $m",
        .formatter = logcie_printf_formatter,
    };

    logcie_set_filter_and(&common_sink, min_info_filter, module_filter, &ctx_and);

    logcie_add_sink(&info_sink);
    logcie_add_sink(&module_sink);
    logcie_add_sink(&common_sink);

    // Example logs
    LOGCIE_TRACE("this should NOT appear");
    LOGCIE_DEBUG("this should NOT appear either");
    LOGCIE_INFO("info log - should appear only in info logger");


    LOGCIE_INFO("you can set filters runtime");
    logcie_set_filter_not(&module_sink, min_info_filter, &ctx_not);
    LOGCIE_TRACE("now this should appear in module logger");

    module();

    LOGCIE_WARN("warning log - should also appear in info logger");
    return 0;
}
