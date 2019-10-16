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

scopeid resolveVarExpr(ResolvePass * pPass, AstNode * pNode);
scopeid resolveExpr(ResolvePass * pPass, AstNode * pNode);
void resolveStmt(ResolvePass * pPass, AstNode * pNode);
void doResolvePass(ResolvePass * pPass, AstNode * pNode);
void reportUnresolvedIdentError(ResolvePass * pPass, ScopedIdentifier ident);
