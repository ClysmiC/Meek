#pragma once

#include "als.h"

// SYNC: with g_mpTokenkDisplay
// SYNC: min/max values at bottom of enum

enum TOKENK
{
    TOKENK_Error,

    TOKENK_Identifier,
    TOKENK_IntLiteral,
    TOKENK_FloatLiteral,
    TOKENK_BoolLiteral,
    TOKENK_StringLiteral,		// TODO: Provide ways to specify more specific kinds of literals? Like s8 or u64?

    // Punctuation symbols

    TOKENK_OpenParen,
    TOKENK_CloseParen,
    TOKENK_OpenBrace,
    TOKENK_CloseBrace,
    TOKENK_OpenBracket,
    TOKENK_CloseBracket,
    TOKENK_Dot,
    TOKENK_Comma,
    TOKENK_Semicolon,
    TOKENK_Colon,
    TOKENK_SingleQuote,
    TOKENK_Plus,
    TOKENK_Minus,
    TOKENK_Star,
    TOKENK_Slash,
    TOKENK_Percent,
    TOKENK_Equal,
    TOKENK_Bang,
    TOKENK_Lesser,
    TOKENK_Greater,
    TOKENK_Carat,

    TOKENK_MinusGreater,

	TOKENK_MinusMinus,
	TOKENK_PlusPlus,

	TOKENK_Pipe,
	TOKENK_PipeEqual,
	TOKENK_PipePipe,
	TOKENK_PipePipeEqual,

	TOKENK_Amp,
	TOKENK_AmpEqual,
	TOKENK_AmpAmp,
	TOKENK_AmpAmpEqual,

	TOKENK_PlusEqual,
	TOKENK_MinusEqual,
	TOKENK_StarEqual,
	TOKENK_SlashEqual,
	TOKENK_PercentEqual,

	TOKENK_EqualEqual,
	TOKENK_BangEqual,
	TOKENK_LesserEqual,
	TOKENK_GreaterEqual,

    // #words

    TOKENK_HashAnd,
    TOKENK_HashOr,
    TOKENK_HashXor,

	// Reserved words (control flow)

	TOKENK_If,
    TOKENK_Else,
	TOKENK_For,
	TOKENK_While,
	TOKENK_Break,		// IDEA: a way to specify the "number" of loops we are breaking out of? break(2) for example? Not very friendly for refactoring though... but I also don't really like labels
	TOKENK_Continue,
	TOKENK_Return,
	TOKENK_Do,

	TOKENK_Struct,
	TOKENK_Enum,
    TOKENK_Fn,
						// TODO: union?
						// TODO: char? string? mstring? (i.e., mutable string... would mainly just be a dynamic array of bytes assuming we have a dynamic array built in type!)

	TOKENK_Eof,

	TOKENK_Max,
	TOKENK_Nil = -1,

	TOKENK_LiteralMin = TOKENK_IntLiteral,
	TOKENK_LiteralMax = TOKENK_StringLiteral + 1,

	/*TOKENK_ReservedWordBuiltInTypeMin = TOKENK_Bool,
	TOKENK_ReservedWordBuiltInTypeMax = TOKENK_F64 + 1*/
};


// SYNC: with ferrtokToDisplay(..)

enum FERRTOK : u32
{
	FERRTOK_Unspecified							= 1 << 0,

	FERRTOK_InvalidCharacter					= 1 << 1,

	FERRTOK_IntLiteralNonBase10WithDecimal		= 1 << 2,
	FERRTOK_IntLiteralNonBase10NoDigits			= 1 << 3,

	FERRTOK_NumberLiteralMultipleDecimals		= 1 << 4,

	FERRTOK_MultilineString 					= 1 << 5,
	FERRTOK_UnterminatedString					= 1 << 6,

	FERRTOK_UnterminatedBlockComment			= 1 << 7,

    FERRTOK_UnknownHashToken                    = 1 << 8,

	// TODO: Report these kinds of errors at semantic analysis

	// FERRTOK_IntLiteralHexMixedCase
	// FERRTOK_NumberLiteralOutOfRange

	FERRTOK_DummyValue,
    FERRTOK_LowestFlag = 1,
	FERRTOK_HighestFlag = FERRTOK_DummyValue - 1,

	GRFERRTOK_None = 0
};
typedef u32 GRFERRTOK;

void errMessagesFromGrferrtok(GRFERRTOK grferrtok, DynamicArray<StringBox<256>> * poMessages);

enum INTLITERALK
{
	INTLITERALK_Binary,
	INTLITERALK_Octal,
	INTLITERALK_Decimal,
	INTLITERALK_Hexadecimal
};

struct Token
{
	int			id = 0;
	int			line = 0;       // Technically the "start" line if there are any tokens that can span multiple lines
	int			column = 0;

	TOKENK		tokenk = TOKENK_Nil;
    GRFERRTOK   grferrtok = GRFERRTOK_None;      // TOKENK_Error

    // TODO: store lexeme length? I *do* make sure that they are null-terminated so
    //  I can always just strlen them but that is pretty lame.

	char *		lexeme = nullptr;
};

struct ReservedWord
{
	char *		lexeme = nullptr;
	TOKENK		tokenk = TOKENK_Nil;

	ReservedWord(char * lexeme, TOKENK tokenk) : lexeme(lexeme), tokenk(tokenk) { ; }
};

inline bool isLiteral(TOKENK tokenk)
{
	return tokenk >= TOKENK_LiteralMin && tokenk < TOKENK_LiteralMax;
}

inline void nillify(Token * poToken)
{
	poToken->id = 0;
	poToken->tokenk = TOKENK_Nil;
}

// TODO: use a dict or trie for reserved words
// TODO: Probably worth making these const correct.

extern ReservedWord g_aReservedWord[];
extern int g_cReservedWord;

extern TOKENK g_aTokenkLiteral[];
extern int g_cTokenkLiteral;

extern TOKENK g_aTokenkUnopPre[];
extern int g_cTokenkUnopPre;

extern const char * g_mpTokenkDisplay[];
