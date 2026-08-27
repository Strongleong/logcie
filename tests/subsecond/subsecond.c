/* NOTE: the nanosecond value is not reproducible, so nothing here prints it.
 * What this prints is which tier the build selected and whether that tier
 * produced a sub-second value at all. test.tspec compiles this one file once
 * per tier and pins the pair. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#if defined(TIME_UTC)
#define TIER "timespec_get"
#elif defined(CLOCK_REALTIME)
#define TIER "clock_gettime"
#else
#define TIER "time"
#endif

int main(void) {
  uint32_t   seen_nonzero = 0;
  Logcie_Log log;
  int        i;

  printf("tier=%s\n", TIER);

  /* NOTE: a real clock is free to land on a whole second, so one sample cannot
   * prove sub-second resolution. Any one of many being non-zero can. */
  for (i = 0; i < 1000; i++) {
    log = logcie_make_log("sub", LOGCIE_LEVEL_INFO, "x", "f.c", 1);

    if (log.nanos >= 1000000000u) {
      printf("nanos_out_of_range=1\n");
      return 1;
    }

    if (log.nanos != 0) {
      seen_nonzero = 1;
    }
  }

  printf("subsecond=%u\n", seen_nonzero);

  /* NOTE: whichever tier ran, and whether its clock call succeeded or not, the
   * fallback has to leave a usable wall-clock timestamp rather than the epoch. */
  printf("time_set=%d\n", log.time > 1700000000);

  return 0;
}
