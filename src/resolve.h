#pragma once

#include "als.h"
#include "ast.h"
#include "symbol.h"

struct Parser;
struct TypeTable;

struct ResolvePassCtx
{
	// TODO: make these globals

	TypeTable * pTypeTable;
	DynamicArray<Scope *> * pMpScopeidPScope;

	// Bookkeeping

	VARSEQID varseqidSeen;
	int cNestedBreakable = 0;		// TODO: Support labeled break?

	struct FnCtx
	{
		DynamicArray<TYPID> aTypidReturn;
	};

	Stack<FnCtx> fnCtxStack;
	Stack<SCOPEID> scopeidStack;
	DynamicArray<TYPID> aTypidChild;

	// Output

	DynamicArray<ScopedIdentifier> unresolvedIdents;
	bool hadError = false;
};

void init(ResolvePassCtx * pPass, Parser * pParser);
void pushAndAuditScopeid(ResolvePassCtx * pPass, SCOPEID scopeid);

// Tree walk

void visitResolvePreorder(AstNode * pNode, void * pContext);
void visitResolvePostorder(AstNode * pNode, void * pContext);
void visitResolveHook(AstNode * pNode, AWHK awhk, void * pContext);

void resolveExpr(ResolvePassCtx * pPass, AstNode * pNode);
void resolveStmt(ResolvePassCtx * pPass, AstNode * pNode);
void doResolvePass(ResolvePassCtx * pPass, AstNode * pNode);
