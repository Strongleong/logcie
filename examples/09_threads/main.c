// Thread safety is opt-in. Without LOGCIE_THREAD_SAFE, two threads logging at
// once will interleave their output or worse; with it, logcie_log takes a lock
// across the whole formatter, so a line is never split by another thread.
//
// It needs pthreads on POSIX, which is why this example carries a build.flags
// file with -lpthread in it.
//
// The lock covers logging, not reconfiguration: add and remove sinks before
// starting threads and after joining them, never in between.
#define LOGCIE_MODULE      "worker"
#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

#include <pthread.h>

#define THREADS 4
#define LINES   50

static void *worker(void *arg) {
  long id = (long)arg;

  for (int i = 0; i < LINES; i++) {
    LOGCIE_INFO("thread %ld, line %d", id, i);
  }

  return NULL;
}

int main(void) {
  pthread_t threads[THREADS];

  // A writer runs while the lock is held, so it needs no locking of its own
  logcie_get_default_sink()->formatter.data = "[$L] ($M) $m";

  for (long t = 0; t < THREADS; t++) {
    if (pthread_create(&threads[t], NULL, worker, (void *)t) != 0) {
      LOGCIE_FATAL("could not start thread %ld", t);
      return 1;
    }
  }

  for (int t = 0; t < THREADS; t++) {
    pthread_join(threads[t], NULL);
  }

  LOGCIE_INFO("%d threads wrote %d lines each, none of them torn", THREADS, LINES);
  return 0;
}
