#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// --

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef TSPEC_LOG
#ifdef LOGCIE
#ifdef LOGCIE_VA_LOGS
#define TSPEC_LOG(level, msg, ...) LOGCIE_LOG_MOD_VA("TSPEC", level, msg, __VA_ARGS__)
#else
#define TSPEC_LOG(level, ...) LOGCIE_LOG_MOD("TSPEC", level, __VA_ARGS__)
#endif
#else
#define TSPEC_LOG(level, ...)                 \
  do {                                        \
    fprintf(stdout, #level ": " __VA_ARGS__); \
    fprintf(stdout, "\n");                    \
  } while (0)
#endif
#endif

#define TSPEC_TRACE(...)   TSPEC_LOG(TRACE, __VA_ARGS__)
#define TSPEC_DEBUG(...)   TSPEC_LOG(DEBUG, __VA_ARGS__)
#define TSPEC_VERBOSE(...) TSPEC_LOG(VERBOSE, __VA_ARGS__)
#define TSPEC_INFO(...)    TSPEC_LOG(INFO, __VA_ARGS__)
#define TSPEC_WARN(...)    TSPEC_LOG(WARN, __VA_ARGS__)
#define TSPEC_ERROR(...)   TSPEC_LOG(ERROR, __VA_ARGS__)
#define TSPEC_FATAL(...)   TSPEC_LOG(FATAL, __VA_ARGS__)

#define TSPEC_MAX_BLOB_SIZE  8192
#define TSPEC_MAX_LINE_SIZE  512
#define TSPEC_MAX_ARG_SIZE   512
#define TSPEC_MAX_ASSERTIONS 16
#define TSPEC_MAX_COMMANDS   32
#define TSPEC_MAX_TESTS      16
#define TSPEC_MAX_ARGV       64

typedef struct {
  // NOTE: +1 at the end for \0 to keep data debug-printable
  uint8_t data[TSPEC_MAX_BLOB_SIZE + 1];
  size_t  size;
} TspecBlob;

typedef enum {
  TSPEC_ASSERTION_RETURN = 0,
  TSPEC_ASSERTION_STDOUT,
  TSPEC_ASSERTION_STDERR,
  TSPEC_ASSERTION_STDOUT_CONTAINS,
  TSPEC_ASSERTION_STDERR_CONTAINS,
  TspecAssertionType_Count,
} TspecAssertionType;

typedef struct {
  TspecAssertionType type;

  union {
    int32_t   int_value;
    TspecBlob blob_value;
  } value;
} TspecAssertion;

typedef struct {
  char name[256];

  TspecBlob executable;
  TspecBlob args;

  int32_t timeout_ms;

  TspecAssertion assertions[TSPEC_MAX_ASSERTIONS];
  size_t         assertion_count;
} TspecCommand;

typedef struct {
  char name[256];

  TspecCommand commands[TSPEC_MAX_COMMANDS];
  size_t       command_count;
} TspecTest;

typedef struct {
  TspecTest tests[TSPEC_MAX_TESTS];
  size_t    test_count;
} TspecFile;

typedef struct {
  int return_code;

  TspecBlob stdout_blob;
  TspecBlob stderr_blob;

  int timed_out;
} TspecExecResult;

typedef struct {
  FILE *file;
  char  line_buffer[TSPEC_MAX_LINE_SIZE];
  int   line_number;

  const char *file_path;
} TspecFileReader;

static int tspec_reader_open(TspecFileReader *reader, const char *path) {
  TSPEC_TRACE("tspec_reader_open(\"%s\")", path);
  assert(reader);

  memset(reader, 0, sizeof(*reader));
  reader->file_path = path;
  reader->file      = fopen(path, "rb");

  if (!reader->file) {
    TSPEC_ERROR("Failed to open '%s': %s", path, strerror(errno));
    return 0;
  }

  return 1;
}

static void tspec_reader_close(TspecFileReader *reader) {
  TSPEC_TRACE("tspec_reader_close()");
  assert(reader);
  assert(reader->file);

  if (reader->file) {
    fclose(reader->file);
    reader->file = NULL;
  }
}

static const char *tspec_reader_next_line(TspecFileReader *reader) {
  TSPEC_TRACE("tspec_reader_next_line()");
  assert(reader);
  assert(reader->file);

  if (!fgets(reader->line_buffer, sizeof(reader->line_buffer), reader->file)) {
    return NULL;
  }

  reader->line_number++;
  size_t len = strlen(reader->line_buffer);

  if (len > 0 && reader->line_buffer[len - 1] == '\n') {
    reader->line_buffer[len - 1] = '\0';
  }

  return reader->line_buffer;
}

static int tspec_reader_read_blob(TspecFileReader *reader, size_t size, TspecBlob *blob) {
  TSPEC_TRACE("tspec_reader_read_blob(\"%s\", %zu)", reader->line_buffer, size);

  assert(reader);
  assert(reader->file);
  assert(blob);

  if (size > TSPEC_MAX_BLOB_SIZE) {
    TSPEC_ERROR("%s:%d: Blob size %zu exceeds max", reader->file_path, reader->line_number, size);
    return 0;
  }

  if (fread(blob->data, 1, size, reader->file) != size) {
    TSPEC_ERROR("%s:%d: fread failed: %s", reader->file_path, reader->line_number, strerror(errno));
    return 0;
  }

  blob->data[size] = '\0';
  blob->size       = size;

  TSPEC_DEBUG("Parsed blob data (%zu bytes): '%s'", blob->size, blob->data);
  int c = fgetc(reader->file);

  if (c != '\n') {
    TSPEC_WARN("%s:%d: Blob is probably not fully parsed", reader->file_path, reader->line_number);
  }

  return 1;
}

static const char *tspec_skip_ws(const char *s) {
  TSPEC_TRACE("tspec_skip_ws(\"%s\")", s);

  while (*s == ' ' || *s == '\t') {
    s++;
  }

  return s;
}

static int tspec_parse_control_line(const char *line, const char *tag, const char **out) {
  TSPEC_TRACE("tspec_parse_control_line(\"%s\", \"%s\")", line, tag);

  line = tspec_skip_ws(line);

  if (*line != ':') {
    return 0;
  }

  line++;

  size_t tag_len = strlen(tag);

  if (strncmp(line, tag, tag_len) != 0) {
    return 0;
  }

  line += tag_len;

  if (*line != ' ') {
    return 0;
  }

  line++;

  if (out) {
    *out = line;
  }

  return 1;
}

static int tspec_parse_int(const char *s, int32_t *out) {
  TSPEC_TRACE("tspec_parse_int(\"%s\")", s);

  char *end   = NULL;
  long  value = strtol(s, &end, 10);

  if (end == s || *end != '\0') {
    return 0;
  }

  if (value < INT32_MIN || value > INT32_MAX) {
    return 0;
  }

  *out = (int32_t)value;

  return 1;
}

static size_t tspec_parse_size(const char *s) {
  TSPEC_TRACE("tspec_parse_size(\"%s\")", s);

  char *end   = NULL;
  long  value = strtol(s, &end, 10);

  if (end == s || *end != '\0' || value < 0) {
    return 0;
  }

  return (size_t)value;
}

static int tspec_parse_cmd_blob(TspecFileReader *reader, const char *line, TspecBlob *dest) {
  TSPEC_TRACE("tspec_parse_cmd_blob(\"%s\")", line);

  assert(reader);
  assert(reader->file);
  assert(dest);

  size_t size = tspec_parse_size(line);

  if (size == 0) {
    TSPEC_ERROR("%s:%d: Invalid blob size", reader->file_path, reader->line_number);
    return 0;
  }

  return tspec_reader_read_blob(reader, size, dest);
}

static int tspec_tokenize_args(const TspecBlob *blob, char argv[TSPEC_MAX_ARGV][TSPEC_MAX_ARG_SIZE], char **argv_ptrs) {
  TSPEC_TRACE("tspec_tokenize_args(\"%s\")", blob->data);
  assert(blob);

  const uint8_t *p   = blob->data;
  const uint8_t *end = blob->data + blob->size;

  int argc = 0;

  while (p < end && argc < TSPEC_MAX_ARGV - 1) {
    while (p < end && *p == ' ') {
      p++;
    }

    if (p == end) {
      break;
    }

    char   *out      = argv[argc];
    uint8_t in_quote = 0;

    argv_ptrs[argc++] = out;

    while (p < end) {
      if (*p == '"' && in_quote && p + 1 < end && p[1] == '"') {
        *out++ = '"';
        p += 2;
        continue;
      }

      if (*p == '"') {
        in_quote = !in_quote;
        p++;
        continue;
      }

      if (!in_quote && *p == ' ') {
        p++;
        break;
      }

      *out++ = *p++;
    }

    *out = '\0';
  }

  argv_ptrs[argc] = NULL;
  return argc;
}

static int tspec_chdir_to_spec(const char *spec_path) {
  size_t spec_path_len = strlen(spec_path);
  char   path_copy[spec_path_len + 1];
  strcpy(path_copy, spec_path);

  char *dir = dirname(path_copy);

  if (chdir(dir) != 0) {
    TSPEC_ERROR("chdir to '%s' failed: %s", dir, strerror(errno));
    return 0;
  }

  return 1;
}

static uint64_t tspec_time_ms(void) {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }

  return (uint64_t)ts.tv_sec * 1000u +
         (uint64_t)ts.tv_nsec / 1000000u;
}

static void tspec_set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);

  if (flags == -1) {
    return;
  }

  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void tspec_drain_fd(int fd, TspecBlob *blob, const char *name) {
  char tmp[4096];

  for (;;) {
    ssize_t n = read(fd, tmp, sizeof(tmp));

    if (n > 0) {
      size_t avail = TSPEC_MAX_BLOB_SIZE - blob->size;

      if (avail > 0) {
        size_t copy = (size_t)n;

        if (copy > avail) {
          copy = avail;
        }

        memcpy(blob->data + blob->size, tmp, copy);
        blob->size += copy;
        blob->data[blob->size] = '\0';

        if ((size_t)n > copy) {
          TSPEC_WARN("%s exceeded buffer, truncating", name);
        }
      }
    } else if (n == 0) {
      break;  // EOF
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;  // no more data available
    } else {
      TSPEC_ERROR("%s read error: %s", name, strerror(errno));
      break;
    }
  }
}

static TspecExecResult tspec_execute_command(const TspecCommand *cmd) {
  TSPEC_TRACE("tspec_execute_command(\"%s\")", cmd->executable.data);
  TSPEC_VERBOSE("Executing command: '%s%s%s'", cmd->executable.data, cmd->args.size > 1 ? " " : "", cmd->args.data);

  TspecExecResult result = {0};

  char *executable = (char *)cmd->executable.data;

  char *argv[TSPEC_MAX_ARGV];
  char  argv_storage[TSPEC_MAX_ARGV][TSPEC_MAX_ARG_SIZE];
  argv[0] = executable;

  int argc = tspec_tokenize_args(&cmd->args, argv_storage, argv + 1);

  if (argc < 0) {
    TSPEC_ERROR("Failed to tokenize args for command '%s'", cmd->name);
    result.return_code = -1;
    return result;
  }

  TSPEC_DEBUG("argc: %d", argc + 1);

  for (int i = 0; i <= argc; i++) {
    TSPEC_DEBUG("argv[%d]: %s", i, argv[i]);
  }

  int stdout_pipe[2];
  int stderr_pipe[2];

  if (pipe(stdout_pipe) == -1) {
    TSPEC_ERROR("pipe(stdout) failed for '%s': %s", cmd->name, strerror(errno));
    result.return_code = -1;
    return result;
  }

  if (pipe(stderr_pipe) == -1) {
    TSPEC_ERROR("pipe(stderr) failed for '%s': %s", cmd->name, strerror(errno));
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    result.return_code = -1;
    return result;
  }

  pid_t pid = fork();

  if (pid == -1) {
    TSPEC_ERROR("fork failed for '%s': %s", cmd->name, strerror(errno));
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    close(stderr_pipe[0]);
    close(stderr_pipe[1]);

    result.return_code = -1;
    return result;
  }

  if (pid == 0) {
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    setpgid(0, 0);

    execvp(executable, argv);
    _exit(127);
  } else {
    setpgid(pid, pid);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  tspec_set_nonblocking(stdout_pipe[0]);
  tspec_set_nonblocking(stderr_pipe[0]);

  int      status   = 0;
  uint64_t start_ms = tspec_time_ms();

  struct timespec ts;
  ts.tv_sec  = 0;
  ts.tv_nsec = 10000000L; /* 10 ms */

  TSPEC_DEBUG("Timeout is set to %dms", cmd->timeout_ms);

  while (1) {
    tspec_drain_fd(stdout_pipe[0], &result.stdout_blob, "stdout");
    tspec_drain_fd(stderr_pipe[0], &result.stderr_blob, "stderr");

    pid_t r = waitpid(pid, &status, WNOHANG);

    if (r == pid) {
      break;
    }

    if (r == -1) {
      TSPEC_ERROR("waitpid failed: %s", strerror(errno));
      result.return_code = -1;
      break;
    }

    uint64_t time_diff_ms = tspec_time_ms() - start_ms;

    if (cmd->timeout_ms > 0 && time_diff_ms >= (uint64_t)cmd->timeout_ms) {
      TSPEC_WARN("Process timed out after %zums", time_diff_ms);

      kill(-pid, SIGKILL);

      if (waitpid(pid, &status, 0) == -1) {
        TSPEC_ERROR("waitpid after kill failed: %s", strerror(errno));
      }

      result.timed_out   = 1;
      result.return_code = -SIGKILL;
      break;
    }

    nanosleep(&ts, NULL);
  }

  tspec_drain_fd(stdout_pipe[0], &result.stdout_blob, "stdout");
  tspec_drain_fd(stderr_pipe[0], &result.stderr_blob, "stderr");

  if (WIFEXITED(status)) {
    result.return_code = WEXITSTATUS(status);
    TSPEC_DEBUG("Process exited with code %d", result.return_code);
  } else if (WIFSIGNALED(status)) {
    int sig            = WTERMSIG(status);
    result.return_code = -sig;

    if (sig == SIGALRM) {
      result.timed_out = 1;
      TSPEC_WARN("Process timed out after %d seconds", cmd->timeout_ms);
    } else {
      TSPEC_WARN("Process killed by signal %d", sig);
    }
  } else {
    result.return_code = -1;
    TSPEC_ERROR("Process exited abnormally");
  }

  TSPEC_DEBUG("stdout (%zu bytes): %s", result.stdout_blob.size, result.stdout_blob.data);
  TSPEC_DEBUG("stderr (%zu bytes): %s", result.stderr_blob.size, result.stderr_blob.data);

  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  return result;
}

static int tspec_blob_equals(const TspecBlob *a, const TspecBlob *b) {
  TSPEC_TRACE("tspec_blob_equals(\"%s\", \"%s\")", a->data, b->data);

  if (a->size != b->size) {
    return 0;
  }

  return memcmp(a->data, b->data, a->size) == 0;
}

static int tspec_blob_contains(const TspecBlob *haystack, const TspecBlob *needle) {
  TSPEC_TRACE("tspec_blob_contains(\"%s\", \"%s\")", haystack->data, needle->data);

  if (needle->size > haystack->size) {
    return 0;
  }

  if (needle->size == 0) {
    return 1;
  }

  for (size_t i = 0; i <= haystack->size - needle->size; i++) {
    if (memcmp(haystack->data + i, needle->data, needle->size) == 0) {
      return 1;
    }
  }

  return 0;
}

static TspecAssertion *tspec_cmd_add_assertion(TspecCommand *cmd) {
  TSPEC_TRACE("tspec_cmd_add_assertion()");
  assert(cmd);

  if (cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
    TSPEC_ERROR("too many assertions in command '%s'", cmd->name);
    return NULL;
  }

  return &cmd->assertions[cmd->assertion_count++];
}

static int tspec_parse_blob_assertion(TspecFileReader *reader, TspecCommand *cmd, const char *line, TspecAssertionType type) {
  TSPEC_TRACE("tspec_parse_blob_assertion(\"%s\")", line);

  assert(TspecAssertionType_Count == 5);
  assert(reader);
  assert(reader->file);
  assert(cmd);

  TspecAssertion *a = tspec_cmd_add_assertion(cmd);

  if (!a) {
    return 0;
  }

  a->type     = type;
  size_t size = tspec_parse_size(line);

  if (size == 0) {
    TSPEC_ERROR("%s:%d: Invalid blob size for assertion", reader->file_path, reader->line_number);
    cmd->assertion_count--;
    return 0;
  }

  if (!tspec_reader_read_blob(reader, size, &a->value.blob_value)) {
    TSPEC_ERROR("%s:%d: Failed reading blob assertion", reader->file_path, reader->line_number);
    cmd->assertion_count--;
    return 0;
  }

  TSPEC_DEBUG("Blob assertion: %s", a->value.blob_value.data);
  return 1;
}

static int tspec_validate_assertions(const TspecCommand *cmd, const TspecExecResult *result) {
  TSPEC_TRACE("tspec_validate_assertions(\"%s\")", cmd->executable.data);

  assert(TspecAssertionType_Count == 5);

  for (size_t i = 0; i < cmd->assertion_count; i++) {
    const TspecAssertion *assertion = &cmd->assertions[i];

    switch (assertion->type) {
      case TSPEC_ASSERTION_RETURN: {
        if (result->return_code != assertion->value.int_value) {
          TSPEC_ERROR("expected return %d but got %d", assertion->value.int_value, result->return_code);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT: {
        if (!tspec_blob_equals(&result->stdout_blob, &assertion->value.blob_value)) {
          TSPEC_ERROR("stdout mismatch");
          TSPEC_DEBUG("expected (%zu bytes): '%s'", assertion->value.blob_value.size, assertion->value.blob_value.data);
          TSPEC_DEBUG("actual   (%zu bytes): '%s'", result->stdout_blob.size, result->stdout_blob.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR: {
        if (!tspec_blob_equals(&result->stderr_blob, &assertion->value.blob_value)) {
          TSPEC_ERROR("stderr mismatch");
          TSPEC_DEBUG("expected (%zu bytes): '%s'", assertion->value.blob_value.size, assertion->value.blob_value.data);
          TSPEC_DEBUG("actual   (%zu bytes): '%s'", result->stderr_blob.size, result->stderr_blob.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT_CONTAINS: {
        if (!tspec_blob_contains(&result->stdout_blob, &assertion->value.blob_value)) {
          TSPEC_ERROR("stdout missing substring: '%s'", assertion->value.blob_value.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR_CONTAINS: {
        if (!tspec_blob_contains(&result->stderr_blob, &assertion->value.blob_value)) {
          TSPEC_ERROR("stderr missing substring: '%s'", assertion->value.blob_value.data);
          return 0;
        }

        break;
      }

      default:
        break;
    }
  }

  return 1;
}

static int tspec_parse_file(const char *path, TspecFile *spec) {
  TSPEC_TRACE("tspec_parse_file(\"%s\")", path);
  TspecFileReader reader = {0};

  assert(TspecAssertionType_Count == 5);

  if (!tspec_reader_open(&reader, path)) {
    return 0;
  }

  memset(spec, 0, sizeof(*spec));

  TspecTest    *current_test = NULL;
  TspecCommand *current_cmd  = NULL;

  const char *line = NULL;

  while ((line = tspec_reader_next_line(&reader)) != NULL) {
    line = tspec_skip_ws(line);

    if (*line == '\0') {
      continue;
    }

    if (tspec_parse_control_line(line, "test", &line)) {
      if (*line == '\0') {
        TSPEC_ERROR("%s:%d: Empty test name", reader.file_path, reader.line_number);
        return 0;
      }

      TSPEC_VERBOSE("Found test: %s", line);

      if (spec->test_count >= TSPEC_MAX_TESTS) {
        TSPEC_ERROR("%s:%d: Too many tests", reader.file_path, reader.line_number);
        return 0;
      }

      current_test = &spec->tests[spec->test_count++];
      memset(current_test, 0, sizeof(*current_test));
      strncpy(current_test->name, line, sizeof(current_test->name) - 1);

      continue;
    }

    if (tspec_parse_control_line(line, "command", &line)) {
      if (*line == '\0') {
        TSPEC_ERROR("%s:%d: Empty command name", reader.file_path, reader.line_number);
        return 0;
      }

      TSPEC_VERBOSE("Found command: %s", line);

      if (!current_test) {
        TSPEC_ERROR("%s:%d: :command outside test", reader.file_path, reader.line_number);
        return 0;
      }

      if (current_test->command_count >= TSPEC_MAX_COMMANDS) {
        TSPEC_ERROR("%s:%d: Too many commands", reader.file_path, reader.line_number);
        return 0;
      }

      current_cmd = &current_test->commands[current_test->command_count++];
      memset(current_cmd, 0, sizeof(*current_cmd));
      strncpy(current_cmd->name, line, sizeof(current_cmd->name) - 1);

      continue;
    }

    if (!current_cmd) {
      continue;
    }

    if (tspec_parse_control_line(line, "blob executable", &line)) {
      if (!tspec_parse_cmd_blob(&reader, line, &current_cmd->executable)) {
        TSPEC_ERROR("%s:%d: Failed reading executable blob", reader.file_path, reader.line_number);
        return 0;
      }

      TSPEC_DEBUG("Found executable: %s", current_cmd->executable.data);
      continue;
    }

    if (tspec_parse_control_line(line, "blob args", &line)) {
      if (!tspec_parse_cmd_blob(&reader, line, &current_cmd->args)) {
        TSPEC_ERROR("%s:%d: Failed reading args blob", reader.file_path, reader.line_number);
        return 0;
      }

      TSPEC_DEBUG("Found args: %s", current_cmd->args.data);
      continue;
    }

    if (tspec_parse_control_line(line, "int timeout_ms", &line)) {
      if (!tspec_parse_int(line, &current_cmd->timeout_ms)) {
        TSPEC_ERROR("%s:%d: Invalid timeout", reader.file_path, reader.line_number);
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int return", &line)) {
      TspecAssertion *a = tspec_cmd_add_assertion(current_cmd);

      if (!a) {
        return 0;
      }

      a->type = TSPEC_ASSERTION_RETURN;

      if (!tspec_parse_int(line, &a->value.int_value)) {
        TSPEC_ERROR("%s:%d: Invalid return value", reader.file_path, reader.line_number);
        current_cmd->assertion_count--;
        return 0;
      }

      TSPEC_DEBUG("Expected return: %d", a->value.int_value);
      continue;
    }

    if (tspec_parse_control_line(line, "blob stdout_contains", &line)) {
      if (!tspec_parse_blob_assertion(&reader, current_cmd, line, TSPEC_ASSERTION_STDOUT_CONTAINS)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stderr_contains", &line)) {
      if (!tspec_parse_blob_assertion(&reader, current_cmd, line, TSPEC_ASSERTION_STDERR_CONTAINS)) {
        return 0;
      }

      continue;
    }

    TSPEC_ERROR("%s:%d: Unknown directive '%s'", reader.file_path, reader.line_number, reader.line_buffer);
    return 0;
  }

  tspec_reader_close(&reader);
  return 1;
}

static int tspec_run_file(const TspecFile *spec) {
  TSPEC_INFO("Running %zu test%s", spec->test_count, spec->test_count == 1 ? "" : "s");

  for (size_t i = 0; i < spec->test_count; i++) {
    const TspecTest *test = &spec->tests[i];
    TSPEC_INFO("[%zu/%zu] test: %s (%zu command%s)", i + 1, spec->test_count, test->name, test->command_count, test->command_count == 1 ? "" : "s");

    for (size_t j = 0; j < test->command_count; j++) {
      const TspecCommand *cmd = &test->commands[j];
      TSPEC_INFO("  [%zu/%zu] command: %s", j + 1, test->command_count, cmd->name);

      TspecExecResult result = tspec_execute_command(cmd);

      if (result.timed_out || !tspec_validate_assertions(cmd, &result)) {
        TSPEC_ERROR("Test %s failed at command %s", test->name, cmd->name);
        return 0;
      }

      TSPEC_VERBOSE("Command %s passed", cmd->name);
    }

    TSPEC_VERBOSE("Test %s passed", test->name);
  }

  TSPEC_INFO("All done!");
  return 1;
}

static TspecFile spec;

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tspec>\n", argv[0]);
    return 1;
  }

  if (!tspec_parse_file(argv[1], &spec)) {
    return 1;
  }

  if (!tspec_chdir_to_spec(argv[1])) {
    return 1;
  }

  if (!tspec_run_file(&spec)) {
    return 1;
  }

  fflush(stderr);

  return 0;
}
