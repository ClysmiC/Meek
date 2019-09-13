#pragma once

#include "als.h"

#include "ast.h"
#include "token.h"
#include "type.h"

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
	DynamicPoolAllocator<ParseType> parseTypeAlloc;

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
AstNode * parseProgram(Parser * pParser, bool * poSuccess);


// HMM: Are these really public?

void parseStmts(Parser * pParser, DynamicArray<AstNode *> * poAPNodes);
AstNode * parseStmt(Parser * pParser);
AstNode * parseExpr(Parser * pParser);


// Internal

// Node allocation

AstNode * astNew(Parser * pParser, ASTK astk, int line);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * pChild0=nullptr, AstNode * pChild1=nullptr);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * aPChildren[], uint cPChildren);
AstNode * astNewErrMoveChildren(Parser * pParser, ASTK astkErr, int line, DynamicArray<AstNode *> * papChildren);

// Error handling

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover);
bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatched=nullptr);

// STMT

AstNode * parseStmt(Parser * pParser);
AstNode * parseExprStmt(Parser * pParser);
AstNode * parseStructDefnStmt(Parser * pParser);
AstNode * parseVarDeclStmt(Parser * pParser);

// EXPR

AstNode * parseExpr(Parser * pParser);
AstNode * parseAssignStmt(Parser * pParser); // TODO: roll this into parse op?
AstNode * parseBinop(Parser * pParser, const BinopInfo & op);
AstNode * parseUnopPre(Parser * pParser);
AstNode * parsePrimary(Parser * pParser);

AstNode * finishParsePrimary(Parser * pParser, AstNode * pLhsExpr);

// NOTE: This moves the children into the AST
AstNode * handleScanOrUnexpectedTokenkErr(Parser * pParser, DynamicArray<AstNode *> * papChildren = nullptr);

// Only claimed tokens will stay allocated by the parser. If you merely peek, the
//  node's memory will be overwritten next time you consume a token.

Token * ensurePendingToken(Parser * pParser);
Token * claimPendingToken(Parser * pParser);
Token * ensureAndClaimPendingToken(Parser * pParser);

inline ParseType * newParseType(Parser * pParser)
{
	return allocate(&pParser->parseTypeAlloc);
}


// Debug

#if DEBUG

void debugPrintAst(const AstNode & pRoot);
void debugPrintSubAst(const AstNode & pNode, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

#endif
