#pragma once

#include "als.h"

enum ASTID : u32
{
};

enum SYMBSEQID : u32
{
    SYMBSEQID_Unset,

    SYMBSEQID_SetStart
};

enum SCOPEID : u32
{
	SCOPEID_Unresolved,
	SCOPEID_BuiltIn,
	SCOPEID_Global,

	SCOPEID_UserDefinedStart,

	SCOPEID_Nil = static_cast<u32>(-1)
};


// SYNC: Built in types should be inserted into the type table in the order that
//  results in the following typids!

enum TYPID : u32
{
	TYPID_Unresolved,
	TYPID_UnresolvedInferred,

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
