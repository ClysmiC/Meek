#include "scan.h"

#include <stdlib.h> // For strncpy and related functions

void init(Scanner * pScanner, char * pText, uint textSize)
{
	ClearStruct(pScanner);

	pScanner->pText = pText;
	pScanner->textSize = textSize;
	pScanner->scanexitk = SCANEXITK_Nil;
	init(&pScanner->newLineIndices);
}

TOKENK consumeToken(Scanner * pScanner, NULLABLE Token * poToken)
{
	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	if (pScanner->peekBuffer.cItem > 0)
	{
		read(&pScanner->peekBuffer, pToken);
		return pToken->tokenk;
	}
	else
	{
		return produceNextToken(pScanner, pToken);
	}
}

TOKENK peekToken(Scanner * pScanner, NULLABLE Token * poToken, uint lookahead)
{
	Assert(!pScanner->isSpeculating);
	Assert(lookahead < Scanner::s_lookMax);
	auto & rbuf = pScanner->peekBuffer;

	// Consume tokens until we have enough in our buffer to peek that far

	Token tokenLookahead;
	while (!isFinished(pScanner) && rbuf.cItem <= lookahead)
	{
		produceNextToken(pScanner, &tokenLookahead);
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

TOKENK prevToken(Scanner * pScanner, NULLABLE Token * poToken, uint lookbehind)
{
	Assert(!pScanner->isSpeculating);
	Assert(lookbehind < Scanner::s_lookMax);
	auto & rbuf = pScanner->prevBuffer;

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

StartEndIndices peekTokenStartEnd(Scanner * pScanner, uint lookahead)
{
	Assert(!pScanner->isSpeculating);

	Token throwaway;
	peekToken(pScanner, &throwaway, lookahead);
	return throwaway.startEnd;
}

StartEndIndices prevTokenStartEnd(Scanner * pScanner, uint lookbehind)
{
	Assert(!pScanner->isSpeculating);

	Token throwaway;
	prevToken(pScanner, &throwaway, lookbehind);
	return throwaway.startEnd;
}

bool tryConsumeToken(Scanner * pScanner, TOKENK tokenkMatch, NULLABLE Token * poToken)
{
	Assert(!pScanner->isSpeculating);

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	TOKENK tokenk = peekToken(pScanner, pToken);

	if (tokenk == tokenkMatch)
	{
		Verify(read(&pScanner->peekBuffer, pToken));
		return true;
	}

	return false;
}

bool tryConsumeToken(Scanner * pScanner, const TOKENK * aTokenkMatch, int cTokenkMatch, NULLABLE Token * poToken)
{
	Assert(!pScanner->isSpeculating);

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	TOKENK tokenk = peekToken(pScanner, pToken);

	for (int i = 0; i < cTokenkMatch; i++)
	{
		if (tokenk == aTokenkMatch[i])
		{
			Verify(read(&pScanner->peekBuffer, pToken));
			return true;
		}
	}

	return false;
}

bool tryPeekToken(Scanner * pScanner, const TOKENK * aTokenkMatch, int cTokenkMatch, NULLABLE Token * poToken)
{
	Assert(!pScanner->isSpeculating);

	Token throwaway;
	Token * pToken = (poToken) ? poToken : &throwaway;

	TOKENK tokenkNext = peekToken(pScanner, pToken);

	for (int i = 0; i < cTokenkMatch; i++)
	{
		if (tokenkNext == aTokenkMatch[i]) return true;
	}

	return false;
}

TOKENK produceNextToken(Scanner * pScanner, Token * poToken)
{
	onStartToken(pScanner);
	while (!checkEndOfFile(pScanner))
	{
		char c = consumeChar(pScanner);

		switch (c)
		{
			case '(':
			{
				makeToken(pScanner, TOKENK_OpenParen, poToken);
			} break;

			case ')':
			{
				makeToken(pScanner, TOKENK_CloseParen, poToken);
			} break;

			case '{':
			{
				makeToken(pScanner, TOKENK_OpenBrace, poToken);
			} break;

			case '}':
			{
				makeToken(pScanner, TOKENK_CloseBrace, poToken);
			} break;

			case '[':
			{
				makeToken(pScanner, TOKENK_OpenBracket, poToken);
			} break;

			case ']':
			{
				makeToken(pScanner, TOKENK_CloseBracket, poToken);
			} break;

			case '.':
			{
				char firstDigit;
				if (tryConsumeChar(pScanner, '0', '9', &firstDigit))
				{
					finishAfterConsumeDotAndDigit(pScanner, firstDigit, poToken);
				}
				else
				{
					makeToken(pScanner, TOKENK_Dot, poToken);
				}
			} break;

			case ',':
			{
				makeToken(pScanner, TOKENK_Comma, poToken);
			} break;

			case ';':
			{
				makeToken(pScanner, TOKENK_Semicolon, poToken);
			} break;

			case ':':
			{
				makeToken(pScanner, TOKENK_Colon, poToken);
			} break;

			case '#':
			{
				if      (tryConsumeCharSequenceThenSpace(pScanner, "and"))  makeToken(pScanner, TOKENK_HashAnd, poToken);
				else if (tryConsumeCharSequenceThenSpace(pScanner, "or"))   makeToken(pScanner, TOKENK_HashOr, poToken);
				else if (tryConsumeCharSequenceThenSpace(pScanner, "xor"))  makeToken(pScanner, TOKENK_HashXor, poToken);
				else                                                        makeErrorToken(pScanner, FERRTOK_UnknownHashToken, poToken);
			} break;

			case '"':
			{
				GRFERRTOK grferrtok = FERRTOK_UnterminatedString;

				while (!checkEndOfFile(pScanner))
				{
					if (tryConsumeChar(pScanner, '\n'))
					{
						grferrtok |= FERRTOK_MultilineString;
					}
					else if (tryConsumeChar(pScanner, '\"'))
					{
						grferrtok &= ~FERRTOK_UnterminatedString;
						if (!grferrtok)
						{
							makeToken(pScanner, TOKENK_StringLiteral, poToken);
						}

						break;
					}
					else
					{
						consumeChar(pScanner);
					}
				}

				if (!pScanner->madeToken)
				{
					makeErrorToken(pScanner, grferrtok, poToken);
				}
			} break;

			case '\'':
			{
				makeToken(pScanner, TOKENK_SingleQuote, poToken);
			} break;

			case '+':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_PlusEqual, poToken);
				else if (tryConsumeChar(pScanner, '+')) makeToken(pScanner, TOKENK_MinusMinus, poToken);
				else makeToken(pScanner, TOKENK_Plus, poToken);
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

				if (tryConsumeChar(pScanner, '='))
				{
					makeToken(pScanner, TOKENK_MinusEqual, poToken);
				}
				else if (tryConsumeChar(pScanner, '0', '9', &firstDigit))
				{
					finishAfterConsumeDigit(pScanner, firstDigit, poToken);
				}
				else if (tryPeekChar(pScanner, '.'))
				{
					int lookahead = 1;
					if (tryPeekChar(pScanner, '0', '9', &firstDigit, lookahead))
					{
						consumeChar(pScanner);  // '.'
						consumeChar(pScanner);  // Digit

						finishAfterConsumeDotAndDigit(pScanner, firstDigit, poToken);
					}
				}
				else if (tryConsumeChar(pScanner, '-')) makeToken(pScanner, TOKENK_MinusMinus, poToken);
				else if (tryConsumeChar(pScanner, '>')) makeToken(pScanner, TOKENK_MinusGreater, poToken);
				else makeToken(pScanner, TOKENK_Minus, poToken);
			} break;

			case '*':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_StarEqual, poToken);
				else makeToken(pScanner, TOKENK_Star, poToken);
			} break;

			case '%':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_PercentEqual, poToken);
				else makeToken(pScanner, TOKENK_Percent, poToken);
			} break;

			case '/':
			{
				// HMM: Any value in capturing the comment lexeme and passing it as a token
				//	to the parser? Parser could mostly ignore it, but could be useful for
				//	supporting comments w/ semantics, maybe some form of metaprogramming

				if (tryConsumeChar(pScanner, '/'))
				{
					if (tryConsumeChar(pScanner, ' ') || tryConsumeChar(pScanner, '\t'))
					{
						// Entire line comments start with '// ' (slash, slash, space) or '//\t'

						while (!checkEndOfFile(pScanner))
						{
							if (tryConsumeChar(pScanner, '\n')) break;
							else consumeChar(pScanner);
						}
					}
					else
					{
						// Inline comments start with '//' (slash, slash) and go until the next space. Example:
						//	doWebRequest(url, //isAsync: true);

						while (!checkEndOfFile(pScanner))
						{
							if (tryConsumeChar(pScanner, ' ') ||
								tryConsumeChar(pScanner, '\n') ||
								tryConsumeChar(pScanner, '\t') ||
								tryConsumeChar(pScanner, '\r'))
							{
								break;
							}
							else
							{
								consumeChar(pScanner);
							}
						}
					}
				}
				else if (tryConsumeChar(pScanner, '*'))
				{
					pScanner->cNestedBlockComment++;
					Assert(pScanner->cNestedBlockComment == 1);

					GRFERRTOK grferrtok = FERRTOK_UnterminatedBlockComment;
					while (!checkEndOfFile(pScanner))
					{
						if (tryConsumeChar(pScanner, '/'))
						{
							if (tryConsumeChar(pScanner, '*'))
							{
								pScanner->cNestedBlockComment++;
							}
						}
						else if (tryConsumeChar(pScanner, '*'))
						{
							if (tryConsumeChar(pScanner, '/'))
							{
								pScanner->cNestedBlockComment--;

								if (pScanner->cNestedBlockComment == 0)
								{
									grferrtok &= ~FERRTOK_UnterminatedBlockComment;
									break;
								}
							}
						}
						else
						{
							consumeChar(pScanner);
						}
					}

					if (grferrtok)
					{
						makeErrorToken(pScanner, grferrtok, poToken);
					}
				}
				else if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_SlashEqual, poToken);
				else makeToken(pScanner, TOKENK_Slash, poToken);
			} break;

			case '!':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_BangEqual, poToken);
				else makeToken(pScanner, TOKENK_Bang, poToken);
			} break;

			case '=':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_EqualEqual, poToken);
				else makeToken(pScanner, TOKENK_Equal, poToken);
			} break;

			case '<':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_LesserEqual, poToken);
				else makeToken(pScanner, TOKENK_Lesser, poToken);
			} break;

			case '>':
			{
				if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_GreaterEqual, poToken);
				else makeToken(pScanner, TOKENK_Greater, poToken);
			} break;

			case '^':
			{
				makeToken(pScanner, TOKENK_Carat, poToken);
			} break;

			case '|':
			{
				if (tryConsumeChar(pScanner, '|'))
				{
					if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_PipePipeEqual, poToken);
					else makeToken(pScanner, TOKENK_PipePipe, poToken);
				}
				else if (tryConsumeChar(pScanner, '='))
				{
					makeToken(pScanner, TOKENK_PipeEqual, poToken);
				}
				else
				{
					makeToken(pScanner, TOKENK_Pipe, poToken);
				}
			} break;

			case '&':
			{
				if (tryConsumeChar(pScanner, '|'))
				{
					if (tryConsumeChar(pScanner, '=')) makeToken(pScanner, TOKENK_AmpAmpEqual, poToken);
					else makeToken(pScanner, TOKENK_AmpAmp, poToken);
				}
				else if (tryConsumeChar(pScanner, '='))
				{
					makeToken(pScanner, TOKENK_AmpEqual, poToken);
				}
				else
				{
					makeToken(pScanner, TOKENK_Amp, poToken);
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
					finishAfterConsumeDigit(pScanner, c, poToken);
				}
				else if (isLetterOrUnderscore(c))
				{
					while (true)
					{
						if (!(tryConsumeChar(pScanner, '_') || tryConsumeChar(pScanner, 'a', 'z') || tryConsumeChar(pScanner, 'A', 'Z') || tryConsumeChar(pScanner, '0', '9')))
						{
							// Try consume fails into here if end of file

							// @Slow

							TOKENK tokenkReserved = TOKENK_Nil;
							StringView lexeme = currentLexeme(*pScanner);
							bool isReserved = isReservedWord(lexeme, &tokenkReserved);

							if (isReserved)
							{
								makeToken(pScanner, tokenkReserved, lexeme, poToken);
							}
							else
							{
								makeToken(pScanner, TOKENK_Identifier, lexeme, poToken);
							}

							break;
						}
					}
				}
				else
				{
					makeErrorToken(pScanner, FERRTOK_InvalidCharacter, poToken);
				}

			} break;
		}

		if (pScanner->madeToken)
		{
			return poToken->tokenk;
		}
		else
		{
			// We parsed white space or a comment, so we are going back to the top of the loop

			onStartToken(pScanner);
		}
	}

	makeEofToken(pScanner, poToken);
	return poToken->tokenk;
}

bool isFinished(Scanner * pScanner)
{
	return pScanner->scanexitk != SCANEXITK_Nil;
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

TOKENK nextTokenkSpeculative(Scanner * pScanner)
{
	if (!pScanner->isSpeculating)
	{
		pScanner->iPeekBufferSpeculative = 0;
		pScanner->iTextSpeculative = pScanner->iText;
		pScanner->isSpeculating = true;
	}

	Token throwaway;
	if (pScanner->iPeekBufferSpeculative < pScanner->peekBuffer.cItem)
	{
		Verify(peek(pScanner->peekBuffer, pScanner->iPeekBufferSpeculative, &throwaway));
		pScanner->iPeekBufferSpeculative++;

		return throwaway.tokenk;
	}

	return produceNextToken(pScanner, &throwaway);
}

void backtrackAfterSpeculation(Scanner * pScanner)
{
	Assert(pScanner->isSpeculating);
	pScanner->isSpeculating = false;
}

void finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, Token * poToken)
{
	finishAfterConsumeDigitMaybeStartingWithDot(pScanner, firstDigit, false, poToken);
}

void finishAfterConsumeDotAndDigit(Scanner * pScanner, char firstDigit, Token * poToken)
{
	finishAfterConsumeDigitMaybeStartingWithDot(pScanner, firstDigit, true, poToken);
}

void finishAfterConsumeDigitMaybeStartingWithDot(Scanner * pScanner, char firstDigit, bool startsWithDot, Token * poToken)
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

			if (tryConsumeChar(pScanner, 'x'))
			{
				base = 16;
				cDigit = 0;		// Prefixes don't count toward cDigit
			}
			else if (tryConsumeChar(pScanner, 'b'))
			{
				base = 2;
				cDigit = 0;
			}
			else if (tryConsumeChar(pScanner, 'o'))
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
			if (tryConsumeChar(pScanner, '.'))
			{
				cDot++;
			}
			else if (tryConsumeChar(pScanner, 'a', 'f') ||
					 tryConsumeChar(pScanner, 'A', 'F') ||
					 tryConsumeChar(pScanner, '0', '9'))
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
			if (tryConsumeChar(pScanner, '.'))
			{
				cDot++;
			}
			else
			{
				char upperChar = '0' + base - 1;
				if (tryConsumeChar(pScanner, '0', upperChar))
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
			makeToken(pScanner, TOKENK_FloatLiteral, poToken) :
			makeToken(pScanner, TOKENK_IntLiteral, poToken);
	}
	else
	{
		makeErrorToken(pScanner, grferrtok, poToken);
	}
}

StringView currentLexeme(const Scanner & scanner)
{
	StringView result;
	result.pCh = scanner.pText + scanner.iTextTokenStart;
	result.cCh = scanner.iText - scanner.iTextTokenStart;
	return result;
}

void makeToken(Scanner * pScanner, TOKENK tokenk, Token * poToken)
{
	makeToken(pScanner, tokenk, currentLexeme(*pScanner), poToken);
}

void makeToken(Scanner * pScanner, TOKENK tokenk, StringView lexeme, Token * poToken)
{
	if (pScanner->isSpeculating)
	{
		// This is the only result we care about when speculating.
		//	Speculation is kinda half-baked right now...

		poToken->tokenk = tokenk;
	}
	else
	{
		Assert(!pScanner->madeToken);

		poToken->id = pScanner->iToken + 1;		// + 1 to make sure we never have id 0
		poToken->startEnd.iStart = pScanner->iTextTokenStart;
		poToken->startEnd.iEnd = pScanner->iText;
		poToken->tokenk = tokenk;
		poToken->lexeme = lexeme;
		poToken->grferrtok = 0;

		forceWrite(&pScanner->prevBuffer, *poToken);
		pScanner->iToken++;
	}

	pScanner->madeToken = true;
}

void makeErrorToken(Scanner * pScanner, GRFERRTOK grferrtok, Token * poToken)
{
	Assert(grferrtok);
	makeToken(pScanner, TOKENK_Error, poToken);
	poToken->grferrtok = grferrtok;
}

void makeEofToken(Scanner * pScanner, Token * poToken)
{
	StringView lexeme = { 0 };
	makeToken(pScanner, TOKENK_Eof, lexeme, poToken);
}

char consumeChar(Scanner * pScanner)
{
	if (checkEndOfFile(pScanner)) return '\0';	 // This check might be redundant/unnecessary

	int * pIText = pScanner->isSpeculating ? &pScanner->iTextSpeculative : &pScanner->iText;

	char c = pScanner->pText[*pIText];
	if (c == '\n' && !pScanner->isSpeculating)
	{
		append(&pScanner->newLineIndices, *pIText);
	}

	(*pIText)++;

	return c;
}

bool tryConsumeChar(Scanner * pScanner, char expected)
{
	if (checkEndOfFile(pScanner)) return false;
	if (pScanner->pText[pScanner->iText] != expected) return false;

	consumeChar(pScanner);

	return true;
}

bool tryConsumeChar(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch)
{
	if (checkEndOfFile(pScanner)) return false;
	if (rangeMin > rangeMax) return false;

	char c = pScanner->pText[pScanner->iText];
	if (c < rangeMin || c > rangeMax) return false;

	consumeChar(pScanner);

	if (poMatch) *poMatch = c;

	return true;
}

bool tryConsumeCharSequenceThenSpace(Scanner * pScanner, const char * sequence)
{
	int lookahead = 0;
	const char * character = sequence;
	while (*character)
	{
		Assert(isLetter(*character));

		if (!tryPeekChar(pScanner, *character, lookahead))
		{
			return false;
		}

		character++;
		lookahead++;
	}

	if (!isWhitespace(peekChar(pScanner, lookahead)))
	{
		return false;
	}

	// All peeks checked out. Now just consume through them.

	for (int i = 0; i < lookahead; i++) consumeChar(pScanner);

	return true;
}

char peekChar(Scanner * pScanner, int lookahead)
{
	if (checkEndOfFile(pScanner, lookahead)) return '\0';
	return pScanner->pText[pScanner->iText + lookahead];
}

bool tryPeekChar(Scanner * pScanner, char expected, int lookahead)
{
	if (checkEndOfFile(pScanner, lookahead)) return false;
	if (pScanner->pText[pScanner->iText + lookahead] != expected) return false;

	return true;
}

bool tryPeekChar(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch, int lookahead)
{
	if (checkEndOfFile(pScanner, lookahead)) return false;
	if (rangeMin > rangeMax) return false;

	char c = pScanner->pText[pScanner->iText + lookahead];
	if (c < rangeMin || c > rangeMax) return false;

	if (poMatch) *poMatch = c;

	return true;
}

void onStartToken(Scanner * pScanner)
{
	pScanner->iTextTokenStart = pScanner->iText;
	pScanner->madeToken = false;
}

bool checkEndOfFile(Scanner * pScanner, int lookahead)
{
	Assert(lookahead >= 0);

	// NOTE: 0 lookahead checks the next character, so we still want to
	//	run this loop iteration at least once, hence <=

	if (pScanner->isSpeculating)
	{
		for (int i = 0; i <= lookahead; i++)
		{
			if (!(pScanner->pText + pScanner->iTextSpeculative + i))
			{
				return true;
			}
		}
	}
	else
	{
		for (int i = 0; i <= lookahead; i++)
		{
			if (!(pScanner->pText + pScanner->iText + i))
			{
				pScanner->scanexitk = SCANEXITK_ReadNullTerminator;
				return true;
			}
		}

		if (pScanner->iText + lookahead >= pScanner->textSize)
		{
			if (lookahead == 0) pScanner->scanexitk = SCANEXITK_ReadAllBytes;
			return true;
		}
	}

	return false;
}
