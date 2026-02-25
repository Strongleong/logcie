#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LOGCIE_IMPLEMENTATION
#include "./logcie.h"

#define ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))

static const char *logcie_module = "build";
static Logcie_Sink stdout_sink = {
  .min_level = LOGCIE_LEVEL_INFO,
  .sink      = NULL,
  .fmt       = LOGCIE_COLOR_GRAY "[$M]$r $c$L$r:$<6$m",
  .formatter = logcie_printf_formatter,
};

#define UNUSED(value) (void)(value)
#define TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
#define UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

pid_t run_cmd(char *const argv[]) {
  pid_t cpid = fork();
  
  if (cpid < 0) {
     LOGCIE_ERROR("Could not fork child proces: %s", strerror(errno));
     return -1;
  }

  if (cpid > 0) {
     return cpid;
  }

  if (execvp(argv[0], argv) < 0) {
     LOGCIE_ERROR("Could not exec child process for %s: %s", argv[0], strerror(errno));
     exit(1);
  }

  UNREACHABLE("run_cmd");
}

void init_logcie(void) {
  stdout_sink.sink = stdout;
  logcie_add_sink(&stdout_sink);
}

size_t explode_len(char *str) {
  char *cur = str;
  size_t i = 0;

  while (cur++) {
    cur = strchr(cur, ' ');
    i++;
  }

  return i + 1;
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
  init_logcie();
  LOGCIE_INFO("Build system started...");  

  struct stat st = {0};

  if(stat("out", &st) == -1 && mkdir("out", 0755) == -1) {
    LOGCIE_FATAL("Can not create output directory out: %s", strerror(errno));
    return 1;
  }

  char *examples[] = {
    "cc -Wall -Wextra -std=c11 -I. -o out/custom_colors    examples/custom_colors.c",
    "cc -Wall -Wextra -std=c11 -I. -o out/custom_formatter examples/custom_formatter.c",
    "cc -Wall -Wextra -std=c11 -I. -o out/filters          examples/filters.c",
    "cc -Wall -Wextra -std=c11 -I. -o out/full             examples/full.c",
    "cc -Wall -Wextra -std=c99 -I. -o out/pedantic_99      examples/pedantic_99.c -pedantic",
    "cc -Wall -Wextra -std=c11 -I. -o out/simple           examples/simple.c",
    "cc -Wall -Wextra -std=c11 -I. -o out/sink             examples/sink.c",
  };

  for (size_t i = 0; i < ARR_LEN(examples); i++) {
    char *example = examples[i];
    LOGCIE_INFO("+ %s", example);

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
