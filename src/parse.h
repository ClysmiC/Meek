#pragma once

#include "als.h"

#include "ast.h"
#include "ast_decorate.h"
#include "token.h"
#include "type.h"

struct Scanner;

struct BinopInfo
{
	static constexpr int s_cTokenMatchMax = 4;	// Bump as needed

	int precedence;
	TOKENK aTokenkMatch[s_cTokenMatchMax];
	int cTokenMatch;
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

	SCOPEID scopeidNext = SCOPEID_UserDefinedStart;
	Stack<Scope> scopeStack;

	// TODO: Move all of these tables out to some god-struct and make these just pointers. No reason for the parser to own this stuff.

	// Symbol Table

	SymbolTable symbTable;

	// Type Table

	TypeTable typeTable;

	// AST decoration

	AstDecorations astDecs;

	// Node construction / bookkeeping

	uint iNode = 0;				// Becomes node's id
	Token * pPendingToken = nullptr;


	// Error and error recovery

	DynamicArray<AstNode *> apErrorNodes;
};



// Might want to move this out of parse.h as this will probably be reusable elsewhere

enum EXPECTK
{
	EXPECTK_Required,
	EXPECTK_Forbidden,
	EXPECTK_Optional
};

// See function_parsing.meek for parsing rules for each FUNCHEADERK

enum FUNCHEADERK
{
	FUNCHEADERK_Defn,
	FUNCHEADERK_Literal,
	FUNCHEADERK_Type,
	FUNCHEADERK_SymbolExpr		// Corresponds to SYMBEXPRK_Func
};

enum PARAMK
{
	PARAMK_Param,
	PARAMK_Return
};


// Public

// NOTE: Any memory by the parser is never freed, since it is all put into the AST and we really
//	have no need to want to free AST nodes. If we ever find that need then we can offer an interface to
//	deallocate stuff through the pool allocator's deallocate functions.

void init(Parser * pParser, Scanner * pScanner);
AstNode * parseProgram(Parser * pParser, bool * poSuccess);
void reportScanAndParseErrors(const Parser & parser);



// Internal

// Scope management

void pushScope(Parser * pParser, SCOPEK scopek);
Scope peekScope(Parser * pParser);
Scope peekScopePrev(Parser * pParser);
Scope popScope(Parser * pParser);


// Node allocation

AstNode * astNew(Parser * pParser, ASTK astk, StartEndIndices startEnd);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, AstNode * pChild0=nullptr, AstNode * pChild1=nullptr, AstNode * pChild2=nullptr);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, AstNode * aPChildren[], uint cPChildren);
AstNode * astNewErrMoveChildren(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, DynamicArray<AstNode *> * papChildren);

// Error handling

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover);
bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatched=nullptr);


// STMT

enum PARSESTMTK
{
	PARSESTMTK_DoStmt,
	PARSESTMTK_TopLevelStmt,
	PARSESTMTK_Stmt
};

AstNode * parseStmt(Parser * pParser, PARSESTMTK parsestmtk=PARSESTMTK_Stmt);
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
AstNode * parseFuncDefnStmt(Parser * pParser);

// EXPR

AstNode * parseExpr(Parser * pParser);
AstNode * parseExpr(Parser * pParser);
AstNode * parseBinop(Parser * pParser, const BinopInfo & op);
AstNode * parseUnopPre(Parser * pParser);
AstNode * parsePrimary(Parser * pParser);
AstNode * parseVarOrMemberVarSymbolExpr(Parser * pParser, NULLABLE AstNode * pMemberOwnerExpr);
AstNode * parseFuncSymbolExpr(Parser * pParser);
AstNode * parseLiteralExpr(Parser * pParser, bool mustBeIntLiteralk=false);
AstNode * parseFuncLiteralExpr(Parser * pParser);

// Helpers

AstNode * parseFuncDefnStmtOrLiteralExpr(Parser * pParser, FUNCHEADERK funcheaderk);
AstNode * finishParsePrimary(Parser * pParser, AstNode * pLhsExpr);

// Type parsing helpers

struct ParseTypeResult
{
	bool success;
	union
	{
		TypePendingResolve * pTypePendingResolve;		// success
		AstErr * pErr;									// !success
	};
};

ParseTypeResult tryParseType(Parser * pParser);

struct ParseFuncHeaderParam
{
	FUNCHEADERK funcheaderk;

	union
	{
		struct _FuncHeaderDefn				// FUNCHEADERK_Defn
		{
			AstFuncDefnStmt * pioNode;
		} paramDefn;

		struct _FuncHeaderLiteral			// FUNCHEADERK_Literal
		{
			AstFuncLiteralExpr * pioNode;
		} paramLiteral;

		struct _FuncHeaderType				// FUNCHEADERK_Type
		{
			FuncType * pioFuncType;
		} paramType;

		struct _FuncHeaderSymbolExpr		// FUNCHEADERK_SymbolExpr
		{
			AstSymbolExpr * pSymbExpr;
		} paramSymbolExpr;
	};
};

NULLABLE AstErr * tryParseFuncHeader(Parser * pParser, const ParseFuncHeaderParam & param);


// NOTE: This moves the children into the AST

AstNode * handleScanOrUnexpectedTokenkErr(Parser * pParser, DynamicArray<AstNode *> * papChildren = nullptr);


// Only claimed tokens are safe to stick in the AST. If you merely peek, the
//  pending token's memory will be overwritten next time you consume a token.

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
