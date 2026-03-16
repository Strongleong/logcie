#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

#ifdef _WIN32

#include <windows.h>
#include <direct.h>
#include <process.h>

typedef intptr_t pid_t;

#define mkdir(path, mode) _mkdir(path)
#define PATH_SEP "\\"

#else

#include <unistd.h>
#include <sys/types.h>

#define PATH_SEP "/"

#endif

#define ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))

#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

#define OPTLY_IMPLEMENTATION
#include "./thirdparty/optly.h"

#define ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))

// static const char *logcie_module = "build";
//
// static Logcie_Sink stdout_sink = {
//   .min_level = LOGCIE_LEVEL_INFO,
//   .fmt       = LOGCIE_COLOR_GRAY "[$M]$r $c$L$r:$<6$m",
//   .formatter = { logcie_printf_formatter,  NULL },
// };

#define UNUSED(value) (void)(value)
#define TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
// #define UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

pid_t run_cmd(char *const argv[]) {
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

  // UNREACHABLE("run_cmd");
#endif
  return -1;
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

size_t explode_len(char *str) {
  size_t count = 1;

  while (*str) {
    if (*str == ' ') {
      count++;
    }

    str++;
  }

  return count + 1;
}

void explode(char *str, char **arr) {
    char *cur = str;
    size_t i = 0;

    while (cur && *cur) {
        arr[i++] = cur;

        char *delim = strchr(cur, ' ');
        if (!delim) break;

        *delim = '\0';
        cur = delim + 1;

        while (*cur == ' ') cur++;
    }

    arr[i] = NULL;
}

int main(void) {
  LOGCIE_INFO("Build system started...");

  struct stat st = {0};

  if(stat("out", &st) == -1 && mkdir("out", 0755) == -1) {
    fprintf(stderr, "Can not create output directory out: %s\n", strerror(errno));
    return 1;
  }

  char *examples[] = {
    "clang -Wall -Wextra -std=c11 -I. -o out"PATH_SEP"custom_colors    examples"PATH_SEP"custom_colors.c",
    "clang -Wall -Wextra -std=c11 -I. -o out"PATH_SEP"custom_formatter examples"PATH_SEP"custom_formatter.c",
    "clang -Wall -Wextra -std=c11 -I. -o out"PATH_SEP"filters          examples"PATH_SEP"filters.c",
    "clang -Wall -Wextra -std=c11 -I. -o out"PATH_SEP"full             examples"PATH_SEP"full.c",
    "clang -Wall -Wextra -std=c99 -I. -o out"PATH_SEP"pedantic_99      examples"PATH_SEP"pedantic_99.c -pedantic",
    "clang -Wall -Wextra -std=c11 -I. -o out"PATH_SEP"simple           examples"PATH_SEP"simple.c",
    "clang -Wall -Wextra -std=c11 -I. -o out"PATH_SEP"sink             examples"PATH_SEP"sink.c",
  };

  for (size_t i = 0; i < ARR_LEN(examples); i++) {
    char *example = examples[i];
    fprintf(stdout, "+ %s\n", example);

    char buf[256] = {0};
    strncpy(buf, example, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    size_t len = explode_len(example);
    char *cmd[len];
    explode(buf, cmd);

    run_cmd(cmd);
  }

  return 0;
}
