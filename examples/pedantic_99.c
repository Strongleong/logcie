#ifndef LOGCIE_PEDANTIC
#define LOGCIE_PEDANTIC
#endif

#define LOGCIE_IMPLEMENTATION
#include <logcie.h>

int main(void)
{
    LOGCIE_INFO("In pedantic mode logcie logs macros takes only one argument");
    LOGCIE_INFO_VA("But if you need printf functionality you can use '%s' macros", "_VA");
    LOGCIE_WARN_VA("There is _VA verson for every log level, (about %d)", Count_LOGCIE_LEVEL);

    return 0;
}
