// Include it and log. There is nothing to set up: Logcie starts with one sink
// already installed, printing to stdout.
//
// LOGCIE_IMPLEMENTATION pulls in the function bodies. Define it in exactly one
// translation unit; every other file just includes the header.
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

int main(void) {
  LOGCIE_TRACE("This is a trace log");
  LOGCIE_DEBUG("Debugging value: %d", 42);
  LOGCIE_VERBOSE("Some verbose log");
  LOGCIE_INFO("Application started");
  LOGCIE_WARN("This is a warning");
  LOGCIE_ERROR("An error occurred: %s", "out of memory");
  LOGCIE_FATAL("Fatal error, aborting!");

  return 0;
}
