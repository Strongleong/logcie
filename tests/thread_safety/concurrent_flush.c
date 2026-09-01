/* NOTE: logcie_flush takes the same mutex logcie_log does, and it reads the
 * sink array while other threads are logging through it. This fixture exists
 * to put that under ThreadSanitizer.
 *
 * NOTE: the flusher is called both from logcie_flush and, at ERROR and above,
 * from inside logcie_log. Both paths hold the lock, so the counter it bumps
 * needs no atomics -- and if that ever stops being true, TSan says so here. */
#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <pthread.h>

#define THREADS          4
#define LINES_PER_THREAD 250
#define TOTAL            (THREADS * LINES_PER_THREAD)

static size_t writer_calls = 0;
static size_t flush_calls  = 0;

static size_t counting_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;
  (void)bytes;

  writer_calls++;
  return len;
}

static void counting_flush(void *user_data) {
  (void)user_data;
  flush_calls++;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$M:$m"},
  {counting_writer, counting_flush, NULL},
  {NULL, NULL},
};

static void *logger(void *arg) {
  long id = (long)arg;

  for (int i = 0; i < LINES_PER_THREAD; i++) {
    LOGCIE_LOG_MOD("worker", INFO, "thread=%ld line=%d", id, i);
  }

  return NULL;
}

static void *flusher(void *arg) {
  (void)arg;

  for (int i = 0; i < LINES_PER_THREAD; i++) {
    logcie_flush();
  }

  return NULL;
}

int main(void) {
  pthread_t loggers[THREADS];
  pthread_t flushers[THREADS];

  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  for (long t = 0; t < THREADS; t++) {
    if (pthread_create(&loggers[t], NULL, logger, (void *)t) != 0) {
      return 1;
    }

    if (pthread_create(&flushers[t], NULL, flusher, NULL) != 0) {
      return 1;
    }
  }

  for (int t = 0; t < THREADS; t++) {
    pthread_join(loggers[t], NULL);
    pthread_join(flushers[t], NULL);
  }

  /* NOTE: flush_calls is not asserted exactly. How many flushes land is a
   * matter of scheduling; that every line arrived and nothing raced is not. */
  printf("writer_calls=%zu\n", writer_calls);
  printf("flushed_at_least_once=%d\n", flush_calls > 0);
  printf("all_accounted_for=%d\n", writer_calls == TOTAL);
  return 0;
}
