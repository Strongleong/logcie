# Changelog

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
