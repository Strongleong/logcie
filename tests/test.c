#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define TSPEC_TRACE(...)  //                           TSPEC_LOG(TRACE, __VA_ARGS__)
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
  uint8_t data[TSPEC_MAX_BLOB_SIZE];
  size_t  size;
} TspecBlob;

typedef enum {
  TSPEC_ASSERTION_RETURN = 0,
  TSPEC_ASSERTION_STDOUT,
  TSPEC_ASSERTION_STDERR,
  TSPEC_ASSERTION_STDOUT_CONTAINS,
  TSPEC_ASSERTION_STDERR_CONTAINS,
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
} TspecFileReader;

static int tspec_reader_open(TspecFileReader *reader, const char *path) {
  TSPEC_TRACE("tspec_reader_open");
  if (!reader) {
    return 0;
  }

  memset(reader, 0, sizeof(*reader));
  reader->file = fopen(path, "rb");

  if (!reader->file) {
    return 0;
  }

  return 1;
}

static void tspec_reader_close(TspecFileReader *reader) {
  TSPEC_TRACE("tspec_reader_close");
  if (!reader) {
    return;
  }

  if (reader->file) {
    fclose(reader->file);
    reader->file = NULL;
  }
}

static const char *tspec_reader_next_line(TspecFileReader *reader) {
  TSPEC_TRACE("tspec_reader_next_line");
  if (!reader || !reader->file) {
    return NULL;
  }

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
  TSPEC_TRACE("tspec_reader_read_blob");

  if (!reader || !blob) {
    return 0;
  }

  if (size > TSPEC_MAX_BLOB_SIZE) {
    return 0;
  }

  if (fread(blob->data, 1, size, reader->file) != size) {
    return 0;
  }

  blob->data[size] = '\0';
  blob->size       = size + 1;

  int c = fgetc(reader->file);

  if (c != '\n') {
    return 0;
  }

  return 1;
}

static const char *tspec_skip_ws(const char *s) {
  TSPEC_TRACE("tspec_skip_ws");

  while (*s == ' ' || *s == '\t') {
    s++;
  }

  return s;
}

static int tspec_parse_control_line(const char *line, const char *tag, const char **out) {
  TSPEC_TRACE("tspec_parse_control_line");

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
  TSPEC_TRACE("tspec_parse_int");

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
  TSPEC_TRACE("tspec_parse_size");

  char *end = NULL;

  long value = strtol(s, &end, 10);

  if (end == s || *end != '\0' || value < 0) {
    return 0;
  }

  return (size_t)value;
}

static int tspec_tokenize_args(const TspecBlob *blob, char argv[TSPEC_MAX_ARGV][TSPEC_MAX_ARG_SIZE], char **argv_ptrs) {
  TSPEC_TRACE("tspec_tokenize_args");

  if (!blob) {
    return -1;
  }

  const uint8_t *p    = blob->data;
  const uint8_t *end  = blob->data + blob->size;
  int            argc = 0;

  while (p < end && argc < TSPEC_MAX_ARGV - 1) {
    while (p < end && *p == ' ') {
      p++;
    }

    if (p == end) {
      break;
    }

    char   *out      = argv[argc];
    uint8_t in_quote = 0;

    argv_ptrs[argc] = out;

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
        break;
      }

      *out++ = *p++;
    }
  }

  argv_ptrs[argc] = NULL;
  return argc;
}

static TspecExecResult tspec_execute_command(const TspecCommand *cmd) {
  TSPEC_TRACE("tspec_execute_command");

  TspecExecResult result = {0};

  char *executable = (char *)cmd->executable.data;

  char *argv[TSPEC_MAX_ARGV];
  char  argv_storage[TSPEC_MAX_ARGV][TSPEC_MAX_ARG_SIZE];
  argv[0] = executable;

  int argc = tspec_tokenize_args(&cmd->args, argv_storage, argv + 1);

  if (argc < 0) {
    result.return_code = -1;
    return result;
  }

  int stdout_pipe[2];
  int stderr_pipe[2];

  if (pipe(stdout_pipe) == -1) {
    result.return_code = -1;
    return result;
  }

  if (pipe(stderr_pipe) == -1) {
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    result.return_code = -1;
    return result;
  }

  pid_t pid = fork();

  if (pid == -1) {
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

    execvp(executable, argv);
    _exit(127);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  int status = 0;

  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    result.return_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.return_code = -WTERMSIG(status);
  } else {
    result.return_code = -1;
  }

  ssize_t bytes = read(stdout_pipe[0], result.stdout_blob.data, TSPEC_MAX_BLOB_SIZE);

  if (bytes > 0) {
    result.stdout_blob.size = (size_t)bytes;
  }

  bytes = read(stderr_pipe[0], result.stderr_blob.data, TSPEC_MAX_BLOB_SIZE);

  if (bytes > 0) {
    result.stderr_blob.size = (size_t)bytes;
  }

  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  return result;
}

static int tspec_blob_equals(const TspecBlob *a, const TspecBlob *b) {
  TSPEC_TRACE("tspec_blob_equals");

  if (a->size != b->size) {
    return 0;
  }

  return memcmp(a->data, b->data, a->size) == 0;
}

static int tspec_blob_contains(const TspecBlob *haystack, const TspecBlob *needle) {
  TSPEC_TRACE("tspec_blob_contains");

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

static int tspec_validate_assertions(const TspecCommand *cmd, const TspecExecResult *result) {
  TSPEC_TRACE("tspec_validate_assertions");

  for (size_t i = 0; i < cmd->assertion_count; i++) {
    const TspecAssertion *assertion = &cmd->assertions[i];

    switch (assertion->type) {
      case TSPEC_ASSERTION_RETURN: {
        if (result->return_code != assertion->value.int_value) {
          TSPEC_FATAL("expected return %d but got %d", assertion->value.int_value, result->return_code);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT: {
        if (!tspec_blob_equals(&result->stdout_blob, &assertion->value.blob_value)) {
          TSPEC_FATAL("stdout mismatch");
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR: {
        if (!tspec_blob_equals(&result->stderr_blob, &assertion->value.blob_value)) {
          TSPEC_FATAL("stderr mismatch");
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT_CONTAINS: {
        if (!tspec_blob_contains(&result->stdout_blob, &assertion->value.blob_value)) {
          TSPEC_FATAL("stdout missing substring");
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR_CONTAINS: {
        if (!tspec_blob_contains(&result->stderr_blob, &assertion->value.blob_value)) {
          TSPEC_FATAL("stderr missing substring");
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
  TSPEC_TRACE("tspec_parse_file");
  TspecFileReader reader = {0};

  if (!tspec_reader_open(&reader, path)) {
    TSPEC_ERROR("failed to open %s: %s", path, strerror(errno));
    return 0;
  }

  memset(spec, 0, sizeof(*spec));

  TspecTest    *current_test = NULL;
  TspecCommand *current_cmd  = NULL;

  const char *line = NULL;

  while ((line = tspec_reader_next_line(&reader)) != NULL) {
    line = tspec_skip_ws(line);

    if (*line != '\0') {
      continue;
    }

    if (tspec_parse_control_line(line, "test", &line)) {
      TSPEC_TRACE("Found test: %s", line);

      if (spec->test_count >= TSPEC_MAX_TESTS) {
        TSPEC_ERROR("too many tests");
        return 0;
      }

      current_test = &spec->tests[spec->test_count++];
      memset(current_test, 0, sizeof(*current_test));
      strncpy(current_test->name, line, sizeof(current_test->name) - 1);

      continue;
    }

    if (tspec_parse_control_line(line, "command", &line)) {
      TSPEC_TRACE("Found command: %s", line);

      if (!current_test) {
        TSPEC_ERROR(":command outside test");
        return 0;
      }

      if (current_test->command_count >= TSPEC_MAX_COMMANDS) {
        TSPEC_ERROR("too many commands");
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
      size_t size = tspec_parse_size(line);

      if (!tspec_reader_read_blob(&reader, size, &current_cmd->executable)) {
        TSPEC_ERROR("failed reading executable blob");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob args", &line)) {
      size_t size = tspec_parse_size(line);

      if (!tspec_reader_read_blob(&reader, size, &current_cmd->args)) {
        TSPEC_ERROR("failed reading args blob");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int timeout_ms", &line)) {
      if (!tspec_parse_int(line, &current_cmd->timeout_ms)) {
        TSPEC_ERROR("invalid timeout_ms");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int return", &line)) {
      if (current_cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
        TSPEC_ERROR("too many assertions");
        return 0;
      }

      TspecAssertion *a = &current_cmd->assertions[current_cmd->assertion_count++];
      a->type           = TSPEC_ASSERTION_RETURN;

      if (!tspec_parse_int(line, &a->value.int_value)) {
        TSPEC_ERROR("invalid return value");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stdout_contains", &line)) {
      if (current_cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
        TSPEC_ERROR("too many assertions");
        return 0;
      }

      TspecAssertion *a = &current_cmd->assertions[current_cmd->assertion_count++];
      a->type           = TSPEC_ASSERTION_STDOUT_CONTAINS;
      size_t size       = tspec_parse_size(line);

      if (!tspec_reader_read_blob(&reader, size, &a->value.blob_value)) {
        TSPEC_ERROR("failed reading stdout_contains");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stderr_contains", &line)) {
      if (current_cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
        TSPEC_ERROR("too many assertions");
        return 0;
      }

      TspecAssertion *a = &current_cmd->assertions[current_cmd->assertion_count++];

      a->type     = TSPEC_ASSERTION_STDERR_CONTAINS;
      size_t size = tspec_parse_size(line);

      if (!tspec_reader_read_blob(&reader, size, &a->value.blob_value)) {
        TSPEC_ERROR("failed reading stderr_contains");
        return 0;
      }

      continue;
    }

    TSPEC_ERROR("unknown directive on line %d: '%s'", reader.line_number, reader.line_buffer);
    return 0;
  }

  tspec_reader_close(&reader);
  return 1;
}

static int tspec_run_file(const TspecFile *spec) {
  printf("hi!\n");
  for (size_t i = 0; i < spec->test_count; i++) {
    const TspecTest *test = &spec->tests[i];
    TSPEC_INFO("running test %s", test->name);

    for (size_t j = 0; j < test->command_count; j++) {
      const TspecCommand *cmd = &test->commands[j];
      TSPEC_INFO("running command %s", cmd->name);

      TspecExecResult result = tspec_execute_command(cmd);

      if (!tspec_validate_assertions(cmd, &result)) {
        TSPEC_FATAL("test %s failed", test->name);
        return 0;
      }

      TSPEC_INFO("%s", cmd->name);
    }
  }

  return 1;
}

static TspecFile spec;

int main(int argc, char **argv) {
  printf("Heyo!\n");

  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tspec>\n", argv[0]);
    return 1;
  }

  if (!tspec_parse_file(argv[1], &spec)) {
    return 1;
  }

  if (!tspec_run_file(&spec)) {
    return 1;
  }

  fflush(stderr);

  return 0;
}
