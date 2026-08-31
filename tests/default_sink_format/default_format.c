/* NOTE: LOGCIE_DEFAULT_SINK_FORMAT was declared but never read -- the sink
 * hardcoded a literal that had already drifted from it. Nothing caught that,
 * so this pins the macro to the output. */
#define LOGCIE_DEFAULT_SINK_FORMAT "custom:$L:$m"
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

int main(void) {
  logcie_get_default_sink()->writer.data = stdout;
  LOGCIE_INFO("hello");
  return 0;
}
