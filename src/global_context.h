#pragma once

#include "als.h"

#include "id_def.h"

struct AstDecorations;
struct AstNode;
struct Scanner;
struct Parser;
struct Scope;
struct TypeTable;

struct MeekCtx
{
	Scanner * scanner;
	Parser * parser;
	TypeTable * typeTable;

	// Indexed by ScopeId
	DynamicArray<Scope *> scopes;

	AstNode * rootNode;
	AstDecorations * astDecorations;

	// All functions

	DynamicArray<AstNode *> functions;
	FuncId mainFuncid;

#ifdef _WIN64
	static const int s_cBitTargetWord = 64;
#elif WIN32
	static const int s_cBitTargetWord = 32;
#else
	StaticAssertNotCompiled;
#endif

	static const int s_cBytePtr = (s_cBitTargetWord + 7) / 8;
};

void init(MeekCtx * pMeekCtx, char * pText, uint textSize);
AstNode * funcNodeFromFuncid(MeekCtx * pCtx, FuncId funcid);
