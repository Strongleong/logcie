// A log line is formatted into a buffer on the stack. Lines that fit cost no
// allocation at all, which is the overwhelmingly common case.
//
// A line that does not fit is rendered again into an exactly sized buffer, so
// it arrives whole rather than clipped. That second buffer is the only thing
// Logcie ever allocates, and you decide where it comes from.

// Deliberately small so the fallback is easy to trigger. 1024 by default
#define LOGCIE_MAX_LINE 128

// Where the rare long line comes from. Define both or neither.
// Define LOGCIE_NO_MALLOC instead to forbid allocation altogether, in which
// case a long line is truncated to LOGCIE_MAX_LINE. That is the setting for a
// target where malloc is banned outright
#define LOGCIE_MALLOC(size) counted_alloc(size)
#define LOGCIE_FREE(ptr)    counted_free(ptr)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int allocations = 0;

static void *counted_alloc(size_t size) {
  allocations++;
  return malloc(size);
}

static void counted_free(void *ptr) {
  free(ptr);
}

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

int main(void) {
  logcie_get_default_sink()->formatter.data = "[$L] $m";

  LOGCIE_INFO("a normal line");
  printf("allocations so far: %d\n", allocations);

  // Longer than LOGCIE_MAX_LINE, so this one needs the allocator
  static char big[400];
  memset(big, 'x', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';

  LOGCIE_INFO("%s", big);
  printf("allocations after a long line: %d\n", allocations);

  return 0;
}
