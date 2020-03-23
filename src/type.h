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
	Type() {}

	union
	{
		struct UNonFuncTypeData
		{
			ScopedIdentifier ident;
		} nonFuncTypeData;

		struct UFuncTypeData
		{
			FuncType funcType;
		} funcTypeData;
	};

	DynamicArray<TypeModifier> aTypemods;
	bool isFuncType = false;
	bool isInferred = false;
};

void init(Type * pType, bool isFuncType);
//void initMove(Type * pType, Type * pTypeSrc);
void initCopy(Type * pType, const Type & typeSrc);
void dispose(Type * pType);

bool isTypeResolved(const Type & type);
bool isUnmodifiedType(const Type & type);
bool isPointerType(const Type & type);

bool isFuncTypeResolved(const FuncType & funcType);

inline bool isTypeResolved(TYPID typid)
{
	return typid >= TYPID_ActualTypesStart;
}

Lexeme getDealiasedTypeLexeme(const Lexeme & lexeme);

bool typeEq(const Type & t0, const Type & t1);
uint typeHash(const Type & t);

bool funcTypeEq(const FuncType & f0, const FuncType & f1);
uint funcTypeHash(const FuncType & f);

inline uint typeHashPtr(Type * const& pType)
{
	return typeHash(*pType);
}

inline bool typeEqPtr(Type * const& pType0, Type * const& pType1)
{
	return typeEq(*pType0, *pType1);
}

void init(FuncType * pFuncType);
// void initMove(FuncType * pFuncType, FuncType * pFuncTypeSrc);
void initCopy(FuncType * pFuncType, const FuncType & funcTypeSrc);
void dispose(FuncType * pFuncType);

bool areTypidListTypesFullyResolved(const DynamicArray<TYPID> & aTypid);

bool areVarDeclListTypesFullyResolved(const DynamicArray<AstNode *> & apVarDecls);
bool areVarDeclListTypesEq(const DynamicArray<AstNode *> & apVarDecls0, const DynamicArray<AstNode *> & apVarDecls1);

bool areTypidListTypesEq(const DynamicArray<TYPID> & aTypid0, const DynamicArray<TYPID> & aTypid1);

// bool areTypidListAndVarDeclListTypesEq(const DynamicArray<TYPID> & aTypid, const DynamicArray<AstNode *> & apVarDecls);

inline uint typidHash(const TYPID & typid)
{
	// HMM: Is identity hash function a bad idea?

	return typid;
}

inline bool typidEq(const TYPID & typid0, const TYPID & typid1)
{
	return typid0 == typid1;
}

struct TypeTable
{
	struct TypePendingResolve
	{
		// Info we need to resolve the type

		Type type;
		Scope * pScope;

		// Typid value to poke into corresponding AST node when we succeed resolving

		static const int s_cTypidUpdateOnResolveMax = 4;
		TYPID * apTypidUpdateOnResolve[s_cTypidUpdateOnResolveMax];
		int cPTypidUpdateOnResolve = 0;
	};

	DynamicArray<TypePendingResolve> typesPendingResolution;
	DynamicPoolAllocator<Type> typeAlloc;
	BiHashMap<TYPID, Type *> table;

	TYPID typidNext = TYPID_ActualTypesStart;
};

void init(TypeTable * pTable);
void init(TypeTable::TypePendingResolve * pTypePending, Scope * pScope, bool isFuncType);
void dispose(TypeTable::TypePendingResolve * pTypePending);

NULLABLE const Type * lookupType(const TypeTable & table, TYPID typid);
NULLABLE const FuncType * funcTypeFromDefnStmt(const TypeTable & typeTable, const AstFuncDefnStmt & defnStmt);

PENDINGTYPID registerPendingNonFuncType(
	TypeTable * pTable,
	Scope * pScope,
	Lexeme ident,
	const DynamicArray<TypeModifier> & aTypemod,
	NULLABLE TYPID * pTypidUpdateOnResolve=nullptr);

PENDINGTYPID registerPendingFuncType(
	TypeTable * pTable,
	Scope * pScope,
	const DynamicArray<TypeModifier> & aTypemod,
	const DynamicArray<PENDINGTYPID> & aPendingTypidParam,
	const DynamicArray<PENDINGTYPID> & aPendingTypidReturn,
	NULLABLE TYPID * pTypidUpdateOnResolve=nullptr);

void setPendingTypeUpdateOnResolvePtr(TypeTable * pTable, PENDINGTYPID pendingTypid, TYPID * pTypidUpdateOnResolve);

TYPID ensureInTypeTable(TypeTable * pTable, Type * pType, bool debugAssertIfAlreadyInTable=false);

bool tryResolveAllTypes(Parser * pParser);

TYPID typidFromLiteralk(LITERALK literalk);

bool canCoerce(TYPID typidFrom, TYPID typidTo);


#if DEBUG
void debugPrintType(const Type & type);
void debugPrintTypeTable(const TypeTable & typeTable);
#endif
