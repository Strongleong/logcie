// Most of Logcie is tuned with macros defined before the include. They have to
// come first because the header reads them as it is compiled, so a define
// placed after #include <logcie.h> does nothing.
//
// The ones here change what you see. The next example covers the ones that
// change how memory is used.

// The built-in sink's format, so you never have to build a sink just to change
// how the default output looks
#define LOGCIE_DEFAULT_SINK_FORMAT "$c$l$r $M | $m"

// Module names are a hierarchy; this is the character that separates the
// levels of it. '.' by default
#define LOGCIE_MODULE_SEPARATOR '/'

// How many sinks can be registered at once. Sinks live in a fixed array, so
// this is the size of it and Logcie never allocates for them
#define LOGCIE_MAX_SINKS 4

#define LOGCIE_MODULE "app"
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

// A color per level, used by $c. Count_LOGCIE_LEVEL is the array size Logcie
// expects; a shorter one is a bug it cannot detect for you
static const char *my_colors[Count_LOGCIE_LEVEL] = {
  "\x1b[90m",    // TRACE   gray
  "\x1b[94m",    // DEBUG   light blue
  "\x1b[92m",    // VERBOSE light green
  "\x1b[1;32m",  // INFO    bright green
  "\x1b[33m",    // WARN    yellow
  "\x1b[1;33m",  // ERROR   bright yellow
  "\x1b[1;31m",  // FATAL   bright red
};

int main(void) {
  LOGCIE_INFO("default sink, format from LOGCIE_DEFAULT_SINK_FORMAT");

  // The separator is why "net" matches "net/http" but not "network"
  LOGCIE_LOG_MOD("net/http", INFO, "a submodule of net");

  logcie_set_colors(my_colors);
  LOGCIE_WARN("now in the colors above");

  // NULL puts the built-in table back
  logcie_set_colors(NULL);
  LOGCIE_WARN("back to the built-in colors");

  // LOGCIE_MAX_SINKS is 4 here and the built-in sink is one of them, so the
  // fifth add fails rather than overrunning anything
  static Logcie_Sink spare[4];

  for (int i = 0; i < 4; i++) {
    spare[i] = *logcie_get_default_sink();
    printf("add %d: %s\n", i, logcie_add_sink(&spare[i]) ? "ok" : "refused, array full");
  }

  return 0;
}
