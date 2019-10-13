#pragma once

#include "als.h"
#include "symbol.h"
#include "token.h"

// Forward declarations

struct AstNode;
struct FuncType;


enum TYPEMODK : u8
{
	TYPEMODK_Array,
	TYPEMODK_Pointer
};

struct TypeModifier
{
	TYPEMODK typemodk;
	AstNode * pSubscriptExpr = nullptr;		// Only valid if TYPEMODK_Array
};

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
	DynamicArray<AstNode *> apParamVarDecls;
	DynamicArray<AstNode *> apReturnVarDecls;		// A.k.a. output params
};

bool funcTypeEq(const FuncType & f0, const FuncType & f1);
