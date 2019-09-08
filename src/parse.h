#pragma once

#include "als.h"

#include "ast.h"
#include "token.h"

struct Scanner;

struct BinopInfo
{
	static constexpr int s_cTokenMatchMax = 4;	// Bump as needed

	int precedence;
	TOKENK aTokenkMatch[s_cTokenMatchMax];
	int cTokenMatch;

    // NOTE (andrews): All implemented binops are left-associative. Assignment is not yet implemented, and I believe
    //  it is the only one that will likely be right associative.
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



	// Error and error recovery

	// bool isPanicMode = false;
    bool hadError = false;
};



// Public

bool init(Parser * pParser, Scanner * pScanner);
AstNode * parseProgram(Parser * pParser);


// HMM: Are these really public?

void parseStmts(Parser * pParser, DynamicArray<AstNode *> * poAPNodes);
AstNode * parseStmt(Parser * pParser);
AstNode * parseExpr(Parser * pParser);


// Internal

// Node allocation

AstNode * astNew(Parser * pParser, ASTK astk, int line);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * pChild0=nullptr, AstNode * pChild1=nullptr);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * aPChildren[], uint cPChildren);

// Error handling

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover);
bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatched);

// STMT

AstNode * parseStmt(Parser * pParser);
AstNode * parseExprStmt(Parser * pParser);

// EXPR

AstNode * parseExpr(Parser * pParser);
AstNode * parseAssignStmt(Parser * pParser); // TODO: roll this into parse op?
AstNode * parseBinop(Parser * pParser, const BinopInfo & op);
AstNode * parseUnopPre(Parser * pParser);
AstNode * parsePrimary(Parser * pParser);

AstNode * finishParsePrimary(Parser * pParser, AstNode * pLhsExpr);

Token * ensurePendingToken(Parser * pParser);

// Only claimed tokens will stay allocated by the parser. If you merely peek, the
//  node's memory will be overwritten next time you consume the pending token.

Token * peekPendingToken(Parser * pParser);
Token * claimPendingToken(Parser * pParser);


// Debug

#if DEBUG

void debugPrintAst(const AstNode & pRoot);
void debugPrintSubAst(const AstNode & pNode, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

#endif
