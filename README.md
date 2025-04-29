# Logcie

Logcie is a minimalistic, single-header C logging library designed to provide logging functionality with support for log levels, formatted output, color support, and customizable sinks.

**NOTE** This library is in development and VERY unfinished. You can add "for now" to literally anything in this readme

## Features

 - Log Levels: Trace, Debug, Verbose, Info, Warn, Error, Fatal.
 - Customizable Format: Define your log format with placeholders for various log data.
 - Color Support: Log messages can be color-coded based on their log level.
 - Custom Sinks: Support for writing logs to different outputs, such as files or stdout.
 - Filters: Combine filters using logical operations (AND, OR, NOT).

You can find example at [./examples/]

## Installation

Simply include logcie.h in your project. To use Logcie, define LOGCIE_IMPLEMENTATION before including the header in one .c file.

```c
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"
```

In other files, you only need to include logcie.h without defining LOGCIE_IMPLEMENTATION.

```c
#include "logcie.h"
```

## Usage

### Basic Logging

You can use the logging macros to log messages at different levels:

```c
LOGCIE_TRACE("This is a trace message");
LOGCIE_DEBUG("This is a debug message");
LOGCIE_INFO("This is an info message");
LOGCIE_WARN("This is a warning message");
LOGCIE_ERROR("This is an error message");
LOGCIE_FATAL("This is a fatal error message");
```

The macros automatically include the file and line number information.

### Custom Log Format

Logcie allows you to define the log output format. You can customize the format for each log sink. A format string may include the following placeholders:

 - $m: Log message.
 - $f: File name.
 - $x: Line number.
 - $M: Module name.
 - $l: Log level (lowercase).
 - $L: Log level (uppercase).
 - $c: Color for the current log level.
 - $r: Color reset.
 - $d: Current date (YYYY-MM-DD).
 - $t: Current time (HH:MM:SS).
 - $z: Time zone offset.

Here is an example:

```c
Logcie_Sink custom_sink = {
    .min_level = LOGCIE_LEVEL_DEBUG,
    .sink = stdout,
    .fmt = "$c$L$r $d $t $f:$x: $m"
};

logcie_add_sink(&custom_sink);
```

### Filters

Logcie supports filters, which allow you to control which log messages should be logged based on criteria. You can combine filters with logical AND, OR, and NOT operations.

```c
uint8_t filter1(Logcie_Sink *sink, Logcie_Log *log, const char *file, uint32_t line) {
    (void)sink; (void)file; (void)line;
    return log->level >= LOGCIE_LEVEL_INFO && log->level <= LOGCIE_ERROR;
}

uint8_t filter2(Logcie_Sink *sink, Logcie_Log *log, const char *file, uint32_t line) {
    (void)sink; (void)file; (void)line;
    return log->module && strcmp(log->module, "module") == 0;
}

logcie_set_filter_and(&custom_sink, filter1, filter2);
logcie_set_filter_or(&custom_sink, filter1, filter2);
logcie_set_filter_not(&custom_sink, filter1);
```

### Setting Log Colors

You can override the default colors for each log level. To do so, provide an array of colors for each log level:

```c
const char *custom_colors[Count_LOGCIE_LEVEL] = {
    LOGCIE_COLOR_BLUE,   // TRACE
    LOGCIE_COLOR_GREEN,  // DEBUG
    LOGCIE_COLOR_CYAN,   // VERBOSE
    LOGCIE_COLOR_BLUE,   // INFO
    LOGCIE_COLOR_YELLOW, // WARN
    LOGCIE_COLOR_RED,    // ERROR
    LOGCIE_COLOR_BRIGHT_RED // FATAL
};

logcie_set_colors(custom_colors);
```

You can pass `NULL` to `logcie_set_colors` to reset colors to default
