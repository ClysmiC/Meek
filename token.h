#pragma once

enum TOKENK
{
    TOKENK_Eof,

    TOKENK_Identifier,
    TOKENK_Literal,     // TODO: Differentiate between int literal, float literal, string literal ?

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
    TOKENK_Quote,
    TOKENK_SingleQuote,
    TOKENK_Plus,
    TOKENK_Minus,
    TOKENK_Star,
    TOKENK_Slash,
    TOKENK_Equal,
    TOKENK_Bang,
    TOKENK_Lesser,
    TOKENK_Greater,

    TOKENK_PlusEqual,
    TOKENK_MinusEqual,
    TOKENK_StarEqual,
    TOKENK_SlashEqual,
    TOKENK_EqualEqual,
    TOKENK_BangEqual,
    TOKENK_LesserEqual,
    TOKENK_GreaterEqual,

    // Reserved words (control flow)

    TOKENK_If,
    TOKENK_For,
    TOKENK_While,
    TOKENK_Break,       // IDEA: a way to specify the "number" of loops we are breaking out of? break(2) for example? Not very friendly for refactoring though... but I also don't really like labels
    TOKENK_Continue,
    TOKENK_Return,

    // Reserved words (types)

    TOKENK_Bool,

    TOKENK_Byte,

    TOKENK_Int,
    TOKENK_S16,
    TOKENK_S32,
    TOKENK_S64,

    TOKENK_Uint,
    TOKENK_U16,
    TOKENK_U32,
    TOKENK_U64,

    TOKENK_Float,       // TODO: 16 bit float?
    TOKENK_F32,
    TOKENK_F64,

    TOKENK_Struct,
    TOKENK_Enum,
                        // TODO: union?

                        // TODO: char? string?

    TOKENK_Max,
    TOKENK_Nil = -1
};

struct Token
{
    int         id = 0;
    int         line = 0;
    int         column = 0;

    TOKENK      tokenk = TOKENK_Nil;

    char *      lexeme = nullptr;   // Only set for identifiers and literals
};

struct ReservedWord
{
    char *      lexeme = nullptr;
    TOKENK      tokenk = TOKENK_Nil;

    ReservedWord(char * lexeme, TOKENK tokenk) : lexeme(lexeme), tokenk(tokenk) { ; }
};

// TODO: use a dict or trie for this

ReservedWord g_reservedWords[];