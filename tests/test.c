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

/* ============================================================================
 * LOGGING
 * ============================================================================ */

#define APP_LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define APP_LOG_INFO(fmt, ...)  fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define APP_LOG_PASS(fmt, ...)  fprintf(stdout, "[PASS] " fmt "\n", ##__VA_ARGS__)
#define APP_LOG_FAIL(fmt, ...)  fprintf(stderr, "[FAIL] " fmt "\n", ##__VA_ARGS__)

/* ============================================================================
 * TYPES
 * ============================================================================ */

#define TSPEC_MAX_BLOB_SIZE  8192
#define TSPEC_MAX_LINE_SIZE  512
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

  char line_buffer[TSPEC_MAX_LINE_SIZE];

  int line_number;
} TspecFileReader;

/* ============================================================================
 * READER
 * ============================================================================ */

static TspecFileReader *tspec_reader_open(const char *path) {
  TspecFileReader *reader = malloc(sizeof(*reader));

  if (!reader) {
    return NULL;
  }

  reader->file = fopen(path, "rb");

  if (!reader->file) {
    free(reader);
    return NULL;
  }

  reader->line_number = 0;

  return reader;
}

static void tspec_reader_close(TspecFileReader *reader) {
  if (!reader) {
    return;
  }

  if (reader->file) {
    fclose(reader->file);
  }

  free(reader);
}

static const char *tspec_reader_next_line(TspecFileReader *reader) {
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
  if (!reader || !blob) {
    return 0;
  }

  if (size > TSPEC_MAX_BLOB_SIZE) {
    return 0;
  }

  if (fread(blob->data, 1, size, reader->file) != size) {
    return 0;
  }

  blob->size = size;

  int c = fgetc(reader->file);

  if (c != '\n') {
    return 0;
  }

  return 1;
}

/* ============================================================================
 * PARSING HELPERS
 * ============================================================================ */

static const char *tspec_skip_ws(const char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }

  return s;
}

static int tspec_parse_control_line(const char *line, const char *tag, const char **out) {
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
  char *end = NULL;

  long value = strtol(s, &end, 10);

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
  char *end = NULL;

  long value = strtol(s, &end, 10);

  if (end == s || *end != '\0' || value < 0) {
    return 0;
  }

  return (size_t)value;
}

/* ============================================================================
 * ARG TOKENIZER
 * ============================================================================ */

static int tspec_tokenize_args(const TspecBlob *blob, char argv[TSPEC_MAX_ARGV][TSPEC_MAX_BLOB_SIZE], char *argv_ptrs[TSPEC_MAX_ARGV]) {
  if (!blob) {
    return -1;
  }

  size_t i    = 0;
  int    argc = 0;

  while (i < blob->size) {
    while (i < blob->size && blob->data[i] == ' ') {
      i++;
    }

    if (i >= blob->size) {
      break;
    }

    int    in_quote = 0;
    size_t len      = 0;

    while (i < blob->size) {
      char c = (char)blob->data[i];

      if (c == '"') {
        if (in_quote && i + 1 < blob->size && blob->data[i + 1] == '"') {
          argv[argc][len++] = '"';
          i += 2;
          continue;
        }

        in_quote = !in_quote;
        i++;
        continue;
      }

      if (c == ' ' && !in_quote) {
        break;
      }

      argv[argc][len++] = c;
      i++;
    }

    argv[argc][len] = '\0';
    argv_ptrs[argc] = argv[argc];

    argc++;

    if (argc >= TSPEC_MAX_ARGV - 1) {
      break;
    }
  }

  argv_ptrs[argc] = NULL;

  return argc;
}

/* ============================================================================
 * EXECUTION
 * ============================================================================ */

static TspecExecResult tspec_execute_command(const TspecCommand *cmd) {
  TspecExecResult result = {0};

  char executable[TSPEC_MAX_BLOB_SIZE + 1];

  memcpy(executable, cmd->executable.data, cmd->executable.size);

  executable[cmd->executable.size] = '\0';

  char  argv_storage[TSPEC_MAX_ARGV][TSPEC_MAX_BLOB_SIZE];
  char *argv[TSPEC_MAX_ARGV];

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

/* ============================================================================
 * ASSERTIONS
 * ============================================================================ */

static int tspec_blob_equals(const TspecBlob *a, const TspecBlob *b) {
  if (a->size != b->size) {
    return 0;
  }

  return memcmp(a->data, b->data, a->size) == 0;
}

static int tspec_blob_contains(const TspecBlob *haystack, const TspecBlob *needle) {
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
  for (size_t i = 0; i < cmd->assertion_count; i++) {
    const TspecAssertion *assertion = &cmd->assertions[i];

    switch (assertion->type) {
      case TSPEC_ASSERTION_RETURN: {
        if (result->return_code != assertion->value.int_value) {
          APP_LOG_FAIL("expected return %d but got %d", assertion->value.int_value, result->return_code);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT: {
        if (!tspec_blob_equals(&result->stdout_blob, &assertion->value.blob_value)) {
          APP_LOG_FAIL("stdout mismatch");
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR: {
        if (!tspec_blob_equals(&result->stderr_blob, &assertion->value.blob_value)) {
          APP_LOG_FAIL("stderr mismatch");
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT_CONTAINS: {
        if (!tspec_blob_contains(&result->stdout_blob, &assertion->value.blob_value)) {
          APP_LOG_FAIL("stdout missing substring");
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR_CONTAINS: {
        if (!tspec_blob_contains(&result->stderr_blob, &assertion->value.blob_value)) {
          APP_LOG_FAIL("stderr missing substring");
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

/* ============================================================================
 * PARSER
 * ============================================================================ */

static int tspec_parse_file(const char *path, TspecFile *spec) {
  TspecFileReader *reader = tspec_reader_open(path);

  if (!reader) {
    APP_LOG_ERROR("failed to open %s", path);
    return 0;
  }

  memset(spec, 0, sizeof(*spec));

  TspecTest    *current_test = NULL;
  TspecCommand *current_cmd  = NULL;

  const char *line = NULL;

  while ((line = tspec_reader_next_line(reader)) != NULL) {
    if (tspec_parse_control_line(line, "test", &line)) {
      if (spec->test_count >= TSPEC_MAX_TESTS) {
        APP_LOG_ERROR("too many tests");
        return 0;
      }

      current_test = &spec->tests[spec->test_count++];

      memset(current_test, 0, sizeof(*current_test));

      strncpy(current_test->name, line, sizeof(current_test->name) - 1);

      continue;
    }

    if (tspec_parse_control_line(line, "command", &line)) {
      if (!current_test) {
        APP_LOG_ERROR(":command outside test");
        return 0;
      }

      if (current_test->command_count >= TSPEC_MAX_COMMANDS) {
        APP_LOG_ERROR("too many commands");
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

      if (!tspec_reader_read_blob(reader, size, &current_cmd->executable)) {
        APP_LOG_ERROR("failed reading executable blob");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob args", &line)) {
      size_t size = tspec_parse_size(line);

      if (!tspec_reader_read_blob(reader, size, &current_cmd->args)) {
        APP_LOG_ERROR("failed reading args blob");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int timeout_ms", &line)) {
      if (!tspec_parse_int(line, &current_cmd->timeout_ms)) {
        APP_LOG_ERROR("invalid timeout_ms");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int return", &line)) {
      if (current_cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
        APP_LOG_ERROR("too many assertions");
        return 0;
      }

      TspecAssertion *a = &current_cmd->assertions[current_cmd->assertion_count++];

      a->type = TSPEC_ASSERTION_RETURN;

      if (!tspec_parse_int(line, &a->value.int_value)) {
        APP_LOG_ERROR("invalid return value");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stdout_contains", &line)) {
      if (current_cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
        APP_LOG_ERROR("too many assertions");
        return 0;
      }

      TspecAssertion *a = &current_cmd->assertions[current_cmd->assertion_count++];

      a->type = TSPEC_ASSERTION_STDOUT_CONTAINS;

      size_t size = tspec_parse_size(line);

      if (!tspec_reader_read_blob(reader, size, &a->value.blob_value)) {
        APP_LOG_ERROR("failed reading stdout_contains");
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stderr_contains", &line)) {
      if (current_cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
        APP_LOG_ERROR("too many assertions");
        return 0;
      }

      TspecAssertion *a = &current_cmd->assertions[current_cmd->assertion_count++];

      a->type = TSPEC_ASSERTION_STDERR_CONTAINS;

      size_t size = tspec_parse_size(line);

      if (!tspec_reader_read_blob(reader, size, &a->value.blob_value)) {
        APP_LOG_ERROR("failed reading stderr_contains");
        return 0;
      }

      continue;
    }

    APP_LOG_ERROR("unknown directive on line %d", reader->line_number);

    return 0;
  }

  tspec_reader_close(reader);

  return 1;
}

/* ============================================================================
 * RUNNER
 * ============================================================================ */

static int tspec_run_file(const TspecFile *spec) {
  for (size_t i = 0; i < spec->test_count; i++) {
    const TspecTest *test = &spec->tests[i];

    APP_LOG_INFO("running test %s", test->name);

    for (size_t j = 0; j < test->command_count; j++) {
      const TspecCommand *cmd = &test->commands[j];

      APP_LOG_INFO("running command %s", cmd->name);

      TspecExecResult result = tspec_execute_command(cmd);

      if (!tspec_validate_assertions(cmd, &result)) {
        APP_LOG_FAIL("test %s failed", test->name);
        return 0;
      }

      APP_LOG_PASS("%s", cmd->name);
    }
  }

  return 1;
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char **argv) {
  printf("Heyo!\n");

  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tspec>\n", argv[0]);
    return 1;
  }

  TspecFile spec;

  if (!tspec_parse_file(argv[1], &spec)) {
    return 1;
  }

  if (!tspec_run_file(&spec)) {
    return 1;
  }

  return 0;
}
