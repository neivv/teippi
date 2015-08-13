#ifdef DEBUG
#include "windows_wrap.h"
#include <stdio.h>
#include "types.h"

void AssertionFailure(const char *file, int line, const char *pred)
{
    if (!IsDebuggerPresent())
    {
        const char *format = "Assertion failure:\n%s:%d, %s\nAttach debugger?";
        static char buf[512];
        snprintf(buf, sizeof buf, format, file, line, pred);
        MessageBoxA(0, buf, "Hui", 0);
    }
    INT3();
}
#endif
