/* $T is whatever the operating system calls the thread, so its value is not
   reproducible. What is: a thread keeps one id for its whole life, two threads
   never share one, and the id survives other threads coming and going. */
#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define SEEN_MAX 8

static char   seen[SEEN_MAX][32];
static size_t seen_len = 0;

static size_t collect_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)user_data;
  (void)log;

  while (len > 0 && bytes[len - 1] == '\n') {
    len--;
  }

  if (seen_len < SEEN_MAX) {
    snprintf(seen[seen_len], sizeof(seen[0]), "%.*s", (int)len, bytes);
    seen_len++;
  }

  return len;
}

static Logcie_Sink sink = {
  {logcie_token_formatter, (void *)"$T"},
  {collect_writer, NULL, NULL},
  {NULL, NULL},
};

static void *worker(void *arg) {
  (void)arg;

  LOGCIE_INFO("first");
  LOGCIE_INFO("second");
  return NULL;
}

int main(void) {
  logcie_remove_all_sinks();
  logcie_add_sink(&sink);

  LOGCIE_INFO("main");

  for (int i = 0; i < 2; i++) {
    pthread_t thread;

    if (pthread_create(&thread, NULL, worker, NULL) != 0) {
      return 1;
    }

    pthread_join(thread, NULL);
  }

  LOGCIE_INFO("main again");

  /* seen is: main, worker A twice, worker B twice, main. */
  printf("lines=%zu\n", seen_len);
  printf("main_is_stable=%d\n", strcmp(seen[0], seen[5]) == 0);
  printf("worker_a_is_stable=%d\n", strcmp(seen[1], seen[2]) == 0);
  printf("worker_b_is_stable=%d\n", strcmp(seen[3], seen[4]) == 0);
  printf("workers_differ=%d\n", strcmp(seen[1], seen[3]) != 0);
  printf("worker_differs_from_main=%d\n", strcmp(seen[0], seen[1]) != 0);
  printf("not_empty=%d\n", seen[0][0] != '\0');
  return 0;
}
