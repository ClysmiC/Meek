#include "token.h"

TOKENK g_aTokenkLiteral[] = {
	TOKENK_IntLiteral,
	TOKENK_FloatLiteral,
	TOKENK_BoolLiteral,
	TOKENK_StringLiteral
};
int g_cTokenkLiteral = ArrayLen(g_aTokenkLiteral);

TOKENK g_aTokenkUnopPre[] = {
	TOKENK_Plus,		// HMM: Is this worthwhile to have?
	TOKENK_Minus,
	TOKENK_Bang,
	TOKENK_Carat,		// Address-of
	TOKENK_Greater,		// Dereference... I kinda like this but hope it doesn't lead to ambiguity! Also it needs ot be able to stack, i.e., >> could be confused with bit shift?
};
int g_cTokenkUnopPre = ArrayLen(g_aTokenkUnopPre);

// Not sure it is worth supporting this. If anything, maybe just make these pre-ops to make it easier to parse?

// TOKENK g_aTokenkUnopPost[] = {
// 	TOKENK_PlusPlus,
// 	TOKENK_MinusMinus,
// };

ReservedWord g_aReservedWord[] = {
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

int g_cReservedWord = ArrayLen(g_aReservedWord);

const char * g_mpTokenkDisplay[] = {
	"<error>",				// TOKENK_Error
	"identifier",			// TOKENK_Identifier
	"int literal",			// TOKENK_IntLiteral
	"float literal",		// TOKENK_FloatLiteral
	"bool literal",			// TOKENK_BoolLiteral
	"string literal",		// TOKENK_StringLiteral
	"'('",					// TOKENK_OpenParen
	"')'",					// TOKENK_CloseParen
	"'{'",					// TOKENK_OpenBrace
	"'}'",					// TOKENK_CloseBrace
	"'['",					// TOKENK_OpenBracket
	"']'",					// TOKENK_CloseBracket
	"'.'",					// TOKENK_Dot
	"','",					// TOKENK_Comma
	"';'",					// TOKENK_Semicolon
	"':'",					// TOKENK_Colon
	"''' (single quote)",	// TOKENK_SingleQuote
	"'+'",					// TOKENK_Plus
	"'-'",					// TOKENK_Minus
	"'*'",					// TOKENK_Star
	"'/'",					// TOKENK_Slash
	"'%'",					// TOKENK_Percent
	"'='",					// TOKENK_Equal
	"'!'",					// TOKENK_Bang
	"'<'",					// TOKENK_Lesser
	"'>'",					// TOKENK_Greater
	"'^'",					// TOKENK_Carat
	"'--'",					// TOKENK_MinusMinus
	"'++'",					// TOKENK_PlusPlus
	"'|'",					// TOKENK_Pipe
	"'|='",					// TOKENK_PipeEqual
	"'||'",					// TOKENK_PipePipe
	"'||='",				// TOKENK_PipePipeEqual
	"'&'",					// TOKENK_Amp
	"'&='",					// TOKENK_AmpEqual
	"'&&'",					// TOKENK_AmpAmp
	"'&&='",				// TOKENK_AmpAmpEqual
	"'+='",					// TOKENK_PlusEqual
	"'-='",					// TOKENK_MinusEqual
	"'*='",					// TOKENK_StarEqual
	"'/='",					// TOKENK_SlashEqual
	"'%='",					// TOKENK_PercentEqual
	"'=='",					// TOKENK_EqualEqual
	"'!='",					// TOKENK_BangEqual
	"'<='",					// TOKENK_LesserEqual
	"'>='",					// TOKENK_GreaterEqual
	"'if'",					// TOKENK_If
	"'for'",				// TOKENK_For
	"'while'",				// TOKENK_While
	"'break'",				// TOKENK_Break
	"'continue'",			// TOKENK_Continue
	"'return'",				// TOKENK_Return
	"'bool'",				// TOKENK_Bool
	"'byte'",				// TOKENK_Byte
	"'int'",				// TOKENK_Int
	"'s16'",				// TOKENK_S16
	"'s32'",				// TOKENK_S32
	"'s64'",				// TOKENK_S64
	"'uint'",				// TOKENK_Uint
	"'u16'",				// TOKENK_U16
	"'u32'",				// TOKENK_U32
	"'u64'",				// TOKENK_U64
	"'float'",				// TOKENK_Float
	"'f32'",				// TOKENK_F32
	"'f64'",				// TOKENK_F64
	"'struct'",				// TOKENK_Struct
	"'enum'",				// TOKENK_Enum
	"<end of file>",		// TOKENK_Eof
};
StaticAssert(ArrayLen(g_mpTokenkDisplay) == TOKENK_Max);
