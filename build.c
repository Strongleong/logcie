#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef _WIN32

#include <direct.h>
#include <process.h>
#include <windows.h>

typedef intptr_t pid_t;

#define mkdir(path, mode) _mkdir(path)
#define PATH_SEP          "\\"

#else

#include <sys/types.h>
#include <unistd.h>

#define PATH_SEP "/"

#endif

#define ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))

#define LOGCIE_MODULE "build"
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#define OPTLY_GEN_HELP_FLAG
#define OPTLY_IMPLEMENTATION
#include "./thirdparty/optly.h"

#define ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))

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
};

static Logcie_LogLevel stdout_sink_log_level = LOGCIE_LEVEL_INFO;

uint8_t stdout_sink_filter(const void *data, Logcie_Log *log) {
  (void)data;
  if (strcmp(log->module, "build") != 0) {
    return false;
  }

  return logcie_filter_level_min_fn((void *)stdout_sink_log_level, log);
}

static Logcie_Sink stdout_sink = {
  .formatter = {logcie_printf_formatter, LOGCIE_COLOR_GRAY "(build) [$M]$r $c$L$r:$<6$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {stdout_sink_filter, NULL}
};

static Logcie_Sink optly_sink = {
  .formatter = {logcie_printf_formatter, LOGCIE_COLOR_GRAY "(optly) [$M]$r $c$L$r:$<6$m"},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = logcie_filter_and(
    logcie_filter_module_eq("optly"),
    logcie_filter_level_min(LOGCIE_LEVEL_INFO)
  )
};

void setup_logcie(void) {
  stdout_sink.writer.data = stdout;
  optly_sink.writer.data  = stdout;

  logcie_add_sink(&stdout_sink);
  logcie_add_sink(&optly_sink);

  LOGCIE_INFO("Build system started...");
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
    fprintf(stderr, "Failed to spawn process %s: %s\n", argv[0], strerror(errno));
  }

  return result;
#else
  pid_t cpid = fork();

  if (cpid < 0) {
    fprintf(stderr, "Could not fork child proces: %s\n", strerror(errno));
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

bool wait_cmd(pid_t pid) {
  if (pid == -1) return false;

#ifdef _WIN32
  DWORD result = WaitForSingleObject(pid, INFINITE);

  if (result == WAIT_FAILED) {
    LOGCIE_ERROR("Could not wait on child process");
    return false;
  }

  DWORD exit_status;

  if (!GetExitCodeProcess(pid, &exit_status)) {
    LOGCIE_ERROR("Could not get process exit code");
    return false;
  }

  if (exit_status != 0) {
    LOGCIE_ERROR("Command exited with exit code %lu", exit_status);
    return false;
  }

  CloseHandle(pid);
#else
  while (true) {
    int wstatus = 0;

    if (waitpid(pid, &wstatus, 0) < 0) {
      LOGCIE_ERROR("Could not wait on command (pid: %d) :%s", pid, strerror(errno));
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

size_t cmd_to_string(char **cmd, char *str) {
  size_t len = 0;

  for (char **c = cmd; *c; c++) {
    len += snprintf(str + len, 250, "%s ", *c);
  }

  return len;
}

#define PATH_MAX_LEN 4096
static char outdir[PATH_MAX_LEN] = {0};

int main(int argc, char *argv[]) {
  setup_logcie();
  optly_parse_args(argc, argv, &command);

  if (optly_flag_value_bool(&command, "silent")) {
    stdout_sink_log_level = LOGCIE_LEVEL_WARN;
  }

  strncpy(outdir, optly_flag_value_string(&command, "outdir"), PATH_MAX_LEN);
  size_t outdir_len = strlen(outdir);

  if (strcmp(&outdir[outdir_len - 1], PATH_SEP) != 0) {
    outdir[outdir_len] = PATH_SEP[0];
  }

  if (!dir_exists(outdir)) {
    LOGCIE_WARN("Output direcotry does not exist. Creating...");

    if (mkdir(outdir, 0755) == -1) {
      LOGCIE_FATAL("Can not create output directory %s: %s", outdir, strerror(errno));
      return 1;
    }
  }

  char   *sources[]   = {"./examples/", "./test.c", NULL};
  FTSENT *node        = NULL;
  FTS    *file_system = fts_open(sources, FTS_NOCHDIR, NULL);

  if (!file_system) {
    LOGCIE_FATAL("Can not open file system: %s", strerror(errno));
    return 1;
  }

  while ((node = fts_read(file_system)) != NULL) {
    switch (node->fts_info) {
      case FTS_ERR:
      case FTS_NS:
        LOGCIE_ERROR("Could not read file %s: %s", node->fts_accpath, strerror(node->fts_errno));
        break;
      case FTS_F: {
        LOGCIE_VERBOSE("Compiling file %s", node->fts_name);
        char *ext = strrchr(node->fts_name, '.');

        if (!ext) {
          break;
        }

        size_t out_len = node->fts_namelen + strlen(outdir) - strlen(ext) + 1;
        char   out[out_len];
        size_t in_len = node->fts_pathlen + node->fts_namelen + 1;
        char   in[in_len];

        bool cpp = strcmp(ext, ".cpp") == 0;

        if (!cpp && strcmp(ext, ".c") != 0) {
          continue;
        }

        char *cmd[32] = {0};
        int   i       = 0;

        cmd[i++] = cpp ? optly_flag_value_string(&command, "cpp-compiler")
                       : optly_flag_value_string(&command, "c-compiler");
        cmd[i++] = "-Wall";
        cmd[i++] = "-Wextra";
        cmd[i++] = cpp ? "-std=c++11" : "-std=c99";

        if (optly_flag_value_bool(&command, "debug")) {
          cmd[i++] = "-O3";
          cmd[i++] = "-ggdb";
          cmd[i++] = "-fsanitize=address";
          cmd[i++] = "-fno-omit-frame-pointer";
          cmd[i++] = "-D_LOGCIE_DEBUG";
          cmd[i++] = "-Og";
        } else {
          cmd[i++] = "-O3";
        }

        cmd[i++] = "-I.";
        cmd[i++] = in;
        cmd[i++] = "-o";
        cmd[i++] = out;
        cmd[i++] = NULL;

        snprintf(out, out_len, "%s%.*s", outdir, (int)(ext - node->fts_name), node->fts_name);
        snprintf(in, in_len, "%s", node->fts_accpath);

        char buf[256] = {0};
        cmd_to_string(cmd, buf);
        LOGCIE_INFO("%s", buf);

        if (!optly_flag_value_bool(&command, "dry-run")) {
          run_cmd(cmd);
        }

        break;
      }
    }
  }

  fts_close(file_system);
  return 0;
}
