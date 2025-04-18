#include <logcie.h>

static const char *logcie_module = "module";

void submodule(void) {
  static const char *logcie_module = "submodule";
  LOGCIE_TRACE("Inside of submodule");
}

void module_stuff(void) {
  LOGCIE_DEBUG("Inside of module");
}
