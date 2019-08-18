#include "parse.h"

#include "scan.h"

TOKENK nextToken(Parser * pParser, Token * poToken)
{
    if (!isEmpty(&pParser->rbufLookahead))
    {
        read(&pParser->rbufLookahead, poToken);
        return poToken->tokenk;
    }
    else
    {
        return nextToken(pParser->pScanner, poToken);
	}
}
