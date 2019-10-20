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
	// Scanner

	Scanner * pScanner = nullptr;

	// Allocators

	DynamicPoolAllocator<AstNode> astAlloc;
	DynamicPoolAllocator<Token> tokenAlloc;
	DynamicPoolAllocator<Type> typeAlloc;


	// Scope

    scopeid scopeidNext = gc_globalScopeid + 1;
    Stack<Scope> scopeStack;

	// Symbol Table

	SymbolTable symbolTable;

	// Node construction / bookkeeping

	uint iNode = 0;				// Becomes node's id
	Token * pPendingToken = nullptr;
	DynamicArray<AstNode *> astNodes;		// Might be able to replace this with just the root node. Although the root node will need to be a type that
											//	is essentially just a dynamic array.


	// Error and error recovery


	// bool isPanicMode = false;
    bool hadError = false;
};



// Might want to move this out of parse.h as this will probably be reusable elsewhere

enum EXPECTK
{
	EXPECTK_Required,
	EXPECTK_Forbidden,
	EXPECTK_Optional
};

enum FUNCHEADERK
{
	FUNCHEADERK_Defn,
	FUNCHEADERK_VarType,
	FUNCHEADERK_Literal
};

enum PARAMK
{
    PARAMK_Param,
    PARAMK_Return
};


// Public

// NOTE: Any memory allocated by the parser is never freed, since it is all put into the AST and we really
//	have no need to want to free AST nodes. If we ever find that need then we can offer an interface to
//	deallocate stuff through the pool allocator's deallocate functions.

bool init(Parser * pParser, Scanner * pScanner);
AstNode * parseProgram(Parser * pParser, bool * poSuccess);


// HMM: Are these really public?

void parseStmts(Parser * pParser, DynamicArray<AstNode *> * poAPNodes); // TODO: I think this is only called in one place so just inline it.
AstNode * parseStmt(Parser * pParser, bool isDoStmt=false);
AstNode * parseExpr(Parser * pParser);


// Internal

// Scope management

void pushScope(Parser * pParser, SCOPEK scopek);
Scope peekScope(Parser * pParser);
Scope peekScopePrev(Parser * pParser);
Scope popScope(Parser * pParser);


// Node allocation

AstNode * astNew(Parser * pParser, ASTK astk, int line);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * pChild0=nullptr, AstNode * pChild1=nullptr, AstNode * pChild2=nullptr);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * aPChildren[], uint cPChildren);
AstNode * astNewErrMoveChildren(Parser * pParser, ASTK astkErr, int line, DynamicArray<AstNode *> * papChildren);

// Error handling

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover);
bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatched=nullptr);

// STMT

AstNode * parseExprStmtOrAssignStmt(Parser * pParser);
AstNode * parseStructDefnStmt(Parser * pParser);
AstNode * parseVarDeclStmt(Parser * pParser, EXPECTK expectkName=EXPECTK_Required, EXPECTK expectkInit=EXPECTK_Optional, EXPECTK expectkSemicolon=EXPECTK_Required);
AstNode * parseIfStmt(Parser * pParser);
AstNode * parseWhileStmt(Parser * pParser);
AstNode * parseDoStmtOrBlockStmt(Parser * pParser, bool pushPopScopeBlock=true);
AstNode * parseBlockStmt(Parser * pParser, bool pushPopScope=true);
AstNode * parseReturnStmt(Parser * pParser);
AstNode * parseBreakStmt(Parser * pParser);
AstNode * parseContinueStmt(Parser * pParser);

// EXPR

AstNode * parseExpr(Parser * pParser);
AstNode * parseBinop(Parser * pParser, const BinopInfo & op);
AstNode * parseUnopPre(Parser * pParser);
AstNode * parsePrimary(Parser * pParser);
AstNode * parseVarExpr(Parser * pParser, AstNode * pOwnerExpr);
AstNode * parseLiteralExpr(Parser * pParser, bool mustBeIntLiteralk=false);

AstNode * finishParsePrimary(Parser * pParser, AstNode * pLhsExpr);


// Any subscript expressions are appended to papNodeChildren for bookkeeping.
// Any errors are also appended to papNodeChildren

bool tryParseType(
	Parser * pParser,
	Type ** ppoType,
	DynamicArray<AstNode *> * papNodeChildren);

bool tryParseFuncDefnStmtOrLiteralExpr(Parser * pParser, FUNCHEADERK funcheaderk, AstNode ** ppoNode);


// Func headers in function definitions and literals are full var decls for params/returns
// NOTE: Errors are embedded in the pap's for tryParseFuncDefnOrLiteralHeader and tryParseFuncDefnOrLiteralHeaderParamList

bool tryParseFuncDefnOrLiteralHeader(
	Parser * pParser,
	FUNCHEADERK funcheaderk,
	DynamicArray<AstNode *> * papParamVarDecls,
	DynamicArray<AstNode *> * papReturnVarDecls,
	ScopedIdentifier * poDefnIdent=nullptr);

bool tryParseFuncDefnOrLiteralHeaderParamList(Parser * pParser, FUNCHEADERK funcheaderk, PARAMK paramk, DynamicArray<AstNode *> * papParamVarDecls);


// Func headers as the type of a variable only require type information. Variable names are optional and ignored.
//	Initializers are disallowed (they don't make any sense when we are talking about *types*).
// TODO: Consider combining with tryParseFuncDefnOrLiteralHeader... the problem is that they
//	have different kinds of out parameters, despite the parsing pattern being very similar!

bool tryParseFuncHeaderTypeOnly(
	Parser * pParser,
	FuncType * poFuncType,
	AstErr ** ppoErr
);


// NOTE: This moves the children into the AST

AstNode * handleScanOrUnexpectedTokenkErr(Parser * pParser, DynamicArray<AstNode *> * papChildren = nullptr);


// Only claimed tokens will stay allocated by the parser. If you merely peek, the
//  node's memory will be overwritten next time you consume a token.

Token * ensurePendingToken(Parser * pParser);
Token * claimPendingToken(Parser * pParser);
Token * ensureAndClaimPendingToken(Parser * pParser);

inline Type * newType(Parser * pParser)
{
	return allocate(&pParser->typeAlloc);
}

inline void releaseType(Parser * pParser, Type * pParseType)
{
	release(&pParser->typeAlloc, pParseType);
}

inline void releaseToken(Parser * pParser, Token * pToken)
{
	release(&pParser->tokenAlloc, pToken);
}

// Debug
// TODO: Move this and implementation to ast_print.h/.cpp

#if DEBUG

void debugPrintAst(const AstNode & pRoot);
void debugPrintSubAst(const AstNode & pNode, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

#endif
