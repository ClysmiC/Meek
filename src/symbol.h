#pragma once

#include "als.h"

struct AstNode;
struct AstVarDeclStmt;
struct AstFuncDefnStmt;
struct AstStructDefnStmt;
struct Token;
struct Type;

typedef u32 scopeid;
extern const scopeid gc_unresolvedScopeid;
extern const scopeid gc_builtInScopeid;
extern const scopeid gc_globalScopeid;

typedef u32 symbseqid;
extern const symbseqid gc_unsetSymbseqid;

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

inline bool isScopeSet(const ScopedIdentifier & ident)
{
    return ident.defnclScopeid != gc_unresolvedScopeid;
}

enum SYMBOLK
{
	SYMBOLK_Var,
	SYMBOLK_Func,
	SYMBOLK_Struct,

    SYMBOLK_BuiltInType,

	// SYMBOLK_ErrorProxy	// "Fake" symbol that we put into table after unresolved ident error so that it only gets reported once
};

struct SymbolInfo
{
	ScopedIdentifier ident;		// Redundant w/ the key, but convenient to store here
	SYMBOLK symbolk;			// Redundant w/ the table type, but convenient to make this a union

	union
	{
		AstVarDeclStmt * pVarDeclStmt;		    // SYMBOLK_Var
		AstFuncDefnStmt * pFuncDefnStmt;		// SYMBOLK_Func
		AstStructDefnStmt * pStructDefnStmt;	// SYMBOLK_Struct
	};
};

void setSymbolInfo(SymbolInfo * pSymbInfo, const ScopedIdentifier & ident, SYMBOLK symbolk, AstNode * pDefncl);

bool isDeclarationOrderIndependent(const SymbolInfo & info);
bool isDeclarationOrderIndependent(SYMBOLK symbolk);

struct SymbolTable
{
    HashMap<ScopedIdentifier, SymbolInfo> varTable;
	HashMap<ScopedIdentifier, SymbolInfo> typeTable;
	HashMap<ScopedIdentifier, DynamicArray<SymbolInfo>> funcTable;

    DynamicArray<SymbolInfo> funcSymbolsPendingTypeResolution;

	DynamicArray<SymbolInfo> redefinedVars;
	DynamicArray<SymbolInfo> redefinedTypes;
	DynamicArray<SymbolInfo> redefinedFuncs;

    int sequenceIdNext = 0;
};

SymbolInfo * lookupVar(SymbolTable * pSymbTable, const ScopedIdentifier & ident);
SymbolInfo * lookupType(SymbolTable * pSymbTable, const ScopedIdentifier & ident);
DynamicArray<SymbolInfo> * lookupFunc(SymbolTable * pSymbTable, const ScopedIdentifier & ident);

void insertBuiltInSymbols(SymbolTable * pSymbolTable);

void init(SymbolTable * pSymbTable);
void dispose(SymbolTable * pSymbTable);

bool tryInsert(SymbolTable * pSymbolTable, const ScopedIdentifier & ident, const SymbolInfo & symbInfo);

#if DEBUG
void debugPrintType(const Type & type);
void debugPrintSymbolTable(const SymbolTable & symbTable);
#endif
