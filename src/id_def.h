#pragma once

#include "als.h"

typedef u32 symbseqid;

enum SCOPEID : u32
{
	SCOPEID_Unresolved,
	SCOPEID_BuiltIn,
	SCOPEID_Global,

	SCOPEID_UserDefinedStart,

	SCOPEID_Nil = -1
};


// SYNC: Built in types should be inserted into the type table in the order that
//  results in the following typids!

enum TYPID : u32
{
	TYPID_Unresolved,
	TYPID_UnresolvedInferred,
	TYPID_TypeError,

	// Built-ins

	TYPID_Int,
	TYPID_Float,
	TYPID_Bool,
	TYPID_String,

	TYPID_UserDefinedStart
};
