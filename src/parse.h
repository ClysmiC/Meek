#pragma once

#include "als.h"

#include "ast.h"
#include "ast_decorate.h"
#include "token.h"
#include "type.h"

struct MeekCtx;

struct BinopInfo
{
	static constexpr int s_cTokenMatchMax = 4;	// Bump as needed

	int precedence;
	TOKENK aTokenkMatch[s_cTokenMatchMax];
	int cTokenMatch;
};

struct Parser
{
	MeekCtx * pCtx;

	// Allocators

	DynamicPoolAllocator<AstNode> astAlloc;
	DynamicPoolAllocator<Token> tokenAlloc;
	DynamicPoolAllocator<Scope> scopeAlloc;

	// Scope

	SCOPEID scopeidNext = SCOPEID_Nil;
	Scope * pScopeCurrent = nullptr;

	Scope * pScopeBuiltin = nullptr;
	Scope * pScopeGlobal = nullptr;

	// Vars

	VARSEQID varseqidNext = VARSEQID_Nil;

	// Node construction / bookkeeping

	uint iNode = 0;				// Becomes node's id
	Token * pPendingToken = nullptr;

	// Error and error recovery

	DynamicArray<AstNode *> apErrorNodes;
};

void init(Parser * pParser, MeekCtx * pCtx);
void reportScanAndParseErrors(const Parser & parser);

AstNode * parseProgram(Parser * pParser, bool * poSuccess);

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
	FUNCHEADERK_SymbolExpr		// Corresponds to SYMEXPRK_Func
};

enum PARAMK
{
	PARAMK_Param,
	PARAMK_Return
};

// Scope management

Scope * pushScope(Parser * pParser, SCOPEK scopek);
Scope * popScope(Parser * pParser);


// Node allocation

AstNode * astNew(Parser * pParser, ASTK astk, StartEndIndices startEnd);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, AstNode * pChild0 = nullptr, AstNode * pChild1 = nullptr, AstNode * pChild2 = nullptr);
AstNode * astNewErr(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, AstNode * aPChildren[], uint cPChildren);
AstNode * astNewErrMoveChildren(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, DynamicArray<AstNode *> * papChildren);

// Error handling

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover);
bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatched = nullptr);


// STMT

enum PARSESTMTK
{
	PARSESTMTK_DoPseudoStmt,
	PARSESTMTK_TopLevelStmt,
	PARSESTMTK_Stmt
};

AstNode * parseStmt(Parser * pParser, PARSESTMTK parsestmtk = PARSESTMTK_Stmt);
AstNode * parseExprStmtOrAssignStmt(Parser * pParser);
AstNode * parseStructDefnStmt(Parser * pParser);
AstNode * parseVarDeclStmt(
	Parser * pParser,
	EXPECTK expectkName = EXPECTK_Required,
	EXPECTK expectkInit = EXPECTK_Optional,
	EXPECTK expectkSemicolon = EXPECTK_Required,
	NULLABLE PENDINGTYPID * poPendingTypid = nullptr);

AstNode * parseIfStmt(Parser * pParser);
AstNode * parseWhileStmt(Parser * pParser);
AstNode * parseDoPseudoStmtOrBlockStmt(Parser * pParser, bool pushPopScopeBlock = true);
AstNode * parseDoPseudoStmt(Parser * pParser);
AstNode * parseBlockStmt(Parser * pParser, bool pushPopScope = true);
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
AstNode * parseLiteralExpr(Parser * pParser, bool mustBeIntLiteralk = false);
AstNode * parseFuncLiteralExpr(Parser * pParser);
AstNode * parseFuncSymbolExpr(Parser * pParser);

// Helpers

AstNode * parseFuncDefnStmtOrLiteralExpr(Parser * pParser, FUNCHEADERK funcheaderk);
AstNode * finishParsePrimary(Parser * pParser, AstNode * pLhsExpr);

// Type parsing helpers

enum PTRK
{
	PTRK_Error,
	PTRK_FuncType,
	PTRK_NonFuncType
};

struct ParseTypeResult
{
	PTRK ptrk;

	union
	{
		struct UErrorData
		{
			AstErr * pErr;
		} errorData;

		//struct UFuncTypeData
		//{
		//	DynamicArray<PENDINGTYPID> aPendingTypidParam;
		//	DynamicArray<PENDINGTYPID> aPendingTypidReturn;
		//} funcTypeData;

		struct UNonFuncTypeData
		{
			PENDINGTYPID pendingTypid;
		} nonErrorData;
	};
};

ParseTypeResult tryParseType(Parser * pParser);

struct ParseFuncHeaderParam
{
	FUNCHEADERK funcheaderk;

	union
	{
		struct UFuncHeaderDefn				// FUNCHEADERK_Defn
		{
			AstFuncDefnStmt * pioNode;
			DynamicArray<PENDINGTYPID> * paPendingTypidParam;
			DynamicArray<PENDINGTYPID> * paPendingTypidReturn;
		} paramDefn;

		struct UFuncHeaderLiteral			// FUNCHEADERK_Literal
		{
			AstFuncLiteralExpr * pioNode;
			DynamicArray<PENDINGTYPID> * paPendingTypidParam;
			DynamicArray<PENDINGTYPID> * paPendingTypidReturn;
		} paramLiteral;

		struct UFuncHeaderType				// FUNCHEADERK_Type
		{
			DynamicArray<PENDINGTYPID> * paPendingTypidParam;
			DynamicArray<PENDINGTYPID> * paPendingTypidReturn;
		} paramType;

		struct UFuncHeaderSymbolExpr		// FUNCHEADERK_SymbolExpr
		{
			DynamicArray<PENDINGTYPID> * paPendingTypidParam;
			Lexeme * poIdent;
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

inline void releaseToken(Parser * pParser, Token * pToken)
{
	release(&pParser->tokenAlloc, pToken);
}
