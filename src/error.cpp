#include "error.h"

#include "global_context.h"
#include "parse.h"
#include "print.h"
#include "scan.h"
#include "token.h"

#include <stdarg.h>

// TODO: print filename
// TODO: print to stderr

void reportScanError(const Scanner & scanner, const Token & tokenErr, const char * errFormat, ...)
{

	printfmt("[Error: test.meek, line %d]\n", lineFromI(scanner, tokenErr.startEnd.iStart));
	va_list arglist;
	va_start(arglist, errFormat);
	vprintfmt(errFormat, arglist);
	println();
	println();
	va_end(arglist);
}

void reportParseError(const Parser & parser, const AstNode & node, const char * errFormat, ...)
{
	MeekCtx * pCtx = parser.pCtx;

	auto startEnd = getStartEnd(*pCtx->pAstDecs, node.astid);

	printfmt("[Error: test.meek, line %d]\n", lineFromI(*pCtx->pScanner, startEnd.iStart));

	va_list arglist;
	va_start(arglist, errFormat);
	vprintfmt(errFormat, arglist);
	println();
	println();
	va_end(arglist);
}

void reportIceAndExit(const char * errFormat, ...)
{
	print("[Internal compiler error]:\n");

	va_list arglist;
	va_start(arglist, errFormat);
	vprintfmt(errFormat, arglist);
	println();
	println();
	va_end(arglist);

	Assert(false);

	exit(EXIT_FAILURE);
}
