#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

// Remove this line too see the crash :)
#define LOGCIE_THREAD_SAFE

#define LOGCIE_MODULE "main"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

#define NUM_THREADS     4

static volatile int running = 1;

// Creating a shit ton of logs form multiple threads
static void *worker(void *arg) {
  int id = *(int *)arg;

  for (int i = 0; running; i++) {
    LOGCIE_INFO_MOD("worker", "thread %d message %d", id, i);
  }

  return NULL;
}

static size_t null_writer(void *user_data, const char *fmt, va_list *va, ...) {
  (void)user_data;
  (void)fmt;
  (void)va;
  return 0;
}

// Constantly creates a sink, registers it, then immediatly frees it.
// Without LOGCIE_THREAD_SAFE a worker thread may still be inside the
// sink when free() is called - use-after-free
static void *destroyer(void *arg) {
  (void)arg;

  while (running) {
    Logcie_Sink *sink = malloc(sizeof(*sink));

    if (!sink) {
      continue;
    }

    *sink = (Logcie_Sink){
      .formatter = {logcie_printf_formatter, "[$L] $M: $m"},
      .writer    = {null_writer, (void *)1},
      .filter    = logcie_filter_level_min(LOGCIE_LEVEL_INFO)
    };

    logcie_add_sink(sink);

    // Give workers a chance to hit the sink
    sleep(0);
    logcie_remove_sink(sink);

    // Freed while still potentially in use
    free(sink);
  }

  return NULL;
}

int main(void) {
  Logcie_Sink stdout_sink = {
    .formatter = {logcie_printf_formatter, "[$c$L$r] $M: $m"},
    .writer    = {logcie_printf_writer, stdout},
    .filter    = logcie_filter_level_min(LOGCIE_LEVEL_INFO)
  };

  logcie_add_sink(&stdout_sink);

  pthread_t threads[NUM_THREADS];
  int       ids[NUM_THREADS];

  pthread_t destroyer_thread;
  pthread_create(&destroyer_thread, NULL, destroyer, NULL);

  for (int i = 0; i < NUM_THREADS; i++) {
    ids[i] = i;
    pthread_create(&threads[i], NULL, worker, &ids[i]);
  }

  // Let the workers and the destroyer fight for two seconds
  sleep(2);
  running = 0;

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  pthread_join(destroyer_thread, NULL);
  LOGCIE_INFO("Finished. If you saw a crash or garbled output, thread safety is mandatory");
  return 0;
}
