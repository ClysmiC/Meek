#pragma once

#include "als.h"

#include "common.h"
#include "token.h"

struct Scanner;

struct Parser
{
	Scanner * pScanner = nullptr;
};



// Public

void init(Parser * pParser);

