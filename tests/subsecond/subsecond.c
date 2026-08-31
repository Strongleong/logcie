/* NOTE: the nanosecond value is not reproducible, so nothing here prints it.
 * What this prints is which tier the build selected and whether that tier
 * produced a sub-second value at all. test.tspec compiles this one file once
 * per tier and pins the pair. */
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#if defined(TIME_UTC) || defined(CLOCK_REALTIME)
#define SUBSECOND_EXPECTED 1
#else
#define SUBSECOND_EXPECTED 0
#endif

int main(void) {
  uint32_t seen_nonzero = 0;
  uint32_t in_range     = 1;
  uint32_t time_set     = 1;
  int      i;

  /* NOTE: a real clock is free to land on a whole second, so one sample cannot
   * prove sub-second resolution. Any one of many being non-zero can. */
  for (i = 0; i < 1000; i++) {
    Logcie_Log log = logcie_make_log("sub", LOGCIE_LEVEL_INFO, "x", "f.c", 1);

    if (log.nanos >= 1000000000u) {
      in_range = 0;
    }

    if (log.nanos != 0) {
      seen_nonzero = 1;
    }

    if (log.time == 0) {
      time_set = 0;
    }
  }

  /* NOTE: nanos must always be a legal fraction of a second, and the seconds
   * must be filled in whichever tier ran and whether or not its call
   * succeeded. Both hold on every platform. */
  printf("nanos_in_range=%u\n", in_range);
  printf("time_always_set=%u\n", time_set);

  /* NOTE: and a build with a sub-second clock has to produce sub-second
   * values, while a build without one has to leave nanos at zero rather than
   * hold junk. Which of the two applies is the platform's business, so the
   * fixture decides and reports a single verdict. */
  printf("matches_available_clock=%d\n", seen_nonzero == SUBSECOND_EXPECTED);

  return 0;
}
