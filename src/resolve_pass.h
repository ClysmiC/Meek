#pragma once

#include "als.h"
#include "symbol.h"

struct AstNode;
struct TypeTable;

struct ResolvePass
{
    SYMBSEQID lastSymbseqid = SYMBSEQID_Unset;      // TODO: USE THIS

	Stack<Scope> scopeStack;
	SymbolTable * pSymbTable;
    TypeTable * pTypeTable;

	DynamicArray<ScopedIdentifier> unresolvedIdents;

	bool hadError = false;
};

void init(ResolvePass * pPass);


inline void dispose(ResolvePass * pResolvePass)
{
	dispose(&pResolvePass->scopeStack);
	dispose(&pResolvePass->unresolvedIdents);
}

TYPID resolveExpr(ResolvePass * pPass, AstNode * pNode);
void resolveStmt(ResolvePass * pPass, AstNode * pNode);
void doResolvePass(ResolvePass * pPass, AstNode * pNode);
void reportUnresolvedIdentError(ResolvePass * pPass, ScopedIdentifier ident);
