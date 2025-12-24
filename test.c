#include <stdbool.h>
#include <string.h>

#define PRINTF_TYPECHECK(a, b)
#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

#define BUFFER_LEN 4096

static char buffer[BUFFER_LEN] = {0};

typedef enum ArgType {
    ARG_NONE,
    ARG_INT,
    ARG_DOUBLE,
    ARG_STR
} ArgType;

typedef struct {
    ArgType type;
    union {
        int i;
        double d;
        const char *s;
    } value;
} Arg;

typedef struct {
    const char *name;

    Logcie_LogLevel level;
    const char *msg;

    const char *module;
    Logcie_LogLevel sink_min_level;
    const char *fmt;

    const char *expected_substr;

    Arg arg;
} Logcie_TestCase;

void chrchr(char *string, char old, char new) {
    for (size_t i = 0; i < strlen(string); i++) {
        if (string[i] == old) {
            string[i] = new;
            return;
        }
    }
}

#define EMIT(level, tc) \
    do { \
        switch ((tc)->arg.type) { \
            case ARG_INT:    LOGCIE_##level((tc)->msg, (tc)->arg.value.i); break; \
            case ARG_DOUBLE: LOGCIE_##level((tc)->msg, (tc)->arg.value.d); break; \
            case ARG_STR:    LOGCIE_##level((tc)->msg, (tc)->arg.value.s); break; \
            case ARG_NONE:   LOGCIE_##level((tc)->msg); break; \
        } \
    } while(0)

static void emit_by_level(const Logcie_TestCase *tc) {
    switch (tc->level) {
        case LOGCIE_LEVEL_TRACE:   EMIT(TRACE, tc);   break;
        case LOGCIE_LEVEL_DEBUG:   EMIT(DEBUG, tc);   break;
        case LOGCIE_LEVEL_VERBOSE: EMIT(VERBOSE, tc); break;
        case LOGCIE_LEVEL_INFO:    EMIT(INFO, tc);    break;
        case LOGCIE_LEVEL_WARN:    EMIT(WARN, tc);    break;
        case LOGCIE_LEVEL_ERROR:   EMIT(ERROR, tc);   break;
        case LOGCIE_LEVEL_FATAL:   EMIT(FATAL, tc);   break;
        default: break;
    }
}

static const char *logcie_module = "test";

static bool run_test(const Logcie_TestCase *tc) {
    memset(buffer, '\0', BUFFER_LEN);
    FILE *tmp = tmpfile();
    if (!tmp) return 0;

    logcie_module = tc->module;

    Logcie_Sink sink = {
        .sink = tmp,
        .min_level = tc->sink_min_level,
        .fmt = tc->fmt,
        .formatter = logcie_printf_formatter
    };

    logcie_add_sink(&sink);

    emit_by_level(tc);

    rewind(tmp);
    fread(buffer, 1, sizeof(buffer) - 1, tmp);

    logcie_remove_sink(&sink);
    fclose(tmp);

    if (tc->expected_substr == NULL) {
        return buffer[0] == '\0';
    }


    return strstr(buffer, tc->expected_substr) != NULL;
}

static Logcie_TestCase tests[] = {
    {
        .name = "TRACE basic",
        .level = LOGCIE_LEVEL_TRACE,
        .msg = "trace msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "TRACE trace msg"
    },
    {
        .name = "DEBUG basic",
        .level = LOGCIE_LEVEL_DEBUG,
        .msg = "debug msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "DEBUG debug msg"
    },
    {
        .name = "VERBOSE basic",
        .level = LOGCIE_LEVEL_VERBOSE,
        .msg = "verbose msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "VERB verbose msg"
    },
    {
        .name = "INFO basic",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "info msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "INFO info msg"
    },
    {
        .name = "WARN basic",
        .level = LOGCIE_LEVEL_WARN,
        .msg = "warn msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "WARN warn msg"
    },
    {
        .name = "ERROR basic",
        .level = LOGCIE_LEVEL_ERROR,
        .msg = "error msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "ERROR error msg"
    },
    {
        .name = "FATAL basic",
        .level = LOGCIE_LEVEL_FATAL,
        .msg = "fatal msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "FATAL fatal msg"
    },
    {
        .name = "TRACE filtered by INFO",
        .level = LOGCIE_LEVEL_TRACE,
        .msg = "no show",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_INFO,
        .fmt = "$L $m",
        .expected_substr = ""
    },
    {
        .name = "INFO passes INFO filter",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "visible",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_INFO,
        .fmt = "$L $m",
        .expected_substr = "INFO visible"
    },
    {
        .name = "Lowercase level token",
        .level = LOGCIE_LEVEL_WARN,
        .msg = "warn msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$l $m",
        .expected_substr = "warn warn msg"
    },
    {
        .name = "Uppercase level token",
        .level = LOGCIE_LEVEL_WARN,
        .msg = "warn msg",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "WARN warn msg"
    },
    {
        .name = "Module token",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "hello",
        .module = "network",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$M $m",
        .expected_substr = "network hello"
    },
    {
        .name = "NULL module fallback",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "fallback",
        .module = NULL,
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$M $m",
        .expected_substr = "Logcie fallback"
    },
    {
        .name = "File token",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "file test",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$f $m",
        .expected_substr = "test.c"
    },
    {
        .name = "Line token",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "line test",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$x",
        .expected_substr = ""  /* any number is OK */
    },
    {
        .name = "Formatted message",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "value=%d",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$m",
        .expected_substr = "value=42",
        .arg = { .type = ARG_INT, .value.i = 42 },
    },
    {
        .name = "Literal dollar",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "money",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$$ $m",
        .expected_substr = "$ money"
    },
    {
        .name = "Color token present",
        .level = LOGCIE_LEVEL_ERROR,
        .msg = "colored",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$c$L$r $m",
        .expected_substr = "\x1b[31;20mERROR\x1b[0m colored"
    },
    {
        .name = "Padding after level",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "aligned",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L$<6 $m",
        .expected_substr = "INFO  aligned"
    },
    {
        .name = "Padding shorter level",
        .level = LOGCIE_LEVEL_WARN,
        .msg = "aligned",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L$<6 $m",
        .expected_substr = "WARN  aligned"
    },
    {
        .name = "Padding longer level no-op",
        .level = LOGCIE_LEVEL_ERROR,
        .msg = "aligned",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L$<5 $m",
        .expected_substr = "ERROR aligned"
    },
    {
        .name = "Bracketed level no padding",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "hello",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "[$L] $m",
        .expected_substr = "[INFO] hello"
    },
    {
        .name = "Long message",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "this is a very long log message used for stress testing",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $m",
        .expected_substr = "this is a very long log message"
    },
};

bool is_test_passed(Logcie_TestCase testcase, int ok) {
    return (testcase.expected_substr && ok) || (!testcase.expected_substr && !ok);
}

int main(void) {
    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        Logcie_TestCase test = tests[i];
        bool ok = run_test(&test);

        if (is_test_passed(test, ok)) {
            printf("[PASS] %s\n", test.name);
            passed++;
        } else {
            chrchr(buffer, '\n', '\0');
            printf("[FAIL] %s\n  expect: '%s'\n  actual: '%s'\n", test.name, test.expected_substr ? test.expected_substr : "(no output)", buffer);
        }
    }

    printf("\nResult: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
