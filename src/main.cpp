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

	AstNode * rootNode = nullptr;
	{
		printfmt("Parsing %s...\n", filename);

		bool success;
		rootNode = parseProgram(ctx.parser, &success);

		if (!success)
		{
			// TODO: Still try to do semantic analysis on non-erroneous parts of the program so that we can report better errors?

			reportScanAndParseErrors(*ctx.parser);
			return 1;
		}

		print("Done\n");
		println();
	}

	ctx.rootNode = rootNode;

#if DEBUG
	{
		for (int iFunc = 0; iFunc < ctx.functions.cItem; iFunc++)
		{
			Assert(funcid(*ctx.functions[iFunc]) == FuncId(iFunc));
		}
	}
#endif

	print("Running resolve pass...\n");
	if (!tryResolveAllTypes(ctx.typeTable))
	{
		print("Unable to resolve some types\n");
		return 1;
	}

	computeScopedVariableOffsets(&ctx, ctx.parser->pScopeGlobal);

	// TODO (andrew) Probably just eagerly insert func names into the symbol table like we do for others, and generate types pending resolution
	//	that will poke in the typid's of the args. Then, this function could be a simple audit to make sure that there are no redefined funcs.

	//if (!tryResolvePendingFuncSymbolsAfterTypesResolved(&parser.symbTable))
	//{
	//	print("Couldn't resolve all func symbols\n");
	//	return 1;
	//}


	ResolvePass resolvePass;
	init(&resolvePass, &ctx);
	doResolvePass(&resolvePass, rootNode);

	print("Done\n");
	println();

	print("Compiling bytecode...\n");

	BytecodeBuilder bytecodeBuilder;
	init(&bytecodeBuilder, &ctx);

	compileBytecode(&bytecodeBuilder);

	print("Done\n");
	println();

#if 0
	disassemble(bytecodeBuilder.bytecodeProgram);
#else

	if (ctx.mainFuncid != FuncId::Nil)
	{
		print("Running interpreter...\n");

		Interpreter interp;
		init(&interp, &ctx);

		interpret(
			&interp,
			bytecodeBuilder.bytecodeProgram,
			bytecodeBuilder.bytecodeProgram.bytecodeFuncs[(int)ctx.mainFuncid].iByte0);

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
