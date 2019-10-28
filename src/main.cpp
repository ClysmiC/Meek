#include "als.h"

#include "ast.h"
#include "ast_print.h"
#include "scan.h"
#include "parse.h"
#include "resolve_pass.h"

#include <stdio.h>

int main()
{
    Defer(getchar());   // Hack to see output at end

#if 1
	// Desktop setup

	char * filename = "C:/Users/Andrew/Desktop/lang/lang/test.cly";		// TODO: Read this in from command line
#else
	// Laptop setup

	char * filename = "C:/Users/Andrew/Desktop/clylang/test.cly";		// TODO: Read this in from command line
#endif

	int bufferSize = 1024 * 1024;										// TODO: Support files bigger than 1 mb. Maybe have scanner return a special value when its buffer is full and it will ask you to pass it a new one

	char * buffer = new char[bufferSize];
	Defer(delete[] buffer);

	FILE * file = fopen(filename, "rb");
	Defer(if (file) fclose(file));

	int bytesRead = file ? fread(buffer, 0x1, bufferSize, file) : -1;

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
	char * lexemeBuffer = new char[bytesRead * 2];
	Defer(delete[] lexemeBuffer);

	Scanner scanner;
	if (!init(&scanner, buffer, bytesRead, lexemeBuffer, lexemeBufferSize))
	{
		// TODO: distinguish this as internal compiler error

		fprintf(stderr, "Failed to initialize scanner");
		return 1;
	}

    Parser parser;
    if (!init(&parser, &scanner))
    {
        // TODO: distinguish this as internal compiler error

        fprintf(stderr, "Failed to initialize parser");
        return 1;
    }

    bool success;
    AstNode * pAst = parseProgram(&parser, &success);

    if (!success)
    {
        printf("Exiting with parse error(s)...");
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
#if DEBUG && 0
    /*DebugPrintCtx debugPrintCtx;
    init(&debugPrintCtx.mpLevelSkip);
    debugPrintCtx.pTypeTable = &parser.typeTable;

    debugPrintAst(&debugPrintCtx, *pAst);*/


    printf("\n");
    if (parser.hadError)
    {
        printf("Parse had error(s)!\n");
    }
    else
    {
        printf("No parse errors :)\n");
    }

#endif
}
