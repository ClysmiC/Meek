#pragma once

#include "als.h"

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
};

void init(MeekCtx * pMeekCtx, char * pText, uint textSize);