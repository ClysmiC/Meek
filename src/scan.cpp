#include "scan.h"

#include <stdlib.h> // For strncpy and related functions

void init(Scanner * scanner, char * pText, uint textSize)
{
	ClearStruct(scanner);

	scanner->pText = pText;
	scanner->textSize = textSize;
	scanner->scanexitk = SCANEXITK_Nil;
	init(&scanner->newLineIndices);
}

TOKENK consumeToken(Scanner * scanner, NULLABLE Token * poToken)
{
	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	if (scanner->peekBuffer.cItem > 0)
	{
		read(&scanner->peekBuffer, pToken);
		return pToken->tokenk;
	}
	else
	{
		return produceNextToken(scanner, pToken);
	}
}

TOKENK peekToken(Scanner * scanner, NULLABLE Token * poToken, uint lookahead)
{
	Assert(!scanner->isSpeculating);
	Assert(lookahead < Scanner::s_lookMax);
	auto & rbuf = scanner->peekBuffer;

	// Consume tokens until we have enough in our buffer to peek that far

	Token tokenLookahead;
	while (!isFinished(*scanner) && rbuf.cItem <= lookahead)
	{
		produceNextToken(scanner, &tokenLookahead);
		write(&rbuf, tokenLookahead);
	}

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	if (lookahead < rbuf.cItem)
	{
		Verify(peek(rbuf, lookahead, pToken));
	}
	else
	{
		// If we peek any amount past the end of the file we return nil token

		nillify(pToken);
	}

	return pToken->tokenk;
}

TOKENK prevToken(Scanner * scanner, NULLABLE Token * poToken, uint lookbehind)
{
	Assert(!scanner->isSpeculating);
	Assert(lookbehind < Scanner::s_lookMax);
	auto & rbuf = scanner->prevBuffer;

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	if (lookbehind < rbuf.cItem)
	{
		Verify(peek(rbuf, rbuf.cItem - lookbehind - 1, pToken));
	}
	else
	{
		// If we peek any amount before beginning of the file we return nil token

		nillify(pToken);
	}

	return pToken->tokenk;
}

StartEndIndices peekTokenStartEnd(Scanner * scanner, uint lookahead)
{
	Assert(!scanner->isSpeculating);

	Token throwaway;
	peekToken(scanner, &throwaway, lookahead);
	return throwaway.startEnd;
}

StartEndIndices prevTokenStartEnd(Scanner * scanner, uint lookbehind)
{
	Assert(!scanner->isSpeculating);

	Token throwaway;
	prevToken(scanner, &throwaway, lookbehind);
	return throwaway.startEnd;
}

bool tryConsumeToken(Scanner * scanner, TOKENK tokenkMatch, NULLABLE Token * poToken)
{
	Assert(!scanner->isSpeculating);

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	TOKENK tokenk = peekToken(scanner, pToken);

	if (tokenk == tokenkMatch)
	{
		Verify(read(&scanner->peekBuffer, pToken));
		return true;
	}

	return false;
}

bool tryConsumeToken(Scanner * scanner, const TOKENK * aTokenkMatch, int cTokenkMatch, NULLABLE Token * poToken)
{
	Assert(!scanner->isSpeculating);

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	TOKENK tokenk = peekToken(scanner, pToken);

	for (int i = 0; i < cTokenkMatch; i++)
	{
		if (tokenk == aTokenkMatch[i])
		{
			Verify(read(&scanner->peekBuffer, pToken));
			return true;
		}
	}

	return false;
}

bool tryPeekToken(Scanner * scanner, const TOKENK * aTokenkMatch, int cTokenkMatch, NULLABLE Token * poToken)
{
	Assert(!scanner->isSpeculating);

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	TOKENK tokenkNext = peekToken(scanner, pToken);

	for (int i = 0; i < cTokenkMatch; i++)
	{
		if (tokenkNext == aTokenkMatch[i]) return true;
	}

	return false;
}

TOKENK produceNextToken(Scanner * scanner, Token * poToken)
{
	onStartToken(scanner);
	while (!checkEndOfFile(scanner))
	{
		char c = consumeChar(scanner);

		switch (c)
		{
			case '(':
			{
				makeToken(scanner, TOKENK_OpenParen, poToken);
			} break;

			case ')':
			{
				makeToken(scanner, TOKENK_CloseParen, poToken);
			} break;

			case '{':
			{
				makeToken(scanner, TOKENK_OpenBrace, poToken);
			} break;

			case '}':
			{
				makeToken(scanner, TOKENK_CloseBrace, poToken);
			} break;

			case '[':
			{
				makeToken(scanner, TOKENK_OpenBracket, poToken);
			} break;

			case ']':
			{
				makeToken(scanner, TOKENK_CloseBracket, poToken);
			} break;

			case '.':
			{
				char firstDigit;
				if (tryConsumeChar(scanner, '0', '9', &firstDigit))
				{
					finishAfterConsumeDotAndDigit(scanner, firstDigit, poToken);
				}
				else
				{
					makeToken(scanner, TOKENK_Dot, poToken);
				}
			} break;

			case ',':
			{
				makeToken(scanner, TOKENK_Comma, poToken);
			} break;

			case ';':
			{
				makeToken(scanner, TOKENK_Semicolon, poToken);
			} break;

			case ':':
			{
				makeToken(scanner, TOKENK_Colon, poToken);
			} break;

			case '#':
			{
				if      (tryConsumeCharSequenceThenSpace(scanner, "and"))  makeToken(scanner, TOKENK_HashAnd, poToken);
				else if (tryConsumeCharSequenceThenSpace(scanner, "or"))   makeToken(scanner, TOKENK_HashOr, poToken);
				else if (tryConsumeCharSequenceThenSpace(scanner, "xor"))  makeToken(scanner, TOKENK_HashXor, poToken);
				else                                                        makeErrorToken(scanner, FERRTOK_UnknownHashToken, poToken);
			} break;

			case '"':
			{
				GRFERRTOK grferrtok = FERRTOK_UnterminatedString;

				while (!checkEndOfFile(scanner))
				{
					if (tryConsumeChar(scanner, '\n'))
					{
						grferrtok |= FERRTOK_MultilineString;
					}
					else if (tryConsumeChar(scanner, '\"'))
					{
						grferrtok &= ~FERRTOK_UnterminatedString;
						if (!grferrtok)
						{
							makeToken(scanner, TOKENK_StringLiteral, poToken);
						}

						break;
					}
					else
					{
						consumeChar(scanner);
					}
				}

				if (!scanner->madeToken)
				{
					makeErrorToken(scanner, grferrtok, poToken);
				}
			} break;

			case '\'':
			{
				makeToken(scanner, TOKENK_SingleQuote, poToken);
			} break;

			case '+':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_PlusEqual, poToken);
				else if (tryConsumeChar(scanner, '+')) makeToken(scanner, TOKENK_MinusMinus, poToken);
				else makeToken(scanner, TOKENK_Plus, poToken);
			} break;

			case '-':
			{
				// Note: We count the negative sign as part of an int/float literal

				// If we only scan positive literals and rely on unary - to negate, then
				//	-2147483648 (which is a valid s32) would be considered an out of range
				//	int literal since the scanner would read it as a positive value
				//
				// C has this problem:
				//	#define INT_MIN     (-2147483647 - 1)

				char firstDigit;

				if (tryConsumeChar(scanner, '='))
				{
					makeToken(scanner, TOKENK_MinusEqual, poToken);
				}
				else if (tryConsumeChar(scanner, '0', '9', &firstDigit))
				{
					finishAfterConsumeDigit(scanner, firstDigit, poToken);
				}
				else if (tryPeekChar(scanner, '.'))
				{
					int lookahead = 1;
					if (tryPeekChar(scanner, '0', '9', &firstDigit, lookahead))
					{
						consumeChar(scanner);  // '.'
						consumeChar(scanner);  // Digit

						finishAfterConsumeDotAndDigit(scanner, firstDigit, poToken);
					}
				}
				else if (tryConsumeChar(scanner, '-')) makeToken(scanner, TOKENK_MinusMinus, poToken);
				else if (tryConsumeChar(scanner, '>')) makeToken(scanner, TOKENK_MinusGreater, poToken);
				else makeToken(scanner, TOKENK_Minus, poToken);
			} break;

			case '*':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_StarEqual, poToken);
				else makeToken(scanner, TOKENK_Star, poToken);
			} break;

			case '%':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_PercentEqual, poToken);
				else makeToken(scanner, TOKENK_Percent, poToken);
			} break;

			case '/':
			{
				// HMM: Any value in capturing the comment lexeme and passing it as a token
				//	to the parser? Parser could mostly ignore it, but could be useful for
				//	supporting comments w/ semantics, maybe some form of metaprogramming

				if (tryConsumeChar(scanner, '/'))
				{
					if (tryConsumeChar(scanner, ' ') || tryConsumeChar(scanner, '\t'))
					{
						// Entire line comments start with '// ' (slash, slash, space) or '//\t'

						while (!checkEndOfFile(scanner))
						{
							if (tryConsumeChar(scanner, '\n')) break;
							else consumeChar(scanner);
						}
					}
					else
					{
						// Inline comments start with '//' (slash, slash) and go until the next space. Example:
						//	doWebRequest(url, //isAsync: true);

						while (!checkEndOfFile(scanner))
						{
							if (tryConsumeChar(scanner, ' ') ||
								tryConsumeChar(scanner, '\n') ||
								tryConsumeChar(scanner, '\t') ||
								tryConsumeChar(scanner, '\r'))
							{
								break;
							}
							else
							{
								consumeChar(scanner);
							}
						}
					}
				}
				else if (tryConsumeChar(scanner, '*'))
				{
					int cNestedBlockComment = 1;

					GRFERRTOK grferrtok = FERRTOK_UnterminatedBlockComment;
					while (!checkEndOfFile(scanner))
					{
						if (tryConsumeChar(scanner, '/'))
						{
							if (tryConsumeChar(scanner, '*'))
							{
								cNestedBlockComment++;
							}
						}
						else if (tryConsumeChar(scanner, '*'))
						{
							if (tryConsumeChar(scanner, '/'))
							{
								cNestedBlockComment--;

								if (cNestedBlockComment == 0)
								{
									grferrtok &= ~FERRTOK_UnterminatedBlockComment;
									break;
								}
							}
						}
						else
						{
							consumeChar(scanner);
						}
					}

					if (grferrtok)
					{
						makeErrorToken(scanner, grferrtok, poToken);
					}
				}
				else if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_SlashEqual, poToken);
				else makeToken(scanner, TOKENK_Slash, poToken);
			} break;

			case '!':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_BangEqual, poToken);
				else makeToken(scanner, TOKENK_Bang, poToken);
			} break;

			case '=':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_EqualEqual, poToken);
				else makeToken(scanner, TOKENK_Equal, poToken);
			} break;

			case '<':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_LesserEqual, poToken);
				else makeToken(scanner, TOKENK_Lesser, poToken);
			} break;

			case '>':
			{
				if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_GreaterEqual, poToken);
				else makeToken(scanner, TOKENK_Greater, poToken);
			} break;

			case '^':
			{
				makeToken(scanner, TOKENK_Carat, poToken);
			} break;

			case '|':
			{
				if (tryConsumeChar(scanner, '|'))
				{
					if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_PipePipeEqual, poToken);
					else makeToken(scanner, TOKENK_PipePipe, poToken);
				}
				else if (tryConsumeChar(scanner, '='))
				{
					makeToken(scanner, TOKENK_PipeEqual, poToken);
				}
				else
				{
					makeToken(scanner, TOKENK_Pipe, poToken);
				}
			} break;

			case '&':
			{
				if (tryConsumeChar(scanner, '&'))
				{
					if (tryConsumeChar(scanner, '=')) makeToken(scanner, TOKENK_AmpAmpEqual, poToken);
					else makeToken(scanner, TOKENK_AmpAmp, poToken);
				}
				else if (tryConsumeChar(scanner, '='))
				{
					makeToken(scanner, TOKENK_AmpEqual, poToken);
				}
				else
				{
					makeToken(scanner, TOKENK_Amp, poToken);
				}
			} break;

			case ' ':
			case '\n':
			case '\r':
			case '\t':
				break;

			default:
			{
				if (isDigit(c))
				{
					finishAfterConsumeDigit(scanner, c, poToken);
				}
				else if (isLetterOrUnderscore(c))
				{
					while (true)
					{
						if (!(tryConsumeChar(scanner, '_') || tryConsumeChar(scanner, 'a', 'z') || tryConsumeChar(scanner, 'A', 'Z') || tryConsumeChar(scanner, '0', '9')))
						{
							// Try consume fails into here if end of file

							// @Slow

							TOKENK tokenkReserved = TOKENK_Nil;
							StringView lexeme = currentLexeme(*scanner);
							bool isReserved = isReservedWord(lexeme, &tokenkReserved);

							if (isReserved)
							{
								makeToken(scanner, tokenkReserved, lexeme, poToken);
							}
							else
							{
								makeToken(scanner, TOKENK_Identifier, lexeme, poToken);
							}

							break;
						}
					}
				}
				else
				{
					makeErrorToken(scanner, FERRTOK_InvalidCharacter, poToken);
				}

			} break;
		}

		if (scanner->madeToken)
		{
			return poToken->tokenk;
		}
		else
		{
			// We parsed white space or a comment, so we are going back to the top of the loop

			onStartToken(scanner);
		}
	}

	makeEofToken(scanner, poToken);
	return poToken->tokenk;
}

bool isFinished(const Scanner & scanner)
{
	return scanner.scanexitk != SCANEXITK_Nil;
}

int lineFromI(const Scanner & scanner, int iText)
{
	// SLOW: Can use binary search + some heuristics to accelerate this, or mabye keep a second list that marks i for every 100th (or 1000th ?) line
	//  that will let us just to the correct "neighborhood" faster

	int line = 1;
	for (int i = 0; i < scanner.newLineIndices.cItem; i++)
	{
		// '\n' itself is considered to be "on" the line that it ends

		if (scanner.newLineIndices[i] >= iText) break;

		line++;
	}

	return line;
}

TOKENK nextTokenkSpeculative(Scanner * scanner)
{
	if (!scanner->isSpeculating)
	{
		scanner->iPeekBufferSpeculative = 0;
		scanner->iTextSpeculative = scanner->iText;
		scanner->isSpeculating = true;
	}

	Token throwaway;
	if (scanner->iPeekBufferSpeculative < scanner->peekBuffer.cItem)
	{
		Verify(peek(scanner->peekBuffer, scanner->iPeekBufferSpeculative, &throwaway));
		scanner->iPeekBufferSpeculative++;

		return throwaway.tokenk;
	}

	return produceNextToken(scanner, &throwaway);
}

void backtrackAfterSpeculation(Scanner * scanner)
{
	Assert(scanner->isSpeculating);
	scanner->isSpeculating = false;
}

void finishAfterConsumeDigit(Scanner * scanner, char firstDigit, Token * poToken)
{
	finishAfterConsumeDigitMaybeStartingWithDot(scanner, firstDigit, false, poToken);
}

void finishAfterConsumeDotAndDigit(Scanner * scanner, char firstDigit, Token * poToken)
{
	finishAfterConsumeDigitMaybeStartingWithDot(scanner, firstDigit, true, poToken);
}

void finishAfterConsumeDigitMaybeStartingWithDot(Scanner * scanner, char firstDigit, bool startsWithDot, Token * poToken)
{
	int cDot = (startsWithDot) ? 1 : 0;
	int base = 10;
	int cDigit = 1;

	if (!startsWithDot)
	{
		if (firstDigit == '0')
		{
			// Right now only supporting lower case prefix.
			//  0xABCD works
			//  0XABCD does not

			if (tryConsumeChar(scanner, 'x'))
			{
				base = 16;
				cDigit = 0;		// Prefixes don't count toward cDigit
			}
			else if (tryConsumeChar(scanner, 'b'))
			{
				base = 2;
				cDigit = 0;
			}
			else if (tryConsumeChar(scanner, 'o'))
			{
				base = 8;
				cDigit = 0;
			}
		}
	}

	GRFERRTOK grferrtok = GRFERRTOK_None;
	if (base == 16)
	{
		// TODO: Move the case checking to semantic analysis so that we can report more interesting errors if they exist

		// Support both lower and upper case hex letters, but you can't
		//  mix and match!
		//  0xABCD works
		//  0xabcd works
		//  0xAbCd does not

		// enum CASEK
		// {
		// 	CASEK_Tbd,
		// 	CASEK_Lower,
		// 	CASEK_Upper
		// };

		// CASEK casek = CASEK_Tbd;

		while (true)
		{
			if (tryConsumeChar(scanner, '.'))
			{
				cDot++;
			}
			else if (tryConsumeChar(scanner, 'a', 'f') ||
					 tryConsumeChar(scanner, 'A', 'F') ||
					 tryConsumeChar(scanner, '0', '9'))
			{
				cDigit++;
			}
			else
			{
				// Try consume fails into here if end of file

				break;
			}
		}
	}
	else
	{
		while (true)
		{
			if (tryConsumeChar(scanner, '.'))
			{
				cDot++;
			}
			else
			{
				char upperChar = '0' + base - 1;
				if (tryConsumeChar(scanner, '0', upperChar))
				{
					cDigit++;
				}
				else
				{
					// Try consume fails into here if end of file

					break;
				}
			}
		}
	}

	if (base != 10 && cDot > 0)
	{
		grferrtok |= FERRTOK_IntLiteralNonBase10WithDecimal;
	}

	if (cDot > 1)
	{
		grferrtok |= FERRTOK_NumberLiteralMultipleDecimals;
	}

	if (base != 10 && cDigit == 0)
	{
		grferrtok |= FERRTOK_IntLiteralNonBase10NoDigits;
	}

	if (!grferrtok)
	{
		(cDot > 0) ?
			makeToken(scanner, TOKENK_FloatLiteral, poToken) :
			makeToken(scanner, TOKENK_IntLiteral, poToken);
	}
	else
	{
		makeErrorToken(scanner, grferrtok, poToken);
	}
}

StringView currentLexeme(const Scanner & scanner)
{
	StringView result;
	result.pCh = scanner.pText + scanner.iTextTokenStart;
	if (scanner.isSpeculating)
	{
		result.cCh = scanner.iTextSpeculative - scanner.iTextTokenStart;
	}
	else
	{
		result.cCh = scanner.iText - scanner.iTextTokenStart;
	}

	return result;
}

void makeToken(Scanner * scanner, TOKENK tokenk, Token * poToken)
{
	makeToken(scanner, tokenk, currentLexeme(*scanner), poToken);
}

void makeToken(Scanner * scanner, TOKENK tokenk, StringView lexeme, Token * poToken)
{
	if (scanner->isSpeculating)
	{
		// This is the only result we care about when speculating.
		//	Speculation is kinda half-baked right now...

		poToken->tokenk = tokenk;
	}
	else
	{
		Assert(!scanner->madeToken);

		poToken->id = scanner->iToken + 1;		// + 1 to make sure we never have id 0
		poToken->startEnd.iStart = scanner->iTextTokenStart;
		poToken->startEnd.iEnd = scanner->iText;
		poToken->tokenk = tokenk;
		setLexeme(&poToken->lexeme, lexeme);
		poToken->grferrtok = 0;

		forceWrite(&scanner->prevBuffer, *poToken);
		scanner->iToken++;
	}

	scanner->madeToken = true;
}

void makeErrorToken(Scanner * scanner, GRFERRTOK grferrtok, Token * poToken)
{
	Assert(grferrtok);
	makeToken(scanner, TOKENK_Error, poToken);
	poToken->grferrtok = grferrtok;
}

void makeEofToken(Scanner * scanner, Token * poToken)
{
	StringView lexeme = { 0 };
	makeToken(scanner, TOKENK_Eof, lexeme, poToken);
}

char consumeChar(Scanner * scanner)
{
	if (checkEndOfFile(scanner)) return '\0';	 // This check might be redundant/unnecessary

	int * pIText = scanner->isSpeculating ? &scanner->iTextSpeculative : &scanner->iText;

	char c = scanner->pText[*pIText];
	if (c == '\n' && !scanner->isSpeculating)
	{
		append(&scanner->newLineIndices, *pIText);
	}

	(*pIText)++;

	return c;
}

bool tryConsumeChar(Scanner * scanner, char expected)
{
	if (checkEndOfFile(scanner)) return false;

	int iText = scanner->isSpeculating ? scanner->iTextSpeculative : scanner->iText;

	if (scanner->pText[iText] != expected) return false;

	consumeChar(scanner);

	return true;
}

bool tryConsumeChar(Scanner * scanner, char rangeMin, char rangeMax, char * poMatch)
{
	if (checkEndOfFile(scanner)) return false;
	if (rangeMin > rangeMax) return false;

	int iText = scanner->isSpeculating ? scanner->iTextSpeculative : scanner->iText;

	char c = scanner->pText[iText];
	if (c < rangeMin || c > rangeMax) return false;

	consumeChar(scanner);

	if (poMatch) *poMatch = c;

	return true;
}

bool tryConsumeCharSequenceThenSpace(Scanner * scanner, const char * sequence)
{
	int lookahead = 0;
	const char * character = sequence;
	while (*character)
	{
		Assert(isLetter(*character));

		if (!tryPeekChar(scanner, *character, lookahead))
		{
			return false;
		}

		character++;
		lookahead++;
	}

	if (!isWhitespace(peekChar(scanner, lookahead)))
	{
		return false;
	}

	// All peeks checked out. Now just consume through them.

	for (int i = 0; i < lookahead; i++) consumeChar(scanner);

	return true;
}

char peekChar(Scanner * scanner, int lookahead)
{
	if (checkEndOfFile(scanner, lookahead)) return '\0';

	int iText = scanner->isSpeculating ? scanner->iTextSpeculative : scanner->iText;

	return scanner->pText[iText + lookahead];
}

bool tryPeekChar(Scanner * scanner, char expected, int lookahead)
{
	if (checkEndOfFile(scanner, lookahead)) return false;

	int iText = scanner->isSpeculating ? scanner->iTextSpeculative : scanner->iText;

	if (scanner->pText[iText + lookahead] != expected) return false;

	return true;
}

bool tryPeekChar(Scanner * scanner, char rangeMin, char rangeMax, char * poMatch, int lookahead)
{
	if (checkEndOfFile(scanner, lookahead)) return false;
	if (rangeMin > rangeMax) return false;

	int iText = scanner->isSpeculating ? scanner->iTextSpeculative : scanner->iText;

	char c = scanner->pText[iText + lookahead];
	if (c < rangeMin || c > rangeMax) return false;

	if (poMatch) *poMatch = c;

	return true;
}

void onStartToken(Scanner * scanner)
{
	int iText = scanner->isSpeculating ? scanner->iTextSpeculative : scanner->iText;

	scanner->iTextTokenStart = iText;
	scanner->madeToken = false;
}

bool checkEndOfFile(Scanner * scanner, int lookahead)
{
	Assert(lookahead >= 0);

	// NOTE: 0 lookahead checks the next character, so we still want to
	//	run this loop iteration at least once, hence <=

	if (scanner->isSpeculating)
	{
		for (int i = 0; i <= lookahead; i++)
		{
			char * pCh = scanner->pText + scanner->iTextSpeculative + i;
			if (*pCh == '\0')
			{
				return true;
			}
		}

		if (scanner->iTextSpeculative + lookahead >= scanner->textSize)
		{
			return true;
		}
	}
	else
	{
		for (int i = 0; i <= lookahead; i++)
		{
			char * pCh = scanner->pText + scanner->iText + i;
			if (*pCh == '\0')
			{
				scanner->scanexitk = SCANEXITK_ReadNullTerminator;
				return true;
			}
		}

		if (scanner->iText + lookahead >= scanner->textSize)
		{
			if (lookahead == 0) scanner->scanexitk = SCANEXITK_ReadAllBytes;
			return true;
		}
	}

	return false;
}
