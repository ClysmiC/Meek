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

struct ScopedIdentifier
{
	Token * pToken;
	scopeid defnclScopeid;

	// Cached

	u32 hash;
};

void setIdent(ScopedIdentifier * pIdentifier, Token * pToken, scopeid declScopeid);
void setIdentNoScope(ScopedIdentifier * pIdentifier, Token * pToken);
void resolveIdentScope(ScopedIdentifier * pIdentifier, scopeid declScopeid);

u32 scopedIdentHash(const ScopedIdentifier & ident);
u32 scopedIdentHashPrecomputed(const ScopedIdentifier & ident);
bool scopedIdentEq(const ScopedIdentifier & i0, const ScopedIdentifier & i1);

bool isScopeSet(const ScopedIdentifier & ident)
{
    return ident.defnclScopeid != gc_unresolvedScopeid;
}

enum SYMBOLK
{
	SYMBOLK_Var,
	SYMBOLK_Func,
	SYMBOLK_Struct,

	// SYMBOLK_ErrorProxy	// "Fake" symbol that we put into table after unresolved ident error so that it only gets reported once
};

struct SymbolInfo
{
	ScopedIdentifier ident;		// Redundant w/ the key, but convenient to store here
	SYMBOLK symbolk;			// Redundant w/ the table type, but convenient to make this a union

	union
	{
		AstVarDeclStmt * varDecl;		// SYMBOLK_Var
		AstFuncDefnStmt * funcDefn;		// SYMBOLK_Func
		AstStructDefnStmt * structDefn;	// SYMBOLK_Struct
	};
};

struct SymbolTableEntry
{
	int sequenceId;		// Increases based on order inserted into symbol table
	SymbolInfo symbInfo;
};

void setSymbolInfo(SymbolInfo * pSymbInfo, const ScopedIdentifier & ident, SYMBOLK symbolk, AstNode * pDefncl);

bool isDeclarationOrderIndependent(const SymbolTableEntry & entry);
bool isDeclarationOrderIndependent(const SymbolInfo & info);
bool isDeclarationOrderIndependent(SYMBOLK symbolk);

struct SymbolTable
{
    HashMap<ScopedIdentifier, SymbolTableEntry> varTable;
	HashMap<ScopedIdentifier, SymbolTableEntry> structTable;
	HashMap<ScopedIdentifier, DynamicArray<SymbolTableEntry>> funcTable;

	DynamicArray<SymbolInfo> redefinedVars;
	DynamicArray<SymbolInfo> redefinedStructs;
	DynamicArray<SymbolInfo> redefinedFuncs;

    int sequenceIdNext = 0;
};

SymbolTableEntry * lookupVar(SymbolTable * pSymbTable, const ScopedIdentifier & ident);
SymbolTableEntry * lookupStruct(SymbolTable * pSymbTable, const ScopedIdentifier & ident);
DynamicArray<SymbolTableEntry> * lookupFunc(SymbolTable * pSymbTable, const ScopedIdentifier & ident);

void init(SymbolTable * pSymbTable);
void dispose(SymbolTable * pSymbTable);

bool tryInsert(SymbolTable * pSymbolTable, const ScopedIdentifier & ident, const SymbolInfo & symbInfo);
