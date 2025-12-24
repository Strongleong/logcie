/*
 * Logcie v0.2.0 - Logging Library (Single Header)
 *
 * Description:
 *   Logcie is a lightweight, modular, single-header logging library written in C.
 *   It supports multiple log levels, ANSI color output, flexible formatting, and
 *   customizable filters and sinks for advanced logging use cases.
 *
 * Usage:
 *   #define LOGCIE_IMPLEMENTATION
 *   #include "logcie.h"
 *
 * Author: Nikita (Strongleong) Chulkov
 * License: MIT
 */

#ifndef LOGCIE
#define LOGCIE

#ifndef LOGCIE_DEF
#define LOGCIE_DEF extern
#endif

// Versioning macros
#define LOGCIE_VERSION_MAJOR    1
#define LOGCIE_VERSION_MINOR    0
#define LOGCIE_VERSION_RELEASE  0
#define LOGCIE_VERSION_NUMBER   (LOGCIE_VERSION_MAJOR *100*100 + LOGCIE_VERSION_MINOR *100 + LOGCIE_VERSION_RELEASE)
#define LOGCIE_VERSION_FULL     LOGCIE_VERSION_MAJOR.LOGCIE_VERSION_MINOR.LOGCIE_VERSION_RELEASE
#define LOGCIE_QUOTE(str)       #str
#define LOGCIE_EXPAND_AND_QUOTE(str) LOGCIE_QUOTE(str)
#define LOGCIE_VERSION_STRING   LOGCIE_EXPAND_AND_QUOTE(LOGCIE_VERSION_FULL)

// ANSI color definitions for terminal output
#define LOGCIE_COLOR_GRAY       "\x1b[90;20m"
#define LOGCIE_COLOR_BLUE       "\x1b[36;20m"
#define LOGCIE_COLOR_YELLOW     "\x1b[33;20m"
#define LOGCIE_COLOR_RED        "\x1b[31;20m"
#define LOGCIE_COLOR_BRIGHT_RED "\x1b[31;1m"
#define LOGCIE_COLOR_RESET      "\x1b[0m"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/**
 * @enum Logcie_LogLevel
 * @brief Enumerates all available log severity levels.
 *
 * Log levels are ordered from most verbose (TRACE) to most severe (FATAL).
 * Each sink can be configured with a minimum level to control verbosity.
 *
 * @value LOGCIE_LEVEL_TRACE   Most detailed information for deep debugging
 * @value LOGCIE_LEVEL_DEBUG   Debugging information for development
 * @value LOGCIE_LEVEL_VERBOSE Verbose operational details
 * @value LOGCIE_LEVEL_INFO    General informational messages
 * @value LOGCIE_LEVEL_WARN    Warning conditions that might need attention
 * @value LOGCIE_LEVEL_ERROR   Error conditions that prevent normal operation
 * @value LOGCIE_LEVEL_FATAL   Fatal conditions requiring immediate shutdown
 * @value Count_LOGCIE_LEVEL   Sentinel value representing total count of levels
 */
typedef enum Logcie_LogLevel {
  LOGCIE_LEVEL_TRACE,
  LOGCIE_LEVEL_DEBUG,
  LOGCIE_LEVEL_VERBOSE,
  LOGCIE_LEVEL_INFO,
  LOGCIE_LEVEL_WARN,
  LOGCIE_LEVEL_ERROR,
  LOGCIE_LEVEL_FATAL,
  Count_LOGCIE_LEVEL,
} Logcie_LogLevel;

/**
 * @brief Defines the module name for the current translation unit.
 *
 * When this variable is defined, all logs from this file will be tagged
 * with the specified module name, which can be displayed using $M in format strings
 * or can be useid in filters.
 *
 * Example usage:
 * @code
 * static const char *logcie_module = "network";
 * @endcode
 */
#if defined(__has_attribute) && __has_attribute(unused)
 static const char __attribute__ ((unused)) *logcie_module;
#else
 static const char *logcie_module;
#endif

typedef struct Logcie_Sink Logcie_Sink;
typedef struct Logcie_Log Logcie_Log;

/**
 * @brief Formatter function type signature.
 *
 * A formatter function is responsible for converting a Logcie_Log structure
 * into formatted output written to a sink. The default formatter is
 * logcie_printf_formatter() which uses $ tokens for formatting.
 *
 * @param sink  The sink to write formatted output to
 * @param log   Log metadata and message to format
 * @param args  Variable argument list for message formatting
 * @return      Number of characters written to the sink
 */
typedef size_t (Logcie_FormatterFn)(Logcie_Sink *sink, Logcie_Log log, va_list *args);

/**
 * @brief Filter function type signature.
 *
 * A filter function determines whether a log message should be emitted
 * to a particular sink. Return 1 (true) to allow the log, 0 (false) to
 * suppress it.
 *
 * @param sink  The sink being evaluated
 * @param log   Log metadata to evaluate
 * @return      1 to emit log, 0 to suppress
 */
typedef uint8_t (Logcie_FilterFn)(Logcie_Sink *sink, Logcie_Log *log);

/**
 * @brief Structure representing a single log sink (output target).
 *
 * A sink defines where log messages are written and how they are formatted.
 * Multiple sinks can be active simultaneously, each with different formatting
 * and filtering rules.
 *
 * @field sink        Output file stream (stdout, file, etc.)
 * @field min_level   Minimum log level to emit (messages below this are ignored)
 * @field fmt         Format string using $ tokens for customization
 * @field formatter   Function pointer to format and write log messages
 * @field filter      Optional filter function to selectively output messages
 * @field userdata    User-provided context data for custom filters
 */
struct Logcie_Sink {
  FILE *sink;                     ///< Output file stream (stdout, file, etc.)
  Logcie_LogLevel min_level;      ///< Minimum log level to emit
  const char *fmt;                ///< Format string using $ tokens
  Logcie_FormatterFn *formatter;  ///< Formatter function
  Logcie_FilterFn *filter;        ///< Optional filter function
  void *userdata;                 ///< User data/context for filters
};

/**
 * @brief Structure representing source code location of a log call.
 *
 * Automatically populated by the LOGCIE_* macros using __FILE__ and __LINE__.
 *
 * @field file      Source file name where log was called
 * @field line      Line number in source file where log was called
 */
typedef struct Logcie_LogLocation {
  const char *file;
  uint32_t line;
} Logcie_LogLocation;

/**
 * @brief Structure representing a complete log message with metadata.
 *
 * This structure contains all information about a log event, including
 * severity level, message text, timestamp, source location, and module.
 * It is typically created by the LOGCIE_* macros and passed to formatters.
 *
 * @field level     Severity level of the log message
 * @field msg       Format string for the log message
 * @field time      Timestamp when the log was created
 * @field module    Optional module name for categorizing logs
 * @field location  Source file and line number where log was called
 */
struct Logcie_Log {
  Logcie_LogLevel level;
  const char *msg;
  time_t time;
  const char *module;
  Logcie_LogLocation location;
};

/**
 * @brief Context structure for logical AND/OR filter combinations.
 *
 * Used with logcie_set_filter_and() and logcie_set_filter_or() to store
 * references to the two filter functions being combined.
 *
 * @field a    First filter function
 * @field b    Second filter function
 */
typedef struct Logcie_CombinedFilterContext {
  Logcie_FilterFn *a;
  Logcie_FilterFn *b;
} Logcie_CombinedFilterContext;

/**
 * @brief Context structure for logical NOT filter.
 *
 * Used with logcie_set_filter_not() to store reference to the filter
 * function being negated.
 *
 * @field a    Filter function to negate
 */
typedef struct Logcie_NotFilterContext {
  Logcie_FilterFn *a;
} Logcie_NotFilterContext;

// Helper macro for constructing a log message
#define LOGCIE_CREATE_LOG(lvl, txt, f, l) (Logcie_Log) { .level = lvl, .msg = txt, .time = time(NULL), .module = logcie_module, .location = {.file = f, .line = l }}

#define PRINTF_TYPECHECK(a, b)

#if defined __has_attribute
#  if __has_attribute(__format__)
#    undef PRINTF_TYPECHECK
#    define PRINTF_TYPECHECK(a, b) __attribute__((__format__(__printf__, a, b)))
#  endif
#endif

/**
 * @brief Convenience macros for each log level.
 * These use __FILE__ and __LINE__ to capture call site.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
# define LOGCIE_TRACE(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_DEBUG(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_VERBOSE(msg, ...) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_INFO(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_WARN(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_ERROR(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
# define LOGCIE_FATAL(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg, __FILE__, __LINE__), msg, __VA_OPT__(,) __VA_ARGS__)
#else
# if !defined(LOGCIE_PEDANTIC) && (defined(__GNUC__) || defined (__clang__))
#  define LOGCIE_TRACE(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#  define LOGCIE_DEBUG(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#  define LOGCIE_VERBOSE(msg, ...) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#  define LOGCIE_INFO(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#  define LOGCIE_WARN(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#  define LOGCIE_ERROR(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#  define LOGCIE_FATAL(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
# else
#  define LOGCIE_TRACE(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg, __FILE__, __LINE__), msg)
#  define LOGCIE_DEBUG(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg, __FILE__, __LINE__), msg)
#  define LOGCIE_VERBOSE(msg) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg)
#  define LOGCIE_INFO(msg)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg, __FILE__, __LINE__), msg)
#  define LOGCIE_WARN(msg)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg, __FILE__, __LINE__), msg)
#  define LOGCIE_ERROR(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg, __FILE__, __LINE__), msg)
#  define LOGCIE_FATAL(msg)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg, __FILE__, __LINE__), msg)
#  define LOGCIE_VA_LOGS
# endif
#endif

#ifdef LOGCIE_VA_LOGS
#  define LOGCIE_TRACE_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE,   msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#  define LOGCIE_DEBUG_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG,   msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#  define LOGCIE_VERBOSE_VA(msg, ...) logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#  define LOGCIE_INFO_VA(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO,    msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#  define LOGCIE_WARN_VA(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN,    msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#  define LOGCIE_ERROR_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR,   msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#  define LOGCIE_FATAL_VA(msg, ...)   logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL,   msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#endif

/**
 * @brief Emit a log message using the provided log metadata and arguments.
 * @note This function is invoked internally by macros like LOGCIE_INFO.
 */
LOGCIE_DEF size_t logcie_log(Logcie_Log log, const char *fmt, ...) PRINTF_TYPECHECK(2, 3);

/**
 * @brief Gets the number of sinks currently registered in the logger.
 *
 * This includes both user-added sinks and the default stdout sink.
 * Useful for iterating through sinks or monitoring sink count.
 *
 * @return Number of active sinks
 */
LOGCIE_DEF size_t logcie_get_sink_count(void);

/**
 * @brief Retrieves a sink by its index in the sink array.
 *
 * Sinks are stored in the order they were added. The default stdout sink
 * is always at index 0 unless logcie_reset() has been called.
 *
 * @param index Zero-based index of the sink to retrieve
 * @return Pointer to the Logcie_Sink, or NULL if index is invalid
 */
LOGCIE_DEF Logcie_Sink *logcie_get_sink(size_t index);

/**
 * @brief Adds a new sink to the logger.
 * @param sink Pointer to a Logcie_Sink structure.
 */
LOGCIE_DEF void logcie_add_sink(Logcie_Sink *sink);

/**
 * @brief Removes a sink from the logger by pointer.
 * @param sink Pointer to the sink to remove.
 * @return 1 if sink was found and removed, 0 otherwise.
 * @note The sink memory is not freed by this function. Caller is responsible.
 */
LOGCIE_DEF uint8_t logcie_remove_sink(Logcie_Sink *sink);

/**
 * @brief Removes a sink from the logger by index.
 * @param index Zero-based index of the sink to remove.
 * @return 1 if sink was found and removed, 0 otherwise.
 * @note The sink memory is not freed by this function. Caller is responsible.
 */
LOGCIE_DEF uint8_t logcie_remove_sink_by_index(size_t index);

/**
 * @brief Removes and frees a sink from the logger (if it was dynamically allocated).
 * @param sink Pointer to the sink to remove and free.
 * @return 1 if sink was found and removed, 0 otherwise.
 * @note Only use this if the sink was allocated with malloc().
 */
LOGCIE_DEF uint8_t logcie_remove_and_free_sink(Logcie_Sink *sink);

/**
 * @brief Removes all sinks except the default stdout sink.
 * @note Sink memory is not freed. Caller is responsible for cleanup.
 */
LOGCIE_DEF void logcie_remove_all_sinks(void);

/**
 * @brief Removes a sink and closes its file stream if it's not stdout/stderr.
 * @param sink Pointer to the sink to remove.
 * @return 1 if sink was found and removed, 0 otherwise.
 */
LOGCIE_DEF uint8_t logcie_remove_sink_and_close(Logcie_Sink *sink);

/**
 * @brief Built-in NOT filter. Inverts result of provided filter.
 */
LOGCIE_DEF uint8_t logcie_filter_not(Logcie_Sink *sink, Logcie_Log *log);

/**
 * @brief Built-in AND filter. Combines two filters with logical AND.
 */
LOGCIE_DEF uint8_t logcie_filter_and(Logcie_Sink *sink, Logcie_Log *log);

/**
 * @brief Built-in OR filter. Combines two filters with logical OR.
 */
LOGCIE_DEF uint8_t logcie_filter_or(Logcie_Sink *sink, Logcie_Log *log);

/**
 * @brief Sets a NOT filter on a sink. Caller must provide context struct.
 */
void logcie_set_filter_not(Logcie_Sink *sink, Logcie_FilterFn *a, Logcie_NotFilterContext *ctx);

/**
 * @brief Sets an AND filter on a sink. Caller must provide context struct.
 */
void logcie_set_filter_and(Logcie_Sink *sink, Logcie_FilterFn *a, Logcie_FilterFn *b, Logcie_CombinedFilterContext *ctx);

/**
 * @brief Sets an OR filter on a sink. Caller must provide context struct.
 */
void logcie_set_filter_or(Logcie_Sink *sink, Logcie_FilterFn *a, Logcie_FilterFn *b, Logcie_CombinedFilterContext *ctx);

/**
 * @brief Default formatter using printf-style formatting and $ tokens.
 */
LOGCIE_DEF size_t logcie_printf_formatter(Logcie_Sink *sink, Logcie_Log log, va_list *args);

/**
 * @brief Allows customization of log level colors. Must be array of size Count_LOGCIE_LEVEL.
 * @param colors Array of strings or NULL to reset to default.
 */
LOGCIE_DEF void logcie_set_colors(const char **colors);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* end of include guard: LOGCIE */

/* --------------------------------------------- */

#ifdef LOGCIE_IMPLEMENTATION

#include <assert.h>
#include <stdlib.h>

static const char *default_module = "Logcie";

# ifndef _LOGCIE_ASSERT
#  define _LOGCIE_ASSERT(bool, msg) assert(bool && msg)
# endif

#ifdef _LOGCIE_DEBUG
# if __STDC_VERSION__ >= 201112L  // Check for C11 support
#  define _LOGCIE_DEBUG_ASSERT(bool, msg) static_assert(bool, msg)
# else
#  define _LOGCIE_DEBUG_ASSERT(bool, msg) _LOGCIE_ASSERT(bool, msg)
# endif
#else
# define _LOGCIE_DEBUG_ASSERT(bool, msg)
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

static inline const char *get_logcie_level_label(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
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
  _LOGCIE_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
  return logcie_level_label_upper[level];
}

static const char *logcie_default_level_color[] = {
  [LOGCIE_LEVEL_TRACE]   = LOGCIE_COLOR_GRAY,
  [LOGCIE_LEVEL_DEBUG]   = LOGCIE_COLOR_GRAY,
  [LOGCIE_LEVEL_VERBOSE] = LOGCIE_COLOR_GRAY,
  [LOGCIE_LEVEL_INFO]    = LOGCIE_COLOR_BLUE,
  [LOGCIE_LEVEL_WARN]    = LOGCIE_COLOR_YELLOW,
  [LOGCIE_LEVEL_ERROR]   = LOGCIE_COLOR_RED,
  [LOGCIE_LEVEL_FATAL]   = LOGCIE_COLOR_BRIGHT_RED,
};

static const char **logcie_level_color = logcie_default_level_color;

void logcie_set_colors(const char **colors) {
  if (colors) {
    logcie_level_color = colors;
  } else {
    logcie_level_color = logcie_default_level_color;
  }
}

static inline const char *get_logcie_level_color(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
  return logcie_level_color[level];
}

static Logcie_Sink default_stdout_sink = {
  .min_level = LOGCIE_LEVEL_TRACE,
  .sink = NULL,
  .fmt = "$c$L$r "LOGCIE_COLOR_GRAY"$f$x$r: $m",
  .formatter = logcie_printf_formatter,
};

static Logcie_Sink *default_stdout_sink_ptr = &default_stdout_sink;

#if defined(__has_attribute) && __has_attribute(constructor)
# define __L_ATTR_CONSTRUCT
#endif

#ifdef __L_ATTR_CONSTRUCT
__attribute__((constructor)) void init_default_stdout_sink(void) {
  default_stdout_sink.sink = stdout;
}
#endif

uint8_t logcie_filter_not(Logcie_Sink *sink, Logcie_Log *log) {
  Logcie_NotFilterContext *ctx = (Logcie_NotFilterContext *)sink->userdata;
  return !ctx->a(sink, log);
}

uint8_t logcie_filter_and(Logcie_Sink *sink, Logcie_Log *log) {
  Logcie_CombinedFilterContext *ctx = (Logcie_CombinedFilterContext *)sink->userdata;
  return ctx->a(sink, log) && ctx->b(sink, log);
}

uint8_t logcie_filter_or(Logcie_Sink *sink, Logcie_Log *log) {
  Logcie_CombinedFilterContext *ctx = (Logcie_CombinedFilterContext *)sink->userdata;
  return ctx->a(sink, log) || ctx->b(sink, log);
}

void logcie_set_filter_not(Logcie_Sink *sink, Logcie_FilterFn *a, Logcie_NotFilterContext *ctx) {
  ctx->a = a;
  sink->filter = logcie_filter_not;
  sink->userdata = ctx;
}

void logcie_set_filter_and(Logcie_Sink *sink, Logcie_FilterFn *a, Logcie_FilterFn *b, Logcie_CombinedFilterContext *ctx) {
  ctx->a = a;
  ctx->b = b;
  sink->filter = logcie_filter_and;
  sink->userdata = ctx;
}

void logcie_set_filter_or(Logcie_Sink *sink, Logcie_FilterFn *a, Logcie_FilterFn *b, Logcie_CombinedFilterContext *ctx) {
  ctx->a = a;
  ctx->b = b;
  sink->filter = logcie_filter_or;
  sink->userdata = ctx;
}

typedef struct Logcie_Logger {
  Logcie_Sink **sinks;
  size_t sinks_len;
  size_t sinks_cap;
  uint8_t *is_active;
} Logcie_Logger;

// INFO: By default there is one default sink to allow logging right
//       after includnig logcie without initializing anything
static Logcie_Logger logcie = {
  .sinks_cap = 1,
  .sinks_len = 1,
  .sinks = &default_stdout_sink_ptr,
  .is_active = NULL,
};

// WARN: This should be called once
static void _logcie_reset(void) {
  /* _LOGCIE_ASSERT(logcie.sinks == NULL, "Badly bad stuff"); */
  logcie.sinks_cap = 8;
  logcie.sinks_len = 0;
  logcie.sinks = malloc(sizeof(*logcie.sinks) * logcie.sinks_cap);
}

size_t logcie_get_sink_count(void) {
  return logcie.sinks_len;
}

Logcie_Sink *logcie_get_sink(size_t index) {
  if (index == 0 || index > logcie.sinks_len) {
    return NULL;
  }

  return logcie.sinks[index];
}

void logcie_add_sink(Logcie_Sink *sink) {

#ifndef __L_ATTR_CONSTRUCT
    if (sink->sink == NULL) sink->sink = stdout;
#endif

  if (logcie.sinks_cap == 1) {
    _logcie_reset();
  }

  if (logcie.sinks_cap == logcie.sinks_len) {
    logcie.sinks_cap *= 2;
    logcie.sinks = realloc(logcie.sinks, sizeof(*logcie.sinks) * logcie.sinks_cap);
  }

  logcie.sinks[logcie.sinks_len] = sink;
  logcie.sinks_len++;
}

uint8_t logcie_remove_sink(Logcie_Sink *sink) {
  if (sink == &default_stdout_sink) {
    return 0; // unreacheble
  }

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    if (logcie.sinks[i] == sink) {
      return logcie_remove_sink_by_index(i);
    }
  }

  return 0;
}

uint8_t logcie_remove_sink_by_index(size_t index) {
  if (index >= logcie.sinks_len) {
    return 0;
  }

  if (logcie.sinks_cap == 1 && index == 0) {
    return 0;
  }

  for (size_t i = index; i < logcie.sinks_len; i++) {
    logcie.sinks[i] = logcie.sinks[i + 1];
  }

  logcie.sinks_len--;
  return 1;
}

uint8_t logcie_remove_and_free_sink(Logcie_Sink *sink) {
  if (logcie_remove_sink(sink)){
    free(sink);
    return 0;
  }

  return 1;
}

void logcie_remove_all_sinks(void) {
  if (logcie.sinks_cap > 1) {
    free(logcie.sinks);
    logcie.sinks_cap = 1;
    logcie.sinks_len = 1;
    logcie.sinks = &default_stdout_sink_ptr;
    return;
  }
}

LOGCIE_DEF uint8_t logcie_remove_sink_and_close(Logcie_Sink *sink) {
  uint8_t is_file = sink->sink != stdout && sink->sink != stderr && sink->sink != NULL;

  if (logcie_remove_sink(sink) && is_file) {
    fclose(sink->sink);
    return 1;
  }

  return 0;
}

size_t logcie_log(Logcie_Log log, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log.msg = fmt;

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    Logcie_Sink *sink = logcie.sinks[i];

    if (log.level < sink->min_level) {
      continue;
    }

    if (sink->filter && !sink->filter(sink, &log)) {
      continue;
    }

    va_list args_copy;
    va_copy(args_copy, args);

    sink->formatter(sink, log, &args_copy);

    va_end(args_copy);
  }

  va_end(args);
  return 0;
}

size_t logcie_printf_formatter(Logcie_Sink *sink, Logcie_Log log, va_list *args) {
  size_t output_len = 0;
  const char *fmt = sink->fmt;

  struct tm local_tm = *localtime(&log.time);
  struct tm utc_tm = *gmtime(&log.time);

  int32_t local_hours = local_tm.tm_hour;
  int32_t timediff = (int32_t)difftime(mktime(&local_tm), mktime(&utc_tm)) / 3600;

  while (*fmt != '\0') {
    if (*fmt != '$') {
      output_len += fprintf(sink->sink, "%c", *fmt);
      fmt++;
      continue;
    }

    fmt++;

    if (*fmt == '\0') {
      break;
    }

    switch (*fmt) {
      case '$':
        output_len += fprintf(sink->sink, "$");
        break;
      case 'm':
        output_len += vfprintf(sink->sink, log.msg, *args);
        break;
      case 'l':
        output_len += fprintf(sink->sink, "%-5s", get_logcie_level_label(log.level));
        break;
      case 'L':
        output_len += fprintf(sink->sink, "%-5s", get_logcie_level_label_upper(log.level));
        break;
      case 'c':
        output_len += fprintf(sink->sink, "%s", get_logcie_level_color(log.level));
        break;
      case 'r':
        output_len += fprintf(sink->sink, LOGCIE_COLOR_RESET);
        break;
      case 'd':
        output_len += fprintf(sink->sink, "%d-%02d-%02d", local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday);
        break;
      case 't':
        output_len += fprintf(sink->sink, "%02d:%02d:%02d", local_hours, local_tm.tm_min, local_tm.tm_sec);
        break;
      case 'z':
        output_len += fprintf(sink->sink, "%+d", timediff);
        break;
      case 'f':
        output_len += fprintf(sink->sink, "%s", log.location.file);
        break;
      case 'x':
        output_len += fprintf(sink->sink, "%u", log.location.line);
        break;
      case 'M':
        output_len += fprintf(sink->sink, "%s", log.module ? log.module : default_module);
        break;
      default:
        fprintf(stderr, "%sWARN: unknown format sequence '$%c'. Skipping...\n"LOGCIE_COLOR_RESET, get_logcie_level_color(LOGCIE_LEVEL_WARN), *fmt);
        break;
    }

    fmt++;
  }

  output_len += fprintf(sink->sink, "\n");
  fflush(sink->sink);
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
