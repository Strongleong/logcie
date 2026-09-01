/*
 * Logcie v2.0.0 - Logging Library (Single Header)
 *
 * Description:
 *   Logcie is a lightweight, modular, single-header logging library written in C.
 *   It supports multiple log levels, ANSI color output, flexible formatting, and
 *   customizable filters and sinks for advanced logging use cases.
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
 * Configuration macros (define before including the header):
 *   LOGCIE_MODULE                  Module name for classic macros (default "Logcie")
 *   LOGCIE_MODULE_SEPARATOR        Moudle names separator for hierarchical module filtering (default '.')
 *   LOGCIE_MAX_SINKS               Maximum capacity of logcie sinks array (default: 16)
 *   LOGCIE_AUTOFLUSH_LEVEL         Maximum log level that fill automatically call flush (default: LOGCIE_LEVEL_ERROR)
 *   LOGCIE_AUTOFLUSH_DISABLE       Diables autoflush. Define to enable (default: not defined)
 *   LOGCIE_MAX_LINE                Stack buffer a log line is formatted into (default: 1024)
 *   LOGCIE_MALLOC / LOGCIE_FREE    Allocator for lines longer than LOGCIE_MAX_LINE
 *   LOGCIE_NO_MALLOC               Never allocate; truncate long lines instead
 *   LOGCIE_DEFAULT_SINK_FORMAT     Format string for the automatic stdout sink
 *   LOGCIE_DEF                     Linkage of public functions (default extern)
 *   LOGCIE_THREAD_SAFE             Enable mutex‑based thread safety (needs pthreads)
 *   LOGCIE_ALLOW_RECURSIVE_LOGGING Allow logging from inside writers/formatters
 *   LOGCIE_PEDANTIC                Force strict C99 fallback (LOGCIE_*_VA macros)
 *   LOGCIE_COLOR_*                 ANSI escape codes per level (also logcie_set_colors)
 *   LOGCIE_DEBUG_CHECKS            Enable internal consistency assertions
 *
 *   Anything named LOGCIE_INTERNAL_* is an implementation detail. It is not a
 *   configuration point and may change in any release.
 *
 *   The library automatically defines LOGCIE_VA_LOGS when variadic macros are not
 *   available – you do not need to touch it.
 *
 * How it works:
 *   The core of this library is `Formatter`, `Writer` and `Filter`.
 *
 *   Formatter turns a Logcie_Log into bytes. It owns the serialization: the
 *             built-in one renders $ tokens into readable text, but a JSON or
 *             a binary formatter is the same interface with different output.
 *             Its user_data is whatever that formatter needs -- a token string
 *             for logcie_token_formatter, possibly nothing for others.
 *   Writer receives one complete formatted line per call, including the terminating
 *          newline, and puts it somewhere. It is never handed a fragment, so a sink that
 *          treats each call as one record -- syslog, a network endpoint, a ring buffer --
 *          is safe. It also gets the log itself, for metadata a transport needs as a
 *          separate value rather than as text. A NULL user_data discards, which is
 *          /dev/null without the open().
 *          Also writer has a flush. A flush is basically an fflush.
 *   Filter decides whether a log reaches the Sink at all. See the Filters section below for the built-i
 *          ones and how to combine them.
 *
 *   A combination of Formatter, Writer and Filter is called a Sink.
 *   You can register up to LOGCIE_MAX_SINKS sinks. Logcie sends every log to every sink available.
 *
 *   Logcie itself is basically an array of Sinks and system of distributing logs to those Sinks.
 *
 *   NOTE: Recursive logging from formatters, writers or filters is not supported!
 *
 *  By default Logcie suppresses recursive log attempts to avoid infinite recursion
 *  and deadlocks. Recursive calls return 0 and produce no output.
 *
 *  If you want to avoid the small overhead of the recursion check,
 *  or if you intentionally rely on recursive logging and you know what you are doing
 *  you can disable the recursion guard:
 *   ```c
 *   #define LOGCIE_ALLOW_RECURSIVE_LOGGING
 *   ```
 *  Disabling the recursion guard may cause infinite recursion, deadlocks,
 *  or stack overflows.
 *
 *   Thread safety:
 *     Thread safety is opt‑in. You can enable it like this:
 *      ```c
 *      #define LOGCIE_THREAD_SAFE
 *      ```
 *     Without it, concurrent calls may interleave or crash.
 *     But even with the thread safety enabled, removing a sink while
 *     threads are logging is unsafe – set up sinks before starting threads
 *     and tear them down after joining.
 *
 * Defaults:
 *   It would not be that great if Logcie was just empty framework and you need to set it up by yourself,
 *   so Logcie comes with a couple of pre-defined functions:
 *
 *      - logcie_file_writer      - built-in writer. Writes the formatted line to a FILE * and have a fflush as flush
 *      - logcie_token_formatter - built-in formatter that provides rich formatting using $ tokens. Here is the list:
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
 *                                   `$N` - Nanoseconds
 *                                   `$z` - Timezone offset
 *                                   `$<n - Pads the previous token out to n columns
 *                                   `$$` - Literal dollar sign
 *
 *   Also by default, Logcie already has a Sink installed with the printf writer and formatter,
 *   so you can start using it immediately after including the library.
 *   You can configure it to your liking wiht `logcie_get_default_sink()`, or remove it
 *   with `logcie_remove_all_sinks()`, `logcie_remove_sink_by_index(0)` or `logcie_remove_sink(logcie_get_default_sink())`
 *
 * Colors:
 *   As you can see, `logcie_token_formatter()` has support for ANSI colored output. It have
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
 *   A filter is a structure that consist of pointer to filtering function and
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
 *    - logcie_filter_module_prefix_eq("module")
 *        Allows logs only from specific module root (see below for learning about modules)
 *
 *    - logcie_filter_message_contains("text")
 *        Allows logs whose messages contains the given substring
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
 *        Inverts the result of a filter
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
 *     - Filters are evaluated per sink, independently.
 *     - Be careful when using temporary data in filters (they rely on
 *       compound literals and must remain valid during logging).
 *
 * Modules:
 *   A module is a string label that identifies the origin of a log message,
 *   such as "network", "core", or "database". It can be displayed with the
 *   `$M` token and used in filters.
 *
 *   Logcie supports three ways to set the module, from simplest to most
 *   explicit:
 *
 *   1. Per‑file default (via macro):
 *      ```c
 *      #define LOGCIE_MODULE "core"
 *      #include "logcie.h"
 *      ```
 *      All classic macros (LOGCIE_INFO, LOGCIE_ERROR, …) in that file will
 *      be tagged with "core".
 *
 *   2. Per‑call explicit module:
 *      ```c
 *      LOGCIE_LOG_MOD("network", INFO, "Connected");
 *      ```
 *      (Use LOGCIE_LOG_MOD_VA when variadic macros are unavailable.)
 *      This does not affect any other log line.
 *
 *   3. Library integration (recommended for header‑only libraries):
 *      ```c
 *      #ifndef YOURLIB_LOG
 *        #ifdef LOGCIE
 *          #ifdef LOGCIE_VA_LOGS
 *            #define YOURLIB_LOG(level, msg, ...) LOGCIE_LOG_MOD_VA("YOURLIB", level, msg, __VA_ARGS__)
 *          #else
 *            #define YOURLIB_LOG(level, ...)      LOGCIE_LOG_MOD("YOURLIB", level, __VA_ARGS__)
 *          #endif
 *        #else
 *          #define YOURLIB_LOG(level, ...) (void*)0
 *        #endif
 *      #endif
 *      ```
 *      If you need to have fallback logging this can be used instead of `(void *)0`:
 *
 *      ```c
 *      #define YOURLIB_LOG(level, ...)                \
 *         do {                                       \
 *           fprintf(stderr, #level ": "__VA_ARGS__); \
 *           fprintf(stderr, "\n");                   \
 *         } while (0)
 *      ```
 *
 *      Just change YOURLLIB to something more fitting, then call `MYLIB_LOG(INFO, "Library ready");`.
 *      The module string `"mylib"` is fixed and never clashes with user code.
 *
 *   You can display the module in your logs using the `$M` format token:
 *     "$d $t [$L] ($M) $m"
 *   produces
 *     2026-03-25 12:00:00 [INFO] (network) Connection established
 *
 *   Modules can also be used in filters to selectively allow or block logs
 *   from specific parts of your application.
 *
 *   Module names are hierarchical: "net", "net.http" and "net.http.tls" form a tree that logcie_filter_module_prefix
 *   matches on. Redefine this before including logcie.h if '.' clashes with your naming.
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
 *     // NOTE: stdout and fopen() are not constant expressions, so a sink
 *     //       cannot carry them in a file-scope initializer. Declare the sink
 *     //       there and fill the writer target in at run time.
 *     Logcie_Sink stdout_sink = {
 *       .formatter = { logcie_token_formatter, "[$L] $m" },
 *       .writer = { logcie_file_writer, logcie_file_flush, NULL },
 *       .filter = { logcie_filter_level_min_fn, LOGCIE_LEVEL_INFO }
 *     };
 *
 *     Logcie_Sink file_sink = {
 *       .formatter = { logcie_token_formatter, "$L:$f:$x: $m ($t $d)" },
 *       .writer = { logcie_file_writer, logcie_file_flush, NULL },
 *     };
 *
 *     int main(void) {
 *       stdout_sink.writer.data = stdout;
 *       file_sink.writer.data   = fopen("./log.txt", "w");
 *
 *       // New sinks must be registered with `logcie_add_sink()`
 *       logcie_add_sink(&stdout_sink);
 *       logcie_add_sink(&file_sink);
 *     }
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
#define LOGCIE_VERSION_MAJOR         2
#define LOGCIE_VERSION_MINOR         0
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
#define LOGCIE_DEFAULT_SINK_FORMAT "$c$L$<6$r " LOGCIE_COLOR_GRAY "$f:$x$r: $m"
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
 * @brief Default module name for classic LOGCIE_* macros.
 *
 * Define this macro **before** including logcie.h to set the module for all
 * classic log calls in the file. If not defined, `"Logcie"` is used.
 *
 * @note This macro has no effect on LOGCIE_LOG_MOD / LOGCIE_LOG_MOD_VA,
 *       which always take an explicit module argument.
 *
 * Example:
 * @code
 * #define LOGCIE_MODULE "network"
 * #include "logcie.h"
 * @endcode
 */
#ifndef LOGCIE_MODULE
#define LOGCIE_MODULE "Logcie"
#endif

/**
 * @brief Character separating the levels of a module name.
 *
 * Module names are hierarchical: "net", "net.http" and "net.http.tls" form a
 * tree that logcie_filter_module_prefix matches on. Redefine this before
 * including logcie.h if '.' clashes with your naming.
 *
 * Example:
 * @code
 * #define LOGCIE_MODULE_SEPARATOR '/'
 * #include "logcie.h"
 * @endcode
 */
#ifndef LOGCIE_MODULE_SEPARATOR
#define LOGCIE_MODULE_SEPARATOR '.'
#endif

/**
 * @brief Size of the stack buffer a log line is formatted into.
 *
 * A line that fits costs no allocation at all. Longer lines go through
 * LOGCIE_MALLOC, or are truncated when there is none.
 */
#ifndef LOGCIE_MAX_LINE
#define LOGCIE_MAX_LINE 1024
#endif

/**
 * @brief Allocator used only for log lines longer than LOGCIE_MAX_LINE.
 *
 * Define both LOGCIE_MALLOC and LOGCIE_FREE to route those rare long lines
 * through your own allocator -- an arena, a ring buffer, a debug allocator:
 *
 * @code
 * #define LOGCIE_MALLOC(size) my_arena_alloc(size)
 * #define LOGCIE_FREE(ptr)    my_arena_free(ptr)
 * #include "logcie.h"
 * @endcode
 *
 * Define LOGCIE_NO_MALLOC to forbid allocation outright, which is what you
 * want where dynamic allocation is banned. Lines longer than LOGCIE_MAX_LINE
 * are then truncated instead of allocated.
 */
#if defined(LOGCIE_MALLOC) != defined(LOGCIE_FREE)
#error "Define both LOGCIE_MALLOC and LOGCIE_FREE, or neither"
#endif

#if !defined(LOGCIE_MALLOC) && !defined(LOGCIE_NO_MALLOC)
#define LOGCIE_MALLOC(size) malloc(size)
#define LOGCIE_FREE(ptr)    free(ptr)
#endif

/**
 * @brief Maximum number of sinks that can be registered at once.
 *
 * Sinks live in a fixed array, so logcie never allocates. Raise this before
 * including logcie.h if sixteen is not enough; logcie_add_sink returns 0 once
 * the array is full.
 */
#ifndef LOGCIE_MAX_SINKS
#define LOGCIE_MAX_SINKS 16
#endif

/**
 * @brief Maximum log level that fill automatically call flush
 */
#ifndef LOGCIE_AUTOFLUSH_LEVEL
#define LOGCIE_AUTOFLUSH_LEVEL LOGCIE_LEVEL_ERROR
#endif

#ifdef LOGCIE_AUTOFLUSH_DISABLE
#undef LOGCIE_AUTOFLUSH_LEVEL
#define LOGCIE_AUTOFLUSH_LEVEL Count_LOGCIE_LEVEL
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
 * A writer puts an already formatted line somewhere. That can be a FILE *, an
 * HTTP endpoint, a ring buffer, or a UART on an embedded target.
 *
 * The log is passed alongside the bytes so a writer can reach the metadata a
 * transport needs as a separate value rather than as text.
 *
 * @param user_data  Destination for this writer (FILE *, socket, ...)
 * @param log        Metadata of the line being written
 * @param bytes      Formatted line, including its terminating newline
 * @param len        Number of bytes
 * @return Number of bytes written
 *
 * @note One call is one complete line. A writer is never handed a fragment, so
 *       a sink that treats each call as one record is safe.
 * @note log->msg is the format string as written at the call site, not the
 *       text. The rendered line is `bytes` -- use that. `log` is there for
 *       metadata: level, module, timestamp, location.
 */
typedef size_t(Logcie_WriterFn)(void *user_data, const Logcie_Log *log, const char *bytes, size_t len);

/**
 * @brief Writer flush function type signature
 *
 * Flush is responsible for sending rest of the buffered data to its destination.
 * It can be either calling fflush or write into a socket.
 *
 * @param user_data  Destination for this writer (FILE *, socket, ...)
 */
typedef void(Logcie_WriterFlushFn)(void *user_data);

/**
 * @brief Writer struct
 *
 * Stores writer function pointer and custom data for it
 *
 * @param write  Writer function pointer
 * @param flush  Flush function pointer
 * @param data   Custom data for writer function
 */
typedef struct Logcie_Writer {
  Logcie_WriterFn      *write;
  Logcie_WriterFlushFn *flush;
  void                 *data;
} Logcie_Writer;

/**
 * @brief Formatter function type signature.
 *
 * A formatter turns a Logcie_Log into bytes and hands them to the writer. It
 * owns the serialization: logcie_token_formatter renders $ tokens into
 * readable text, but a JSON or a binary formatter is the same interface with
 * different output.
 *
 * @param writer     Writer to hand the finished line to
 * @param user_data  Whatever this formatter needs. logcie_token_formatter
 *                   reads it as a token string; another formatter may want a
 *                   field mask, or nothing at all
 * @param log        Log to format
 * @param args       Arguments passed to the logging macro, for log->msg.
 *                   Use logcie_render_message() rather than handling them by hand
 * @return Number of bytes handed to the writer
 *
 * @note Call writer->write once, with the whole line. A writer is allowed to
 *       treat one call as one record, so splitting a line across calls would
 *       break syslog and network sinks.
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
 * @field nanos     Nanoseconds. 0 when not supported
 * @field module    Optional module name for categorizing logs
 * @field location  Source file and line number where log was called
 */
struct Logcie_Log {
  Logcie_LogLevel    level;
  const char        *msg;
  time_t             time;
  uint32_t           nanos;
  const char        *module;
  Logcie_LogLocation location;
};

#define LOGCIE_INTERNAL_CREATE_LOG(lvl, txt, f, l)          logcie_make_log(LOGCIE_MODULE, lvl, txt, f, l)
#define LOGCIE_INTERNAL_CREATE_LOG_MOD(mod, lvl, txt, f, l) logcie_make_log(mod, lvl, txt, f, l)

// NOTE: the __has_attribute test has to be nested. The preprocessor does not
// short-circuit the way C does, so a compiler without __has_attribute -- MSVC
// -- still has to make sense of the tokens on the right of the &&, and warns
// about them.
#ifdef __has_attribute
#if __has_attribute(__format__)
#define LOGCIE_INTERNAL_PRINTF_TYPE_CHECK(a, b) __attribute__((__format__(__printf__, a, b)))
#endif
#else
#define LOGCIE_INTERNAL_PRINTF_TYPE_CHECK(a, b)
#endif

/**
 * @brief Compiler‑specific variadic handling for the core macro
 * These use __FILE__ and __LINE__ to capture call site.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
#define LOGCIE_LOG_IMPL(level, msg, ...)        logcie_log(LOGCIE_INTERNAL_CREATE_LOG(LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_LOG_MOD(module, level, msg, ...) logcie_log(LOGCIE_INTERNAL_CREATE_LOG_MOD(module, LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg __VA_OPT__(, ) __VA_ARGS__)
#elif !defined(LOGCIE_PEDANTIC) && (defined(__GNUC__) || defined(__clang__))
#define LOGCIE_LOG_IMPL(level, msg, ...)        logcie_log(LOGCIE_INTERNAL_CREATE_LOG(LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#define LOGCIE_LOG_MOD(module, level, msg, ...) logcie_log(LOGCIE_INTERNAL_CREATE_LOG_MOD(module, LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg, ##__VA_ARGS__)
#elif !defined(LOGCIE_PEDANTIC) && defined(_MSC_VER) && defined(_MSVC_TRADITIONAL) && !_MSVC_TRADITIONAL
// Conforming MSVC preprocessor (/Zc:preprocessor). __VA_OPT__ is available
// there regardless of the language standard in effect.
#define LOGCIE_LOG_IMPL(level, msg, ...)        logcie_log(LOGCIE_INTERNAL_CREATE_LOG(LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg __VA_OPT__(, ) __VA_ARGS__)
#define LOGCIE_LOG_MOD(module, level, msg, ...) logcie_log(LOGCIE_INTERNAL_CREATE_LOG_MOD(module, LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg __VA_OPT__(, ) __VA_ARGS__)
#elif !defined(LOGCIE_PEDANTIC) && defined(_MSC_VER)
// Traditional MSVC preprocessor: it drops the comma before an empty
// __VA_ARGS__, but it also hands __VA_ARGS__ to a nested macro as one glued
// argument. LOGCIE_INTERNAL_EXPAND below forces the extra rescan that splits
// it back apart.
#define LOGCIE_LOG_IMPL(level, msg, ...)        logcie_log(LOGCIE_INTERNAL_CREATE_LOG(LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_LOG_MOD(module, level, msg, ...) logcie_log(LOGCIE_INTERNAL_CREATE_LOG_MOD(module, LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#else
#define LOGCIE_LOG_IMPL(level, msg)        logcie_log(LOGCIE_INTERNAL_CREATE_LOG(LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg)
#define LOGCIE_LOG_MOD(module, level, msg) logcie_log(LOGCIE_INTERNAL_CREATE_LOG_MOD(module, LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg)
#define LOGCIE_VA_LOGS
#endif

// Identity macro. On every conforming preprocessor this is a no-op; on the
// traditional MSVC preprocessor the extra rescan is what re-splits __VA_ARGS__
// into separate macro arguments.
#define LOGCIE_INTERNAL_EXPAND(x) x

#define LOGCIE_TRACE(...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(TRACE, __VA_ARGS__))
#define LOGCIE_DEBUG(...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(DEBUG, __VA_ARGS__))
#define LOGCIE_VERBOSE(...) LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(VERBOSE, __VA_ARGS__))
#define LOGCIE_INFO(...)    LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(INFO, __VA_ARGS__))
#define LOGCIE_WARN(...)    LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(WARN, __VA_ARGS__))
#define LOGCIE_ERROR(...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(ERROR, __VA_ARGS__))
#define LOGCIE_FATAL(...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(FATAL, __VA_ARGS__))

#define LOGCIE_TRACE_MOD(mod, ...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, TRACE, __VA_ARGS__))
#define LOGCIE_DEBUG_MOD(mod, ...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, DEBUG, __VA_ARGS__))
#define LOGCIE_VERBOSE_MOD(mod, ...) LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, VERBOSE, __VA_ARGS__))
#define LOGCIE_INFO_MOD(mod, ...)    LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, INFO, __VA_ARGS__))
#define LOGCIE_WARN_MOD(mod, ...)    LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, WARN, __VA_ARGS__))
#define LOGCIE_ERROR_MOD(mod, ...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, ERROR, __VA_ARGS__))
#define LOGCIE_FATAL_MOD(mod, ...)   LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_MOD(mod, FATAL, __VA_ARGS__))

#define LOGCIE_LOG(level, ...) LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL(level, __VA_ARGS__))

// Separate variadic logs for compilers that do not support optional variadics in macros
#ifdef LOGCIE_VA_LOGS
#define LOGCIE_LOG_IMPL_VA(level, msg, ...)        logcie_log(LOGCIE_INTERNAL_CREATE_LOG(LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg, __VA_ARGS__)
#define LOGCIE_LOG_MOD_VA(module, level, msg, ...) logcie_log(LOGCIE_INTERNAL_CREATE_LOG_MOD(module, LOGCIE_LEVEL_##level, msg, __FILE__, __LINE__), msg, __VA_ARGS__)

#define LOGCIE_TRACE_VA(...)      LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(TRACE, __VA_ARGS__))
#define LOGCIE_DEBUG_VA(...)      LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(DEBUG, __VA_ARGS__))
#define LOGCIE_VERBOSE_VA(...)    LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(VERBOSE, __VA_ARGS__))
#define LOGCIE_INFO_VA(...)       LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(INFO, __VA_ARGS__))
#define LOGCIE_WARN_VA(...)       LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(WARN, __VA_ARGS__))
#define LOGCIE_ERROR_VA(...)      LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(ERROR, __VA_ARGS__))
#define LOGCIE_FATAL_VA(...)      LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(FATAL, __VA_ARGS__))
#define LOGCIE_LOG_VA(level, ...) LOGCIE_INTERNAL_EXPAND(LOGCIE_LOG_IMPL_VA(level, __VA_ARGS__))
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
LOGCIE_DEF size_t logcie_log(Logcie_Log log, const char *fmt, ...) LOGCIE_INTERNAL_PRINTF_TYPE_CHECK(2, 3);

/**
 * @brief Constructor for Logcie_Log strcture
 *
 * @param module    Optional module name for categorizing logs
 * @param level     Severity level of the log message
 * @param msg       Format string for the log message
 * @param file      Source file name where log was called
 * @param line      Line number in source file where log was called
 * @note this function is invoked internally by macro chain statring from macros like LOGCIE_INFO
 */
LOGCIE_DEF Logcie_Log logcie_make_log(const char *module, Logcie_LogLevel level, const char *msg, const char *file, uint32_t line);

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
 * @brief Returns pointer to default stdout sink
 *
 * Allows you to custompize default sink rather than added brand new one.
 *
 * @return Const pointer to the default Logcie_Sink
 */
LOGCIE_DEF Logcie_Sink *logcie_get_default_sink(void);

/**
 * @brief Adds a new sink to the logger.
 *
 * The sink will receive all log messages that pass its filter criteria.
 * Multiple sinks can be active simultaneously with different configurations.
 *
 * @param sink Pointer to a Logcie_Sink structure to add
 * @return 1 if sink was added, 0 if sink is NULL or LOGCIE_MAX_SINKS sinks
 *         are already registered
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
 */
LOGCIE_DEF uint8_t logcie_remove_sink_by_index(size_t index);

/**
 * @brief Removes all sinks.
 *
 * Resets the logger to its initial state with only the default stdout sink.
 * Useful for cleanup or reconfiguration scenarios.
 *
 * @note Sink memory is not freed. Caller is responsible for cleanup.
 * @note File streams are not closed. Caller must close them separately.
 */
LOGCIE_DEF void logcie_remove_all_sinks(void);

/**
 * @brief Flushes all registerd sinks.
 *
 * Iterates for every registered sink and calls its flusher, if it exists.
 *
 * @returns total size of all flushed bytes
 */
LOGCIE_DEF void logcie_flush(void);

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
 * `$N` - Nanoseconds
 * `$z` - Timezone offset
 * `$<n - Pads the previous token out to n columns
 * `$$` - Literal dollar sign
 *
 * @param writer     Pointer to writer (see Logcie_Writer)
 * @param user_data  Pointer to format string
 * @param log        Lot to format
 * @param va         Variadic arguments that was passed to logging function (LOGCIE_INFO("message %s", "this would be in va")
 * @return Number of characters written to the sink
 */
LOGCIE_DEF size_t logcie_token_formatter(Logcie_Writer *writer, void *user_data, Logcie_Log log, va_list *args);

/**
 * @brief Built-in writer that appends a line to a FILE *
 *
 * @param user_data  FILE * to write to, or NULL to discard
 * @param log        Metadata of the line. Unused here, but part of the contract
 * @param bytes      Formatted line, including its terminating newline.
 *                   Not NUL terminated -- use len
 * @param len        Number of bytes
 * @return Number of bytes written, or 0 when user_data is NULL
 *
 * @note A NULL user_data discards the line. That is the cheapest way to mute a
 *       sink without removing it, and /dev/null without opening it. It is also
 *       the first thing to check when a sink is unexpectedly silent: logcie
 *       does not substitute stdout for you.
 */
LOGCIE_DEF size_t logcie_file_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len);

/**
 * @brief Built-in flusher for build-in file wirter
 * @see logcie_file_writer
 *
 * @param user_data  FILE * to write to, or NULL to discard
 */
LOGCIE_DEF void logcie_file_flush(void *user_data);

/**
 * @brief Renders the user's message into a buffer.
 *
 * Useful when writing a formatter: every formatter has to turn log->msg plus
 * its arguments into text, and this handles the va_list copying that a second
 * render pass requires.
 *
 * @param buf   Destination buffer, or NULL when cap is 0
 * @param cap   Capacity of buf
 * @param log   Log whose msg is rendered
 * @param args  Arguments passed to the logging macro
 * @return Length the message would have, not counting the terminator, which
 *         may exceed cap
 *
 * @note Like snprintf, this reports the length it wanted even when it
 *       truncates, and (NULL, 0) sizes without writing. Prefer rendering into
 *       a buffer you already have and retrying only if the return value did
 *       not fit -- that costs one pass for the common line, where sizing first
 *       always costs two.
 */
LOGCIE_DEF size_t logcie_render_message(char *buf, size_t cap, const Logcie_Log *log, va_list *args);

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
 * @brief Filters out logs if log module prefix is equal to specified string
 * @param data cosnt char*
 */
LOGCIE_DEF uint8_t logcie_filter_module_prefix_eq_fn(const void *data, Logcie_Log *log);

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
#define logcie_filter_not(f)                                     \
  Logcie_Filter {                                                \
    .filter = [](const void *data, Logcie_Log *log) -> uint8_t { \
      (void)data;                                                \
      return (uint8_t)!(f).filter((f).data, log);                \
    },                                                           \
    .data = NULL,                                                \
  }
#else  // __cplusplus
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

#define logcie_filter_not(f)        \
  ((Logcie_Filter){                 \
    .filter = logcie_filter_not_fn, \
    .data   = &(f)                  \
  })
#endif  // __cplusplus

#define logcie_filter_level_min(level)    \
  ((Logcie_Filter){                       \
    .filter = logcie_filter_level_min_fn, \
    .data   = (void *)(level)             \
  })

#define logcie_filter_level_max(level)    \
  ((Logcie_Filter){                       \
    .filter = logcie_filter_level_max_fn, \
    .data   = (void *)(level)             \
  })

#define logcie_filter_module_eq(module)   \
  ((Logcie_Filter){                       \
    .filter = logcie_filter_module_eq_fn, \
    .data   = (module)                    \
  })

#define logcie_filter_module_prefix_eq(prefix)   \
  ((Logcie_Filter){                              \
    .filter = logcie_filter_module_prefix_eq_fn, \
    .data   = (prefix)                           \
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

#ifdef LOGCIE_MALLOC
#include <stdlib.h>
#endif

#ifndef LOGCIE_INTERNAL_ASSERT
#define LOGCIE_INTERNAL_ASSERT(bool, msg) assert(bool &&msg)
#endif

#ifdef LOGCIE_DEBUG_CHECKS
#if __STDC_VERSION__ >= 201112L  // Check for C11 support
#define LOGCIE_INTERNAL_DEBUG_ASSERT(bool, msg) static_assert(bool, msg)
#else
#define LOGCIE_INTERNAL_DEBUG_ASSERT(bool, msg) LOGCIE_INTERNAL_ASSERT(bool, msg)
#endif
#else
#define LOGCIE_INTERNAL_DEBUG_ASSERT(bool, msg)
#endif

#ifndef LOGCIE_THREAD_SAFE
#define LOGCIE_MUTEX_DECLARE(name) struct logcie_unused_##name  // NOTE: To fix dandling `;`
#define LOGCIE_MUTEX_INIT(m)
#define LOGCIE_MUTEX_DESTROY(m)
#define LOGCIE_MUTEX_LOCK(m)
#define LOGCIE_MUTEX_UNLOCK(m)
#else
#if defined(_WIN32)
#include <windows.h>
#define LOGCIE_MUTEX_DECLARE(name) SRWLOCK name = SRWLOCK_INIT
#define LOGCIE_MUTEX_INIT(m)
#define LOGCIE_MUTEX_DESTROY(m)
#define LOGCIE_MUTEX_LOCK(m)   AcquireSRWLockExclusive(&(m))
#define LOGCIE_MUTEX_UNLOCK(m) ReleaseSRWLockExclusive(&(m))
#else
#include <pthread.h>
#define LOGCIE_MUTEX_DECLARE(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define LOGCIE_MUTEX_INIT(m)       pthread_mutex_init(&(m), NULL)
#define LOGCIE_MUTEX_DESTROY(m)    pthread_mutex_destroy(&(m))
#define LOGCIE_MUTEX_LOCK(m)       pthread_mutex_lock(&(m))
#define LOGCIE_MUTEX_UNLOCK(m)     pthread_mutex_unlock(&(m))
#endif
#endif

LOGCIE_MUTEX_DECLARE(logcie_mutex);

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LOGCIE_THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define LOGCIE_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
#define LOGCIE_THREAD_LOCAL __thread
#else
#define LOGCIE_THREAD_LOCAL
#warning "No thread local storage support"
#endif

#ifndef LOGCIE_ALLOW_RECURSIVE_LOGGING
static LOGCIE_THREAD_LOCAL int logcie_log_depth = 0;
#endif

// NOTE: localtime and gmtime hand back a pointer into storage shared across the
// process, so two threads formatting at once can read a struct the other is
// rewriting. That buffer lives in libc, so no lock held here can protect it --
// the reentrant form is the only fix. Use it wherever one is exposed, and copy
// out of the shared buffer only where strict ISO C leaves nothing else.
//
// NOTE: these zero the result first, so a failing conversion yields a
// deterministic wrong date rather than whatever was on the stack.
static void logcie_localtime(const time_t *t, struct tm *out) {
  memset(out, 0, sizeof(*out));

#if defined(_MSC_VER)
  localtime_s(out, t);
#elif defined(_POSIX_C_SOURCE) || defined(_DEFAULT_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
  localtime_r(t, out);
#else
  struct tm *shared = localtime(t);

  if (shared != NULL) {
    *out = *shared;
  }
#endif
}

static void logcie_gmtime(const time_t *t, struct tm *out) {
  memset(out, 0, sizeof(*out));

#if defined(_MSC_VER)
  gmtime_s(out, t);
#elif defined(_POSIX_C_SOURCE) || defined(_DEFAULT_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
  gmtime_r(t, out);
#else
  struct tm *shared = gmtime(t);

  if (shared != NULL) {
    *out = *shared;
  }
#endif
}

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
  LOGCIE_INTERNAL_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  LOGCIE_INTERNAL_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
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
  LOGCIE_INTERNAL_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  LOGCIE_INTERNAL_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
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
  LOGCIE_MUTEX_LOCK(logcie_mutex);

  if (colors) {
    // If compiled with -fsanitize=address and colors array is wrong it will crash
    // If it is compiled without -fsanitize=address then color would be NULL (I hope)
    const char *color = colors[Count_LOGCIE_LEVEL - 1];
    LOGCIE_INTERNAL_ASSERT(color != NULL, "Size of array of colors in logcie_set_colors is not equal to Count_LOGCIE_LEVEL");
    logcie_level_color = colors;
  } else {
    logcie_level_color = logcie_default_level_color;
  }

  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
}

static inline const char *get_logcie_level_color(Logcie_LogLevel level) {
  LOGCIE_INTERNAL_DEBUG_ASSERT(Count_LOGCIE_LEVEL == 7, "Forgot to update get_logcie_level_label, you dummy dumb fuck");
  LOGCIE_INTERNAL_ASSERT(level < Count_LOGCIE_LEVEL, "Unexpected log level");
  return logcie_level_color[level];
}

// NOTE: positional, not designated. C++ has no designated initializers before
// C++20, so a designated one here makes the header unusable as C++ under
// -pedantic on every earlier standard.
static Logcie_Sink default_stdout_sink = {
  {logcie_token_formatter, (void *)(LOGCIE_DEFAULT_SINK_FORMAT)},
  {logcie_file_writer, logcie_file_flush, NULL},
  {NULL, NULL},
};

#ifdef __has_attribute
#if __has_attribute(constructor)
#define LOGCIE_INTERNAL_HAS_CONSTRUCTOR
#endif
#endif

#ifdef LOGCIE_INTERNAL_HAS_CONSTRUCTOR
__attribute__((constructor)) void init_default_stdout_sink(void) {
  default_stdout_sink.writer.data = stdout;
}
#endif

typedef struct Logcie_Logger {
  Logcie_Sink *sinks[LOGCIE_MAX_SINKS];
  size_t       sinks_len;
} Logcie_Logger;

// NOTE: positional, not designated. C++ has no designated initializers before
// C++20, so a designated one here makes the header unusable as C++ under
// -pedantic on every earlier standard.
static Logcie_Logger logcie = {
  {&default_stdout_sink},
  1,
};

size_t logcie_get_sink_count(void) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);
  size_t count = logcie.sinks_len;
  LOGCIE_MUTEX_UNLOCK(logcie_mutex);

  return count;
}

Logcie_Sink *logcie_get_sink(size_t index) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);

  if (index >= logcie.sinks_len) {
    LOGCIE_MUTEX_UNLOCK(logcie_mutex);
    return NULL;
  }

  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
  return logcie.sinks[index];
}

LOGCIE_DEF Logcie_Sink *logcie_get_default_sink(void) {
  return &default_stdout_sink;
}

uint8_t logcie_add_sink(Logcie_Sink *sink) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);

  if (sink == NULL) {
    LOGCIE_MUTEX_UNLOCK(logcie_mutex);
    return 0;
  }

#ifndef LOGCIE_INTERNAL_HAS_CONSTRUCTOR
  if (sink == &default_stdout_sink && sink->writer.data == NULL) {
    sink->writer.data = stdout;
  }
#endif

  if (logcie.sinks_len >= LOGCIE_MAX_SINKS) {
    LOGCIE_MUTEX_UNLOCK(logcie_mutex);
    return 0;
  }

  logcie.sinks[logcie.sinks_len] = sink;
  logcie.sinks_len++;

  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
  return 1;
}

static uint8_t logcie_remove_sink_by_index_locked(size_t index) {
  if (index >= logcie.sinks_len) {
    return 0;
  }

  for (size_t i = index; i < logcie.sinks_len - 1; i++) {
    logcie.sinks[i] = logcie.sinks[i + 1];
  }

  logcie.sinks_len--;
  return 1;
}

uint8_t logcie_remove_sink(Logcie_Sink *sink) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    if (logcie.sinks[i] == sink) {
      uint8_t res = logcie_remove_sink_by_index_locked(i);
      LOGCIE_MUTEX_UNLOCK(logcie_mutex);
      return res;
    }
  }

  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
  return 0;
}

uint8_t logcie_remove_sink_by_index(size_t index) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);
  uint8_t res = logcie_remove_sink_by_index_locked(index);
  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
  return res;
}

void logcie_remove_all_sinks(void) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);
  logcie.sinks_len = 0;
  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
}

LOGCIE_DEF void logcie_flush(void) {
  LOGCIE_MUTEX_LOCK(logcie_mutex);

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    Logcie_WriterFlushFn *flusher = logcie.sinks[i]->writer.flush;

    if (flusher) {
      flusher(logcie.sinks[i]->writer.data);
    }
  }

  LOGCIE_MUTEX_UNLOCK(logcie_mutex);
}

size_t logcie_log(Logcie_Log log, const char *fmt, ...) {
#ifndef LOGCIE_ALLOW_RECURSIVE_LOGGING
  if (logcie_log_depth > 0) {
    return 0;
  }

  logcie_log_depth++;
#endif

  LOGCIE_MUTEX_LOCK(logcie_mutex);

  va_list args;
  va_start(args, fmt);

  log.msg = fmt;

  for (size_t i = 0; i < logcie.sinks_len; i++) {
    Logcie_Sink *sink = logcie.sinks[i];
    LOGCIE_INTERNAL_ASSERT(sink && sink->formatter.format, "Sink have no formatter");

    if (sink->filter.filter && !sink->filter.filter(sink->filter.data, &log)) {
      continue;
    }

    va_list args_copy;
    va_copy(args_copy, args);

    sink->formatter.format(&sink->writer, sink->formatter.data, log, &args_copy);

    if (log.level >= LOGCIE_AUTOFLUSH_LEVEL && sink->writer.flush) {
      sink->writer.flush(sink->writer.data);
    }

    va_end(args_copy);
  }

  LOGCIE_MUTEX_UNLOCK(logcie_mutex);

#ifndef LOGCIE_ALLOW_RECURSIVE_LOGGING
  logcie_log_depth--;
#endif

  va_end(args);
  return 0;
}

LOGCIE_DEF Logcie_Log logcie_make_log(const char *module, Logcie_LogLevel level, const char *msg, const char *file, uint32_t line) {
  Logcie_Log log;

  log.module        = module;
  log.level         = level;
  log.msg           = msg;
  log.location.file = file;
  log.location.line = line;
  log.time          = 0;
  log.nanos         = 0;

#if defined(TIME_UTC)
  struct timespec ts;

  if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
    log.time  = ts.tv_sec;
    log.nanos = (uint32_t)ts.tv_nsec;
  }
#elif defined(CLOCK_REALTIME)
  struct timespec ts;

  if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
    log.time  = ts.tv_sec;
    log.nanos = (uint32_t)ts.tv_nsec;
  }
#else
  log.time  = time(NULL);
  log.nanos = 0;
#endif

  return log;
}

// NOTE: appends one token and keeps `needed` as the length the whole line
// would have, fitted or not. snprintf reports that length even when it
// truncates, which is what lets one pass both render and measure -- there is
// no separate sizing walk over the format string to keep in sync.
//
// NOTE: the offset is clamped to cap before the pointer arithmetic. Forming
// buf + needed past the end of the buffer is undefined even without a
// dereference, and once needed reaches cap a size of 0 makes snprintf write
// nothing while still returning the length it wanted.
#define LOGCIE_INTERNAL_EMIT(...)                                 \
  do {                                                            \
    size_t  off_ = needed < cap ? needed : cap;                   \
    int32_t n_   = snprintf(buf + off_, cap - off_, __VA_ARGS__); \
    last_len     = n_ > 0 ? (size_t)n_ : 0;                       \
    needed += last_len;                                           \
  } while (0)

/**
 * @brief Renders the user's message into a buffer.
 *
 * Every formatter needs this and none should reimplement it: the va_list has
 * to be copied because the caller may render more than once.
 *
 * @return Length the message would have, which may exceed cap
 */
LOGCIE_DEF size_t logcie_render_message(char *buf, size_t cap, const Logcie_Log *log, va_list *args) {
  // NOTE: (NULL, 0) is a legal sizing call, exactly as it is for snprintf, so
  // buf is deliberately not checked. It is how a formatter asks how much room
  // the message needs without rendering it anywhere.
  LOGCIE_INTERNAL_ASSERT((cap == 0 || buf), "Render buffer is missing");
  LOGCIE_INTERNAL_ASSERT(log, "Log is missing");

  va_list copy;
  va_copy(copy, *args);

  int32_t written = vsnprintf(buf, cap, log->msg, copy);

  va_end(copy);
  return written > 0 ? (size_t)written : 0;
}

// NOTE: renders the whole line and returns the length it would have had. A
// return value >= cap means it was truncated, and the caller decides whether
// to retry on a bigger buffer or accept the truncation.
static size_t logcie_render_tokens(char *buf, size_t cap, const char *fmt, const Logcie_Log *log, va_list *args) {
  LOGCIE_INTERNAL_ASSERT(buf, "Render buffer is missing");
  LOGCIE_INTERNAL_ASSERT(cap > 0, "Render buffer has no capacity");

  size_t needed   = 0;
  size_t last_len = 0;

  // NOTE: the timestamp is captured at the call site, in
  // LOGCIE_INTERNAL_CREATE_LOG. This is only the conversion of that instant
  // into fields, and a format with no $d, $t or $z should not pay for it.

#ifdef __cplusplus
  struct tm local_tm = {};
  struct tm utc_tm   = {};
#else
  struct tm local_tm = {0};
  struct tm utc_tm   = {0};
#endif

  int32_t local_hours = 0;
  int32_t timediff    = 0;
  uint8_t time_ready  = 0;

  // NOTE: the time functions are safe here because the formatter only runs
  // from logcie_log, which holds the lock when LOGCIE_THREAD_SAFE is on.
#define LOGCIE_INTERNAL_ENSURE_TIME()                                             \
  do {                                                                            \
    if (!time_ready) {                                                            \
      logcie_localtime(&log->time, &local_tm);                                    \
      logcie_gmtime(&log->time, &utc_tm);                                         \
      local_hours = local_tm.tm_hour;                                             \
      timediff    = (int32_t)difftime(mktime(&local_tm), mktime(&utc_tm)) / 3600; \
      time_ready  = 1;                                                            \
    }                                                                             \
  } while (0)

  while (*fmt != '\0') {
    if (*fmt != '$') {
      LOGCIE_INTERNAL_EMIT("%c", *fmt);
      fmt++;
      continue;
    }

    fmt++;

    if (*fmt == '\0') {
      break;
    }

    switch (*fmt) {
      case '$': LOGCIE_INTERNAL_EMIT("$"); break;
      case 'l': LOGCIE_INTERNAL_EMIT("%s", get_logcie_level_label(log->level)); break;
      case 'L': LOGCIE_INTERNAL_EMIT("%s", get_logcie_level_label_upper(log->level)); break;
      case 'c': LOGCIE_INTERNAL_EMIT("%s", get_logcie_level_color(log->level)); break;
      case 'r': LOGCIE_INTERNAL_EMIT("%s", LOGCIE_COLOR_RESET); break;
      case 'N': LOGCIE_INTERNAL_EMIT("%09u", log->nanos); break;
      case 'f': LOGCIE_INTERNAL_EMIT("%s", log->location.file ? log->location.file : ""); break;
      case 'x': LOGCIE_INTERNAL_EMIT("%u", log->location.line); break;
      case 'M': LOGCIE_INTERNAL_EMIT("%s", log->module ? log->module : ""); break;

      case 'm': {
        size_t off = needed < cap ? needed : cap;

        last_len = logcie_render_message(buf + off, cap - off, log, args);
        needed += last_len;
        break;
      }

      case 'd':
        LOGCIE_INTERNAL_ENSURE_TIME();
        LOGCIE_INTERNAL_EMIT("%d-%02d-%02d", local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday);
        break;

      case 't':
        LOGCIE_INTERNAL_ENSURE_TIME();
        LOGCIE_INTERNAL_EMIT("%02d:%02d:%02d", local_hours, local_tm.tm_min, local_tm.tm_sec);
        break;

      case 'z':
        LOGCIE_INTERNAL_ENSURE_TIME();
        LOGCIE_INTERNAL_EMIT("%+d", timediff);
        break;

      case '<': {
        fmt++;
        uint16_t target = 0;

        while (*fmt >= '0' && *fmt <= '9') {
          target = (uint16_t)(target * 10 + (uint16_t)(*fmt - '0'));
          fmt++;
        }

        // NOTE: signed on both sides. target is uint16_t and last_len is
        // size_t, so an unsigned subtraction wraps when the previous token is
        // already wider than the target.
        int32_t pad = (int32_t)target - (int32_t)last_len;

        if (pad > 0) {
          LOGCIE_INTERNAL_EMIT("%*s", pad, "");
        } else {
          // NOTE: nothing was written, so the previous token's length must not
          // survive into the next $<n.
          last_len = 0;
        }

        fmt--;
        break;
      }

      default:
        fprintf(stderr, "%sWARN: unknown format sequence '$%c'. Skipping...\n" LOGCIE_COLOR_RESET, get_logcie_level_color(LOGCIE_LEVEL_WARN), *fmt);
        break;
    }

    fmt++;
  }

  LOGCIE_INTERNAL_EMIT("\n");

#undef LOGCIE_INTERNAL_ENSURE_TIME

  return needed;
}

#undef LOGCIE_INTERNAL_EMIT

size_t logcie_token_formatter(Logcie_Writer *writer, void *data, Logcie_Log log, va_list *args) {
  const char *fmt = (const char *)data;

  LOGCIE_INTERNAL_ASSERT(writer, "Sink have no writer");
  LOGCIE_INTERNAL_ASSERT(fmt, "Sink have no format string");

  // NOTE: a line that fits costs one stack buffer and no allocator call, and
  // the writer is handed the finished line in a single call.
  char    stack_buf[LOGCIE_MAX_LINE];
  va_list attempt;

  va_copy(attempt, *args);
  size_t needed = logcie_render_tokens(stack_buf, sizeof(stack_buf), fmt, &log, &attempt);
  va_end(attempt);

  if (needed < sizeof(stack_buf)) {
    writer->write(writer->data, &log, stack_buf, needed);
    return needed;
  }

#ifdef LOGCIE_MALLOC
  {
    char *heap = (char *)LOGCIE_MALLOC(needed + 1);

    if (heap != NULL) {
      va_copy(attempt, *args);
      size_t written = logcie_render_tokens(heap, needed + 1, fmt, &log, &attempt);
      va_end(attempt);

      writer->write(writer->data, &log, heap, written);
      LOGCIE_FREE(heap);
      return written;
    }
  }
#endif

  // NOTE: reached when the line does not fit and either there is no allocator
  // or it failed. Losing the tail of one line beats losing the process.
  writer->write(writer->data, &log, stack_buf, sizeof(stack_buf) - 1);
  return sizeof(stack_buf) - 1;
}

LOGCIE_DEF size_t logcie_file_writer(void *user_data, const Logcie_Log *log, const char *bytes, size_t len) {
  (void)log;

  // NOTE: a NULL target discards, which is /dev/null without the open().
  FILE *file = (FILE *)user_data;

  if (file == NULL) {
    return 0;
  }

  return fwrite(bytes, 1, len, file);
}

LOGCIE_DEF void logcie_file_flush(void *user_data) {
  if (user_data) {
    fflush((FILE *)user_data);
  }
}

LOGCIE_DEF uint8_t logcie_filter_not_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_not'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_not'");
  Logcie_Filter *filter = (Logcie_Filter *)data;
  return !filter->filter(filter->data, log);
}

LOGCIE_DEF uint8_t logcie_filter_and_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_and'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_and'");
  Logcie_FilterCombinationData *d = (Logcie_FilterCombinationData *)data;
  return d->a.filter(d->a.data, log) && d->b.filter(d->b.data, log);
}

LOGCIE_DEF uint8_t logcie_filter_or_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_or'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_or'");
  Logcie_FilterCombinationData *d = (Logcie_FilterCombinationData *)data;
  return d->a.filter(d->a.data, log) || d->b.filter(d->b.data, log);
}

LOGCIE_DEF uint8_t logcie_filter_level_min_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT((uintptr_t)data < Count_LOGCIE_LEVEL, "Param 'data' is not correct for filter 'logcie_filter_level_min'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_level_min'");
  Logcie_LogLevel level = (Logcie_LogLevel)(uintptr_t)data;
  return log->level >= level;
}

LOGCIE_DEF uint8_t logcie_filter_level_max_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT((uintptr_t)data < Count_LOGCIE_LEVEL, "Param 'data' is not correct for filter 'logcie_filter_level_max'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_level_max'");
  Logcie_LogLevel level = (Logcie_LogLevel)(uintptr_t)data;
  return log->level <= level;
}

LOGCIE_DEF uint8_t logcie_filter_module_eq_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_module_eq'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_module_eq'");
  const char *module = (const char *)data;
  return log->module && strcmp(module, log->module) == 0;
}

LOGCIE_DEF uint8_t logcie_filter_module_prefix_eq_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_module_prefix_eq'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_module_prefix_eq'");

  const char *prefix = (const char *)data;

  if (!log->module) {
    return 0;
  }

  size_t len = strlen(prefix);

  // NOTE: the empty prefix is the root of the hierarchy, so it matches every
  // module. Without this it would match nothing, since no name begins with a
  // separator.
  if (len == 0) {
    return 1;
  }

  if (strncmp(log->module, prefix, len) != 0) {
    return 0;
  }

  // NOTE: the match has to end on a separator or on the end of the name,
  // otherwise "net" would also swallow "network". That boundary is the whole
  // difference between a hierarchy and a substring search.
  return log->module[len] == '\0' || log->module[len] == LOGCIE_MODULE_SEPARATOR;
}

LOGCIE_DEF uint8_t logcie_filter_message_contains_fn(const void *data, Logcie_Log *log) {
  LOGCIE_INTERNAL_ASSERT(data, "Param 'data' is not present for filter 'logcie_filter_message_contains'");
  LOGCIE_INTERNAL_ASSERT(log, "Param 'log' is not present for filter 'logcie_filter_message_contains'");
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
