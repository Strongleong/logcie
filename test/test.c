#include <stdio.h>
#include <time.h>

#if defined(__STDC__)
# define PREDEF_STANDARD_C_1989
# if defined(__STDC_VERSION__)
#  define PREDEF_STANDARD_C_1990
#  if (__STDC_VERSION__ >= 199409L)
#   define PREDEF_STANDARD_C_1994
#  endif
#  if (__STDC_VERSION__ >= 199901L)
#   define PREDEF_STANDARD_C_1999
#  endif
#  if (__STDC_VERSION__ >= 201112L)
#    define PREDEF_STANDARD_C_2011
#  endif
#  if (__STDC_VERSION__ >= 201710L)
#   define PREDEF_STANDARD_C_2018
#  endif
# endif
#endif

#define LOGCIE_PRINT fprintf

typedef enum Logcie_LogLevel {
  LEVEL_DEBUG,
  LEVEL_VERBOSE,
  LEVEL_INFO,
  LEVEL_WARN,
  LEVEL_ERROR,
  LEVEL_FATAL,
} Logcie_LogLevel;

static char *logcie_level_label[] = {
  [LEVEL_DEBUG]   = "debug",
  [LEVEL_VERBOSE] = "verbose",
  [LEVEL_INFO]    = "info",
  [LEVEL_WARN]    = "warn",
  [LEVEL_ERROR]   = "error",
  [LEVEL_FATAL]   = "fatal",
};

static char *logcie_level_color[] = {
  [LEVEL_DEBUG]   = "\x1b[90m",         // gray
  [LEVEL_VERBOSE] = "\x1b[90m",         // gray
  [LEVEL_INFO]    = "\x1b[37m\x1b[44m", // white on blue
  [LEVEL_WARN]    = "\x1b[37m\x1b[33m", // white on yellow
  [LEVEL_ERROR]   = "\x1b[37m\x1b[91m", // white on red
  [LEVEL_FATAL]   = "\x1b[37m\x1b[91m", // white on red
};

/**
  * Format:
  *  $$ - literally symbol $
  *  $m - log message
  *  $l - log level (info, debug, warn)
  *  $L - log level in upper case (INFO, DEBUG, WARN)
  *  $d - current date (YYYY-MM-DD)
  *  $t - current time (H:i:m)
  *  $z - current time zone
  *  $c - color marker of log level
  *  $r - color reset
  */
typedef struct Logcie_Sink {
  FILE *sink;
  Logcie_LogLevel level;
  const char *fmt;
} Logcie_Sink;

typedef struct Logcie_Log {
  Logcie_LogLevel level;
  const char *msg;
  time_t time;
} Logcie_Log;

typedef struct Logcie_Logger {
  Logcie_Sink *sinks;
  size_t sinks_len;
  size_t sinks_cap;
} Logcie_Logger;

int logcie_log(Logcie_Sink *sink, Logcie_Log log) {
  if (log.level < sink->level) {
    return 0;
  }

  int output_len = 0;
  const char *fmt = sink->fmt;

  struct tm *tm = localtime(&log.time);
  int local_hours = tm->tm_hour;
  tm = gmtime(&log.time);
  int timediff = local_hours - tm->tm_hour;

  while (*fmt != '\0') {
    if (*fmt != '$') {
      output_len += LOGCIE_PRINT(sink->sink, "%c", *fmt);
      fmt++;
      continue;
    }

    fmt++;

    if (*fmt == '\0') {
      break;
    }

    switch (*fmt) {
      case '$':
        output_len += LOGCIE_PRINT(sink->sink, "$");
        break;
      case 'm':
        output_len += LOGCIE_PRINT(sink->sink, "%s", log.msg);
        break;
      case 'l':
        output_len += LOGCIE_PRINT(sink->sink, "%s", logcie_level_label[log.level]);
        break;
      /* case 'L': *out = 'L'; break; */
      case 'd':
        output_len += LOGCIE_PRINT(sink->sink, "%d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        break;
      case 't':
        output_len += LOGCIE_PRINT(sink->sink, "%02d:%02d:%02d", local_hours, tm->tm_min, tm->tm_sec);
        break;
      case 'z':
        output_len += LOGCIE_PRINT(sink->sink, "%c%d", timediff > 0 ? '+' : '-', timediff);
        break;
      case 'c':
        output_len += LOGCIE_PRINT(sink->sink, "%s", logcie_level_color[log.level]);
        break;
      case 'r':
        output_len += LOGCIE_PRINT(sink->sink, "\x1b[0m");
        break;
      default: break;
      // TODO: Add reporing of unknown log operator
      // TODO: Come up with actial name of those "log operators" thingy
    }

    fmt++;
  }

  output_len += LOGCIE_PRINT(sink->sink, "\n");
  return output_len;
}

int main(void) {
  Logcie_Sink sink = {
    .level = LEVEL_DEBUG,
    .sink = stdout,
    .fmt = "$d $t (GMT $z) $c[$l]$r: $m @stdout"
  };

  logcie_log(&sink, (Logcie_Log){ .level = LEVEL_DEBUG,   .msg = "debug",   .time = time(NULL) });
  logcie_log(&sink, (Logcie_Log){ .level = LEVEL_VERBOSE, .msg = "verbose", .time = time(NULL) });
  logcie_log(&sink, (Logcie_Log){ .level = LEVEL_INFO,    .msg = "info",    .time = time(NULL) });
  logcie_log(&sink, (Logcie_Log){ .level = LEVEL_WARN,    .msg = "warn",    .time = time(NULL) });
  logcie_log(&sink, (Logcie_Log){ .level = LEVEL_ERROR,   .msg = "error",   .time = time(NULL) });
  logcie_log(&sink, (Logcie_Log){ .level = LEVEL_FATAL,   .msg = "fatal",   .time = time(NULL) });

  return 0;
}
