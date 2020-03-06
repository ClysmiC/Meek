#include "als.h"

#include "ast.h"
#include "ast_print.h"
#include "error.h"
#include "parse.h"
#include "resolve_pass.h"
#include "scan.h"

#include <stdio.h>

int main()
{
	Defer(char c = getchar());		// Hack to see output at end

#if 1
	// Desktop setup

	char * filename = "W:/Meek/examples/test.meek";		// TODO: Read this in from command line
#else
	// Laptop setup

	char * filename = "C:/Users/Andrew/Desktop/clylang/test.meek";		// TODO: Read this in from command line
#endif

	int bufferSize = 1024 * 1024;										// TODO: Support files bigger than 1 mb. Maybe have scanner return a special value when its buffer is full and it will ask you to pass it a new buffer?

	char * buffer = new char[bufferSize];
	Defer(delete[] buffer);

	FILE * file = fopen(filename, "rb");
	Defer(if (file) fclose(file));

	int bytesRead = (int)(file ? fread(buffer, 0x1, bufferSize, file) : -1);

	if (bytesRead < 0)
	{
		fprintf(stderr, "Error opening file\n");
		return 1;
	}
	else if (bytesRead == bufferSize)
	{
		fprintf(stderr, "Files larger than %d bytes are not yet supported", bufferSize);
		return 1;
	}

	// NOTE: Lexeme buffer size is multiplied by 2 to handle worst case scenario where each byte is its own lexeme,
	//  which gets written into the buffer and then null terminated.

	int lexemeBufferSize = bytesRead * 2;
	char * lexemeBuffer = new char[(u64)bytesRead * 2];
	Defer(delete[] lexemeBuffer);

	Scanner scanner;
	if (!init(&scanner, buffer, bytesRead, lexemeBuffer, lexemeBufferSize))
	{
		reportIceAndExit("Failed to initialize scanner");
		return 1;
	}

	Parser parser;
	if (!init(&parser, &scanner))
	{
		reportIceAndExit("Failed to initialize parser");
		return 1;
	}

	bool success;
	AstNode * pAst = parseProgram(&parser, &success);

	if (!success)
	{
		// TODO: Still try to do semantic analysis on non-erroneous parts of the program so that we can report better errors?

		reportScanAndParseErrors(parser);
		return 1;
	}

	bool allTypesResolved = tryResolveAllPendingTypesIntoTypeTable(&parser);
	bool allFuncSymbolsResolved = tryResolvePendingFuncSymbolsAfterTypesResolved(&parser.symbTable);

	if (!allTypesResolved)
	{
		printf("Unable to resolve following type(s):\n");

		for (int i = 0; i < parser.typeTable.typesPendingResolution.cItem; i++)
		{
			Type * pType = parser.typeTable.typesPendingResolution[i].pType;

			if (pType->isFuncType)
			{
				printf("(skipping func type)\n");
			}
			else
			{
				printf("%s\n", pType->ident.pToken->lexeme);
			}
		}

		return 1;
	}

	if (!allFuncSymbolsResolved)
	{
		printf("Couldn't resolve all func symbols\n");
		return 1;
	}

	ResolvePass resolvePass;
	init(&resolvePass);
	resolvePass.pSymbTable = &parser.symbTable;
	resolvePass.pTypeTable = &parser.typeTable;
	doResolvePass(&resolvePass, pAst);

	printf("Resolve pass all done\n");

	printf("\n\n");
	debugPrintSymbolTable(parser.symbTable);

#if DEBUG && 1
	DebugPrintCtx debugPrintCtx;
	init(&debugPrintCtx.mpLevelSkip);
	debugPrintCtx.pTypeTable = &parser.typeTable;

	debugPrintAst(&debugPrintCtx, *pAst);


	printf("\n");
	if (parser.apErrorNodes.cItem > 0)
	{
		printf("Parse had error(s)!\n");
	}
	else
	{
		printf("No parse errors :)\n");
	}

#endif
}
