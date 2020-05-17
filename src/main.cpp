#include "als.h"

#include "ast.h"
#include "ast_print.h"
#include "bytecode.h"
#include "error.h"
#include "global_context.h"
#include "interp.h"
#include "parse.h"
#include "print.h"
#include "resolve.h"
#include "scan.h"
#include "symbol.h"

#include <stdio.h>

int main()
{
	// TODO: Read file in from command line

#if 1
	// Desktop setup

	// char * filename = "W:/Meek/examples/test.meek";
	char * filename = "W:/Meek/examples/simple.meek";
#else
	// Laptop setup

	char * filename = "C:/Users/Andrew/Desktop/clylang/test.meek";		// TODO: Read this in from command line
#endif

	// TODO: Support files bigger than 1 mb. Maybe have scanner return a special value when its buffer is full and it will ask you to pass it a new buffer?

	int bufferSize = 1024 * 1024;

	char * buffer = new char[bufferSize];
	Defer(delete[] buffer);

	FILE * file = fopen(filename, "rb");
	Defer(if (file) fclose(file));

	int bytesRead = (int)(file ? fread(buffer, 0x1, bufferSize, file) : -1);

	if (bytesRead < 0)
	{
		// TODO: print to stderr

		print("Error opening file\n");
		return 1;
	}
	else if (bytesRead == bufferSize)
	{
		// TOOD: print to stderr

		printfmt("Files larger than %d bytes are not yet supported", bufferSize);
		return 1;
	}

	MeekCtx ctx;
	init(&ctx, buffer, bytesRead);

	AstNode * pNodeRoot = nullptr;
	{
		printfmt("Parsing %s...\n", filename);

		bool success;
		pNodeRoot = parseProgram(ctx.pParser, &success);

		if (!success)
		{
			// TODO: Still try to do semantic analysis on non-erroneous parts of the program so that we can report better errors?

			reportScanAndParseErrors(*ctx.pParser);
			return 1;
		}

		print("Done\n");
		println();
	}

	ctx.pNodeRoot = pNodeRoot;

#if DEBUG
	{
		for (int iFunc = 0; iFunc < ctx.apFuncDefnAndLiteral.cItem; iFunc++)
		{
			Assert(funcid(*ctx.apFuncDefnAndLiteral[iFunc]) == FUNCID(iFunc));
		}
	}
#endif

	print("Running resolve pass...\n");
	if (!tryResolveAllTypes(ctx.pTypeTable))
	{
		print("Unable to resolve some types\n");
		return 1;
	}

	computeScopedVariableOffsets(&ctx, ctx.pParser->pScopeGlobal);

	// TODO (andrew) Probably just eagerly insert func names into the symbol table like we do for others, and generate types pending resolution
	//	that will poke in the typid's of the args. Then, this function could be a simple audit to make sure that there are no redefined funcs.

	//if (!tryResolvePendingFuncSymbolsAfterTypesResolved(&parser.symbTable))
	//{
	//	print("Couldn't resolve all func symbols\n");
	//	return 1;
	//}


	ResolvePass resolvePass;
	init(&resolvePass, &ctx);
	doResolvePass(&resolvePass, pNodeRoot);

	print("Done\n");
	println();

	print("Compiling bytecode...\n");

	BytecodeBuilder bytecodeBuilder;
	init(&bytecodeBuilder, &ctx);

	compileBytecode(&bytecodeBuilder);

	print("Done\n");
	println();

#if 1
	for (int iFunc = 0; iFunc < bytecodeBuilder.aBytecodeFunc.cItem; iFunc++)
	{
		disassemble(bytecodeBuilder.aBytecodeFunc[iFunc]);
		println();
	}
#else

	if (ctx.funcidMain != FUNCID_Nil)
	{
		print("Running interpreter...\n");

		Interpreter interp;
		init(&interp, &ctx);

		interpret(&interp, bytecodeBuilder.aBytecodeFunc[ctx.funcidMain]);

		print("Done\n");
		println();
	}
	else
	{
		print("No main function. Not running interpreter.");
		println();
	}
#endif


	// DebugPrintCtx dpc;
	// init(&dpc, &ctx);
	// debugPrintAst(&dpc, *pNodeRoot);
}
