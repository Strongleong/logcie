// A filter is one function answering "does this log belong in this sink":
//
//   uint8_t my_filter(const void *data, Logcie_Log *log);
//
// Anything you can express in C is a valid filter.
// logcie_filter_and, _or and _not are filters too,
// which is why they nest: each takes filters and returns one.
//
// They are macros producing a value, so they can sit in an initializer, but the
// operands live in compound literals that last only as long as the enclosing
// scope. Build them where the sink lives.
#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

void net_work(void);

// Everything at INFO and above, plus anything from net.* at any level.
static Logcie_Sink console = {
  .formatter = {logcie_token_formatter, "[$L] ($M) $m"},
  .writer    = {logcie_file_writer, NULL},
  .filter    = logcie_filter_or(
    logcie_filter_level_min(LOGCIE_LEVEL_INFO),
    logcie_filter_module_prefix_eq("net")
  )
};

int main(void) {
  console.writer.data = stdout;

  logcie_remove_sink(logcie_get_default_sink());
  logcie_add_sink(&console);

  LOGCIE_DEBUG("dropped: below INFO and not net");
  LOGCIE_INFO("kept: INFO");

  net_work();

  // "net" does not match "network": a prefix has to end on a separator or on
  // the end of the name, or the hierarchy would just be a substring search
  LOGCIE_LOG_MOD("network", DEBUG, "dropped: prefixes stop at a separator");

  return 0;
}
