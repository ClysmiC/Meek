#pragma once

#include "als.h"
#include "symbol.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct Parser;
struct Type;

typedef u32 typid;

extern const typid gc_typidUnresolved;
extern const typid gc_typidUnresolvedInferred;

enum TYPEMODK : u8
{
	TYPEMODK_Array,
	TYPEMODK_Pointer
};

struct TypeModifier
{
	// TODO: I want different values here when parsing and after typechecking.
	//	Namely, while parsing I want to store the expressions inside subscripts,
	//	and after typechecking those expressions should all be resolved to ints.

	TYPEMODK typemodk;
	AstNode * pSubscriptExpr = nullptr;		// Only valid if TYPEMODK_Array
};

struct FuncType
{
    DynamicArray<typid> paramTypids;
    DynamicArray<typid> returnTypids;		// A.k.a. output params
};

struct Type
{
	union
	{
		ScopedIdentifier ident;		// !isFuncType
		FuncType funcType;			// isFuncType
	};

	DynamicArray<TypeModifier> aTypemods;
	bool isFuncType = false;
};

void init(Type * pType, bool isFuncType);
void initMove(Type * pType, Type * pTypeSrc);
void dispose(Type * pType);

bool isTypeResolved(const Type & type);
bool isTypeInferred(const Type & type);
bool isUnmodifiedType(const Type & type);

bool isFuncTypeResolved(const FuncType & funcType);

inline bool isTypeResolved(typid typid)
{
    return typid != gc_typidUnresolved && typid != gc_typidUnresolvedInferred;
}

bool typeEq(const Type & t0, const Type & t1);
uint typeHash(const Type & t);

bool funcTypeEq(const FuncType & f0, const FuncType & f1);
uint funcTypeHash(const FuncType & f);

void init(FuncType * pFuncType);
void initMove(FuncType * pFuncType, FuncType * pFuncTypeSrc);
void dispose(FuncType * pFuncType);

bool areVarDeclListTypesFullyResolved(const DynamicArray<AstNode *> & apVarDecls);
bool areVarDeclListTypesEq(const DynamicArray<AstNode *> & apVarDecls0, const DynamicArray<AstNode *> & apVarDecls1);

// Life-cycle of a type
//	- During parse, types are allocated by a pool allocator and filled out w/ all known info.
//	- If type can be fully resolved during parse, we ensure that a copy is in the table (copying it in if needed) and then
//		dispose/release the original.
//	- If it can't be fully resolved, we store a pointer to it in the "pending resolution" list. After parsing,
//		we iterate over the list as many times as needed to insert all types pending resolution. Once resolved,
//		we do the same thing as types that get resolved during the parse (copy into table then dispose/release original)

inline uint typidHash(const typid & typid)
{
	// HMM: Is identity hash function a bad idea?

	return typid;
}

inline bool typidEq(const typid & typid0, const typid & typid1)
{
	return typid0 == typid1;
}

struct TypePendingResolution
{
	Stack<Scope> scopeStack;
	Type * pType;
	typid * pTypidUpdateWhenResolved;
};

struct TypeTable
{
	// Parser * pParser;		// HACK: Need to access parser to release Types once they get resolved, since the originals are pool allocated from the parser
	BiHashMap<typid, Type> table;

	// Due to order independence of certain types of declarations, unresolved types are put in
	//	a "pending" list which gets resolved at a later time.

	DynamicArray<TypePendingResolution> typesPendingResolution;

    typid typidNext = gc_typidUnresolvedInferred + 1;
};

void init(TypeTable * pTable);
const Type * lookupType(TypeTable * pTable, typid typid);

// NOTE: This function releases pType
typid ensureInTypeTable(TypeTable * pTable, const Type & type, bool debugAssertIfAlreadyInTable=false);

bool tryResolveType(Type * pType, const SymbolTable & symbolTable, const Stack<Scope> & scopeStack);
typid resolveIntoTypeTableOrSetPending(
	Parser * pParser,
	Type * pType,
	TypePendingResolution ** ppoTypePendingResolution);

bool tryResolveAllPendingTypesIntoTypeTable(Parser * pParser);
