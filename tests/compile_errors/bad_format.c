#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

int main(void) {
  LOGCIE_INFO("%d", "not an int");
  return 0;
}
