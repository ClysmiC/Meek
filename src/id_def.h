#pragma once

#include "als.h"

enum ASTID : u32
{
};

enum VARSEQID : u32
{
	VARSEQID_Zero,
	VARSEQID_Start,

	VARSEQID_Nil = static_cast<u32>(0xFF'FF'FF'FF),
};

enum SCOPEID : u32
{
	SCOPEID_BuiltIn,
	SCOPEID_Global,

	SCOPEID_UserDefinedStart,

	SCOPEID_Max = static_cast<u32>(0xFF'FF'FF'FF - 1),
	SCOPEID_Nil = static_cast<u32>(0xFF'FF'FF'FF)
};


// SYNC: Built in types should be inserted into the type table in the order that
//  results in the following typids!

enum TYPID : u32
{
	TYPID_Unresolved,
	TYPID_UnresolvedInferred,
	TYPID_UnresolvedHasCandidates,

	// Type checking errors

	TYPID_TypeError,
	TYPID_BubbleError,

	// Built-ins

	TYPID_Void,
	TYPID_Int,
	TYPID_Float,
	TYPID_Bool,
	TYPID_String,

	TYPID_UserDefinedStart,
	TYPID_ActualTypesStart = TYPID_Void
};

enum PENDINGTYPID : u32
{
	PENDINGTYPID_Nil = static_cast<u32>(0xFF'FF'FF'FF)
};
