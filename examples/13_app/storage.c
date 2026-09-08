#define LOGCIE_MODULE "app.storage"
#include <logcie.h>

#include "app.h"

static int open_files = 0;

int storage_open(const char *path) {
  LOGCIE_DEBUG("opening %s", path);

  open_files++;
  LOGCIE_INFO("storage ready (%d open)", open_files);
  return 0;
}

void storage_write(const char *key, size_t bytes) {
  LOGCIE_TRACE("write key=%s bytes=%zu", key, bytes);

  if (bytes > 1024) {
    LOGCIE_WARN("large record: %s is %zu bytes", key, bytes);
  }
}

void storage_close(void) {
  open_files--;
  LOGCIE_INFO("storage closed");
}
