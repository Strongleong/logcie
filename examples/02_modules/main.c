// A module tags a log with where it came from. It is the mechanism that lets
// the user of a library decide what to do with that library's logs without the
// library knowing anything about it: tag your logs, and whoever links you picks
// the routing.
//
// LOGCIE_MODULE sets the tag for every classic macro in the file. It is read at
// the call site, so it must be defined before including the header, and each
// file can choose its own.
#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

void net_connect(void);

int main(void) {
  // Show the module with $M
  logcie_get_default_sink()->formatter.data = "[$L] ($M) $m";

  LOGCIE_INFO("Starting");

  net_connect();

  // Or name the module per call
  LOGCIE_LOG_MOD("db", INFO, "Connected to %s", "postgres");

  return 0;
}
