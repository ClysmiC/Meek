#pragma once

#include "als.h"
#include "token.h"

// SYNC: typidFromLiteralk

enum LITERALK : u8
{
	LITERALK_Int,
	LITERALK_Float,
	LITERALK_Bool,
	LITERALK_String,

	LITERALK_Max,
	LITERALK_Min = 0,

	LITERALK_Nil = static_cast<u8>(-1)
};

LITERALK literalkFromTokenk(TOKENK tokenk);
