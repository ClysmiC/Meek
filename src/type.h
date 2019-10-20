#pragma once

#include "als.h"
#include "symbol.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct Type;

typedef s32 typid;

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
    DynamicArray<Type *> apParamType;
    DynamicArray<Type *> apReturnType;		// A.k.a. output params
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

bool isTypeFullyResolved(const Type & type);
bool isTypeInferred(const Type & type);
bool isUnmodifiedType(const Type & type);

bool typeEq(const Type & t0, const Type & t1);
uint typeHash(const Type & t);

bool funcTypeEq(const FuncType & f0, const FuncType & f1);
uint funcTypeHash(const FuncType & f);

void init(FuncType * pFuncType);
void initMove(FuncType * pFuncType, FuncType * pFuncTypeSrc);
void dispose(FuncType * pFuncType);

bool areVarDeclListTypesEq(const DynamicArray<AstNode *> & apVarDecls0, const DynamicArray<AstNode *> & apVarDecls1);

struct TypeTable
{
	BiHashMap<typid, Type> table;
};
