#include "scan.h"

#include "common.h"

#include <stdlib.h>

bool init(Scanner * pScanner, char * pText, int textSize, char * pLexemeBuffer, int lexemeBufferSize)
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
				if (tryMatch(pScanner, '0', '9', &firstDigit, SCANMATCHK_Peek))
				{
					finishAfterConsumeDotDigit(pScanner, firstDigit, poToken);
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
					if (tryMatch(pScanner, '\n'))
					{
						grferrtok |= FERRTOK_MultilineString;
					}
					else if (tryMatch(pScanner, '\"'))
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
				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_PlusEqual, poToken);
				else makeToken(pScanner, TOKENK_Plus, poToken);
			} break;

			case '-':
			{
				// HMM: Should we count the negative sign as part of a int/float literal,
				//	or should we just call it a unary operator in that case.

				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_MinusEqual, poToken);
				else makeToken(pScanner, TOKENK_Minus, poToken);
			} break;

			case '*':
			{
				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_StarEqual, poToken);
				else makeToken(pScanner, TOKENK_Star, poToken);
			} break;

			case '/':
			{
				// HMM: Any value in capturing the comment lexeme and passing it as a token
				//	to the parser? Parser could mostly ignore it, but could be useful for
				//	supporting comments w/ semantics, maybe some form of metaprogramming

				bool isComment = false;
				if (tryMatch(pScanner, '/'))
				{
					isComment = true;

					while (!checkEndOfFile(pScanner))
					{
						if (tryMatch(pScanner, '\n')) break;
						else consumeChar(pScanner);
					}
				}
				else if (tryMatch(pScanner, '*'))
				{
					isComment = true;

					pScanner->cNestedBlockComment++;
					Assert(pScanner->cNestedBlockComment == 1);

					GRFERRTOK grferrtok = FERRTOK_UnterminatedBlockComment;
					while (!checkEndOfFile(pScanner))
					{
						if (tryMatch(pScanner, '/'))
						{
							if (tryMatch(pScanner, '*'))
							{
								pScanner->cNestedBlockComment++;
							}
						}
						else if (tryMatch(pScanner, '*'))
						{
							if (tryMatch(pScanner, '/'))
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
				else if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_SlashEqual, poToken);
				else makeToken(pScanner, TOKENK_Slash, poToken);
			} break;

			case '!':
			{
				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_BangEqual, poToken);
				else makeToken(pScanner, TOKENK_Bang, poToken);
			} break;

			case '=':
			{
				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_EqualEqual, poToken);
				else makeToken(pScanner, TOKENK_Equal, poToken);
			} break;

			case '<':
			{
				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_LesserEqual, poToken);
				else makeToken(pScanner, TOKENK_Lesser, poToken);
			} break;

			case '>':
			{
				if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_GreaterEqual, poToken);
				else makeToken(pScanner, TOKENK_Greater, poToken);
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
						if (!(tryMatch(pScanner, '_') || tryMatch(pScanner, 'a', 'z') || tryMatch(pScanner, 'A', 'Z') || tryMatch(pScanner, '0', '9')))
						{
							// Try match fails into here if end of file

							// Write current lexeme into buffer so that we have a null terminated string to
							//	strncpm with

							int iLexemeBufferPreWrite = pScanner->iLexemeBuffer;
							char * lexeme = pScanner->pLexemeBuffer + pScanner->iLexemeBuffer;
							writeCurrentLexemeIntoBuffer(pScanner);

							int lexemeLength = pScanner->iLexemeBuffer - iLexemeBufferPreWrite;

							// @Slow

							bool isReservedWord = false;
							for (int i = 0; i < g_reservedWordCount; i++)
							{
								ReservedWord * pReservedWord = g_reservedWords + i;

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

void finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, Token * poToken)
{
	_finishAfterConsumeDigit(pScanner, firstDigit, false, poToken);
}

void finishAfterConsumeDotDigit(Scanner * pScanner, char firstDigit, Token * poToken)
{
	_finishAfterConsumeDigit(pScanner, firstDigit, true, poToken);
}

void _finishAfterConsumeDigit(Scanner * pScanner, char firstDigit, bool startsWithDot, Token * poToken)
{
	int cDot = (startsWithDot) ? 1 : 0;
	int cDigit = 0;		// prefixes like 0x don't count toward cDigit
	int base = 10;

	if (!startsWithDot)
	{
		if (tryMatch(pScanner, '0'))
		{
            // Right now only supporting lower case prefix.
            //  0xABCD works
            //  0XABCD does not

			if (tryMatch(pScanner, 'x'))
			{
				base = 16;
			}
			else if (tryMatch(pScanner, 'b'))
			{
				base = 2;
			}
			else if (tryMatch(pScanner, 'o'))
			{
				base = 8;
			}
			else
			{
				cDigit++;	// Base 10, so leading 0 is actually part of the number
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
			if (tryMatch(pScanner, '.'))
			{
				cDot++;
			}
			else if (tryMatch(pScanner, 'a', 'z'))
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
			else if (tryMatch(pScanner, 'A', 'Z'))
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
			else if (tryMatch(pScanner, '0', '9'))
			{
				cDigit++;
			}
			else
			{
				// Try match fails into here if end of file

				break;
			}
		}
	}
	else
	{
		while (true)
		{
			if (tryMatch(pScanner, '.'))
			{
				cDot++;
			}
			else
			{
				char upperChar = '0' + base - 1;
				if (tryMatch(pScanner, '0', upperChar))
				{
					cDigit++;
				}
				else
				{
					// Try match fails into here if end of file

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

	poToken->id = pScanner->iToken;
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

	pScanner->madeToken = true;
	pScanner->iToken++;
}

void makeErrorToken(Scanner * pScanner, GRFERRTOK grferrtok, Token * poToken)
{
	if (grferrtok)
	{
		makeToken(pScanner, TOKENK_Error, poToken);
		poToken->grferrtok = grferrtok;
	}
	else
	{
		Assert(false);
		poToken->tokenk = TOKENK_Nil;
	}
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

bool tryMatch(Scanner * pScanner, char expected, SCANMATCHK scanmatchk)
{
	if (checkEndOfFile(pScanner)) return false;
	if (pScanner->pText[pScanner->iText] != expected) return false;

	if (scanmatchk == SCANMATCHK_Consume)
	{
		consumeChar(pScanner);
	}

	return true;
}

bool tryMatch(Scanner * pScanner, char rangeMin, char rangeMax, char * poMatch, SCANMATCHK scanmatchk)
{
	if (checkEndOfFile(pScanner)) return false;
	if (rangeMin > rangeMax) return false;

	char c = pScanner->pText[pScanner->iText];

	if (c < rangeMin || c > rangeMax) return false;

	if (poMatch) *poMatch = c;

	if (scanmatchk == SCANMATCHK_Consume)
	{
		 consumeChar(pScanner);
	}

	return true;
}

void onStartToken(Scanner * pScanner)
{
	pScanner->iTextTokenStart = pScanner->iText;
	pScanner->lineTokenStart = pScanner->line;
	pScanner->columnTokenStart = pScanner->column;
	pScanner->madeToken = false;
}

bool checkEndOfFile(Scanner * pScanner)
{
	if (!pScanner->pText)
	{
		pScanner->scanexitk = SCANEXITK_ReadNullTerminator;
		return true;
	}

	if (pScanner->iText >= pScanner->textSize)
	{
		pScanner->scanexitk = SCANEXITK_ReadAllBytes;
		return true;
	}

	return false;
}
