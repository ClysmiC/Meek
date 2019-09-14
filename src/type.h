#pragma once

#include "als.h"

#include "token.h"

// Forward declarations

struct AstNode;
struct ParseFuncType;


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

        // HMM: Do I need this level of indirection or can I just ember the
        //  ParseFuncType here (maybe anonymously?). That might make the union
        //  a bit more confusing to understand but the ParseType allocator would
        //  subsume the need for a ParseFuncType allocator!

		ParseFuncType * pParseFuncType;	// Valid if isFuncType
	};

	DynamicArray<ParseTypeModifier> aTypemods;
	bool isFuncType = false;
};

bool isTypeInferred(const ParseType & parseType);
bool isUnmodifiedType(const ParseType & parseType);



struct ParseFuncType
{
	DynamicArray<AstNode *> apParamVarDecls;
	DynamicArray<AstNode *> apReturnVarDecls;		// A.k.a. output params
};

// NOTE: Also used for return values since they have the same syntax as input params. Just think
//	of them as "output params"

// struct ParseParam
// {
// 	union
// 	{
// 		ParseType * pType;			// Value if !isDecl
// 		AstVarDeclStmt * pDecl;		// Value if isDecl
// 	};

// 	bool isDecl = false;
// };




// [5+6]int elevenInts;

// ^[12]int paInts;

// [12]^[3]^^^[12]int apInts;

// ^[15]func (int, float)(int) myFunc;

// Maybe optional arrows to help the reader make sense of something like this?
// [15]func (func ()(), float) -> (func( func(float)->(int) )->(int))
