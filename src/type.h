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
	Token * pType;		// Unmodified type
	DynamicArray<ParseTypeModifier> aTypemods;
};

bool isTypeInferred(const ParseType & parseType);
bool isUnmodifiedType(const ParseType & parseType);


// [5+6]int elevenInts;

// ^[12]int paInts;

// [12]^[3]^^^[12]int apInts;
