/* NOTE: logcie_log holds logcie_mutex across the whole sink loop, writers
 * included, and the mutex is not recursive. A writer calling logcie_flush is
 * asking for a lock its own thread already holds. Threads contending for that
 * lock from outside is a different case, covered by concurrent_flush.c, and it
 * passes whether or not the re-entrancy guard exists.
 *
 * NOTE: a deadlock produces no return code to assert on, so this fixture arms
 * an alarm and reports the hang itself. Without the guard it prints
 * "deadlocked" and exits 1. */
#define _POSIX_C_SOURCE 200809L

#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <signal.h>
#include <unistd.h>

static int writes  = 0;
static int flushes = 0;

static void on_alarm(int sig) {
  (void)sig;

  static const char msg[] = "deadlocked\n";
  ssize_t           n     = write(STDOUT_FILENO, msg, sizeof(msg) - 1);

  (void)n;
  _exit(1);
}

static void counting_flush(void *user_data) {
  (void)user_data;
  flushes++;
}

static size_t flushing_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;
  (void)bytes;

  writes++;
  logcie_flush();
  return len;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$m"},
  {flushing_writer, counting_flush, NULL},
  {NULL, NULL},
};

int main(void) {
  signal(SIGALRM, on_alarm);
  alarm(5);

  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("below the autoflush level");
  printf("after_info: writes=%d flushes=%d\n", writes, flushes);

  /* At ERROR the autoflush path calls the flusher as well, so the writer's own
   * logcie_flush re-enters a lock that is about to be used again. */
  LOGCIE_ERROR("at the autoflush level");
  printf("after_error: writes=%d flushes=%d\n", writes, flushes);

  alarm(0);
  return 0;
}
