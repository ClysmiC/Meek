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

	char * pText = nullptr;
	int textSize = 0;

	// Scan state

	int iText = 0;

	int iTextTokenStart = 0;

	int iToken = 0;						// Becomes tokens id

	bool madeToken = false;				// Resets at beginning of each call to makeToken
	bool hadError = false;

	// Sorted array of each \n index. Used to retrieve line #'s from raw indices (which are the only thing we track)

	DynamicArray<int> newLineIndices;

	// Peek and prev buffers

	static constexpr int s_lookMax = 16;
	RingBuffer<Token, s_lookMax> peekBuffer;
	RingBuffer<Token, s_lookMax> prevBuffer;    // More recent tokens are at the end of the buffer

	// Speculative parsing/backtracking

	bool isSpeculating = false;
	uint iPeekBufferSpeculative=  0;
	int iTextSpeculative = 0;

	// Exit kind

	SCANEXITK	scanexitk = SCANEXITK_Nil;
};

void init(Scanner * scanner, char * pText, uint textSize);
bool isFinished(const Scanner & scanner);
int lineFromI(const Scanner & scanner, int iText);

// Scanning

TOKENK peekToken(Scanner * scanner, NULLABLE Token * poToken=nullptr, uint lookahead=0);
TOKENK prevToken(Scanner * scanner, NULLABLE Token * poToken=nullptr, uint lookbehind=0);
StartEndIndices peekTokenStartEnd(Scanner * scanner, uint lookahead=0);
StartEndIndices prevTokenStartEnd(Scanner * scanner, uint lookbehind=0);
bool tryConsumeToken(Scanner * scanner, TOKENK tokenk, NULLABLE Token * poToken=nullptr);
bool tryConsumeToken(Scanner * scanner, const TOKENK * aTokenk, int cTokenk, NULLABLE Token * poToken=nullptr);
bool tryPeekToken(Scanner * scanner, const TOKENK * aTokenk, int cTokenk, NULLABLE Token * poToken=nullptr);
TOKENK consumeToken(Scanner * scanner, NULLABLE Token * poToken=nullptr);

// Speculative scanning

TOKENK nextTokenkSpeculative(Scanner * scanner);
void backtrackAfterSpeculation(Scanner * scanner);



// Internal

TOKENK produceNextToken(Scanner * scanner, Token * poToken);
bool tryConsumeChar(Scanner * scanner, char expected);
bool tryConsumeChar(Scanner * scanner, char rangeMin, char rangeMax, char * poMatch=nullptr);
bool tryConsumeCharSequenceThenSpace(Scanner * scanner, const char * sequence);
char consumeChar(Scanner * scanner);
bool tryPeekChar(Scanner * scanner, char expected, int lookahead=0);
bool tryPeekChar(Scanner * scanner, char rangeMin, char rangeMax, char * poMatch=nullptr, int lookahead=0);
char peekChar(Scanner * scanner, int lookahead=0);

void onStartToken(Scanner * scanner);
StringView currentLexeme(const Scanner & scanner);
void makeToken(Scanner * scanner, TOKENK tokenk, Token * poToken);
void makeToken(Scanner * scanner, TOKENK tokenk, StringView lexeme, Token * poToken);
void makeErrorToken(Scanner * scanner, GRFERRTOK ferrtok, Token * poToken);
void makeEofToken(Scanner * scanner, Token * poToken);
void finishAfterConsumeDotAndDigit(Scanner * scanner, char firstDigit, Token * poToken);
void finishAfterConsumeDigit(Scanner * scanner, char firstDigit, Token * poToken);
void finishAfterConsumeDigitMaybeStartingWithDot(Scanner * scanner, char firstDigit, bool startsWithDot, Token * poToken);

bool checkEndOfFile(Scanner * scanner, int lookahead=0);

inline bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

inline bool isLetter(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool isLetterOrUnderscore(char c)
{
	return c == '_' || isLetter(c);
}

// SYNC: Switch statement in scanner for chewing thru white space
// SYNC: Charcters we check for to end //inline comment
inline bool isWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

inline bool isDigitOrLetterOrUnderscore(char c)
{
	return isDigit(c) || isLetterOrUnderscore(c);
}
