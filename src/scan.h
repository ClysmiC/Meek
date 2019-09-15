#pragma once

#include "als.h"

#include "token.h"

enum SCANEXITK
{
	SCANEXITK_ReadAllBytes,
	SCANEXITK_ReadNullTerminator,

	SCANEXITK_Max,
	SCANEXITK_Nil = -1
};

struct Scanner
{
	// Init state

	char *		pText = nullptr;
	uint		textSize = 0;
	char *		pLexemeBuffer = nullptr;		// Must be as large as pText
	uint		lexemeBufferSize = 0;

	// Scan state

	uint		iText = 0;
	uint		line = 1;
	uint		column = 1;

	uint		iTextTokenStart = 0;
	uint		lineTokenStart = 1;
	uint		columnTokenStart = 1;

	uint		iToken = 0;						// Becomes tokens id

	int			cNestedBlockComment = 0;		/* this style of comment can nest */

	uint        currentIntLiteralBase;

	bool		madeToken = false;				// Resets at beginning of each call to makeToken
	bool		hadError = false;

	// Peek and prev buffers

    static constexpr uint s_lookMax = 16;
	RingBuffer<Token, s_lookMax> peekBuffer;
	RingBuffer<Token, s_lookMax> prevBuffer;    // More recent tokens are at the end of the buffer

	// Lexeme buffer management

	uint		iLexemeBuffer = 0;

	// Exit kind

	SCANEXITK	scanexitk = SCANEXITK_Nil;
};



// Public

bool init(Scanner * pScanner, char * pText, uint textSize, char * pLexemeBuffer, uint lexemeBufferSize);
TOKENK peekToken(Scanner * pScanner, Token * poToken=nullptr, uint lookahead=0);
TOKENK prevToken(Scanner * pScanner, Token * poToken=nullptr, uint lookbehind=0);
// bool tryPeekTokenSequence(Scanner * pScanner, const TOKENK * aSequence, int cSequence);
int peekTokenLine(Scanner * pScanner, uint lookahead=0);
int prevTokenLine(Scanner * pScanner, uint lookbehind=0);
bool tryConsumeToken(Scanner * pScanner, TOKENK tokenk, Token * poToken=nullptr);
bool tryConsumeToken(Scanner * pScanner, const TOKENK * aTokenk, int cTokenk, Token * poToken=nullptr);
TOKENK consumeToken(Scanner * pScanner, Token * poToken=nullptr);
bool isFinished(Scanner * pScanner);


// Internal

TOKENK produceNextToken(Scanner * pScanner, Token * poToken);
bool tryConsumeChar(Scanner * pScanner, char expected);
bool tryConsumeChar(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch=nullptr);
bool tryPeekChar(Scanner * pScanner, char expected, int lookahead=0);
bool tryPeekChar(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch=nullptr, int lookahead=0);
char consumeChar(Scanner * pScanner);

void onStartToken(Scanner * pScanner);
void writeCurrentLexemeIntoBuffer(Scanner * pScanner);
void makeToken(Scanner * pScanner, TOKENK tokenk, Token * poToken);
void makeTokenWithLexeme(Scanner * pScanner, TOKENK tokenk, char * lexeme, Token * poToken);
void makeErrorToken(Scanner * pScanner, GRFERRTOK ferrtok, Token * poToken);
void makeEofToken(Scanner * pScanner, Token * poToken);
void finishAfterConsumeDotAndDigit(Scanner * pScanner, char firstDigit, Token * poToken);
void finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, Token * poToken);
void _finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, bool startsWithDot, Token * poToken);

bool checkEndOfFile(Scanner * pScanner, int lookahead=0);

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
