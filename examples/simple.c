#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

int main(void) {
    // Simple logs
    LOGCIE_TRACE("This is a trace log");
    LOGCIE_DEBUG("Debugging value: %d", 42);
    LOGCIE_VERBOSE("Some verbose log");
    LOGCIE_INFO("Application started");
    LOGCIE_WARN("This is a warning");
    LOGCIE_ERROR("An error occurred: %s", "out of memory");
    LOGCIE_FATAL("Fatal error, aborting!");

    return 0;
}
