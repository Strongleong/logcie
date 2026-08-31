// A different file, a different module. Module names are hierarchical, so
// "net.http" belongs to "net" and both can be filtered together later.
#define LOGCIE_MODULE "net.http"
#include <logcie.h>

void net_connect(void);

void net_connect(void) {
  LOGCIE_INFO("GET /index.html");
  LOGCIE_WARN("Retrying, attempt %d", 2);
}
