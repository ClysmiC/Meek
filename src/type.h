#pragma once

#include "als.h"

#include "id_def.h"
#include "literal.h"
#include "symbol.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct Parser;
struct Type;

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

	// TODO: I really hate that we are hiding part of the AST away in this random-ass type modifier struct.
	//	I think I should restructure it so that there is a special AST group node called AstTypeDescriptionGrp.
	//	Then, instead of an array of modifiers we can just chain the AST nodes together.

	AstNode * pSubscriptExpr = nullptr;		// Only valid if TYPEMODK_Array
};

struct FuncType
{
	DynamicArray<TYPID> paramTypids;
	DynamicArray<TYPID> returnTypids;		// A.k.a. output params
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
void initCopy(Type * pType, const Type & typeSrc);
void dispose(Type * pType);

bool isTypeResolved(const Type & type);
bool isTypeInferred(const Type & type);
bool isUnmodifiedType(const Type & type);
bool isPointerType(const Type & type);

bool isFuncTypeResolved(const FuncType & funcType);

inline bool isTypeResolved(TYPID typid)
{
	return typid >= TYPID_ActualTypesStart;
}

bool typeEq(const Type & t0, const Type & t1);
uint typeHash(const Type & t);

bool funcTypeEq(const FuncType & f0, const FuncType & f1);
uint funcTypeHash(const FuncType & f);

void init(FuncType * pFuncType);
void initMove(FuncType * pFuncType, FuncType * pFuncTypeSrc);
void initCopy(FuncType * pFuncType, const FuncType & funcTypeSrc);
void dispose(FuncType * pFuncType);

bool areTypidListTypesFullyResolved(const DynamicArray<TYPID> & aTypid);

bool areVarDeclListTypesFullyResolved(const DynamicArray<AstNode *> & apVarDecls);
bool areVarDeclListTypesEq(const DynamicArray<AstNode *> & apVarDecls0, const DynamicArray<AstNode *> & apVarDecls1);

bool areTypidListAndVarDeclListTypesEq(const DynamicArray<TYPID> & aTypid, const DynamicArray<AstNode *> & apVarDecls);

inline uint typidHash(const TYPID & typid)
{
	// HMM: Is identity hash function a bad idea?

	return typid;
}

inline bool typidEq(const TYPID & typid0, const TYPID & typid1)
{
	return typid0 == typid1;
}


struct TypePendingResolve
{
	// Info we need to resolve the type

	Stack<Scope> scopeStack;
	Type * pType;

	// Typid value to poke into corresponding AST node when we succeed resolving

	TYPID * pTypidUpdateOnResolve;
};

// Life-cycle of a type
//	- During parse, types are allocated by a pool allocator and filled out w/ all known info.
//	- We store a pointer to it in the "pending resolution" list. After parsing,
//		we iterate over the list as many times as needed to insert all types pending resolution. Once resolved,
//		we copy them into table then dispose/release the original.

struct TypeTable
{
	// Parser * pParser;		// HACK: Need to access parser to release Types once they get resolved, since the originals are pool allocated from the parser
	BiHashMap<TYPID, Type> table;

	// Due to order independence of certain types of declarations, unresolved types are put in
	//	a "pending" list which gets resolved at a later time.

	DynamicArray<TypePendingResolve> typesPendingResolution;

	TYPID typidNext = TYPID_ActualTypesStart;
};

void init(TypeTable * pTable);
void insertBuiltInTypes(TypeTable * pTable);
const Type * lookupType(const TypeTable & table, TYPID typid);


TYPID ensureInTypeTable(TypeTable * pTable, const Type & type, bool debugAssertIfAlreadyInTable=false);

bool tryResolveType(Type * pType, const SymbolTable & symbolTable, const Stack<Scope> & scopeStack);

bool tryResolveAllTypes(Parser * pParser);

TYPID typidFromLiteralk(LITERALK literalk);


#if DEBUG
void debugPrintType(const Type& type);
void debugPrintTypeTable(const TypeTable& typeTable);
#endif
