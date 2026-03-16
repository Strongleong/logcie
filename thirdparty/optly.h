/*
  optly.h — v1.3.1
  Single-header command line argument parser for C.

  Features
  --------
  * STB-style single-header library
  * Commands and command-specific flags
  * Long flags (--verbose)
  * Short flags (-v)
  * Batched short flags (-abc)
  * Inline flag values (--threads=4)
  * Separate flag values (--threads 4)
  * Typed flag values
  * Positional arguments
  * Optional commands
  * Optional flags
  * No dynamic memory allocation
  * Portable C (C99)

  Basic Usage
  -----------

    #define OPTLY_IMPLEMENTATION
    #include "optly.h"

    // You need to define "main" command, which is your application.
    static OptlyCommand cmd = {

      // Name will be used in usage. If you set is as NULL it will be argv[0]
      .name = NULL,

      // If not null, then it will be used in usage
      .description = NULL,

      // This is your "top-level global" flags. They should be specified before any command (./app --thread 2 -v)
      .flags = (OptlyFlag[]){
        // Long (full name)   short name   description (for usage)  Default value           Flag value  type
        { "verbose",          'v',         "Enable verbose output", .value.as_bool = false, .type = OPTLY_TYPE_BOOL   },

        // Values are unions, so you need to specify member of value with correct type some way
        { "threads",          't',         "Worker threads", false, {.as_uint32 = 4},       OPTLY_TYPE_UINT32 },

        // Flag arrays should alwasy ends with NULL_FLAG. Try to not forget about it :)
        NULL_FLAG,
      },

      // This is your commands. (git `commit`, docker `compose` `up`)
      .commands = (OptlyCommand[]){

        // Instead of defining whole struct manually you can use helper functions
        optly_command("run", "Runs server",

          // optly_flags macro deals with type castings and closing array with NULL_FLAG
          optly_flags(

            // This is *command* flag. `./app -p 8080 run` will not work, but `./app run -p 8080` will
            optly_flag_uint16("port", 'p', "Server port", .value.as_uint16 = 8080)

            // NOTE: this flag and 'verbose' GLOBAL flag are not the same
            // `./app -v run` - enables global -v flag
            // `./app run -v` - enables command -v flag
            optly_flag_bool("verbose", 'v', "Enable verbose output for worker", .value.as_bool = false)
          ),

          // This is subcommands of command "run"
          optly_commands(

            // `./app run check` subcommand have no description, flags or subcommands.
            //  NULL is here to suppress warning about not providing argument in variadic macro
            optly_command("check", NULL),

            // `./app run dump_config` subcommands
            optly_command("dump_config",

              // We skipped description, so we need to specify fileds now
              .flags = optly_flags(
                optly_flag_bool("color", 'c', "Show colors", .value.as_bool = false)
              )
            )
          ),

          // Positioanl arguments can be defined lilke this
          optly_positionals(optly_positional("address", "Address to listen on", .min = 0, .max = 1)),
        ),
      }
    };

    int main(int argc, char **argv) {
      optly_parse_args(argc, argv, &cmd);

      printf("Verbose: %s\n", optly_bool(&cmd, "verbose") ? "true" : "false");
      printf("Threads: %u\n", optly_uint32(&cmd, "threads"));

      if (!cmd.next_command) {
        return 0;
      }

      printf("Command: %s\n", cmd.next_command->name);

      printf("Verbose: %s\n", optly_bool(cmd.next_command, "verbose") ? "true" : "false");
      printf("Port: %u\n", optly_uint16(cmd.next_command, "port"));

      if (cmd.next_command->next_command) {
        printf("Command: %s\n", cmd.next_command->next_command->name);
      }

      OptlyPositional *address = optly_get_positional(&cmd, "address");

      if (address && address->count == 1) {
        printf("Address: %s\n", address->values[1]);
      } else {
        printf("Address: 0.0.0.0\n");
      }
    }

  Commands
  --------

  Commands are positional tokens that do not begin with '-'.

    app build
    app run

  Each command may define its own flags.

  Flags
  -----

  Flags may be long or short.

    --verbose
    -v

  Flags may have values.

    --threads=4
    --threads 4
    -t 4

  Short flags can be batched.

    -abc  ->  -a -b -c

  Positional Arguments
  --------------------

  Any non-flag tokens after command selection are stored as positional arguments.

    app build file1 file2

  Access:

  Named positional: (can have many valies inside)

    Positional *pos = optly_get_positional(&cmd, "name");

  Or directly through command:

    for (OptlyPositional *p = cmd.positionals; p->name; p++) {
      // ...
    }

  Help command/flag generation
  ----------------------------

  You can define

    #define OPTLY_GEN_HELP_FLAG
    #define OPTLY_GEN_HELP_COMMAND

  to generate help flag `--help | -h` and/or help command `help cmd`.
  If help command/flag would be found during parsing usage would be automatically called

  Licese
  ------

  MIT/Public domain - choose whitchever you prefer
*/

#ifndef OPTLY_H
#define OPTLY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef OPTLYDEF
#define OPTLYDEF
#endif  // OPTLYDEF

// Versioning macros
#define OPTLY_VERSION_MAJOR         1
#define OPTLY_VERSION_MINOR         3
#define OPTLY_VERSION_RELEASE       1
#define OPTLY_VERSION_NUMBER        (OPTLY_VERSION_MAJOR * 100 * 100 + OPTLY_VERSION_MINOR * 100 + OPTLY_VERSION_RELEASE)
#define OPTLY_VERSION_FULL          OPTLY_VERSION_MAJOR.OPTLY_VERSION_MINOR.OPTLY_VERSION_RELEASE
#define OPTLY_QUOTE(str)            #str
#define OPTLY_EXPAND_AND_QUOTE(str) OPTLY_QUOTE(str)
#define OPTLY_VERSION_STRING        OPTLY_EXPAND_AND_QUOTE(OPTLY_VERSION_FULL)

// If you need more that 64 positional arguments per ONE command
// you should look in the mirror really deep
#ifndef OPTLY_MAX_POSITIONALS
#define OPTLY_MAX_POSITIONALS 64
#endif

#ifndef OPTLY_FLAG_BUFFER_LENGTH
#define OPTLY_FLAG_BUFFER_LENGTH 256
#endif

typedef enum OptlyFlagType {
  OPTLY_TYPE_BOOL,
  OPTLY_TYPE_CHAR,
  OPTLY_TYPE_STRING,
  OPTLY_TYPE_INT8,
  OPTLY_TYPE_INT16,
  OPTLY_TYPE_INT32,
  OPTLY_TYPE_INT64,
  OPTLY_TYPE_UINT8,
  OPTLY_TYPE_UINT16,
  OPTLY_TYPE_UINT32,
  OPTLY_TYPE_UINT64,
  OPTLY_TYPE_FLOAT,
  OPTLY_TYPE_DOUBLE,
} OptlyFlagType;

typedef union OptlyFlagValue {
  bool as_bool;

  char  as_char;
  char *as_string;

  int8_t  as_int8;
  int16_t as_int16;
  int32_t as_int32;
  int64_t as_int64;

  uint8_t  as_uint8;
  uint16_t as_uint16;
  uint32_t as_uint32;
  uint64_t as_uint64;

  float  as_float;
  double as_double;
} OptlyFlagValue;

typedef struct {
  char *fullname;
  char  shortname;
  char *description;

  bool required;
  bool present;

  OptlyFlagValue value;
  OptlyFlagType  type;
} OptlyFlag;

typedef struct {
  char *name;
  char *description;

  size_t min;
  size_t max;

  char  *values[OPTLY_MAX_POSITIONALS];
  size_t count;
} OptlyPositional;

typedef struct OptlyCommand OptlyCommand;
struct OptlyCommand {
  char *name;
  char *description;

  OptlyFlag       *flags;
  OptlyCommand    *commands;
  OptlyPositional *positionals;

  OptlyCommand *next_command;
};

#define NULL_FLAG       {.fullname = NULL, .shortname = 0, .value = {.as_int64 = 0}, .type = 0}
#define NULL_COMMAND    {.name = NULL, .flags = NULL}
#define NULL_POSITIONAL {.name = NULL}

// NOTE: Forcing designated initializer to automatically zero initialize missing fields
#define optly_flag(name, ...)        \
  (OptlyFlag) {                      \
    .fullname = (name), __VA_ARGS__, \
    .present  = false                \
  }
#define optly_command(namme, ...) \
  (OptlyCommand) {                \
    .name = (namme), __VA_ARGS__  \
  }

#define optly_flags(...)   \
  (OptlyFlag[]) {          \
    __VA_ARGS__, NULL_FLAG \
  }
#define optly_commands(...)   \
  (OptlyCommand[]) {          \
    __VA_ARGS__, NULL_COMMAND \
  }

#define optly_positional(namme, ...) \
  (OptlyPositional) {                \
    .name = (namme), __VA_ARGS__     \
  }
#define optly_positionals(...)   \
  (OptlyPositional[]) {          \
    __VA_ARGS__, NULL_POSITIONAL \
  }

#define optly_flag_bool(name, ...)   optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_BOOL)
#define optly_flag_char(name, ...)   optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_CHAR)
#define optly_flag_string(name, ...) optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_STRING)
#define optly_flag_int8(name, ...)   optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_INT8)
#define optly_flag_int16(name, ...)  optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_INT16)
#define optly_flag_int32(name, ...)  optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_INT32)
#define optly_flag_int64(name, ...)  optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_INT64)
#define optly_flag_uint8(name, ...)  optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_UINT8)
#define optly_flag_uint16(name, ...) optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_UINT16)
#define optly_flag_uint32(name, ...) optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_UINT32)
#define optly_flag_uint64(name, ...) optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_UINT64)
#define optly_flag_float(name, ...)  optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_FLOAT)
#define optly_flag_double(name, ...) optly_flag(name, __VA_ARGS__, .type = OPTLY_TYPE_DOUBLE)

OPTLYDEF OptlyCommand *optly_parse_args(int argc, char *argv[], OptlyCommand *main_cmd);

OPTLYDEF const OptlyFlag *optly_get_flag(const OptlyFlag *flags, const char *name);

OPTLYDEF bool     optly_bool(OptlyCommand *command, const char *name);
OPTLYDEF char     optly_char(OptlyCommand *command, const char *name);
OPTLYDEF char    *optly_string(OptlyCommand *command, const char *name);
OPTLYDEF int8_t   optly_int8(OptlyCommand *command, const char *name);
OPTLYDEF int16_t  optly_int16(OptlyCommand *command, const char *name);
OPTLYDEF int32_t  optly_int32(OptlyCommand *command, const char *name);
OPTLYDEF int64_t  optly_int64(OptlyCommand *command, const char *name);
OPTLYDEF uint8_t  optly_uint8(OptlyCommand *command, const char *name);
OPTLYDEF uint16_t optly_uint16(OptlyCommand *command, const char *name);
OPTLYDEF uint32_t optly_uint32(OptlyCommand *command, const char *name);
OPTLYDEF uint64_t optly_uint64(OptlyCommand *command, const char *name);
OPTLYDEF float    optly_float(OptlyCommand *command, const char *name);
OPTLYDEF double   optly_double(OptlyCommand *command, const char *name);

OPTLYDEF bool optly_is_command(OptlyCommand *command, const char *name);

OPTLYDEF OptlyPositional *optly_get_positional(OptlyCommand *command, const char *name);

OPTLYDEF void optly_usage(OptlyCommand *command);

static inline bool optly_is_flag_null(const OptlyFlag *flag) {
  return flag == NULL || (flag->fullname == NULL && flag->shortname == 0);
}

static inline bool optly_is_command_null(const OptlyCommand *cmd) {
  return cmd == NULL || cmd->name == NULL;
}

inline OPTLYDEF bool optly_flag_value_bool(const OptlyCommand *command, const char *name);
inline OPTLYDEF char optly_flag_value_char(const OptlyCommand *command, const char *name);
inline OPTLYDEF char *optly_flag_value_string(const OptlyCommand *command, const char *name);
inline OPTLYDEF int8_t optly_flag_value_int8(const OptlyCommand *command, const char *name);
inline OPTLYDEF int16_t optly_flag_value_int16(const OptlyCommand *command, const char *name);
inline OPTLYDEF int32_t optly_flag_value_int32(const OptlyCommand *command, const char *name);
inline OPTLYDEF int64_t optly_flag_value_int64(const OptlyCommand *command, const char *name);
inline OPTLYDEF uint8_t optly_flag_value_uint8(const OptlyCommand *command, const char *name);
inline OPTLYDEF uint16_t optly_flag_value_uint16(const OptlyCommand *command, const char *name);
inline OPTLYDEF uint32_t optly_flag_value_uint32(const OptlyCommand *command, const char *name);
inline OPTLYDEF uint64_t optly_flag_value_uint64(const OptlyCommand *command, const char *name);
inline OPTLYDEF float optly_flag_value_float(const OptlyCommand *command, const char *name);
inline OPTLYDEF double optly_flag_value_double(const OptlyCommand *command, const char *name);
inline OPTLYDEF OptlyPositional *optly_get_positional(OptlyCommand *command, const char *name);

#endif  // OPTLY_H

// -----------------------------------

#ifdef OPTLY_IMPLEMENTATION

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Logcie integration

#ifndef OPTLY_LOG_BACKEND
#ifdef LOGCIE
#ifdef LOGCIE_VA_LOGS
#define OPTLY_LOG_BACKED(level, ...) LOGCIE_##level##_VA(__VA_ARGS__)
#else
#define OPTLY_LOG_BACKED(level, ...) LOGCIE_##level(__VA_ARGS__)
#endif
#else
#define OPTLY_LOG_BACKED(level, ...)
#endif
#endif

#ifndef OPTLY_LOG_TRACE
#define OPTLY_LOG_TRACE(...) OPTLY_LOG_BACKED(TRACE, __VA_ARGS__)
#endif

#ifndef OPTLY_LOG_DEBUG
#define OPTLY_LOG_DEBUG(...) OPTLY_LOG_BACKED(DEBUG, __VA_ARGS__)
#endif

#ifndef OPTLY_LOG_VERBOSE
#define OPTLY_LOG_VERBOSE(...) OPTLY_LOG_BACKED(VERBOSE, __VA_ARGS__)
#endif

#ifndef OPTLY_LOG_INFO
#define OPTLY_LOG_INFO(...) OPTLY_LOG_BACKED(INFO, __VA_ARGS__)
#endif

#ifndef OPTLY_LOG_WARN
#define OPTLY_LOG_WARN(...) OPTLY_LOG_BACKED(WARN, __VA_ARGS__)
#endif

#ifndef OPTLY_LOG_ERROR
#define OPTLY_LOG_ERROR(...) OPTLY_LOG_BACKED(ERROR, __VA_ARGS__)
#endif

#ifndef OPTLY_LOG_FATAL
#define OPTLY_LOG_FATAL(...) OPTLY_LOG_BACKED(FATAL, __VA_ARGS__)
#endif

#define SHIFT_ARG(argv, argc) (++(argv), --(argc))

#define UNREACHABLE(message)                                               \
  do {                                                                     \
    OPTLY_LOG_FATAL("%s:%d: UNREACHABLE: %s", __FILE__, __LINE__, message) \
    abort();                                                               \
  } while (0)

static bool optly__has_flags(OptlyCommand *cmd) {
  return cmd && cmd->flags && !optly_is_flag_null(cmd->flags);
}

static bool optly__has_commands(OptlyCommand *cmd) {
  return cmd && cmd->commands && !optly_is_command_null(cmd->commands);
}

static bool optly__has_positionals(OptlyCommand *cmd) {
  return cmd && cmd->positionals && cmd->positionals->name;
}

static void optly__usage_positionals_signature(OptlyPositional *pos) {
  if (!pos) return;

  for (; pos->name; pos++) {
    bool required = pos->min > 0;
    bool variadic = pos->max == 0 || pos->max > 1;

    if (required)
      fprintf(stderr, " <%s%s>", pos->name, variadic ? "..." : "");
    else
      fprintf(stderr, " [%s%s]", pos->name, variadic ? "..." : "");
  }
}

static void optly__usage_signature(OptlyCommand *cmd) {
  fprintf(stderr, "Usage: %s", cmd->name);

  if (optly__has_flags(cmd))
    fprintf(stderr, " [FLAGS]");

  if (optly__has_positionals(cmd))
    optly__usage_positionals_signature(cmd->positionals);

  if (optly__has_commands(cmd))
    fprintf(stderr, " <COMMAND>");

  fprintf(stderr, "\n");
}

static uint8_t type_name_pad = 8;

static const char *optly__flag_type_name(OptlyFlagType type) {
  switch (type) {
    case OPTLY_TYPE_BOOL:   return "";
    case OPTLY_TYPE_CHAR:   return "<char>";
    case OPTLY_TYPE_STRING: return "<str>";
    case OPTLY_TYPE_INT8:   return "<i8>";
    case OPTLY_TYPE_INT16:  return "<i16>";
    case OPTLY_TYPE_INT32:  return "<i32>";
    case OPTLY_TYPE_INT64:  return "<i64>";
    case OPTLY_TYPE_UINT8:  return "<u8>";
    case OPTLY_TYPE_UINT16: return "<u16>";
    case OPTLY_TYPE_UINT32: return "<u32>";
    case OPTLY_TYPE_UINT64: return "<u64>";
    case OPTLY_TYPE_FLOAT:  return "<float>";
    case OPTLY_TYPE_DOUBLE: return "<double>";
  }

  return "";
}

static size_t optly__flag_print_width(OptlyFlag *flags) {
  size_t max = 0;

  for (OptlyFlag *flag = flags; !optly_is_flag_null(flag); flag++) {
    size_t len = 0;

    if (flag->shortname) {
      len += 2;
    }

    if (flag->fullname) {
      len += strlen(flag->fullname) + 3;
    }

    if (len > max) {
      max = len;
    }
  }

  return max;
}

static void optly__print_default_value(OptlyFlag *flag) {
  if (flag->type == OPTLY_TYPE_BOOL) return;

  fprintf(stderr, " (default: ");

  switch (flag->type) {
    case OPTLY_TYPE_CHAR:   fprintf(stderr, "%c", flag->value.as_char); break;
    case OPTLY_TYPE_STRING: fprintf(stderr, "%s", flag->value.as_string); break;
    case OPTLY_TYPE_INT8:   fprintf(stderr, "%d", flag->value.as_int8); break;
    case OPTLY_TYPE_INT16:  fprintf(stderr, "%d", flag->value.as_int16); break;
    case OPTLY_TYPE_INT32:  fprintf(stderr, "%d", flag->value.as_int32); break;
    case OPTLY_TYPE_INT64:  fprintf(stderr, "%lld", (long long)flag->value.as_int64); break;
    case OPTLY_TYPE_UINT8:  fprintf(stderr, "%u", flag->value.as_uint8); break;
    case OPTLY_TYPE_UINT16: fprintf(stderr, "%u", flag->value.as_uint16); break;
    case OPTLY_TYPE_UINT32: fprintf(stderr, "%u", flag->value.as_uint32); break;
    case OPTLY_TYPE_UINT64: fprintf(stderr, "%llu", (unsigned long long)flag->value.as_uint64); break;
    case OPTLY_TYPE_FLOAT:  fprintf(stderr, "%f", flag->value.as_float); break;
    case OPTLY_TYPE_DOUBLE: fprintf(stderr, "%f", flag->value.as_double); break;
    default:                break;
  }

  fprintf(stderr, ")");
}

static size_t optly__command_print_width(OptlyCommand *commands) {
  size_t max = 0;

  for (OptlyCommand *cmd = commands; !optly_is_command_null(cmd); cmd++) {
    size_t len = strlen(cmd->name);

    if (len > max) {
      max = len;
    }
  }

  return max;
}

static void optly__usage_commands_list(OptlyCommand *commands) {
  if (!commands) return;

  fprintf(stderr, "\nCOMMANDS\n");
  size_t pad = optly__command_print_width(commands);

  for (OptlyCommand *cmd = commands; !optly_is_command_null(cmd); cmd++) {
    fprintf(stderr, "  %-*s  %s\n", (int)pad, cmd->name, cmd->description ? cmd->description : "");
  }

#ifdef OPTLY_GEN_HELP_COMMAND
  fprintf(stderr, "  %-*s  %s\n", (int)pad, "help", "Show help for command");
#endif
}

static void optly__usage_flags(OptlyFlag *flags) {
  if (!flags) return;

  fprintf(stderr, "\nFLAGS\n");

  size_t pad = optly__flag_print_width(flags);

  for (OptlyFlag *flag = flags; !optly_is_flag_null(flag); flag++) {
    char        buf[OPTLY_FLAG_BUFFER_LENGTH];
    const char *type = optly__flag_type_name(flag->type);

    if (flag->shortname && flag->fullname) {
      snprintf(buf, sizeof(buf), "-%c --%s %s", flag->shortname, flag->fullname, type);
    } else if (flag->fullname) {
      snprintf(buf, sizeof(buf), "--%s %s", flag->fullname, type);
    } else {
      snprintf(buf, sizeof(buf), "-%c %s", flag->shortname, type);
    }

    fprintf(stderr, "  %-*s  %s", (int)pad + type_name_pad, buf, flag->description ? flag->description : "");

    if (flag->required) {
      fprintf(stderr, " (required)");
    } else if (flag->value.as_string != NULL) {
      optly__print_default_value(flag);
    }

    fprintf(stderr, "\n");
  }

#ifdef OPTLY_GEN_HELP_FLAG
  char *help = "-h --help";
  fprintf(stderr, "\n  %-*s Show this message\n", (int)pad, help);
#endif
}

static void optly__usage_positionals(OptlyPositional *pos) {
  if (!pos) return;

  fprintf(stderr, "\nPOSITIONAL ARGUMENTS\n");

  for (; pos->name; pos++) {
    fprintf(stderr, "  %s  (%zu..%zu values)\n", pos->name, pos->min, pos->max ? pos->max : SIZE_MAX);
  }
}

OPTLYDEF void optly_usage(OptlyCommand *command) {
  if (command->description) {
    fprintf(stderr, "%s\n\n", command->description);
  }

  optly__usage_signature(command);

  optly__usage_commands_list(command->commands);
  optly__usage_positionals(command->positionals);
  optly__usage_flags(command->flags);

#ifdef OPTLY_GET_HELP_COMMAND
  fprintf(stderr, "\nRun '%s help <command>' for more information.\n", command->name);
#endif
}

#ifdef OPTLY_GET_HELP_FLAG
static OptlyFlag optly__help_flag = {
  .fullname    = "help",
  .shortname   = 'h',
  .description = "Show this help message",
  .required    = false,
  .present     = false,
  .value       = {.as_bool = false},
  .type        = OPTLY_TYPE_BOOL
};
#endif

#ifdef OPTLY_GET_HELP_COMMAND
static OptlyCommand optly__help_command = {
  .name        = "help",
  .description = "Show help for command"
};
#endif

/**
 * Check if argument matches a flag definition.
 */
static bool optly__flag_matches(const char *arg, const OptlyFlag *flag) {
  bool is_short = arg[1] != '-';

  return (!is_short && strcmp(arg + 2, flag->fullname) == 0) ||
         (is_short && (arg[1] == flag->shortname));
}

/**
 * Find a flag by argument.
 */
static OptlyFlag *optly__find_flag(const char *arg, OptlyFlag *flags) {
  for (OptlyFlag *f = flags; !optly_is_flag_null(f); f++) {
    if (optly__flag_matches(arg, f)) {
      return f;
    }
  }

  return NULL;
}

static bool optly__flag_set_value(OptlyFlag *flag, char *value) {
  assert(flag);

  if (flag->type != OPTLY_TYPE_BOOL && !value) {
    OPTLY_LOG_FATAL("Flag --%s requires value", flag->fullname);
    exit(1);
  }

  flag->value.as_int64 = 0;
  char *end            = "";

  switch (flag->type) {
    case OPTLY_TYPE_CHAR:   flag->value.as_char = *value; break;
    case OPTLY_TYPE_STRING: flag->value.as_string = value; break;
    case OPTLY_TYPE_INT8:   flag->value.as_int8 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_INT16:  flag->value.as_int16 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_INT32:  flag->value.as_int32 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_INT64:  flag->value.as_int64 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_UINT8:  flag->value.as_uint8 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_UINT16: flag->value.as_uint16 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_UINT32: flag->value.as_uint32 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_UINT64: flag->value.as_uint64 = strtoll(value, &end, 10); break;
    case OPTLY_TYPE_FLOAT:  flag->value.as_float = strtod(value, &end); break;
    case OPTLY_TYPE_DOUBLE: flag->value.as_double = strtod(value, &end); break;
    case OPTLY_TYPE_BOOL:   flag->value.as_bool = true; break;
  }

  if (*end != '\0') {
    OPTLY_LOG_ERROR("Argument '%s' is not a number (%s)", flag->fullname, value);
    return false;
  }

  flag->present = true;
  return true;
}

/**
 * Parse flags from argv.
 */
static void optly__parse_flags(char ***argv_ptr, int *argc_ptr, OptlyFlag *flags) {
  char **argv = *argv_ptr;
  int    argc = *argc_ptr;

  if (!argv || argc <= 0 || !*argv) {
    return;
  }

  char *arg = *argv;

  // Detect batch short flags like `-abc`
  bool is_batch_short = (arg[0] == '-' && arg[1] != '-' && strlen(arg) > 2) && arg[2] != '=';

  if (is_batch_short) {
    if (strchr(arg, '=') != NULL) {
      *argv_ptr = argv;
      *argc_ptr = argc;
      return;
    }

    for (char *c = &arg[1]; *c; c++) {
      char sarg[3];
      snprintf(sarg, sizeof(sarg), "-%c", *c);

      OptlyFlag *flag = optly__find_flag(sarg, flags);

      if (!flag) {
#ifdef OPTLY_GEN_HELP_FLAG
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
          optly__help_flag.present = true;
          return;
        }
#endif

        OPTLY_LOG_WARN("Unknown short flag: %s", sarg);
        continue;
      }

      if (flag->type != OPTLY_TYPE_BOOL) {
        OPTLY_LOG_WARN("cannot batch non-boolean flags (invalid flag in %s)", sarg);
        continue;
      }

      flag->value.as_bool = true;
      flag->present       = true;
    }

    *argv_ptr = argv;
    *argc_ptr = argc;
    return;
  }

  // Handle single flag: long or short
  char *value = NULL;

  char *eq = strchr(arg, '=');
  char  tmp[OPTLY_FLAG_BUFFER_LENGTH];

  if (eq) {
    size_t len = strlen(arg);
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, arg, len);
    tmp[len] = '\0';

    char *eq2 = strchr(tmp, '=');
    *eq2      = '\0';
    arg       = tmp;
    value     = eq + 1;
  }

  OptlyFlag *flag = optly__find_flag(arg, flags);

  if (!flag) {
#ifdef OPTLY_GEN_HELP_FLAG
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      optly__help_flag.present = true;
      return;
    }
#endif

    OPTLY_LOG_WARN("Unknown flag: %s", arg);
    return;
  }

  flag->present = true;

  // If no value provided inline and next arg looks like a value (not a flag)
  if (!value && flag->type != OPTLY_TYPE_BOOL && argc > 1 && argv[1][0] != '-') {
    value = argv[1];
    SHIFT_ARG(argv, argc);
  }

  if (!optly__flag_set_value(flag, value)) {
    OPTLY_LOG_ERROR("Failed to set value for flag: %s\n", arg);
  }

  *argv_ptr = argv;
  *argc_ptr = argc;
}

/**
 * Parse a command from argv.
 */
static OptlyCommand *optly__parse_command(const char *arg, OptlyCommand *commands) {
  for (OptlyCommand *cmd = commands; !optly_is_command_null(cmd); cmd++) {
    if (strcmp(arg, cmd->name) == 0) {
      return cmd;
    }
  }

  return NULL;
}

static void optly__push_positional(OptlyCommand *cmd, char *value) {
  if (!cmd->positionals) return;
  size_t pos_count = 0;

  for (OptlyPositional *p = cmd->positionals; p->name; p++) {
    pos_count++;

    // Ensure at least 1 arg in optioanl positional
    size_t min = p->min == 0 ? 1 : p->min;

    if (p->count < min) {
      p->values[p->count++] = value;
      return;
    }
  }

  OptlyPositional *last_p         = &cmd->positionals[pos_count - 1];
  last_p->values[last_p->count++] = value;

  for (size_t i = pos_count - 1; i > 1; i--) {
    OptlyPositional *p      = &cmd->positionals[i];
    OptlyPositional *p_prev = &cmd->positionals[i - 1];

    if (p->count > p->max && p->max != 0) {
      p_prev->values[p_prev->count++] = p->values[0];

      for (size_t i = 0; i < p->count - 1; i++) {
        p->values[i] = p->values[i + 1];
      }

      p->count--;
    }
  }
}

static bool optly__validate_positionals(OptlyCommand *cmd) {
  if (!cmd->positionals) {
    return true;
  }

  bool ok = true;

  for (OptlyPositional *pos = cmd->positionals; pos->name; pos++) {
    if (pos->count < pos->min) {
      OPTLY_LOG_ERROR("Not enough values for positional '%s'", pos->name);
      ok = false;
    }

    if (pos->max != 0 && pos->count > pos->max) {
      OPTLY_LOG_ERROR("Too many values for positional '%s'", pos->name);
      ok = false;
    }
  }

  return ok;
}

inline OPTLYDEF bool optly_is_command(OptlyCommand *command, const char *name) {
  return command && strcmp(command->name, name) == 0;
}

const OptlyFlag *optly_get_flag(const OptlyFlag *flags, const char *name) {
  for (const OptlyFlag *flag = flags; !optly_is_flag_null(flag); flag++) {
    if (strcmp(flag->fullname, name) == 0) {
      return flag;
    }
  }

  return NULL;
}

inline OPTLYDEF bool optly_flag_value_bool(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_bool : false;
}

inline OPTLYDEF char optly_flag_value_char(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_char : '\0';
}

inline OPTLYDEF char *optly_flag_value_string(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_string : "";
}

inline OPTLYDEF int8_t optly_flag_value_int8(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_int8 : 0;
}

inline OPTLYDEF int16_t optly_flag_value_int16(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_int16 : 0;
}

inline OPTLYDEF int32_t optly_flag_value_int32(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_int32 : 0;
}

inline OPTLYDEF int64_t optly_flag_value_int64(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_int64 : 0;
}

inline OPTLYDEF uint8_t optly_flag_value_uint8(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_uint8 : 0;
}

inline OPTLYDEF uint16_t optly_flag_value_uint16(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_uint16 : 0;
}

inline OPTLYDEF uint32_t optly_flag_value_uint32(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_uint32 : 0;
}

inline OPTLYDEF uint64_t optly_flag_value_uint64(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_uint64 : 0;
}

inline OPTLYDEF float optly_flag_value_float(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_float : 0;
}

inline OPTLYDEF double optly_flag_value_double(const OptlyCommand *command, const char *name) {
  const OptlyFlag *flag = optly_get_flag(command->flags, name);
  return flag ? flag->value.as_double : 0;
}

inline OPTLYDEF OptlyPositional *optly_get_positional(OptlyCommand *command, const char *name) {
  for (OptlyPositional *p = command->positionals; p->name; p++) {
    if (strcmp(p->name, name) == 0) {
      return p;
    }
  }

  return NULL;
}

OPTLYDEF OptlyCommand *optly_parse_args(int argc, char *argv[], OptlyCommand *main_cmd) {
  assert(argc > 0);

  if (main_cmd->name == NULL) {
    main_cmd->name = argv[0];
  }

  OptlyCommand *current_cmd = main_cmd;

  bool positional_only            = false;
  bool all_required_flags_present = true;

  SHIFT_ARG(argv, argc);

  while (argc > 0) {
    char *arg = *argv;

    if (!arg) {
      break;
    }

    if (positional_only) {
      optly__push_positional(current_cmd, arg);
      SHIFT_ARG(argv, argc);
      continue;
    }

    if (arg[0] == '-') {
      if (arg[1] == '-' && arg[2] == '\0') {
        // `--` encountered
        positional_only = true;
      } else if (current_cmd->flags) {
        optly__parse_flags(&argv, &argc, current_cmd->flags);
      } else {
        optly__push_positional(current_cmd, arg);
      }
    } else {
      OptlyCommand *cmd = optly__parse_command(arg, current_cmd->commands);

#ifdef OPTLY_GEN_HELP_COMMAND
      if (strcmp(arg, "help") == 0) {
        SHIFT_ARG(argv, argc);

        if (argc == 0) {
          optly_usage(current_cmd);
          exit(0);
        }

        char         *target = *argv;
        OptlyCommand *cmd    = optly__parse_command(target, current_cmd->commands);

        if (!cmd) {
          OPTLY_LOG_ERROR("Unknown command: %s", target);
          exit(1);
        }

        optly_usage(cmd);
        exit(0);
      }
#endif

      if (!cmd) {
        if (current_cmd->positionals) {
          optly__push_positional(current_cmd, arg);
        } else {
          OPTLY_LOG_ERROR("Unknown command %s", arg);
        }

        SHIFT_ARG(argv, argc);
        continue;
      }

      for (OptlyFlag *flag = current_cmd->flags; !optly_is_flag_null(flag); flag++) {
        if (flag->required && !flag->present) {
          all_required_flags_present = false;
          OPTLY_LOG_ERROR("Required flag '--%s' is not present", flag->fullname);
        }
      }

      if (!optly__validate_positionals(current_cmd)) {
        all_required_flags_present = false;
      }

      bool infinite_found = false;

      for (OptlyPositional *pos = current_cmd->positionals; pos != NULL && pos->name != NULL; pos++) {
        if (pos->max == 0) {
          if (infinite_found) {
            OPTLY_LOG_FATAL("Positional '%s' allows infinite values, but another variadic positional already exists", pos->name);
            exit(1);
          }

          infinite_found = true;
        }
      }

      current_cmd->next_command = cmd;
      current_cmd               = current_cmd->next_command;
    }

#ifdef OPTLY_GET_HELP_FLAG
    if (optly__help_flag.present) {
      optly_usage(current_cmd);
      exit(0);
    }
#endif

    SHIFT_ARG(argv, argc);
  }

  for (OptlyFlag *flag = current_cmd->flags; !optly_is_flag_null(flag); flag++) {
    if (flag->required && !flag->present) {
      all_required_flags_present = false;
      OPTLY_LOG_ERROR("Required flag '--%s' is not present", flag->fullname);
    }
  }

  if (!optly__validate_positionals(current_cmd)) {
    all_required_flags_present = false;
  }

  if (!all_required_flags_present) {
    exit(1);
  }

  current_cmd->next_command = NULL;
  return main_cmd;
}

#endif  // OPTLY_IMPLEMENTATION

// TODO: Add auto `command --help|-h` and `help command`
// TODO: Should I print type of flag value in usage? @print_flag_type
// TODO: Usage and version string customisation (usage per command)
// TODO: Types for variadics?
// TODO: Support different kind of numbers (0xBABA, 0123)?
// TODO: Return state? errors? from optly_parse_args
// TODO: Come up with a way to automatically choose field for .value in optly_flag
// TODO: Enum type flag support

/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2026 Nikita Chukov nikita_chul@mail.ru
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
