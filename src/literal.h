#pragma once

#include "als.h"

// SYNC: typidFromLiteralk

enum LITERALK : u8
{
	LITERALK_Int,
	LITERALK_Float,
	LITERALK_Bool,
		LITERALK_String,

		LITERALK_Max,
		LITERALK_Min = 0,
};
