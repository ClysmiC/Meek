#include "token.h"

#include "error.h"

#include <stdio.h>

const StartEndIndices gc_startEndBubble(-1, -1);

const TOKENK g_aTokenkLiteral[] = {
	TOKENK_IntLiteral,
	TOKENK_FloatLiteral,
	TOKENK_BoolLiteral,
	TOKENK_StringLiteral
};
const int g_cTokenkLiteral = ArrayLen(g_aTokenkLiteral);

const TOKENK g_aTokenkUnopPre[] = {
	TOKENK_Plus,		// HMM: Is this worthwhile to have?
	TOKENK_Minus,
	TOKENK_Bang,
	TOKENK_Carat,		// Address-of
	// TOKENK_Greater,		// Dereference... I kinda like this but hope it doesn't lead to ambiguity! Also it needs ot be able to stack, i.e., >> could be confused with bit shift?
};
const int g_cTokenkUnopPre = ArrayLen(g_aTokenkUnopPre);

const ReservedWord g_aReservedWord[] = {
	{ "if",			TOKENK_If },
	{ "else",       TOKENK_Else },
	{ "for",		TOKENK_For },
	{ "while",		TOKENK_While },
	{ "break",		TOKENK_Break },
	{ "continue",	TOKENK_Continue },
	{ "return",		TOKENK_Return },
	{ "do",			TOKENK_Do },
	{ "print",		TOKENK_Print },
	{ "void",		TOKENK_Identifier },
	{ "bool",		TOKENK_Identifier },
	// { "byte",		TOKENK_Identifier },
	{ "int",		TOKENK_Identifier },
	{ "s8",			TOKENK_Identifier },
	{ "s16",		TOKENK_Identifier },
	{ "s32",		TOKENK_Identifier },
	{ "s64",		TOKENK_Identifier },
	{ "uint",		TOKENK_Identifier },
	{ "u8",			TOKENK_Identifier },
	{ "u16",		TOKENK_Identifier },
	{ "u32",		TOKENK_Identifier },
	{ "u64",		TOKENK_Identifier },
	{ "float",		TOKENK_Identifier },
	{ "f32",		TOKENK_Identifier },
	{ "f64",		TOKENK_Identifier },
	{ "string",		TOKENK_Identifier },
	{ "struct",		TOKENK_Struct },
	{ "enum",		TOKENK_Enum },
	{ "fn",         TOKENK_Fn },
	{ "true",		TOKENK_BoolLiteral },
	{ "false",		TOKENK_BoolLiteral },
};
const int g_cReservedWord = ArrayLen(g_aReservedWord);

const char * g_mpTokenkStrDisplay[] = {
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
	"'->'",                 // TOKENK_MinusGreater
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
	"'#and'",               // TOKENK_HashAnd
	"'#or'",                // TOKENK_HashOr
	"'#xor'",               // TOKENK_HashXor,
	"'if'",					// TOKENK_If
	"'else'",               // TOKENK_Else
	"'for'",				// TOKENK_For
	"'while'",				// TOKENK_While
	"'break'",				// TOKENK_Break
	"'continue'",			// TOKENK_Continue
	"'return'",				// TOKENK_Return
	"'do'",					// TOKENK_Do
	"'print'",				// TOKENK_Print
	"'struct'",				// TOKENK_Struct
	"'enum'",				// TOKENK_Enum
	"'func'",               // TOKENK_Func
	"<end of file>",		// TOKENK_Eof
};
StaticAssert(ArrayLen(g_mpTokenkStrDisplay) == TOKENK_Max);

const char * errMessageFromFerrtok(FERRTOK ferrtok)
{
	Assert(ferrtok);

	if (ferrtok == FERRTOK_Unspecified)
	{
		return "Unspecified error";
	}

	switch (ferrtok)
	{
		case FERRTOK_InvalidCharacter:
		{
			return "Invalid character";
		} break;

		case FERRTOK_IntLiteralNonBase10WithDecimal:
		{
			return "Numeric literal may only contain a decimal if it is in base-10";
		} break;

		case FERRTOK_IntLiteralNonBase10NoDigits:
		{
			return "Numeric literal with non-base-10 prefix must contain at least one digit";
		} break;

		case FERRTOK_NumberLiteralMultipleDecimals:
		{
			return "Numeric literal cannot contain more than one decimal";
		} break;

		case FERRTOK_MultilineString:
		{
			return "Multiline string literals are not permitted";
		} break;

		case FERRTOK_UnterminatedString:
		{
			return "Unterminated string literal";
		} break;

		case FERRTOK_UnterminatedBlockComment:
		{
			return "Unterminated block comment";
		} break;

		default:
		{
			reportIceAndExit("Unknown FERRTOK value %d", ferrtok);
			return "";
		} break;
	}
}

bool isReservedWord(StringView strv, NULLABLE TOKENK * poTokenk)
{
	// @Slow

	for (int iRw = 0; iRw < g_cReservedWord; iRw++)
	{
		const ReservedWord * pReservedWord = &g_aReservedWord[iRw];

		if (strv == pReservedWord->lexeme)
		{
			if (poTokenk)
			{
				*poTokenk = pReservedWord->tokenk;
			}

			return true;
		}
	}

	return false;
}
