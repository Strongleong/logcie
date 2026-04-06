# Logcie

**Logcie** is a lightweight, single-header logging library for C with a modular design
that supports multiple output sinks, customizable formatting, and flexible filtering.

## Features

- Multiple log levels
- ANSI color support
- Fully customizable output format
- Filters support
- Support for multiple sinks (stdout, file, etc.)
- c11/c99 compatible (with -pedantic file)


## Table of Contents

- [Quick Start](#quick-start)
- [Installation](#installation)
- [Basic Usage](#basic-usage)
- [Log Levels](#log-levels)
- [Architecture Overview](#architecture-overview)
  - [Formatter](#formatter)
  - [Writer](#writer)
  - [Filter](#filter)
- [Sinks and Output Configuration](#sinks-and-output-configuration)
  - [Default sink](#default_sink)
  - [Creating a Custom Sink](#creating-a-custom-sink)
- [Module-Based Logging](#module-based-logging)
  - [C++ Compatibility](#c++-compatibility)
- [Memory Management Notes](#memory-management-notes)
- [Format Tokens](#format-tokens)
  - [Format Examples](#format-examples)
- [Filters](#filters)
  - [Custom Filter Function](#custom-filter-function)
- [Limitations](#limitations)
- [Usage in libraries](#usage-in-libraries)
- [License](#license)

## Quick Start

```c
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

int main() {
    LOGCIE_INFO("Application started");
    LOGCIE_DEBUG("Processing value: %d", 42);
    LOGCIE_WARN("This is a warning message");
    LOGCIE_ERROR("An error occurred: %s", "file not found");
    return 0;
}
```

## Installation

 Copy `logcie.h` into your project and include it.

```c
// In one file (main.c, libs.c, etc)
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

// In any other file where you want to use logcie
#include "logcie.h"
```

## Basic Usage

Logcie provides macros for all log levels that automatically capture the file name and line number:

```c
LOGCIE_TRACE("Detailed tracing information");
LOGCIE_DEBUG("Debug value: %d", some_value);
LOGCIE_VERBOSE("Additional verbose details");
LOGCIE_INFO("Informational message");
LOGCIE_WARN("Warning: %s", warning_message);
LOGCIE_ERROR("Error code: %d", error_code);
LOGCIE_FATAL("Fatal error, shutting down");
```

All macros support printf-style formatting. The message string supports the same format specifiers as `printf()`.

## Log Levels

Logcie defines seven log levels in increasing order of severity:

| Level   | Description                 | Typical Use                              |
| ------- | -------------               | -------------                            |
| TRACE   | Most detailed information   | Function entry/exit, variable values     |
| DEBUG   | Debugging information       | State changes, intermediate results      |
| VERBOSE | Verbose operational details | Configuration loading, minor events      |
| INFO    | General information         | Startup messages, major events           |
| WARN    | Warning conditions          | Recoverable errors, deprecated usage     |
| ERROR   | Error conditions            | Operation failures, unexpected states    |
| FATAL   | Fatal conditions            | Unrecoverable errors, immediate shutdown |


## Architecture Overview

Logcie is built arout three core components:

### Formatter

Tranforms a log structure into formatted output and passes it to [Writer](#writer)

### Writer

Handles where fomratted output goes (FILE*, network, etc.).

### Fitler

Decides whether a log should be emmited.

A combination of these three components is called a **Sink**

## Sinks and Output Configuration

A **sink** defines where log messages are written and how they are formatted.
You can add additional sinks for files, network sockets, or custom destinations.

### Default sink

Logcie provides a default stdout sink automatically, so you can start logging immediately.

This is how default sinks looks like:

```c
static Logcie_Sink default_stdout_sink = {
    .formatter = {logcie_printf_formatter, "$c$L$r " LOGCIE_COLOR_GRAY "$f:$x$r: $m"},
    .writer    = {logcie_printf_writer, stdouit},
    .filter    = {NULL, NULL},
};
```

However, when you add your first Sink using `logcie_add_sink()`, the default printf Sink is removed.
This design choice ensures you have full control over sink configuration once you start customizing.

Important behaviors to understand:
 - Initial state: By default, one stdout sink exists at index 0
 - First sink addition: When you add your first custom sink, the default sink is removed
 - Restoring defaults: Use logcie_remove_all_sinks() to return to the initial default configuration

If you want to keep both the default stdout sink and add additional sinks, you must re-add it explicitly.

### Creating a Custom Sink

```c
// Create a file sink for error logs
Logcie_Sink error_sink = {
    .min_level = LOGCIE_LEVEL_DEBUG,
    // nice format: date, time, level, module, message
    .formatter = {logcie_printf_formatter,  "$d $t [$L] $f:$x - $m"},
    .writer    = {logcie_printf_writer, fopen("errors.log", "a")},
    .filter    = {logcie_filter_level_min, LOGCIE_LEVEL_ERROR}
};

// Add it to the logger
logcie_add_sink(&error_sink);
```

If you want to keep default sink you can do it like this:

```c
Logcie_Sink *default_sink = logcie_get_sink(0);
logcie_add_sink(&file_sink);
logcie_add_sink(&default_sink);
```

## Module-Based Logging

 Logcie has another important concept: modules. A module is simply a string used to label a *scope* where the log originated.

 Modules allow you to group logs by subsystem (e.g., "network", "core", "database")
 and can be used in format strings or filters to provied additional context or control log output.

 To define a module, declare a variable name `logcie_module` in your translation uint:

```c
static const char *logcie_module = "network";
```

 When defined, this value will be attached to every log emitted from that file. If not defined, a default module name is used.
 Modules can also be used in custom filters to selectively allow or block logs from specific parts of your application.

```c
// In each source file, define a module name
static const char *logcie_module = "network";

Logcie_Sink file_sink = {
    .sink      = stdout;
    .min_level = LOGCIE_LEVEL_DEBUG;
    .fmt       = "$d $t [$L] ($M) $m";  // nice format: date, time, level, module, message
    .formatter = logcie_printf_formatter;
};

logcie_add_sink(&file_sink);

// Then log as usual
const char *hostname = "gnu.org";
LOGCIE_INFO("Connection established to %s", hostname);

// Output: 2025-12-25 01:15:10 [INFO] (network) Connection established to gnu.org
```

The module name will appear in logs when using the `$M` format token.

### C++ Compatibility

Logcie supports C++ with minor adjustments:

```c
// In C++ files, define logcie_module differently:
extern "C" {
    const char *logcie_module = "module_name";
}

// Or if including in multiple files, use extern:
// In header:
extern "C" {
    extern const char *logcie_module;
}

// In one source file:
extern "C" {
    const char *logcie_module = "module_name";
}
```

## Memory Management Notes

Since logcie_add_sink() stores the pointer to your sink structure (not a copy), you must ensure:
  - Stack-allocated sinks: Must not go out of scope while registered
  - Heap-allocated sinks: Must be freed only after removal
  - Modification: You can modify sink properties after adding (changes take effect immediately)

## Format Tokens

Format strings use `$` tokens to insert log metadata. The default formatter supports the following tokens:

| Token   | Description                        | Example Output           |
| ------- | -------------                      | ----------------         |
| `$m`    | Log message with printf formatting | "Connection established" |
| `$f`    | Source file name                   | "main.c"                 |
| `$x`    | Line number                        | "42"                     |
| `$M`    | Module name                        | "network"                |
| `$l`    | Log level (lowercase)              | "info"                   |
| `$L`    | Log level (uppercase)              | "INFO"                   |
| `$c`    | ANSI color code for log level      | `\x1b[36;20m`            |
| `$r`    | ANSI reset color code              | `\x1b[0m`                |
| `$d`    | Date (YYYY-MM-DD)                  | "2025-12-24"             |
| `$t`    | Time (HH:MM:SS)                    | "14:30:15"               |
| `$z`    | Timezone offset                    | "+3"                     |
| `$<n`   | Pads with n spaces                 | "    "                   |
| `$$`    | Literal dollar sign                | "$"                      |

### Format Examples

```c
// Simple format with color
"$c$L$r: $m"

// Detailed format with timestamp and location
"$d $t [$L] $f:$x - $m"

// Module-based format
"[$M] $c$L$r $t - $m"
```

## Filters

Filters allow you to control which log messages are emitted to a sink.
You can create custom filter functions and logs will flow through them before going to formatter.

### Custom Filter Function

A filter function returns `1` (true) if the log should be emitted, or `0` (false) if it should be suppressed.

```c
uint8_t filter_by_module(Logcie_Sink *sink, Logcie_Log *log) {
    // Only emit logs from the "core" module
    return log->module && strcmp(log->module, "core") == 0;
}

uint8_t filter_by_level_range(Logcie_Sink *sink, Logcie_Log *log) {
    // Only emit INFO through ERROR logs
    return log->level >= LOGCIE_LEVEL_INFO && log->level <= LOGCIE_LEVEL_ERROR;
}
```

## Limitations

- **Not thread-safe** - Concurrent calls to logging functions from multiple threads may interleave output. Thread safety and multithreading is planned for version 1.0.0
- **Memory allocation** - The sink array uses `malloc()`/`realloc()` for dynamic growth
- **No built-in log rotation** - File management must be handled by the application (or just use `logrotate`)
- **Custom formatters require `va_list` handling** - Advanced usage requires understanding of variadic arguments

Future versions may address these limitations based on user feedback and requirements.

## Usage in libraries

You can add simple snippet to make your library support logcie

```c
// Logcie integration

#ifndef YOURLIB_LOG
  #ifdef LOGCIE
    #ifdef LOGCIE_VA_LOGS
      #define YOURLIB_LOG(level, ...) LOGCIE_##level##_VA(__VA_ARGS__)
    #else
      #define YOURLIB_LOG(level, ...) LOGCIE_##level(__VA_ARGS__)
    #endif
  #else
    #define YOURLIB_LOG(level, ...)                \
       do {                                       \
         fprintf(stderr, #level ": "__VA_ARGS__); \
         fprintf(stderr, "\n");                   \
       } while (0)
  #endif
#endif
```

Just change YOURLLIB to something more fitting :)

## License

Logcie is released under the MIT License. See [LICENSE file for more info](./LICENSE)

For questions or contributions, contact: Nikita (Strongleong) Chulkov <nikita_chul@mail.ru>
