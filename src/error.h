#pragma once

#include "als.h"

#include <stdarg.h>
#include <stdio.h>

inline void reportIceAndExit(const char * errFormat, ...)
{
    va_list arglist;
    va_start(arglist, errFormat);
    printf("\n!!![Internal compiler error]: ");
    vprintf(errFormat, arglist);
    printf("\n");
    va_end(arglist);
    fflush(stdout);

    Assert(false);

    exit(EXIT_FAILURE);
}
