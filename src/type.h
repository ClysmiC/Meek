#pragma once

#include "als.h"

#include "id_def.h"
#include "literal.h"
#include "symbol.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct MeekCtx;
struct Type;

enum TYPEMODK : s8
{
	TYPEMODK_Array,
	TYPEMODK_Pointer,

	TYPEMODK_Nil = -1
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


enum TYPEK
{
	TYPEK_Named,
	TYPEK_Func,
	TYPEK_Mod
};

struct Type
{
	Type() {}

	union
	{
		struct UNamedTypeData
		{
			ScopedIdentifier ident;
		} namedTypeData;

		struct UFuncTypeData
		{
			FuncType funcType;
		} funcTypeData;

		struct UModTypeData
		{
			TypeModifier typemod;
			TYPID typidModified;
		} modTypeData;
	};

	TYPEK typek;

	// TODO: Split this out into TypeIdentInfo and TypeMetadata?
	//	Then we can use the former while resolving names without
	//	having to lug around this unset type info struct.

	struct ComputedInfo
	{
		static const u32 s_unset = -1;

		u32 size;			// Bytes
		u32 alignment;		// Bytes
	};

	ComputedInfo info;
	bool isInferred = false;
};

void init(Type * pType, TYPEK typek);
void initCopy(Type * pType, const Type & typeSrc);
void dispose(Type * pType);

bool isTypeResolved(const Type & type);
bool isFuncTypeResolved(const FuncType & funcType);
bool isTypeResolved(TYPID typid);
bool isPointerType(const Type & type);
bool isArrayType(const Type & type);

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
	MeekCtx * pCtx;

	struct TypePendingResolve
	{
		// Info we need to resolve the type

		Type type;
		Scope * pScope;

		// Typid value to poke into corresponding AST node when we succeed resolving

		static const int s_cTypidUpdateOnResolveMax = 4;
		TYPID * apTypidUpdateOnResolve[s_cTypidUpdateOnResolveMax];
		int cPTypidUpdateOnResolve = 0;

		// Are we resolving a type that then has a modifier applied to it? If so, we
		//	need to update our typid in the pendingtype that has the modifier.

		PENDINGTYPID pendingTypidModifiedBy = PENDINGTYPID_Nil;
	};

	DynamicArray<TypePendingResolve> typesPendingResolution;
	DynamicPoolAllocator<Type> typeAlloc;
	BiHashMap<TYPID, Type *> table;

	TYPID typidNext = TYPID_ActualTypesStart;
};

void init(TypeTable * pTable, MeekCtx * pCtx);
void init(TypeTable::TypePendingResolve * pTypePending, Scope * pScope, TYPEK typek);
void dispose(TypeTable::TypePendingResolve * pTypePending);

NULLABLE const Type * lookupType(const TypeTable & table, TYPID typid);
void setTypeInfo(const TypeTable & table, TYPID typid, const Type::ComputedInfo & typeInfo);

NULLABLE const FuncType * funcTypeFromDefnStmt(const TypeTable & typeTable, const AstFuncDefnStmt & defnStmt);

PENDINGTYPID registerPendingNamedType(
	TypeTable * pTable,
	Scope * pScope,
	Lexeme ident,
	NULLABLE TYPID * pTypidUpdateOnResolve=nullptr);

PENDINGTYPID registerPendingFuncType(
	TypeTable * pTable,
	Scope * pScope,
	const DynamicArray<PENDINGTYPID> & aPendingTypidParam,
	const DynamicArray<PENDINGTYPID> & aPendingTypidReturn,
	NULLABLE TYPID * pTypidUpdateOnResolve=nullptr);

PENDINGTYPID registerPendingModType(
	TypeTable * pTable,
	Scope * pScope,
	TypeModifier typemod,
	PENDINGTYPID pendingTypidModified,
	NULLABLE TYPID * pTypidUpdateOnResolve=nullptr);

void setPendingTypeUpdateOnResolvePtr(TypeTable * pTable, PENDINGTYPID pendingTypid, TYPID * pTypidUpdateOnResolve);

struct EnsureInTypeTableResult
{
	TYPID typid = TYPID_Unresolved;
	bool typeInfoComputed = false;
};
EnsureInTypeTableResult ensureInTypeTable(TypeTable * pTable, Type * pType, bool debugAssertIfAlreadyInTable=false);

Type::ComputedInfo tryComputeTypeInfoAndSetMemberOffsets(
	const MeekCtx & ctx,
	SCOPEID scopeid,
	const DynamicArray<AstNode *> & apVarDeclStmt,
	bool includeEndPadding = true);
bool tryComputeTypeInfo(const TypeTable & typeTable, TYPID typid);
bool tryResolveAllTypes(TypeTable * pTable);

TYPID typidFromLiteralk(LITERALK literalk);

bool canCoerce(TYPID typidFrom, TYPID typidTo);

bool isAnyInt(TYPID typid);
bool isAnyFloat(TYPID typid);

#if DEBUG
// void debugPrintType(const Type & type);
// void debugPrintTypeTable(const TypeTable & typeTable);
#endif
