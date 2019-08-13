#include "common_macro.h"
#include "scan.h"

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
    while (!checkEndOfFile(pScanner))
    {
        onStartToken(pScanner);
        char c = consumeChar(pScanner);
        int iTokenStart = pScanner->iToken;

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
                makeToken(pScanner, TOKENK_Dot, poToken);
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
                makeToken(pScanner, TOKENK_Quote, poToken);
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
                if (tryMatch(pScanner, '/'))
                {
                    while (!checkEndOfFile(pScanner))
                    {
                        if (tryMatch(pScanner, '\n')) break;
                        else consumeChar(pScanner);
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

            case '\n':
            {
                onConsumeNewline(pScanner);
            } break;

            case ' ':
            case '\r':
            case '\t':
                break;

            default:
            {
                // TODO:
                //  -number literal
                //  -identifier
                //  -reserved word
                //  -report error "unrecognized character"

                poToken->tokenk = TOKENK_Nil;
            } break;
        }

        if (pScanner->iToken > iTokenStart)
        {
            return poToken->tokenk;
        }
    }

    return TOKENK_Nil;
}

void makeToken(Scanner * pScanner, TOKENK tokenk, Token * poToken)
{
    // Copy lexeme into lexeme buffer

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

    poToken->id = pScanner->iToken;
    poToken->line = pScanner->lineTokenStart;
    poToken->column = pScanner->columnTokenStart;
    poToken->tokenk = tokenk;
    poToken->lexeme = dest;

    pScanner->iToken++;
}

char consumeChar(Scanner * pScanner)
{
    // HMM:
    //  -Do we want to do any end of file shenanigans here?
    //  -Do we want to check for \n and bump line?
    //
    //  "skip" as opposed to "consume" suggests that it is fine to just blindly skip

    if (checkEndOfFile(pScanner)) return '\0';   // This check might be redundant/unnecessary

    char c = pScanner->pText[pScanner->iText];
    pScanner->iText++;
    pScanner->column++;

    if (c == '\n') onConsumeNewline(pScanner);

    return c;
}

bool tryMatch(Scanner * pScanner, char expected, bool consumeIfMatch)
{
    if (checkEndOfFile(pScanner)) return false;
    if (pScanner->pText[pScanner->iText] != expected) return false;

    if (consumeIfMatch)
    {
        pScanner->iText++;

        if (expected = '\n') onConsumeNewline(pScanner);
    }

    return true;
}

void onConsumeNewline(Scanner * pScanner)
{
    pScanner->line++;
    pScanner->column = 1;
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


