#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

int main(void) {
    const char *my_colors[Count_LOGCIE_LEVEL] = {
        [LOGCIE_LEVEL_TRACE]   = "\x1b[90m",  // Gray
        [LOGCIE_LEVEL_DEBUG]   = "\x1b[94m",  // Light blue
        [LOGCIE_LEVEL_VERBOSE] = "\x1b[92m",  // Light green
        [LOGCIE_LEVEL_INFO]    = "\x1b[1;32m",// Bright green (bold)
        [LOGCIE_LEVEL_WARN]    = "\x1b[33m",  // Yellow
        [LOGCIE_LEVEL_ERROR]   = "\x1b[1;33m",// Bright yellow (bold)
        [LOGCIE_LEVEL_FATAL]   = "\x1b[1;31m",// Bright red (bold)
    };

    logcie_set_colors(my_colors);

    LOGCIE_INFO("This is a bright green info message!");
    LOGCIE_ERROR("This is a bold yellow error!");
    LOGCIE_FATAL("This is a bright red fatal error!");

    // Reset colors
    logcie_set_colors(NULL);

    LOGCIE_INFO("Now this a cyan color info");
    LOGCIE_ERROR("Error nw is red");
    LOGCIE_FATAL("Still red and bold");

    return 0;
}
