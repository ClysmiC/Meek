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
    bool startsWithDot = false;
    int iTokenStart = pScanner->iToken;
    bool startNewToken = true;

    while (!checkEndOfFile(pScanner))
    {
        if (startNewToken)
        {
            onStartToken(pScanner);
        }

        startNewToken = false;

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
                if (tryMatch(pScanner, '0', '9', nullptr, SCANMATCHK_Peek))
                {
                    startsWithDot = true;
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
                bool isTerminated = false;
                bool multilineString = false;

                while (!checkEndOfFile(pScanner))
                {
                    if (tryMatch(pScanner, '\n'))
                    {
                        multilineString = true;
                    }
                    else if (tryMatch(pScanner, '\"'))
                    {
                        if (!multilineString)
                        {
                            makeToken(pScanner, TOKENK_StringLiteral, poToken);
                        }

                        isTerminated = true;
                        break;
                    }
                    else
                    {
                        consumeChar(pScanner);
                    }
                }

                if (!isTerminated)
                {
                    makeErrorToken(pScanner, ERRORTOKENK_UnterminatedString, poToken);
                }
                else if (multilineString)
                {
                    makeErrorToken(pScanner, ERRORTOKENK_MultilineString, poToken);
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
                //  or should we just call it a unary operator in that case.

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
                //  to the parser? Parser could mostly ignore it, but could be useful for
                //  supporting comments w/ semantics, maybe some form of metaprogramming

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

                    bool isTerminated = false;
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
                                    isTerminated = true;
                                    break;
                                }
                            }   
                        }
                        else
                        {
                            consumeChar(pScanner);
                        }
                    }

                    if (!isTerminated)
                    {
                        makeErrorToken(pScanner, ERRORTOKENK_UnterminatedBlockComment, poToken);
                    }
                }
                else if (tryMatch(pScanner, '=')) makeToken(pScanner, TOKENK_SlashEqual, poToken);
                else makeToken(pScanner, TOKENK_Slash, poToken);

                if (isComment)
                {
                    startNewToken = true;
                }
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
            {
                startNewToken = true;
            } break;

            default:
            {
                if (isDigit(c))
                {
                    bool hasDot = startsWithDot;
                    bool multipleDotError = false;

                    while (true)
                    {
                        if (tryMatch(pScanner, '.'))
                        {
                            if (hasDot)
                            {
                                multipleDotError = true;
                            }
                            else
                            {
                                hasDot = true;
                            }
                        }
                        else if (!tryMatch(pScanner, '0', '9'))
                        {
                            // Try match fails into here if end of file

                            break;


                        }
                    }

                    if (multipleDotError)
                    {
                        makeErrorToken(pScanner, ERRORTOKENK_FloatLiteralMultipleDecimals, poToken);
                    }
                    else
                    {
                        (hasDot) ?
                            makeToken(pScanner, TOKENK_FloatLiteral, poToken) :
                            makeToken(pScanner, TOKENK_IntLiteral, poToken);
                    }
                }
                else if (isLetterOrUnderscore(c))
                {
                    while (true)
                    {
                        if (!(tryMatch(pScanner, '_') || tryMatch(pScanner, 'a', 'z') || tryMatch(pScanner, 'A', 'Z') || tryMatch(pScanner, '0', '9')))
                        {
                            // Try match fails into here if end of file

                            // Write current lexeme into buffer so that we have a null terminated string to
                            //  strncpm with

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
                                    //  known reserved words to it. We can just use the global.

                                    pScanner->iLexemeBuffer = iLexemeBufferPreWrite;
                                    makeTokenWithLexeme(pScanner, pReservedWord->tokenk, pReservedWord->lexeme, poToken);
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
                    makeErrorToken(pScanner, ERRORTOKENK_InvalidCharacter, poToken);
                }

            } break;
        }

        if (pScanner->iToken > iTokenStart)
        {
            return poToken->tokenk;
        }
    }

    makeEofToken(pScanner, poToken);
    return poToken->tokenk;
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
        // TODO: Delete this #if once I have a better way to test scanner

        lexeme = pScanner->pLexemeBuffer + pScanner->iLexemeBuffer;
        writeCurrentLexemeIntoBuffer(pScanner);   
    }
#endif

    makeTokenWithLexeme(pScanner, tokenk, lexeme, poToken);
}

void makeTokenWithLexeme(Scanner * pScanner, TOKENK tokenk, char * lexeme, Token * poToken)
{
    poToken->id = pScanner->iToken;
    poToken->line = pScanner->lineTokenStart;
    poToken->column = pScanner->columnTokenStart;
    poToken->tokenk = tokenk;
    poToken->lexeme = lexeme;

    if (tokenk == TOKENK_BoolLiteral)
    {
        if (lexeme[0] == 't')    poToken->literalBool = true;
        else                    poToken->literalBool = false;
    }
    else if (tokenk == TOKENK_IntLiteral)
    {
        // TODO:
        //  -Support 0x, 0o, and 0b prefixes for hex, octal, binary
        //  -Support literals of int types other than s32

        char * end;
        long value = strtol(lexeme, &end, 10);

        if (errno == ERANGE || value > S32_MAX || value < S32_MIN)
        {
            poToken->tokenk = TOKENK_Error;
            poToken->errortokenk = ERRORTOKENK_IntLiteralOutOfRange;
        }
        else if (errno || lexeme == end)
        {
            poToken->tokenk = TOKENK_Error;
            poToken->errortokenk = ERRORTOKENK_Unspecified;
        }
        else
        {
            poToken->literalInt = (int)value;
        }
    }
    else if (tokenk == TOKENK_FloatLiteral)
    {
        // TODO:
        //  -Support literals of float types other than f32

        char * end;
        float value = strtof(lexeme, &end);

        if (errno == ERANGE)
        {
            poToken->tokenk = TOKENK_Error;
            poToken->errortokenk = ERRORTOKENK_FloatLiteralOutOfRange;   
        }
        else if (errno || lexeme == end)
        {
            poToken->tokenk = TOKENK_Error;
            poToken->errortokenk = ERRORTOKENK_Unspecified;
        }
        else
        {
            poToken->literalFloat = (float)value;
        }
    }

    pScanner->iToken++;
}

void makeErrorToken(Scanner * pScanner, ERRORTOKENK errortokenk, Token * poToken)
{
    makeToken(pScanner, TOKENK_Error, poToken);
    poToken->errortokenk = errortokenk;
}

void makeEofToken(Scanner * pScanner, Token * poToken)
{
    makeTokenWithLexeme(pScanner, TOKENK_Eof, nullptr, poToken);
}

char consumeChar(Scanner * pScanner)
{
    if (checkEndOfFile(pScanner)) return '\0';   // This check might be redundant/unnecessary

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


