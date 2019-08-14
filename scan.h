#pragma once

#include "token.h"

enum SCANEXITK
{
    SCANEXITK_ReadAllBytes,
    SCANEXITK_ReadNullTerminator,

    SCANEXITK_Max,
    SCANEXITK_Nil = -1
};

enum SCANMATCHK
{
    SCANMATCHK_Consume,
    SCANMATCHK_Peek
};

struct Scanner
{
	// Init state

    char *      pText = nullptr;
    int		    textSize = 0;
    char *		pLexemeBuffer = nullptr;	// Must be as large as pText
    int			lexemeBufferSize = 0;

    // Scan state

    int         iText = 0;
    int			line = 1;
    int			column = 1;

    int         iTextTokenStart = 0;
    int			lineTokenStart = 1;
    int			columnTokenStart = 1;

    int         iToken = 0;					// Becomes tokens id
    
    int			cNestedBlockComment = 0;    /* this style of comment can nest */

    bool        hadError = false;

    // Lexeme buffer management

    int			iLexemeBuffer = 0;

    // Exit kind

    SCANEXITK   scanexitk = SCANEXITK_Nil;
};

// Public interface

bool init(Scanner * pScanner, char * pText, int textSize, char * pLexemeBuffer, int lexemeBufferSize);
TOKENK nextToken(Scanner * pScanner, Token * poToken);

// Internal

bool tryMatch(Scanner * pScanner, char expected, SCANMATCHK scanmatchk=SCANMATCHK_Consume);
bool tryMatch(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch=nullptr, SCANMATCHK scanmatchk=SCANMATCHK_Consume);
char consumeChar(Scanner * pScanner);

void onStartToken(Scanner * pScanner);
void writeCurrentLexemeIntoBuffer(Scanner * pScanner);
void makeToken(Scanner * pScanner, TOKENK tokenk, Token * poToken);
void makeTokenWithLexeme(Scanner * pScanner, TOKENK tokenk, char * lexeme, Token * poToken);
void makeErrorToken(Scanner * pScanner, ERRORTOKENK errortokenk, Token * poToken);
void makeEofToken(Scanner * pScanner, Token * poToken);

bool checkEndOfFile(Scanner * pScanner);

inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

inline bool isLetterOrUnderscore(char c)
{
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool isDigitOrLetterOrUnderscore(char c)
{
	return isDigit(c) || isLetterOrUnderscore(c);
}