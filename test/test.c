#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

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

#ifndef LOGCIE_PRINT
#define LOGCIE_PRINT fprintf
#endif

#ifndef LOGCIE_VAPRINT
#define LOGCIE_VAPRINT vfprintf
#endif

typedef enum Logcie_LogLevel {
  LOGCIE_LEVEL_DEBUG,
  LOGCIE_LEVEL_VERBOSE,
  LOGCIE_LEVEL_INFO,
  LOGCIE_LEVEL_WARN,
  LOGCIE_LEVEL_ERROR,
  LOGCIE_LEVEL_FATAL,
} Logcie_LogLevel;

static char *logcie_level_label[] = {
  [LOGCIE_LEVEL_DEBUG]   = "debug",
  [LOGCIE_LEVEL_VERBOSE] = "verbose",
  [LOGCIE_LEVEL_INFO]    = "info",
  [LOGCIE_LEVEL_WARN]    = "warn",
  [LOGCIE_LEVEL_ERROR]   = "error",
  [LOGCIE_LEVEL_FATAL]   = "fatal",
};

static char *logcie_level_label_upper[] = {
  [LOGCIE_LEVEL_DEBUG]   = "DEBUG",
  [LOGCIE_LEVEL_VERBOSE] = "VERBOSE",
  [LOGCIE_LEVEL_INFO]    = "INFO",
  [LOGCIE_LEVEL_WARN]    = "WARN",
  [LOGCIE_LEVEL_ERROR]   = "ERROR",
  [LOGCIE_LEVEL_FATAL]   = "FATAL",
};

static char *logcie_level_color[] = {
  [LOGCIE_LEVEL_DEBUG]   = "\x1b[90m",         // gray
  [LOGCIE_LEVEL_VERBOSE] = "\x1b[90m",         // gray
  [LOGCIE_LEVEL_INFO]    = "\x1b[37m\x1b[44m", // white on blue
  [LOGCIE_LEVEL_WARN]    = "\x1b[30m\x1b[43m", // black on yellow
  [LOGCIE_LEVEL_ERROR]   = "\x1b[37m\x1b[41m", // white on red
  [LOGCIE_LEVEL_FATAL]   = "\x1b[37m\x1b[0;101m", // white on red
};

/**
  * Format:
  *  $$ - literally symbol $
  *  $m - log message
  *  $f - file
  *  $x - line
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

int logcie_printf_formatter(Logcie_Sink *sink, Logcie_Log log, const char *file, uint32_t line, ...) {
  va_list args;
  va_start(args, line);

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
        output_len += LOGCIE_VAPRINT(sink->sink, log.msg, args);
        break;
      case 'l':
        output_len += LOGCIE_PRINT(sink->sink, "%s", logcie_level_label[log.level]);
        break;
      case 'L':
        output_len += LOGCIE_PRINT(sink->sink, "%s", logcie_level_label_upper[log.level]);
        break;
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
      case 'f':
        output_len += LOGCIE_PRINT(sink->sink, "%s", file);
        break;
      case 'x':
        output_len += LOGCIE_PRINT(sink->sink, "%u", line);
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
  va_end(args);
  return output_len;
}

#define LOGCIE_CREATE_LOG(lvl, txt) (Logcie_Log) { .level = lvl, .msg = txt, }

#define LOGCIE_DEBUG(sink, msg, ...)   logcie_printf_formatter(&sink, LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg), __FILE__, __LINE__, __VA_ARGS__)
#define LOGCIE_VERBOSE(sink, msg, ...) logcie_printf_formatter(&sink, LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg), __FILE__, __LINE__, __VA_ARGS__)
#define LOGCIE_INFO(sink, msg, ...)    logcie_printf_formatter(&sink, LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg), __FILE__, __LINE__, __VA_ARGS__)
#define LOGCIE_WARN(sink, msg, ...)    logcie_printf_formatter(&sink, LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg), __FILE__, __LINE__, __VA_ARGS__)
#define LOGCIE_ERROR(sink, msg, ...)   logcie_printf_formatter(&sink, LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg), __FILE__, __LINE__, __VA_ARGS__)
#define LOGCIE_FATAL(sink, msg, ...)   logcie_printf_formatter(&sink, LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg), __FILE__, __LINE__, __VA_ARGS__)

// TODO: Align output up to character

int main(void) {
  Logcie_Sink sink = {
    .level = LOGCIE_LEVEL_DEBUG,
    .sink = stdout,
    .fmt = "$f:$x: $c$L$r: $d $t (GMT $z) $c[$l]$r: $m $$stdout"
  };

  LOGCIE_DEBUG(sink, "debugguy loggy %d %s", 1, "adsf");
  LOGCIE_VERBOSE(sink, "verbosesy loggy %d", 2);
  LOGCIE_INFO(sink, "infofofo loggy %d", 3);
  LOGCIE_WARN(sink, "warnny loggy %d", 4);
  LOGCIE_ERROR(sink, "errorry loggy %d", 5);
  LOGCIE_FATAL(sink, "fatallyly loggy %d", 6);

  return 0;
}
