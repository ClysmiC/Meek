#pragma once

#include "als.h"

#include "ast.h"
#include "token.h"

// TODO: replace this with bucket array or own dynamic array
#include <vector>

struct Scanner;

struct Parser
{
	Scanner * pScanner = nullptr;

	DynamicPoolAllocator<AstNode> astAlloc;
	DynamicPoolAllocator<Token> tokenAlloc;

	uint iNode = 0;		// Becomes node's id

	Token * pPendingToken = nullptr;
	std::vector<AstNode *> apAstNodes; // Might be able to replace this with just the root node. Although the root node will need to have this same vector
};



// Public

void init(Parser * pParser, Scanner * pScanner);
AstNode * parse(Parser * pParser);

AstNode * parseStmt(Parser * pParser);

AstNode * parseExpr(Parser * pParser);


// Internal

void astNew(Parser * pParser, ASTK astk);
