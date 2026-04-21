/*
 * Logcie v0.11.0 - Logging Library (Single Header)
 *
 * Description:
 *   Logcie is a lightweight, modular, single-header logging library written in C.
 *   It supports multiple log levels, ANSI color output, flexible formatting, and
 *   customizable filters and sinks for advanced logging use cases.
 *
 *   NOTE: This library is NOT thread-safe in version 0.x.
 *       Concurrent calls from multiple threads may interleave output.
 *       Thread safety is planned for version 1.0.
 *
 * Basic usage:
 *   #define LOGCIE_IMPLEMENTATION
 *   #include "logcie.h"
 *
 *   LOGCIE_INFO("Hello from Logcie");
 *   LOGCIE_VERBOSE("Logcie supports %s logging", "printf-style");
 *
 * Log levels:
 *   Logcie supports 7 different log levels, ordered from lowest to highest severity:
 *     TRACE   - Most detailed information for deep debugging
 *     DEBUG   - Debugging information for development
 *     VERBOSE - Verbose operational details
 *     INFO    - General informational messages
 *     WARN    - Warning conditions that might need attention
 *     ERROR   - Error conditions that prevent normal operation
 *     FATAL   - Fatal conditions requiring immediate shutdown
 *
 * How it works:
 *   The core of this library is `Formatter`, `Writer` and `Filter`.
 *
 *   Formatter takes a log structure and is responsible for generating a string out of it.
 *             While generating text that will be written in the log it calls Writer.
 *   Writer receives formatted strings from the Formatter and it responsible for writing them
 *          to the final destination. It can write logs to a FILE, send them to an HTTP API endpoint,
 *          or even render them on embeded displays while blinking LED with color of current log level.
 *   Filter filters out logs. Pretty much self-explanatory. We will ignore then for now scince they not finished yet.
 *
 *   A combination of Formatter, Writer and Filter is called a Sink.
 *   You can have as many sinks as you want. Logcie will send logs to every sinks available.
 *
 *   Logcie itself is basically an array of Sinks and system of distributing logs to those Sinks.
 *
 * Defaults:
 *   It would not be that great if Logcie was just empty framework and you need to set it up by yourself,
 *   so Logcie comes with a couple of pre-defined functions:
 *
 *      - logcie_printf_writer    - built-in writer. Outputs logs in FILE* via vfprintf
 *      - logcie_printf_formatter - built-in formatter that provides rich formatting using $ tokens. Here is the list:
 *                                   `$m` - Log message with printf formatting
 *                                   `$f` - Source file name
 *                                   `$x` - Line number
 *                                   `$M` - Module name
 *                                   `$l` - Log level (lowercase)
 *                                   `$L` - Log level (uppercase)
 *                                   `$c` - ANSI color code for log level
 *                                   `$r` - ANSI reset color code
 *                                   `$d` - Date (YYYY-MM-DD)
 *                                   `$t` - Time (HH:MM:SS)
 *                                   `$z` - Timezone offset
 *                                   `$<n - Pads with n spaces
 *                                   `$$` - Literal dollar sign
 *
 *   Also by default, Logcie alredy has a Sink installed with the printf writer and formatter,
 *   so you can start using it immediately after including the library.
 *
 *   Note: When you add your first Sink using `logcie_add_sink()`, the default printf Sink is removed.
 *   You can restore it and remove your own sinks by calling `logcie_remove_all_sinks()`.
 *
 * Colors:
 *   As you can see, `logcie_printf_formatter()` has support for ANSI colored output. It have
 *   log level to ANSI color table to make your errors red, warnings yellow and infos blue.
 *   You can modify this table with `logcie_set_colors()`:
 *     ```c
 *     const char *my_colors[Count_LOGCIE_LEVEL] = {
 *         [LOGCIE_LEVEL_TRACE]   = "\x1b[90m",    // Gray
 *         [LOGCIE_LEVEL_DEBUG]   = "\x1b[94m",    // Light blue
 *         [LOGCIE_LEVEL_VERBOSE] = "\x1b[92m",    // Light green
 *         [LOGCIE_LEVEL_INFO]    = "\x1b[1;32m",  // Bright green (bold)
 *         [LOGCIE_LEVEL_WARN]    = "\x1b[33m",    // Yellow
 *         [LOGCIE_LEVEL_ERROR]   = "\x1b[1;33m",  // Bright yellow (bold)
 *         [LOGCIE_LEVEL_FATAL]   = "\x1b[1;31m",  // Bright red (bold)
 *     };
 *
 *     logcie_set_colors(my_colors);
 *     ```
 *
 *     To reset colors, call `logcie_set_colors(NULL)`.
 *
 * Memory management:
 *   This library does not manage the lifetime of Sinks or their associated resources.
 *   Ensure that any Sink you create remains valid for as long as it is in use.
 *   TIP: Just have them in main function, or in static/global scope.
 *
 * Filters:
 *   Filters allow you to control which logs are emitted to a specific Sink.
 *   Each Sink can have its own filter, enabling fine-grained routing of logs.
 *
 *   A filter is a structure that consist of pointer to filtering fucntion and
 *   a pointer to custom data that filter might want to use.
 *
 *   A filtering function is simply a function that recieves a `Logcie_Log` and returns:
 *     1 (true)  - to allow the log
 *     0 (false) - to suppress the log
 *
 *  If a Sink has no filter all logs are allowed.
 *
 *  Here is a list of built-in filters:
 *
 *    - logcie_filter_level_min(level)
 *        Allows logs with level >= specified level
 *
 *    - logcie_filter_level_max(level)
 *        Allows logs with level <= specified level
 *
 *    - logcie_filter_module_eq("module")
 *        Allows logs only from specific module (see below for learning about modules)
 *
 *    - logcie_filter_message_contains("text")
 *        Allows logs whosse messages contains the given substring
 *
 *   Combining filters:
 *
 *    - logcie_filter_and(a, b)
 *        Allows logs only if BOTH filters pass
 *
 *    - logcie_filter_or(a, b)
 *        Allows logs only if EITHER filters pass
 *
 *    - logcie_filter_not(a)
 *        Inverts theresult of a filter
 *
 *   Example:
 *     ```c
 *     // Sink that takes logs with level more than VERBOSE and not from "network" module
 *     Logcie_Sink sink = {
 *       //...
 *       .filter = logcie_filter_and(
 *         logcie_filter_level_min(LOGCIE_VERBOSE),
 *         logcie_filter_not(
 *           logcie_filter_module_eq("network")
 *         )
 *       )
 *     };
 *
 *     uint8_t custom_filter_fn(void *data, Logcie_Log *log) {
 *       (void) data; // ignored
 *
 *       // Do not allow logs from even lines
 *       return log->location.line % 2 == 0;
 *     }
 *
 *     Logcie_Sink another_sink = {
 *       // ...
 *       .filter = (Logcie_Filter) {
 *         .filter = custom_filter_fn,
 *         .data = NULL,
 *       }
 *     }
 *     ```
 *
 *   Notes:
 *     - Filters are evealuated per sink, independently.
 *     - Be careful when using temporary data in filters (they rely on
 *       compound literals and must remain valid during logging).
 *
 * Modules:
 *   Logcie has another important concept: modules. A module is simply a string used
 *   to label a *scope* where the log originated.
 *
 *   Modules allow you to group logs by subsystem (e.g., "network", "core", "database")
 *   and can be used in format strings or filters to provied additional context or control
 *   log output.
 *
 *   To define a module, declare a variable name `logcie_module` in your translation uint:
 *     ```c
 *     static const char *logcie_module = "network";
 *     ```
 *
 *   When defined, this value will be attached to every log emitted from that file.
 *   If not defined, a default module name is used.
 *
 *   You can display the module in your logs using `$M` format token.
 *   For example:
 *     "$d $t [$L] ($M) $m"
 *   will output
 *      2026-03-25 12:00:00 [INFO] (network) Connection established
 *
 *   Modules can also be used in custom filters to selectively allow or block logs
 *   from specific parts of your application.
 *
 *   Modules also make it easy to integrate Logcie-compatible logging into third-party
 *   libraries without creating tight dependencies.
 *
 *   A library can define its own module name and use logging macros (or wrapper macros)
 *   without needing direct knowledge of the application's logging setup.
 *
 *   Example (inside a library):
 *     ```c
 *     #ifndef MYLIB_LOG
 *       #ifdef LOGCIE
 *         static const char *logcie_module = "mylib";
 *         #define MYLIB_LOG(level, ...) LOGCIE_##level(__VA_ARGS__)
 *       #else
 *         #define MYLIB_LOG(level, ...)
 *       #endif
 *     #endif
 *
 *     MYLIB_LOG(INFO, "Library initialized");
 *     ```
 *
 * Example:
 *   You can have sink that will format log with "[$log level$] $log message$"
 *    format to stdout, filtering out everything more verbose than LOGCIE_INFO level. At the
 *    same time you can have second sink that will dump every log up until LOGCIE_DEBUG level
 *    in './log.txt' file with format that would look like:
 *    "$log level$:$file$:$line$: $log message$ ($log time$ $log date$)".
 *
 *     ```c
 *     // Defining our sinks
 *     Logcie_Sink stdout_sink = {
 *       .formatter = { logcie_printf_formatter, "[$L] $m" },
 *       .writer = { logcie_printf_writer, stdout },
 *       .filter = { logcie_filter_level_min, LOGCIE_LEVEL_INFO }
 *     };
 *
 *     Logcie_Sink file_sink = {
 *       .formatter = { logcie_printf_formatter, "$L:$f:$x: $m ($t $d)" },
 *       .writer = { logcie_printf_writer, fopen("./log.txt", "w") },
 *     };
 *
 *     // New sinks must be registred with `logcie_add_sink()`
 *     logcie_add_sink(&stdout_sink);
 *     logcie_add_sink(&file_sink);
 *     ```
 *
 *     Now lets say you have this logs in code:
 *        ```c
 *        LOGCIE_INFO("Starting the application");
 *        LOGCIE_VERBOSE("Version v%s", get_version_string());
 *        LOGCIE_DEBUG("Commit hash: %s", get_commit_hash());
 *        LOGCIE_FATAL("Out of memory");
 *        ```
 *     User would see in the console:
 *        ```console
 *        [INFO] Starting the application
 *        [FATAL] Out of memory
 *        ```
 *     but in the './log.txt':
 *        ```text
 *        INFO:main.c:12 Starting the application (12:07:59 11:03:2026)
 *        VERBOSE:main.c:13 Version v4.25.1 (12:07:59 11:03:2026)
 *        DEBUG:main.c:14 Commit hash: bf3b539fcbffcc8113f241ab8bf5454f84487b67 (12:08:00 11:03:2026)
 *        FATAL:main.c:32 Out of memory (12:08:00 11:03:2026)
 *        ```
 *
 *      While we are here, let's also go through how you can remove your Sinks:
 *        ```c
 *        // Remove your sink by index
 *        logcie_remove_sink_by_index(1);
 *
 *        // Or do it by pointer
 *        logcie_remove_sink(&file_sink);
 *
 *        // If you malloc'ed your sink there is handy way to remove it and free
 *        logcie_remove_and_free_sink(&file_sink);
 *        ```
 *
 * Author: Nikita (Strongleong) Chulkov nikita_chul@mail.ru
 * License: MIT
 */

#ifndef LOGCIE
#define LOGCIE

#ifndef LOGCIE_DEF
#define LOGCIE_DEF extern
#endif

// Versioning macros
#define LOGCIE_VERSION_MAJOR         0
#define LOGCIE_VERSION_MINOR         11
#define LOGCIE_VERSION_RELEASE       0
#define LOGCIE_VERSION_NUMBER        (LOGCIE_VERSION_MAJOR * 100 * 100 + LOGCIE_VERSION_MINOR * 100 + LOGCIE_VERSION_RELEASE)
#define LOGCIE_VERSION_FULL          LOGCIE_VERSION_MAJOR.LOGCIE_VERSION_MINOR.LOGCIE_VERSION_RELEASE
#define LOGCIE_QUOTE(str)            #str
#define LOGCIE_EXPAND_AND_QUOTE(str) LOGCIE_QUOTE(str)
#define LOGCIE_VERSION_STRING        LOGCIE_EXPAND_AND_QUOTE(LOGCIE_VERSION_FULL)

// ANSI color definitions for terminal output
#define LOGCIE_COLOR_GRAY       "\x1b[90;20m"
#define LOGCIE_COLOR_BLUE       "\x1b[36;20m"
#define LOGCIE_COLOR_YELLOW     "\x1b[33;20m"
#define LOGCIE_COLOR_RED        "\x1b[31;20m"
#define LOGCIE_COLOR_BRIGHT_RED "\x1b[31;1m"
#define LOGCIE_COLOR_RESET      "\x1b[0m"

#ifndef LOGCIE_DEFAULT_SINK_FORMAT
#define LOGCIE_DEFAULT_SINK_FORMAT "$c$L$r " LOGCIE_COLOR_GRAY "$f:$x$r: $m"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
 * or can be used in filters.
 *
 * For C++, we need a different approach
 * Users should define: const char *logcie_module = "module";
 *
 * Example usage:
 * @code
 * static const char *logcie_module = "network";
 * @endcode
 */
#ifdef __cplusplus
#define LOGCIE_MODULE_DEF
#else
#define LOGCIE_MODULE_DEF static
#endif

#if defined(__has_attribute) && __has_attribute(unused)
LOGCIE_MODULE_DEF const char __attribute__((unused)) * logcie_module;
#else
LOGCIE_MODULE_DEF const char *logcie_module;
#endif

/**
 * @brief Structure representing a single log sink (output target).
 * @see struct Logcie_Sink
 */
typedef struct Logcie_Sink Logcie_Sink;

/**
 * @brief Structure representing a complete log message with metadata.
 * @see struct Logcie_Log
 */
typedef struct Logcie_Log Logcie_Log;

/**
 * @brief Writer function type signature
 *
 * A writer function is responsible for writing formatted log data
 * to a sink. Sink could be anything, from FILE* to a HTTP API endpoint,
 * so this is why it is a customizable function.
 *
 * @param user_data  Data for writing logs (FILE *, API endpoint, etc.)
 * @param fmt        String to output (can be printf format string)
 * @param va         List of arguments. Can be null, and arguments can be provided as variadics
 * @return Total number of characters written to the sink by writer
 */
typedef size_t(Logcie_WriterFn)(void *user_data, const char *fmt, va_list *va, ...);

/**
 * @brief Writer struct
 *
 * Stores writer function pointer and custom data for it
 *
 * @param write  Writer function pointer
 * @param data   Custom data for writer function
 */
typedef struct Logcie_Writer {
  Logcie_WriterFn *write;
  void            *data;
} Logcie_Writer;

/**
 * @brief Formatter function type signature.
 *
 * A formatter function is responsible for converting a Logcie_Log structure
 * into formatted output that would be written to a sink. It should call writer->write
 * with formatted chunks of log to write them to a sink.
 *
 * @param writer     Pointer to writer (see Logcie_Writer)
 * @param user_data  Data for formatting logs (Format string, options flags, current phase of the moon, etc)
 * @param log        Log to format
 * @param va         Variadic arguments that was passed to logging function (LOGCIE_INFO("message %s", "this would be in va")
 * @return Number of characters written to the sink
 */
typedef size_t(Logcie_FormatterFn)(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args);

/**
 * @brief Formatter struct
 *
 * Stores formatter function pointer and custom data for it
 *
 * @param format  Formatter function pointer
 * @param data    Custom data for writer function
 */
typedef struct Logcie_Formatter {
  Logcie_FormatterFn *format;
  void               *data;
} Logcie_Formatter;

/**
 * @brief Filter function type signature.
 *
 * A filter function determines whether a log message should be emitted
 * to a particular sink. Return 1 (true) to allow the log, 0 (false) to
 * suppress it.
 *
 * @param data  Data for filtering (required log level, string to compare to, etc.)
 * @param log   Log metadata to evaluate
 * @return      1 to emit log, 0 to suppress
 */
typedef uint8_t(Logcie_FilterFn)(const void *data, Logcie_Log *log);

/**
 * @brief Filter struct
 *
 * Stores filter function pointer and custom data for it
 *
 * @param filter  Filter function pointer
 * @param data    Custom data for filter function
 */
typedef struct Logcie_Filter {
  Logcie_FilterFn *filter;
  const void      *data;
} Logcie_Filter;

/**
 * @brief Structure representing a single log sink (output target).
 *
 * A sink defines where log messages are written and how they are formatted.
 * Multiple sinks can be active simultaneously, each with different formatting,
 * write target and filtering rules.
 *
 * @field formatter  Formatter that will format logs
 * @field writer     Writer that will write logs
 * @field filter     Filter for filtering logs
 */
struct Logcie_Sink {
  Logcie_Formatter formatter;
  Logcie_Writer    writer;
  Logcie_Filter    filter;
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
  uint32_t    line;
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
  Logcie_LogLevel    level;
  const char        *msg;
  time_t             time;
  const char        *module;
  Logcie_LogLocation location;
};

// Helper macro for constructing a log message
#define LOGCIE_CREATE_LOG(lvl, txt, f, l) \
  (Logcie_Log) {                          \
    .level    = lvl,                      \
    .msg      = txt,                      \
    .time     = time(NULL),               \
    .module   = logcie_module,            \
    .location = {                         \
      .file = f,                          \
      .line = l,                          \
    }                                     \
  }

#ifndef PRINTF_TYPECHECK
#if defined __has_attribute && __has_attribute(__format__)
#define PRINTF_TYPECHECK(a, b) __attribute__((__format__(__printf__, a, b)))
#else
#define PRINTF_TYPECHECK(a, b)
#endif
#endif

/**
 * @brief Convenience macros for each log level.
 * These use __FILE__ and __LINE__ to capture call site.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
#define LOGCIE_TRACE(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_DEBUG(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_VERBOSE(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_INFO(msg, ...)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_WARN(msg, ...)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_ERROR(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_FATAL(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL, msg, __FILE__, __LINE__), msg, __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_LOG(level, msg, ...) LOGCIE_##level(msg, __VA_OPT__(, ) __VA_ARGS__)
#else
#if !defined(LOGCIE_PEDANTIC) && (defined(__GNUC__) || defined(__clang__))
#define LOGCIE_TRACE(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_DEBUG(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_VERBOSE(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_INFO(msg, ...)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_WARN(msg, ...)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_ERROR(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_FATAL(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_LOG(level, msg, ...) LOGCIE_##level(msg, ##__VA_ARGS__)
#else
#define LOGCIE_TRACE(msg)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE, msg, __FILE__, __LINE__), msg)
#define LOGCIE_DEBUG(msg)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG, msg, __FILE__, __LINE__), msg)
#define LOGCIE_VERBOSE(msg)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg)
#define LOGCIE_INFO(msg)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO, msg, __FILE__, __LINE__), msg)
#define LOGCIE_WARN(msg)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN, msg, __FILE__, __LINE__), msg)
#define LOGCIE_ERROR(msg)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR, msg, __FILE__, __LINE__), msg)
#define LOGCIE_FATAL(msg)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL, msg, __FILE__, __LINE__), msg)
#define LOGCIE_LOG(level, msg) LOGCIE_##level(msg)
#define LOGCIE_VA_LOGS
#endif
#endif

// Separate variadic logs for compilers that do not support optional variadics in macros
#ifdef LOGCIE_VA_LOGS
#define LOGCIE_TRACE_VA(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_TRACE, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_DEBUG_VA(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_DEBUG, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_VERBOSE_VA(msg, ...)    logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_VERBOSE, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_INFO_VA(msg, ...)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_INFO, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_WARN_VA(msg, ...)       logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_WARN, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_ERROR_VA(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_ERROR, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_FATAL_VA(msg, ...)      logcie_log(LOGCIE_CREATE_LOG(LOGCIE_LEVEL_FATAL, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_LOG_VA(level, msg, ...) LOGCIE_##level##_VA(msg, __VA_ARGS__)
#endif

/**
 * @brief Emit a log message using the provided log metadata and arguments.
 *
 * This is the core logging function that processes log messages through all
 * registered sinks. Each sink applies its own filtering and formatting rules.
 * This function is typically called via the LOGCIE_* macros and should not
 * be called directly in most cases.
 *
 * @param log Log metadata structure containing level, timestamp, location, etc.
 * @param fmt Format string for the log message (supports printf-style formatting)
 * @param ... Variable arguments for format string placeholders
 * @return Always returns 0 (reserved for future use)
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
 * @param index Zero-based index of the sink to retrieve (0 = default stdout sink)
 * @return Pointer to the Logcie_Sink, or NULL if index is invalid
 */
LOGCIE_DEF Logcie_Sink *logcie_get_sink(size_t index);

/**
 * @brief Adds a new sink to the logger.
 *
 * The sink will receive all log messages that pass its minimum level and
 * filter criteria. Multiple sinks can be active simultaneously with different
 * configurations.
 *
 * @param sink Pointer to a Logcie_Sink structure to add
 * @return 1 if sink was added, 0 otherwise
 */
LOGCIE_DEF uint8_t logcie_add_sink(Logcie_Sink *sink);

/**
 * @brief Removes a sink from the logger by pointer.
 *
 * Searches for the sink by pointer equality and removes it from the sink list.
 * The sink structure is not freed; caller must manage memory.
 *
 * @param sink Pointer to the sink to remove
 * @return 1 if sink was found and removed, 0 otherwise
 * @note The sink memory is not freed by this function. Caller is responsible.
 * @note Cannot remove the default stdout sink (always returns 0 for it)
 */
LOGCIE_DEF uint8_t logcie_remove_sink(Logcie_Sink *sink);

/**
 * @brief Removes a sink from the logger by index.
 *
 * Removes the sink at the specified index. Indices are 0-based and
 * correspond to the order sinks were added.
 *
 * @param index Zero-based index of the sink to remove
 * @return 1 if sink was found and removed, 0 otherwise
 * @note The sink memory is not freed by this function. Caller is responsible.
 * @note Index 0 is the default stdout sink and cannot be removed
 */
LOGCIE_DEF uint8_t logcie_remove_sink_by_index(size_t index);

/**
 * @brief Removes and frees a sink from the logger (if it was dynamically allocated).
 *
 * Combines logcie_remove_sink() with free() for convenience when working
 * with heap-allocated sinks.
 *
 * @param sink Pointer to the sink to remove and free
 * @return 1 if sink was found and removed, 0 otherwise
 * @note Only use this if the sink was allocated with malloc() or similar.
 */
LOGCIE_DEF uint8_t logcie_remove_and_free_sink(Logcie_Sink *sink);

/**
 * @brief Removes all sinks except the default stdout sink.
 *
 * Resets the logger to its initial state with only the default stdout sink.
 * Useful for cleanup or reconfiguration scenarios.
 *
 * @note Sink memory is not freed. Caller is responsible for cleanup.
 * @note File streams are not closed. Caller must close them separately.
 */
LOGCIE_DEF void logcie_remove_all_sinks(void);

/**
 * @brief Default formatter using printf-style formatting and $ tokens.
 *
 * This is the built-in formatter that provides rich formatting capabilities
 * using $ tokens. It supports timestamps, colors, file locations, modules,
 * and custom formatting.
 *
 * Here is the list of all formatting tokens:
 *
 * `$m` - Log message with printf formatting
 * `$f` - Source file name
 * `$x` - Line number
 * `$M` - Module name
 * `$l` - Log level (lowercase)
 * `$L` - Log level (uppercase)
 * `$c` - ANSI color code for log level
 * `$r` - ANSI reset color code
 * `$d` - Date (YYYY-MM-DD)
 * `$t` - Time (HH:MM:SS)
 * `$z` - Timezone offset
 * `$<n - Pads with n spaces
 * `$$` - Literal dollar sign
 *
 * @param writer     Pointer to writer (see Logcie_Writer)
 * @param user_data  Pointer to format string
 * @param log        Lot to format
 * @param va         Variadic arguments that was passed to logging function (LOGCIE_INFO("message %s", "this would be in va")
 * @return Number of characters written to the sink
 */
LOGCIE_DEF size_t logcie_printf_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args);

/**
 * @brief Default printf writer
 *
 * This is the built-in writer that writes logs using vfprintf
 *
 * @param user_data  Pointer to FILE where logs would be written
 * @param fmt        String to output (can be printf format string)
 * @param va         List of arguments. Can be null, and arguments can be provided as variadics
 * @return Total number of characters written to the sink by writer
 */
LOGCIE_DEF size_t logcie_printf_writer(void *user_data, const char *fmt, va_list *va, ...);

typedef struct Logcie_FilterCombinationData {
  Logcie_Filter a;
  Logcie_Filter b;
} Logcie_FilterCombinationData;

/**
 * @brief Filters out logs with two different filters combining them with 'or' function
 * @param data *Logcie_FilterCombinationData
 */
LOGCIE_DEF uint8_t logcie_filter_or_fn(const void *data, Logcie_Log *log);

/**
 * @brief Filters out logs with two different filters combining them with 'and' function
 * @param data *Logcie_FilterCombinationData
 */
LOGCIE_DEF uint8_t logcie_filter_and_fn(const void *data, Logcie_Log *log);

/**
 * @brief Negates result of a filter
 * @param data *Logcie_Filter
 */
LOGCIE_DEF uint8_t logcie_filter_not_fn(const void *data, Logcie_Log *log);

/**
 * @brief Filters out logs if log level is less than specified level
 * @param data *Logcie_LogLevel
 */
LOGCIE_DEF uint8_t logcie_filter_level_min_fn(const void *data, Logcie_Log *log);

/**
 * @brief Filters out logs if log level is more than specified level
 * @param data *Logcie_LogLevel
 */
LOGCIE_DEF uint8_t logcie_filter_level_max_fn(const void *data, Logcie_Log *log);

/**
 * @brief Filters out logs if log module is equal to specified string
 * @param data cosnt char*
 */
LOGCIE_DEF uint8_t logcie_filter_module_eq_fn(const void *data, Logcie_Log *log);

/**
 * @brief Filters out logs if log messages contains specified string
 * @param data const char*
 */
LOGCIE_DEF uint8_t logcie_filter_message_contains_fn(const void *data, Logcie_Log *log);

typedef uint8_t(Logcie_FilterCustomPredicateFn)(Logcie_Log *log);

// Some handy filter "constructors"

#ifdef __cplusplus
#define logcie_filter_and(a, b)                                      \
  Logcie_Filter {                                                    \
    .filter = [](const void *data, Logcie_Log *log) -> uint8_t {     \
      (void)data;                                                    \
      return (a).filter((a).data, log) && (b).filter((b).data, log); \
    },                                                               \
    .data = NULL,                                                    \
  }

#define logcie_filter_or(a, b)                                       \
  Logcie_Filter {                                                    \
    .filter = [](const void *data, Logcie_Log *log) -> uint8_t {     \
      (void)data;                                                    \
      return (a).filter((a).data, log) || (b).filter((b).data, log); \
    },                                                               \
    .data = NULL,                                                    \
  }
#else
#define logcie_filter_and(a, b)                         \
  ((Logcie_Filter){                                     \
    .filter = logcie_filter_and_fn,                     \
    .data   = &(Logcie_FilterCombinationData){(a), (b)} \
  })

#define logcie_filter_or(a, b)                          \
  ((Logcie_Filter){                                     \
    .filter = logcie_filter_or_fn,                      \
    .data   = &(Logcie_FilterCombinationData){(a), (b)} \
  })
#endif

#define logcie_filter_not(f)        \
  ((Logcie_Filter){                 \
    .filter = logcie_filter_not_fn, \
    .data   = &f                    \
  })

#define logcie_filter_level_min(level)    \
  ((Logcie_Filter){                       \
    .filter = logcie_filter_level_min_fn, \
    .data   = (void *)(level)             \
  })

#define logcie_filter_level_max(level)    \
  ((Logcie_Filter){                       \
    .filter = logcie_filter_level_max_fn, \
    .data   = &(Logcie_LogLevel){(level)} \
  })

#define logcie_filter_module_eq(module)   \
  ((Logcie_Filter){                       \
    .filter = logcie_filter_module_eq_fn, \
    .data   = (module)                    \
  })

#define logcie_filter_message_contains(substr)   \
  ((Logcie_Filter){                              \
    .filter = logcie_filter_message_contains_fn, \
    .data   = (substr)                           \
  })

/**
 * @brief Allows customization of log level colors. Must be array of size Count_LOGCIE_LEVEL.
 *
 * Override the default ANSI color codes for each log level. The array must
 * contain exactly Count_LOGCIE_LEVEL (7) elements. Pass NULL to reset to defaults.
 *
 * @param colors Array of ANSI color code strings, one per log level in order:
 *               [TRACE, DEBUG, VERBOSE, INFO, WARN, ERROR, FATAL]
 *               or NULL to reset to defaults
 * @note Colors are applied globally to all sinks using the default formatter
 * @note Custom formatters may ignore these colors
 * @note I can not check if colors array is valid in runtime. I did what I can,
 *       but still try to compile if -fsanitize=address to check if everything is all right
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

#ifndef _LOGCIE_ASSERT
#define _LOGCIE_ASSERT(bool, msg) assert(bool && msg)
#endif

#ifdef _LOGCIE_DEBUG
#if __STDC_VERSION__ >= 201112L  // Check for C11 support
#define _LOGCIE_DEBUG_ASSERT(bool, msg) static_assert(bool, msg)
#else
#define _LOGCIE_DEBUG_ASSERT(bool, msg) _LOGCIE_ASSERT(bool, msg)
#endif
#else
#define _LOGCIE_DEBUG_ASSERT(bool, msg)
#endif

#ifndef _LOGCIE_ARR_LEN
#define _LOGCIE_ARR_LEN(array) ((int)sizeof(array) / (int)sizeof((array)[0]))
#endif

static const char *logcie_level_label[] = {
  "trace",
  "debug",
  "verb",
  "info",
  "warn",
  "error",
  "fatal",
};

static inline const char *get_logcie_level_label(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
  return logcie_level_label[level];
}

static const char *logcie_level_label_upper[] = {
  "TRACE",
  "DEBUG",
  "VERB",
  "INFO",
  "WARN",
  "ERROR",
  "FATAL",
};

static inline const char *get_logcie_level_label_upper(Logcie_LogLevel level) {
  _LOGCIE_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  _LOGCIE_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
  return logcie_level_label_upper[level];
}

static const char *logcie_default_level_color[] = {
  LOGCIE_COLOR_GRAY,
  LOGCIE_COLOR_GRAY,
  LOGCIE_COLOR_GRAY,
  LOGCIE_COLOR_BLUE,
  LOGCIE_COLOR_YELLOW,
  LOGCIE_COLOR_RED,
  LOGCIE_COLOR_BRIGHT_RED,
};

static const char **logcie_level_color = logcie_default_level_color;

void logcie_set_colors(const char **colors) {
  if (colors) {
    // If compiled with -fsanitize=address and colors array is wrong it will crash
    // If it is compiled without -fsanitize=address then color would be NULL (I hope)
    const char *color = colors[Count_LOGCIE_LEVEL - 1];
    _LOGCIE_ASSERT(color != NULL, "Size of array of colors in logcie_set_colors is not equal to Count_LOGCIE_LEVEL");
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
  .formatter = {logcie_printf_formatter, (void *)("$c$L$<6$r " LOGCIE_COLOR_GRAY "$f:$x$r: $m")},
  .writer    = {logcie_printf_writer, NULL},
  .filter    = {NULL, NULL},
};

static Logcie_Sink *default_stdout_sink_ptr = &default_stdout_sink;

#if defined(__has_attribute) && __has_attribute(constructor)
#define __L_ATTR_CONSTRUCT
#endif

#ifdef __L_ATTR_CONSTRUCT
__attribute__((constructor)) void init_default_stdout_sink(void) {
  default_stdout_sink.writer.data = stdout;
}
#endif

typedef struct Logcie_Logger {
  Logcie_Sink **sinks;
  size_t        sinks_len;
  size_t        sinks_cap;
} Logcie_Logger;

// INFO: By default there is one default sink to allow logging right
//       after includnig Logcie without initializing anything
static Logcie_Logger logcie = {
  .sinks     = &default_stdout_sink_ptr,
  .sinks_len = 1,
  .sinks_cap = 1,
};

size_t logcie_get_sink_count(void) {
  return logcie.sinks_len;
}

Logcie_Sink *logcie_get_sink(size_t index) {
  if (index >= logcie.sinks_len) {
    return NULL;
  }

  return logcie.sinks[index];
}

uint8_t logcie_add_sink(Logcie_Sink *sink) {
  if (sink == NULL) {
    return 0;
  }

#ifndef __L_ATTR_CONSTRUCT
  if (sink->writer.data == NULL)
    sink->writer.data = stdout;
#endif

  if (logcie.sinks_cap == 1) {
    logcie.sinks_cap = 8;
    logcie.sinks_len = 0;
    logcie.sinks     = (Logcie_Sink **)malloc(sizeof(*logcie.sinks) * logcie.sinks_cap);
  }

  if (logcie.sinks_cap == logcie.sinks_len) {
    logcie.sinks_cap *= 2;
    logcie.sinks = (Logcie_Sink **)realloc(logcie.sinks, sizeof(*logcie.sinks) * logcie.sinks_cap);
  }

  logcie.sinks[logcie.sinks_len] = sink;
  logcie.sinks_len++;

  return 1;
}

uint8_t logcie_remove_sink(Logcie_Sink *sink) {
  if (sink == &default_stdout_sink) {
    return 0;  // unreachable
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
  if (logcie_remove_sink(sink)) {
    free(sink);
    return 1;
  }

  return 0;
}

void logcie_remove_all_sinks(void) {
  if (logcie.sinks_cap > 1) {
    free(logcie.sinks);
    logcie.sinks_cap = 1;
    logcie.sinks_len = 1;
    logcie.sinks     = &default_stdout_sink_ptr;
    return;
  }
}

size_t logcie_log(Logcie_Log log, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log.msg = fmt;

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    Logcie_Sink *sink = logcie.sinks[i];
    _LOGCIE_ASSERT(sink && sink->formatter.format, "Sink have no formatter");

    if (sink->filter.filter && !sink->filter.filter(sink->filter.data, &log)) {
      continue;
    }

    va_list args_copy;
    va_copy(args_copy, args);

    sink->formatter.format(&sink->writer, sink->formatter.data, log, &args_copy);

    va_end(args_copy);
  }

  va_end(args);
  return 0;
}

size_t logcie_printf_formatter(Logcie_Writer *writer, void *data, Logcie_Log log, va_list *args) {
  const char *fmt = (const char *)data;
  _LOGCIE_ASSERT(writer, "Sink have no writer");
  _LOGCIE_ASSERT(writer->data, "printf sink have nowhere to print");

  size_t output_len = 0;
  size_t last_len   = 0;

  struct tm local_tm = *localtime(&log.time);
  struct tm utc_tm   = *gmtime(&log.time);

  int32_t local_hours = local_tm.tm_hour;
  int32_t timediff    = (int32_t)difftime(mktime(&local_tm), mktime(&utc_tm)) / 3600;

  while (*fmt != '\0') {
    if (*fmt != '$') {
      output_len += writer->write(writer->data, "%c", NULL, *fmt);
      fmt++;
      continue;
    }

    fmt++;

    if (*fmt == '\0') {
      break;
    }

    switch (*fmt) {
      case '$':
        last_len = writer->write(writer->data, "$", NULL);
        break;
      case 'm':
        last_len = writer->write(writer->data, log.msg, args);
        break;
      case 'l':
        last_len = writer->write(writer->data, "%s", NULL, get_logcie_level_label(log.level));
        break;
      case 'L':
        last_len = writer->write(writer->data, "%s", NULL, get_logcie_level_label_upper(log.level));
        break;
      case 'c':
        last_len = writer->write(writer->data, "%s", NULL, get_logcie_level_color(log.level));
        break;
      case 'r':
        last_len = writer->write(writer->data, LOGCIE_COLOR_RESET, NULL);
        break;
      case 'd':
        last_len = writer->write(writer->data, "%d-%02d-%02d", NULL, local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday);
        break;
      case 't':
        last_len = writer->write(writer->data, "%02d:%02d:%02d", NULL, local_hours, local_tm.tm_min, local_tm.tm_sec);
        break;
      case 'z':
        last_len = writer->write(writer->data, "%+d", NULL, timediff);
        break;
      case 'f':
        last_len = writer->write(writer->data, "%s", NULL, log.location.file);
        break;
      case 'x':
        last_len = writer->write(writer->data, "%u", NULL, log.location.line);
        break;
      case 'M':
        last_len = writer->write(writer->data, "%s", NULL, log.module ? log.module : default_module);
        break;
      case '<': {
        fmt++;
        uint16_t target = 0;

        while (*fmt >= '0' && *fmt <= '9') {
          target = target * 10 + (*fmt - '0');
          fmt++;
        }

        int16_t pad = target - last_len - 1;

        if (pad > 0) {
          last_len = writer->write(writer->data, "%*s", NULL, pad, "");
        }

        fmt--;
        break;
      }
      default:
        fprintf(stderr, "%sWARN: unknown format sequence '$%c'. Skipping...\n" LOGCIE_COLOR_RESET, get_logcie_level_color(LOGCIE_LEVEL_WARN), *fmt);
        break;
    }

    output_len += last_len;
    fmt++;
  }

  output_len += writer->write(writer->data, "\n", NULL);
  return output_len;
}

// TODO: logcie_writer_flush()???

LOGCIE_DEF size_t logcie_printf_writer(void *user_data, const char *fmt, va_list *va, ...) {
  _LOGCIE_ASSERT(user_data, "Printf writer have nothing to write to");
  FILE   *file = (FILE *)user_data;
  va_list args;

  if (va != NULL) {
    va_copy(args, *va);
  } else {
    va_start(args, va);
  }

  size_t written = vfprintf(file, fmt, args);

  va_end(args);
  return written;
}

LOGCIE_DEF uint8_t logcie_filter_not_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_not'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_not'");
  Logcie_Filter *filter = (Logcie_Filter *)data;
  return !filter->filter(filter->data, log);
}

LOGCIE_DEF uint8_t logcie_filter_and_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_and'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_and'");
  Logcie_FilterCombinationData *d = (Logcie_FilterCombinationData *)data;
  return d->a.filter(d->a.data, log) && d->b.filter(d->b.data, log);
}

LOGCIE_DEF uint8_t logcie_filter_or_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_or'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_or'");
  Logcie_FilterCombinationData *d = (Logcie_FilterCombinationData *)data;
  return d->a.filter(d->a.data, log) || d->b.filter(d->b.data, log);
}

LOGCIE_DEF uint8_t logcie_filter_level_min_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT((uintptr_t)data < Count_LOGCIE_LEVEL, "Param 'data' is not correct for filter 'logcie_filter_level_max'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_level_min'");
  Logcie_LogLevel level = (Logcie_LogLevel)(uintptr_t)data;
  return log->level >= level;
}

LOGCIE_DEF uint8_t logcie_filter_level_max_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT((uintptr_t)data < Count_LOGCIE_LEVEL, "Param 'data' is not correct for filter 'logcie_filter_level_max'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_level_max'");
  Logcie_LogLevel level = (Logcie_LogLevel)(uintptr_t)data;
  return log->level <= level;
}

LOGCIE_DEF uint8_t logcie_filter_module_eq_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_module_eq'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_module_eq'");
  const char *module = (const char *)data;
  return log->module && strcmp(module, log->module) == 0;
}

LOGCIE_DEF uint8_t logcie_filter_message_contains_fn(const void *data, Logcie_Log *log) {
  _LOGCIE_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_message_contains'");
  _LOGCIE_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_message_contains'");
  const char *str = (const char *)data;
  return log->msg && strstr(log->msg, str);
}

// TODO: Abiblity to accept custom stuff in logging (logging arrays)

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
