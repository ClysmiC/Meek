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
		ResolvedIdentifier ident;	// !isFuncType
		FuncType * pFuncType;		// isFuncType
	};

	DynamicArray<TypeModifier> aTypemods;
	bool isFuncType = false;
};

bool isTypeInferred(const Type & type);
bool isUnmodifiedType(const Type & type);



struct FuncType
{
	DynamicArray<AstNode *> apParamVarDecls;
	DynamicArray<AstNode *> apReturnVarDecls;		// A.k.a. output params
};
