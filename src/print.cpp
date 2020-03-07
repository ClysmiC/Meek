#include "print.h"

#include <stdarg.h>
#include <stdio.h>

void print(const char * pStr)
{
	printf("%s", pStr);
}

void print(int i)
{
	printf("%d", i);
}

void print(float f)
{
	// TODO: formatting?

	printf("%f", f);
}

void print(StringView stringView)
{
	for (int iCh = 0; iCh < stringView.cCh; iCh++)
	{
		putchar(stringView.pCh[iCh]);
	}
}

void println()
{
	// TODO: CRLF on windows?

	printf("\n");
}

void printfmt(const char * pStrFormat, ...)
{
	va_list arglist;
	va_start(arglist, pStrFormat);
	vprintf(pStrFormat, arglist);
	va_end(arglist);
}

void vprintfmt(const char * pStrFormat, va_list arg)
{
	vprintf(pStrFormat, arg);
}
