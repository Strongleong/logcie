# Changelog

## Upcoming

## v3.0.0

### Added
- `Logcie_Writer` gained a `flush` field, for destinations that buffer. It is
  called with the same `data` as `write`, and `NULL` means there is nothing to
  flush. `logcie_file_flush` is the built-in one to pair with
  `logcie_file_writer`.
- `logcie_flush()` flushes every registered sink. Call it before exiting, and
  before removing a sink -- logcie does not flush on removal, because closing
  the destination is yours to do. It is safe to call from inside a formatter or
  a writer, unlike logging, which is refused there unless
  `LOGCIE_ALLOW_RECURSIVE_LOGGING` is defined.
- Logs at `LOGCIE_AUTOFLUSH_LEVEL` or above now flush the sink they were
  written to, so a crash does not take the lines explaining it. Defaults to
  `LOGCIE_LEVEL_ERROR`; define `LOGCIE_AUTOFLUSH_DISABLE` to switch it off.

### Fixed
- `logcie.h` builds with `NDEBUG` again. A probe in `logcie_set_colors` was
  used only inside an assertion, so once assertions compiled away
  `-Wall -Wextra -Werror` rejected it.


### Changed
- **You are affected if you initialize `Logcie_Writer` positionally.** The
  struct is now `{write, flush, data}`, so `{my_writer, target}` puts your
  target in the flush slot. Add the flush argument, or `NULL`:
  `{my_writer, NULL, target}`. Designated initializers (`.write`, `.data`) do
  not need changing.

## v2.0.0

### Added
- `LOGCIE_MAX_LINE` (default 1024) -- stack buffer a line is formatted into
- `LOGCIE_MALLOC` / `LOGCIE_FREE` -- allocator used only for lines longer than
  `LOGCIE_MAX_LINE`, so an arena or a debug allocator can be plugged in
- `LOGCIE_NO_MALLOC` -- forbid allocation entirely; long lines are truncated
- `logcie_render_message()` -- renders `log->msg` and its arguments into a
  buffer, so a custom formatter does not reimplement the `va_list` handling

### Changed
- **Writer signature.** `Logcie_WriterFn` is now
  `size_t (*)(void *user_data, const Logcie_Log *log, const char *bytes, size_t len)`.
  Writers get formatted bytes instead of a printf format string plus a
  `va_list *`, and they get the log so a transport can use metadata as a value:
  syslog wants a priority, Android wants a priority and a tag, a network sink
  may want the module as a routing key. None of that can be recovered from the
  formatted bytes without parsing them back
- **One writer call is one complete line**, terminating newline included. A
  writer is never handed a fragment, so a sink that treats each call as one
  record is safe
- **`logcie_printf_formatter` is now `logcie_token_formatter`.** printf named
  the part every formatter shares, not the part that makes this one different:
  it renders `$` tokens. A JSON or binary formatter is the same interface
- **`logcie_printf_writer` is now `logcie_file_writer`.** No printf remained in
  it; the name now says what its `user_data` must be
- A line that fits `LOGCIE_MAX_LINE` is formatted with no allocation at all.
  Longer lines are rendered again into an exact-sized buffer, so they arrive
  whole rather than truncated
- `logcie_add_sink` no longer touches the default sink; it is removable and
  gettable instead
- A writer with NULL `user_data` discards, which is /dev/null without the open()

### Fixed
- The format string was walked twice, once to size the buffer and once to
  render. `snprintf` reports the length it would have written even when it
  truncates, so one pass now does both and the two walks can no longer drift
- `$<n` dropped the rest of the line when the preceding token was already at
  least `n` columns wide
- `logcie_add_sink` compared a `Logcie_Sink *` against a `Logcie_Sink` in the
  branch used when `__attribute__((constructor))` is unavailable, so the header
  did not compile on MSVC
- `logcie_file_writer` passed a `size_t` to the `int` precision of `%.*s`; it
  uses `fwrite` now, which also skips re-scanning bytes that are already
  formatted

### Migrating from v1

Writers gain a parameter and lose the varargs:

```c
/* v1 */ size_t w(void *data, const char *fmt, va_list *va, ...);
/* v2 */ size_t w(void *data, const Logcie_Log *log, const char *bytes, size_t len);
```

Write `bytes` directly; there is no format string to interpret. `log->msg` is
the format string from the call site, *not* the rendered text.

Rename `logcie_printf_formatter` to `logcie_token_formatter` and
`logcie_printf_writer` to `logcie_file_writer`.

If you relied on the first `logcie_add_sink` removing the built-in sink, call
`logcie_remove_sink(logcie_get_default_sink())` explicitly.

`$<n` pads to `n` columns rather than `n-1`, so a format tuned against the old
behaviour gains one space.

## v1.3.0

### Added
- `$N` token for nanoseconds
- Introduced `logcie_get_default_sink()`

## v1.2.1

### Fixed
- Enchanged compatibility with c++ standars <20 with -pedantic flag

## v1.2.0

### Added
- `LOGCIE_MAX_SINKS` (default 16) — sinks live in a fixed array
- `LOGCIE_DEBUG_CHECKS`, replacing the undocumented `_LOGCIE_DEBUG`

### Changed
- logcie no longer allocates. No malloc, realloc or free anywhere, which is
  what allows it on targets where dynamic allocation is banned.
  `logcie_add_sink` returns 0 once `LOGCIE_MAX_SINKS` sinks are registered
- `$d`, `$t` and `$z` convert the timestamp on first use rather than on every
  line. The instant recorded is unchanged, still captured at the call site
- Internal macros renamed to `LOGCIE_INTERNAL_*`. The previous names began with
  an underscore followed by an uppercase letter, which C reserves for the
  implementation

### Fixed
- Both examples in the header documentation did not compile: filter
  constructors were shown as struct fields, and `stdout`/`fopen()` appeared in
  file-scope initializers where they are not constant expressions
- Documentation for `$<n` padding, sink limits, filter status, and when
  `logcie_add_sink` fails

## v1.1.0
- Introduced hierarchical modules and filterling by module prefixes
  with `logcie_filter_module_prefix_eq`

## v1.0.1

### Fixed
- `logcie.h` failed to compile with `-pedantic`: an empty `LOGCIE_MUTEX_DECLARE`
  left a stray `;` at file scope
- `LOGCIE_ALLOW_RECURSIVE_LOGGING` failed under `-Werror`: `logcie_log_depth`
  was declared but unused
- `logcie_filter_not` did not compile in C++: it took the address of a compound
  literal, which is an rvalue there
- `logcie_printf_formatter` asserted on `writer->data`, which it never uses,
  breaking custom writers that do not need it

### Added
- `.tspec` test suite in `tests/`, run with `./build tests`

## v1.0.0
- First release
