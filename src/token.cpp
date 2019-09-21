#include "token.h"

#include <stdio.h>

const scopeid gc_unresolvedScopeid = -1;

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
    { "else",       TOKENK_Else },
	{ "for",		TOKENK_For },
	{ "while",		TOKENK_While },
	{ "break",		TOKENK_Break },
	{ "continue",	TOKENK_Continue },
	{ "return",		TOKENK_Return },
	{ "do",			TOKENK_Do },
	{ "bool",		TOKENK_Identifier },
	{ "byte",		TOKENK_Identifier },
	{ "int",		TOKENK_Identifier },
	{ "s16",		TOKENK_Identifier },
	{ "s32",		TOKENK_Identifier },
	{ "s64",		TOKENK_Identifier },
	{ "uint",		TOKENK_Identifier },
	{ "u16",		TOKENK_Identifier },
	{ "u32",		TOKENK_Identifier },
	{ "u64",		TOKENK_Identifier },
	{ "float",		TOKENK_Identifier },
	{ "f32",		TOKENK_Identifier },
	{ "f64",		TOKENK_Identifier },
	{ "struct",		TOKENK_Struct },
	{ "enum",		TOKENK_Enum },
    { "func",       TOKENK_Func },
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

void setIdent(Identifier * pIdentifier, Token * pToken, scopeid declScopeid)
{
	pIdentifier->pToken = pToken;
	pIdentifier->declScopeid = declScopeid;
	pIdentifier->hash = identHash(*pIdentifier);
}

void setIdentUnresolved(Identifier * pIdentifier, Token * pToken)
{
	pIdentifier->pToken = pToken;
	pIdentifier->declScopeid = gc_unresolvedScopeid;
}

u32 identHash(const Identifier & ident)
{
	// FNV-1a : http://www.isthe.com/chongo/tech/comp/fnv/

	const static u32 s_offsetBasis = 2166136261;
	const static u32 s_fnvPrime = 16777619;

	u32 result = s_offsetBasis;

	// Hash the scope identifier

	for (uint i = 0; i < sizeof(ident.declScopeid); i++)
	{
		u8 byte = *(reinterpret_cast<const u8*>(&ident.declScopeid) + i);
		result ^= byte;
		result *= s_fnvPrime;
	}

	// Hash the lexeme

	char * pChar = ident.pToken->lexeme;
	AssertInfo(pChar, "Identifier shouldn't be an empty string...");

	while (*pChar)
	{
		result ^= *pChar;
		result *= s_fnvPrime;
		pChar++;
	}

	return result;
}

u32 identHashPrecomputed(const Identifier & i)
{
	return i.hash;
}

bool identEq(const Identifier & i0, const Identifier & i1)
{
	// TODO: Can probably get rid of this first check as long as we make sure that we
	//	are only putting identifiers w/ tokens in the symbol table (which should be
	//	the case...)

	if (!i0.pToken || !i1.pToken)			return false;

	if (i0.hash != i1.hash)					return false;
	if (i0.declScopeid != i1.declScopeid)	return false;

	if (i0.pToken->lexeme == i1.pToken->lexeme)		return true;

	return (strcmp(i0.pToken->lexeme, i1.pToken->lexeme) == 0);
}
