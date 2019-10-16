#pragma once

#include "als.h"
#include "symbol.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct FuncType;

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

// Type information available during/after parse, and before any types have
//	been resolved!

struct Type
{
	union
	{
		ScopedIdentifier ident;		// !isFuncType
		FuncType * pFuncType;		// isFuncType
	};

	DynamicArray<TypeModifier> aTypemods;
	bool isFuncType = false;
};

bool typeEq(const Type & t0, const Type & t1);
bool isTypeInferred(const Type & type);
bool isUnmodifiedType(const Type & type);

struct FuncType
{
	DynamicArray<Type *> apParamType;
	DynamicArray<Type *> apReturnType;		// A.k.a. output params
};

void init(FuncType * pFuncType);
void dispose(FuncType * pFuncType);
bool funcTypeEq(const FuncType & f0, const FuncType & f1);
