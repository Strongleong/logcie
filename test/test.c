#define LOGCIE_IMPLEMENTATION
#define LOGCIE_VA_LOGS
#include <logcie.h>

static const char *logcie_module = "main";

/* void module_stuff(void); */

int main(void) {
  LOGCIE_TRACE("tracely loggy");
  LOGCIE_DEBUG("debugguy loggy");
  LOGCIE_VERBOSE_VA("verbosesy loggy %d", 2);
  LOGCIE_INFO_VA("infofofo loggy %d", 3);

  /* module_stuff(); */

  LOGCIE_WARN_VA("warnny loggy %d", 4);
  LOGCIE_ERROR_VA("errorry loggy %d", 5);
  LOGCIE_FATAL_VA("fatallyly loggy %d", 6);

  return 0;
}
