#pragma once

#include "als.h"

// Stupidly simple printing routines. Since printf isn't extensible and cout is C++ madness.

void print(const char * pStr);
void print(int i);
void print(float f);
void print(StringView stringView);
void println();
void printfmt(const char * pStrFormat, ...);
void vprintfmt(const char * pStrFormat, va_list arg);