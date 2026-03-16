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
- [Sinks and Output Configuration](#sinks-and-output-configuration)
- [Format Tokens](#format-tokens)
- [Filters](#filters)
- [Customization](#customization)
- [API Reference](#api-reference)
- [Configuration Options](#configuration-options)
- [Limitations](#limitations)
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

### Simple Logging

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

Each sink can be configured with a minimum log level. Messages below this threshold are not emitted to that sink.

## Sinks and Output Configuration

A **sink** defines where log messages are written and how they are formatted.
You can add additional sinks for files, network sockets, or custom destinations.

### Default sink

Logcie automatically creates a default stdout sink so you can start logging immediately.
However, when you add your first custom sink using logcie_add_sink(), the default sink is automatically removed.
This design choice ensures you have full control over sink configuration once you start customizing.

This is how default sinks looks like:

```c
static Logcie_Sink default_stdout_sink = {
    .min_level = LOGCIE_LEVEL_TRACE,
    .sink      = stdout,
    .fmt       = "$c$L$r " LOGCIE_COLOR_GRAY "$f$x$r: $m",
    .formatter = logcie_printf_formatter,
};
```

Important behaviors to understand:
 - Initial state: By default, one stdout sink exists at index 0
 - First sink addition: When you add your first custom sink, the default sink is removed
 - Restoring defaults: Use logcie_remove_all_sinks() to return to the initial default configuration

Example:
```c
// Start with default stdout sink
LOGCIE_INFO("This goes to default stdout");

// Add a file sink - default sink is now REMOVED!
Logcie_Sink file_sink = { /* ... */ };
logcie_add_sink(&file_sink);

// Only file_sink receives this message
LOGCIE_INFO("This goes only to file");

// Restore default configuration
logcie_remove_all_sinks();
LOGCIE_INFO("Back to default stdout");
```

If you want to keep both the default stdout sink and add additional sinks, you must re-add it explicitly:

```c
// Get default sink before it's removed
Logcie_Sink *default_sink = logcie_get_sink(0);

// Add your custom sink (default sink will be removed)
Logcie_Sink file_sink = { /* ... */ };
logcie_add_sink(&file_sink);

// Re-add default sink if you want both
logcie_add_sink(default_sink);
// Now both sinks receive messages
```

### Creating a Custom Sink

```c
// Create a file sink for error logs
Logcie_Sink error_sink = {
    .sink = fopen("errors.log", "a"),
    .min_level = LOGCIE_LEVEL_ERROR,
    .fmt = "$d $t [$L] $f:$x - $m",
    .formatter = logcie_printf_formatter
};

// Add it to the logger
logcie_add_sink(&error_sink);
```

### Sink Structure

The `Logcie_Sink` structure has the following fields:

| Field       | Type                  | Description                                            |
| -------     | ------                | -------------                                          |
| `sink`      | `FILE*`               | Output stream (stdout, file pointer, etc.)             |
| `min_level` | `Logcie_LogLevel`     | Minimum log level to output                            |
| `fmt`       | `const char*`         | Format string using `$` tokens                         |
| `formatter` | `Logcie_FormatterFn*` | Formatter function (usually `logcie_printf_formatter`) |
| `filter`    | `Logcie_FilterFn*`    | Optional filter function (NULL for no filtering)       |
| `userdata`  | `void*`               | User data for custom filters                           |

### Module-Based Logging

`logcie_module` is variable that you can define in your source file for flexible filtering and additional information in logs.

For more info about filtering check out [filters section](#filters)

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

// Output: 2025-12-25 01:15:10 [INFO ] (network) Connection established to gnu.org
```

The module name will appear in logs when using the `$M` format token.

### Memory Management Notes

Since logcie_add_sink() stores the pointer to your sink structure (not a copy), you must ensure:
  - Stack-allocated sinks: Must not go out of scope while registered
  - Heap-allocated sinks: Must be freed only after removal
  - Modification: You can modify sink properties after adding (changes take effect immediately)

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

## Customization

### Custom Colors

You can override the default ANSI colors for each log level:

```c
// Define custom colors for each level
const char *custom_colors[Count_LOGCIE_LEVEL] = {
    "\x1b[90m",      // TRACE - bright black
    "\x1b[37m",      // DEBUG - white
    "\x1b[96m",      // VERBOSE - bright cyan
    "\x1b[34m",      // INFO - blue
    "\x1b[33m",      // WARN - yellow
    "\x1b[31m",      // ERROR - red
    "\x1b[35;1m"     // FATAL - bright magenta
};

// Apply the custom colors
logcie_set_colors(custom_colors);

// To reset to defaults
logcie_set_colors(NULL);
```

### Custom Formatters

You can create custom formatter functions for specialized output needs. A formatter function has the following signature:

```c
size_t my_formatter(Logcie_Sink *sink, Logcie_Log log, va_list *args);
```

The formatter should:
1. Process the sink's format string (`sink->fmt`)
2. Write output to `sink->sink`
3. Use `va_list` operations to handle printf-style arguments
4. Return the number of characters written

You don't *need* to write logs somewhere in formatter.
For example: you can send logs to remote API, or collect statistics.

### Internal Helper Functions

When LOGCIE_IMPLEMENTATION is defined, the following internal helper functions become available:

| Function                         | Description                                 |
| --------                         | -----------                                 |
| `get_logcie_level_label()`       | Returns lowercase level name (e.g., "info") |
| `get_logcie_level_label_upper()` | Returns uppercase level name (e.g., "INFO") |
| `get_logcie_level_color()`       | Returns ANSI color code for given level     |

These functions are declared as static inline and are primarily intended for use within custom formatters. They can only be used in translation units where LOGCIE_IMPLEMENTATION is defined.

Example usage in custom formatters:

```c
size_t my_formatter(Logcie_Sink *sink, Logcie_Log log, va_list *args) {
    fprintf(sink->sink, "[%s] ", get_logcie_level_label_upper(log.level));
    // ... rest of formatter
}
```

## API Reference

### Core Functions

| Function                         | Description                                                                 |
| ----------                       | -------------                                                               |
| `logcie_log()`                   | Internal function called by logging macros                                  |
| `logcie_get_sink_count()`        | Gets the number of sinks currently registered in the logger.                |
| `logcie_get_sink()`              | Retrieves a sink by its index in the sink array.                            |
| `logcie_add_sink()`              | Add a sink to the logger                                                    |
| `logcie_remove_sink()`           | Removes a sink from the logger by pointer.                                  |
| `logcie_remove_sink_by_index()`  | Removes a sink from the logger by index.                                    |
| `logcie_remove_and_free_sink()`  | Removes and frees a sink from the logger (if it was dynamically allocated). |
| `logcie_remove_all_sinks()`      | Set custom colors for log levels                                            |
| `logcie_remove_sink_and_close()` | Removes a sink and closes its file stream if it's not stdout/stderr         |
| `logcie_set_colors()`            | Allows customization of log level colors. Must                              |

### Built-in Formatters

| Function                    | Description                                              |
| ----------                  | -------------                                            |
| `logcie_printf_formatter()` | Default formatter using `$` tokens and printf formatting |

### Log Level Macros

| Macro              | Level   | Description                       |
| -------            | ------- | -------------                     |
| `LOGCIE_TRACE()`   | TRACE   | Most detailed tracing information |
| `LOGCIE_DEBUG()`   | DEBUG   | Debugging information             |
| `LOGCIE_VERBOSE()` | VERBOSE | Verbose operational details       |
| `LOGCIE_INFO()`    | INFO    | General informational messages    |
| `LOGCIE_WARN()`    | WARN    | Warning conditions                |
| `LOGCIE_ERROR()`   | ERROR   | Error conditions                  |
| `LOGCIE_FATAL()`   | FATAL   | Fatal conditions                  |

For compilers without full variadic macro support, use the `_VA` variants (e.g., `LOGCIE_TRACE_VA()`).

## Configuration Options

The following preprocessor defines can be set before including `logcie.h` to configure library behavior:

### Core Configuration

| Define                  | Purpose                                         | Default     |
| --------                | ---------                                       | ---------   |
| `LOGCIE_IMPLEMENTATION` | Enable implementation in one translation unit   | Not defined |
| `LOGCIE_DEF`            | Control function linkage (static, extern, etc.) | `extern`    |
| `LOGCIE_PEDANTIC`       | Enable strict C compatibility mode              | Not defined |

### Color Configuration

You can override the default ANSI color definitions:

```c
#define LOGCIE_COLOR_GRAY       "\x1b[90m"
#define LOGCIE_COLOR_BLUE       "\x1b[34m"
#define LOGCIE_COLOR_YELLOW     "\x1b[33m"
#define LOGCIE_COLOR_RED        "\x1b[31m"
#define LOGCIE_COLOR_BRIGHT_RED "\x1b[31;1m"
#define LOGCIE_COLOR_RESET      "\x1b[0m"
```

### Version Information

Logcie provides version macros for compile-time checks:

```c
// Version as separate components
LOGCIE_VERSION_MAJOR    // 1
LOGCIE_VERSION_MINOR    // 0
LOGCIE_VERSION_RELEASE  // 0

// Combined numeric version (major*10000 + minor*100 + release)
LOGCIE_VERSION_NUMBER   // 10000

// String version
LOGCIE_VERSION_STRING   // "1.0.0"
```

## Complete Example

```c
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"
#include <string.h>

// Define module names
static const char *logcie_module = "main";

// Custom filter: only log from specific file
uint8_t filter_by_file(Logcie_Sink *sink, Logcie_Log *log) {
    return strstr(log->location.file, "network.c") != NULL;
}

int main() {
    // Create a file sink for detailed logging
    FILE *logfile = fopen("app.log", "a");
    if (logfile) {
        Logcie_Sink file_sink = {
            .sink = logfile,
            .min_level = LOGCIE_LEVEL_DEBUG,
            .fmt = "$d $t [$L] $M - $m\n",
            .formatter = logcie_printf_formatter
        };
        logcie_add_sink(&file_sink);
    }

    // Customize stdout sink format
    Logcie_Sink console_sink = {
        .sink = stdout,
        .min_level = LOGCIE_LEVEL_INFO,
        .fmt = "$c[$L]$r $t - $m\n",
        .formatter = logcie_printf_formatter
    };
    logcie_add_sink(&console_sink);

    // Log some messages
    LOGCIE_INFO("Application v%s starting", LOGCIE_VERSION_STRING);
    LOGCIE_DEBUG("Initializing subsystems");

    // Simulate some work
    for (int i = 0; i < 3; i++) {
        LOGCIE_VERBOSE("Processing iteration %d", i);
    }

    LOGCIE_WARN("Configuration file not found, using defaults");
    LOGCIE_INFO("Application shutdown complete");

    return 0;
}
```

## Limitations

- **Not thread-safe** - Concurrent calls to logging functions from multiple threads may interleave output. Thread safety and multithreading is planned for version 1.0.0
- **Memory allocation** - The sink array uses `malloc()`/`realloc()` for dynamic growth
- **No built-in log rotation** - File management must be handled by the application (or just use `logrotate`)
- **Custom formatters require `va_list` handling** - Advanced usage requires understanding of variadic arguments

Future versions may address these limitations based on user feedback and requirements.

## License

Logcie is released under the MIT License:

```
Copyright (c) 2024 Nikita (Strongleong) Chulkov

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
```

For questions or contributions, contact: Nikita (Strongleong) Chulkov <nikita_chul@mail.ru>
