#include "error.h"

#include "parse.h"
#include "scan.h"
#include "token.h"

void reportScanError(const Scanner & scanner, const Token & tokenErr, const char * errFormat, ...)
{
    // TODO: filename...

	fprintf(stderr, "[Error: test.meek, line %d]\n", lineFromI(scanner, tokenErr.startEnd.iStart));
	//fprintf(stderr, "\t>");

    va_list arglist;
    va_start(arglist, errFormat);
    vfprintf(stderr, errFormat, arglist);
	fprintf(stderr, "\n\n");
    va_end(arglist);
}

void reportParseError(const Parser & parser, const AstNode & node, const char * errFormat, ...)
{
	auto startEnd = getStartEnd(parser.astDecs, node.astid);

	fprintf(stderr, "[Error: test.meek, line %d]\n", lineFromI(*parser.pScanner, startEnd.iStart));
	//fprintf(stderr, "\t>");

    va_list arglist;
    va_start(arglist, errFormat);
    vfprintf(stderr, errFormat, arglist);
	fprintf(stderr, "\n\n");
    va_end(arglist);
}

void reportIceAndExit(const char * errFormat, ...)
{
    fprintf(stderr, "[Internal compiler error]:\n");
	//fprintf(stderr, "\t>");

    va_list arglist;
    va_start(arglist, errFormat);
    vfprintf(stderr, errFormat, arglist);
	fprintf(stderr, "\n\n");
    va_end(arglist);
    fflush(stderr);

    Assert(false);

    exit(EXIT_FAILURE);
}
