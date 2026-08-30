#define LOGCIE_MODULE "app.api"
#include <logcie.h>

#include "app.h"

void api_serve(int requests) {
  for (int i = 0; i < requests; i++) {
    LOGCIE_DEBUG("request %d received", i);

    storage_write("session", (size_t)i * 700);

    if (i == 2) {
      LOGCIE_ERROR("request %d failed: %s", i, "upstream timeout");
      continue;
    }

    LOGCIE_TRACE("request %d served", i);
  }
}
