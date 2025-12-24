#include <stdbool.h>
#include <string.h>

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

typedef struct {
    const char *name;

    Logcie_LogLevel level;
    const char *msg;

    const char *module;
    Logcie_LogLevel sink_min_level;
    const char *fmt;

    const char *expected_substr;
} Logcie_TestCase;


static void emit_by_level(Logcie_LogLevel level, const char *msg) {
    switch (level) {
        case LOGCIE_LEVEL_TRACE:   LOGCIE_TRACE("%s",   msg); break;
        case LOGCIE_LEVEL_DEBUG:   LOGCIE_DEBUG("%s",   msg); break;
        case LOGCIE_LEVEL_VERBOSE: LOGCIE_VERBOSE("%s", msg); break;
        case LOGCIE_LEVEL_INFO:    LOGCIE_INFO("%s",    msg); break;
        case LOGCIE_LEVEL_WARN:    LOGCIE_WARN("%s",    msg); break;
        case LOGCIE_LEVEL_ERROR:   LOGCIE_ERROR("%s",   msg); break;
        case LOGCIE_LEVEL_FATAL:   LOGCIE_FATAL("%s",   msg); break;
        default: break;
    }
}

static const char *logcie_module = "test";
static char buffer[4096] = {0};

static bool run_test(const Logcie_TestCase *tc) {

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

    emit_by_level(tc->level, tc->msg);

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
        .name = "INFO basic message",
        .level = LOGCIE_LEVEL_INFO,
        .msg = "hello",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_TRACE,
        .fmt = "$L $M $m",
        .expected_substr = "INFO core hello"
    },
    {
        .name = "DEBUG filtered out",
        .level = LOGCIE_LEVEL_DEBUG,
        .msg = "nope",
        .module = "core",
        .sink_min_level = LOGCIE_LEVEL_INFO,
        .fmt = "$L $m",
        .expected_substr = NULL
    },
    {
        .name = "ERROR passes filter",
        .level = LOGCIE_LEVEL_ERROR,
        .msg = "boom",
        .module = "net",
        .sink_min_level = LOGCIE_LEVEL_WARN,
        .fmt = "$L $m",
        .expected_substr = "ERROR boom"
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
            printf("[FAIL] %s (expected: '%s', actual: '%s')\n", test.name, test.expected_substr ? test.expected_substr : "(no output)", buffer);
        }
    }

    printf("\nResult: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
