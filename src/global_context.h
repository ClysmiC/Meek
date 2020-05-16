#pragma once

#include "als.h"

#include "id_def.h"

struct AstDecorations;
struct AstNode;
struct Scanner;
struct Parser;
struct Scope;
struct TypeTable;

// NOTE (andrew) These are all pointers to globals, instead of embedding all the globals in a single struct. The extra
//	pointer reach through is annoying, but improves #include hygiene a lot.

struct MeekCtx
{
	Scanner * pScanner;
	Parser * pParser;
	TypeTable * pTypeTable;

	DynamicArray<Scope *> mpScopeidPScope;

	AstNode * pNodeRoot;
	AstDecorations * pAstDecs;

	// All functions

	DynamicArray<AstNode *> apFuncDefnAndLiteral;
	bool isMainAssignedFuncid;
	bool areFuncsSortedByFuncid;
	FUNCID funcidNext;

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
AstNode * funcNodeFromFuncid(MeekCtx * pCtx, FUNCID funcid);