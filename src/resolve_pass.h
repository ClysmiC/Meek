#pragma once

#include "als.h"
#include "symbol.h"

struct AstNode;
struct TypeTable;

struct ResolvePass
{
    symbseqid lastSymbseqid = 0;
	Stack<SCOPEID> scopeidStack;
	SymbolTable * pSymbTable;
    TypeTable * pTypeTable;

	DynamicArray<ScopedIdentifier> unresolvedIdents;

	bool hadError = false;
};

inline void init(ResolvePass * pPass)
{
	pPass->lastSymbseqid = 0;
	init(&pPass->scopeidStack);
	init(&pPass->unresolvedIdents);
	pPass->hadError = false;
}

inline void dispose(ResolvePass * pResolvePass)
{
	dispose(&pResolvePass->scopeidStack);
	dispose(&pResolvePass->unresolvedIdents);
}

TYPID resolveExpr(ResolvePass * pPass, AstNode * pNode);
void resolveStmt(ResolvePass * pPass, AstNode * pNode);
void doResolvePass(ResolvePass * pPass, AstNode * pNode);
void reportUnresolvedIdentError(ResolvePass * pPass, ScopedIdentifier ident);
