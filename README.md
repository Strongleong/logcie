# Logcie

**Logcie** is a lightweight, modular, single-header logging library for C.

It provides:
- Multiple log levels
- ANSI color support
- Fully customizable output format
- Filters (with combinators like AND/OR/NOT)
- Support for multiple sinks (stdout, file, etc.)

> ⚠️ Note: This library is under active development. Many APIs or behaviors may change. Add "for now" to basically anything in this README.

---

## 🔧 Installation

Copy `logcie.h` to your project and include it:

```c
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"
```

In other translation units (files), use:

```c
#include "logcie.h"
```

---

## 🪵 Logging Usage

Logcie provides macros for common log levels:

```c
LOGCIE_TRACE("Hello from trace");
LOGCIE_DEBUG("Value: %d", val);
LOGCIE_VERBOSE("Running details...");
LOGCIE_INFO("All good here.");
LOGCIE_WARN("Something might be wrong.");
LOGCIE_ERROR("Something is wrong!");
LOGCIE_FATAL("We're going down!");
```

These macros include:
- Log level
- Source file
- Line number
- Optional module (via `logcie_module`)

You can also use `LOGCIE_TRACE_VA` style macros if variadic support is limited.

---

## 🎨 Formatting Output

Each `Logcie_Sink` allows a format string using special `$` tokens:

| Token   | Description                                    |
| ------- | -------------                                  |
| `$m`    | Log message (supports printf-style formatting) |
| `$f`    | File name                                      |
| `$x`    | Line number                                    |
| `$M`    | Module name (`logcie_module`)                  |
| `$l`    | Log level (lowercase)                          |
| `$L`    | Log level (uppercase)                          |
| `$c`    | Start color for log level                      |
| `$r`    | Reset color                                    |
| `$d`    | Date (YYYY-MM-DD)                              |
| `$t`    | Time (HH:MM:SS)                                |
| `$z`    | Timezone offset                                |
| `$$`    | Literal `$`                                    |

### Example

```c
Logcie_Sink sink = {
    .min_level = LOGCIE_LEVEL_DEBUG,
    .sink = stdout,
    .fmt = "$c$L$r $d $t [$f:$x] $m",
    .formatter = logcie_printf_formatter,
};

logcie_add_sink(&sink);
```

---

## 🧪 Filters

Filters allow fine-grained control over which logs are emitted to a sink. You can:
- Write custom filters
- Combine filters with `AND`, `OR`, and `NOT`

### Example

```c
uint8_t level_filter(Logcie_Sink *sink, Logcie_Log *log) {
    return log->level >= LOGCIE_LEVEL_INFO;
}

uint8_t module_filter(Logcie_Sink *sink, Logcie_Log *log) {
    return log->module && strcmp(log->module, "core") == 0;
}

// Allocate contexts statically
static Logcie_CombinedFilterContext and_ctx;
static Logcie_NotFilterContext not_ctx;

// Combine them
logcie_set_filter_and(&sink, level_filter, module_filter, &and_ctx);
logcie_set_filter_not(&sink, module_filter, &not_ctx);
```

---

## 🎨 Custom Log Level Colors

Override the default ANSI colors per log level:

```c
const char *my_colors[Count_LOGCIE_LEVEL] = {
    LOGCIE_COLOR_GRAY,  // TRACE
    LOGCIE_COLOR_GRAY,  // DEBUG
    LOGCIE_COLOR_CYAN,  // VERBOSE
    LOGCIE_COLOR_BLUE,  // INFO
    LOGCIE_COLOR_YELLOW,// WARN
    LOGCIE_COLOR_RED,   // ERROR
    LOGCIE_COLOR_BRIGHT_RED // FATAL
};

logcie_set_colors(my_colors);
```

To reset to defaults:
```c
logcie_set_colors(NULL);
```

---

## 📚 Example

```c
#define LOGCIE_IMPLEMENTATION
#include "logcie.h"

int main() {
    LOGCIE_INFO("Program started: version %s", LOGCIE_VERSION_STRING);
    return 0;
}
```

You can find more examples at [./examples/](./examples/)

---

## 🧱 TODOs & Limitations

- No built-in thread safety yet
- No sink destruction API
- No output alignment (TODO noted in code)
- Custom formatters are manual and require `va_list` handling

---

## 📜 License

MIT License © 2025 [Nikita (Strongleong) Chulkov](mailto:nikita_chul@mail.ru)
