#pragma once

#include "als.h"
#include "symbol.h"

struct AstNode;
struct Parser;
struct TypeTable;

struct ResolvePass
{

	SYMBSEQID lastSymbseqid = SYMBSEQID_Unset;      // TODO: USE THIS

	SCOPEID scopeidCur = SCOPEID_Global;
	TypeTable * pTypeTable;
	DynamicArray<Scope *> * pMpScopeidPScope;

	DynamicArray<ScopedIdentifier> unresolvedIdents;

	// FnCtx for making sure return values match expected types

	struct FnCtx
	{
		DynamicArray<TYPID> aTypidReturn;
	};

	Stack<FnCtx> fnCtxStack;

	int cNestedBreakable = 0;		// TODO: Support labeled break?

	bool hadError = false;
};

void init(ResolvePass * pPass, Parser * pParser);


//inline void dispose(ResolvePass * pResolvePass)
//{
//	dispose(&pResolvePass->scopeStack);
//	dispose(&pResolvePass->unresolvedIdents);
//}

void auditDuplicateSymbols(ResolvePass * pPass);

TYPID resolveExpr(ResolvePass * pPass, AstNode * pNode);
void resolveStmt(ResolvePass * pPass, AstNode * pNode);
void doResolvePass(ResolvePass * pPass, AstNode * pNode);
