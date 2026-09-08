#define LOGCIE_MODULE "app.api"
#include <logcie.h>

#include "app.h"

void api_serve(int worker, int requests) {
  for (int i = 0; i < requests; i++) {
    LOGCIE_DEBUG("worker %d: request %d received", worker, i);

    storage_write("session", (size_t)i * 700);

    if (i == 2) {
      LOGCIE_ERROR("worker %d: request %d failed: %s", worker, i, "upstream timeout");
      continue;
    }

    LOGCIE_TRACE("worker %d: request %d served", worker, i);
  }
}
