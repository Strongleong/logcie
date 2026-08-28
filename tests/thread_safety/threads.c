/* NOTE: LOGCIE_THREAD_SAFE only does anything under contention, so this is the
 * one fixture that needs real threads. The guarantee under test is that a line
 * from one thread is never interleaved with another's: logcie_log holds the
 * lock across the whole formatter, so every line must arrive whole.
 *
 * NOTE: the writer runs while the lock is held, so it needs no lock of its own. */
#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <pthread.h>
#include <string.h>

#define THREADS          4
#define LINES_PER_THREAD 250
#define TOTAL            (THREADS * LINES_PER_THREAD)

static char   collected[TOTAL * 64];
static size_t collected_len = 0;
static size_t writer_calls  = 0;

static size_t collecting_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;

  writer_calls++;

  if (collected_len + len < sizeof(collected)) {
    memcpy(collected + collected_len, bytes, len);
    collected_len += len;
  }

  return len;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$M:$m"},
  {collecting_writer, NULL},
  {NULL, NULL},
};

static void *worker(void *arg) {
  long id = (long)arg;

  for (int i = 0; i < LINES_PER_THREAD; i++) {
    LOGCIE_LOG_MOD("worker", INFO, "thread=%ld line=%d", id, i);
  }

  return NULL;
}

int main(void) {
  pthread_t threads[THREADS];

  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  for (long t = 0; t < THREADS; t++) {
    if (pthread_create(&threads[t], NULL, worker, (void *)t) != 0) {
      return 1;
    }
  }

  for (int t = 0; t < THREADS; t++) {
    pthread_join(threads[t], NULL);
  }

  /* NOTE: a torn line shows up as one that does not match the shape, which is
   * exactly what interleaving would produce. */
  size_t lines  = 0;
  size_t intact = 0;
  char  *line   = collected;

  collected[collected_len] = '\0';

  while (*line) {
    char *end = strchr(line, '\n');

    if (!end) {
      break;
    }

    *end = '\0';
    lines++;

    long id = -1;
    int  n  = -1;

    if (sscanf(line, "worker:thread=%ld line=%d", &id, &n) == 2 &&
        id >= 0 && id < THREADS && n >= 0 && n < LINES_PER_THREAD) {
      intact++;
    }

    line = end + 1;
  }

  printf("writer_calls=%zu\n", writer_calls);
  printf("lines=%zu\n", lines);
  printf("intact=%zu\n", intact);
  printf("all_accounted_for=%d\n", writer_calls == TOTAL && lines == TOTAL && intact == TOTAL);
  return 0;
}
