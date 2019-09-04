#pragma once

#include "als.h"

#include "ast.h"
#include "token.h"

struct Scanner;

struct ParseOp
{
	static constexpr int s_cTokenMatchMax = 4;	// Bump as needed

	int precedence;
	TOKENK aTokenkMatch[s_cTokenMatchMax];
	int cTokenMatch;
};

struct Parser
{
	Scanner * pScanner = nullptr;

	DynamicPoolAllocator<AstNode> astAlloc;
	DynamicPoolAllocator<Token> tokenAlloc;

	uint iNode = 0;		// Becomes node's id

	Token * pPendingToken = nullptr;

	// Might be able to replace this with just the root node. Although the root node will need to be a type that
	//	is essentially just a dynamic array.

	DynamicArray<AstNode *> astNodes;
};



// Public

bool init(Parser * pParser, Scanner * pScanner);
AstNode * parse(Parser * pParser);

AstNode * parseStmt(Parser * pParser);

AstNode * parseExpr(Parser * pParser);


// Internal


StaticAssert(AstErr::s_cChildrenMax == 2); // Keep the # of error children in sync!
AstNode * astNewErr(Parser * pParser, ASTK astkErr, AstNode * pChild0=nullptr, AstNode * pChild1=nullptr);

AstNode * astNew(Parser * pParser, ASTK astk);

AstNode * parseStmt(Parser * pParser);
AstNode * parseExpr(Parser * pParser);
AstNode * parseAssignStmt(Parser * pParser); // TODO: roll this into parse op?
AstNode * parseOp(Parser * pParser, const ParseOp & op);
AstNode * parsePrimary(Parser * pParser);

AstNode * finishParsePrimary(Parser * pParser, AstNode * pLhsExpr);

Token * ensurePendingToken(Parser * pParser);
Token * claimPendingToken(Parser * pParser);


// Debug

#if DEBUG

void debugPrintAst(const AstNode & pRoot);
void debugPrintSubAst(const AstNode & pNode);

#endif
