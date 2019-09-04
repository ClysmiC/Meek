#include "scan.h"

#include <stdlib.h> // For strncpy and related functions

bool init(Scanner * pScanner, char * pText, uint textSize, char * pLexemeBuffer, uint lexemeBufferSize)
{
	if (!pScanner || !pText || !pLexemeBuffer || lexemeBufferSize < textSize) return false;

	ClearStruct(pScanner);

	pScanner->pText = pText;
	pScanner->textSize = textSize;
	pScanner->pLexemeBuffer = pLexemeBuffer;
	pScanner->lexemeBufferSize = lexemeBufferSize;
	pScanner->scanexitk = SCANEXITK_Nil;
	pScanner->line = 1;
	pScanner->lineTokenStart = 1;
	pScanner->column = 1;
	pScanner->columnTokenStart = 1;

	return true;
}

TOKENK nextToken(Scanner * pScanner, Token * poToken)
{
	if (count(pScanner->peekBuffer) > 0)
	{
		read(&pScanner->peekBuffer, poToken);
		return poToken->tokenk;
	}
	else
	{
		return produceNextToken(pScanner, poToken);
	}
}

TOKENK peekToken(Scanner * pScanner, Token * poToken, uint lookahead)
{
	Assert(lookahead < Scanner::s_lookMax);
	auto & rbuf = pScanner->peekBuffer;

	// Consume tokens until we have enough in our buffer to peek that far

	Token tokenLookahead;
	while (!isFinished(pScanner) && count(rbuf) <= lookahead)
	{
		produceNextToken(pScanner, &tokenLookahead);
		write(&rbuf, tokenLookahead);
	}

	// If we peek any amount past the end of the file we return nil token

	if (lookahead < rbuf.cItem)
	{
		Verify(peek(rbuf, lookahead, poToken));
	}
	else
	{
		nillify(poToken);
	}

	return poToken->tokenk;
}

TOKENK prevToken(Scanner * pScanner, Token * poToken, uint lookbehind)
{
	Assert(lookbehind < Scanner::s_lookMax);
	auto & rbuf = pScanner->prevBuffer;

	// If we peek any amount before beginning of the file we return nil token

	if (lookbehind < rbuf.cItem)
	{
		Verify(peek(rbuf, rbuf.cItem - lookbehind - 1, poToken));
	}
	else
	{
		nillify(poToken);
	}

	return poToken->tokenk;
}

bool tryConsumeToken(Scanner * pScanner, TOKENK tokenkMatch, Token * poToken)
{
	TOKENK tokenk = peekToken(pScanner, poToken);

	if (tokenk == tokenkMatch)
	{
        Verify(read(&pScanner->peekBuffer, poToken));
		return true;
	}

	return false;
}

bool tryConsumeToken(Scanner * pScanner, const TOKENK * aTokenkMatch, int cTokenkMatch, Token * poToken)
{
	TOKENK tokenk = peekToken(pScanner, poToken);

	for (int i = 0; i < cTokenkMatch; i++)
	{
		if (tokenk == aTokenkMatch[i])
		{
            Verify(read(&pScanner->peekBuffer, poToken));
			return true;
		}
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
				if (tryConsume(pScanner, '0', '9', &firstDigit))
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

			case '"':
			{
				GRFERRTOK grferrtok = FERRTOK_UnterminatedString;

				while (!checkEndOfFile(pScanner))
				{
					if (tryConsume(pScanner, '\n'))
					{
						grferrtok |= FERRTOK_MultilineString;
					}
					else if (tryConsume(pScanner, '\"'))
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
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_PlusEqual, poToken);
				else if (tryConsume(pScanner, '+')) makeToken(pScanner, TOKENK_MinusMinus, poToken);
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

				if (tryConsume(pScanner, '='))
				{
					makeToken(pScanner, TOKENK_MinusEqual, poToken);
				}
				else if (tryConsume(pScanner, '0', '9', &firstDigit))
				{
					finishAfterConsumeDigit(pScanner, firstDigit, poToken);
				}
				else if (tryPeek(pScanner, '.'))
				{
					int lookahead = 1;
					if (tryPeek(pScanner, '0', '9', &firstDigit, lookahead))
					{
						consumeChar(pScanner);  // '.'
						consumeChar(pScanner);  // Digit

						finishAfterConsumeDotAndDigit(pScanner, firstDigit, poToken);
					}
				}
				else if (tryConsume(pScanner, '-')) makeToken(pScanner, TOKENK_MinusMinus, poToken);
				else makeToken(pScanner, TOKENK_Minus, poToken);
			} break;

			case '*':
			{
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_StarEqual, poToken);
				else makeToken(pScanner, TOKENK_Star, poToken);
			} break;

			case '%':
			{
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_PercentEqual, poToken);
				else makeToken(pScanner, TOKENK_Percent, poToken);
			} break;

			case '/':
			{
				// HMM: Any value in capturing the comment lexeme and passing it as a token
				//	to the parser? Parser could mostly ignore it, but could be useful for
				//	supporting comments w/ semantics, maybe some form of metaprogramming

				bool isComment = false;
				if (tryConsume(pScanner, '/'))
				{
					isComment = true;

					while (!checkEndOfFile(pScanner))
					{
						if (tryConsume(pScanner, '\n')) break;
						else consumeChar(pScanner);
					}
				}
				else if (tryConsume(pScanner, '*'))
				{
					isComment = true;

					pScanner->cNestedBlockComment++;
					Assert(pScanner->cNestedBlockComment == 1);

					GRFERRTOK grferrtok = FERRTOK_UnterminatedBlockComment;
					while (!checkEndOfFile(pScanner))
					{
						if (tryConsume(pScanner, '/'))
						{
							if (tryConsume(pScanner, '*'))
							{
								pScanner->cNestedBlockComment++;
							}
						}
						else if (tryConsume(pScanner, '*'))
						{
							if (tryConsume(pScanner, '/'))
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
				else if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_SlashEqual, poToken);
				else makeToken(pScanner, TOKENK_Slash, poToken);
			} break;

			case '!':
			{
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_BangEqual, poToken);
				else makeToken(pScanner, TOKENK_Bang, poToken);
			} break;

			case '=':
			{
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_EqualEqual, poToken);
				else makeToken(pScanner, TOKENK_Equal, poToken);
			} break;

			case '<':
			{
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_LesserEqual, poToken);
				else makeToken(pScanner, TOKENK_Lesser, poToken);
			} break;

			case '>':
			{
				if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_GreaterEqual, poToken);
				else makeToken(pScanner, TOKENK_Greater, poToken);
			} break;

			case '|':
			{
				if (tryConsume(pScanner, '|'))
				{
					if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_PipePipeEqual, poToken);
					else makeToken(pScanner, TOKENK_PipePipe, poToken);
				}
				else if (tryConsume(pScanner, '='))
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
				if (tryConsume(pScanner, '|'))
				{
					if (tryConsume(pScanner, '=')) makeToken(pScanner, TOKENK_AmpAmpEqual, poToken);
					else makeToken(pScanner, TOKENK_AmpAmp, poToken);
				}
				else if (tryConsume(pScanner, '='))
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
						if (!(tryConsume(pScanner, '_') || tryConsume(pScanner, 'a', 'z') || tryConsume(pScanner, 'A', 'Z') || tryConsume(pScanner, '0', '9')))
						{
							// Try consume fails into here if end of file

							// Write current lexeme into buffer so that we have a null terminated string to
							//	strncpm with

							int iLexemeBufferPreWrite = pScanner->iLexemeBuffer;
							char * lexeme = pScanner->pLexemeBuffer + pScanner->iLexemeBuffer;
							writeCurrentLexemeIntoBuffer(pScanner);

							int lexemeLength = pScanner->iLexemeBuffer - iLexemeBufferPreWrite;

							// @Slow

							bool isReservedWord = false;
							for (int i = 0; i < g_cReservedWord; i++)
							{
								const ReservedWord * pReservedWord = g_aReservedWord + i;

								if (strncmp(lexeme, pReservedWord->lexeme, lexemeLength) == 0)
								{
									// Move lexeme buffer cursor back, since we don't need to write
									//	known reserved words to it. We can just use the global.

                                    isReservedWord = true;
									pScanner->iLexemeBuffer = iLexemeBufferPreWrite;
									makeTokenWithLexeme(pScanner, pReservedWord->tokenk, pReservedWord->lexeme, poToken);
                                    break;
								}
							}

							if (!isReservedWord)
							{
								makeTokenWithLexeme(pScanner, TOKENK_Identifier, lexeme, poToken);
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

void finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, Token * poToken)
{
	_finishAfterConsumeDigit(pScanner, firstDigit, false, poToken);
}

void finishAfterConsumeDotAndDigit(Scanner * pScanner, char firstDigit, Token * poToken)
{
	_finishAfterConsumeDigit(pScanner, firstDigit, true, poToken);
}

void _finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, bool startsWithDot, Token * poToken)
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

			if (tryConsume(pScanner, 'x'))
			{

				base = 16;
				cDigit = 0;		// Prefixes don't count toward cDigit
			}
			else if (tryConsume(pScanner, 'b'))
			{
				base = 2;
				cDigit = 0;
			}
			else if (tryConsume(pScanner, 'o'))
			{
				base = 8;
				cDigit = 0;
			}
		}
	}

	GRFERRTOK grferrtok = GRFERRTOK_None;
	if (base == 16)
	{
		// Support both lower and upper case hex letters, but you can't
		//  mix and match!
		//  0xABCD works
		//  0xabcd works
		//  0xAbCd does not

		enum CASEK
		{
			CASEK_Tbd,
			CASEK_Lower,
			CASEK_Upper
		};

		CASEK casek = CASEK_Tbd;

		while (true)
		{
			if (tryConsume(pScanner, '.'))
			{
				cDot++;
			}
			else if (tryConsume(pScanner, 'a', 'z'))
			{
				cDigit++;

				if (casek == CASEK_Upper)
				{
					grferrtok |= FERRTOK_IntLiteralHexMixedCase;
				}
				else
				{
					casek = CASEK_Lower;
				}
			}
			else if (tryConsume(pScanner, 'A', 'Z'))
			{
				cDigit++;

				if (casek == CASEK_Lower)
				{
					grferrtok |= FERRTOK_IntLiteralHexMixedCase;
				}
				else
				{
					casek = CASEK_Upper;
				}
			}
			else if (tryConsume(pScanner, '0', '9'))
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
			if (tryConsume(pScanner, '.'))
			{
				cDot++;
			}
			else
			{
				char upperChar = '0' + base - 1;
				if (tryConsume(pScanner, '0', upperChar))
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
		// Note makeToken handles some classes of errors too, like int literals
		//	that are too big for us to actually store!

		pScanner->currentIntLiteralBase = base;

		(cDot > 0) ?
			makeToken(pScanner, TOKENK_FloatLiteral, poToken) :
			makeToken(pScanner, TOKENK_IntLiteral, poToken);
	}
	else
	{
		makeErrorToken(pScanner, grferrtok, poToken);
	}
}

void writeCurrentLexemeIntoBuffer(Scanner * pScanner)
{
	int lexemeSize = pScanner->iText - pScanner->iTextTokenStart;
	char * dest = pScanner->pLexemeBuffer + pScanner->iLexemeBuffer;
	memcpy(
		dest,
		pScanner->pText + pScanner->iTextTokenStart,
		lexemeSize
	);

	// Null terminate

	pScanner->pLexemeBuffer[pScanner->iLexemeBuffer + lexemeSize] = '\0';
	pScanner->iLexemeBuffer += lexemeSize + 1;
}

void makeToken(Scanner * pScanner, TOKENK tokenk, Token * poToken)
{
	char * lexeme = nullptr;

	if (tokenk == TOKENK_Identifier || tokenk == TOKENK_Error || isLiteral(tokenk))
	{
		lexeme = pScanner->pLexemeBuffer + pScanner->iLexemeBuffer;
		writeCurrentLexemeIntoBuffer(pScanner);
	}
#if 1
	else
	{
		// TODO: Delete this #if once I have a better way to test scanner,
		//	since these lexemes are all deterministic... I just didn't feel
		//	like writing a big switch statement!

		lexeme = pScanner->pLexemeBuffer + pScanner->iLexemeBuffer;
		writeCurrentLexemeIntoBuffer(pScanner);
	}
#endif

	makeTokenWithLexeme(pScanner, tokenk, lexeme, poToken);
}

void makeTokenWithLexeme(Scanner * pScanner, TOKENK tokenk, char * lexeme, Token * poToken)
{
	Assert(!pScanner->madeToken);

	poToken->id = pScanner->iToken + 1;		// + 1 to make sure we never have id 0
	poToken->line = pScanner->lineTokenStart;
	poToken->column = pScanner->columnTokenStart;
	poToken->tokenk = tokenk;
	poToken->lexeme = lexeme;

	if (tokenk == TOKENK_BoolLiteral)
	{
		if (lexeme[0] == 't')	poToken->literalBool = true;
		else					poToken->literalBool = false;
	}
	else if (tokenk == TOKENK_IntLiteral)
	{
		int base = pScanner->currentIntLiteralBase;
		char * end;
		long value = strtol(lexeme, &end, base);

		if (errno == ERANGE || value > S32_MAX || value < S32_MIN)
		{
			poToken->tokenk = TOKENK_Error;
			poToken->grferrtok |= FERRTOK_NumberLiteralOutOfRange;
		}
		else if (errno || lexeme == end)
		{
			poToken->tokenk = TOKENK_Error;
			poToken->grferrtok = FERRTOK_Unspecified;
		}
		else
		{
			poToken->intliteralk = (base == 10) ? INTLITERALK_Decimal : (base == 16) ? INTLITERALK_Hexadecimal : (base == 8) ? INTLITERALK_Octal : INTLITERALK_Binary;
			poToken->literalInt = (int)value;
		}
	}
	else if (tokenk == TOKENK_FloatLiteral)
	{
		// TODO:
		//	-Support literals of float types other than f32

		char * end;
		float value = strtof(lexeme, &end);

		if (errno == ERANGE)
		{
			poToken->tokenk = TOKENK_Error;
			poToken->grferrtok = FERRTOK_NumberLiteralOutOfRange;
		}
		else if (errno || lexeme == end)
		{
			poToken->tokenk = TOKENK_Error;
			poToken->grferrtok = FERRTOK_Unspecified;
		}
		else
		{
			poToken->literalFloat = (float)value;
		}
	}

    forceWrite(&pScanner->prevBuffer, *poToken);
	pScanner->madeToken = true;
	pScanner->iToken++;
}

void makeErrorToken(Scanner * pScanner, GRFERRTOK grferrtok, Token * poToken)
{
	Assert(grferrtok);
	makeToken(pScanner, TOKENK_Error, poToken);
	poToken->grferrtok = grferrtok;
}

void makeEofToken(Scanner * pScanner, Token * poToken)
{
	makeTokenWithLexeme(pScanner, TOKENK_Eof, nullptr, poToken);
}

char consumeChar(Scanner * pScanner)
{
	if (checkEndOfFile(pScanner)) return '\0';	 // This check might be redundant/unnecessary

	char c = pScanner->pText[pScanner->iText];
	pScanner->iText++;
	pScanner->column++;

	if (c == '\n')
	{
		pScanner->line++;
		pScanner->column = 1;
	}

	return c;
}

bool tryConsume(Scanner * pScanner, char expected)
{
	if (checkEndOfFile(pScanner)) return false;
	if (pScanner->pText[pScanner->iText] != expected) return false;

	consumeChar(pScanner);

	return true;
}

bool tryConsume(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch)
{
	if (checkEndOfFile(pScanner)) return false;
	if (rangeMin > rangeMax) return false;

	char c = pScanner->pText[pScanner->iText];
	if (c < rangeMin || c > rangeMax) return false;

	consumeChar(pScanner);

	if (poMatch) *poMatch = c;

	return true;
}

bool tryPeek(Scanner * pScanner, char expected, int lookahead)
{
	if (checkEndOfFile(pScanner, lookahead)) return false;
	if (pScanner->pText[pScanner->iText + lookahead] != expected) return false;

	return true;
}

bool tryPeek(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch, int lookahead)
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
	pScanner->lineTokenStart = pScanner->line;
	pScanner->columnTokenStart = pScanner->column;
	pScanner->madeToken = false;
}

bool checkEndOfFile(Scanner * pScanner, int lookahead)
{
	if (!pScanner->pText)
	{
		pScanner->scanexitk = SCANEXITK_ReadNullTerminator;
		return true;
	}

	if (pScanner->iText + lookahead >= pScanner->textSize)
	{
		if (lookahead == 0) pScanner->scanexitk = SCANEXITK_ReadAllBytes;

		return true;
	}

	return false;
}
