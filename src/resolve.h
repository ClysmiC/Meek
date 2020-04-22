#pragma once

// TODO: Rename this file/pass since it does a lot more than resolve names/types now

#include "als.h"
#include "ast.h"
#include "symbol.h"

struct Parser;
struct TypeTable;

struct ResolvePass
{
	MeekCtx * pCtx;

	// Bookkeeping

	VARSEQID varseqidSeen;
	int cNestedBreakable = 0;		// TODO: Support labeled break?

	struct FnCtx
	{
		DynamicArray<TYPID> aTypidReturn;
	};

	Stack<FnCtx> fnCtxStack;
	Stack<SCOPEID> scopeidStack;

	// Output

	DynamicArray<ScopedIdentifier> unresolvedIdents;
	bool hadError = false;
};

void init(ResolvePass * pPass, MeekCtx * pCtx);
void pushAndProcessScope(ResolvePass * pPass, SCOPEID scopeid);

// Tree walk

bool visitResolvePreorder(AstNode * pNode, void * pPass_);
void visitResolvePostorder(AstNode * pNode, void * pPass_);
void visitResolveHook(AstNode * pNode, AWHK awhk, void * pPass_);

void resolveExpr(ResolvePass * pPass, AstNode * pNode);
void resolveStmt(ResolvePass * pPass, AstNode * pNode);
void doResolvePass(ResolvePass * pPass, AstNode * pNode);
