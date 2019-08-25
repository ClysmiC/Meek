#include "token.h"

ReservedWord g_reservedWords[] = {
	{ "if",			TOKENK_If },
	{ "for",		TOKENK_For },
	{ "while",		TOKENK_While },
	{ "break",		TOKENK_Break },
	{ "continue",	TOKENK_Continue },
	{ "return",		TOKENK_Return },
	{ "bool",		TOKENK_Bool },
	{ "byte",		TOKENK_Byte },
	{ "int",		TOKENK_Int },
	{ "s16",		TOKENK_S16 },
	{ "s32",		TOKENK_S32 },
	{ "s64",		TOKENK_S64 },
	{ "uint",		TOKENK_Uint },
	{ "u16",		TOKENK_U16 },
	{ "u32",		TOKENK_U32 },
	{ "u64",		TOKENK_U64 },
	{ "float",		TOKENK_Float },
	{ "f32",		TOKENK_F32 },
	{ "f64",		TOKENK_F64 },
	{ "struct",		TOKENK_Struct },
	{ "enum",		TOKENK_Enum },
	{ "true",		TOKENK_BoolLiteral },
	{ "false",		TOKENK_BoolLiteral },
};

int g_reservedWordCount = ArrayLen(g_reservedWords);