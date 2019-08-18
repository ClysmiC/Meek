#pragma once

#include "common.h"
#include "token.h"

struct Scanner;

struct Parser
{
	Scanner * pScanner = nullptr;

	RingBuffer<Token, 16> rbufLookahead;
	int cLookaheadCached = 0;
};

TOKENK nextToken(Parser * pParser, Token * poToken);
