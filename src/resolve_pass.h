#pragma once

#include "als.h"
#include "symbol.h"

struct AstNode;

// Resolve identifiers


struct ResolvePass
{
    symbseqid lastSymbseqid = 0;
	Stack<scopeid> scopeidStack;
	SymbolTable * pSymbTable;

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

void doResolvePass(ResolvePass * pPass, AstNode * pNode);
scopeid resolveVarExpr(ResolvePass * pPass, AstNode * pExpr);
void reportUnresolvedIdentError(ResolvePass * pPass, ScopedIdentifier ident);
