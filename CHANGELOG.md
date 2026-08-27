# Changelog

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
