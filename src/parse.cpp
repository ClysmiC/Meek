#include "parse.h"

#include "error.h"
#include "global_context.h"
#include "scan.h"

// Absolutely sucks that I need to use 0, 1, 2 suffixes. I tried this approach to simulate default parameters in a macro but MSVC has a bug
//	with varargs expansion that made it blow up: https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros

#define AstNew(parser, astk, startEnd) reinterpret_cast<Ast##astk *>(astNew(parser, ASTK_##astk, startEnd))

#define AstNewErr0Child(parser, astkErr, startEnd) reinterpret_cast<Ast##astkErr *>(astNewErr(parser, ASTK_##astkErr, startEnd, nullptr, nullptr))
#define AstNewErr1Child(parser, astkErr, startEnd, pChild0) reinterpret_cast<Ast##astkErr *>(astNewErr(parser, ASTK_##astkErr, startEnd, pChild0, nullptr))
#define AstNewErr2Child(parser, astkErr, startEnd, pChild0, pChild1) reinterpret_cast<Ast##astkErr *>(astNewErr(parser, ASTK_##astkErr, startEnd, pChild0, pChild1))
#define AstNewErr3Child(parser, astkErr, startEnd, pChild0, pChild1, pChild2) reinterpret_cast<Ast##astkErr *>(astNewErr(parser, ASTK_##astkErr, startEnd, pChild0, pChild1, pChild2))

// NOTE: This macro is functionally equivalent to AstNewErr2Child since we rely on function overloading to choose the correct one.
//	I'm not really happy with this solution but it works for now.

#define AstNewErrListChild(parser, astkErr, startEnd, apChildren, cPChildren) reinterpret_cast<Ast##astkErr *>(astNewErr(parser, ASTK_##astkErr, startEnd, apChildren, cPChildren))
#define AstNewErrListChildMove(parser, astkErr, startEnd, papChildren) reinterpret_cast<Ast##astkErr *>(astNewErrMoveChildren(parser, ASTK_##astkErr, startEnd, papChildren))

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

void init(Parser * parser, MeekCtx * pCtx)
{
	parser->pCtx = pCtx;

	init(&parser->astAlloc);
	init(&parser->tokenAlloc);
	init(&parser->scopeAlloc);
	init(&parser->apErrorNodes);

	parser->pScopeBuiltin = allocate(&parser->scopeAlloc);
	init(parser->pScopeBuiltin, SCOPEID_BuiltIn, SCOPEK_BuiltIn, nullptr);

	parser->pScopeGlobal = allocate(&parser->scopeAlloc);
	init(parser->pScopeGlobal, SCOPEID_Global, SCOPEK_Global, parser->pScopeBuiltin);

	parser->pScopeCurrent = parser->pScopeGlobal;
	parser->scopeidNext = SCOPEID_UserDefinedStart;

	parser->varseqidNext = VARSEQID_Start;
}

AstNode * parseProgram(Parser * parser, bool * poSuccess)
{
	Scanner * scanner = parser->pCtx->scanner;
	AstDecorations * astDecorations = parser->pCtx->astDecorations;

	DynamicArray<AstNode *> apNodes;
	init(&apNodes);
	Defer(AssertInfo(!apNodes.pBuffer, "Should be moved into program AST node"));

	StartEndIndices startEnd;

	while (!isFinished(*scanner) && peekToken(scanner) != TOKENK_Eof)
	{
		AstNode * pNode = parseStmt(parser, PARSESTMTK_TopLevelStmt);

		if (isErrorNode(*pNode))
		{
			TOKENK tokenkPrev = prevToken(scanner);
			if (tokenkPrev != TOKENK_Semicolon)
			{
				bool recovered = tryRecoverFromPanic(parser, TOKENK_Semicolon);
				Assert(Implies(!recovered, isFinished(*scanner)));
			}
		}

		if (apNodes.cItem == 0)
		{
			auto startEndNode = getStartEnd(*astDecorations, pNode->astid);
			startEnd.iStart = startEndNode.iStart;
			startEnd.iEnd = startEndNode.iEnd;
		}
		else
		{
			auto startEndNode = getStartEnd(*astDecorations, pNode->astid);
			startEnd.iEnd = startEndNode.iEnd;
		}

		append(&apNodes, pNode);
	}

	auto * pNodeProgram = AstNew(parser, Program, startEnd);
	initMove(&pNodeProgram->apNodes, &apNodes);
	*poSuccess = (parser->apErrorNodes.cItem == 0);
	return Up(pNodeProgram);
}

AstNode * parseStmt(Parser * parser, PARSESTMTK parsestmtk)
{
	// HMM: need to think a lot about how fun and struct decl's look (or if they are even permitted!) in the following 3 cases
	//	-compile time immutable
	//	-runtime immutable
	//	-runtime mutable

	Scanner * scanner = parser->pCtx->scanner;
	AstDecorations * astDecorations = parser->pCtx->astDecorations;

	// Peek around a bit to figure out what kind of statement it is!

	TOKENK tokenkNext = peekToken(scanner, nullptr, 0);
	TOKENK tokenkNextNext = peekToken(scanner, nullptr, 1);

	if (tokenkNext == TOKENK_Struct)
	{
		// Struct defn

		auto * pNode = parseStructDefnStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_StructDefnStmt);

		if (parsestmtk == PARSESTMTK_DoPseudoStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalDoPseudoStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (parsestmtk != PARSESTMTK_DoPseudoStmt && tokenkNext == TOKENK_Fn && tokenkNextNext == TOKENK_Identifier)
	{
		// Func defn

		Assert(parsestmtk != PARSESTMTK_DoPseudoStmt);

		AstNode * pNode = parseFuncDefnStmt(parser);

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
		auto * pNode = parseIfStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_IfStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_While)
	{
		auto * pNode = parseWhileStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_WhileStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_OpenBrace)
	{
		// Block

		auto * pNode = parseBlockStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_BlockStmt);

		if (parsestmtk == PARSESTMTK_DoPseudoStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalDoPseudoStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
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

		bool isGlobal = (parsestmtk == PARSESTMTK_TopLevelStmt);
		auto * pNode = parseVarDeclStmt(parser, isGlobal ? VARDECLK_Global : VARDECLK_Local);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_VarDeclStmt);

		if (parsestmtk == PARSESTMTK_DoPseudoStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalDoPseudoStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Return)
	{
		// Return

		auto * pNode = parseReturnStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_ReturnStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Break)
	{
		// Break

		auto * pNode = parseBreakStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_BreakStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Continue)
	{
		// Continue

		auto * pNode = parseContinueStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_ContinueStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
	else if (tokenkNext == TOKENK_Print)
	{
		// Print

		auto * pNode = parsePrintStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_PrintStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
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

		auto * pNode = parseDoPseudoStmt(parser);
		
		if (isErrorNode(*pNode)) return pNode;

		return pNode;
	}
	else
	{
		// Expr stmt or Assignment stmt

		auto * pNode = parseExprStmtOrAssignStmt(parser);

		if (isErrorNode(*pNode)) return pNode;

		Assert(pNode->astk == ASTK_AssignStmt || pNode->astk == ASTK_ExprStmt);

		if (parsestmtk == PARSESTMTK_TopLevelStmt)
		{
			auto * pErr = AstNewErr1Child(parser, IllegalTopLevelStmtErr, getStartEnd(*astDecorations, pNode->astid), pNode);
			pErr->astkStmt = pNode->astk;
			return Up(pErr);
		}

		return pNode;
	}
}

AstNode * parseExprStmtOrAssignStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	// Parse lhs expression

	int iStart = peekTokenStartEnd(scanner).iStart;

	AstNode * pLhsExpr = parseExpr(parser);
	if (isErrorNode(*pLhsExpr))
	{
		return pLhsExpr;
	}

	// Check if it is an assignment

	bool isAssignment = false;
	AstNode * pRhsExpr = nullptr;

	static const TOKENK s_aTokenkAssign[] = {
		TOKENK_Equal,
		TOKENK_PlusEqual,
		TOKENK_MinusEqual,
		TOKENK_StarEqual,
		TOKENK_SlashEqual,
		TOKENK_PercentEqual
	};
	static const int s_cTokenkAssign = ArrayLen(s_aTokenkAssign);

	Token * pAssignToken = nullptr;
	if (tryConsumeToken(scanner, s_aTokenkAssign, s_cTokenkAssign, ensurePendingToken(parser)))
	{
		isAssignment = true;
		pAssignToken = claimPendingToken(parser);

		// Parse rhs expression

		pRhsExpr = parseExpr(parser);
		if (isErrorNode(*pRhsExpr))
		{
			auto * pErr = AstNewErr2Child(parser, BubbleErr, gc_startEndBubble, pLhsExpr, pRhsExpr);
			return Up(pErr);
		}

		if (tryConsumeToken(scanner, s_aTokenkAssign, s_cTokenkAssign))
		{
			// NOTE: This check isn't necessary for correctness, but it gives a better error message for a common case.

			auto * pErr = AstNewErr2Child(
							parser,
							ChainedAssignErr,
							makeStartEnd(iStart, prevTokenStartEnd(scanner).iEnd),
							pLhsExpr,
							pRhsExpr);

			return Up(pErr);
		}
	}

	// Parse semicolon

	if (!tryConsumeToken(scanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		if (isAssignment)
		{
			auto * pErr = AstNewErr2Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pLhsExpr, pRhsExpr);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pLhsExpr);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
	}

	int iEnd = prevTokenStartEnd(scanner).iEnd;
	auto startEnd = makeStartEnd(iStart, iEnd);

	// Success!

	if (isAssignment)
	{
		auto * pNode = AstNew(parser, AssignStmt, startEnd);

		pNode->pAssignToken = pAssignToken;
		pNode->pLhsExpr = pLhsExpr;
		pNode->pRhsExpr = pRhsExpr;
		return Up(pNode);
	}
	else
	{
		Assert(!pRhsExpr);
		Assert(!pAssignToken);

		auto * pNode = AstNew(parser, ExprStmt, startEnd);
		pNode->pExpr = pLhsExpr;
		return Up(pNode);
	}
}

AstNode * parseStructDefnStmt(Parser * parser)
{
	MeekCtx * pCtx = parser->pCtx;
	Scanner * scanner = pCtx->scanner;
	TypeTable * typeTable = pCtx->typeTable;

	Scope * pScopeDefn = parser->pScopeCurrent;
	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse 'struct'

	if (!tryConsumeToken(scanner, TOKENK_Struct))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Struct);
		return Up(pErr);
	}


	// Parse identifier

	if (!tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Identifier);
		return Up(pErr);
	}

	Token * pTokenIdent = claimPendingToken(parser);
	Assert(pTokenIdent->tokenk == TOKENK_Identifier);

	// Parse '{'

	if (!tryConsumeToken(scanner, TOKENK_OpenBrace))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		return Up(pErr);
	}

	Scope * pScopeIntroduced = pushScope(parser, SCOPEK_StructDefn);
	Defer(popScope(parser));

	// TODO: Allow nested struct definitions!

	// Parse vardeclstmt list, then '}'

	DynamicArray<AstNode *> apVarDeclStmt;
	init(&apVarDeclStmt);
	Defer(Assert(!apVarDeclStmt.pBuffer));

	while (!tryConsumeToken(scanner, TOKENK_CloseBrace))
	{
		// Parse member var decls

		AstNode * pVarDeclStmt = parseVarDeclStmt(parser, VARDECLK_Member);
		append(&apVarDeclStmt, pVarDeclStmt);

		if (isErrorNode(*pVarDeclStmt))
		{
			static const TOKENK s_aTokenkRecover[] = { TOKENK_CloseBrace, TOKENK_Semicolon };
			TOKENK tokenkRecover;
			if (tryRecoverFromPanic(parser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
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

			auto * pErr = AstNewErrListChildMove(parser, BubbleErr, gc_startEndBubble, &apVarDeclStmt);
			return Up(pErr);
		}
	}

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, StructDefnStmt, makeStartEnd(iStart, iEnd));
	pNode->ident.lexeme = pTokenIdent->lexeme;
	pNode->ident.scopeid = pScopeDefn->id;
	initMove(&pNode->apVarDeclStmt, &apVarDeclStmt);
	pNode->scopeid = pScopeIntroduced->id;

	// Insert into symbol table of enclosing scope

	SymbolInfo structDefnInfo;
	structDefnInfo.symbolk = SYMBOLK_Struct;
	structDefnInfo.structData.pStructDefnStmt = pNode;

	defineSymbol(pCtx, pScopeDefn, pNode->ident.lexeme, structDefnInfo);

	// Insert into type table

	Type type;
	init(&type, TYPEK_Named);

	type.namedTypeData.ident.lexeme = pNode->ident.lexeme;
	type.namedTypeData.ident.scopeid = pNode->ident.scopeid;

	const bool debugAssertIfAlreadyInTable = true;
	auto ensureResult = ensureInTypeTable(typeTable, &type, debugAssertIfAlreadyInTable);
	pNode->typidDefn = ensureResult.typid;

	return Up(pNode);
}

AstNode * parseVarDeclStmt(
	Parser * parser,
	VARDECLK vardeclk,
	NULLABLE PendingTypeId * poTypidPending)
{
	struct Expectations
	{
		EXPECTK name;
		EXPECTK semicolon;
	};

	const Expectations c_mpVardeclkExpectations[] =
	{
		//	name					semicolon

		{	EXPECTK_Required,		EXPECTK_Required	},		// VARDECLK_Global
		{	EXPECTK_Required,		EXPECTK_Required	},		// VARDECLK_Local
		{	EXPECTK_Optional,		EXPECTK_Forbidden	},		// VARDECLK_Param
		{	EXPECTK_Required,		EXPECTK_Required	},		// VARDECLK_Member
	};

	const Expectations & expectations = c_mpVardeclkExpectations[vardeclk];

	MeekCtx * pCtx = parser->pCtx;
	Scanner * scanner = pCtx->scanner;
	TypeTable * typeTable = pCtx->typeTable;

	AstErr * pErr = nullptr;
	Token * pTokenIdent = nullptr;

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse type

	// TypePendingResolve * pTypePendingResolve = nullptr;

	PendingTypeId pendingTypid = PendingTypeId::Nil;
	{
		ParseTypeResult parseTypeResult = tryParseType(parser);
		if (parseTypeResult.ptrk == PTRK_Error)
		{
			pErr = parseTypeResult.errorData.pErr;
			goto LFailCleanup;
		}

		pendingTypid = parseTypeResult.nonErrorData.pendingTypid;

		if (poTypidPending)
		{
			*poTypidPending = pendingTypid;
		}
	}

	// Parse name

	if (expectations.name == EXPECTK_Optional || expectations.name == EXPECTK_Required)
	{
		if (tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
		{
			pTokenIdent = claimPendingToken(parser);
			Assert(pTokenIdent->tokenk == TOKENK_Identifier);
		}
		else if (expectations.name == EXPECTK_Required)
		{
			auto startEndPrev = prevTokenStartEnd(scanner);

			auto * pExpectedTokenErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
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
	if (tryConsumeToken(scanner, TOKENK_Equal))
	{
		iEqualStart = prevTokenStartEnd(scanner).iStart;

		pInitExpr = parseExpr(parser);

		if (isErrorNode(*pInitExpr))
		{
			pErr = DownErr(pInitExpr);
			goto LFailCleanup;
		}

		if (tryConsumeToken(scanner, TOKENK_Equal))
		{
			// NOTE: This check isn't necessary for correctness, but it gives a better error message for a common case.

			pErr = UpErr(AstNewErr1Child(
							parser,
							ChainedAssignErr,
							makeStartEnd(iStart, prevTokenStartEnd(scanner).iEnd),
							pInitExpr));

			goto LFailCleanup;
		}
	}

	// Providing init expr isn't allowed if the var isn't named...

	if (!pTokenIdent && pInitExpr)
	{
		Assert(iEqualStart != -1);
		auto startEndPrev = prevTokenStartEnd(scanner);

		pErr = UpErr(AstNewErr1Child(parser, InitUnnamedVarErr, makeStartEnd(iEqualStart, startEndPrev.iEnd), pInitExpr));
		goto LFailCleanup;
	}

	// Parse semicolon

	if (expectations.semicolon == EXPECTK_Required)
	{
		if (!tryConsumeToken(scanner, TOKENK_Semicolon, ensurePendingToken(parser)))
		{
			auto startEndPrev = prevTokenStartEnd(scanner);

			auto * pErrExpectedTokenk = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pInitExpr);
			append(&pErrExpectedTokenk->aTokenkValid, TOKENK_Semicolon);

			pErr = UpErr(pErrExpectedTokenk);
			goto LFailCleanup;
		}
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, VarDeclStmt, makeStartEnd(iStart, iEnd));
	if (pTokenIdent)
	{
		pNode->ident.lexeme = pTokenIdent->lexeme;
		pNode->ident.scopeid = parser->pScopeCurrent->id;
	}
	else
	{
		setLexeme(&pNode->ident.lexeme, "");
		pNode->ident.scopeid = SCOPEID_Nil;
	}
	pNode->pInitExpr = pInitExpr;
	pNode->vardeclk = vardeclk;

	// Remember to poke in the typid once this type is resolved

	setPendingTypeUpdateOnResolvePtr(typeTable, pendingTypid, &pNode->typidDefn);

	if (pTokenIdent)
	{
		SymbolInfo varDeclInfo;
		varDeclInfo.symbolk = SYMBOLK_Var;
		varDeclInfo.varData.pVarDeclStmt = pNode;
		defineSymbol(pCtx, parser->pScopeCurrent, pTokenIdent->lexeme, varDeclInfo);
	}

	// Set seqid

	pNode->varseqid = parser->varseqidNext;
	parser->varseqidNext = VARSEQID(parser->varseqidNext + 1);

	return Up(pNode);

LFailCleanup:

	Assert(pErr);
	if (pTokenIdent)
	{
		release(&parser->tokenAlloc, pTokenIdent);
	}

	return Up(pErr);
}

AstNode * parseIfStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse 'if'

	if (!tryConsumeToken(scanner, TOKENK_If))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_If);
		return Up(pErr);
	}

	// Parse cond expr

	AstNode * pCondExpr = parseExpr(parser);
	if (isErrorNode(*pCondExpr))
	{
		return pCondExpr;
	}

	// Parse 'do <stmt>' or block stmt

	AstNode * pThenStmt = parseDoPseudoStmtOrBlockStmt(parser);
	if (isErrorNode(*pThenStmt))
	{
		auto * pErr = AstNewErr2Child(parser, BubbleErr, gc_startEndBubble, pCondExpr, pThenStmt);
		return Up(pErr);
	}

	// Try parse 'else' statement

	AstNode * pElseStmt = nullptr;
	if (tryConsumeToken(scanner, TOKENK_Else))
	{
		if (peekToken(scanner) == TOKENK_If)
		{
			pElseStmt = parseIfStmt(parser);
		}
		else
		{
			pElseStmt = parseDoPseudoStmtOrBlockStmt(parser);
		}
	}

	if (pElseStmt && isErrorNode(*pElseStmt))
	{
		auto * pErr = AstNewErr3Child(parser, BubbleErr, gc_startEndBubble, pCondExpr, pThenStmt, pElseStmt);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, IfStmt, makeStartEnd(iStart, iEnd));
	pNode->pCondExpr = pCondExpr;
	pNode->pThenStmt = pThenStmt;
	pNode->pElseStmt = pElseStmt;
	return Up(pNode);
}

AstNode * parseWhileStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse 'while'

	if (!tryConsumeToken(scanner, TOKENK_While))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd));
		append(&pErr->aTokenkValid, TOKENK_While);
		return Up(pErr);
	}

	// Parse cond expr

	AstNode * pCondExpr = parseExpr(parser);
	if (isErrorNode(*pCondExpr))
	{
		return pCondExpr;
	}

	// Parse 'do <stmt>' or block stmt

	AstNode * pBodyStmt = parseDoPseudoStmtOrBlockStmt(parser);
	if (isErrorNode(*pBodyStmt))
	{
		auto * pErr = AstNewErr2Child(parser, BubbleErr, gc_startEndBubble, pCondExpr, pBodyStmt);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, WhileStmt, makeStartEnd(iStart, iEnd));
	pNode->pCondExpr = pCondExpr;
	pNode->pBodyStmt = pBodyStmt;
	return Up(pNode);
}

AstNode * parseDoPseudoStmtOrBlockStmt(Parser * parser, bool pushPopScopeBlock)
{
	Scanner * scanner = parser->pCtx->scanner;

	TOKENK tokenk = peekToken(scanner);
	if (tokenk != TOKENK_OpenBrace &&
		tokenk != TOKENK_Do)
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		append(&pErr->aTokenkValid, TOKENK_Do);
		return Up(pErr);
	}

	AstNode * pStmt = nullptr;
	if (tokenk == TOKENK_OpenBrace)
	{
		pStmt = parseBlockStmt(parser, pushPopScopeBlock);
	}
	else
	{
		Verify(consumeToken(scanner) == TOKENK_Do);
		pStmt = parseStmt(parser, PARSESTMTK_DoPseudoStmt);
	}

	if (isErrorNode(*pStmt))
	{
		return pStmt;
	}

	// Success!

	return pStmt;
}

AstNode * parseDoPseudoStmt(Parser * parser)
{
	// NOTE (andrew) "do" is a psuedo statement in that it doesn't actually get it's own statement kind in
	//	the AST. It is literally just a statement that is restricted from being certain kinds of statements.
	//	It is used to enforce that there aren't nonsensical things like variable declarations in a one-stmt
	//	for loop. It is also used to tell the parser to take a particular branch when hitting a certain
	//	keyword. E.g., "fn" usually branches to parsing fn defn or var decls. But in an expression context,
	//	it can also be used to disambiguate a fn identifier, or define a fn literal. If you start a statement
	//	with such an expression, you need to use "do" to tell the parser that the "fn" isn't starting a defn/decl!

	Scanner * scanner = parser->pCtx->scanner;

	// Parse 'do'

	if (!tryConsumeToken(scanner, TOKENK_Do))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Do);
		return Up(pErr);
	}

	AstNode * pNode = parseStmt(parser, PARSESTMTK_DoPseudoStmt);

	if (isErrorNode(*pNode))
	{
		return pNode;
	}

	// Success!

	return pNode;
}

AstNode * parseBlockStmt(Parser * parser, bool pushPopScope)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse {

	if (!tryConsumeToken(scanner, TOKENK_OpenBrace))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		return Up(pErr);
	}

	// Assumption: any block stmt that pushes its own scope is of kind "FunctionInner"
	//	This assumption holds for the current spec, but may change in the future.

	if (pushPopScope) pushScope(parser, SCOPEK_FuncInner);
	Defer(if (pushPopScope) popScope(parser));

	// Parse statements until }

	DynamicArray<AstNode *> apStmts;
	init(&apStmts);
	Defer(Assert(!apStmts.pBuffer));		// buffer should get "moved" into AST

	while (!tryConsumeToken(scanner, TOKENK_CloseBrace))
	{
		AstNode * pStmt = parseStmt(parser);
		append(&apStmts, pStmt);

		if (isErrorNode(*pStmt))
		{
			static const TOKENK s_aTokenkRecover[] = { TOKENK_CloseBrace, TOKENK_Semicolon };
			TOKENK tokenkRecover;
			if (tryRecoverFromPanic(parser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
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

			auto * pErr = AstNewErrListChildMove(parser, BubbleErr, gc_startEndBubble, &apStmts);
			return Up(pErr);
		}
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, BlockStmt, makeStartEnd(iStart, iEnd));
	initMove(&pNode->apStmts, &apStmts);
	pNode->scopeid = parser->pScopeCurrent->id;
	pNode->inheritsParentScopeid = !pushPopScope;

	return Up(pNode);
}

AstNode * parseReturnStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse 'return'

	if (!tryConsumeToken(scanner, TOKENK_Return))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Return);
		return Up(pErr);
	}

	// Optionally parse expr

	AstNode * pExpr = nullptr;
	if (peekToken(scanner) != TOKENK_Semicolon)
	{
		pExpr = parseExpr(parser);

		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}
	}

	// Parse ';'

	if (!tryConsumeToken(scanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		AstExpectedTokenkErr * pErr;

		if (pExpr)
		{
			pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr);
		}
		else
		{
			pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		}

		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, ReturnStmt, makeStartEnd(iStart, iEnd));
	pNode->pExpr = pExpr;
	return Up(pNode);
}

AstNode * parseBreakStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	if (!tryConsumeToken(scanner, TOKENK_Break))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Break);
		return Up(pErr);
	}

	if (!tryConsumeToken(scanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, BreakStmt, makeStartEnd(iStart, iEnd));
	return Up(pNode);
}

AstNode * parseContinueStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	if (!tryConsumeToken(scanner, TOKENK_Continue))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Continue);
		return Up(pErr);
	}

	if (!tryConsumeToken(scanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, ContinueStmt, makeStartEnd(iStart, iEnd));
	return Up(pNode);
}

AstNode * parsePrintStmt(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	if (!tryConsumeToken(scanner, TOKENK_Print))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Print);
		return Up(pErr);
	}

	if (!tryConsumeToken(scanner, TOKENK_OpenParen))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_OpenParen);
		return Up(pErr);
	}

	AstNode * pExpr = parseExpr(parser);
	if (isErrorNode(*pExpr))
	{
		return pExpr;
	}

	if (!tryConsumeToken(scanner, TOKENK_CloseParen))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr);
		append(&pErr->aTokenkValid, TOKENK_CloseParen);
		return Up(pErr);
	}

	if (!tryConsumeToken(scanner, TOKENK_Semicolon))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr);
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	int iEnd = prevTokenStartEnd(scanner).iEnd;

	auto * pNode = AstNew(parser, PrintStmt, makeStartEnd(iStart, iEnd));
	pNode->pExpr = pExpr;
	return Up(pNode);
}

AstNode * parseFuncDefnStmt(Parser * parser)
{
	return parseFuncDefnStmtOrLiteralExpr(parser, FUNCHEADERK_Defn);
}

AstNode * parseExpr(Parser * parser)
{
	return parseBinop(parser, s_aParseOp[s_iParseOpMax - 1]);
}

AstNode * parseBinop(Parser * parser, const BinopInfo & op)
{
	auto oneStepLower = [](Parser * parser, int precedence) -> AstNode *
	{
		Assert(precedence >= 0);

		if (precedence == 0)
		{
			return parseUnopPre(parser);
		}
		else
		{
			const BinopInfo & opNext = s_aParseOp[precedence - 1];
			return parseBinop(parser, opNext);
		}
	};

	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	AstNode * pExpr = oneStepLower(parser, op.precedence);
	Assert(pExpr);

	// NOTE: Error check is done after we get a chance to parse rhs so that we can catch errors in both expressions. If we don't
	//	match a binop then we just want to return pExpr as is anyway, whether it is an error or not.

	while (tryConsumeToken(scanner, op.aTokenkMatch, op.cTokenMatch, ensurePendingToken(parser)))
	{
		Token * pOp = claimPendingToken(parser);

		AstNode * pRhsExpr = oneStepLower(parser, op.precedence);
		Assert(pRhsExpr);

		if (isErrorNode(*pExpr) || isErrorNode(*pRhsExpr))
		{
			auto * pErr = AstNewErr2Child(parser, BubbleErr, gc_startEndBubble, pExpr, pRhsExpr);
			return Up(pErr);
		}

		int iEnd = prevTokenStartEnd(scanner).iEnd;

		auto * pNode = AstNew(parser, BinopExpr, makeStartEnd(iStart, iEnd));
		pNode->pOp = pOp;
		pNode->pLhsExpr = pExpr;
		pNode->pRhsExpr = pRhsExpr;

		pExpr = Up(pNode);
	}

	return pExpr;
}

AstNode * parseUnopPre(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	if (tryConsumeToken(scanner, g_aTokenkUnopPre, g_cTokenkUnopPre, ensurePendingToken(parser)))
	{
		int iStart = prevTokenStartEnd(scanner).iStart;

		Token * pOp = claimPendingToken(parser);

		AstNode * pExpr = parseUnopPre(parser);
		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}

		int iEnd = prevTokenStartEnd(scanner).iEnd;

		auto * pNode = AstNew(parser, UnopExpr, makeStartEnd(iStart, iEnd));
		pNode->pOp = pOp;
		pNode->pExpr = pExpr;

		return Up(pNode);
	}
	else
	{
		return parsePrimary(parser);
	}
}

AstNode * parsePrimary(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	if (tryConsumeToken(scanner, TOKENK_OpenParen, ensurePendingToken(parser)))
	{
		// Group ( )

		Token * pOpenParen = claimPendingToken(parser);
		AstNode * pExpr = parseExpr(parser);

		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}

		if (!tryConsumeToken(scanner, TOKENK_CloseParen, ensurePendingToken(parser)))
		{
			auto startEndPrev = prevTokenStartEnd(scanner);

			auto * pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseParen);
			return Up(pErr);
		}

		int iEnd = prevTokenStartEnd(scanner).iEnd;

		auto * pNode = AstNew(parser, GroupExpr, makeStartEnd(iStart, iEnd));
		pNode->pExpr = pExpr;

		return finishParsePrimary(parser, Up(pNode));
	}
	else if (tryPeekToken(scanner, g_aTokenkLiteral, g_cTokenkLiteral))
	{
		// Literal

		static const bool s_mustBeIntLiteral = false;
		return parseLiteralExpr(parser, s_mustBeIntLiteral);
	}
	else if (peekToken(scanner) == TOKENK_Identifier)
	{
		// Identifier

		AstNode * pVarOwner = nullptr;
		return parseVarOrMemberVarSymbolExpr(parser, pVarOwner);
	}
	else if (peekToken(scanner) == TOKENK_Fn)
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
				TOKENK tokenkSpeculate = nextTokenkSpeculative(scanner);

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

							if (nextTokenkSpeculative(scanner) == TOKENK_Identifier)
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

					case TOKENK_Identifier:
					{
						if (cParenCtx == 0)
						{
							isFnSymbolExpr = true;
							speculate = false;
						}
					} break;
				}
			}

			backtrackAfterSpeculation(scanner);
		}

		if (isFnSymbolExpr)
		{
			AstNode * pNode = parseFuncSymbolExpr(parser);

			if (isErrorNode(*pNode))
			{
				return pNode;
			}

			// Success!

			return finishParsePrimary(parser, pNode);
		}
		else
		{
			// Func literal

			AstNode * pNode = parseFuncLiteralExpr(parser);

			if (isErrorNode(*pNode))
			{
				return pNode;
			}

			if (peekToken(scanner) == TOKENK_OpenParen)
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

				int iOpenParenStart = peekTokenStartEnd(scanner).iStart;

				auto * pErr = AstNewErr1Child(parser, InvokeFuncLiteralErr, makeStartEnd(iOpenParenStart), pNode);
				return Up(pErr);
			}

			// Success!

			return pNode;
		}
	}
	else
	{
		return handleScanOrUnexpectedTokenkErr(parser);
	}
}

AstNode * parseLiteralExpr(Parser * parser, bool mustBeIntLiteralk)
{
	Scanner * scanner = parser->pCtx->scanner;

	static const TOKENK s_tokenkIntLit = TOKENK_IntLiteral;

	const TOKENK * aTokenkValid = (mustBeIntLiteralk) ? &s_tokenkIntLit : g_aTokenkLiteral;
	const int cTokenkValid = (mustBeIntLiteralk) ? 1 : g_cTokenkLiteral;

	if (!tryConsumeToken(scanner, aTokenkValid, cTokenkValid, ensurePendingToken(parser)))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		AstExpectedTokenkErr * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		appendMultiple(&pErr->aTokenkValid, aTokenkValid, cTokenkValid);

		return Up(pErr);
	}

	Token * pToken = claimPendingToken(parser);

	auto * pExpr = AstNew(parser, LiteralExpr, pToken->startEnd);
	pExpr->pToken = pToken;
	pExpr->literalk = literalkFromTokenk(pExpr->pToken->tokenk);
	pExpr->isValueSet = false;
	pExpr->isValueErroneous = false;

	return Up(pExpr);
}

AstNode * parseFuncLiteralExpr(Parser * parser)
{
	return parseFuncDefnStmtOrLiteralExpr(parser, FUNCHEADERK_Literal);
}

AstNode * parseFuncSymbolExpr(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;
	TypeTable * typeTable = parser->pCtx->typeTable;

	int iStart = peekTokenStartEnd(scanner).iStart;

	TOKENK tokenkNext = peekToken(scanner);
	TOKENK tokenkNextNext = peekToken(scanner, nullptr, 1);

	Assert(tokenkNext == TOKENK_Fn);

	if (tokenkNextNext == TOKENK_Identifier)
	{
		Verify(tryConsumeToken(scanner, TOKENK_Fn));
		Verify(tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)));

		Token * pTokenIdent = claimPendingToken(parser);

		// fn <ident> specifies that we are talking about a function, but we still
		//	don't necessarily know exactly which one

		auto * pNode = AstNew(parser, SymbolExpr, makeStartEnd(iStart, pTokenIdent->startEnd.iEnd));
		pNode->ident = pTokenIdent->lexeme;
		pNode->symbexprk = SYMBEXPRK_Unresolved;
		pNode->unresolvedData.ignoreVars = true;
		init(&pNode->unresolvedData.aCandidates);

		return Up(pNode);
	}
	else if (tokenkNextNext == TOKENK_OpenParen)
	{
		Lexeme ident;
		DynamicArray<PendingTypeId> aPendingTypidParam;
		init(&aPendingTypidParam);
		Defer(dispose(&aPendingTypidParam));

		ParseFuncHeaderParam parseFuncHeaderParam;
		parseFuncHeaderParam.funcheaderk = FUNCHEADERK_SymbolExpr;
		parseFuncHeaderParam.paramSymbolExpr.paPendingTypidParam = &aPendingTypidParam;
		parseFuncHeaderParam.paramSymbolExpr.poIdent = &ident;

		AstErr * pErr = tryParseFuncHeader(parser, parseFuncHeaderParam);
		if (pErr)
		{
			return Up(pErr);
		}

		// Success!

		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pNode = AstNew(parser, SymbolExpr, makeStartEnd(iStart, startEndPrev.iEnd));
		pNode->ident = ident;
		pNode->symbexprk = SYMBEXPRK_Func;
		pNode->funcData.pDefnCached = nullptr;		// Not yet resolved
		init(&pNode->funcData.aTypidDisambig);

		for (int iTypeDisambig = 0; iTypeDisambig < parseFuncHeaderParam.paramSymbolExpr.paPendingTypidParam->cItem; iTypeDisambig++)
		{
			TypeId * pTypid = appendNew(&pNode->funcData.aTypidDisambig);
			*pTypid = TypeId::Unresolved;

			setPendingTypeUpdateOnResolvePtr(
				typeTable,
				(*parseFuncHeaderParam.paramSymbolExpr.paPendingTypidParam)[iTypeDisambig],
				pTypid);
		}

		return Up(pNode);
	}
	else
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Identifier);
		append(&pErr->aTokenkValid, TOKENK_OpenParen);

		return Up(pErr);
	}
}

AstNode * parseVarOrMemberVarSymbolExpr(Parser * parser, NULLABLE AstNode * pMemberOwnerExpr)
{
	Scanner * scanner = parser->pCtx->scanner;
	AstDecorations * astDecorations = parser->pCtx->astDecorations;

	if (!tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		AstExpectedTokenkErr * pErr;
		if (pMemberOwnerExpr)
		{
			pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pMemberOwnerExpr);
		}
		else
		{
			pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		}

		append(&pErr->aTokenkValid, TOKENK_Identifier);
		return Up(pErr);
	}

	Token * pIdent = claimPendingToken(parser);
	Assert(pIdent->tokenk == TOKENK_Identifier);

	// HMM: should the start/end range of a member var expr be just the start/end of the member? Or should it include the owner?

	int iStart = pIdent->startEnd.iStart;
	if (pMemberOwnerExpr)
	{
		iStart = getStartEnd(*astDecorations, pMemberOwnerExpr->astid).iStart;
	}

	auto * pNode = AstNew(parser, SymbolExpr, makeStartEnd(iStart, pIdent->startEnd.iEnd));

	pNode->ident = pIdent->lexeme;
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

	return finishParsePrimary(parser, Up(pNode));
}

ParseTypeResult tryParseType(Parser * parser)
{
	Scanner * scanner = parser->pCtx->scanner;
	TypeTable * typeTable = parser->pCtx->typeTable;

	Stack<TypeModifier> typemods;
	init(&typemods);
	Defer(dispose(&typemods));

	ParseTypeResult result;

	while (peekToken(scanner) != TOKENK_Identifier &&
		   peekToken(scanner) != TOKENK_Fn)
	{
		if (tryConsumeToken(scanner, TOKENK_OpenBracket))
		{
			// [

			auto * pSubscriptExpr = parseExpr(parser);

			if (isErrorNode(*pSubscriptExpr))
			{
				result.ptrk = PTRK_Error;
				result.errorData.pErr = DownErr(pSubscriptExpr);
				return result;
			}

			if (!tryConsumeToken(scanner, TOKENK_CloseBracket))
			{
				auto startEndPrev = prevTokenStartEnd(scanner);

				auto * pErr = AstNewErr1Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pSubscriptExpr);
				append(&pErr->aTokenkValid, TOKENK_CloseBracket);

				result.ptrk = PTRK_Error;
				result.errorData.pErr = UpErr(pErr);
				return result;
			}

			TypeModifier mod;
			mod.typemodk = TYPEMODK_Array;
			mod.pSubscriptExpr = pSubscriptExpr;
			push(&typemods, mod);
		}
		else if (tryConsumeToken(scanner, TOKENK_Carat))
		{
			// ^

			TypeModifier mod;
			mod.typemodk = TYPEMODK_Pointer;
			push(&typemods, mod);
		}
		else
		{
			AstNode * pErr = handleScanOrUnexpectedTokenkErr(parser, nullptr);

			result.ptrk = PTRK_Error;
			result.errorData.pErr = DownErr(pErr);
			return result;
		}
	}

	PendingTypeId pendingTypidUnmodified = PendingTypeId::Nil;
	{
		TOKENK tokenkPeek = peekToken(scanner);
		if (tokenkPeek == TOKENK_Identifier)
		{
			// Non-func type

			// Success!

			Token * pTokenIdent = ensureAndClaimPendingToken(parser);
			consumeToken(scanner, pTokenIdent);

			result.ptrk = PTRK_NonFuncType;
			pendingTypidUnmodified =
				registerPendingNamedType(
					typeTable,
					parser->pScopeCurrent,
					pTokenIdent->lexeme);
		}
		else if (tokenkPeek == TOKENK_Fn)
		{
			// Func type

			DynamicArray<PendingTypeId> aPendingTypidParam;
			init(&aPendingTypidParam);
			Defer(dispose(&aPendingTypidParam));

			DynamicArray<PendingTypeId> aPendingTypidReturn;
			init(&aPendingTypidReturn);
			Defer(dispose(&aPendingTypidReturn));

			ParseFuncHeaderParam param;
			param.funcheaderk = FUNCHEADERK_Type;
			param.paramType.paPendingTypidParam = &aPendingTypidParam;
			param.paramType.paPendingTypidReturn = &aPendingTypidReturn;

			AstErr * pErr = tryParseFuncHeader(parser, param);
			if (pErr)
			{
				result.ptrk = PTRK_Error;
				result.errorData.pErr = pErr;
				return result;
			}

			// Success!

			result.ptrk = PTRK_FuncType;
			pendingTypidUnmodified =
				registerPendingFuncType(
					typeTable,
					parser->pScopeCurrent,
					aPendingTypidParam,
					aPendingTypidReturn);
		}
		else
		{
			AstNode * pErr = handleScanOrUnexpectedTokenkErr(parser, nullptr);

			result.ptrk = PTRK_Error;
			result.errorData.pErr = DownErr(pErr);
			return result;
		}
	}

	Assert(pendingTypidUnmodified != PendingTypeId::Nil);

	while (!isEmpty(typemods))
	{
		TypeModifier typemod = pop(&typemods);
		pendingTypidUnmodified =
			registerPendingModType(
				typeTable,
				parser->pScopeCurrent,
				typemod,
				pendingTypidUnmodified);
	}

	result.nonErrorData.pendingTypid = pendingTypidUnmodified;
	return result;
}

NULLABLE AstErr * tryParseFuncHeader(Parser * parser, const ParseFuncHeaderParam & param)
{
	// TODO: Can probably rip out all of this ParseParamListParam glue and just use ParseFuncHeaderParam directly
	//	in the parse param list function.

	struct ParseParamListParam
	{
		FUNCHEADERK funcheaderk;
		PARAMK paramk;

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
				pPplParam->paramDefn.paPendingTypidParam = pfhParam.paramDefn.paPendingTypidParam;
				pPplParam->paramDefn.paPendingTypidReturn = pfhParam.paramDefn.paPendingTypidReturn;
			} break;

			case FUNCHEADERK_Literal:
			{
				pPplParam->paramLiteral.pioNode = pfhParam.paramLiteral.pioNode;
				pPplParam->paramLiteral.paPendingTypidParam = pfhParam.paramLiteral.paPendingTypidParam;
				pPplParam->paramLiteral.paPendingTypidReturn = pfhParam.paramLiteral.paPendingTypidReturn;
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

	auto tryParseParamList = [](Parser * parser, const ParseParamListParam & param) -> NULLABLE AstErr *
	{
		Scanner * scanner = parser->pCtx->scanner;

		Assert(Implies(param.funcheaderk == FUNCHEADERK_SymbolExpr, param.paramk == PARAMK_Param));

		// Parse (

		bool isNakedSingleReturn = false;
		if (!tryConsumeToken(scanner, TOKENK_OpenParen))
		{
			if (param.paramk != PARAMK_Return)
			{
				auto startEndPrev = prevTokenStartEnd(scanner);

				auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
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
			if (!isNakedSingleReturn && tryConsumeToken(scanner, TOKENK_CloseParen))
				break;

			if (isNakedSingleReturn && cParam > 0)
				break;

			// ,

			if (cParam > 0 && !tryConsumeToken(scanner, TOKENK_Comma))
			{
				auto startEndPrev = prevTokenStartEnd(scanner);

				auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
				append(&pErr->aTokenkValid, TOKENK_Comma);
				return UpErr(pErr);
			}

			if (param.funcheaderk == FUNCHEADERK_Defn || param.funcheaderk == FUNCHEADERK_Literal)
			{
				PendingTypeId pendingTypid;
				AstNode * pNode = parseVarDeclStmt(parser, VARDECLK_Param, &pendingTypid);
				if (isErrorNode(*pNode))
				{
					return DownErr(pNode);
				}

				if (param.funcheaderk == FUNCHEADERK_Defn)
				{
					if (param.paramk == PARAMK_Param)
					{
						append(&param.paramDefn.pioNode->pParamsReturnsGrp->apParamVarDecls, pNode);
						append(param.paramDefn.paPendingTypidParam, pendingTypid);
					}
					else
					{
						Assert(param.paramk == PARAMK_Return);
						append(&param.paramDefn.pioNode->pParamsReturnsGrp->apReturnVarDecls, pNode);
						append(param.paramDefn.paPendingTypidReturn, pendingTypid);
					}
				}
				else
				{
					Assert(param.funcheaderk == FUNCHEADERK_Literal);

					if (param.paramk == PARAMK_Param)
					{
						append(&param.paramLiteral.pioNode->pParamsReturnsGrp->apParamVarDecls, pNode);
						append(param.paramLiteral.paPendingTypidParam, pendingTypid);
					}
					else
					{
						Assert(param.paramk == PARAMK_Return);
						append(&param.paramLiteral.pioNode->pParamsReturnsGrp->apReturnVarDecls, pNode);
						append(param.paramLiteral.paPendingTypidReturn, pendingTypid);
					}
				}
			}
			else
			{
				// Type

				ParseTypeResult parseTypeResult = tryParseType(parser);
				if (parseTypeResult.ptrk == PTRK_Error)
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
						tryConsumeToken(scanner, TOKENK_Identifier);
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

	Scanner * scanner = parser->pCtx->scanner;

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse "fn"

	if (!tryConsumeToken(scanner, TOKENK_Fn))
	{
		auto startEndPrev = prevTokenStartEnd(scanner);

		auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
		append(&pErr->aTokenkValid, TOKENK_Fn);
		return UpErr(pErr);
	}

	// Parse definition name

	if (param.funcheaderk == FUNCHEADERK_Defn)
	{
		if (!tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
		{
			auto startEndPrev = prevTokenStartEnd(scanner);

			auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
			append(&pErr->aTokenkValid, TOKENK_Identifier);
			return UpErr(pErr);
		}

		param.paramDefn.pioNode->ident.lexeme = parser->pPendingToken->lexeme;
		param.paramDefn.pioNode->ident.scopeid = SCOPEID_Nil;		// NOTE (andrew) This gets set by caller
	}
	else if (param.funcheaderk == FUNCHEADERK_SymbolExpr)
	{
		if (tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
		{
			// Success! (short-circuited)

			*param.paramSymbolExpr.poIdent = parser->pPendingToken->lexeme;
			return nullptr;
		}
	}
	else
	{
		if (tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
		{
			// TODO: Maybe this should be a more specific/informative error, since we basically
			//	understand what they are trying to do. Something like NamedFuncNonDefnErr...?
			//	This is probably a good test case for adding more "context" to errors!!!

			Token * pErrToken = claimPendingToken(parser);
			Assert(pErrToken->tokenk == TOKENK_Identifier);

			auto * pErr = AstNewErr0Child(parser, UnexpectedTokenkErr, pErrToken->startEnd);
			pErr->pErrToken = pErrToken;
			return UpErr(pErr);
		}
	}

	// Parse "in" parameters

	ParseParamListParam pplParam;
	init(&pplParam, param, PARAMK_Param);
	AstErr * pErr = tryParseParamList(parser, pplParam);
	if (pErr)
	{
		return pErr;
	}

	if (param.funcheaderk != FUNCHEADERK_SymbolExpr)
	{
		// Parse ->

		bool arrowPresent = false;
		if (tryConsumeToken(scanner, TOKENK_MinusGreater, ensurePendingToken(parser)))
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
			AstErr * pErr = tryParseParamList(parser, pplParam);
			if (pErr)
			{
				return pErr;
			}
		}
	}
	else
	{
		// Parse trailing definition name

		if (!tryConsumeToken(scanner, TOKENK_Identifier, ensurePendingToken(parser)))
		{
			auto startEndPrev = prevTokenStartEnd(scanner);

			auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
			append(&pErr->aTokenkValid, TOKENK_Identifier);
			return UpErr(pErr);
		}

		*param.paramSymbolExpr.poIdent = parser->pPendingToken->lexeme;
	}

	// Success!

	// Everything has been set!
			
	return nullptr;
}

AstNode * finishParsePrimary(Parser * parser, AstNode * pExpr)
{
	// Parse post-fix operations and allow them to chain

	Scanner * scanner = parser->pCtx->scanner;
	AstDecorations * astDecorations = parser->pCtx->astDecorations;

	int iStart = getStartEnd(*astDecorations, pExpr->astid).iStart;

	if (tryConsumeToken(scanner, TOKENK_Dot, ensurePendingToken(parser)))
	{
		// Member access

		return parseVarOrMemberVarSymbolExpr(parser, pExpr);
	}
	else if (tryConsumeToken(scanner, TOKENK_Carat, ensurePendingToken(parser)))
	{
		// Pointer dereference

		int iEnd = prevTokenStartEnd(scanner).iEnd;

		auto * pNode = AstNew(parser, PointerDereferenceExpr, makeStartEnd(iStart, iEnd));
		pNode->pPointerExpr = pExpr;

		return finishParsePrimary(parser, Up(pNode));
	}
	else if (tryConsumeToken(scanner, TOKENK_OpenBracket, ensurePendingToken(parser)))
	{
		// Array access

		static const bool s_mustBeIntLiteral = true;		// TODO: Support arbitrary expressions (semantic pass makes sure they are compile time const)
		AstNode * pSubscriptExpr = parseLiteralExpr(parser, s_mustBeIntLiteral);
		if (isErrorNode(*pSubscriptExpr))
		{
			if (tryRecoverFromPanic(parser, TOKENK_CloseBracket))
			{
				// Embed error in pSubscriptExpr

				goto arrayEnd;
			}
			else
			{
				// Couldn't recover

				auto * pErr = AstNewErr2Child(parser, BubbleErr, gc_startEndBubble, pExpr, pSubscriptExpr);
				return Up(pErr);
			}
		}
		else if (!tryConsumeToken(scanner, TOKENK_CloseBracket, ensurePendingToken(parser)))
		{
			// Missing ]

			auto startEndPrev = prevTokenStartEnd(scanner);

			auto * pErr = AstNewErr2Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1), pExpr, pSubscriptExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseBracket);

			if (tryRecoverFromPanic(parser, TOKENK_CloseBracket))
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
		int iEnd = prevTokenStartEnd(scanner).iEnd;

		auto * pNode = AstNew(parser, ArrayAccessExpr, makeStartEnd(iStart, iEnd));
		pNode->pArrayExpr = pExpr;
		pNode->pSubscriptExpr = pSubscriptExpr;

		return finishParsePrimary(parser, Up(pNode));
	}
	else if (tryConsumeToken(scanner, TOKENK_OpenParen, ensurePendingToken(parser)))
	{
		// Func call

		DynamicArray<AstNode *> apArgs;
		init(&apArgs);
		Defer(Assert(!apArgs.pBuffer));		// buffer should get "moved" into AST

		bool isFirstArg = true;
		bool isCommaAlreadyAccountedFor = false;

		static const TOKENK s_aTokenkRecover[] = { TOKENK_Comma, TOKENK_CloseParen };

		while (!tryConsumeToken(scanner, TOKENK_CloseParen, ensurePendingToken(parser)))
		{
			if (!isFirstArg && !isCommaAlreadyAccountedFor && !tryConsumeToken(scanner, TOKENK_Comma))
			{
				// Missing ,

				auto startEndPrev = prevTokenStartEnd(scanner);

				auto * pErr = AstNewErr0Child(parser, ExpectedTokenkErr, makeStartEnd(startEndPrev.iEnd + 1));
				append(&pErr->aTokenkValid, TOKENK_Comma);

				TOKENK tokenkRecover;
				if (tryRecoverFromPanic(parser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
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

			AstNode * pExprArg = parseExpr(parser);

			if (isErrorNode(*pExprArg))
			{
				// Erroneous expr

				TOKENK tokenkRecover;
				if (tryRecoverFromPanic(parser, s_aTokenkRecover, ArrayLen(s_aTokenkRecover), &tokenkRecover))
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
					auto * pErrNode = AstNewErrListChildMove(parser, BubbleErr, gc_startEndBubble, &apArgs);
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

		int iEnd = prevTokenStartEnd(scanner).iEnd;

		auto * pNode = AstNew(parser, FuncCallExpr, makeStartEnd(iStart, iEnd));
		pNode->pFunc = pExpr;
		initMove(&pNode->apArgs, &apArgs);

		return finishParsePrimary(parser, Up(pNode));
	}
	else
	{
		return pExpr;
	}
}

AstNode * parseFuncDefnStmtOrLiteralExpr(Parser * parser, FUNCHEADERK funcheaderk)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);

	MeekCtx * pCtx = parser->pCtx;
	Scanner * scanner = pCtx->scanner;
	TypeTable * typeTable = pCtx->typeTable;
	AstDecorations * astDecorations = pCtx->astDecorations;

	bool isDefn = (funcheaderk == FUNCHEADERK_Defn);

	int iStart = peekTokenStartEnd(scanner).iStart;

	// Parse header
	
	// NOTE (andrew) Push scope before parsing header so that the symbols declared in the header
	//	are subsumed by the function's scope.
	
	Scope * pScopeOuter = parser->pScopeCurrent;
	Scope * pScopeInner = pushScope(parser, SCOPEK_FuncTopLevel);

	Defer(popScope(parser));

	ParseFuncHeaderParam parseFuncHeaderParam;
	parseFuncHeaderParam.funcheaderk = funcheaderk;

	// TODO (andrew) Could probably do away with the concept of PendingTYPID and have the high bit
	//	flag if it is currently pending. Then, instead of passing around a bunch of pendingtypid glue
	//	for defns and literals, we could just inspect the vardecls!

	DynamicArray<PendingTypeId> aPendingTypidParam;
	init(&aPendingTypidParam);
	Defer(dispose(&aPendingTypidParam));

	DynamicArray<PendingTypeId> aPendingTypidReturn;
	init(&aPendingTypidReturn);
	Defer(dispose(&aPendingTypidReturn));

	// NOTE (andrew) We break our normal pattern and allocate the node eagerly here because parseFuncHeaderGrp
	//	needs the node to fill its info out. We are responsible for the cleanup if we hit any errors.

	AstNode * pNodeUnderConstruction = nullptr;
	AstParamsReturnsGrp * pParamsReturnsUnderConstruction = nullptr;
	{
		StartEndIndices startEndPlaceholder(-1, -1);

		pParamsReturnsUnderConstruction = AstNew(parser, ParamsReturnsGrp, startEndPlaceholder);;
		init(&pParamsReturnsUnderConstruction->apParamVarDecls);
		init(&pParamsReturnsUnderConstruction->apReturnVarDecls);

		if (isDefn)
		{
			auto * pDefnNodeUnderConstruction = AstNew(parser, FuncDefnStmt, startEndPlaceholder);
			pDefnNodeUnderConstruction->pParamsReturnsGrp = pParamsReturnsUnderConstruction;
			parseFuncHeaderParam.paramDefn.pioNode = pDefnNodeUnderConstruction;
			parseFuncHeaderParam.paramDefn.paPendingTypidParam = &aPendingTypidParam;
			parseFuncHeaderParam.paramDefn.paPendingTypidReturn = &aPendingTypidReturn;

			pNodeUnderConstruction = Up(pDefnNodeUnderConstruction);
		}
		else
		{
			auto * pLiteralNodeUnderConstruction = AstNew(parser, FuncLiteralExpr, startEndPlaceholder);
			pLiteralNodeUnderConstruction->pParamsReturnsGrp = pParamsReturnsUnderConstruction;
			parseFuncHeaderParam.paramLiteral.pioNode = pLiteralNodeUnderConstruction;
			parseFuncHeaderParam.paramLiteral.paPendingTypidParam = &aPendingTypidParam;
			parseFuncHeaderParam.paramLiteral.paPendingTypidReturn = &aPendingTypidReturn;

			pNodeUnderConstruction = Up(pLiteralNodeUnderConstruction);
		}
	}

	AstErr * pErr = tryParseFuncHeader(parser, parseFuncHeaderParam);
	if (pErr)
	{
		pErr = UpErr(AstNewErr1Child(parser, BubbleErr, gc_startEndBubble, Up(pErr)));
		goto LFailCleanup;
	}

	int iEndHeader = prevTokenStartEnd(scanner).iEnd;
	decorate(&astDecorations->startEndDecoration, Up(pParamsReturnsUnderConstruction)->astid, StartEndIndices(iStart, iEndHeader));

	// Parse { <stmts> } or do <stmt>

	const bool pushPopScope = false;
	AstNode * pBody = parseDoPseudoStmtOrBlockStmt(parser, pushPopScope);

	if (isErrorNode(*pBody))
	{
		pErr = UpErr(AstNewErr1Child(parser, BubbleErr, gc_startEndBubble, pBody));
		goto LFailCleanup;
	}

	// Success!

	// Replace our start/end placeholder

	int iEnd = prevTokenStartEnd(scanner).iEnd;
	decorate(&astDecorations->startEndDecoration, pNodeUnderConstruction->astid, StartEndIndices(iStart, iEnd));

	AstNode * pNodeResult = nullptr;
	if (isDefn)
	{
		AstFuncDefnStmt * pNode = Down(pNodeUnderConstruction, FuncDefnStmt);
		pNode->pBodyStmt = pBody;
		pNode->ident.scopeid = pScopeOuter->id;
		pNode->scopeid = pScopeInner->id;
		pNode->funcid = FuncId(pCtx->functions.cItem);

		SymbolInfo funcDefnInfo;
		funcDefnInfo.symbolk = SYMBOLK_Func;
		funcDefnInfo.funcData.pFuncDefnStmt = pNode;

		Assert(pNode->ident.lexeme.strv.cCh > 0);

		defineSymbol(pCtx, pScopeOuter, pNode->ident.lexeme, funcDefnInfo);
		registerPendingFuncType(
			typeTable,
			pScopeOuter,
			aPendingTypidParam,
			aPendingTypidReturn,
			&pNode->typidDefn);

		pNodeResult = Up(pNode);
	}
	else
	{
		AstFuncLiteralExpr * pNode = Down(pNodeUnderConstruction, FuncLiteralExpr);
		pNode->pBodyStmt = pBody;
		pNode->scopeid = pScopeInner->id;
		pNode->funcid = FuncId(pCtx->functions.cItem);

		registerPendingFuncType(
			typeTable,
			pScopeOuter,
			aPendingTypidParam,
			aPendingTypidReturn,
			&UpExpr(pNode)->typidEval);

		pNodeResult = Up(pNode);
	}

	Assert(funcid(*pNodeResult) == FuncId(pCtx->functions.cItem));
	append(&pCtx->functions, pNodeResult);
	return pNodeResult;

LFailCleanup:
	Assert(pErr);
	appendMultiple(&pErr->apChildren, pParamsReturnsUnderConstruction->apParamVarDecls);
	appendMultiple(&pErr->apChildren, pParamsReturnsUnderConstruction->apReturnVarDecls);

	dispose(&pParamsReturnsUnderConstruction->apParamVarDecls);
	dispose(&pParamsReturnsUnderConstruction->apReturnVarDecls);
	release(&parser->astAlloc, Up(pParamsReturnsUnderConstruction));
	release(&parser->astAlloc, pNodeUnderConstruction);

	return Up(pErr);
}

AstNode * handleScanOrUnexpectedTokenkErr(Parser * parser, DynamicArray<AstNode *> * papChildren)
{
	Scanner * scanner = parser->pCtx->scanner;

	Token * pErrToken =	 ensureAndClaimPendingToken(parser);
	consumeToken(scanner, pErrToken);

	if (pErrToken->tokenk == TOKENK_Error)
	{
		// Scan err

		if (papChildren)
		{
			auto * pErr = AstNewErrListChildMove(parser, ScanErr, pErrToken->startEnd, papChildren);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr0Child(parser, ScanErr, pErrToken->startEnd);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
	}
	else
	{
		// Unexpected tokenk err

		if (papChildren)
		{
			auto * pErr = AstNewErrListChildMove(parser, UnexpectedTokenkErr, pErrToken->startEnd, papChildren);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr0Child(parser, UnexpectedTokenkErr, pErrToken->startEnd);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
	}
}

Scope * pushScope(Parser * parser, SCOPEK scopek)
{
	MeekCtx * pCtx = parser->pCtx;

	Assert(parser->pScopeCurrent);

	Scope * pScope = allocate(&parser->scopeAlloc);

	init(pScope, parser->scopeidNext, scopek, parser->pScopeCurrent);

	Assert(pCtx->scopes.cItem == pScope->id);
	append(&pCtx->scopes, pScope);

	parser->pScopeCurrent = pScope;
	parser->scopeidNext = SCOPEID(parser->scopeidNext + 1);

	return parser->pScopeCurrent;
}

Scope * popScope(Parser * parser)
{
	Assert(parser->pScopeCurrent);
	Assert(parser->pScopeCurrent->pScopeParent);

	Scope * pScopeResult = parser->pScopeCurrent;
	parser->pScopeCurrent = parser->pScopeCurrent->pScopeParent;

	return pScopeResult;
}

void reportScanAndParseErrors(const Parser & parser)
{
	Scanner * scanner = parser.pCtx->scanner;

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
						reportScanError(*scanner, *pErr->pErrToken, errMessageFromFerrtok(ferrtok));
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

			default:
			{
				AssertNotReached;
			}
			break;
		}
	}
}

AstNode * astNew(Parser * parser, ASTK astk, StartEndIndices startEnd)
{
	AstDecorations * astDecorations = parser->pCtx->astDecorations;

	AstNode * pNode = allocate(&parser->astAlloc);
	pNode->astk = astk;
	pNode->astid = static_cast<ASTID>(parser->iNode);
	parser->iNode++;

	decorate(&astDecorations->startEndDecoration, pNode->astid, startEnd);

	return pNode;
}

AstNode * astNewErr(Parser * parser, ASTK astkErr, StartEndIndices startEnd, AstNode * pChild0, AstNode * pChild1, AstNode * pChild2)
{
	Assert(Implies(pChild2, pChild1));
	Assert(Implies(pChild1, pChild0));

	AstNode * apChildren[3] = { pChild0, pChild1, pChild2 };
	int cPChildren = (pChild2) ? 3 : (pChild1) ? 2 : (pChild0) ? 1 : 0;
	return astNewErr(parser, astkErr, startEnd, apChildren, cPChildren);
}

AstNode * astNewErr(Parser * parser, ASTK astkErr, StartEndIndices startEnd, AstNode * apChildren[], uint cPChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = DownErr(astNew(parser, astkErr, startEnd));
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
		append(&parser->apErrorNodes, Up(pNode));
	}

	return Up(pNode);
}

AstNode * astNewErrMoveChildren(Parser * parser, ASTK astkErr, StartEndIndices startEnd, DynamicArray<AstNode *> * papChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = DownErr(astNew(parser, astkErr, startEnd));
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
		append(&parser->apErrorNodes, Up(pNode));
	}

	return Up(pNode);
}

bool tryRecoverFromPanic(Parser * parser, TOKENK tokenkRecover)
{
	return tryRecoverFromPanic(parser, &tokenkRecover, 1);
}

bool tryRecoverFromPanic(Parser * parser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatch)
{
	// Consume tokens until we hit a recoverable token or a semicolon IN OUR CURRENT CONTEXT. New contexts are pushed
	//  when we consume (, {, or [. We need to pop any pushed context before we can match a recoverable token.

	// NOTE: Matching a semicolon returns true if it is one of our "recoverable" tokens, and
	//	false otherwise.

	// NOTE: The token that we successfully match to recover also gets consumed.

	Scanner * scanner = parser->pCtx->scanner;

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
		if (isFinished(*scanner)) return false;

		TOKENK tokenk = peekToken(scanner);

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
					consumeToken(scanner);
					return true;
				}
			}

			if (tokenk == TOKENK_Semicolon)
			{
				consumeToken(scanner);
				return false;
			}
		}

		// Keep chewing!

		consumeToken(scanner);
	}
}

Token * ensurePendingToken(Parser * parser)
{
	if (!parser->pPendingToken)
	{
		parser->pPendingToken = allocate(&parser->tokenAlloc);
	}

	return parser->pPendingToken;
}

Token * claimPendingToken(Parser * parser)
{
	Assert(parser->pPendingToken);

	Token * pResult = parser->pPendingToken;
	parser->pPendingToken = nullptr;
	return pResult;
}

Token * ensureAndClaimPendingToken(Parser * parser)
{
	ensurePendingToken(parser);
	return claimPendingToken(parser);
}
