/*
Logcie v0.0.1

Single header only library in C
*/

#ifndef LOGCIE
#define LOGCIE

#ifndef LOGCIE_DEF
#define LOGCIE_DEF extern
#endif

#define LOGCIE_VERSION_MAJOR    0
#define LOGCIE_VERSION_MINOR    0
#define LOGCIE_VERSION_RELEASE  1
#define LOGCIE_VERSION_NUMBER (LOGCIE_VERSION_MAJOR *100*100 + LOGCIE_VERSION_MINOR *100 + LOGCIE_VERSION_RELEASE)

#define LOGCIE_VERSION_FULL LOGCIE_VERSION_MAJOR.LOGCIE_VERSION_MINOR.LOGCIE_VERSION_RELEASE
#define LOGCIE_QUOTE(str) #str
#define LOGCIE_EXPAND_AND_QUOTE(str) LOGCIE_QUOTE(str)
#define LOGCIE_VERSION_STRING LOGCIE_EXPAND_AND_QUOTE(LOGCIE_VERSION_FULL)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOGCIE_PRINT
#define LOGCIE_PRINT fprintf
#endif

#ifndef LOGCIE_VAPRINT
#define LOGCIE_VAPRINT vfprintf
#endif

#ifndef LOGCIE_NO_STD
# include <stdio.h>
# include <time.h>
# include <stdint.h>
# include <stdarg.h>
# if (__STDC_VERSION__ < 201710L)
#  include <stdbool.h>
# endif
#endif

#if defined(LOGCIE_NO_STD) && !defined(LOGCIE_PRINT) && !defined(LOGCIE_VAPRINT)
# error "You must provide implementation of std shit and stuff yo bitch"
#endif

typedef enum Logcie_LogLevel {
  LOGCIE_LEVEL_TRACE,
  LOGCIE_LEVEL_DEBUG,
  LOGCIE_LEVEL_VERBOSE,
  LOGCIE_LEVEL_INFO,
  LOGCIE_LEVEL_WARN,
  LOGCIE_LEVEL_ERROR,
  LOGCIE_LEVEL_FATAL,
  Count_LOGCICE_LEVEL,
} Logcie_LogLevel;

#ifdef __GNUC__
 static const char __attribute__ ((unused)) *logcie_module;
#else
 static const char *logcie_module;
#endif

typedef struct Logcie_Sink Logcie_Sink;
typedef struct Logcie_Log Logcie_Log;
#define LOGCIE_FOMRATTER_PARAMS Logcie_Sink *sink, Logcie_Log log, const char *file, uint32_t line, va_list *args
typedef int (Logcie_FomratterFn)(LOGCIE_FOMRATTER_PARAMS);

/**
  * Format sequences:
  *  $$ - literally symbol $
  *  $m - log message
  *  $f - file
  *  $x - line
  *  $M - module
  *  $l - log level (info, debug, warn)
  *  $L - log level in upper case (INFO, DEBUG, WARN)
  *  $d - current date (YYYY-MM-DD)
  *  $t - current time (H:i:m)
  *  $z - current time zone
  *  $| - align up to this delimiter
  */
struct Logcie_Sink {
  FILE *sink;
  Logcie_LogLevel level;
  const char *fmt;
  Logcie_FomratterFn *formatter;
  bool color;
};

struct Logcie_Log {
  Logcie_LogLevel level;
  const char *msg;
  time_t time;
  const char *module;
};

#define LOGCIE_CREATE_LOG(lvl, txt) (Logcie_Log) { .level = lvl, .msg = txt, .time = time(NULL), .module = logcie_module }

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202000L)
# define LOGCIE_TRACE(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_DEBUG(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_VERBOSE(msg, ...) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_INFO(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_WARN(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_ERROR(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_FATAL(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg), __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
#else
# if !defined(LOGCIE_PEDANTIC) && (defined(__GNUC__) || defined (__clang__))
#  define LOGCIE_TRACE(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg), __FILE__, __LINE__, ##__VA_ARGS__)
#  define LOGCIE_DEBUG(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg), __FILE__, __LINE__, ##__VA_ARGS__)
#  define LOGCIE_VERBOSE(msg, ...) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg), __FILE__, __LINE__, ##__VA_ARGS__)
#  define LOGCIE_INFO(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg), __FILE__, __LINE__, ##__VA_ARGS__)
#  define LOGCIE_WARN(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg), __FILE__, __LINE__, ##__VA_ARGS__)
#  define LOGCIE_ERROR(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg), __FILE__, __LINE__, ##__VA_ARGS__)
#  define LOGCIE_FATAL(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg), __FILE__, __LINE__, ##__VA_ARGS__)
# else
#  define LOGCIE_TRACE(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg), __FILE__, __LINE__)
#  define LOGCIE_DEBUG(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg), __FILE__, __LINE__)
#  define LOGCIE_VERBOSE(msg) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg), __FILE__, __LINE__)
#  define LOGCIE_INFO(msg)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg), __FILE__, __LINE__)
#  define LOGCIE_WARN(msg)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg), __FILE__, __LINE__)
#  define LOGCIE_ERROR(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg), __FILE__, __LINE__)
#  define LOGCIE_FATAL(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg), __FILE__, __LINE__)
#  define LOGCIE_VA_LOGS
# endif
#endif

#ifdef LOGCIE_VA_LOGS
#  define LOGCIE_TRACE_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg), __FILE__, __LINE__, __VA_ARGS__)
#  define LOGCIE_DEBUG_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg), __FILE__, __LINE__, __VA_ARGS__)
#  define LOGCIE_VERBOSE_VA(msg, ...) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg), __FILE__, __LINE__, __VA_ARGS__)
#  define LOGCIE_INFO_VA(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg), __FILE__, __LINE__, __VA_ARGS__)
#  define LOGCIE_WARN_VA(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg), __FILE__, __LINE__, __VA_ARGS__)
#  define LOGCIE_ERROR_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg), __FILE__, __LINE__, __VA_ARGS__)
#  define LOGCIE_FATAL_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg), __FILE__, __LINE__, __VA_ARGS__)
#endif

int logcie_log(Logcie_Log log, const char *file, uint32_t line, ...);
int logcie_printf_formatter(LOGCIE_FOMRATTER_PARAMS);
void logcie_add_sink(Logcie_Sink sink);

#define LOGCIE_COLOR_GRAY       "\x1b[90;20m"
#define LOGCIE_COLOR_BLUE       "\x1b[36;20m"
#define LOGCIE_COLOR_YELLOW     "\x1b[33;20m"
#define LOGCIE_COLOR_RED        "\x1b[31;20m"
#define LOGCIE_COLOR_BRIGHT_RED "\x1b[31;1m"
#define LOGCIE_COLOR_RESET      "\x1b[0m"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* end of include guard: LOGCIE */

/* --------------------------------------------- */

#ifdef LOGCIE_IMPLEMENTATION

static const char *default_module = "Logcie";

#ifndef _LOGCIE_MALLOC
#  include <stdlib.h>
#  define _LOGCIE_MALLOC malloc
#endif

#ifndef _LOGCIE_ASSERT
#  define _LOGCIE_ASSERT(bool, msg) assert(bool && msg);
#endif

#ifdef _LOGCIE_DEBUG
#  include <assert.h>
#  ifdef C_STD_2011
#    define _LOGCIE_DEBUG_ASSERT(bool, msg) static_assert(bool, msg)
#  else
#    define _LOGCIE_DEBUG_ASSERT(bool, msg) _LOGCIE_ASSERT(bool, msg)
#  endif
#endif

static const char *logcie_level_label[] = {
  [LOGCIE_LEVEL_TRACE]   = "trace",
  [LOGCIE_LEVEL_DEBUG]   = "debug",
  [LOGCIE_LEVEL_VERBOSE] = "verb",
  [LOGCIE_LEVEL_INFO]    = "info",
  [LOGCIE_LEVEL_WARN]    = "warn",
  [LOGCIE_LEVEL_ERROR]   = "error",
  [LOGCIE_LEVEL_FATAL]   = "fatal",
};

static inline const char *get_logcie_level_lable(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCICE_LEVEL == 7, "Forgot to update get_logcie_level_lable, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCICE_LEVEL, "Unexpected log level");
  return logcie_level_label[level];
}

static const char *logcie_level_label_upper[] = {
  [LOGCIE_LEVEL_TRACE]   = "TRACE",
  [LOGCIE_LEVEL_DEBUG]   = "DEBUG",
  [LOGCIE_LEVEL_VERBOSE] = "VERB",
  [LOGCIE_LEVEL_INFO]    = "INFO",
  [LOGCIE_LEVEL_WARN]    = "WARN",
  [LOGCIE_LEVEL_ERROR]   = "ERROR",
  [LOGCIE_LEVEL_FATAL]   = "FATAL",
};

static inline const char *get_logcie_level_label_upper(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCICE_LEVEL == 7, "Forgot to update get_logcie_level_lable, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCICE_LEVEL, "Unexpected log level");
  return logcie_level_label_upper[level];
}

static const char *logcie_level_color[] = {
  [LOGCIE_LEVEL_TRACE]   = LOGCIE_COLOR_GRAY,
  [LOGCIE_LEVEL_DEBUG]   = LOGCIE_COLOR_GRAY,
  [LOGCIE_LEVEL_VERBOSE] = LOGCIE_COLOR_GRAY,
  [LOGCIE_LEVEL_INFO]    = LOGCIE_COLOR_BLUE,
  [LOGCIE_LEVEL_WARN]    = LOGCIE_COLOR_YELLOW,
  [LOGCIE_LEVEL_ERROR]   = LOGCIE_COLOR_RED,
  [LOGCIE_LEVEL_FATAL]   = LOGCIE_COLOR_BRIGHT_RED,
};

static inline const char *get_logcie_level_color(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCICE_LEVEL == 7, "Forgot to update get_logcie_level_lable, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCICE_LEVEL, "Unexpected log level");
  return logcie_level_color[level];
}

static Logcie_Sink default_stdout_sink = {
  .level = LOGCIE_LEVEL_TRACE,
  .sink = NULL,
  .fmt = "$L: $m",
  .formatter = logcie_printf_formatter,
  .color = true,
};

#if defined(__has_attribute) && __has_attribute(constructor)
# define __L_ATTR_CONSTRUCT
#endif

#ifdef __L_ATTR_CONSTRUCT
__attribute__((constructor)) void init_default_stdout_sink(void) {
  default_stdout_sink.sink = stdout;
}
#endif

typedef struct Logcie_Logger {
  Logcie_Sink *sinks;
  size_t sinks_len;
  size_t sinks_cap;
} Logcie_Logger;

static Logcie_Logger logcie = {
  .sinks_cap = 1,
  .sinks_len = 1,
  .sinks = &default_stdout_sink
};

// This should be called once
static void _logcie_reset(void) {
  /* _LOGCIE_ASSERT(logcie.sinks == NULL, "Badly bad stuff"); */
  logcie.sinks_cap = 8;
  logcie.sinks_len = 0;
  logcie.sinks = malloc(sizeof(*logcie.sinks) * 8);
}

void logcie_add_sink(Logcie_Sink sink) {

#ifndef __L_ATTR_CONSTRUCT
    if (sink.sink == NULL) sink.sink = stdout;
#endif

  if (logcie.sinks_cap == 1) {
    _logcie_reset();
  }

  if (logcie.sinks_cap == logcie.sinks_len) {
    logcie.sinks_cap *= 2;
    logcie.sinks = realloc(logcie.sinks, logcie.sinks_cap);
  }

  logcie.sinks[logcie.sinks_len] = sink;
  logcie.sinks_len++;
}

int logcie_log(Logcie_Log log, const char *file, uint32_t line, ...) {
  va_list args;
  va_start(args, line);

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    Logcie_Sink sink = logcie.sinks[i];
    sink.formatter(&sink, log, file, line, &args);
  }

  va_end(args);
  return 0;
}

int logcie_printf_formatter(LOGCIE_FOMRATTER_PARAMS) {
  if (log.level < sink->level) {
    return 0;
  }

  int output_len = 0;
  const char *fmt = sink->fmt;

  struct tm *tm = localtime(&log.time);
  int local_hours = tm->tm_hour;
  tm = gmtime(&log.time);
  int timediff = local_hours - tm->tm_hour;

  if (sink->color) {
    output_len += LOGCIE_PRINT(sink->sink, "%s", get_logcie_level_color(log.level));
  }

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
        output_len += LOGCIE_VAPRINT(sink->sink, log.msg, *args);
        break;
      case 'l':
        output_len += LOGCIE_PRINT(sink->sink, "%-5s", get_logcie_level_lable(log.level));
        break;
      case 'L':
        output_len += LOGCIE_PRINT(sink->sink, "%-5s", get_logcie_level_label_upper(log.level));
        break;
      case 'd':
        output_len += LOGCIE_PRINT(sink->sink, "%d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        break;
      case 't':
        output_len += LOGCIE_PRINT(sink->sink, "%02d:%02d:%02d", local_hours, tm->tm_min, tm->tm_sec);
        break;
      case 'z':
        output_len += LOGCIE_PRINT(sink->sink, "%c%d", timediff > 0 ? '+' : ' ', timediff);
        break;
      case 'f':
        output_len += LOGCIE_PRINT(sink->sink, "%s", file);
        break;
      case 'x':
        output_len += LOGCIE_PRINT(sink->sink, "%u", line);
        break;
      case 'M':
        output_len += LOGCIE_PRINT(sink->sink, "%s", log.module ? log.module : default_module);
        break;
      default:
        LOGCIE_PRINT(stderr, "%sWARN: unknown format sequence '$%c'. Skipping...\n"LOGCIE_COLOR_RESET, get_logcie_level_color(LOGCIE_LEVEL_WARN), *fmt);
        break;
    }

    fmt++;
  }

  if (sink->color) {
    output_len += LOGCIE_PRINT(sink->sink, LOGCIE_COLOR_RESET);
  }

  output_len += LOGCIE_PRINT(sink->sink, "\n");
  return output_len;
}

// TODO: Align output up to delimiter


#endif /* end of include guard: LOGCIE_IMPLEMENTATION */

/*
The MIT License (MIT)
Copyright (c) 2025 Nikita (Strongleong) Chulkov nikita_chul@mail.ru
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
