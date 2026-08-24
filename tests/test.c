#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
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

#define LOGCIE_MODULE "TSPEC"
#define LOGCIE_IMPLEMENTATION
#include "../logcie.h"

#define TSPEC_MAX_BLOB_SIZE  8192
#define TSPEC_MAX_LINE_SIZE  512
#define TSPEC_MAX_ARG_SIZE   512
#define TSPEC_MAX_ASSERTIONS 16
#define TSPEC_MAX_COMMANDS   32
#define TSPEC_MAX_TESTS      16
#define TSPEC_MAX_ARGV       64
#define TSPEC_MAX_PATH       512
#define TSPEC_MAX_SPEC_FILES 512

#define TSPEC_SIDE_EFFECTS_DIRNAME ".side_effects"

typedef struct TspecBlob {
  // NOTE: +1 at the end for \0 to keep data debug-printable
  uint8_t data[TSPEC_MAX_BLOB_SIZE + 1];
  size_t  size;
} TspecBlob;

typedef enum TspecAssertionType {
  TSPEC_ASSERTION_RETURN = 0,
  TSPEC_ASSERTION_STDOUT,
  TSPEC_ASSERTION_STDERR,
  TSPEC_ASSERTION_STDOUT_CONTAINS,
  TSPEC_ASSERTION_STDERR_CONTAINS,
  TSPEC_ASSERTION_FILE,
  TSPEC_ASSERTION_FILE_CONTAINS,
  TSPEC_ASSERTION_FILE_MISSING,
  TspecAssertionType_Count,
} TspecAssertionType;

typedef struct TspecFile {
  char      path[TSPEC_MAX_PATH];
  size_t    path_len;
  // NOTE: Only populated for TSPEC_ASSERTION_FILE_CONTAINS
  TspecBlob content;
} TspecFile;

typedef struct TspecAssertion {
  TspecAssertionType type;

  union {
    int32_t   int_value;
    TspecBlob blob_value;
    TspecFile file_value;
  } value;
} TspecAssertion;

typedef struct TspecCommand {
  char name[256];

  TspecBlob executable;
  TspecBlob args;

  int32_t timeout_ms;

  TspecAssertion assertions[TSPEC_MAX_ASSERTIONS];
  size_t         assertion_count;
} TspecCommand;

typedef struct TspecExecResult {
  int32_t return_code;

  TspecBlob stdout_blob;
  TspecBlob stderr_blob;

  uint8_t timed_out;
} TspecExecResult;

typedef struct TspecOutcomeStats {
  size_t tests_run;
  size_t tests_passed;
  size_t tests_failed;
} TspecOutcomeStats;

uint8_t tspec_run_file(const char *path, TspecOutcomeStats *outcome);

typedef struct TspecFileReader {
  FILE    *file;
  char     line_buffer[TSPEC_MAX_LINE_SIZE];
  uint32_t line_number;

  const char *file_path;
} TspecFileReader;

typedef struct TspecStats {
  size_t test_count;
  size_t command_count[TSPEC_MAX_TESTS];
} TspecStats;

static uint8_t tspec_reader_open(TspecFileReader *reader, const char *path) {
  LOGCIE_TRACE("tspec_reader_open(%p, \"%s\")", (void *)reader, path);

  assert(reader);
  assert(path);

  memset(reader, 0, sizeof(*reader));
  reader->file_path = path;
  reader->file      = fopen(path, "rb");

  if (!reader->file) {
    LOGCIE_ERROR("Failed to open '%s': %s", path, strerror(errno));
    return 0;
  }

  return 1;
}

static void tspec_reader_close(TspecFileReader *reader) {
  LOGCIE_TRACE("tspec_reader_close(%p)", (void *)reader);

  assert(reader);

  if (reader->file) {
    fclose(reader->file);
    reader->file = NULL;
  }
}

static const char *tspec_reader_next_line(TspecFileReader *reader) {
  LOGCIE_TRACE("tspec_reader_next_line(%p)", (void *)reader);

  assert(reader);
  assert(reader->file);

  if (!fgets(reader->line_buffer, sizeof(reader->line_buffer), reader->file)) {
    return NULL;
  }

  reader->line_number++;
  size_t len = strlen(reader->line_buffer);

  if (len > 0 && reader->line_buffer[len - 1] == '\n') {
    // NOTE: len can legitimately be 1 here (a blank line, which the format
    // explicitly allows). len - 2 must not be evaluated unless len >= 2,
    // otherwise this underflows (len is size_t) and reads out of bounds.
    if (len >= 2 && reader->line_buffer[len - 2] == '\r') {
      LOGCIE_FATAL("%s:%u: CRLF line endings are not allowed", reader->file_path, reader->line_number);
      return NULL;
    }

    reader->line_buffer[len - 1] = '\0';
  }

  return reader->line_buffer;
}

static uint8_t tspec_reader_read_blob(TspecFileReader *reader, size_t size, TspecBlob *blob) {
  LOGCIE_TRACE("tspec_reader_read_blob(%p, %zu, %p)", (void *)reader, size, (void *)blob);

  assert(reader);
  assert(reader->file);
  assert(blob);

  if (size > TSPEC_MAX_BLOB_SIZE) {
    LOGCIE_ERROR("%s:%u: Blob size %zu exceeds max capacity", reader->file_path, reader->line_number, size);
    return 0;
  }

  blob->data[size] = '\0';
  blob->size       = size;

  if (fread(blob->data, 1, size, reader->file) != size) {
    LOGCIE_ERROR("%s:%u: fread failed: %s", reader->file_path, reader->line_number, strerror(errno));
    return 0;
  }

  LOGCIE_DEBUG("Parsed blob data (%zu bytes): '%s'", blob->size, blob->data);
  int32_t c = fgetc(reader->file);

  if (c != '\n') {
    LOGCIE_ERROR("%s:%u: Blob is not fully parsed ('%c' was left)", reader->file_path, reader->line_number, c);
    return 0;
  }

  return 1;
}

static const char *tspec_skip_ws(const char *s) {
  LOGCIE_TRACE("tspec_skip_ws(\"%s\")", s);

  assert(s);

  while (*s == ' ' || *s == '\t') {
    s++;
  }

  return s;
}

static uint8_t tspec_parse_control_line(const char *line, const char *tag, const char **out) {
  LOGCIE_TRACE("tspec_parse_control_line(\"%s\", \"%s\", %p)", line, tag, (void *)out);

  assert(line);
  assert(tag);

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

static uint8_t tspec_parse_int(const char *s, int32_t *out) {
  LOGCIE_TRACE("tspec_parse_int(\"%s\", %p)", s, (void *)out);

  assert(s);
  assert(out);

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

static uint8_t tspec_parse_size(const char *s, size_t *out) {
  LOGCIE_TRACE("tspec_parse_size(\"%s\", %p)", s, (void *)out);

  assert(s);
  assert(out);

  char *end   = NULL;
  long  value = strtol(s, &end, 10);

  if (end == s || *end != '\0' || value < 0) {
    return 0;
  }

  *out = (size_t)value;
  return 1;
}

static uint8_t tspec_parse_sized_inline(const char *line, char *data_out, size_t data_out_cap, size_t *size_out, const char **rest_out) {
  LOGCIE_TRACE("tspec_parse_sized_inline(\"%s\", %p, %zu, %p, %p)", line, (void *)data_out, data_out_cap, (void *)size_out, (void *)rest_out);

  assert(line);
  assert(data_out);
  assert(data_out_cap > 0);

  char *end   = NULL;
  long  value = strtol(line, &end, 10);

  if (end == line || value < 0) {
    return 0;
  }

  if (*end != ' ') {
    return 0;
  }

  const char *data = end + 1;
  size_t      size = (size_t)value;

  if (size + 1 > data_out_cap) {
    LOGCIE_ERROR("sized field length %zu exceeds max capacity %zu", size, data_out_cap - 1);
    return 0;
  }

  if (strlen(data) < size) {
    return 0;
  }

  memcpy(data_out, data, size);
  data_out[size] = '\0';

  if (size_out) {
    *size_out = size;
  }

  if (rest_out) {
    *rest_out = data + size;
  }

  return 1;
}

static uint8_t tspec_parse_blob_field(TspecFileReader *reader, const char *line, TspecBlob *dest) {
  LOGCIE_TRACE("tspec_parse_blob_field(%p, \"%s\", %p)", (void *)reader, line, (void *)dest);

  assert(reader);
  assert(reader->file);
  assert(line);
  assert(dest);

  size_t size;

  if (!tspec_parse_size(line, &size)) {
    LOGCIE_ERROR("%s:%u: Invalid blob size", reader->file_path, reader->line_number);
    return 0;
  }

  return tspec_reader_read_blob(reader, size, dest);
}

static uint8_t tspec_collect_stats(const char *path, TspecStats *stats, TspecFileReader *reader) {
  LOGCIE_TRACE("tspec_collect_stats(\"%s\", %p, %p)", path, (void *)stats, (void *)reader);

  assert(path);
  assert(stats);
  assert(reader);

  memset(stats, 0, sizeof(*stats));
  size_t current_test = (size_t)-1;

  const char *line;

  while ((line = tspec_reader_next_line(reader)) != NULL) {
    line = tspec_skip_ws(line);

    if (*line == '\0') {
      continue;
    }

    if (tspec_parse_control_line(line, "test", &line)) {
      stats->test_count++;

      if (stats->test_count > TSPEC_MAX_TESTS) {
        LOGCIE_ERROR("%s:%u: too many tests (max %d)", path, reader->line_number, TSPEC_MAX_TESTS);
        return 0;
      }

      current_test = stats->test_count - 1;

      stats->command_count[current_test] = 0;
      continue;
    }

    if (tspec_parse_control_line(line, "command", &line)) {
      if (current_test == (size_t)-1) {
        LOGCIE_ERROR("%s:%u: command outside test", reader->file_path, reader->line_number);
        return 0;
      }

      stats->command_count[current_test]++;

      if (stats->command_count[current_test] > TSPEC_MAX_COMMANDS) {
        LOGCIE_ERROR("%s:%u: too many commands (max %d)", reader->file_path, reader->line_number, TSPEC_MAX_COMMANDS);
        return 0;
      }

      continue;
    }
  }

  return 1;
}

static uint32_t tspec_tokenize_args(const TspecBlob *blob, char argv[TSPEC_MAX_ARGV][TSPEC_MAX_ARG_SIZE], char **argv_ptrs) {
  LOGCIE_TRACE("tspec_tokenize_args(%p, %p, %p)", (void *)blob, (void *)argv, (void *)argv_ptrs);

  assert(blob);
  assert(argv);
  assert(argv_ptrs);

  const uint8_t *p   = blob->data;
  const uint8_t *end = blob->data + blob->size;

  uint32_t argc = 0;

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

static uint8_t tspec_resolve_base_dir(const char *spec_path, char *out, size_t out_cap) {
  LOGCIE_TRACE("tspec_resolve_base_dir(\"%s\", %p, %zu)", spec_path, (void *)out, out_cap);

  assert(spec_path);
  assert(out);
  assert(out_cap > 0);

  size_t spec_path_len = strlen(spec_path);
  char   path_copy[spec_path_len + 1];
  strcpy(path_copy, spec_path);

  char *dir = dirname(path_copy);

  char resolved[PATH_MAX];

  if (!realpath(dir, resolved)) {
    LOGCIE_ERROR("Could not resolve directory '%s': %s", dir, strerror(errno));
    return 0;
  }

  if (strlen(resolved) >= out_cap) {
    LOGCIE_ERROR("Resolved directory path too long: '%s'", resolved);
    return 0;
  }

  strcpy(out, resolved);
  return 1;
}

static void tspec_rm_rf(const char *path) {
  LOGCIE_TRACE("tspec_rm_rf(\"%s\")", path);

  assert(path);

  struct stat st;

  if (lstat(path, &st) != 0) {
    return;
  }

  if (!S_ISDIR(st.st_mode)) {
    unlink(path);
    return;
  }

  DIR *d = opendir(path);

  if (d) {
    struct dirent *e;

    while ((e = readdir(d)) != NULL) {
      if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
        continue;
      }

      char child[TSPEC_MAX_PATH];
      snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
      tspec_rm_rf(child);
    }

    closedir(d);
  }

  rmdir(path);
}

static uint64_t tspec_time_ms(void) {
  LOGCIE_TRACE("tspec_time_ms()");

  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }

  return (uint64_t)ts.tv_sec * 1000u +
         (uint64_t)ts.tv_nsec / 1000000u;
}

static void tspec_set_nonblocking(int32_t fd) {
  LOGCIE_TRACE("tspec_set_nonblocking(%d)", fd);

  assert(fd >= 0);

  int32_t flags = fcntl(fd, F_GETFL);

  if (flags == -1) {
    return;
  }

  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void tspec_drain_fd(int32_t fd, TspecBlob *blob, const char *name) {
  LOGCIE_TRACE("tspec_drain_fd(%d, %p, \"%s\")", fd, (void *)blob, name);

  assert(fd >= 0);
  assert(blob);
  assert(name);

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
          LOGCIE_WARN("%s exceeded buffer, truncating", name);
        }
      }
    } else if (n == 0) {
      break;  // EOF
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;  // no more data available
    } else {
      LOGCIE_ERROR("%s read error: %s", name, strerror(errno));
      break;
    }
  }
}

static TspecExecResult tspec_execute_command(const TspecCommand *cmd, const char *base_dir) {
  LOGCIE_TRACE("tspec_execute_command(%p, \"%s\")", (void *)cmd, base_dir);

  assert(cmd);
  assert(base_dir);

  LOGCIE_VERBOSE("Executing command: '%s%s%s'", cmd->executable.data, cmd->args.size > 1 ? " " : "", cmd->args.data);

  TspecExecResult result = {0};

  char *executable = (char *)cmd->executable.data;

  char *argv[TSPEC_MAX_ARGV];
  char  argv_storage[TSPEC_MAX_ARGV][TSPEC_MAX_ARG_SIZE];
  argv[0] = executable;

  uint32_t argc = tspec_tokenize_args(&cmd->args, argv_storage, argv + 1);
  LOGCIE_DEBUG("argc: %d", argc + 1);

  for (uint32_t i = 0; i <= argc; i++) {
    LOGCIE_DEBUG("argv[%d]: %s", i, argv[i]);
  }

  int32_t stdout_pipe[2];
  int32_t stderr_pipe[2];

  if (pipe(stdout_pipe) == -1) {
    LOGCIE_ERROR("pipe(stdout) failed for '%s': %s", cmd->name, strerror(errno));
    result.return_code = -1;
    return result;
  }

  if (pipe(stderr_pipe) == -1) {
    LOGCIE_ERROR("pipe(stderr) failed for '%s': %s", cmd->name, strerror(errno));
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    result.return_code = -1;
    return result;
  }

  pid_t pid = fork();

  if (pid == -1) {
    LOGCIE_ERROR("fork failed for '%s': %s", cmd->name, strerror(errno));
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

    if (chdir(base_dir) != 0) {
      _exit(127);
    }

    execvp(executable, argv);
    _exit(127);
  } else {
    setpgid(pid, pid);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  tspec_set_nonblocking(stdout_pipe[0]);
  tspec_set_nonblocking(stderr_pipe[0]);

  int32_t  status   = 0;
  uint64_t start_ms = tspec_time_ms();

  struct timespec ts;
  ts.tv_sec  = 0;
  ts.tv_nsec = 10000000L; /* 10 ms */

  if (cmd->timeout_ms > 0) {
    LOGCIE_DEBUG("Timeout is set to %dms", cmd->timeout_ms);
  }

  while (1) {
    tspec_drain_fd(stdout_pipe[0], &result.stdout_blob, "stdout");
    tspec_drain_fd(stderr_pipe[0], &result.stderr_blob, "stderr");

    pid_t r = waitpid(pid, &status, WNOHANG);

    if (r == pid) {
      break;
    }

    if (r == -1) {
      LOGCIE_ERROR("waitpid failed: %s", strerror(errno));
      result.return_code = -1;
      break;
    }

    uint64_t time_diff_ms = tspec_time_ms() - start_ms;

    if (cmd->timeout_ms > 0 && time_diff_ms >= (uint64_t)cmd->timeout_ms) {
      LOGCIE_WARN("Process timed out after %zums", time_diff_ms);

      kill(-pid, SIGKILL);

      if (waitpid(pid, &status, 0) == -1) {
        LOGCIE_ERROR("waitpid after kill failed: %s", strerror(errno));
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
    LOGCIE_DEBUG("Process exited with code %d", result.return_code);
  } else if (WIFSIGNALED(status)) {
    int32_t sig        = WTERMSIG(status);
    result.return_code = -sig;

    LOGCIE_WARN("Process killed by signal %d", sig);
  } else {
    result.return_code = -1;
    LOGCIE_ERROR("Process exited abnormally");
  }

  LOGCIE_DEBUG("stdout (%zu bytes): %s", result.stdout_blob.size, result.stdout_blob.data);
  LOGCIE_DEBUG("stderr (%zu bytes): %s", result.stderr_blob.size, result.stderr_blob.data);

  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  return result;
}

static uint8_t tspec_blob_equals(const TspecBlob *a, const TspecBlob *b) {
  LOGCIE_TRACE("tspec_blob_equals(%p, %p)", (void *)a, (void *)b);

  assert(a);
  assert(b);

  if (a->size != b->size) {
    return 0;
  }

  return memcmp(a->data, b->data, a->size) == 0;
}

static uint8_t tspec_blob_contains(const TspecBlob *haystack, const TspecBlob *needle) {
  LOGCIE_TRACE("tspec_blob_contains(%p, %p)", (void *)haystack, (void *)needle);

  assert(haystack);
  assert(needle);

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
  LOGCIE_TRACE("tspec_cmd_add_assertion(%p)", (void *)cmd);

  assert(cmd);

  if (cmd->assertion_count >= TSPEC_MAX_ASSERTIONS) {
    LOGCIE_ERROR("too many assertions in command '%s'", cmd->name);
    return NULL;
  }

  return &cmd->assertions[cmd->assertion_count++];
}

static uint8_t tspec_parse_blob_assertion(TspecFileReader *reader, TspecCommand *cmd, const char *line, TspecAssertionType type) {
  LOGCIE_TRACE("tspec_parse_blob_assertion(%p, %p, \"%s\", %d)", (void *)reader, (void *)cmd, line, type);

  assert(TspecAssertionType_Count == 8);
  assert(reader);
  assert(reader->file);
  assert(cmd);
  assert(line);
  assert(type >= TSPEC_ASSERTION_STDOUT);
  assert(type <= TSPEC_ASSERTION_STDERR_CONTAINS);

  TspecAssertion *assertion = tspec_cmd_add_assertion(cmd);

  if (!assertion) {
    return 0;
  }

  assertion->type = type;

  if (!tspec_parse_blob_field(reader, line, &assertion->value.blob_value)) {
    LOGCIE_ERROR("%s:%u: Failed reading blob assertion", reader->file_path, reader->line_number);
    cmd->assertion_count--;
    return 0;
  }

  LOGCIE_DEBUG("Blob assertion: %s", assertion->value.blob_value.data);
  return 1;
}

static uint8_t tspec_parse_file_assertion(TspecFileReader *reader, TspecCommand *cmd, const char *line, TspecAssertionType type) {
  LOGCIE_TRACE("tspec_parse_file_assertion(%p, %p, \"%s\", %d)", (void *)reader, (void *)cmd, line, type);

  assert(TspecAssertionType_Count == 8);
  assert(reader);
  assert(reader->file);
  assert(cmd);
  assert(line);
  assert(type == TSPEC_ASSERTION_FILE || type == TSPEC_ASSERTION_FILE_CONTAINS || type == TSPEC_ASSERTION_FILE_MISSING);

  TspecAssertion *a = tspec_cmd_add_assertion(cmd);

  if (!a) {
    return 0;
  }

  a->type = type;

  const char *rest = NULL;

  if (!tspec_parse_sized_inline(line, a->value.file_value.path, sizeof(a->value.file_value.path), &a->value.file_value.path_len, &rest)) {
    LOGCIE_ERROR("%s:%u: Malformed file assertion", reader->file_path, reader->line_number);
    cmd->assertion_count--;
    return 0;
  }

  LOGCIE_DEBUG("Parsed file path: %s (len: %zu)", a->value.file_value.path, a->value.file_value.path_len);

  if (type == TSPEC_ASSERTION_FILE_CONTAINS) {
    if (*rest != ' ') {
      LOGCIE_ERROR("%s:%u: Malformed file_contains assertion (missing content size)", reader->file_path, reader->line_number);
      cmd->assertion_count--;
      return 0;
    }

    rest++;

    size_t content_size;

    if (!tspec_parse_size(rest, &content_size)) {
      LOGCIE_ERROR("%s:%u: Invalid content size for file_contains assertion", reader->file_path, reader->line_number);
      cmd->assertion_count--;
      return 0;
    }

    if (!tspec_reader_read_blob(reader, content_size, &a->value.file_value.content)) {
      LOGCIE_ERROR("%s:%u: Failed reading file_contains content", reader->file_path, reader->line_number);
      cmd->assertion_count--;
      return 0;
    }
  }

  return 1;
}

static uint8_t tspec_join_path(const char *base_dir, const char *rel_path, char *out, size_t out_cap) {
  LOGCIE_TRACE("tspec_join_path(\"%s\", \"%s\", %p, %zu)", base_dir, rel_path, (void *)out, out_cap);

  assert(base_dir);
  assert(rel_path);
  assert(out);
  assert(out_cap > 0);

  int written = snprintf(out, out_cap, "%s/%s", base_dir, rel_path);

  if (written < 0 || (size_t)written >= out_cap) {
    out[0] = '\0';
    return 0;
  }

  return 1;
}

static uint8_t tspec_validate_assertions(const TspecCommand *cmd, const TspecExecResult *result, const char *base_dir) {
  LOGCIE_TRACE("tspec_validate_assertions(%p, %p, \"%s\")", (void *)cmd, (void *)result, base_dir);

  assert(TspecAssertionType_Count == 8);
  assert(cmd);
  assert(result);
  assert(base_dir);

  for (size_t i = 0; i < cmd->assertion_count; i++) {
    const TspecAssertion *assertion = &cmd->assertions[i];

    switch (assertion->type) {
      case TSPEC_ASSERTION_RETURN: {
        if (result->return_code != assertion->value.int_value) {
          LOGCIE_ERROR("expected return %d but got %d", assertion->value.int_value, result->return_code);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT: {
        if (!tspec_blob_equals(&result->stdout_blob, &assertion->value.blob_value)) {
          LOGCIE_ERROR("stdout mismatch");
          LOGCIE_DEBUG("expected (%zu bytes): '%s'", assertion->value.blob_value.size, assertion->value.blob_value.data);
          LOGCIE_DEBUG("actual   (%zu bytes): '%s'", result->stdout_blob.size, result->stdout_blob.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR: {
        if (!tspec_blob_equals(&result->stderr_blob, &assertion->value.blob_value)) {
          LOGCIE_ERROR("stderr mismatch");
          LOGCIE_DEBUG("expected (%zu bytes): '%s'", assertion->value.blob_value.size, assertion->value.blob_value.data);
          LOGCIE_DEBUG("actual   (%zu bytes): '%s'", result->stderr_blob.size, result->stderr_blob.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDOUT_CONTAINS: {
        if (!tspec_blob_contains(&result->stdout_blob, &assertion->value.blob_value)) {
          LOGCIE_ERROR("stdout missing substring: '%s'", assertion->value.blob_value.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_STDERR_CONTAINS: {
        if (!tspec_blob_contains(&result->stderr_blob, &assertion->value.blob_value)) {
          LOGCIE_ERROR("stderr missing substring: '%s'", assertion->value.blob_value.data);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_FILE: {
        char full_path[PATH_MAX];

        if (!tspec_join_path(base_dir, assertion->value.file_value.path, full_path, sizeof(full_path))) {
          LOGCIE_ERROR("path too long: '%s'", assertion->value.file_value.path);
          return 0;
        }

        if (access(full_path, F_OK) != 0) {
          LOGCIE_ERROR("expected file to exist: '%s'", assertion->value.file_value.path);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_FILE_MISSING: {
        char full_path[PATH_MAX];

        if (!tspec_join_path(base_dir, assertion->value.file_value.path, full_path, sizeof(full_path))) {
          LOGCIE_ERROR("path too long: '%s'", assertion->value.file_value.path);
          return 0;
        }

        if (access(full_path, F_OK) == 0) {
          LOGCIE_ERROR("expected file to be missing: '%s'", assertion->value.file_value.path);
          return 0;
        }

        break;
      }

      case TSPEC_ASSERTION_FILE_CONTAINS: {
        const TspecFile *fv = &assertion->value.file_value;
        char             full_path[PATH_MAX];

        if (!tspec_join_path(base_dir, fv->path, full_path, sizeof(full_path))) {
          LOGCIE_ERROR("path too long: '%s'", fv->path);
          return 0;
        }

        struct stat st;

        if (stat(full_path, &st) != 0) {
          LOGCIE_ERROR("expected file to exist: '%s'", fv->path);
          return 0;
        }

        if ((size_t)st.st_size != fv->content.size) {
          LOGCIE_ERROR("file '%s' size mismatch (expected %zu, got %lld)", fv->path, fv->content.size, (long long)st.st_size);
          return 0;
        }

        FILE *f = fopen(full_path, "rb");

        if (!f) {
          LOGCIE_ERROR("failed to open '%s': %s", fv->path, strerror(errno));
          return 0;
        }

        uint8_t actual[TSPEC_MAX_BLOB_SIZE];
        size_t  got = fread(actual, 1, fv->content.size, f);
        fclose(f);

        if (got != fv->content.size || memcmp(actual, fv->content.data, fv->content.size) != 0) {
          LOGCIE_ERROR("content mismatch for '%s'", fv->path);
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

static uint8_t tspec_finish_command(TspecCommand *cmd, const char *base_dir) {
  LOGCIE_TRACE("tspec_finish_command(%p, \"%s\")", (void *)cmd, base_dir);

  assert(cmd);
  assert(base_dir);

  TspecExecResult result = tspec_execute_command(cmd, base_dir);

  if (result.timed_out || !tspec_validate_assertions(cmd, &result, base_dir)) {
    return 0;
  }

  return 1;
}

static void tspec_finish_or_skip_command(TspecCommand *cmd, uint8_t *test_failed, const char *base_dir) {
  LOGCIE_TRACE("tspec_finish_or_skip_command(%p, %p, \"%s\")", (void *)cmd, (void *)test_failed, base_dir);

  assert(cmd);
  assert(test_failed);
  assert(base_dir);

  if (*test_failed) {
    LOGCIE_DEBUG("Skipping command '%s' (test already failed)", cmd->name);
    return;
  }

  if (!tspec_finish_command(cmd, base_dir)) {
    *test_failed = 1;
  }
}

static void tspec_conclude_test(const char *test_name, uint8_t have_test, uint8_t test_failed, TspecOutcomeStats *outcome) {
  LOGCIE_TRACE("tspec_conclude_test(\"%s\", %u, %u, %p)", test_name, have_test, test_failed, (void *)outcome);

  assert(test_name);
  assert(outcome);
  assert(have_test <= 1);
  assert(test_failed <= 1);

  if (!have_test) {
    return;
  }

  outcome->tests_run++;

  if (test_failed) {
    outcome->tests_failed++;
    LOGCIE_ERROR("FAIL  %s", test_name);
  } else {
    outcome->tests_passed++;
    LOGCIE_INFO("PASS  %s", test_name);
  }
}

static uint8_t tspec_stream_run(const char *path, TspecStats *stats, TspecFileReader *reader, TspecOutcomeStats *outcome, const char *base_dir) {
  LOGCIE_TRACE("tspec_stream_run(\"%s\", %p, %p, %p, \"%s\")", path, (void *)stats, (void *)reader, (void *)outcome, base_dir);

  assert(path);
  assert(stats);
  assert(reader);
  assert(reader->file);
  assert(outcome);
  assert(base_dir);

  TspecCommand current_cmd = {0};

  size_t current_test_index = (size_t)-1;
  size_t current_cmd_index  = (size_t)-1;

  uint8_t have_test      = 0;
  uint8_t have_cmd       = 0;
  uint8_t test_failed    = 0;
  char    test_name[256] = {0};

  const char *line;

  while ((line = tspec_reader_next_line(reader)) != NULL) {
    line = tspec_skip_ws(line);

    if (*line == '\0') {
      continue;
    }

    if (tspec_parse_control_line(line, "test", &line)) {
      if (have_cmd) {
        if (current_cmd.executable.size == 0) {
          LOGCIE_FATAL("%s:%u: No executable for command %s", reader->file_path, reader->line_number, current_cmd.name);
          return 0;
        }

        LOGCIE_INFO("  [%zu/%zu] command: %s", current_cmd_index + 1, stats->command_count[current_test_index], current_cmd.name);

        tspec_finish_or_skip_command(&current_cmd, &test_failed, base_dir);

        have_cmd = 0;
        memset(&current_cmd, 0, sizeof(current_cmd));
      }

      tspec_conclude_test(test_name, have_test, test_failed, outcome);
      test_failed = 0;

      current_test_index++;

      if (current_test_index >= stats->test_count) {
        LOGCIE_ERROR("Internal test counter mismatch");
        return 0;
      }

      current_cmd_index = (size_t)-1;
      have_test         = 1;

      strncpy(test_name, line, sizeof(test_name) - 1);
      test_name[sizeof(test_name) - 1] = '\0';

      LOGCIE_INFO(
        "[%zu/%zu] test: %s (%zu command%s)",
        current_test_index + 1,
        stats->test_count,
        line,
        stats->command_count[current_test_index],
        stats->command_count[current_test_index] == 1 ? "" : "s"
      );

      continue;
    }

    if (tspec_parse_control_line(line, "command", &line)) {
      if (!have_test) {
        LOGCIE_ERROR("%s:%u: command outside test", reader->file_path, reader->line_number);
        return 0;
      }

      if (have_cmd) {
        if (current_cmd.executable.size == 0) {
          LOGCIE_FATAL("%s:%u: No executable for command %s", reader->file_path, reader->line_number, current_cmd.name);
          return 0;
        }

        LOGCIE_INFO("  [%zu/%zu] command: %s", current_cmd_index + 1, stats->command_count[current_test_index], current_cmd.name);

        tspec_finish_or_skip_command(&current_cmd, &test_failed, base_dir);

        memset(&current_cmd, 0, sizeof(current_cmd));
      }

      current_cmd_index++;
      strncpy(current_cmd.name, line, sizeof(current_cmd.name) - 1);
      current_cmd.name[sizeof(current_cmd.name) - 1] = '\0';

      have_cmd = 1;

      continue;
    }

    if (!have_cmd) {
      LOGCIE_ERROR("%s:%u: directive outside command", reader->file_path, reader->line_number);
      return 0;
    }

    if (tspec_parse_control_line(line, "blob executable", &line)) {
      if (!tspec_parse_blob_field(reader, line, &current_cmd.executable)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob args", &line)) {
      if (!tspec_parse_blob_field(reader, line, &current_cmd.args)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int timeout_ms", &line)) {
      if (!tspec_parse_int(line, &current_cmd.timeout_ms)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "int return", &line)) {
      TspecAssertion *a = tspec_cmd_add_assertion(&current_cmd);

      if (!a) {
        return 0;
      }

      a->type = TSPEC_ASSERTION_RETURN;

      if (!tspec_parse_int(line, &a->value.int_value)) {
        current_cmd.assertion_count--;
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stdin", &line)) {
      LOGCIE_ERROR("%s:%u: stdin is reserved and not supported", reader->file_path, reader->line_number);
      return 0;
    }

    if (tspec_parse_control_line(line, "blob stdout", &line)) {
      if (!tspec_parse_blob_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_STDOUT)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stderr", &line)) {
      if (!tspec_parse_blob_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_STDERR)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stdout_contains", &line)) {
      if (!tspec_parse_blob_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_STDOUT_CONTAINS)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "blob stderr_contains", &line)) {
      if (!tspec_parse_blob_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_STDERR_CONTAINS)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "file_contains", &line)) {
      if (!tspec_parse_file_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_FILE_CONTAINS)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "file_missing", &line)) {
      if (!tspec_parse_file_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_FILE_MISSING)) {
        return 0;
      }

      continue;
    }

    if (tspec_parse_control_line(line, "file", &line)) {
      if (!tspec_parse_file_assertion(reader, &current_cmd, line, TSPEC_ASSERTION_FILE)) {
        return 0;
      }

      continue;
    }

    LOGCIE_ERROR("%s:%u: Unknown directive '%s'", reader->file_path, reader->line_number, reader->line_buffer);
    return 0;
  }

  if (have_cmd) {
    if (current_cmd.executable.size == 0) {
      LOGCIE_FATAL("%s:%u: No executable for command %s", reader->file_path, reader->line_number, current_cmd.name);
      return 0;
    }

    LOGCIE_INFO("  [%zu/%zu] command: %s", current_cmd_index + 1, stats->command_count[current_test_index], current_cmd.name);

    tspec_finish_or_skip_command(&current_cmd, &test_failed, base_dir);
  }

  tspec_conclude_test(test_name, have_test, test_failed, outcome);

  return 1;
}

uint8_t tspec_run_file(const char *path, TspecOutcomeStats *outcome) {
  LOGCIE_TRACE("tspec_run_file(\"%s\", %p)", path, (void *)outcome);

  assert(path);
  assert(outcome);

  TspecStats      stats  = {0};
  TspecFileReader reader = {0};

  if (!tspec_reader_open(&reader, path)) {
    return 0;
  }

  if (!tspec_collect_stats(path, &stats, &reader)) {
    tspec_reader_close(&reader);
    return 0;
  }

  char base_dir[PATH_MAX];

  if (!tspec_resolve_base_dir(path, base_dir, sizeof(base_dir))) {
    tspec_reader_close(&reader);
    return 0;
  }

  char side_effects_dir[PATH_MAX];

  if (!tspec_join_path(base_dir, TSPEC_SIDE_EFFECTS_DIRNAME, side_effects_dir, sizeof(side_effects_dir))) {
    LOGCIE_ERROR("Path too long: %s/%s", base_dir, TSPEC_SIDE_EFFECTS_DIRNAME);
    tspec_reader_close(&reader);
    return 0;
  }

  tspec_rm_rf(side_effects_dir);

  if (mkdir(side_effects_dir, 0755) != 0) {
    LOGCIE_ERROR("Could not create %s: %s", side_effects_dir, strerror(errno));
    tspec_reader_close(&reader);
    return 0;
  }

  fseek(reader.file, 0, 0);
  reader.line_number = 0;

  uint8_t res = tspec_stream_run(path, &stats, &reader, outcome, base_dir);

  tspec_reader_close(&reader);
  return res;
}

typedef struct TspecFileList {
  char   paths[TSPEC_MAX_SPEC_FILES][TSPEC_MAX_PATH];
  size_t count;
} TspecFileList;

static uint8_t tspec_has_suffix(const char *s, const char *suffix) {
  LOGCIE_TRACE("tspec_has_suffix(\"%s\", \"%s\")", s, suffix);

  assert(s);
  assert(suffix);

  size_t s_len   = strlen(s);
  size_t suf_len = strlen(suffix);

  if (suf_len > s_len) {
    return 0;
  }

  return strcmp(s + s_len - suf_len, suffix) == 0;
}

static int tspec_path_cmp(const void *a, const void *b) {
  LOGCIE_TRACE("tspec_path_cmp(%p, %p)", a, b);

  assert(a);
  assert(b);

  return strcmp((const char *)a, (const char *)b);
}

static void tspec_discover(const char *dir, TspecFileList *list) {
  LOGCIE_TRACE("tspec_discover(\"%s\", %p)", dir, (void *)list);

  assert(dir);
  assert(list);

  DIR *d = opendir(dir);

  if (!d) {
    LOGCIE_ERROR("Failed to open directory '%s': %s", dir, strerror(errno));
    return;
  }

  struct dirent *e;

  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] == '.') {
      continue;
    }

    char child[TSPEC_MAX_PATH];

    if ((size_t)snprintf(child, sizeof(child), "%s/%s", dir, e->d_name) >= sizeof(child)) {
      LOGCIE_ERROR("Path too long, skipping: %s/%s", dir, e->d_name);
      continue;
    }

    struct stat st;

    if (stat(child, &st) != 0) {
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      tspec_discover(child, list);
      continue;
    }

    if (tspec_has_suffix(child, ".tspec")) {
      if (list->count >= TSPEC_MAX_SPEC_FILES) {
        LOGCIE_ERROR("Too many .tspec files found (max %d), skipping '%s'", TSPEC_MAX_SPEC_FILES, child);
        continue;
      }

      strncpy(list->paths[list->count], child, TSPEC_MAX_PATH - 1);
      list->paths[list->count][TSPEC_MAX_PATH - 1] = '\0';
      list->count++;
    }
  }

  closedir(d);
}

static void tspec_print_summary(const TspecOutcomeStats *outcome, size_t files_total, size_t files_errored) {
  LOGCIE_TRACE("tspec_print_summary(%p, %zu, %zu)", (void *)outcome, files_total, files_errored);

  assert(outcome);

  LOGCIE_INFO(" ");
  LOGCIE_INFO("==================== summary ====================");
  LOGCIE_INFO("files: %zu total, %zu could not be run (format/setup errors)", files_total, files_errored);
  LOGCIE_INFO("tests: %zu total, %zu passed, %zu failed", outcome->tests_run, outcome->tests_passed, outcome->tests_failed);
}

static Logcie_Sink tspec_sink = {
  .formatter = {logcie_printf_formatter, LOGCIE_COLOR_GRAY "[$M]$r $c$L$r:$<6$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = logcie_filter_and(
    logcie_filter_module_eq("TSPEC"),
    logcie_filter_level_min(LOGCIE_LEVEL_INFO)
  )
};

static void logcie_init(void) {
  tspec_sink.writer.data = stdout;
  logcie_add_sink(&tspec_sink);
}

int32_t main(int32_t argc, char **argv) {
  logcie_init();

  if (argc > 2) {
    fprintf(stderr, "usage: %s [file.tspec | directory]\n", argv[0]);
    fprintf(stderr, "  file.tspec  run a single spec file directly\n");
    fprintf(stderr, "  directory   recursively discover and run every *.tspec file under it (default: \"tests\")\n");
    return 1;
  }

  const char *target = (argc == 2) ? argv[1] : "tests";

  assert(target);

  struct stat target_st;

  if (stat(target, &target_st) != 0) {
    fprintf(stderr, "%s: %s\n", target, strerror(errno));
    return 1;
  }

  TspecOutcomeStats outcome       = {0};
  size_t            files_total   = 0;
  size_t            files_errored = 0;

  if (S_ISDIR(target_st.st_mode)) {
    TspecFileList list = {0};
    tspec_discover(target, &list);
    qsort(list.paths, list.count, TSPEC_MAX_PATH, tspec_path_cmp);

    if (list.count == 0) {
      LOGCIE_WARN("No .tspec files found under '%s'", target);
    }

    for (size_t i = 0; i < list.count; i++) {
      LOGCIE_INFO("==================== %s ====================", list.paths[i]);
      files_total++;

      TspecOutcomeStats file_outcome = {0};

      if (!tspec_run_file(list.paths[i], &file_outcome)) {
        files_errored++;
        LOGCIE_ERROR("'%s' did not complete (format/setup error)", list.paths[i]);
      }

      outcome.tests_run += file_outcome.tests_run;
      outcome.tests_passed += file_outcome.tests_passed;
      outcome.tests_failed += file_outcome.tests_failed;
    }
  } else {
    files_total = 1;

    if (!tspec_run_file(target, &outcome)) {
      files_errored = 1;
    }
  }

  tspec_print_summary(&outcome, files_total, files_errored);

  fflush(stderr);
  fflush(stdout);

  uint8_t all_ok = (files_errored == 0) && (outcome.tests_failed == 0);
  return all_ok ? 0 : 1;
}
