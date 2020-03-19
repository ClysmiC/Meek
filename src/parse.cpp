#include "parse.h"

#include "error.h"
#include "scan.h"

// Absolutely sucks that I need to use 0, 1, 2 suffixes. I tried this approach to simulate default parameters in a macro but MSVC has a bug
//	with varargs expansion that made it blow up: https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros

#define AstNew(pParser, astk, startEnd) reinterpret_cast<Ast##astk *>(astNew(pParser, ASTK_##astk, startEnd))

#define AstNewErr0Child(pParser, astkErr, startEnd) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, startEnd, nullptr, nullptr))
#define AstNewErr1Child(pParser, astkErr, startEnd, pChild0) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, startEnd, pChild0, nullptr))
#define AstNewErr2Child(pParser, astkErr, startEnd, pChild0, pChild1) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, startEnd, pChild0, pChild1))
#define AstNewErr3Child(pParser, astkErr, startEnd, pChild0, pChild1, pChild2) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, startEnd, pChild0, pChild1, pChild2))

// NOTE: This macro is functionally equivalent to AstNewErr2Child since we rely on function overloading to choose the correct one.
//	I'm not really happy with this solution but it works for now.

#define AstNewErrListChild(pParser, astkErr, startEnd, apChildren, cPChildren) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, startEnd, apChildren, cPChildren))
#define AstNewErrListChildMove(pParser, astkErr, startEnd, papChildren) reinterpret_cast<Ast##astkErr *>(astNewErrMoveChildren(pParser, ASTK_##astkErr, startEnd, papChildren))

static const BinopInfo s_aParseOp[] = {
	{ 0, { TOKENK_Star, TOKENK_Slash, TOKENK_Percent }, 3 },
	{ 1, { TOKENK_Plus, TOKENK_Minus }, 2 },
	{ 2, { TOKENK_Lesser, TOKENK_Greater, TOKENK_LesserEqual, TOKENK_GreaterEqual }, 4 },
	{ 3, { TOKENK_EqualEqual, TOKENK_BangEqual }, 2 },
	{ 4, { TOKENK_HashAnd }, 1 },
	{ 5, { TOKENK_HashXor }, 1 },
	{ 6, { TOKENK_HashOr }, 1 },
	{ 7, { TOKENK_AmpAmp }, 1 },
	{ 8, { TOKENK_PipePipe }, 1 },
};
static constexpr int s_iParseOpMax = ArrayLen(s_aParseOp);

void init(Parser * pParser, Scanner * pScanner)
{
	pParser->pScanner = pScanner;
	init(&pParser->astAlloc);
	init(&pParser->tokenAlloc);
	init(&pParser->typeTable);
	init(&pParser->scopeAlloc);
	init(&pParser->astDecs);
	init(&pParser->apErrorNodes);

	init(&pParser->mpScopeidScope);

	pParser->pScopeRoot = allocate(&pParser->scopeAlloc);
	init(pParser->pScopeRoot, SCOPEID_BuiltIn, SCOPEK_BuiltIn, nullptr);

	Assert(pParser->mpScopeidScope.cItem == SCOPEID_BuiltIn);
	append(&pParser->mpScopeidScope, pParser->pScopeRoot);
}

AstNode * parseProgram(Parser * pParser, bool * poSuccess)
{
	// NOTE: Empty program is valid to parse, but I may still decide that
	//	that is a semantic error...

	pushScope(pParser, SCOPEK_Global);

	DynamicArray<AstNode *> apNodes;
	init(&apNodes);
	Defer(AssertInfo(!apNodes.pBuffer, "Should be moved into program AST node"));

	StartEndIndices startEnd;

	while (!isFinished(pParser->pScanner) && peekToken(pParser->pScanner) != TOKENK_Eof)
	{
		AstNode * pNode = parseStmt(pParser, PARSESTMTK_TopLevelStmt);

		if (isErrorNode(*pNode))
		{
			TOKENK tokenkPrev = prevToken(pParser->pScanner);
			if (tokenkPrev != TOKENK_Semicolon)
			{
				bool recovered = tryRecoverFromPanic(pParser, TOKENK_Semicolon);
				Assert(Implies(!recovered, isFinished(pParser->pScanner)));
			}
		}

		if (apNodes.cItem == 0)
		{
			auto startEndNode = getStartEnd(pParser->astDecs, pNode->astid);
			startEnd.iStart = startEndNode.iStart;
			startEnd.iEnd = startEndNode.iEnd;
		}
		else
		{
			auto startEndNode = getStartEnd(pParser->astDecs, pNode->astid);
			startEnd.iEnd = startEndNode.iEnd;
		}

		append(&apNodes, pNode);
	}

	auto * pNodeProgram = AstNew(pParser, Program, startEnd);
	initMove(&pNodeProgram->apNodes, &apNodes);
	*poSuccess = (pParser->apErrorNodes.cItem == 0);
	return Up(pNodeProgram);
}

AstNode * parseStmt(Parser * pParser, PARSESTMTK parsestmtk)
{
	// HMM: Need different syntax for const and compile time const vardecls. for struct defn's and
	//	top-level fun defn's they want to be compile time const. But maybe we can aggressively infer
	//	compile time constness and make it a semantic error if they aren't. So that way the following
	//	syntaxes would be allowed

	// Basically just need to think a lot about how fun and struct decl's look (or if they are even permitted!) in the following 3 cases
	//	-compile time immutable
	//	-runtime immutable
	//	-runtime mutable


	// Peek around a bit to figure out what kind of statement it is!

	TOKENK tokenkNext = peekToken(pParser->pScanner, nullptr, 0);
	TOKENK tokenkNextNext = peekToken(pParser->pScanner, nullptr, 1);

	if (tokenkNext == TOKENK_Struct)
	{
		// Struct defn

		auto * pNode = parseStructDefnStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_StructDefnStmt);

		if (parsestmtk == PARSESTMTK_DoPseudoStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalDoPseudoStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (parsestmtk != PARSESTMTK_DoPseudoStmt && tokenkNext == TOKENK_Fn && tokenkNextNext == TOKENK_Identifier)
	{
		// Func defn

		Assert(parsestmtk != PARSESTMTK_DoPseudoStmt);

		AstNode * pNode = parseFuncDefnStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_FuncDefnStmt);

		return pNode;
	}
	//else if (tokenkNext == TOKENK_Enum && tokenkNextNext == TOKENK_Identifier)
	//{
	//	// Enum defn

	//	Assert(false);		// TODO
	//}
	//else if (tokenkNext == TOKENK_Union && tokenkNextNext == TOKENK_Identifier)
	//{
	//	// Union defn

	//	// TODO
	//}
	else if (tokenkNext == TOKENK_If)
	{
		auto * pNode = parseIfStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_IfStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_While)
	{
		auto * pNode = parseWhileStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_WhileStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_OpenBrace)
	{
		// Block

		auto * pNode = parseBlockStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_BlockStmt);

		if (parsestmtk == PARSESTMTK_DoPseudoStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalDoPseudoStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_OpenBracket ||
			 tokenkNext == TOKENK_Carat ||
			 (tokenkNext == TOKENK_Identifier && tokenkNextNext == TOKENK_Identifier) ||
			 (tokenkNext == TOKENK_Fn && parsestmtk != PARSESTMTK_DoPseudoStmt))
	{
		// HMM: Should I put this earlier since var decls are so common?
		//	I have to have it at least after func defn so I can be assured
		//	that if we peek "fn" that it isn't part of a func defn

		// Var decl

		Assert(Implies(tokenkNext == TOKENK_Fn, parsestmtk != PARSESTMTK_DoPseudoStmt));

		auto * pNode = parseVarDeclStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_VarDeclStmt);

		if (parsestmtk == PARSESTMTK_DoPseudoStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalDoPseudoStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Return)
	{
		// Return

		auto * pNode = parseReturnStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_ReturnStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Break)
	{
		// Break

		auto * pNode = parseBreakStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_BreakStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Continue)
	{
		// Continue

		auto * pNode = parseContinueStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_ContinueStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (parsestmtk == PARSESTMTK_Stmt && tokenkNext == TOKENK_Do)
	{
		// NOTE (andrew) Other parsestmtk's will just fall into an "unexpected 'do'" error, which is probably better than trying
		//	to encode all the weird situations you can get into with the do pseudo-statement in their own specific error types.
		//	Example weird situations: 'do do <stmt>', top level 'do <stmt>', etc.

		auto * pNode = parseDoPseudoStmt(pParser);
		
		if (isErrorNode(*pNode)) return pNode;

		return pNode;
	}
	else
	{
		// Expr stmt or Assignment stmt

		auto * pNode = parseExprStmtOrAssignStmt(pParser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_AssignStmt || pNode->astk == ASTK_ExprStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(pParser, IllegalTopLevelStmtErr, getStartEnd(pParser->astDecs, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
}

AstNode * parseExprStmtOrAssignStmt(Parser * pParser)
{
	// Parse lhs expression

	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	AstNode * pLhsExpr = parseExpr(pParser);
	if (isErrorNode(*pLhsExpr))
	{
		return pLhsExpr;
	}

	// Check if it is an assignment

	bool isAssignment = false;
	AstNode * pRhsExpr = nullptr;

	const static TOKENK s_aTokenkAssign[] = {
		TOKENK_Equal,
		TOKENK_PlusEqual,
		TOKENK_MinusEqual,
		TOKENK_StarEqual,
		TOKENK_SlashEqual,
		TOKENK_PercentEqual
	};
	const static int s_cTokenkAssign = ArrayLen(s_aTokenkAssign);

	Token * pAssignToken = nullptr;
	if (tryConsumeToken(pParser->pScanner, s_aTokenkAssign, s_cTokenkAssign, ensurePendingToken(pParser)))
	{
		isAssignment = true;
		pAssignToken = claimPendingToken(pParser);

		// Parse rhs expression

		pRhsExpr = parseExpr(pParser);
		if (isErrorNode(*pRhsExpr))
		{
			auto * pErr = AstNewErr2Child(pParser, BubbleErr, gc_startEndBubble, pLhsExpr, pRhsExpr);
			return Up(pErr);
		}

		if (tryConsumeToken(pParser->pScanner, s_aTokenkAssign, s_cTokenkAssign))
		{
			// NOTE: This check isn't necessary for correctness, but it gives a better error message for a common case.

			auto * pErr = AstNewErr2Child(
							pParser,
							ChainedAssignErr,
							makeStartEnd(iStart, prevTokenStartEnd(pParser->pScanner).iEnd),
							pLhsExpr,
							pRhsExpr);

			return Up(pErr);
		}
	}

	// Parse semicolon

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		if (isAssignment)
		{
			auto * pErr = AstNewErr2Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pLhsExpr, pRhsExpr);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pLhsExpr);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
	}

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;
	auto startEnd = makeStartEnd(iStart, iEnd);

	// Success!

	if (isAssignment)
	{
		auto * pNode = AstNew(pParser, AssignStmt, startEnd);

		pNode->pAssignToken = pAssignToken;
		pNode->pLhsExpr = pLhsExpr;
		pNode->pRhsExpr = pRhsExpr;
		return Up(pNode);
	}
	else
	{
		Assert(!pRhsExpr);
		Assert(!pAssignToken);

		auto * pNode = AstNew(pParser, ExprStmt, startEnd);
		pNode->pExpr = pLhsExpr;
		return Up(pNode);
	}
}

AstNode * parseStructDefnStmt(Parser * pParser)
{
	Scope * pScopeDefn = pParser->pScopeCurrent;
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse 'struct'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Struct))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Struct);
		return Up(pErr);
	}


	// Parse identifier

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Identifier);
		return Up(pErr);
	}

	Token * pTokenIdent = claimPendingToken(pParser);
	Assert(pTokenIdent->tokenk == TOKENK_Identifier);

	// Parse '{'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		return Up(pErr);
	}

	Scope * pScopeIntroduced = pushScope(pParser, SCOPEK_StructDefn);
	Defer(popScope(pParser));

	// TODO: Allow nested struct definitions!

	// Parse vardeclstmt list, then '}'

	DynamicArray<AstNode *> apVarDeclStmt;
	init(&apVarDeclStmt);
	Defer(Assert(!apVarDeclStmt.pBuffer));

	while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBrace))
	{
		// Parse member var decls

		AstNode * pVarDeclStmt = parseVarDeclStmt(pParser);
		append(&apVarDeclStmt, pVarDeclStmt);

		if (isErrorNode(*pVarDeclStmt))
		{
			static const TOKENK s_aTokenkRecover[] = { TOKENK_CloseBrace, TOKENK_Semicolon };
			TOKENK tokenkRecover;
			if (tryRecoverFromPanic(pParser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
			{
				if (tokenkRecover == TOKENK_CloseBrace)
				{
					break;
				}
				else
				{
					Assert(tokenkRecover == TOKENK_Semicolon);
					continue;
				}
			}

			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, gc_startEndBubble, &apVarDeclStmt);
			return Up(pErr);
		}
	}

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, StructDefnStmt, makeStartEnd(iStart, iEnd));
	pNode->ident.lexeme = pTokenIdent->lexeme;
	pNode->ident.scopeid = pScopeDefn->id;
	initMove(&pNode->apVarDeclStmt, &apVarDeclStmt);
	pNode->scopeid = pScopeIntroduced->id;

	// Insert into symbol table of enclosing scope

	SymbolInfo structDefnInfo;
	structDefnInfo.symbolk = SYMBOLK_Struct;
	structDefnInfo.structData.pStructDefnStmt = pNode;

	defineSymbol(pScopeDefn, pNode->ident.lexeme, structDefnInfo);

	// Insert into type table

	Type type;
	init(&type, false /* isFuncType */);

	type.nonFuncTypeData.ident.lexeme = pNode->ident.lexeme;
	type.nonFuncTypeData.ident.scopeid = pNode->ident.scopeid;
	pNode->typidSelf = ensureInTypeTable(&pParser->typeTable, type, true /* debugAssertIfAlreadyInTable */ );

	return Up(pNode);
}

AstNode * parseVarDeclStmt(Parser * pParser, EXPECTK expectkName, EXPECTK expectkInit, EXPECTK expectkSemicolon)
{
	AssertInfo(expectkName != EXPECTK_Forbidden, "Function does not support being called with expectkName forbidden. Variables usually require names, but are optional for return values");
	AssertInfo(expectkSemicolon != EXPECTK_Optional, "Semicolon should either be required or forbidden");

	AstErr * pErr = nullptr;
	Token * pTokenIdent = nullptr;

	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse type

	// TypePendingResolve * pTypePendingResolve = nullptr;

	PENDINGTYPID pendingTypid = PENDINGTYPID_Nil;
	{
		ParseTypeResult parseTypeResult = tryParseType(pParser);
		if (!parseTypeResult.ptrk == PTRK_Error)
		{
			pErr = parseTypeResult.errorData.pErr;
			goto LFailCleanup;
		}
	}

	// Parse name

	if (expectkName == EXPECTK_Optional || expectkName == EXPECTK_Required)
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			pTokenIdent = claimPendingToken(pParser);
			Assert(pTokenIdent->tokenk == TOKENK_Identifier);
		}
		else if (expectkName == EXPECTK_Required)
		{
			auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

			auto * pExpectedTokenErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
			append(&pExpectedTokenErr->aTokenkValid, TOKENK_Identifier);

			pErr = UpErr(pExpectedTokenErr);
			goto LFailCleanup;
		}
	}
	else
	{
		AssertInfo(false, "Var decls can be optionally named in some contexts (func params), but it doesn't make sense to forbid naming them!");
	}

	// Parse init expr

	AstNode * pInitExpr = nullptr;

	int iEqualStart = -1;
	if (tryConsumeToken(pParser->pScanner, TOKENK_Equal))
	{
		iEqualStart = prevTokenStartEnd(pParser->pScanner).iStart;

		pInitExpr = parseExpr(pParser);

		if (isErrorNode(*pInitExpr))
		{
			pErr = DownErr(pInitExpr);
			goto LFailCleanup;
		}

		if (tryConsumeToken(pParser->pScanner, TOKENK_Equal))
		{
			// NOTE: This check isn't necessary for correctness, but it gives a better error message for a common case.

			pErr = UpErr(AstNewErr1Child(
							pParser,
							ChainedAssignErr,
							makeStartEnd(iStart, prevTokenStartEnd(pParser->pScanner).iEnd),
							pInitExpr));

			goto LFailCleanup;
		}
	}

	// Providing init expr isn't allowed if the var isn't named...

	if (!pTokenIdent && pInitExpr)
	{
		Assert(iEqualStart != -1);
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		pErr = UpErr(AstNewErr1Child(pParser, InitUnnamedVarErr, makeStartEnd(iEqualStart, startEndPrev.iEnd), pInitExpr));
		goto LFailCleanup;
	}

	// Parse semicolon

	if (expectkSemicolon == EXPECTK_Required)
	{
		if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon, ensurePendingToken(pParser)))
		{
			auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

			auto * pErrExpectedTokenk = AstNewErr1Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pInitExpr);
			append(&pErrExpectedTokenk->aTokenkValid, TOKENK_Semicolon);

			pErr = UpErr(pErrExpectedTokenk);
			goto LFailCleanup;
		}
	}

	// Success!

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, VarDeclStmt, makeStartEnd(iStart, iEnd));
	if (pTokenIdent)
	{
		pNode->ident.lexeme = pTokenIdent->lexeme;
		pNode->ident.scopeid = pParser->pScopeCurrent->id;
	}
	else
	{
		setLexeme(&pNode->ident.lexeme, "");
		pNode->ident.scopeid = SCOPEID_Nil;
	}
	pNode->pInitExpr = pInitExpr;

	// Remember to poke in the typid once this type is resolved

	setPendingTypeUpdateOnResolvePtr(&pParser->typeTable, pendingTypid, &pNode->typid);

	if (pTokenIdent)
	{
		SymbolInfo varDeclInfo;
		varDeclInfo.symbolk = SYMBOLK_Var;
		varDeclInfo.varData.pVarDeclStmt = pNode;
		defineSymbol(pParser->pScopeCurrent, pTokenIdent->lexeme, varDeclInfo);
	}

	return Up(pNode);

LFailCleanup:

	Assert(pErr);
	if (pTokenIdent)
	{
		release(&pParser->tokenAlloc, pTokenIdent);
	}

	return Up(pErr);
}

AstNode * parseIfStmt(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse 'if'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_If))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_If);
		return Up(pErr);
	}

	// Parse cond expr

	AstNode * pCondExpr = parseExpr(pParser);
	if (isErrorNode(*pCondExpr))
	{
		return pCondExpr;
	}

	// Parse 'do <stmt>' or block stmt

	AstNode * pThenStmt = parseDoPseudoStmtOrBlockStmt(pParser);
	if (isErrorNode(*pThenStmt))
	{
		auto * pErr = AstNewErr2Child(pParser, BubbleErr, gc_startEndBubble, pCondExpr, pThenStmt);
		return Up(pErr);
	}

	// Try parse 'else' statement

	AstNode * pElseStmt = nullptr;
	if (tryConsumeToken(pParser->pScanner, TOKENK_Else))
	{
		if (peekToken(pParser->pScanner) == TOKENK_If)
		{
			pElseStmt = parseIfStmt(pParser);
		}
		else
		{
			pElseStmt = parseDoPseudoStmtOrBlockStmt(pParser);
		}
	}

	if (pElseStmt && isErrorNode(*pElseStmt))
	{
		auto * pErr = AstNewErr3Child(pParser, BubbleErr, gc_startEndBubble, pCondExpr, pThenStmt, pElseStmt);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, IfStmt, makeStartEnd(iStart, iEnd));
	pNode->pCondExpr = pCondExpr;
	pNode->pThenStmt = pThenStmt;
	pNode->pElseStmt = pElseStmt;
	return Up(pNode);
}

AstNode * parseWhileStmt(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse 'while'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_While))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd));
		append(&pErr->aTokenkValid, TOKENK_While);
		return Up(pErr);
	}

	// Parse cond expr

	AstNode * pCondExpr = parseExpr(pParser);
	if (isErrorNode(*pCondExpr))
	{
		return pCondExpr;
	}

	// Parse 'do <stmt>' or block stmt

	AstNode * pBodyStmt = parseDoPseudoStmtOrBlockStmt(pParser);
	if (isErrorNode(*pBodyStmt))
	{
		auto * pErr = AstNewErr2Child(pParser, BubbleErr, gc_startEndBubble, pCondExpr, pBodyStmt);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, WhileStmt, makeStartEnd(iStart, iEnd));
	pNode->pCondExpr = pCondExpr;
	pNode->pBodyStmt = pBodyStmt;
	return Up(pNode);
}

AstNode * parseDoPseudoStmtOrBlockStmt(Parser * pParser, bool pushPopScopeBlock)
{
	TOKENK tokenk = peekToken(pParser->pScanner);
	if (tokenk != TOKENK_OpenBrace &&
		tokenk != TOKENK_Do)
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		append(&pErr->aTokenkValid, TOKENK_Do);
		return Up(pErr);
	}

	AstNode * pStmt = nullptr;
	if (tokenk == TOKENK_OpenBrace)
	{
		pStmt = parseBlockStmt(pParser, pushPopScopeBlock);
	}
	else
	{
		Assert(tokenk == TOKENK_Do);
		pStmt = parseStmt(pParser, PARSESTMTK_DoPseudoStmt);
	}

	if (isErrorNode(*pStmt))
	{
		return pStmt;
	}

	// Success!

	return pStmt;
}

AstNode * parseDoPseudoStmt(Parser * pParser)
{
	// NOTE (andrew) "do" is a psuedo statement in that it doesn't actually get it's own statement kind in
	//	the AST. It is literally just a statement that is restricted from being certain kinds of statements.
	//	It is used to enforce that there aren't nonsensical things like variable declarations in a one-stmt
	//	for loop. It is also used to tell the parser to take a particular branch when hitting a certain
	//	keyword. E.g., "fn" usually branches to parsing fn defn or var decls. But in an expression context,
	//	it can also be used to disambiguate a fn identifier, or define a fn literal. If you start a statement
	//	with such an expression, you need to use "do" to tell the parser that the "fn" isn't starting a defn/decl!

	// Parse 'do'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Do))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Do);
		return Up(pErr);
	}

	AstNode * pNode = parseStmt(pParser, PARSESTMTK_DoPseudoStmt);

	if (isErrorNode(*pNode))
	{
		return pNode;
	}

	// Success!

	return pNode;
}

AstNode * parseBlockStmt(Parser * pParser, bool pushPopScope)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse {

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		return Up(pErr);
	}

	if (pushPopScope) pushScope(pParser, SCOPEK_CodeBlock);
	Defer(if (pushPopScope) popScope(pParser));

	// Parse statements until }

	DynamicArray<AstNode *> apStmts;
	init(&apStmts);
	Defer(Assert(!apStmts.pBuffer));		// buffer should get "moved" into AST

	while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBrace))
	{
		AstNode * pStmt = parseStmt(pParser);
		append(&apStmts, pStmt);

		if (isErrorNode(*pStmt))
		{
			static const TOKENK s_aTokenkRecover[] = { TOKENK_CloseBrace, TOKENK_Semicolon };
			TOKENK tokenkRecover;
			if (tryRecoverFromPanic(pParser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
			{
				if (tokenkRecover == TOKENK_CloseBrace)
				{
					break;
				}
				else
				{
					Assert(tokenkRecover == TOKENK_Semicolon);
					continue;
				}
			}

			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, gc_startEndBubble, &apStmts);
			return Up(pErr);
		}
	}

	// Success!

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, BlockStmt, makeStartEnd(iStart, iEnd));
	initMove(&pNode->apStmts, &apStmts);
	pNode->scopeid = pParser->pScopeCurrent->id;

	return Up(pNode);
}

AstNode * parseReturnStmt(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse 'return'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Return))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Return);
		return Up(pErr);
	}

	// Optionally parse expr

	AstNode * pExpr = nullptr;
	if (peekToken(pParser->pScanner) != TOKENK_Semicolon)
	{
		pExpr = parseExpr(pParser);

		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}
	}

	// Parse ';'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		AstExpectedTokenkErr * pErr;

		if (pExpr)
		{
			pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr);
		}
		else
		{
			pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		}

		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, ReturnStmt, makeStartEnd(iStart, iEnd));
	pNode->pExpr = pExpr;
	return Up(pNode);
}

AstNode * parseBreakStmt(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Break))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Break);
		return Up(pErr);
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, BreakStmt, makeStartEnd(iStart, iEnd));
	return Up(pNode);
}

AstNode * parseContinueStmt(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Continue))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Continue);
		return Up(pErr);
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

	auto * pNode = AstNew(pParser, ContinueStmt, makeStartEnd(iStart, iEnd));
	return Up(pNode);
}

AstNode * parseFuncDefnStmt(Parser * pParser)
{
	return parseFuncDefnStmtOrLiteralExpr(pParser, FUNCHEADERK_Defn);
}

AstNode * parseExpr(Parser * pParser)
{
	return parseBinop(pParser, s_aParseOp[s_iParseOpMax - 1]);
}

AstNode * parseBinop(Parser * pParser, const BinopInfo & op)
{
	auto oneStepLower = [](Parser * pParser, int precedence) -> AstNode *
	{
		Assert(precedence >= 0);

		if (precedence == 0)
		{
			return parseUnopPre(pParser);
		}
		else
		{
			const BinopInfo & opNext = s_aParseOp[precedence - 1];
			return parseBinop(pParser, opNext);
		}
	};

	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	AstNode * pExpr = oneStepLower(pParser, op.precedence);
	Assert(pExpr);

	// NOTE: Error check is done after we get a chance to parse rhs so that we can catch errors in both expressions. If we don't
	//	match a binop then we just want to return pExpr as is anyway, whether it is an error or not.

	while (tryConsumeToken(pParser->pScanner, op.aTokenkMatch, op.cTokenMatch, ensurePendingToken(pParser)))
	{
		Token * pOp = claimPendingToken(pParser);

		AstNode * pRhsExpr = oneStepLower(pParser, op.precedence);
		Assert(pRhsExpr);

		if (isErrorNode(*pExpr) || isErrorNode(*pRhsExpr))
		{
			auto * pErr = AstNewErr2Child(pParser, BubbleErr, gc_startEndBubble, pExpr, pRhsExpr);
			return Up(pErr);
		}

		int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

		auto * pNode = AstNew(pParser, BinopExpr, makeStartEnd(iStart, iEnd));
		pNode->pOp = pOp;
		pNode->pLhsExpr = pExpr;
		pNode->pRhsExpr = pRhsExpr;

		pExpr = Up(pNode);
	}

	return pExpr;
}

AstNode * parseUnopPre(Parser * pParser)
{
	if (tryConsumeToken(pParser->pScanner, g_aTokenkUnopPre, g_cTokenkUnopPre, ensurePendingToken(pParser)))
	{
		int iStart = prevTokenStartEnd(pParser->pScanner).iStart;

		Token * pOp = claimPendingToken(pParser);

		AstNode * pExpr = parseUnopPre(pParser);
		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}

		int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

		auto * pNode = AstNew(pParser, UnopExpr, makeStartEnd(iStart, iEnd));
		pNode->pOp = pOp;
		pNode->pExpr = pExpr;

		return Up(pNode);
	}
	else
	{
		return parsePrimary(pParser);
	}
}

AstNode * parsePrimary(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// Group ( )

		Token * pOpenParen = claimPendingToken(pParser);
		AstNode * pExpr = parseExpr(pParser);

		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

			auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseParen);
			return Up(pErr);
		}

		int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

		auto * pNode = AstNew(pParser, GroupExpr, makeStartEnd(iStart, iEnd));
		pNode->pExpr = pExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryPeekToken(pParser->pScanner, g_aTokenkLiteral, g_cTokenkLiteral))
	{
		// Literal

		const static bool s_mustBeIntLiteral = false;
		return parseLiteralExpr(pParser, s_mustBeIntLiteral);
	}
	else if (peekToken(pParser->pScanner) == TOKENK_Identifier)
	{
		// Identifier

		AstNode * pVarOwner = nullptr;
		return parseVarOrMemberVarSymbolExpr(pParser, pVarOwner);
	}
	else if (peekToken(pParser->pScanner) == TOKENK_Fn)
	{
		// The way the grammar currently exists, we require arbitrary lookahead to determine if this is a function symbolexpr
		//	or a function literal. It's not great, but we need to speculatively look ahead until we find a symbol that only
		//	belongs to one or the other.

		bool isFnSymbolExpr = false;
		{
			int cParenCtx = 0;
			bool speculate = true;
			while (speculate)
			{
				TOKENK tokenkSpeculate = nextTokenkSpeculative(pParser->pScanner);

				switch (tokenkSpeculate)
				{
					case TOKENK_Error:
					case TOKENK_Eof:
					{
						// Doesn't matter which branch we take... will hit error either way

						speculate = false;
					} break;

					case TOKENK_OpenParen:
					{
						cParenCtx++;
					} break;

					case TOKENK_CloseParen:
					{
						cParenCtx--;
						if (cParenCtx == 0)
						{
							speculate = false;

							if (nextTokenkSpeculative(pParser->pScanner) == TOKENK_Identifier)
							{
								isFnSymbolExpr = true;
							}
						}
						else if (cParenCtx < 0)
						{
							// Doesn't matter which branch we take... will hit error either way

							speculate = false;
						}

					} break;

					case TOKENK_Equal:
					{
						if (cParenCtx > 0)
						{
							isFnSymbolExpr = false;
							speculate = false;
						}
					} break;
				}
			}

			backtrackAfterSpeculation(pParser->pScanner);
		}

		if (isFnSymbolExpr)
		{
			AstNode * pNode = parseFuncSymbolExpr(pParser);

			if (isErrorNode(*pNode))
			{
				return pNode;
			}

			// Success!

			return pNode;
		}
		else
		{
			// Func literal

			AstNode * pNode = parseFuncLiteralExpr(pParser);

			if (isErrorNode(*pNode))
			{
				return pNode;
			}

			if (peekToken(pParser->pScanner) == TOKENK_OpenParen)
			{
				// HMM: I am torn about whether I want to call finishParsePrimaryHere,
				//	which would let you invoke a function literal at the spot that it is
				//	defined (JavaScript calls this IIFE). Personally I am not convinced
				//	that this is good practice... I can't think of a single use case for it.

				// NOTE: Current check for this is kind of a hack when really it would be
				//	better to make calling a func literal a valid parse, but a semantic error!
				//	Otherwise we would error on something like this since we are only checking
				//	for open parens...
				//	fn() -> () { doSomething(); }(;

				int iOpenParenStart = peekTokenStartEnd(pParser->pScanner).iStart;

				auto * pErr = AstNewErr1Child(pParser, InvokeFuncLiteralErr, makeStartEnd(iOpenParenStart), pNode);
				return Up(pErr);
			}

			// Success!

			return pNode;
		}
	}
	else
	{
		return handleScanOrUnexpectedTokenkErr(pParser);
	}
}

AstNode * parseLiteralExpr(Parser * pParser, bool mustBeIntLiteralk)
{
	const static TOKENK s_tokenkIntLit = TOKENK_IntLiteral;

	const TOKENK * aTokenkValid = (mustBeIntLiteralk) ? &s_tokenkIntLit : g_aTokenkLiteral;
	const int cTokenkValid = (mustBeIntLiteralk) ? 1 : g_cTokenkLiteral;

	if (!tryConsumeToken(pParser->pScanner, aTokenkValid, cTokenkValid, ensurePendingToken(pParser)))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		AstExpectedTokenkErr * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		appendMultiple(&pErr->aTokenkValid, aTokenkValid, cTokenkValid);

		return Up(pErr);
	}

	Token * pToken = claimPendingToken(pParser);

	auto * pExpr = AstNew(pParser, LiteralExpr, pToken->startEnd);
	pExpr->pToken = pToken;
	pExpr->literalk = literalkFromTokenk(pExpr->pToken->tokenk);
	pExpr->isValueSet = false;
	pExpr->isValueErroneous = false;

	return Up(pExpr);
}

AstNode * parseFuncLiteralExpr(Parser * pParser)
{
	return parseFuncDefnStmtOrLiteralExpr(pParser, FUNCHEADERK_Literal);
}

AstNode * parseFuncSymbolExpr(Parser * pParser)
{
	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	TOKENK tokenkNext = peekToken(pParser->pScanner);
	TOKENK tokenkNextNext = peekToken(pParser->pScanner, nullptr, 1);

	Assert(tokenkNext == TOKENK_Fn);

	if (tokenkNextNext == TOKENK_Identifier)
	{
		Verify(tryConsumeToken(pParser->pScanner, TOKENK_Fn));
		Verify(tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)));

		Token * pTokenIdent = claimPendingToken(pParser);

		// fn <ident> specifies that we are talking about a function, but we still
		//	don't necessarily know exactly which one

		auto * pNode = AstNew(pParser, SymbolExpr, makeStartEnd(iStart, pTokenIdent->startEnd.iEnd));
		pNode->pTokenIdent = pTokenIdent;
		pNode->symbexprk = SYMBEXPRK_Unresolved;
		pNode->unresolvedData.ignoreVars = true;
		init(&pNode->unresolvedData.aCandidates);

		return Up(pNode);
	}
	else if (tokenkNextNext == TOKENK_OpenParen)
	{
		Lexeme ident;
		DynamicArray<PENDINGTYPID> aPendingTypidParam;
		init(&aPendingTypidParam);
		Defer(dispose(&aPendingTypidParam));

		ParseFuncHeaderParam parseFuncHeaderParam;
		parseFuncHeaderParam.funcheaderk = FUNCHEADERK_SymbolExpr;
		parseFuncHeaderParam.paramSymbolExpr.paPendingTypidParam = &aPendingTypidParam;
		parseFuncHeaderParam.paramSymbolExpr.poIdent = &ident;

		AstErr * pErr = tryParseFuncHeader(pParser, parseFuncHeaderParam);
		if (pErr)
		{
			return Up(pErr);
		}

		// Success!

		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pNode = AstNew(pParser, SymbolExpr, makeStartEnd(iStart + startEndPrev.iEnd));
		pNode->symbexprk = SYMBEXPRK_Func;
		pNode->funcData.pDefnCached = nullptr;		// Not yet resolved
		init(&pNode->funcData.aTypidDisambig);

		for (int iTypeDisambig = 0; iTypeDisambig < parseFuncHeaderParam.paramSymbolExpr.paPendingTypidParam->cItem; iTypeDisambig++)
		{
			TYPID * pTypid = appendNew(&pNode->funcData.aTypidDisambig);
			*pTypid = TYPID_Unresolved;

			setPendingTypeUpdateOnResolvePtr(
				&pParser->typeTable,
				(*parseFuncHeaderParam.paramSymbolExpr.paPendingTypidParam)[iTypeDisambig],
				pTypid);
		}

		return Up(pNode);
	}
	else
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Identifier);
		append(&pErr->aTokenkValid, TOKENK_OpenParen);

		return Up(pErr);
	}
}

AstNode * parseVarOrMemberVarSymbolExpr(Parser * pParser, NULLABLE AstNode * pMemberOwnerExpr)
{
	if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		AstExpectedTokenkErr * pErr;
		if (pMemberOwnerExpr)
		{
			pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pMemberOwnerExpr);
		}
		else
		{
			pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		}

		append(&pErr->aTokenkValid, TOKENK_Identifier);
		return Up(pErr);
	}

	Token * pIdent = claimPendingToken(pParser);
	Assert(pIdent->tokenk == TOKENK_Identifier);

	// HMM: should the start/end range of a member var expr be just the start/end of the member? Or should it include the owner?

	int iStart = pIdent->startEnd.iStart;
	if (pMemberOwnerExpr)
	{
		iStart = getStartEnd(pParser->astDecs, pMemberOwnerExpr->astid).iStart;
	}

	auto * pNode = AstNew(pParser, SymbolExpr, makeStartEnd(iStart, pIdent->startEnd.iEnd));

	pNode->pTokenIdent = pIdent;
	if (pMemberOwnerExpr)
	{
		pNode->symbexprk = SYMBEXPRK_MemberVar;
		pNode->memberData.pOwner = pMemberOwnerExpr;
		pNode->memberData.pDeclCached = nullptr;
	}
	else
	{
		// Cannot determine yet if this is a var or func

		pNode->symbexprk = SYMBEXPRK_Unresolved;
		init(&pNode->unresolvedData.aCandidates);
		pNode->unresolvedData.ignoreVars = false;
	}

	return finishParsePrimary(pParser, Up(pNode));
}

ParseTypeResult tryParseType(Parser * pParser)
{
	DynamicArray<TypeModifier> aTypemods;
	init(&aTypemods);
	Defer(dispose(&aTypemods););

	ParseTypeResult result;

	while (peekToken(pParser->pScanner) != TOKENK_Identifier &&
		   peekToken(pParser->pScanner) != TOKENK_Fn)
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket))
		{
			// [

			auto * pSubscriptExpr = parseExpr(pParser);

			if (isErrorNode(*pSubscriptExpr))
			{
				result.ptrk = PTRK_Error;
				result.errorData.pErr = DownErr(pSubscriptExpr);
				return result;
			}

			if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket))
			{
				auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

				auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pSubscriptExpr);
				append(&pErr->aTokenkValid, TOKENK_CloseBracket);

				result.ptrk = PTRK_Error;
				result.errorData.pErr = UpErr(pErr);
				return result;
			}

			TypeModifier mod;
			mod.typemodk = TYPEMODK_Array;
			mod.pSubscriptExpr = pSubscriptExpr;
			append(&aTypemods, mod);
		}
		else if (tryConsumeToken(pParser->pScanner, TOKENK_Carat))
		{
			// ^

			TypeModifier mod;
			mod.typemodk = TYPEMODK_Pointer;
			append(&aTypemods, mod);
		}
		else
		{
			AstNode * pErr = handleScanOrUnexpectedTokenkErr(pParser, nullptr);

			result.ptrk = PTRK_Error;
			result.errorData.pErr = DownErr(pErr);
			return result;
		}
	}

	TOKENK tokenkPeek = peekToken(pParser->pScanner);
	if (tokenkPeek == TOKENK_Identifier)
	{
		// Non-func type

		// Success!

		Token * pTokenIdent = ensureAndClaimPendingToken(pParser);
		consumeToken(pParser->pScanner, pTokenIdent);

		result.ptrk = PTRK_NonFuncType;
		result.nonErrorData.pendingTypid =
			registerPendingNonFuncType(
				&pParser->typeTable,
				pParser->pScopeCurrent,
				pTokenIdent->lexeme,
				aTypemods);

		return result;
	}
	else if (tokenkPeek == TOKENK_Fn)
	{
		// Func type

		DynamicArray<PENDINGTYPID> aPendingTypidParam;
		init(&aPendingTypidParam);
		Defer(dispose(&aPendingTypidParam));

		DynamicArray<PENDINGTYPID> aPendingTypidReturn;
		init(&aPendingTypidReturn);
		Defer(dispose(&aPendingTypidReturn));

		ParseFuncHeaderParam param;
		param.funcheaderk = FUNCHEADERK_Type;
		param.paramType.paPendingTypidParam = &aPendingTypidParam;
		param.paramType.paPendingTypidReturn = &aPendingTypidReturn;

		AstErr * pErr = tryParseFuncHeader(pParser, param);
		if (pErr)
		{
			result.ptrk = PTRK_Error;
			result.errorData.pErr = pErr;
			return result;
		}

		// Success!

		result.ptrk = PTRK_FuncType;
		result.nonErrorData.pendingTypid =
			registerPendingFuncType(
				&pParser->typeTable,
				pParser->pScopeCurrent,
				aTypemods,
				aPendingTypidParam,
				aPendingTypidReturn);

		return result;
	}
	else
	{
		AstNode * pErr = handleScanOrUnexpectedTokenkErr(pParser, nullptr);
		
		result.ptrk = PTRK_Error;
		result.errorData.pErr = DownErr(pErr);
		return result;
	}
}

AstErr * tryParseFuncHeader(Parser * pParser, const ParseFuncHeaderParam & param)
{
	struct ParseParamListParam
	{
		FUNCHEADERK funcheaderk;
		PARAMK paramk;

		struct UFuncHeaderDefn				// FUNCHEADERK_Defn
		{
			AstFuncDefnStmt * pioNode;
		} paramDefn;

		struct UFuncHeaderLiteral			// FUNCHEADERK_Literal
		{
			AstFuncLiteralExpr * pioNode;
		} paramLiteral;

		struct UFuncHeaderType				// FUNCHEADERK_Type
		{
			DynamicArray<PENDINGTYPID> * paPendingTypidParam;
			DynamicArray<PENDINGTYPID> * paPendingTypidReturn;
		} paramType;

		struct UFuncHeaderSymbolExpr		// FUNCHEADERK_SymbolExpr
		{
			DynamicArray<PENDINGTYPID> * paPendingTypidParam;
		} paramSymbolExpr;
	};

	auto init = [](ParseParamListParam * pPplParam, const ParseFuncHeaderParam & pfhParam, PARAMK paramk)
	{
		pPplParam->funcheaderk = pfhParam.funcheaderk;
		pPplParam->paramk = paramk;

		switch (pPplParam->funcheaderk)
		{
			case FUNCHEADERK_Defn:
			{
				pPplParam->paramDefn.pioNode = pfhParam.paramDefn.pioNode;
			} break;

			case FUNCHEADERK_Literal:
			{
				pPplParam->paramLiteral.pioNode = pfhParam.paramLiteral.pioNode;
			} break;

			case FUNCHEADERK_Type:
			{
				pPplParam->paramType.paPendingTypidParam = pfhParam.paramType.paPendingTypidParam;
				pPplParam->paramType.paPendingTypidReturn = pfhParam.paramType.paPendingTypidReturn;
			} break;

			case FUNCHEADERK_SymbolExpr:
			{
				pPplParam->paramSymbolExpr.paPendingTypidParam = pfhParam.paramSymbolExpr.paPendingTypidParam;
			} break;

			default:
			{
				reportIceAndExit("Unknown FUNCHEADERK %d", pPplParam->funcheaderk);
			}
		}
	};

	auto tryParseParamList = [](Parser * pParser, const ParseParamListParam & param) -> NULLABLE AstErr *
	{
		Assert(Implies(param.funcheaderk == FUNCHEADERK_SymbolExpr, param.paramk == PARAMK_Param));

		// Parse (

		bool isNakedSingleReturn = false;
		if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen))
		{
			if (param.paramk != PARAMK_Return)
			{
				auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
				append(&pErr->aTokenkValid, TOKENK_OpenParen);
				return UpErr(pErr);
			}
			else
			{
				isNakedSingleReturn = true;
			}
		}

		int cParam = 0;
		while (true)
		{
			if (!isNakedSingleReturn && tryConsumeToken(pParser->pScanner, TOKENK_CloseParen))
				break;

			if (isNakedSingleReturn && cParam > 0)
				break;

			// ,

			if (cParam > 0 && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
			{
				auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
				append(&pErr->aTokenkValid, TOKENK_Comma);
				return UpErr(pErr);
			}

			if (param.funcheaderk == FUNCHEADERK_Defn || param.funcheaderk == FUNCHEADERK_Literal)
			{
				// Var decl

				static const EXPECTK s_expectkName = EXPECTK_Optional;
				static const EXPECTK s_expectkSemicolon = EXPECTK_Forbidden;

				EXPECTK expectkInit = EXPECTK_Optional;
				if (param.funcheaderk == FUNCHEADERK_Type)
				{
					expectkInit = EXPECTK_Forbidden;
				}

				AstNode * pNode = parseVarDeclStmt(pParser, s_expectkName, expectkInit, s_expectkSemicolon);
				if (isErrorNode(*pNode))
				{
					return DownErr(pNode);
				}

				if (param.funcheaderk == FUNCHEADERK_Defn)
				{
					if (param.paramk == PARAMK_Param)
					{
						append(&param.paramDefn.pioNode->pParamsReturnsGrp->apParamVarDecls, pNode);
					}
					else
					{
						Assert(param.paramk == PARAMK_Return);
						append(&param.paramDefn.pioNode->pParamsReturnsGrp->apReturnVarDecls, pNode);
					}
				}
				else
				{
					Assert(param.funcheaderk == FUNCHEADERK_Literal);

					if (param.paramk == PARAMK_Param)
					{
						append(&param.paramLiteral.pioNode->pParamsReturnsGrp->apParamVarDecls, pNode);
					}
					else
					{
						Assert(param.paramk == PARAMK_Return);
						append(&param.paramLiteral.pioNode->pParamsReturnsGrp->apReturnVarDecls, pNode);
					}
				}
			}
			else
			{
				// Type

				ParseTypeResult parseTypeResult = tryParseType(pParser);
				if (!parseTypeResult.ptrk == PTRK_Error)
				{
					return DownErr(parseTypeResult.errorData.pErr);
				}

				if (param.funcheaderk == FUNCHEADERK_Type)
				{
					// (Optional) name.
				
					// Note that we don't actually bind this name to anything. It is recommended to be omitted for type definitions,
					//	but is permitted to keep the syntax as close to the same as possible in all 3 cases.
					// It is disallowed in naked single returns because this func header is the type of a variable, and it would
					//	be ambiguous whether the next identifier is the optional return value name or the name of the variable!

					if (!isNakedSingleReturn)
					{
						tryConsumeToken(pParser->pScanner, TOKENK_Identifier);
					}

					if (param.paramk == PARAMK_Param)
					{
						append(param.paramType.paPendingTypidParam, parseTypeResult.nonErrorData.pendingTypid);
					}
					else
					{
						Assert(param.paramk == PARAMK_Return);
						append(param.paramType.paPendingTypidReturn, parseTypeResult.nonErrorData.pendingTypid);
					}
				}
				else
				{
					// We also disallow names for parameters in FUNCHEADERK_SymbolExpr as this language feature is purely
					//	a disambiguation tool. Names are irrelevant.

					Assert(param.funcheaderk == FUNCHEADERK_SymbolExpr);
					Assert(param.paramk == PARAMK_Param);

					append(param.paramSymbolExpr.paPendingTypidParam, parseTypeResult.nonErrorData.pendingTypid);
				}
			}

			cParam++;
		}

		// Success!

		return nullptr;
	};

	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse "fn"

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Fn))
	{
		auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Fn);
		return UpErr(pErr);
	}

	// Parse definition name

	if (param.funcheaderk == FUNCHEADERK_Defn)
	{
		if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

			auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
			append(&pErr->aTokenkValid, TOKENK_Identifier);
			return UpErr(pErr);
		}

		param.paramDefn.pioNode->ident.lexeme = pParser->pPendingToken->lexeme;
		param.paramDefn.pioNode->ident.scopeid = SCOPEID_Nil;		// NOTE (andrew) This gets set by caller
	}
	else
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			// TODO: Maybe this should be a more specific/informative error, since we basically
			//	understand what they are trying to do. Something like NamedFuncNonDefnErr...?
			//	This is probably a good test case for adding more "context" to errors!!!

			Token * pErrToken = claimPendingToken(pParser);
			Assert(pErrToken->tokenk == TOKENK_Identifier);

			auto * pErr = AstNewErr0Child(pParser, UnexpectedTokenkErr, pErrToken->startEnd);
			pErr->pErrToken = pErrToken;
			return UpErr(pErr);
		}
	}

	// Parse "in" parameters

	ParseParamListParam pplParam;
	init(&pplParam, param, PARAMK_Param);
	AstErr * pErr = tryParseParamList(pParser, pplParam);
	if (pErr)
	{
		return pErr;
	}

	if (param.funcheaderk != FUNCHEADERK_SymbolExpr)
	{
		// Parse ->

		bool arrowPresent = false;
		if (tryConsumeToken(pParser->pScanner, TOKENK_MinusGreater, ensurePendingToken(pParser)))
		{
			arrowPresent = true;
		}

		//
		// Parse out parameters if there was a ->
		//

		if (arrowPresent)
		{
			ParseParamListParam pplParam;
			init(&pplParam, param, PARAMK_Return);
			AstErr * pErr = tryParseParamList(pParser, pplParam);
			if (pErr)
			{
				return pErr;
			}
		}
	}
	else
	{
		// Parse trailing definition name

		if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

			auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
			append(&pErr->aTokenkValid, TOKENK_Identifier);
			return UpErr(pErr);
		}

		*param.paramSymbolExpr.poIdent = pParser->pPendingToken->lexeme;
	}

	// Success!

	// Everything has been set!
			
	return nullptr;
}

AstNode * finishParsePrimary(Parser * pParser, AstNode * pExpr)
{
	// Parse post-fix operations and allow them to chain

	int iStart = getStartEnd(pParser->astDecs, pExpr->astid).iStart;

	if (tryConsumeToken(pParser->pScanner, TOKENK_Dot, ensurePendingToken(pParser)))
	{
		// Member access

		return parseVarOrMemberVarSymbolExpr(pParser, pExpr);
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_Carat, ensurePendingToken(pParser)))
	{
		// Pointer dereference

		int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

		auto * pNode = AstNew(pParser, PointerDereferenceExpr, makeStartEnd(iStart, iEnd));
		pNode->pPointerExpr = pExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket, ensurePendingToken(pParser)))
	{
		// Array access

		static const bool s_mustBeIntLiteral = true;		// TODO: Support arbitrary expressions (semantic pass makes sure they are compile time const)
		AstNode * pSubscriptExpr = parseLiteralExpr(pParser, s_mustBeIntLiteral);
		if (isErrorNode(*pSubscriptExpr))
		{
			if (tryRecoverFromPanic(pParser, TOKENK_CloseBracket))
			{
				// Embed error in pSubscriptExpr

				goto arrayEnd;
			}
			else
			{
				// Couldn't recover

				auto * pErr = AstNewErr2Child(pParser, BubbleErr, gc_startEndBubble, pExpr, pSubscriptExpr);
				return Up(pErr);
			}
		}
		else if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
		{
			// Missing ]

			auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

			auto * pErr = AstNewErr2Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr, pSubscriptExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseBracket);

			if (tryRecoverFromPanic(pParser, TOKENK_CloseBracket))
			{
				// Embed error in pSubscriptExpr

				append(&UpErr(pErr)->apChildren, pSubscriptExpr);
				pSubscriptExpr = Up(pErr);
				goto arrayEnd;
			}
			else
			{
				// Couldn't recover

				return Up(pErr);
			}
		}

	arrayEnd:
		int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

		auto * pNode = AstNew(pParser, ArrayAccessExpr, makeStartEnd(iStart, iEnd));
		pNode->pArrayExpr = pExpr;
		pNode->pSubscriptExpr = pSubscriptExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// Func call

		DynamicArray<AstNode *> apArgs;
		init(&apArgs);
		Defer(Assert(!apArgs.pBuffer));		// buffer should get "moved" into AST

		bool isFirstArg = true;
		bool isCommaAlreadyAccountedFor = false;

		static const TOKENK s_aTokenkRecover[] = { TOKENK_Comma, TOKENK_CloseParen };

		while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			if (!isFirstArg && !isCommaAlreadyAccountedFor && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
			{
				// Missing ,

				auto startEndPrev = prevTokenStartEnd(pParser->pScanner);

				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
				append(&pErr->aTokenkValid, TOKENK_Comma);

				TOKENK tokenkRecover;
				if (tryRecoverFromPanic(pParser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
				{
					append(&apArgs, Up(pErr));	// Error node simply slots into an argument

					if (tokenkRecover == TOKENK_Comma)
					{
						// Do nothing (this substitutes as the comma we were missing)
					}
					else
					{
						Assert(tokenkRecover == TOKENK_CloseParen);
						break;;
					}
				}
				else
				{
					// Couldn't recover

					prepend(&apArgs, pExpr);
					reinitMove(&DownErr(pErr)->apChildren, &apArgs);
					return Up(pErr);
				}
			}

			isCommaAlreadyAccountedFor = false;		// For next iteration...

			// Parse expr

			AstNode * pExprArg = parseExpr(pParser);

			if (isErrorNode(*pExprArg))
			{
				// Erroneous expr

				TOKENK tokenkRecover;
				if (tryRecoverFromPanic(pParser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
				{
					// Slot error into arg if we recovered

					append(&apArgs, pExprArg);

					if (tokenkRecover == TOKENK_Comma)
					{
						isCommaAlreadyAccountedFor = true;
						continue;
					}
					else
					{
						Assert(tokenkRecover == TOKENK_CloseParen);
						break;;
					}
				}
				else
				{
					// Couldn't recover

					prepend(&apArgs, pExpr);
					auto * pErrNode = AstNewErrListChildMove(pParser, BubbleErr, gc_startEndBubble, &apArgs);
					return Up(pErrNode);
				}
			}
			else
			{
				// Add expr to args

				append(&apArgs, pExprArg);
			}

			isFirstArg = false;
		}

		int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;

		auto * pNode = AstNew(pParser, FuncCallExpr, makeStartEnd(iStart, iEnd));
		pNode->pFunc = pExpr;
		initMove(&pNode->apArgs, &apArgs);

		return finishParsePrimary(pParser, Up(pNode));
	}
	else
	{
		return pExpr;
	}
}

AstNode * parseFuncDefnStmtOrLiteralExpr(Parser * pParser, FUNCHEADERK funcheaderk)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);

	bool isDefn = (funcheaderk == FUNCHEADERK_Defn);

	int iStart = peekTokenStartEnd(pParser->pScanner).iStart;

	// Parse header
	
	// NOTE (andrew) Push scope before parsing header so that the symbols declared in the header
	//	are subsumed by the function's scope.
	
	Scope * pScopeOuter = pParser->pScopeCurrent;
	Scope * pScopeInner = pushScope(pParser, SCOPEK_CodeBlock);

	Defer(popScope(pParser));

	ParseFuncHeaderParam parseFuncHeaderParam;
	parseFuncHeaderParam.funcheaderk = funcheaderk;

	// NOTE (andrew) We break our normal pattern and allocate the node eagerly here because parseFuncHeaderGrp
	//	needs the node to fill its info out. We are responsible for the cleanup if we hit any errors.

	AstNode * pNodeUnderConstruction = nullptr;
	AstParamsReturnsGrp * pParamsReturnsUnderConstruction = nullptr;
	{
		StartEndIndices startEndPlaceholder(-1, -1);

		pParamsReturnsUnderConstruction = AstNew(pParser, ParamsReturnsGrp, startEndPlaceholder);;
		init(&pParamsReturnsUnderConstruction->apParamVarDecls);
		init(&pParamsReturnsUnderConstruction->apReturnVarDecls);

		if (isDefn)
		{
			auto * pDefnNodeUnderConstruction = AstNew(pParser, FuncDefnStmt, startEndPlaceholder);
			pDefnNodeUnderConstruction->pParamsReturnsGrp = pParamsReturnsUnderConstruction;
			parseFuncHeaderParam.paramDefn.pioNode = pDefnNodeUnderConstruction;

			pNodeUnderConstruction = Up(pDefnNodeUnderConstruction);
		}
		else
		{
			auto * pLiteralNodeUnderConstruction = AstNew(pParser, FuncLiteralExpr, startEndPlaceholder);
			pLiteralNodeUnderConstruction->pParamsReturnsGrp = pParamsReturnsUnderConstruction;
			parseFuncHeaderParam.paramLiteral.pioNode = pLiteralNodeUnderConstruction;

			pNodeUnderConstruction = Up(pLiteralNodeUnderConstruction);
		}
	}

	AstErr * pErr = tryParseFuncHeader(pParser, parseFuncHeaderParam);
	if (pErr)
	{
		pErr = UpErr(AstNewErr1Child(pParser, BubbleErr, gc_startEndBubble, Up(pErr)));
		goto LFailCleanup;
	}

	int iEndHeader = prevTokenStartEnd(pParser->pScanner).iEnd;
	decorate(&pParser->astDecs.startEndDecoration, Up(pParamsReturnsUnderConstruction)->astid, StartEndIndices(iStart, iEndHeader));

	// Parse { <stmts> } or do <stmt>

	const bool pushPopScope = false;
	AstNode * pBody = parseDoPseudoStmtOrBlockStmt(pParser, pushPopScope);

	if (isErrorNode(*pBody))
	{
		pErr = UpErr(AstNewErr1Child(pParser, BubbleErr, gc_startEndBubble, pBody));
		goto LFailCleanup;
	}

	// Success!

	// Replace our start/end placeholder

	int iEnd = prevTokenStartEnd(pParser->pScanner).iEnd;
	decorate(&pParser->astDecs.startEndDecoration, pNodeUnderConstruction->astid, StartEndIndices(iStart, iEnd));

	if (isDefn)
	{
		AstFuncDefnStmt * pNode = Down(pNodeUnderConstruction, FuncDefnStmt);
		pNode->pBodyStmt = pBody;
		pNode->ident.scopeid = pScopeOuter->id;
		pNode->scopeid = pScopeInner->id;

		SymbolInfo funcDefnInfo;
		funcDefnInfo.symbolk = SYMBOLK_Func;
		funcDefnInfo.funcData.pFuncDefnStmt = pNode;

		Assert(pNode->ident.lexeme.strv.cCh > 0);

		defineSymbol(pScopeOuter, pNode->ident.lexeme, funcDefnInfo);

		return Up(pNode);
	}
	else
	{
		AstFuncLiteralExpr * pNode = Down(pNodeUnderConstruction, FuncLiteralExpr);
		pNode->pBodyStmt = pBody;
		pNode->scopeid = pScopeInner->id;

		return Up(pNode);
	}

LFailCleanup:
	Assert(pErr);
	appendMultiple(&pErr->apChildren, pParamsReturnsUnderConstruction->apParamVarDecls);
	appendMultiple(&pErr->apChildren, pParamsReturnsUnderConstruction->apReturnVarDecls);

	dispose(&pParamsReturnsUnderConstruction->apParamVarDecls);
	dispose(&pParamsReturnsUnderConstruction->apReturnVarDecls);
	release(&pParser->astAlloc, Up(pParamsReturnsUnderConstruction));
	release(&pParser->astAlloc, pNodeUnderConstruction);

	return Up(pErr);
}

AstNode * handleScanOrUnexpectedTokenkErr(Parser * pParser, DynamicArray<AstNode *> * papChildren)
{
	Token * pErrToken =	 ensureAndClaimPendingToken(pParser);
	consumeToken(pParser->pScanner, pErrToken);

	if (pErrToken->tokenk == TOKENK_Error)
	{
		// Scan err

		if (papChildren)
		{
			auto * pErr = AstNewErrListChildMove(pParser, ScanErr, pErrToken->startEnd, papChildren);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr0Child(pParser, ScanErr, pErrToken->startEnd);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
	}
	else
	{
		// Unexpected tokenk err

		if (papChildren)
		{
			auto * pErr = AstNewErrListChildMove(pParser, UnexpectedTokenkErr, pErrToken->startEnd, papChildren);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr0Child(pParser, UnexpectedTokenkErr, pErrToken->startEnd);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
	}
}

Scope * pushScope(Parser * pParser, SCOPEK scopek)
{
	Assert(pParser->pScopeCurrent);

	Scope * pScope = allocate(&pParser->scopeAlloc);

	init(pScope, pParser->scopeidNext, scopek, pParser->pScopeCurrent);

	Assert(pParser->mpScopeidScope.cItem == pScope->id);
	append(&pParser->mpScopeidScope, pScope);

	pParser->pScopeCurrent = pScope;
	return pParser->pScopeCurrent;
}

Scope * popScope(Parser * pParser)
{
	Assert(pParser->pScopeCurrent);
	Assert(pParser->pScopeCurrent->pScopeParent);

	Scope * pScopeResult = pParser->pScopeCurrent;
	pParser->pScopeCurrent = pParser->pScopeCurrent->pScopeParent;

	return pScopeResult;
}

void reportScanAndParseErrors(const Parser & parser)
{
	for (int i = 0; i < parser.apErrorNodes.cItem; i++)
	{
		auto * pNode = parser.apErrorNodes[i];
		Assert(category(pNode->astk) == ASTCATK_Error);
		Assert(pNode->astk != ASTK_BubbleErr);

		switch (pNode->astk)
		{
			case ASTK_ScanErr:
			{
				auto * pErr = Down(pNode, ScanErr);
				Assert(pErr->pErrToken->tokenk == TOKENK_Error);
				Assert(pErr->pErrToken->grferrtok);

				ForFlag(FERRTOK, ferrtok)
				{
					if (pErr->pErrToken->grferrtok & ferrtok)
					{
						reportScanError(*parser.pScanner, *pErr->pErrToken, errMessageFromFerrtok(ferrtok));
					}
				}
			}
			break;

			case ASTK_BubbleErr:
			{
				AssertInfo(false, "Bubble errors should not be included in the error node list!");
			}
			break;

			case ASTK_UnexpectedTokenkErr:
			{
				auto * pErr = Down(pNode, UnexpectedTokenkErr);
				reportParseError(parser, *pNode, "Unexpected %s", g_mpTokenkStrDisplay[pErr->pErrToken->tokenk]);
			}
			break;

			case ASTK_ExpectedTokenkErr:
			{
				auto * pErr = Down(pNode, ExpectedTokenkErr);

				Assert(pErr->aTokenkValid.cItem > 0);

				if (pErr->aTokenkValid.cItem == 1)
				{
					reportParseError(parser, *pNode, "Expected %s", g_mpTokenkStrDisplay[pErr->aTokenkValid[0]]);
				}
				else if (pErr->aTokenkValid.cItem == 2)
				{
					reportParseError(parser, *pNode, "Expected %s or %s", g_mpTokenkStrDisplay[pErr->aTokenkValid[0]], g_mpTokenkStrDisplay[pErr->aTokenkValid[1]]);
				}
				else
				{
					String strError;
					init(&strError, "Expected ");
					append(&strError, g_mpTokenkStrDisplay[pErr->aTokenkValid[0]]);

					for (uint i = 1; i < pErr->aTokenkValid.cItem; i++)
					{
						append(&strError, ", ");

						bool isLast = (i == pErr->aTokenkValid.cItem - 1);
						if (isLast) append(&strError, "or ");


						append(&strError, g_mpTokenkStrDisplay[pErr->aTokenkValid[i]]);
					}

					reportParseError(parser, *pNode, strError.pBuffer);
				}
			}
			break;

			case ASTK_InitUnnamedVarErr:
			{
				reportParseError(parser, *pNode, "Cannot initialize unnamed variable");
			}
			break;

			case ASTK_ChainedAssignErr:
			{
				reportParseError(parser, *pNode, "Chaining assignments is not permitted");
			}
			break;

			case ASTK_IllegalDoPseudoStmtErr:
			{
				auto * pErr = Down(pNode, IllegalDoPseudoStmtErr);
				reportParseError(parser, *pNode, "%s is not permitted following 'do'", displayString(pErr->astkStmt, true /* capitalize */));
			}
			break;

			case ASTK_IllegalTopLevelStmtErr:
			{
				auto * pErr = Down(pNode, IllegalTopLevelStmtErr);
				reportParseError(parser, *pNode, "%s is not permitted as a top level statement", displayString(pErr->astkStmt, true /* capitalize */));
			}
			break;

			case ASTK_InvokeFuncLiteralErr:
			{
				reportParseError(parser, *pNode, "Function literal can not be directly invoked");
			}
			break;

			case ASTK_ArrowAfterFuncSymbolExpr:
			{
				reportParseError(parser, *pNode, "Unexpected '->' after function symbol. Only parameter types may be specified.");
			}
			break;

			default:
			{
				reportIceAndExit("Unknown parse error: %d", pNode->astk);
			}
			break;
		}
	}
}

AstNode * astNew(Parser * pParser, ASTK astk, StartEndIndices startEnd)
{
	AstNode * pNode = allocate(&pParser->astAlloc);
	pNode->astk = astk;
	pNode->astid = static_cast<ASTID>(pParser->iNode);
	pParser->iNode++;

	decorate(&pParser->astDecs.startEndDecoration, pNode->astid, startEnd);

	return pNode;
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, AstNode * pChild0, AstNode * pChild1, AstNode * pChild2)
{
	Assert(Implies(pChild2, pChild1));
	Assert(Implies(pChild1, pChild0));

	AstNode * apChildren[3] = { pChild0, pChild1, pChild2 };
	int cPChildren = (pChild2) ? 3 : (pChild1) ? 2 : (pChild0) ? 1 : 0;
	return astNewErr(pParser, astkErr, startEnd, apChildren, cPChildren);
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, AstNode * apChildren[], uint cPChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = DownErr(astNew(pParser, astkErr, startEnd));
	init(&pNode->apChildren);
	appendMultiple(&pNode->apChildren, apChildren, cPChildren);

#if DEBUG
	// Audit that we are only creating bubble errors if we are actually bubbling up error.

	if (astkErr == ASTK_BubbleErr)
	{
		bool hasErrorChild = false;
		for (int i = 0; i < pNode->apChildren.cItem; i++)
		{
			if (isErrorNode(*(pNode->apChildren[i])))
			{
				hasErrorChild = true;
				break;
			}
		}

		if (!hasErrorChild)
		{
			AssertInfo(false, "Bubble child by definition should have 1 or more error child(ren)");
		}
	}
#endif

	if (astkErr != ASTK_BubbleErr)
	{
		append(&pParser->apErrorNodes, Up(pNode));
	}

	return Up(pNode);
}

AstNode * astNewErrMoveChildren(Parser * pParser, ASTK astkErr, StartEndIndices startEnd, DynamicArray<AstNode *> * papChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = DownErr(astNew(pParser, astkErr, startEnd));
	initMove(&pNode->apChildren, papChildren);

#if DEBUG
	bool hasErrorChild = false;
	if (astkErr == ASTK_BubbleErr)
	{
		for (int i = 0; i < pNode->apChildren.cItem; i++)
		{
			if (isErrorNode(*(pNode->apChildren[i])))
			{
				hasErrorChild = true;
				break;
			}
		}

		if (!hasErrorChild)
		{
			AssertInfo(false, "Bubble child by definition should have 1 or more error child(ren)");
		}
	}
#endif

	if (astkErr != ASTK_BubbleErr)
	{
		append(&pParser->apErrorNodes, Up(pNode));
	}

	return Up(pNode);
}

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover)
{
	return tryRecoverFromPanic(pParser, &tokenkRecover, 1);
}

bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatch)
{
	// Consume tokens until we hit a recoverable token or a semicolon IN OUR CURRENT CONTEXT. New contexts are pushed
	//  when we consume (, {, or [. We need to pop any pushed context before we can match a recoverable token.

	// NOTE: Matching a semicolon returns true if it is one of our "recoverable" tokens, and
	//	false otherwise.

	// NOTE: The token that we successfully match to recover also gets consumed.

#if DEBUG
	for (int i = 0; i < cTokenkRecover; i++)
	{
		TOKENK tokenkRecover = aTokenkRecover[i];

		AssertInfo(
			tokenkRecover == TOKENK_Semicolon ||
			tokenkRecover == TOKENK_Comma ||
			tokenkRecover == TOKENK_CloseParen ||
			tokenkRecover == TOKENK_CloseBrace ||
			tokenkRecover == TOKENK_CloseBracket,
			"Only these tokens are supported right now"
		);
	}
#endif

	// NOTE: This will consume EOF token if it hits it!

	if (poTokenkMatch) *poTokenkMatch = TOKENK_Nil;

	int cParensPushed = 0;
	int cBracesPushed = 0;
	int cBracketsPushed = 0;

	while(true)
	{
		if (isFinished(pParser->pScanner)) return false;

		TOKENK tokenk = peekToken(pParser->pScanner);

		// Push contexts

		switch (tokenk)
		{
			case TOKENK_OpenParen:		cParensPushed++; break;
			case TOKENK_OpenBrace:		cBracesPushed++; break;
			case TOKENK_OpenBracket:	cBracketsPushed++; break;
		}

		bool isAnyContextPushed =
			cParensPushed != 0 ||
			cBracesPushed != 0 ||
			cBracketsPushed != 0;

		if (isAnyContextPushed)
		{
			// Pop contexts

			switch (tokenk)
			{
				case TOKENK_CloseParen:		cParensPushed = Max(cParensPushed - 1, 0); break;
				case TOKENK_CloseBrace:		cBracesPushed = Max(cBracesPushed - 1, 0); break;
				case TOKENK_CloseBracket:	cBracketsPushed = Max(cBracketsPushed - 1, 0); break;
			}
		}
		else
		{
			// Try to match!

			for (int i = 0; i < cTokenkRecover; i++)
			{
				TOKENK tokenkRecover = aTokenkRecover[i];

				if (tokenkRecover == tokenk)
				{
					if (poTokenkMatch) *poTokenkMatch = tokenk;
					consumeToken(pParser->pScanner);
					return true;
				}
			}

			if (tokenk == TOKENK_Semicolon)
			{
				consumeToken(pParser->pScanner);
				return false;
			}
		}

		// Keep chewing!

		consumeToken(pParser->pScanner);
	}
}

Token * ensurePendingToken(Parser * pParser)
{
	if (!pParser->pPendingToken)
	{
		pParser->pPendingToken = allocate(&pParser->tokenAlloc);
	}

	return pParser->pPendingToken;
}

Token * claimPendingToken(Parser * pParser)
{
	Assert(pParser->pPendingToken);

	Token * pResult = pParser->pPendingToken;
	pParser->pPendingToken = nullptr;
	return pResult;
}

Token * ensureAndClaimPendingToken(Parser * pParser)
{
	ensurePendingToken(pParser);
	return claimPendingToken(pParser);
}
