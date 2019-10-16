#include "parse.h"

#include "scan.h"

// Absolutely sucks that I need to use 0, 1, 2 suffixes. I tried this approach to simulate default parameters in a macro but MSVC has a bug
//	with varargs expansion that made it blow up: https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros

#define AstNewErr0Child(pParser, astkErr, line) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, line, nullptr, nullptr))
#define AstNewErr1Child(pParser, astkErr, line, pChild0) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, line, pChild0, nullptr))
#define AstNewErr2Child(pParser, astkErr, line, pChild0, pChild1) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, line, pChild0, pChild1))
#define AstNewErr3Child(pParser, astkErr, line, pChild0, pChild1, pChild2) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, line, pChild0, pChild1, pChild2))

// NOTE: This macro is functionally equivalent to AstNewErr2Child since we rely on function overloading to choose the correct one.
//	I'm not really happy with this solution but it's the best I have right now.

#define AstNewErrListChild(pParser, astkErr, line, apChildren, cPChildren) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, line, apChildren, cPChildren))
#define AstNewErrListChildMove(pParser, astkErr, line, papChildren) reinterpret_cast<Ast##astkErr *>(astNewErrMoveChildren(pParser, ASTK_##astkErr, line, papChildren))

// TODO: should assignment statement be in here?
// What about . operator?
//	I handle . already, but if I want to support something like "stringLiteral".length,
//	then I would need to handle it like an operator.

static const BinopInfo s_aParseOp[] = {
	{ 0, { TOKENK_Star, TOKENK_Slash, TOKENK_Percent }, 3 },
	{ 1, { TOKENK_Plus, TOKENK_Minus }, 2 },
	{ 2, { TOKENK_Lesser, TOKENK_Greater, TOKENK_LesserEqual, TOKENK_GreaterEqual }, 4 },
	{ 3, { TOKENK_EqualEqual, TOKENK_BangEqual }, 2 },
	{ 4, { TOKENK_Amp }, 1 },
	{ 5, { TOKENK_Pipe }, 1 },
	{ 6, { TOKENK_AmpAmp }, 1 },
	{ 7, { TOKENK_PipePipe }, 1 },
};
static constexpr int s_iParseOpMax = ArrayLen(s_aParseOp);

bool init(Parser * pParser, Scanner * pScanner)
{
	if (!pParser || !pScanner) return false;

	pParser->pScanner = pScanner;
	init(&pParser->astAlloc);
	init(&pParser->tokenAlloc);
	init(&pParser->typeAlloc);
	init(&pParser->funcTypeAlloc);
	// init(&pParser->symbolInfoAlloc);
	init(&pParser->astNodes);
	init(&pParser->scopeStack);
	init(&pParser->symbolTable);

	return true;
}

AstNode * parseProgram(Parser * pParser, bool * poSuccess)
{
	// NOTE: Empty program is valid to parse, but I may still decide that
	//	that is a semantic error.

	pushScope(pParser, SCOPEK_BuiltIn);
	Assert(peekScope(pParser).id == gc_builtInScopeid);

	// TOOD: put built-in names in the symbol table (int, float, etc.)

	pushScope(pParser, SCOPEK_Global);
	Assert(peekScope(pParser).id == gc_globalScopeid);

	auto * pNode = AstNew(pParser, Program, 0);
	init(&pNode->apNodes);

	parseStmts(pParser, &pNode->apNodes);

	*poSuccess = pParser->hadError;
	return Up(pNode);
}

void parseStmts(Parser * pParser, DynamicArray<AstNode *> * poAPNodes)
{
	while (!isFinished(pParser->pScanner) && peekToken(pParser->pScanner) != TOKENK_Eof)
	{
		AstNode * pNode = parseStmt(pParser);

		if (isErrorNode(*pNode))
		{
			// NOTE: should move this recoverFromPanic to the parseXXXStmt functions
			//	themselves. Otherwise errors will always bubble up to here which is
			//	inconsistent with how they behave in expression contexts.

			bool recovered = tryRecoverFromPanic(pParser, TOKENK_Semicolon);

			Assert(Implies(!recovered, isFinished(pParser->pScanner)));
		}

		append(poAPNodes, pNode);
	}
}

AstNode * parseStmt(Parser * pParser, bool isDoStmt)
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

		if (!isDoStmt) return parseStructDefnStmt(pParser);
		else
		{
			auto * pErr = AstNewErr0Child(pParser, IllegalDoStmtErr, peekTokenLine(pParser->pScanner));
			pErr->astkStmt = ASTK_StructDefnStmt;
			return Up(pErr);
		}
	}
	else if (tokenkNext == TOKENK_Func && tokenkNextNext == TOKENK_Identifier)
	{
		// Func defn

		if (!isDoStmt)
		{
			AstNode * pNode;
			tryParseFuncDefnStmtOrLiteralExpr(pParser, FUNCHEADERK_Defn, &pNode);
			return pNode;
		}
		else
		{
			auto * pErr = AstNewErr0Child(pParser, IllegalDoStmtErr, peekTokenLine(pParser->pScanner));
			pErr->astkStmt = ASTK_FuncDefnStmt;
			return Up(pErr);
		}
	}
	else if (tokenkNext == TOKENK_Enum && tokenkNextNext == TOKENK_Identifier)
	{
		// Enum defn

		// TODO
	}
	//else if (tokenkNext == TOKENK_Union && tokenkNextNext == TOKENK_Identifier)
	//{
	//	// Union defn

	//	// TODO
	//}
	else if (tokenkNext == TOKENK_If)
	{
		return parseIfStmt(pParser);
	}
	else if (tokenkNext == TOKENK_While)
	{
		return parseWhileStmt(pParser);
	}
	else if (tokenkNext == TOKENK_OpenBrace)
	{
		if (!isDoStmt) return parseBlockStmt(pParser);
		else
		{
			auto * pErr = AstNewErr0Child(pParser, IllegalDoStmtErr, peekTokenLine(pParser->pScanner));
			pErr->astkStmt = ASTK_BlockStmt;
			return Up(pErr);
		}
	}
	else if (tokenkNext == TOKENK_OpenBracket ||
			 tokenkNext == TOKENK_Carat ||
			 (tokenkNext == TOKENK_Identifier && tokenkNextNext == TOKENK_Identifier) ||
			 tokenkNext == TOKENK_Func)
	{
		// HMM: Should I put this earlier since var decls are so common?
		//	I have to have it at least after func defn so I can be assured
		//	that if we peek "func" that it isn't part of a func defn

		// Var decl

		if (!isDoStmt) return parseVarDeclStmt(pParser);
		else
		{
			auto * pErr = AstNewErr0Child(pParser, IllegalDoStmtErr, peekTokenLine(pParser->pScanner));
			pErr->astkStmt = ASTK_VarDeclStmt;
			return Up(pErr);
		}
	}
	else if (tokenkNext == TOKENK_Return)
	{
		return parseReturnStmt(pParser);
	}
	else if (tokenkNext == TOKENK_Break)
	{
		return parseBreakStmt(pParser);
	}
	else if (tokenkNext == TOKENK_Continue)
	{
		return parseContinueStmt(pParser);
	}


	return parseExprStmtOrAssignStmt(pParser);
}

AstNode * parseExprStmtOrAssignStmt(Parser * pParser)
{
	// Parse lhs expression

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
			auto * pErr = AstNewErr2Child(pParser, BubbleErr, pRhsExpr->startLine, pLhsExpr, pRhsExpr);
			return Up(pErr);
		}

		if (tryConsumeToken(pParser->pScanner, s_aTokenkAssign, s_cTokenkAssign, ensurePendingToken(pParser)))
		{
			// NOTE: This check isn't necessary for correctness, but it gives a better error message for a common case.

			auto * pErr = AstNewErr2Child(pParser, ChainedAssignErr, pRhsExpr->startLine, pLhsExpr, pRhsExpr);
			return Up(pErr);
		}
	}

	// Parse semicolon

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		if (isAssignment)
		{
			auto * pErr = AstNewErr2Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pLhsExpr, pRhsExpr);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pLhsExpr);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
	}

	// Success!

	if (isAssignment)
	{
		auto * pNode = AstNew(pParser, AssignStmt, pLhsExpr->startLine);

		pNode->pAssignToken = pAssignToken;
		pNode->pLhsExpr = pLhsExpr;
		pNode->pRhsExpr = pRhsExpr;
		return Up(pNode);
	}
	else
	{
		Assert(!pRhsExpr);
		Assert(!pAssignToken);

		auto * pNode = AstNew(pParser, ExprStmt, pLhsExpr->startLine);
		pNode->pExpr = pLhsExpr;
		return Up(pNode);
	}
}

AstNode * parseStructDefnStmt(Parser * pParser)
{
	scopeid declScope = peekScope(pParser).id;

	// Parse 'struct'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Struct))
	{
		// TODO: Add a peekTokenLine(..) function because I am doing this a lot just to get
		//	line # for error tokens

		Token errToken;
		peekToken(pParser->pScanner, &errToken);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, errToken.line);
		append(&pErr->aTokenkValid, TOKENK_Struct);
		return Up(pErr);
	}

	// Parse identifier

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		Token errToken;
		peekToken(pParser->pScanner, &errToken);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, errToken.line);
		append(&pErr->aTokenkValid, TOKENK_Identifier);
		return Up(pErr);
	}

	Token * pIdentTok = claimPendingToken(pParser);
	Assert(pIdentTok->tokenk == TOKENK_Identifier);

	// Parse '{'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, pIdentTok->line);
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		return Up(pErr);
	}

	pushScope(pParser, SCOPEK_StructDefn);
	Defer(popScope(pParser));

	// Parse vardeclstmt list, then '}'

	DynamicArray<AstNode *> apVarDeclStmt;
	init(&apVarDeclStmt);
	Defer(dispose(&apVarDeclStmt));

	while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBrace))
	{
		// Parse member var decls

		AstNode * pVarDeclStmt = parseVarDeclStmt(pParser);
		append(&apVarDeclStmt, pVarDeclStmt);

		if (isErrorNode(*pVarDeclStmt))
		{
			static const TOKENK s_aTokenkRecoverable[] = { TOKENK_Semicolon, TOKENK_CloseBrace };

			// NOTE: Panic recovery needs reworking! See comment @ tryRecoverFromPanic

			TOKENK tokenkMatch;
			tryRecoverFromPanic(pParser, s_aTokenkRecoverable, ArrayLen(s_aTokenkRecoverable), &tokenkMatch);

			if (tokenkMatch == TOKENK_CloseParen)
			{
				goto finishStruct;
			}
			else if (tokenkMatch == TOKENK_Semicolon)
			{
				// No action needed
			}
			else
			{
				Assert(tokenkMatch == TOKENK_Nil);
				Assert(apVarDeclStmt.cItem > 0);

				auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, apVarDeclStmt[0]->startLine, &apVarDeclStmt);
				return Up(pErr);
			}
		}
	}

finishStruct:

	auto * pNode = AstNew(pParser, StructDefnStmt, pIdentTok->line);
	setIdent(&pNode->ident, pIdentTok, declScope);
	initMove(&pNode->apVarDeclStmt, &apVarDeclStmt);
	pNode->scopeid = peekScope(pParser).id;

	SymbolInfo structDefnInfo;
	setSymbolInfo(&structDefnInfo, pNode->ident, SYMBOLK_Struct, Up(pNode));
	tryInsert(&pParser->symbolTable, pNode->ident, structDefnInfo);

	return Up(pNode);
}

bool tryParseFuncDefnStmtOrLiteralExpr(Parser * pParser, FUNCHEADERK funcheaderk, AstNode ** ppoNode)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);
	Assert(ppoNode);

	bool success = false;
	bool isDefn = funcheaderk == FUNCHEADERK_Defn;
	int startingLine = peekTokenLine(pParser->pScanner);

	FuncType * pFuncType = nullptr;
	Defer(if (!success && pFuncType) releaseFuncType(pParser, pFuncType););

	// Parse header

	// NOTE: Push scope before parsing header so that the symbols declared in the header
	//	are subsumed by the function's scope.

	pushScope(pParser, SCOPEK_CodeBlock);
	Defer(popScope(pParser));

	ScopedIdentifier identDefn;		// Only valid if isDefn
	DynamicArray<AstNode *> apParamVarDecl;
	DynamicArray<AstNode *> apReturnVarDecl;
	init(&apParamVarDecl);
	init(&apReturnVarDecl);

	{
		ScopedIdentifier * pDefnIdent = (isDefn) ? &identDefn : nullptr;
		if (!tryParseFuncDefnOrLiteralHeader(pParser, funcheaderk, &apParamVarDecl, &apReturnVarDecl, pDefnIdent))
		{
			Assert(containsErrorNode(apParamVarDecl) || containsErrorNode(apReturnVarDecl));

			// NOTE: Append output parameters to the list of input parameters to make our life
			//	easier using the AstNewErr macro. Note that this means we have to manually destroy
			//	the out param list since only the (now combined) in param list is getting "moved"
			//	into the error node.

			appendMultiple(&apParamVarDecl, apReturnVarDecl.pBuffer, apReturnVarDecl.cItem);
			dispose(&apReturnVarDecl);

			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apParamVarDecl);
			return success = false;
		}
	}

	// Parse { <stmts> } or do <stmt>

	bool pushPopScope = false;
	AstNode * pBody = parseDoStmtOrBlockStmt(pParser, pushPopScope);

	if (isErrorNode(*pBody))
	{
		appendMultiple(&apParamVarDecl, apReturnVarDecl.pBuffer, apReturnVarDecl.cItem);
		dispose(&apReturnVarDecl);

		append(&apParamVarDecl, pBody);
		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apParamVarDecl);
		return success = false;
	}

	if (!isDefn)
	{
		auto * pNode = AstNew(pParser, FuncLiteralExpr, startingLine);
		pNode->pBodyStmt = pBody;
		pNode->scopeid = peekScope(pParser).id;
		initMove(&pNode->apParamVarDecl, &apParamVarDecl);
		initMove(&pNode->apReturnVarDecl, &apReturnVarDecl);
		*ppoNode = Up(pNode);

		return success = true;
	}
	else
	{
		auto * pNode = AstNew(pParser, FuncDefnStmt, startingLine);
		pNode->ident = identDefn;
		pNode->pBodyStmt = pBody;
		pNode->scopeid = peekScope(pParser).id;
		initMove(&pNode->apParamVarDecl, &apParamVarDecl);
		initMove(&pNode->apReturnVarDecl, &apReturnVarDecl);

		// Insert into symbol table

		SymbolInfo funcDefnInfo;
		setSymbolInfo(&funcDefnInfo, pNode->ident, SYMBOLK_Func, Up(pNode));
		tryInsert(&pParser->symbolTable, identDefn, funcDefnInfo);

		*ppoNode = Up(pNode);
		return success = true;
	}
}

AstNode * parseVarDeclStmt(Parser * pParser, EXPECTK expectkName, EXPECTK expectkInit, EXPECTK expectkSemicolon)
{
	AssertInfo(expectkName != EXPECTK_Forbidden, "Function does not (currently) support being called with expectkName forbidden");
	AssertInfo(expectkSemicolon != EXPECTK_Optional, "Semicolon should either be required or forbidden");

	// This list already kind of exists embedded in the modifiers and func types,
	//	but we store it separately here to make it easier to attach them to an error
	//	node should we ultimately produce an error.

	DynamicArray<AstNode *> apNodeChildren;
	init(&apNodeChildren);
	Defer(dispose(&apNodeChildren););

	// Remember line # of the first token

	int line;
	{
		Token tokenNext;
		peekToken(pParser->pScanner, &tokenNext);
		line = tokenNext.line;
	}

    bool success = false;

    // Parse type

    Type * pType = newType(pParser);
    Defer(if (!success) releaseType(pParser, pType););

    if (!tryParseType(pParser, &pType, &apNodeChildren))
    {
        Assert(apNodeChildren.cItem > 0);

        AstErr * pErrFromParseType;
        {
            AstNode * pNode = apNodeChildren[apNodeChildren.cItem - 1];
            Assert(category(pNode->astk) == ASTCATK_Error);
            pErrFromParseType = DownErr(pNode);
        }

        auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apNodeChildren);
        return Up(pErr);
    }

	// Parse name

	Token * pVarIdent = nullptr;
	Defer(
		if (!success && pVarIdent) release(&pParser->tokenAlloc, pVarIdent);
	);

	if (expectkName == EXPECTK_Optional || expectkName == EXPECTK_Required)
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			pVarIdent = claimPendingToken(pParser);
			Assert(pVarIdent->tokenk == TOKENK_Identifier);
		}
		else if (expectkName == EXPECTK_Required)
		{
			int line = peekTokenLine(pParser->pScanner);
			auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, line, &apNodeChildren);
			append(&pErr->aTokenkValid, TOKENK_Identifier);
			return Up(pErr);
		}
	}
	else
	{
		AssertInfo(false, "Var decls can be optionally named in some contexts (func params), but it doesn't make sense to forbid naming them!");
	}

	// Parse init expr

	AstNode * pInitExpr = nullptr;

	if (tryConsumeToken(pParser->pScanner, TOKENK_Equal))
	{
		pInitExpr = parseExpr(pParser);
		append(&apNodeChildren, pInitExpr);

		if (isErrorNode(*pInitExpr))
		{
			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, pInitExpr->startLine, &apNodeChildren);
			return Up(pErr);
		}

		if (tryConsumeToken(pParser->pScanner, TOKENK_Equal))
		{
			// NOTE: This check isn't necessary for correctness, but it gives a better error message for a common case.

			auto * pErr = AstNewErrListChildMove(pParser, ChainedAssignErr, pInitExpr->startLine, &apNodeChildren);
			return Up(pErr);
		}
	}

	// Providing init expr isn't allowed if the var isn't named...

	if (!pVarIdent && pInitExpr)
	{
		auto * pErr = AstNewErrListChildMove(pParser, InitUnnamedVarErr, pInitExpr->startLine, &apNodeChildren);
		return Up(pErr);
	}

	// Providing init expr is an error in some contexts!

	if (pInitExpr && expectkInit == EXPECTK_Forbidden)
	{
		Assert(pVarIdent);

		auto * pErr = AstNewErrListChildMove(pParser, IllegalInitErr, pInitExpr->startLine, &apNodeChildren);
		pErr->pVarIdent = pVarIdent;
		return Up(pErr);
	}

	// Parse semicolon

	if (expectkSemicolon == EXPECTK_Required)
	{
		if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon, ensurePendingToken(pParser)))
		{
			Token * pErrToken = ensurePendingToken(pParser);
			peekToken(pParser->pScanner, pErrToken);

			auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, pErrToken->line, &apNodeChildren);
			append(&pErr->aTokenkValid, TOKENK_Semicolon);
			return Up(pErr);
		}
	}

	scopeid declScope = peekScope(pParser).id;

	auto * pNode = AstNew(pParser, VarDeclStmt, line);
	setIdent(&pNode->ident, pVarIdent, declScope);
	pNode->pType = pType;
	pNode->pInitExpr = pInitExpr;

	SymbolInfo varDeclInfo;
	setSymbolInfo(&varDeclInfo, pNode->ident, SYMBOLK_Var, Up(pNode));
	tryInsert(&pParser->symbolTable, pNode->ident, varDeclInfo);

	return Up(pNode);
}

AstNode * parseIfStmt(Parser * pParser)
{
	int ifLine = prevTokenLine(pParser->pScanner);

	// Parse 'if'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_If))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
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

	AstNode * pThenStmt = parseDoStmtOrBlockStmt(pParser);
	if (isErrorNode(*pThenStmt))
	{
		auto * pErr = AstNewErr2Child(pParser, BubbleErr, pThenStmt->startLine, pCondExpr, pThenStmt);
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
			pElseStmt = parseDoStmtOrBlockStmt(pParser);
		}
	}

	if (pElseStmt && isErrorNode(*pElseStmt))
	{
		auto * pErr = AstNewErr3Child(pParser, BubbleErr, pThenStmt->startLine, pCondExpr, pThenStmt, pElseStmt);
		return Up(pErr);
	}

	// Success!

	auto * pNode = AstNew(pParser, IfStmt, ifLine);
	pNode->pCondExpr = pCondExpr;
	pNode->pThenStmt = pThenStmt;
	pNode->pElseStmt = pElseStmt;
	return Up(pNode);
}

AstNode * parseWhileStmt(Parser * pParser)
{
	// Parse 'while'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_While))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_While);
		return Up(pErr);
	}

	int whileLine = prevTokenLine(pParser->pScanner);

	// Parse cond expr

	AstNode * pCondExpr = parseExpr(pParser);
	if (isErrorNode(*pCondExpr))
	{
		return pCondExpr;
	}

	// Parse 'do <stmt>' or block stmt

	AstNode * pBodyStmt = parseDoStmtOrBlockStmt(pParser);
	if (isErrorNode(*pBodyStmt))
	{
		auto * pErr = AstNewErr2Child(pParser, BubbleErr, pBodyStmt->startLine, pCondExpr, pBodyStmt);
		return Up(pErr);
	}

	// Success!

	auto * pNode = AstNew(pParser, WhileStmt, whileLine);
	pNode->pCondExpr = pCondExpr;
	pNode->pBodyStmt = pBodyStmt;
	return Up(pNode);
}

AstNode * parseDoStmtOrBlockStmt(Parser * pParser, bool pushPopScopeBlock)
{
	TOKENK tokenk = peekToken(pParser->pScanner);
	if (tokenk != TOKENK_OpenBrace &&
		tokenk != TOKENK_Do)
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		append(&pErr->aTokenkValid, TOKENK_Do);
		return Up(pErr);
	}

	AstNode * pStmt = nullptr;
	if (tokenk == TOKENK_OpenBrace)
	{
		pStmt = parseBlockStmt(pParser, pushPopScopeBlock);

		if (isErrorNode(*pStmt))
		{
			auto * pErr = AstNewErr1Child(pParser, BubbleErr, pStmt->startLine, pStmt);
			return Up(pErr);
		}
	}
	else
	{
		Assert(tokenk == TOKENK_Do);
		consumeToken(pParser->pScanner);		// 'do'

		bool isDoStmt = true;
		pStmt = parseStmt(pParser, isDoStmt);

		Assert(pStmt->astk != ASTK_BlockStmt);
		Assert(pStmt->astk != ASTK_VarDeclStmt);

		if (isErrorNode(*pStmt))
		{
			auto * pErr = AstNewErr1Child(pParser, BubbleErr, pStmt->startLine, pStmt);
			return Up(pErr);
		}
	}

	return pStmt;
}

AstNode * parseBlockStmt(Parser * pParser, bool pushPopScope)
{
	// Parse {

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_OpenBrace);
		return Up(pErr);
	}

	if (pushPopScope) pushScope(pParser, SCOPEK_CodeBlock);
	Defer(if (pushPopScope) popScope(pParser));

	int openBraceLine = prevTokenLine(pParser->pScanner);

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
			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apStmts);
			return Up(pErr);
		}
	}

	// Success!

	auto * pNode = AstNew(pParser, BlockStmt, openBraceLine);
	initMove(&pNode->apStmts, &apStmts);
	pNode->scopeid = peekScope(pParser).id;

	return Up(pNode);
}

AstNode * parseReturnStmt(Parser * pParser)
{
	int returnLine = peekTokenLine(pParser->pScanner);

	// Parse 'return'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Return))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
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
		AstExpectedTokenkErr * pErr;

		if (pExpr)
		{
			pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pExpr);
		}
		else
		{
			pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		}

		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	auto * pNode = AstNew(pParser, ReturnStmt, returnLine);
	pNode->pExpr = pExpr;
	return Up(pNode);
}

AstNode * parseBreakStmt(Parser * pParser)
{
	int breakLine = peekTokenLine(pParser->pScanner);

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Break))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_Break);
		return Up(pErr);
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	auto * pNode = AstNew(pParser, BreakStmt, breakLine);
	return Up(pNode);
}

AstNode * parseContinueStmt(Parser * pParser)
{
	int continueLine = peekTokenLine(pParser->pScanner);

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Continue))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_Continue);
		return Up(pErr);
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_Semicolon);
		return Up(pErr);
	}

	// Success!

	auto * pNode = AstNew(pParser, ContinueStmt, continueLine);
	return Up(pNode);
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
			auto * pErr = AstNewErr2Child(pParser, BubbleErr, pExpr->startLine, pExpr, pRhsExpr);
			return Up(pErr);
		}

		auto * pNode = AstNew(pParser, BinopExpr, pExpr->startLine);
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
		Token * pOp = claimPendingToken(pParser);

		AstNode * pExpr = parseUnopPre(pParser);
		if (isErrorNode(*pExpr))
		{
			return pExpr;
		}

		auto * pNode = AstNew(pParser, UnopExpr, pOp->line);
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
	if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// Group ( )

		Token * pOpenParen = claimPendingToken(pParser);
		AstNode * pExpr = parseExpr(pParser);

		if (isErrorNode(*pExpr))
		{
			// Panic recovery
			// NOTE: Panic recovery needs reworking! See comment @ tryRecoverFromPanic

			if (tryRecoverFromPanic(pParser, TOKENK_CloseParen))
			{
				goto finishGroup;
			}

			return pExpr;
		}
		else
		{
			if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
			{
				// IDEA: Better error line message if we walk pExpr and find line of the highest AST node child it has.
				//	We could easily cache that when constructing an AST node...

				auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, pExpr->startLine, pExpr);
				append(&pErr->aTokenkValid, TOKENK_CloseParen);
				return Up(pErr);
			}
		}

	finishGroup:
		auto * pNode = AstNew(pParser, GroupExpr, pOpenParen->line);
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
		return parseVarExpr(pParser, pVarOwner);
	}
	else if (peekToken(pParser->pScanner) == TOKENK_Func)
	{
		// Func literal

		AstNode * pNode;
		tryParseFuncDefnStmtOrLiteralExpr(pParser, FUNCHEADERK_Literal, &pNode);

		if (isErrorNode(*pNode))
		{
			return pNode;
		}

		if (peekToken(pParser->pScanner) == TOKENK_OpenParen)
		{
			// HMM: I am torn about whether I want to call finishParsePrimaryHere,
			//	which would let you invoke a function literal at the spot that it is
			//	defined. Personally I am not convinced that that is good practice...
			//	It also doesn't work with the "do" syntax for one-liners.

			// NOTE: Current check for this is kind of a hack when really it would be
			//	better to make calling a func literal a valid parse, but a semantic error!
			//	Otherwise we would error on something like this since we are only checking
			//	for open parens...
			//	func()() { doSomething(); }(;

			auto * pErr = AstNewErr1Child(pParser, InvokeFuncLiteralErr, peekTokenLine(pParser->pScanner), pNode);
			return Up(pErr);
		}

		return pNode;
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
		AstExpectedTokenkErr * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		appendMultiple(&pErr->aTokenkValid, aTokenkValid, cTokenkValid);

		return Up(pErr);
	}

	Token * pToken = claimPendingToken(pParser);

	auto * pExpr = AstNew(pParser, LiteralExpr, pToken->line);
	pExpr->pToken = pToken;
	pExpr->isValueSet = false;
	pExpr->isValueErroneous = false;

	return Up(pExpr);
}

AstNode * parseVarExpr(Parser * pParser, AstNode * pOwnerExpr)
{
	if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		AstExpectedTokenkErr * pErr;
		if (pOwnerExpr)
		{
			pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pOwnerExpr);
		}
		else
		{
			pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		}

		append(&pErr->aTokenkValid, TOKENK_Identifier);
		return Up(pErr);
	}

	Token * pIdent = claimPendingToken(pParser);
	Assert(pIdent->tokenk == TOKENK_Identifier);

	auto * pNode = AstNew(pParser, VarExpr, pIdent->line);
	pNode->pOwner = pOwnerExpr;
	pNode->pTokenIdent = pIdent;
	pNode->pResolvedDecl = nullptr;

	return finishParsePrimary(pParser, Up(pNode));
}

bool tryParseFuncDefnOrLiteralHeader(
	Parser * pParser,
	FUNCHEADERK funcheaderk,
	DynamicArray<AstNode *> * papParamVarDecls,
	DynamicArray<AstNode *> * papReturnVarDecls,
	ScopedIdentifier * poDefnIdent)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);

	EXPECTK expectkName = (funcheaderk == FUNCHEADERK_Defn) ? EXPECTK_Required : EXPECTK_Forbidden;

	AssertInfo(Implies(expectkName == EXPECTK_Forbidden, !poDefnIdent), "Don't provide an ident pointer in a context where a name is illegal!");
	AssertInfo(Implies(expectkName != EXPECTK_Forbidden, poDefnIdent), "Should provide an ident pointer in a context where a name is legal!");

	// NOTE: If function fails, we embed an error node in one of the pap*VarDecl variables

	bool success = false;
	Token * pIdent = nullptr;
	Defer(
		if (!success && pIdent) release(&pParser->tokenAlloc, pIdent)
	);

	// Parse "func"

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Func))
	{
		int line = peekTokenLine(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
		append(&pErr->aTokenkValid, TOKENK_Func);
		append(papParamVarDecls, Up(pErr));
		return false;
	}

	// Parse definition name

	if (expectkName == EXPECTK_Required)
	{
		Assert(poDefnIdent);

		if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			int line = peekTokenLine(pParser->pScanner);

			auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
			append(&pErr->aTokenkValid, TOKENK_Identifier);
			append(papParamVarDecls, Up(pErr));
			return false;
		}

		pIdent = claimPendingToken(pParser);

		// NOTE: We are not responsible for adding the identifier to the symbol table.
		//	That responsibility falls on the function that allocates the AST node that
		//	contains the identifier.

		setIdent(poDefnIdent, pIdent, peekScope(pParser).id);
	}
	else
	{
		Assert(expectkName == EXPECTK_Forbidden);

		if (tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			// TODO: Maybe this should be a more specific/informative error, since we basically
			//	understand what they are trying to do. Something like NamedFuncNonDefnErr...?
			//	This is probably a good test case for adding "context" messages to errors!!!

			Token * pErrToken = claimPendingToken(pParser);

			auto * pErr = AstNewErr0Child(pParser, UnexpectedTokenkErr, pErrToken->line);
			pErr->pErrToken = pErrToken;
			append(papParamVarDecls, Up(pErr));
			return false;
		}
	}

	// Parse "in" parameters

	if (!tryParseFuncDefnOrLiteralHeaderParamList(pParser, funcheaderk, papParamVarDecls))
	{
		Assert(containsErrorNode(*papParamVarDecls));
		return false;
	}

	// Parse "out" parameters (a.k.a. return types)

	if (!tryParseFuncDefnOrLiteralHeaderParamList(pParser, funcheaderk, papReturnVarDecls))
	{
		Assert(containsErrorNode(*papReturnVarDecls));
		return false;
	}

	// Success!

	return true;
}

bool tryParseFuncDefnOrLiteralHeaderParamList(Parser * pParser, FUNCHEADERK funcheaderk, DynamicArray<AstNode *> * papParamVarDecls)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen))
	{
		int line = peekTokenLine(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
		append(&pErr->aTokenkValid, TOKENK_OpenParen);
		append(papParamVarDecls, Up(pErr));
		return false;
	}

	bool isFirstParam = true;

	while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen))
	{
		if (!isFirstParam && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
		{
			int line = peekTokenLine(pParser->pScanner);

			auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, line, papParamVarDecls);
			append(&pErr->aTokenkValid, TOKENK_Comma);
			append(papParamVarDecls, Up(pErr));
			return false;
		}

		const static EXPECTK s_expectkVarName = EXPECTK_Optional;
		const static EXPECTK s_expectkSemicolon = EXPECTK_Forbidden;
		const static EXPECTK s_expectkVarInit = EXPECTK_Optional;

		AstNode * pNode = parseVarDeclStmt(pParser, s_expectkVarName, s_expectkVarInit, s_expectkSemicolon);
		append(papParamVarDecls, pNode);

		if (isErrorNode(*pNode))
		{
			return false;
		}

		isFirstParam = false;
	}

	return true;
}

bool tryParseType(Parser * pParser, Type ** ppoType, DynamicArray<AstNode *> * papNodeChildren)
{
	DynamicArray<TypeModifier> aModifiers;
	init(&aModifiers);
	Defer(dispose(&aModifiers););

	// NOTE; Since all nodes are stored in papNodeChildren, error nodes do not need to attach any children. It is the
	//	responsibility of the caller to add papNodeChildren to a bubble error if we return false

	while (peekToken(pParser->pScanner) != TOKENK_Identifier &&
		   peekToken(pParser->pScanner) != TOKENK_Func)
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket))
		{
			// [

			auto * pSubscriptExpr = parseExpr(pParser);
			append(papNodeChildren, pSubscriptExpr);

			if (isErrorNode(*pSubscriptExpr))
			{
				return false;
			}

			if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket))
			{
				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, pSubscriptExpr->startLine);
				append(&pErr->aTokenkValid, TOKENK_CloseBracket);
				append(papNodeChildren, Up(pErr));
				return false;
			}

			TypeModifier mod;
			mod.typemodk = TYPEMODK_Array;
			mod.pSubscriptExpr = pSubscriptExpr;
			append(&aModifiers, mod);
		}
		else if (tryConsumeToken(pParser->pScanner, TOKENK_Carat))
		{
			// ^

			TypeModifier mod;
			mod.typemodk = TYPEMODK_Pointer;
			append(&aModifiers, mod);
		}
		else
		{
			AstNode * pErr = handleScanOrUnexpectedTokenkErr(pParser, nullptr);
			append(papNodeChildren, pErr);
			return false;
		}
	}

	bool success = false;
	Token * pTypeIdent = nullptr;		// Only for non-func type
	FuncType * pFuncType = nullptr;		// Only for func type

	// Defer cleanup

	Defer(
		if (!success && pTypeIdent)
		{
			release(&pParser->tokenAlloc, pTypeIdent);
		}

		if (!success && pFuncType)
		{
            for (uint i = 0; i < pFuncType->apParamType.cItem; i++)
            {
                releaseType(pParser, pFuncType->apParamType[i]);
            }

            for (uint i = 0; i < pFuncType->apReturnType.cItem; i++)
            {
                releaseType(pParser, pFuncType->apReturnType[i]);
            }

			dispose(pFuncType);
			releaseFuncType(pParser, pFuncType);
		}
	);

	bool isFuncType = false;
	if (peekToken(pParser->pScanner) == TOKENK_Identifier)
	{
		isFuncType = false;
		pTypeIdent = ensureAndClaimPendingToken(pParser);
		consumeToken(pParser->pScanner, pTypeIdent);
	}
	else
	{
		Assert(peekToken(pParser->pScanner) == TOKENK_Func);

		isFuncType = true;

		pFuncType = newFuncType(pParser);
		init(pFuncType);

		AstErr * pErr = nullptr;
		bool funcHeaderSuccess = tryParseFuncHeaderTypeOnly(pParser, pFuncType, &pErr);

		Assert(Implies(funcHeaderSuccess, !pErr));

		if (!funcHeaderSuccess)
		{
			append(papNodeChildren, Up(pErr));
			return false;
		}
	}

	*ppoType = newType(pParser);
	if (isFuncType)
	{
		(*ppoType)->isFuncType = true;
		(*ppoType)->pFuncType = pFuncType;
	}
	else
	{
		ScopedIdentifier identType;
		setIdentNoScope(&identType, pTypeIdent);

		Assert(pTypeIdent->tokenk == TOKENK_Identifier);
		(*ppoType)->isFuncType = false;
		(*ppoType)->ident = identType;
	}

	initMove(&(*ppoType)->aTypemods, &aModifiers);

	success = true;		// Used in Defer
	return success;
}

bool tryParseFuncHeaderTypeOnly(Parser * pParser, FuncType * poFuncType, AstErr ** ppoErr)
{
	// NOTE: poFuncType should be inited by caller

	// Parse "func"

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Func))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_Func);

		*ppoErr = DownErr(Up(pErr));
		return false;
	}

	// Subscript expressions are AstNodes that we have to attach as children if we error

	DynamicArray<AstNode *> apNodeChildren;
	init(&apNodeChildren);
	Defer(dispose(&apNodeChildren));

	auto tryParseParameterTypes = [](
		Parser * pParser,
		DynamicArray<Type *> * papTypes,			// Value we are computing
		DynamicArray<AstNode *> * papNodeChildren)	// Boookkeeping so caller can attach children to errors
		-> bool
	{
		// Parse (

		if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen))
		{
			auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
			append(&pErr->aTokenkValid, TOKENK_OpenParen);
            append(papNodeChildren, Up(pErr));
			return false;
		}

		bool isFirstParam = true;
		while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen))
		{
			// ,

			if (!isFirstParam && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
			{
				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
				append(&pErr->aTokenkValid, TOKENK_Comma);
                append(papNodeChildren, Up(pErr));
				return false;
			}

			// type

            Type * pType = newType(pParser);
            append(papTypes, pType);

			if (!tryParseType(pParser, &pType, papNodeChildren))
			{
				Assert(
					papNodeChildren->cItem > 0 &&
					category((*papNodeChildren)[papNodeChildren->cItem - 1]->astk) == ASTCATK_Error
				);

                releaseType(pParser, pType);
				return false;
			}

			// name (optional)
			// HMM: It feels funny to allow an identifier but to not put it in the symbol table or bind it to anything.
			//	but the documentation benefits just feel so good... if this is something I run into in other scenarios,
			//	maybe introduce throwaway, unbound identifiers as a concept in the language with its own syntax.
			//	Maybe something like fn (int #startIndex, int #count) ... but that would rule out # for things like
			//	compiler directives!
			// Maybe define // to not comment to the end of the line if it isn't followed by a space? So it would
			//	look like fn (int //startIndex, int //count)... I actually don't hate how that looks, even though
			//	the semantics are kinda unusual.


			tryConsumeToken(pParser->pScanner, TOKENK_Identifier);
		}

        return true;
    };

	//
	// Parse in parameters
	//

	if (!tryParseParameterTypes(pParser, &poFuncType->apParamType, &apNodeChildren))
	{
		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apNodeChildren);
		*ppoErr = DownErr(Up(pErr));
		return false;
	}

	//
	// Parse in parameters
	//

	if (!tryParseParameterTypes(pParser, &poFuncType->apReturnType, &apNodeChildren))
	{
		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apNodeChildren);
		*ppoErr = DownErr(Up(pErr));
		return false;
	}

    return true;
}

AstNode * finishParsePrimary(Parser * pParser, AstNode * pExpr)
{
	switch (pExpr->astk)
	{
		case ASTK_GroupExpr:
		case ASTK_FuncCallExpr:
		case ASTK_ArrayAccessExpr:
			break;

		case ASTK_VarExpr:
		{
			Assert(Down(pExpr, VarExpr)->pTokenIdent->tokenk == TOKENK_Identifier);
		} break;

		default:
			Assert(false);
	}

	if (tryConsumeToken(pParser->pScanner, TOKENK_Dot, ensurePendingToken(pParser)))
	{
		// Member access

		return parseVarExpr(pParser, pExpr);
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket, ensurePendingToken(pParser)))
	{
		// Array access

		static const bool s_mustBeIntLiteral = true;		// TODO: Support arbitrary expressions (semantic pass makes sure they are compile time const)
		AstNode * pSubscriptExpr = parseLiteralExpr(pParser, s_mustBeIntLiteral);
		if (isErrorNode(*pSubscriptExpr))
		{
			// Panic recovery
			// NOTE: Panic recovery needs reworking! See comment @ tryRecoverFromPanic

			if (tryRecoverFromPanic(pParser, TOKENK_CloseBracket))
			{
				goto finishArrayAccess;
			}

			auto * pErr = AstNewErr2Child(pParser, BubbleErr, pExpr->startLine, pExpr, pSubscriptExpr);
			return Up(pErr);
		}
		else if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
		{
			auto * pErr = AstNewErr2Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pExpr, pSubscriptExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseBracket);

			return Up(pErr);
		}

	finishArrayAccess:
		auto * pNode = AstNew(pParser, ArrayAccessExpr, pExpr->startLine);
		pNode->pArray = pExpr;
		pNode->pSubscript = pSubscriptExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// Func call

		DynamicArray<AstNode *> apArgs;
		init(&apArgs);
		Defer(Assert(!apArgs.pBuffer));		// buffer should get "moved" into AST

		bool isFirstArg = true;

		// Error recovery variables

		bool skipCommaCheck = false;
		static const TOKENK s_aTokenkRecoverable[] = { TOKENK_CloseParen, TOKENK_Comma };

		while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			if (!isFirstArg && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
			{
				int line = peekTokenLine(pParser->pScanner);

				// NOTE: Panic recovery needs reworking! See comment @ tryRecoverFromPanic

				TOKENK tokenkMatch;
				bool recovered = tryRecoverFromPanic(pParser, s_aTokenkRecoverable, ArrayLen(s_aTokenkRecoverable), &tokenkMatch);

				if (recovered)
				{
					auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
					append(&pErr->aTokenkValid, TOKENK_Comma);

					append(&apArgs, Up(pErr));

					// NOTE: We slot the error into the argument list. If we matched a ',',
					//	we can keep parsing the next arguments. If we matched a ')' we
					//	can jump to the end logic.

					if (tokenkMatch == TOKENK_CloseParen)
					{
						goto finishFuncCall;
					}
				}
				else
				{
					prepend(&apArgs, pExpr);

					auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, line, &apArgs);
					append(&pErr->aTokenkValid, TOKENK_Comma);

					return Up(pErr);
				}
			}

			skipCommaCheck = false;

			AstNode * pExprArg = parseExpr(pParser);
			append(&apArgs, pExprArg);

			if (isErrorNode(*pExprArg))
			{
				// NOTE: Panic recovery needs reworking! See comment @ tryRecoverFromPanic

				TOKENK tokenkMatch;
				tryRecoverFromPanic(pParser, s_aTokenkRecoverable, ArrayLen(s_aTokenkRecoverable), &tokenkMatch);

				if (tokenkMatch == TOKENK_CloseParen)
				{
					goto finishFuncCall;
				}
				else if (tokenkMatch == TOKENK_Comma)
				{
					// We already consumed the comma for the next argument

					skipCommaCheck = true;
				}
				else
				{
					Assert(tokenkMatch == TOKENK_Nil);

					prepend(&apArgs, pExpr);

					auto * pErrNode = AstNewErrListChildMove(pParser, BubbleErr, pExprArg->startLine, &apArgs);
					return Up(pErrNode);
				}
			}

			isFirstArg = false;
		}

	finishFuncCall:
		auto * pNode = AstNew(pParser, FuncCallExpr, pExpr->startLine);
		pNode->pFunc = pExpr;
		initMove(&pNode->apArgs, &apArgs);

		return finishParsePrimary(pParser, Up(pNode));
	}
	else
	{
		return pExpr;
	}
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
			auto * pErr = AstNewErrListChildMove(pParser, ScanErr, pErrToken->line, papChildren);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr0Child(pParser, ScanErr, pErrToken->line);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
	}
	else
	{
		// Unexpected tokenk err

		if (papChildren)
		{
			auto * pErr = AstNewErrListChildMove(pParser, UnexpectedTokenkErr, pErrToken->line, papChildren);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr0Child(pParser, UnexpectedTokenkErr, pErrToken->line);
			pErr->pErrToken = pErrToken;
			return Up(pErr);
		}
	}
}

void pushScope(Parser * pParser, SCOPEK scopek)
{
	Scope s;
	s.id = pParser->scopeidNext;
	s.scopek = scopek;
	push(&pParser->scopeStack, s);
	pParser->scopeidNext++;
}

Scope peekScope(Parser * pParser)
{
	Scope s;
	peek(pParser->scopeStack, &s);

	return s;
}

Scope popScope(Parser * pParser)
{
	Scope s;
	pop(&pParser->scopeStack, &s);

	return s;
}

AstNode * astNew(Parser * pParser, ASTK astk, int line)
{
	AstNode * pNode = allocate(&pParser->astAlloc);
	pNode->astk = astk;
	pNode->id = pParser->iNode;
	pNode->startLine = line;
	pParser->iNode++;

	append(&pParser->astNodes, pNode);

	return pNode;
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * pChild0, AstNode * pChild1, AstNode * pChild2)
{
	Assert(Implies(pChild2, pChild1));
	Assert(Implies(pChild1, pChild0));

	AstNode * apChildren[3] = { pChild0, pChild1, pChild2 };
	int cPChildren = (pChild2) ? 3 : (pChild1) ? 2 : (pChild0) ? 1 : 0;
	return astNewErr(pParser, astkErr, line, apChildren, cPChildren);
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * apChildren[], uint cPChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = Down(astNew(pParser, astkErr, line), Err);
	init(&pNode->apChildren);
	appendMultiple(&pNode->apChildren, apChildren, cPChildren);

#if DEBUG
	bool hasErrorChild = false;
	if (astkErr == ASTK_BubbleErr)
	{
		for (uint i = 0; i < pNode->apChildren.cItem; i++)
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

	return Up(pNode);
}

AstNode * astNewErrMoveChildren(Parser * pParser, ASTK astkErr, int line, DynamicArray<AstNode *> * papChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = Down(astNew(pParser, astkErr, line), Err);
	initMove(&pNode->apChildren, papChildren);

#if DEBUG
	bool hasErrorChild = false;
	if (astkErr == ASTK_BubbleErr)
	{
		for (uint i = 0; i < pNode->apChildren.cItem; i++)
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

	return Up(pNode);
}

// FIXME:
// Panic recovery is pretty busted. Trying to recover from something like this by matching
//	with ; or } as it does now will lead to a totally busted parse. We need some way
//	to identify the nested {} and try to parse them cleanly while still being in
//	panic in the outer context??? That sounds hard! Might be easier to just skip to
//	the } at the end of the outer context (can track { that we hit along the way to
//	make sure that we properly match enough } to be outside the outer context)

// struct NestedStructWithError
// {
//	int bar;
//	int foo;

//	%bar

//	struct Nested
//	 {
//		 int j;
//		 int k;
//	}

//	int z;
// }

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover)
{
	return tryRecoverFromPanic(pParser, &tokenkRecover, 1);
}

bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatch)
{
	pParser->hadError = true;

	if (poTokenkMatch) *poTokenkMatch = TOKENK_Nil;

	// Scan up to (but not past) the next semicolon, or until we consume a tokenk that
	//	we can recover from. Note that if that tokenk is a semicolon then we *do*
	//	consume it.

	// NOTE: This will consume EOF token if it hits it!

	while(true)
	{
		if (isFinished(pParser->pScanner)) return false;

		Token token;
		for (int i = 0; i < cTokenkRecover; i++)
		{
			if (tryConsumeToken(pParser->pScanner, aTokenkRecover[i], &token))
			{
				if (poTokenkMatch) *poTokenkMatch = aTokenkRecover[i];

				return true;
			}
		}

		peekToken(pParser->pScanner, &token);
		if (token.tokenk == TOKENK_Semicolon)
		{
			return false;
		}

		// Keep chewing!

		consumeToken(pParser->pScanner, &token);
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



#if DEBUG

#include <stdio.h>

void debugPrintAst(const AstNode & root)
{
	DynamicArray<bool> mpLevelSkip;
	init(&mpLevelSkip);
	Defer(dispose(&mpLevelSkip));

	debugPrintSubAst(root, 0, false, &mpLevelSkip);
	printf("\n");
}

void debugPrintSubAst(const AstNode & node, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
{
	auto setSkip = [](DynamicArray<bool> * pMapLevelSkip, uint level, bool skip) {
		Assert(level <= pMapLevelSkip->cItem);

		if (level == pMapLevelSkip->cItem)
		{
			append(pMapLevelSkip, skip);
		}
		else
		{
			(*pMapLevelSkip)[level] = skip;
		}
	};

	auto printTabs = [setSkip](int level, bool printArrows, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip) {
		Assert(Implies(skipAfterArrow, printArrows));
		for (int i = 0; i < level; i++)
		{
			if ((*pMapLevelSkip)[i])
			{
				printf("   ");
			}
			else
			{
				printf("|");

				if (printArrows && i == level - 1)
				{
					printf("- ");

					if (skipAfterArrow)
					{
						setSkip(pMapLevelSkip, level - 1, skipAfterArrow);
					}
				}
				else
				{
					printf("  ");
				}
			}
		}
	};

	auto printChildren = [printTabs](const DynamicArray<AstNode *> & apChildren, int level, const char * label, bool setSkipOnLastChild, DynamicArray<bool> * pMapLevelSkip)
	{
		for (uint i = 0; i < apChildren.cItem; i++)
		{
			bool isLastChild = i == apChildren.cItem - 1;
			bool shouldSetSkip = setSkipOnLastChild && isLastChild;

			printf("\n");
			printTabs(level, false, false, pMapLevelSkip);

			printf("(%s %d):", label, i);
			debugPrintSubAst(
				*apChildren[i],
				level,
				shouldSetSkip,
				pMapLevelSkip
			);

			if (!isLastChild)
			{
				printf("\n");
				printTabs(level, false, false, pMapLevelSkip);
			}
		}
	};

	auto printErrChildren = [printChildren](const AstErr & node, int level, DynamicArray<bool> * pMapLevelSkip)
	{
		printChildren(node.apChildren, level, "child", true, pMapLevelSkip);
	};

	auto debugPrintFuncHeader = [printTabs, printChildren](const DynamicArray<AstNode *> & apParamVarDecls, const DynamicArray<AstNode *> & apReturnVarDecls, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
	{
		if (apParamVarDecls.cItem == 0)
		{
			printf("\n");
			printTabs(level, true, false, pMapLevelSkip);
			printf("(no params)");
		}
		else
		{
			printChildren(apParamVarDecls, level, "param", false, pMapLevelSkip);
		}

		if (apReturnVarDecls.cItem == 0)
		{
			printf("\n");
			printTabs(level, true, skipAfterArrow, pMapLevelSkip);
			printf("(no return vals)");
		}
		else
		{
			printChildren(apReturnVarDecls, level, "return val", skipAfterArrow, pMapLevelSkip);
		}
	};

	auto debugPrintType = [printTabs, setSkip](const Type & type, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
	{
		auto debugPrintTypeInternal = [&](const auto & self)
		{
			setSkip(pMapLevelSkip, level, false);

			int levelNext = level + 1;

			for (uint i = 0; i < type.aTypemods.cItem; i++)
			{
				printf("\n");
				printTabs(level, false, false, pMapLevelSkip);

				TypeModifier ptm = type.aTypemods[i];

				if (ptm.typemodk == TYPEMODK_Array)
				{
					printf("(array of)");
					debugPrintSubAst(*ptm.pSubscriptExpr, levelNext, false, pMapLevelSkip);
				}
				else
				{
					Assert(ptm.typemodk == TYPEMODK_Pointer);
					printf("(pointer to)");
				}
			}

			if (type.isFuncType)
			{
				printf("\n");
				printTabs(level, false, false, pMapLevelSkip);
				printf("(func)");

				printf("\n");
				printTabs(level, false, false, pMapLevelSkip);

				printf("\n");
				printTabs(level, false, false, pMapLevelSkip);

				printf("(params)");
				printTabs(level, false, false, pMapLevelSkip);

				for (uint i = 0; i < type.pFuncType->apParamType.cItem; i++)
				{
					bool isLastChild = i == type.pFuncType->apParamType.cItem - 1;

					printf("\n");
					printTabs(level, false, false, pMapLevelSkip);

					printf("(%s %d):", "param", i);

					debugPrintType(type.pFuncType->apParamType[i], levelNext, false, pMapLevelSkip);

					if (!isLastChild)
					{
						printf("\n");
						printTabs(level, false, false, pMapLevelSkip);
					}
				}

				printf("\n");
				printTabs(level, false, false, pMapLevelSkip);

				printf("(return values)");
				printTabs(level, false, false, pMapLevelSkip);

				for (uint i = 0; i < type.pFuncType->apReturnType.cItem; i++)
				{
					bool isLastChild = i == type.pFuncType->apReturnType.cItem - 1;
					bool shouldSetSkip = setSkipOnLastChild && isLastChild;

					printf("\n");
					printTabs(level, false, false, pMapLevelSkip);

					printf("(%s %d):", "return", i);

					self(type.pFuncType->apReturnType[i], levelNext, isLastChild, pMapLevelSkip);

					if (!isLastChild)
					{
						printf("\n");
						printTabs(level, false, false, pMapLevelSkip);
					}
				}
			}
			else
			{
				printf("\n");
				printTabs(level, true, skipAfterArrow, pMapLevelSkip);
				printf("%s", type.ident.pToken->lexeme);
			}
		};

		debugPrintTypeInternal(debugPrintTypeInternal);
	};

	static const char * scanErrorString = "[scan error]:";
	static const char * parseErrorString = "[parse error]:";

	setSkip(pMapLevelSkip, level, false);
	printf("\n");
	printTabs(level, true, skipAfterArrow, pMapLevelSkip);

	int levelNext = level + 1;

	switch (node.astk)
	{
		// ERR

		// TODO: Factor these error strings out into a function that can exist outside of DEBUG and have bells and whistles
		//	like reporting error line #! Maybe even print the surrounding context... might be kinda hard since I don't store it
		//	anywhere, but I could store an index into the text buffer and it could walk forward and backward until it hits
		//	new lines or some length limit and then BOOM I've got context!

		case ASTK_ScanErr:
		{
			auto * pErr = DownConst(&node, ScanErr);
			auto * pErrCasted = UpErrConst(pErr);

			DynamicArray<StringBox<256>> errMsgs;
			init(&errMsgs);
			Defer(dispose(&errMsgs));

			errMessagesFromGrferrtok(pErr->pErrToken->grferrtok, &errMsgs);
			Assert(errMsgs.cItem > 0);

			if (errMsgs.cItem == 1)
			{
				printf("%s %s", scanErrorString, errMsgs[0].aBuffer);
			}
			else
			{
				printf("%s", scanErrorString);

				for (uint i = 0; i < errMsgs.cItem; i++)
				{
					printf("\n");
					printTabs(level, true, skipAfterArrow, pMapLevelSkip);
					printf("- %s", errMsgs[i].aBuffer);
				}
			}

			printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
		} break;

		case ASTK_BubbleErr:
		{
			auto * pErr = DownConst(&node, BubbleErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s (bubble)", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_UnexpectedTokenkErr:
		{
			auto * pErr = DownConst(&node, UnexpectedTokenkErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s unexpected %s", parseErrorString, g_mpTokenkDisplay[pErr->pErrToken->tokenk]);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_ExpectedTokenkErr:
		{
			auto * pErr = DownConst(&node, ExpectedTokenkErr);
			auto * pErrCasted = UpErrConst(pErr);

			if (pErr->aTokenkValid.cItem == 1)
			{
				printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pErr->aTokenkValid[0]]);
			}
			else if (pErr->aTokenkValid.cItem == 2)
			{
				printf("%s expected %s or %s", parseErrorString, g_mpTokenkDisplay[pErr->aTokenkValid[0]], g_mpTokenkDisplay[pErr->aTokenkValid[1]]);
			}
			else
			{
				Assert(pErr->aTokenkValid.cItem > 2);

				printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pErr->aTokenkValid[0]]);

				for (uint i = 1; i < pErr->aTokenkValid.cItem; i++)
				{
					bool isLast = (i == pErr->aTokenkValid.cItem - 1);

					printf(", ");
					if (isLast) printf("or ");
					printf("%s", g_mpTokenkDisplay[pErr->aTokenkValid[i]]);
				}
			}

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_InitUnnamedVarErr:
		{
			auto * pErr = DownConst(&node, InitUnnamedVarErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s attempt to assign initial value to unnamed variable", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_IllegalInitErr:
		{
			auto * pErr = DownConst(&node, IllegalInitErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s variable '%s' is not allowed to be assigned an initial value", parseErrorString, pErr->pVarIdent->lexeme);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_ChainedAssignErr:
		{
			auto * pErr = DownConst(&node, ChainedAssignErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s chaining assignments is not permitted", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_IllegalDoStmtErr:
		{
			auto * pErr = DownConst(&node, IllegalDoStmtErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s %s is not permitted following 'do'", parseErrorString, displayString(pErr->astkStmt));

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;

		case ASTK_InvokeFuncLiteralErr:
		{
			auto * pErr = DownConst(&node, InvokeFuncLiteralErr);
			auto * pErrCasted = UpErrConst(pErr);

			printf("%s function literals can not be directly invoked", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
			}
		} break;



		// EXPR

		case ASTK_BinopExpr:
		{
			auto * pExpr = DownConst(&node, BinopExpr);

			printf("%s ", pExpr->pOp->lexeme);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			debugPrintSubAst(*pExpr->pLhsExpr, levelNext, false, pMapLevelSkip);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			debugPrintSubAst(*pExpr->pRhsExpr, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_GroupExpr:
		{
			auto * pExpr = DownConst(&node, GroupExpr);

			printf("()");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			debugPrintSubAst(*pExpr->pExpr, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_LiteralExpr:
		{
			auto * pExpr = DownConst(&node, LiteralExpr);

			printf("%s", pExpr->pToken->lexeme);
		} break;

		case ASTK_UnopExpr:
		{
			auto * pExpr = DownConst(&node, UnopExpr);

			printf("%s ", pExpr->pOp->lexeme);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			debugPrintSubAst(*pExpr->pExpr, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_VarExpr:
		{
			auto * pExpr = DownConst(&node, VarExpr);

			if (pExpr->pOwner)
			{
				printf(". %s", pExpr->pTokenIdent->lexeme);
				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("(owner):");
				debugPrintSubAst(*pExpr->pOwner, levelNext, true, pMapLevelSkip);
			}
			else
			{
				printf("%s", pExpr->pTokenIdent->lexeme);
			}
		} break;

		case ASTK_ArrayAccessExpr:
		{
			auto * pExpr = DownConst(&node, ArrayAccessExpr);
			printf("[]");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(array):");
			debugPrintSubAst(*pExpr->pArray, levelNext, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(subscript):");
			debugPrintSubAst(*pExpr->pSubscript, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_FuncCallExpr:
		{
			auto * pExpr = DownConst(&node, FuncCallExpr);
			printf("(func call)");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(func):");

			bool noArgs = pExpr->apArgs.cItem == 0;

			debugPrintSubAst(*pExpr->pFunc, levelNext, noArgs, pMapLevelSkip);

			printChildren(pExpr->apArgs, levelNext, "arg", true, pMapLevelSkip);
		} break;

		case ASTK_FuncLiteralExpr:
		{
			auto * pStmt = DownConst(&node, FuncLiteralExpr);

			printf("(func literal)");

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			debugPrintFuncHeader(pStmt->apParamVarDecl, pStmt->apReturnVarDecl, levelNext, false, pMapLevelSkip);

			debugPrintSubAst(*pStmt->pBodyStmt, levelNext, true, pMapLevelSkip);
		} break;



		// STMT;

		case ASTK_ExprStmt:
		{
			auto * pStmt = DownConst(&node, ExprStmt);

			printf("(expr stmt)");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);

			debugPrintSubAst(*pStmt->pExpr, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_AssignStmt:
		{
			auto * pStmt = DownConst(&node, AssignStmt);

			printf("%s", pStmt->pAssignToken->lexeme);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(lhs):");

			debugPrintSubAst(*pStmt->pLhsExpr, levelNext, false, pMapLevelSkip);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(rhs):");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			debugPrintSubAst(*pStmt->pRhsExpr, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = DownConst(&node, VarDeclStmt);

			printf("(var decl)");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("\n");

			if (pStmt->ident.pToken)
			{
				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("(ident):");
				printf("\n");

				printTabs(levelNext, true, false, pMapLevelSkip);
				printf("%s", pStmt->ident.pToken->lexeme);
				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("\n");
			}

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(type):");

			bool hasInitExpr = (pStmt->pInitExpr != nullptr);

			debugPrintType(*pStmt->pType, levelNext, !hasInitExpr, pMapLevelSkip);
			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);


			if (hasInitExpr)
			{
				printf("\n");
				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("(init):");
				debugPrintSubAst(*pStmt->pInitExpr, levelNext, true, pMapLevelSkip);
			}
		} break;

		case ASTK_StructDefnStmt:
		{
			auto * pStmt = DownConst(&node, StructDefnStmt);

			printf("(struct defn)");

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(name):");

			printf("\n");
			printTabs(levelNext, true, false, pMapLevelSkip);
			printf("%s", pStmt->ident.pToken->lexeme);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printChildren(pStmt->apVarDeclStmt, levelNext, "vardecl", true, pMapLevelSkip);
		} break;

		case ASTK_FuncDefnStmt:
		{
			auto * pStmt = DownConst(&node, FuncDefnStmt);

			printf("(func defn)");

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(name):");

			printf("\n");
			printTabs(levelNext, true, false, pMapLevelSkip);
			printf("%s", pStmt->ident.pToken->lexeme);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			debugPrintFuncHeader(pStmt->apParamVarDecl, pStmt->apReturnVarDecl, levelNext, false, pMapLevelSkip);

			debugPrintSubAst(*pStmt->pBodyStmt, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_BlockStmt:
		{
			auto * pStmt = DownConst(&node, BlockStmt);

			printf("(block)");

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printChildren(pStmt->apStmts, levelNext, "stmt", true, pMapLevelSkip);
		} break;

		case ASTK_WhileStmt:
		{
			auto * pStmt = DownConst(&node, WhileStmt);

			printf("(while)");

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(cond):");

			debugPrintSubAst(*pStmt->pCondExpr, levelNext, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			debugPrintSubAst(*pStmt->pBodyStmt, levelNext, true, pMapLevelSkip);
		} break;

		case ASTK_IfStmt:
		{
			auto * pStmt = DownConst(&node, IfStmt);

			printf("(if)");

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(cond):");

			debugPrintSubAst(*pStmt->pCondExpr, levelNext, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			debugPrintSubAst(*pStmt->pThenStmt, levelNext, !pStmt->pElseStmt, pMapLevelSkip);

			if (pStmt->pElseStmt)
			{
				printf("\n");
				printTabs(levelNext, false, false, pMapLevelSkip);

				printf("\n");
				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("(else):");

				debugPrintSubAst(*pStmt->pElseStmt, levelNext, true, pMapLevelSkip);
			}
		} break;

		case ASTK_ReturnStmt:
		{
			auto * pStmt = DownConst(&node, ReturnStmt);
			printf("(return)");

			if (pStmt->pExpr)
			{
				printf("\n");
				printTabs(levelNext, false, false, pMapLevelSkip);

				debugPrintSubAst(*pStmt->pExpr, levelNext, true, pMapLevelSkip);
			}
		} break;

		case ASTK_BreakStmt:
		{
			auto * pStmt = DownConst(&node, BreakStmt);
			printf("(break)");
		} break;

		case ASTK_ContinueStmt:
		{
			auto * pStmt = DownConst(&node, ContinueStmt);
			printf("(continue)");
		} break;



		// PROGRAM

		case ASTK_Program:
		{
			auto * pNode = DownConst(&node, Program);

			printf("(program)");
			printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);

			printChildren(pNode->apNodes, levelNext, "stmt", true, pMapLevelSkip);
		} break;

		default:
		{
			// Not implemented!

			Assert(false);
		}
	}
}

#endif
