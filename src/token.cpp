#include "token.h"

#include <stdio.h>

// TODO: should probably make these globals "const" for good measure.

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
	// TOKENK_Greater,		// Dereference... I kinda like this but hope it doesn't lead to ambiguity! Also it needs ot be able to stack, i.e., >> could be confused with bit shift?
};
int g_cTokenkUnopPre = ArrayLen(g_aTokenkUnopPre);

ReservedWord g_aReservedWord[] = {
	{ "if",			TOKENK_If },
    { "else",       TOKENK_Else },
	{ "for",		TOKENK_For },
	{ "while",		TOKENK_While },
	{ "break",		TOKENK_Break },
	{ "continue",	TOKENK_Continue },
	{ "return",		TOKENK_Return },
	{ "do",			TOKENK_Do },
	{ "void",		TOKENK_Identifier },
	{ "bool",		TOKENK_Identifier },
	// { "byte",		TOKENK_Identifier },
	{ "int",		TOKENK_Identifier },
	// { "s16",		TOKENK_Identifier },
	// { "s32",		TOKENK_Identifier },
	// { "s64",		TOKENK_Identifier },
	// { "uint",		TOKENK_Identifier },
	// { "u16",		TOKENK_Identifier },
	// { "u32",		TOKENK_Identifier },
	// { "u64",		TOKENK_Identifier },
	{ "float",		TOKENK_Identifier },
	// { "f32",		TOKENK_Identifier },
	// { "f64",		TOKENK_Identifier },
	{ "string",		TOKENK_Identifier },
	{ "struct",		TOKENK_Struct },
	{ "enum",		TOKENK_Enum },
    { "fn",         TOKENK_Fn },
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
	"'struct'",				// TOKENK_Struct
	"'enum'",				// TOKENK_Enum
    "'func'",               // TOKENK_Func
	"<end of file>",		// TOKENK_Eof
};
StaticAssert(ArrayLen(g_mpTokenkDisplay) == TOKENK_Max);

void errMessagesFromGrferrtok(GRFERRTOK grferrtok, DynamicArray<StringBox<256>> * poMessages)
{
    StringBox<256> str;

    if (grferrtok == FERRTOK_Unspecified)
    {
        sprintf(str.aBuffer, "Unspecified error");
        append(poMessages, str);
        return;
    }

    ForFlag(FERRTOK, ferrtok)
    {
        if (grferrtok & ferrtok)
        {
            switch (ferrtok)
            {
                case FERRTOK_InvalidCharacter:
                {
                    sprintf(str.aBuffer, "Invalid character");
                } break;

                case FERRTOK_IntLiteralNonBase10WithDecimal:
                {
                    sprintf(str.aBuffer, "Numeric literal may only contain a decimal if it is in base-10");
                } break;

                case FERRTOK_IntLiteralNonBase10NoDigits:
                {
                    sprintf(str.aBuffer, "Numeric literal with non-base-10 prefix must contain at least one digit");
                } break;

                case FERRTOK_NumberLiteralMultipleDecimals:
                {
                    sprintf(str.aBuffer, "Numeric literal cannot contain more than one decimal");
                } break;

                case FERRTOK_MultilineString:
                {
                    sprintf(str.aBuffer, "Multiline string literals are not permitted");
                } break;

                case FERRTOK_UnterminatedString:
                {
                    sprintf(str.aBuffer, "Unterminated string literal");
                } break;

                case FERRTOK_UnterminatedBlockComment:
                {
                    sprintf(str.aBuffer, "Unterminated block comment");
                } break;

                default:
                {
                    Assert(false);
                    sprintf(str.aBuffer, "!!! Internal Compiler Error: errTokenMessages failing on flag value %d", ferrtok);
                }
            }

            append(poMessages, str);
        }
    }
}
