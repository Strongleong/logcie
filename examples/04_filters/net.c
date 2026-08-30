#define LOGCIE_MODULE "net.http"
#include <logcie.h>

void net_work(void);

void net_work(void) {
  LOGCIE_DEBUG("kept: net.* passes at any level");
  LOGCIE_TRACE("kept too");
}
