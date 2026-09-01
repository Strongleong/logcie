#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>

// ERROR from windows clashes with logging macros in optly
#ifdef ERROR
#undef ERROR
#endif

#define mkdir(path, mode) _mkdir(path)
#define PATH_SEP          "\\"
#define PATH_LIST_SEP     ';'
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PATH_SEP      "/"
#define PATH_LIST_SEP ':'
#endif  // _WIN32

#define ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))

#define LOGCIE_MODULE "build"
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#define OPTLY_GEN_HELP_FLAG
#define OPTLY_GEN_HELP_COMMAND
#define OPTLY_IMPLEMENTATION
#include "./thirdparty/optly.h"

#define PATH_MAX_LEN 4096
#define CMD_MAX_ARGV 64
#define CMD_MAX_LINE 1024

// NOTE: strum is the reference .tspec runner, but the format is not tied to
// it. --tspec-runner exists so a second implementation can be dropped in
// without touching this file.
#define DEFAULT_TSPEC_RUNNER "strum"
#define STRUM_HOMEPAGE       "https://github.com/Strongleong/Strum"

static OptlyCommand command = {
  "build",
  NULL,
  .flags = optly_flags(
    optly_flag_bool("debug", 'd', "Compile with debug flags"),
    optly_flag_bool("silent", 's', "Compile without unnececary output"),
    optly_flag_string("outdir", 'o', "Set output dir", .value.as_string = "." PATH_SEP "out" PATH_SEP),
    optly_flag_string("c-compiler", 'c', "Set which C compier to use", .value.as_string = "clang"),
    optly_flag_string("cpp-compiler", 'x', "Set which C++ compier to use", .value.as_string = "clang++"),
    optly_flag_bool("dry-run", 'r', "Do not compile but show compile commands")
  ),
  .commands = optly_commands(
    optly_command(
      "tests",
      "Run the .tspec test suite",
      .flags = optly_flags(
        optly_flag_string("tspec-runner", 't', "Executable used to run .tspec files", .value.as_string = DEFAULT_TSPEC_RUNNER)
      ),
      .positionals = optly_positionals(
        optly_positional("target", "Passed through to the runner", .min = 0, .max = 0)
      )
    )
  ),
};

static Logcie_LogLevel stdout_sink_log_level = LOGCIE_LEVEL_INFO;

uint8_t stdout_sink_filter(const void *data, Logcie_Log *log) {
  (void)data;

  if (!log->module || strcmp(log->module, "build") != 0) {
    return false;
  }

  return logcie_filter_level_min_fn((void *)stdout_sink_log_level, log);
}

static Logcie_Sink stdout_sink = {
  .formatter = {logcie_token_formatter, LOGCIE_COLOR_GRAY "[$M]$r $c$L$r:$m"},
  .writer    = {logcie_file_writer, NULL},
  .filter    = {stdout_sink_filter, NULL}
};

static Logcie_Sink optly_sink = {
  .formatter = {logcie_token_formatter, LOGCIE_COLOR_GRAY "[$M]$r $c$L$r:$m"},
  .writer    = {logcie_file_writer, NULL},
  .filter    = logcie_filter_and(
    logcie_filter_module_eq("optly"),
    logcie_filter_level_min(LOGCIE_LEVEL_INFO)
  )
};

void setup_logcie(void) {
  stdout_sink.writer.data = stdout;
  optly_sink.writer.data  = stdout;

  // NOTE: the built-in sink is an ordinary sink and stays registered unless it
  // is removed. Leaving it in would print every line twice, once in its own
  // format and once in ours.
  logcie_remove_sink(logcie_get_default_sink());

  logcie_add_sink(&stdout_sink);
  logcie_add_sink(&optly_sink);

  LOGCIE_INFO("Build system started...");
}

typedef struct {
  char  path[PATH_MAX_LEN];
  char *name;
} WalkEntry;

static bool walk_dir(const char *dir, bool (*cb)(const WalkEntry *entry, void *user), void *user) {
#ifdef _WIN32
  WIN32_FIND_DATA fd;
  HANDLE          hFind;
  char            search[PATH_MAX_LEN];
  snprintf(search, sizeof(search), "%s\\*", dir);

  hFind = FindFirstFile(search, &fd);
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
      continue;
    }

    WalkEntry entry;
    snprintf(entry.path, sizeof(entry.path), "%s\\%s", dir, fd.cFileName);
    entry.name = entry.path + strlen(dir) + 1;  // skip the dir part + '\'

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (!walk_dir(entry.path, cb, user)) {
        FindClose(hFind);
        return false;
      }

      continue;
    }

    if (!cb(&entry, user)) {
      FindClose(hFind);
      return false;
    }
  } while (FindNextFile(hFind, &fd));

  FindClose(hFind);
  return true;
#else
  DIR *d = opendir(dir);

  if (!d) {
    return false;
  }

  struct dirent *e;

  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
      continue;
    }

    WalkEntry entry;
    snprintf(entry.path, sizeof(entry.path), "%s/%s", dir, e->d_name);
    entry.name = entry.path + strlen(dir) + 1;

    struct stat st;
    if (stat(entry.path, &st) == -1) {
      continue;
    }

    // NOTE: a directory is descended into and never also handed to the
    // callback. Doing both would try to compile a directory named foo.c.
    if (S_ISDIR(st.st_mode)) {
      if (!walk_dir(entry.path, cb, user)) {
        closedir(d);
        return false;
      }

      continue;
    }

    if (!cb(&entry, user)) {
      closedir(d);
      return false;
    }
  }
  closedir(d);
  return true;
#endif
}

#define UNUSED(value) (void)(value)
#define TODO(message)                                                  \
  do {                                                                 \
    fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); \
    abort();                                                           \
  } while (0)

#define UNREACHABLE(message)                                                  \
  do {                                                                        \
    fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); \
    abort();                                                                  \
  } while (0)

pid_t start_process(char *const argv[]) {
#ifdef _WIN32
  intptr_t result = _spawnvp(_P_NOWAIT, argv[0], (const char *const *)argv);

  if (result == -1) {
    LOGCIE_ERROR("Failed to spawn process %s: %s", argv[0], strerror(errno));
  }

  return result;
#else
  pid_t cpid = fork();

  if (cpid < 0) {
    LOGCIE_ERROR("Could not fork child proces: %s", strerror(errno));
    return -1;
  }

  if (cpid > 0) {
    return cpid;
  }

  if (execvp(argv[0], argv) < 0) {
    fprintf(stderr, "Could not exec child process for %s: %s\n", argv[0], strerror(errno));
    exit(1);
  }

  UNREACHABLE("run_cmd");
#endif
  return -1;
}

bool wait_cmd(intptr_t pid) {
  if (pid == -1) return false;

#ifdef _WIN32
  HANDLE hProcess = (HANDLE)pid;
  DWORD  result   = WaitForSingleObject(hProcess, INFINITE);

  if (result == WAIT_FAILED) {
    LOGCIE_ERROR("Could not wait on child process");
    return false;
  }

  DWORD exit_status;

  if (!GetExitCodeProcess(hProcess, &exit_status)) {
    LOGCIE_ERROR("Could not get process exit code");
    return false;
  }

  if (exit_status != 0) {
    LOGCIE_ERROR("Command exited with exit code %lu", exit_status);
    return false;
  }

  CloseHandle(hProcess);
#else
  while (true) {
    int wstatus = 0;

    if (waitpid(pid, &wstatus, 0) < 0) {
      LOGCIE_ERROR("Could not wait on command (pid: %ld): %s", (long)pid, strerror(errno));
      return false;
    }

    if (WIFEXITED(wstatus)) {
      int exit_code = WEXITSTATUS(wstatus);

      if (exit_code != 0) {
        LOGCIE_ERROR("Command exited with exit code %d", exit_code);
        return false;
      }

      break;
    }

    if (WIFSIGNALED(wstatus)) {
      LOGCIE_ERROR("Command process was terminated by signal %d", WTERMSIG(wstatus));
      return false;
    }
  }
#endif

  return true;
}

bool run_cmd(char *const argv[]) {
  // NOTE: the child shares this stdout. Without a flush its output can
  // overtake anything buffered here but not yet written.
  fflush(stdout);
  fflush(stderr);

  return wait_cmd(start_process(argv));
}

int dir_exists(const char *path) {
#ifdef _WIN32
  DWORD attr = GetFileAttributesA(path);
  return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

size_t cmd_to_string(char *const *cmd, char *str, size_t cap) {
  size_t len = 0;

  for (char *const *c = cmd; *c; c++) {
    int written = snprintf(str + len, cap - len, "%s ", *c);

    if (written < 0 || (size_t)written >= cap - len) {
      break;
    }

    len += (size_t)written;
  }

  return len;
}

static bool is_executable(const char *path) {
#ifdef _WIN32
  return _access(path, 0) == 0;
#else
  return access(path, X_OK) == 0;
#endif
}

// NOTE: a name containing a separator is taken as written, a bare name is
// looked up on PATH. That is the rule execvp already follows, so
// `--tspec-runner strum` and `--tspec-runner ./out/strum` behave the way they
// would if typed into a shell.
static bool find_executable(const char *name, char *out, size_t out_cap) {
  if (strchr(name, PATH_SEP[0]) != NULL) {
    snprintf(out, out_cap, "%s", name);
    return is_executable(out);
  }

  const char *path = getenv("PATH");

  if (!path) {
    return false;
  }

  while (*path) {
    const char *sep = strchr(path, PATH_LIST_SEP);
    size_t      len = sep ? (size_t)(sep - path) : strlen(path);

    if (len > 0) {
      snprintf(out, out_cap, "%.*s" PATH_SEP "%s", (int)len, path, name);

      if (is_executable(out)) {
        return true;
      }
    }

    if (!sep) {
      break;
    }

    path = sep + 1;
  }

  return false;
}

static bool run_tests(OptlyCommand *tests) {
  const char *runner = optly_flag_value_string(tests, "tspec-runner");
  char        resolved[PATH_MAX_LEN];

  if (!find_executable(runner, resolved, sizeof(resolved))) {
    LOGCIE_FATAL("Could not find a .tspec runner named '%s'", runner);

    if (strcmp(runner, DEFAULT_TSPEC_RUNNER) == 0) {
      LOGCIE_INFO("strum is the reference runner: " STRUM_HOMEPAGE);
      LOGCIE_INFO("Build it and put it on PATH, or point --tspec-runner at the binary");
    } else {
      LOGCIE_INFO("Check the path, or drop --tspec-runner to use " DEFAULT_TSPEC_RUNNER);
    }

    return false;
  }

  LOGCIE_VERBOSE("Using tspec runner %s", resolved);

  char *cmd[CMD_MAX_ARGV] = {0};
  int   i                 = 0;

  cmd[i++] = resolved;

  // NOTE: anything positional goes straight to the runner, so
  // `./build tests tests/filters` narrows a run without this file needing to
  // know what a target is.
  OptlyPositional *targets = optly_get_positional(tests, "target");

  if (targets) {
    for (size_t t = 0; t < targets->count && i < CMD_MAX_ARGV - 1; t++) {
      cmd[i++] = targets->values[t];
    }
  }

  cmd[i] = NULL;

  char line[CMD_MAX_LINE] = {0};
  cmd_to_string(cmd, line, sizeof(line));
  LOGCIE_INFO("%s", line);

  if (optly_flag_value_bool(&command, "dry-run")) {
    return true;
  }

  return run_cmd(cmd);
}

static char outdir[PATH_MAX_LEN] = {0};

// NOTE: one example directory is one program. 07_app is several .c files that
// must be compiled together, and naming the binary after the directory keeps
// ./out flat.
typedef struct {
  char *sources[CMD_MAX_ARGV];
  int   count;
  bool  cpp;
} ExampleSources;

static bool collect_source(const WalkEntry *entry, void *user) {
  ExampleSources *out = (ExampleSources *)user;
  char           *ext = strrchr(entry->name, '.');

  if (!ext) {
    return true;
  }

  bool cpp = strcmp(ext, ".cpp") == 0;

  if (!cpp && strcmp(ext, ".c") != 0) {
    return true;
  }

  if (out->count >= CMD_MAX_ARGV - 16) {
    LOGCIE_ERROR("Too many source files in one example");
    return false;
  }

  out->cpp = out->cpp || cpp;

  size_t len  = strlen(entry->path) + 1;
  char  *copy = (char *)malloc(len);

  if (!copy) {
    LOGCIE_FATAL("Out of memory");
    return false;
  }

  snprintf(copy, len, "%s", entry->path);
  out->sources[out->count++] = copy;
  return true;
}

// NOTE: an example that needs extra compiler or linker flags puts them in a
// build.flags file next to its sources, one per line or space separated. Only
// 09_threads needs this today, for -lpthread, and linking pthreads into every
// example to serve one of them would be a lie about what the rest require.
static int read_build_flags(const char *dir, char *storage, size_t cap, char **out, int max) {
  char path[PATH_MAX_LEN];
  snprintf(path, sizeof(path), "%s" PATH_SEP "build.flags", dir);

  FILE *f = fopen(path, "r");

  if (!f) {
    return 0;
  }

  size_t len = fread(storage, 1, cap - 1, f);
  fclose(f);

  storage[len] = '\0';

  int   count = 0;
  char *token = strtok(storage, " \t\r\n");

  while (token && count < max) {
    out[count++] = token;
    token        = strtok(NULL, " \t\r\n");
  }

  return count;
}

static bool build_self() {
  LOGCIE_VERBOSE("Building self");

  char *cmd[9] = {0};
  int   i      = 0;

  cmd[i++] = optly_flag_value_string(&command, "c-compiler");
  cmd[i++] = "-Wall";
  cmd[i++] = "-Wextra";
  cmd[i++] = "-std=c11";
  cmd[i++] = "-I.";
  cmd[i++] = "./build.c";
  cmd[i++] = "-o";
  cmd[i++] = "./build";
  cmd[i++] = NULL;

  char line[CMD_MAX_LINE] = {0};
  cmd_to_string(cmd, line, sizeof(line));
  LOGCIE_INFO("%s", line);

  return run_cmd(cmd);
}

static bool build_example(const char *dir, const char *name) {
  ExampleSources found;
  found.count = 0;
  found.cpp   = false;

  if (!walk_dir(dir, collect_source, &found)) {
    return false;
  }

  if (found.count == 0) {
    LOGCIE_VERBOSE("No sources in %s, skipping", dir);
    return true;
  }

  LOGCIE_VERBOSE("Building example %s", name);

  char out[PATH_MAX_LEN];
  snprintf(out, sizeof(out), "%s%s", outdir, name);

  char *cmd[CMD_MAX_ARGV] = {0};
  int   i                 = 0;

  cmd[i++] = found.cpp ? optly_flag_value_string(&command, "cpp-compiler")
                       : optly_flag_value_string(&command, "c-compiler");
  cmd[i++] = "-Wall";
  cmd[i++] = "-Wextra";
  cmd[i++] = found.cpp ? "-std=c++11" : "-std=c99";

  if (optly_flag_value_bool(&command, "debug")) {
    cmd[i++] = "-ggdb";
#ifndef _WIN32
    cmd[i++] = "-fsanitize=address";
#endif
    cmd[i++] = "-fno-omit-frame-pointer";
    cmd[i++] = "-DLOGCIE_DEBUG_CHECKS";
    cmd[i++] = "-Og";
  } else {
    cmd[i++] = "-O3";
  }

  cmd[i++] = "-I.";

  for (int f = 0; f < found.count; f++) {
    cmd[i++] = found.sources[f];
  }

  cmd[i++] = "-o";
  cmd[i++] = out;

  char  flag_storage[PATH_MAX_LEN];
  char *flags[16];
  int   flag_count = read_build_flags(dir, flag_storage, sizeof(flag_storage), flags, 16);

  for (int f = 0; f < flag_count && i < CMD_MAX_ARGV - 1; f++) {
    cmd[i++] = flags[f];
  }

  cmd[i++] = NULL;

  char line[CMD_MAX_LINE] = {0};
  cmd_to_string(cmd, line, sizeof(line));
  LOGCIE_INFO("%s", line);

  bool ok = optly_flag_value_bool(&command, "dry-run") || run_cmd(cmd);

  for (int f = 0; f < found.count; f++) {
    free(found.sources[f]);
  }

  return ok;
}

// NOTE: the directories are numbered to be read in order, so the build log
// should follow that order rather than whatever the filesystem hands back.
static int compare_names(const void *a, const void *b) {
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}

#define MAX_EXAMPLES 64

static bool build_examples(void) {
  const char *root = "." PATH_SEP "examples";
  bool        all  = true;

  char  names[MAX_EXAMPLES][PATH_MAX_LEN];
  char *order[MAX_EXAMPLES];
  int   count = 0;

#ifdef _WIN32
  WIN32_FIND_DATA fd;
  char            search[PATH_MAX_LEN];
  snprintf(search, sizeof(search), "%s\\*", root);

  HANDLE hFind = FindFirstFile(search, &fd);

  if (hFind == INVALID_HANDLE_VALUE) {
    LOGCIE_ERROR("Could not open %s", root);
    return false;
  }

  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      continue;
    }

    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
      continue;
    }

    if (count < MAX_EXAMPLES) {
      snprintf(names[count], PATH_MAX_LEN, "%s", fd.cFileName);
      order[count] = names[count];
      count++;
    }
  } while (FindNextFile(hFind, &fd));

  FindClose(hFind);
#else
  DIR *d = opendir(root);

  if (!d) {
    LOGCIE_ERROR("Could not open %s: %s", root, strerror(errno));
    return false;
  }

  struct dirent *e;

  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] == '.') {
      continue;
    }

    char dir[PATH_MAX_LEN];
    snprintf(dir, sizeof(dir), "%s/%s", root, e->d_name);

    struct stat st;

    if (stat(dir, &st) == -1 || !S_ISDIR(st.st_mode)) {
      continue;
    }

    if (count < MAX_EXAMPLES) {
      snprintf(names[count], PATH_MAX_LEN, "%s", e->d_name);
      order[count] = names[count];
      count++;
    }
  }

  closedir(d);
#endif

  qsort(order, (size_t)count, sizeof(*order), compare_names);

  for (int i = 0; i < count; i++) {
    char dir[PATH_MAX_LEN];
    snprintf(dir, sizeof(dir), "%s" PATH_SEP "%s", root, order[i]);
    all = build_example(dir, order[i]) && all;
  }

  return all;
}

int main(int argc, char *argv[]) {
  setup_logcie();
  optly_parse_args(argc, argv, &command);

  if (optly_flag_value_bool(&command, "silent")) {
    stdout_sink_log_level = LOGCIE_LEVEL_WARN;
  }

  if (!build_self()) {
    return 1;
  }

  if (optly_is_command(command.next_command, "tests")) {
    return run_tests(command.next_command) ? 0 : 1;
  }

  snprintf(outdir, sizeof(outdir), "%s", optly_flag_value_string(&command, "outdir"));
  size_t outdir_len = strlen(outdir);

  if (outdir_len > 0 && outdir[outdir_len - 1] != PATH_SEP[0]) {
    outdir[outdir_len++] = PATH_SEP[0];
    outdir[outdir_len]   = '\0';
  }

  if (!dir_exists(outdir)) {
    LOGCIE_WARN("Output direcotry does not exist. Creating...");

    if (mkdir(outdir, 0755) == -1) {
      LOGCIE_FATAL("Can not create output directory %s: %s", outdir, strerror(errno));
      return 1;
    }
  }

  return build_examples() ? 0 : 1;
}
