#pragma once

#include "als.h"

struct AstNode;
struct AstVarDeclStmt;
struct AstFuncDefnStmt;
struct AstStructDefnStmt;
struct Token;

typedef s32 scopeid;
extern const scopeid gc_unresolvedScopeid;
extern const scopeid gc_builtInScopeid;
extern const scopeid gc_globalScopeid;

enum SCOPEK : s8
{
	SCOPEK_BuiltIn,
	SCOPEK_Global,
	SCOPEK_StructDefn,
	SCOPEK_CodeBlock,

	SCOPEK_Nil = -1
};

struct Scope
{
	scopeid id = gc_unresolvedScopeid;
	SCOPEK scopek = SCOPEK_Nil;
};

//struct ScopeStack
//{
//    scopeid scopeidNext = 0;
//    Stack<Scope> stack;
//};
//
//void init(ScopeStack * pScopeStack);
//void pushScope(ScopeStack * pScopeStack, SCOPEK scopek);
//Scope peekScope(ScopeStack * pScopeStack);
//Scope popScope(ScopeStack * pScopeStack);

struct ResolvedIdentifier
{
	Token * pToken;
	scopeid declScopeid;

	// Cached

	u32 hash;
};

void setIdentResolved(ResolvedIdentifier * pIdentifier, Token * pToken, scopeid declScopeid);
void setIdentUnresolved(ResolvedIdentifier * pIdentifier, Token * pToken);

u32 identHash(const ResolvedIdentifier & ident);
u32 identHashPrecomputed(const ResolvedIdentifier & i);
bool identEq(const ResolvedIdentifier & i0, const ResolvedIdentifier & i1);

inline bool isResolved(const ResolvedIdentifier & i)
{
	return i.declScopeid != gc_unresolvedScopeid;
}



enum SYMBOLK
{
	SYMBOLK_Var,
	SYMBOLK_Func,
	SYMBOLK_Struct,
};

struct SymbolInfo
{
	ResolvedIdentifier identDefncl;
	SYMBOLK symbolk;

	// HMM: Could make this union the first field and use type punning to save pointers
	//	back to the SymbolInfo

	union
	{
		AstVarDeclStmt * varDecl;		// SYMBOLK_Var
		AstFuncDefnStmt * funcDefn;		// SYMBOLK_Func
		AstStructDefnStmt * structDefn;	// SYMBOLK_Struct
	};

    // Order inserted into the symbol table. Used during resolution to figure out if symbol is accessible at a particular point.
    //  Maintained by the symbol table insertion routine.

    int tableOrder;
};

void setSymbolInfo(SymbolInfo * pSymbInfo, const ResolvedIdentifier & ident, SYMBOLK symbolk, AstNode * pDefncl);



struct SymbolTable
{
    HashMap<ResolvedIdentifier, SymbolInfo> table;
	DynamicArray<ResolvedIdentifier> redefinedIdents;
    int orderNext = 0;
};

void init(SymbolTable * pSymbTable);

bool tryInsert(SymbolTable * pSymbolTable, ResolvedIdentifier ident, SymbolInfo symbInfo);
