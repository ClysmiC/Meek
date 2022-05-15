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

	Scope * pScopeBuiltin = nullptr;	// Parser probably shouldn't own these...
	Scope * pScopeGlobal = nullptr;

	// Vars

	VARSEQID varseqidNext = VARSEQID_Nil;

	// Node construction / bookkeeping

	uint iNode = 0;				// Becomes node's id
	Token * pPendingToken = nullptr;

	// Error and error recovery

	DynamicArray<AstNode *> apErrorNodes;
};

void init(Parser * parser, MeekCtx * pCtx);
void reportScanAndParseErrors(const Parser & parser);

AstNode * parseProgram(Parser * parser, bool * poSuccess);

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

Scope * pushScope(Parser * parser, SCOPEK scopek);
Scope * popScope(Parser * parser);


// Node allocation

AstNode * astNew(Parser * parser, ASTK astk, StartEndIndices startEnd);
AstNode * astNewErr(Parser * parser, ASTK astkErr, StartEndIndices startEnd, AstNode * pChild0 = nullptr, AstNode * pChild1 = nullptr, AstNode * pChild2 = nullptr);
AstNode * astNewErr(Parser * parser, ASTK astkErr, StartEndIndices startEnd, AstNode * aPChildren[], uint cPChildren);
AstNode * astNewErrMoveChildren(Parser * parser, ASTK astkErr, StartEndIndices startEnd, DynamicArray<AstNode *> * papChildren);

// Error handling

bool tryRecoverFromPanic(Parser * parser, TOKENK tokenkRecover);
bool tryRecoverFromPanic(Parser * parser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatched = nullptr);


// STMT

enum PARSESTMTK
{
	PARSESTMTK_DoPseudoStmt,
	PARSESTMTK_TopLevelStmt,
	PARSESTMTK_Stmt
};

AstNode * parseStmt(Parser * parser, PARSESTMTK parsestmtk = PARSESTMTK_Stmt);
AstNode * parseExprStmtOrAssignStmt(Parser * parser);
AstNode * parseStructDefnStmt(Parser * parser);
AstNode * parseVarDeclStmt(
	Parser * parser,
	VARDECLK vardeclk,
	NULLABLE PendingTypeId * poPendingTypid = nullptr);

AstNode * parseIfStmt(Parser * parser);
AstNode * parseWhileStmt(Parser * parser);
AstNode * parseDoPseudoStmtOrBlockStmt(Parser * parser, bool pushPopScopeBlock = true);
AstNode * parseDoPseudoStmt(Parser * parser);
AstNode * parseBlockStmt(Parser * parser, bool pushPopScope = true);
AstNode * parseReturnStmt(Parser * parser);
AstNode * parseBreakStmt(Parser * parser);
AstNode * parseContinueStmt(Parser * parser);
AstNode * parsePrintStmt(Parser * parser);
AstNode * parseFuncDefnStmt(Parser * parser);

// EXPR

AstNode * parseExpr(Parser * parser);
AstNode * parseExpr(Parser * parser);
AstNode * parseBinop(Parser * parser, const BinopInfo & op);
AstNode * parseUnopPre(Parser * parser);
AstNode * parsePrimary(Parser * parser);
AstNode * parseVarOrMemberVarSymbolExpr(Parser * parser, NULLABLE AstNode * pMemberOwnerExpr);
AstNode * parseLiteralExpr(Parser * parser, bool mustBeIntLiteralk = false);
AstNode * parseFuncLiteralExpr(Parser * parser);
AstNode * parseFuncSymbolExpr(Parser * parser);

// Helpers

AstNode * parseFuncDefnStmtOrLiteralExpr(Parser * parser, FUNCHEADERK funcheaderk);
AstNode * finishParsePrimary(Parser * parser, AstNode * pLhsExpr);

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
		//	DynamicArray<PendingTYPID> aPendingTypidParam;
		//	DynamicArray<PendingTYPID> aPendingTypidReturn;
		//} funcTypeData;

		struct UNonFuncTypeData
		{
			PendingTypeId pendingTypid;
		} nonErrorData;
	};
};

ParseTypeResult tryParseType(Parser * parser);

struct ParseFuncHeaderParam
{
	FUNCHEADERK funcheaderk;

	union
	{
		struct UFuncHeaderDefn				// FUNCHEADERK_Defn
		{
			AstFuncDefnStmt * pioNode;
			DynamicArray<PendingTypeId> * paPendingTypidParam;
			DynamicArray<PendingTypeId> * paPendingTypidReturn;
		} paramDefn;

		struct UFuncHeaderLiteral			// FUNCHEADERK_Literal
		{
			AstFuncLiteralExpr * pioNode;
			DynamicArray<PendingTypeId> * paPendingTypidParam;
			DynamicArray<PendingTypeId> * paPendingTypidReturn;
		} paramLiteral;

		struct UFuncHeaderType				// FUNCHEADERK_Type
		{
			DynamicArray<PendingTypeId> * paPendingTypidParam;
			DynamicArray<PendingTypeId> * paPendingTypidReturn;
		} paramType;

		struct UFuncHeaderSymbolExpr		// FUNCHEADERK_SymbolExpr
		{
			DynamicArray<PendingTypeId> * paPendingTypidParam;
			Lexeme * poIdent;
		} paramSymbolExpr;
	};
};

NULLABLE AstErr * tryParseFuncHeader(Parser * parser, const ParseFuncHeaderParam & param);


// NOTE: This moves the children into the AST

AstNode * handleScanOrUnexpectedTokenkErr(Parser * parser, DynamicArray<AstNode *> * papChildren = nullptr);


// Only claimed tokens are safe to stick in the AST. If you merely peek, the
//  pending token's memory will be overwritten next time you consume a token.

Token * ensurePendingToken(Parser * parser);
Token * claimPendingToken(Parser * parser);
Token * ensureAndClaimPendingToken(Parser * parser);

inline void releaseToken(Parser * parser, Token * pToken)
{
	release(&parser->tokenAlloc, pToken);
}
