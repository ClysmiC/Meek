#pragma once

#include "als.h"

struct AstNode;

// Resolve identifiers, do type inference + checking

struct ResolvePass
{
    int mostRecentSymbTableOrder = 0;
};

void doResolvePass(ResolvePass * pPass, AstNode * pNode);