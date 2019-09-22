#pragma once

#include "als.h"

struct AstNode;
struct AstVarDeclStmt;
struct AstFuncDefnStmt;
struct AstStructDefnStmt;
struct Token;

typedef s32 scopeid;
extern const scopeid gc_unresolvedScopeid;
extern const scopeid gc_globalAndBuiltinScopeid;

struct ResolvedIdentifier
{
	Token * pToken;
	scopeid declScopeid;

	// Cached

	u32 hash;
};

enum SYMBOLK
{
	SYMBOLK_Var,
	SYMBOLK_Func,
	SYMBOLK_Struct,
};

struct SymbolInfo
{
	ResolvedIdentifier * pIdentDeclfn;

	SYMBOLK symbolk;

	// HMM: Could make this union the first field and use type punning to save pointers
	//	back to the SymbolInfo

	union
	{
		AstVarDeclStmt * varDecl;		// SYMBOLK_Var
		AstFuncDefnStmt * funcDefn;		// SYMBOLK_Func
		AstStructDefnStmt * structDefn;	// SYMBOLK_Struct
	};
};

void setSymbolInfo(SymbolInfo * pSymbInfo, SYMBOLK symbolk, AstNode * pDefncl);
void setIdentResolved(ResolvedIdentifier * pIdentifier, Token * pToken, scopeid declScopeid);
void setIdentUnresolved(ResolvedIdentifier * pIdentifier, Token * pToken);

u32 identHash(const ResolvedIdentifier & ident);
u32 identHashPrecomputed(const ResolvedIdentifier & i);
bool identEq(const ResolvedIdentifier & i0, const ResolvedIdentifier & i1);

inline bool isResolved(const ResolvedIdentifier & i)
{
	return i.declScopeid != gc_unresolvedScopeid;
}
