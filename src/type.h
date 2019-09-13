#pragma once

#include "als.h"

#include "token.h"

// Forward declarations

struct AstNode;



enum TYPEMODK : u8
{
	TYPEMODK_Array,
	TYPEMODK_Pointer
};

struct ParseTypeModifier
{
	TYPEMODK typemodk;
	AstNode * pSubscriptExpr = nullptr;		// Only valid if TYPEMODK_Array
};

struct ParseType
{
	union
	{
		Token * pType;					// Name of unnmodified type. Valid if !isFuncType
		ParseFuncType * pFuncType;		// Valid if isFuncType;
	};

	DynamicArray<ParseTypeModifier> aTypemods;
	bool isFuncType = false;
};

bool isTypeInferred(const ParseType & parseType);
bool isUnmodifiedType(const ParseType & parseType);



struct ParseFuncType
{
	DynamicArray<ParseParam> apParams;
	DynamicArray<ParseParam> apReturns;
};

// NOTE: Also used for return values since they have the same syntax as input params. Just think
//	of them as "output params"

struct ParseParam
{
	union
	{
		ParseType * pType;			// Value if !isDecl
		AstVarDeclStmt * pDecl;		// Value if isDecl
	};

	bool isDecl = false;
};

ParseType * getParseType(const ParseParam & param);



// [5+6]int elevenInts;

// ^[12]int paInts;

// [12]^[3]^^^[12]int apInts;

// ^[15]func (int, float)(int) myFunc;

// Maybe optional arrows to help the reader make sense of something like this?
// [15]func (func ()(), float) -> (func( func(float)->(int) )->(int))
