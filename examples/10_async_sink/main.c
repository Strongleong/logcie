#define _POSIX_C_SOURCE 200809L

// Logcie has no worker threads of its own. If a sink is slow -- a socket, a
// disk that stalls -- give that sink a thread and let logcie_log return as soon
// as the line is copied.
//
// The pieces logcie provides are enough on their own:
//
//   - a writer is handed one complete line per call, never a fragment, so the
//     queue can store whole lines and nothing has to be reassembled
//   - log.msg is the finished message by the time a writer sees it
//   - writer.flush is called by logcie_flush(), which is where the queue drains
//
// Copy this file, replace slow_write with whatever you actually write to, and
// the rest is yours to tune.

#define _GNU_SOURCE

#define LOGCIE_MODULE "app"
#define LOGCIE_THREAD_SAFE
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

#include <pthread.h>
#include <string.h>
#include <time.h>

#define QUEUE_SLOTS 64
#define LINE_MAX    256

typedef struct {
  char   lines[QUEUE_SLOTS][LINE_MAX];
  size_t len[QUEUE_SLOTS];
  size_t head;
  size_t count;
  size_t dropped;

  pthread_mutex_t lock;
  pthread_cond_t  filled;
  pthread_cond_t  drained;
  pthread_t       thread;
  int             running;
} Async_Sink;

// Stands in for the slow thing: a socket, a stalling disk.
static void slow_write(const char *bytes, size_t len) {
  struct timespec pause = {0, 2 * 1000 * 1000};

  nanosleep(&pause, NULL);
  fwrite(bytes, 1, len, stdout);
}

static void *async_sink_main(void *arg) {
  Async_Sink *queue = (Async_Sink *)arg;

  for (;;) {
    pthread_mutex_lock(&queue->lock);

    while (queue->running && queue->count == 0) {
      pthread_cond_signal(&queue->drained);
      pthread_cond_wait(&queue->filled, &queue->lock);
    }

    if (queue->count == 0) {
      pthread_mutex_unlock(&queue->lock);
      break;
    }

    // Copied out under the lock. Holding a pointer into the ring across the
    // unlock would be a use-after-overwrite.
    char   line[LINE_MAX];
    size_t len = queue->len[queue->head];

    memcpy(line, queue->lines[queue->head], len);
    queue->head = (queue->head + 1) % QUEUE_SLOTS;
    queue->count--;
    pthread_mutex_unlock(&queue->lock);

    slow_write(line, len);

    pthread_mutex_lock(&queue->lock);

    if (queue->count == 0) {
      pthread_cond_signal(&queue->drained);
    }

    pthread_mutex_unlock(&queue->lock);
  }

  return NULL;
}

// The writer. It copies and returns; it never touches the socket.
static size_t async_sink_write(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)log;

  Async_Sink *queue = (Async_Sink *)user_data;

  if (len > LINE_MAX) {
    len = LINE_MAX;
  }

  pthread_mutex_lock(&queue->lock);

  if (queue->count == QUEUE_SLOTS) {
    // Full. Drop the oldest and count it, so a burst costs old lines rather
    // than blocking the thread that is trying to log.
    queue->head = (queue->head + 1) % QUEUE_SLOTS;
    queue->count--;
    queue->dropped++;
  }

  size_t tail = (queue->head + queue->count) % QUEUE_SLOTS;

  memcpy(queue->lines[tail], bytes, len);
  queue->len[tail] = len;
  queue->count++;

  pthread_cond_signal(&queue->filled);
  pthread_mutex_unlock(&queue->lock);
  return len;
}

// The flush. logcie_flush() calls this, so waiting here is what makes
// logcie_flush() mean "everything is written".
static void async_sink_flush(void *user_data) {
  Async_Sink *queue = (Async_Sink *)user_data;

  pthread_mutex_lock(&queue->lock);

  while (queue->count > 0) {
    pthread_cond_signal(&queue->filled);
    pthread_cond_wait(&queue->drained, &queue->lock);
  }

  pthread_mutex_unlock(&queue->lock);
  fflush(stdout);
}

static void async_sink_start(Async_Sink *queue) {
  memset(queue, 0, sizeof(*queue));
  queue->running = 1;
  pthread_mutex_init(&queue->lock, NULL);
  pthread_cond_init(&queue->filled, NULL);
  pthread_cond_init(&queue->drained, NULL);
  pthread_create(&queue->thread, NULL, async_sink_main, queue);
}

static void async_sink_stop(Async_Sink *queue) {
  pthread_mutex_lock(&queue->lock);
  queue->running = 0;
  pthread_cond_signal(&queue->filled);
  pthread_mutex_unlock(&queue->lock);

  pthread_join(queue->thread, NULL);
  pthread_cond_destroy(&queue->drained);
  pthread_cond_destroy(&queue->filled);
  pthread_mutex_destroy(&queue->lock);
}

static Async_Sink queue;

static Logcie_Sink slow_sink = {
  .formatter = {logcie_token_formatter, "[$L] ($M:$T) $m"},
  .writer    = {async_sink_write, async_sink_flush, &queue},
  .filter    = {NULL, NULL},
};

int main(void) {
  async_sink_start(&queue);

  logcie_remove_all_sinks();
  logcie_add_sink(&slow_sink);

  for (int i = 0; i < 20; i++) {
    LOGCIE_INFO("line %d", i);
  }

  // Returns as soon as the queue drains, not when each line was written.
  logcie_flush();
  LOGCIE_INFO("%zu line(s) dropped", queue.dropped);

  // Remove the sink before the queue it points at goes away.
  logcie_flush();
  logcie_remove_sink(&slow_sink);
  async_sink_stop(&queue);
  return 0;
}
