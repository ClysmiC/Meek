#pragma once

#include "als.h"

enum ASTID : u32
{
	ASTID_Nil = static_cast<u32>(0xFF'FF'FF'FF)
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

enum class TypeId : u32
{
	Nil = 0,

	UnresolvedInferred,
	UnresolvedHasCandidates,

	// Type checking errors

	TypeError,
	BubbleError,

	// Built-ins

	Void,
	S8,
	S16,
	S32,
	S64,
	U8,
	U16,
	U32,
	U64,
	F32,
	F64,
	Bool,
	String,

	mFirstUserDefined,

	Unresolved = Nil,

	mFirstResolved = Void,
	mFirstInt = S8,
	mLastInt = U64,
	mFirstFloat = F32,
	mLastFloat = F64,
};

enum class FuncId : u32
{
	Nil = 0,
};

enum class PendingTypeId : u32
{
	Nil = 0,

	mFirstValid = 1
};
