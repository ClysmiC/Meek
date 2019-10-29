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
	{ 4, { TOKENK_HashAnd }, 1 },
    { 5, { TOKENK_HashXor }, 1 },
	{ 6, { TOKENK_HashOr }, 1 },
	{ 7, { TOKENK_AmpAmp }, 1 },
	{ 8, { TOKENK_PipePipe }, 1 },
};
static constexpr int s_iParseOpMax = ArrayLen(s_aParseOp);

bool init(Parser * pParser, Scanner * pScanner)
{
	if (!pParser || !pScanner) return false;

	pParser->pScanner = pScanner;
	init(&pParser->astAlloc);
	init(&pParser->tokenAlloc);
	init(&pParser->typeAlloc);
	init(&pParser->astNodes);
	init(&pParser->scopeStack);
	init(&pParser->symbTable);
	init(&pParser->typeTable);

	return true;
}

AstNode * parseProgram(Parser * pParser, bool * poSuccess)
{
	// NOTE: Empty program is valid to parse, but I may still decide that
	//	that is a semantic error.

	pushScope(pParser, SCOPEK_BuiltIn);
	Assert(peekScope(pParser).id == SCOPEID_BuiltIn);

    insertBuiltInTypes(&pParser->typeTable);
    insertBuiltInSymbols(&pParser->symbTable);

	pushScope(pParser, SCOPEK_Global);
	Assert(peekScope(pParser).id == SCOPEID_Global);

	auto * pNode = AstNew(pParser, Program, 0);
	init(&pNode->apNodes);

	parseStmts(pParser, &pNode->apNodes);

	*poSuccess = !pParser->hadError;
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
	else if (tokenkNext == TOKENK_Fn && tokenkNextNext == TOKENK_Identifier)
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
			 tokenkNext == TOKENK_Fn)
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
	SCOPEID declScope = peekScope(pParser).id;

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

    bool success = false;
	pushScope(pParser, SCOPEK_StructDefn);
    Defer(if (!success) popScope(pParser));     // NOTE: In success case we pop it manually before inserting into symbol table

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
			Assert(apVarDeclStmt.cItem > 0);

			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, apVarDeclStmt[0]->startLine, &apVarDeclStmt);
			return Up(pErr);
		}
	}

    success = true;

	auto * pNode = AstNew(pParser, StructDefnStmt, pIdentTok->line);
	setIdent(&pNode->ident, pIdentTok, declScope);
	initMove(&pNode->apVarDeclStmt, &apVarDeclStmt);
	pNode->scopeid = peekScope(pParser).id;

	// Insert into symbol table of enclosing scope

    popScope(pParser);

	SymbolInfo structDefnInfo;
	setSymbolInfo(&structDefnInfo, pNode->ident, SYMBOLK_Struct, Up(pNode));
	tryInsert(&pParser->symbTable, pNode->ident, structDefnInfo, pParser->scopeStack);

	// Insert into type table

	Type type;
	init(&type, false /* isFuncType */);

	type.ident = pNode->ident;
	pNode->typidSelf = ensureInTypeTable(&pParser->typeTable, type, true /* debugAssertIfAlreadyInTable */ );

	return Up(pNode);
}

bool tryParseFuncDefnStmtOrLiteralExpr(Parser * pParser, FUNCHEADERK funcheaderk, AstNode ** ppoNode)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);
	Assert(ppoNode);

	bool success = false;
	bool isDefn = funcheaderk == FUNCHEADERK_Defn;
	int startingLine = peekTokenLine(pParser->pScanner);

	// Parse header

	// NOTE: Push scope before parsing header so that the symbols declared in the header
	//	are subsumed by the function's scope.

	pushScope(pParser, SCOPEK_CodeBlock);
	Defer(if (!success) popScope(pParser));     // NOTE: In success case we pop it manually before inserting into symbol table

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
            *ppoNode = Up(pErr);

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
        *ppoNode = Up(pErr);

		return success = false;
	}

	if (!isDefn)
	{
		auto * pNode = AstNew(pParser, FuncLiteralExpr, startingLine);
		pNode->pBodyStmt = pBody;
		pNode->scopeid = peekScope(pParser).id;
		initMove(&pNode->apParamVarDecls, &apParamVarDecl);
		initMove(&pNode->apReturnVarDecls, &apReturnVarDecl);
		*ppoNode = Up(pNode);

		return success = true;
	}
	else
	{
		auto * pNode = AstNew(pParser, FuncDefnStmt, startingLine);
		pNode->ident = identDefn;
		pNode->pBodyStmt = pBody;
		pNode->scopeid = peekScope(pParser).id;
		initMove(&pNode->apParamVarDecls, &apParamVarDecl);
		initMove(&pNode->apReturnVarDecls, &apReturnVarDecl);

		// Insert into symbol table (pop first so the scope stack doesn't include the scope that the function itself pushed)

        popScope(pParser);

		SymbolInfo funcDefnInfo;
		setSymbolInfo(&funcDefnInfo, pNode->ident, SYMBOLK_Func, Up(pNode));
		tryInsert(&pParser->symbTable, identDefn, funcDefnInfo, pParser->scopeStack);

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

    // START HERE:
    // Problem... tryParseType wants me to register with it the typid that it should update when the type ultimately gets resolved.
    //  This typid is embedded in the AST node that we are creating. The problem is, that we don't want to actually create the AST node
    //  until we are certain that there are no errors in it! But we can't be certain of that until we parse the type! Really I want the
    //  registering to be something we can do at a different time, but we need some context! Maybe if this fails it returns a ptr to a PendingUnresolvedType
    //  with null typidToUpdate and we fill out the typidToUpdate once we create the AST node?

    TypePendingResolution * pTypePending;
    TYPID typidResolved;
    {
        PARSETYPERESULT parseTypeResult = tryParseType(pParser, &apNodeChildren, &typidResolved, &pTypePending);
        if (parseTypeResult == PARSETYPERESULT_ParseFailed)
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

        Assert(Implies(parseTypeResult == PARSETYPERESULT_ParseSucceededTypeResolveSucceeded, !pTypePending));
        Assert(Implies(parseTypeResult == PARSETYPERESULT_ParseSucceededTypeResolveFailed, pTypePending));
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

	// Success!

	SCOPEID declScope = peekScope(pParser).id;

	auto * pNode = AstNew(pParser, VarDeclStmt, line);
	setIdent(&pNode->ident, pVarIdent, declScope);
	pNode->pInitExpr = pInitExpr;

    if (pTypePending)
    {
        pTypePending->pTypidUpdateWhenResolved = &pNode->typid;
    }
    else
    {
        Assert(isTypeResolved(typidResolved));
        pNode->typid = typidResolved;
    }

    if (pNode->ident.pToken)
    {
        SymbolInfo varDeclInfo;
        setSymbolInfo(&varDeclInfo, pNode->ident, SYMBOLK_Var, Up(pNode));
        tryInsert(&pParser->symbTable, pNode->ident, varDeclInfo, pParser->scopeStack);
    }

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
			return pExpr;
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			// IDEA: Better error line message if we walk pExpr and find line of the highest AST node child it has.
			//	We could easily cache that when constructing an AST node...

			auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, pExpr->startLine, pExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseParen);
			return Up(pErr);
		}

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
	else if (peekToken(pParser->pScanner) == TOKENK_Fn)
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
    pExpr->literalk = literalkFromTokenk(pExpr->pToken->tokenk);
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

	// Parse "fn"

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Fn))
	{
		int line = peekTokenLine(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
		append(&pErr->aTokenkValid, TOKENK_Fn);
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

        // NOTE: Use prev scopeid for the func defn ident instead of the one that the func itself
        //  has pushed!

		setIdent(poDefnIdent, pIdent, peekScopePrev(pParser).id);
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
			Assert(pErrToken->tokenk == TOKENK_Identifier);

			auto * pErr = AstNewErr0Child(pParser, UnexpectedTokenkErr, pErrToken->line);
			pErr->pErrToken = pErrToken;
			append(papParamVarDecls, Up(pErr));
			return false;
		}
	}

	// Parse "in" parameters

	if (!tryParseFuncDefnOrLiteralHeaderParamList(pParser, funcheaderk, PARAMK_Param, papParamVarDecls))
	{
		Assert(containsErrorNode(*papParamVarDecls));
		return false;
	}

	// Parse ->

    bool arrowOmitted = false;
    if (!tryConsumeToken(pParser->pScanner, TOKENK_MinusGreater))
    {
        arrowOmitted = true;
    }

    //
    // Parse out parameters
    //

    if (!arrowOmitted)
    {
        if (!tryParseFuncDefnOrLiteralHeaderParamList(pParser, funcheaderk, PARAMK_Return, papReturnVarDecls))
        {
            Assert(containsErrorNode(*papReturnVarDecls));
            return false;
        }
    }

	// Success!

	return true;
}

bool tryParseFuncDefnOrLiteralHeaderParamList(
    Parser * pParser,
    FUNCHEADERK funcheaderk,
    PARAMK paramk,
    DynamicArray<AstNode *> * papParamVarDecls)
{
	Assert(funcheaderk == FUNCHEADERK_Defn || funcheaderk == FUNCHEADERK_Literal);

    bool singleReturnWithoutParens = false;
	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen))
	{
        if (paramk != PARAMK_Return)
        {
		    int line = peekTokenLine(pParser->pScanner);

		    auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
		    append(&pErr->aTokenkValid, TOKENK_OpenParen);
		    append(papParamVarDecls, Up(pErr));
		    return false;
        }
        else
        {
            singleReturnWithoutParens = true;
        }
	}

	bool isFirstParam = true;
	while (true)
	{
        if (!singleReturnWithoutParens && tryConsumeToken(pParser->pScanner, TOKENK_CloseParen))
            break;

        if (singleReturnWithoutParens && !isFirstParam)
            break;

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

PARSETYPERESULT tryParseType(
    Parser * pParser,
    DynamicArray<AstNode *> * papNodeChildren,
    TYPID * poTypidResolved,
    TypePendingResolution ** ppoTypePendingResolution)
{
    AssertInfo(ppoTypePendingResolution, "You must have a plan for handling types that are pending resolution if you call this function!");

	DynamicArray<TypeModifier> aModifiers;
	init(&aModifiers);
	Defer(dispose(&aModifiers););

    *ppoTypePendingResolution = nullptr;
    *poTypidResolved = TYPID_Unresolved;      // TODO: Might need to change this when I add type inference ?

	// NOTE; Since all nodes are stored in papNodeChildren, error nodes do not need to attach any children. It is the
	//	responsibility of the caller to add papNodeChildren to a bubble error if we return false

	while (peekToken(pParser->pScanner) != TOKENK_Identifier &&
		   peekToken(pParser->pScanner) != TOKENK_Fn)
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket))
		{
			// [

			auto * pSubscriptExpr = parseExpr(pParser);
			append(papNodeChildren, pSubscriptExpr);

			if (isErrorNode(*pSubscriptExpr))
			{
				return PARSETYPERESULT_ParseFailed;
			}

			if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket))
			{
				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, pSubscriptExpr->startLine);
				append(&pErr->aTokenkValid, TOKENK_CloseBracket);
				append(papNodeChildren, Up(pErr));
				return PARSETYPERESULT_ParseFailed;
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
			return PARSETYPERESULT_ParseFailed;
		}
	}

	bool parseSucceeded = false;

	// Init type

	Type * pType = newType(pParser);
	if (peekToken(pParser->pScanner) == TOKENK_Identifier)
	{
		init(pType, false /* isFuncType */);
	}
	else
	{
		init(pType, true /* isFuncType */);
	}

	reinitMove(&pType->aTypemods, &aModifiers);

	// Defer cleanup on fail

	Defer(
		if (!parseSucceeded)
		{
			dispose(pType);
			releaseType(pParser, pType);
		}
	);

    TYPID typid;
	if (!pType->isFuncType)
	{
		Token * pTypeIdent = ensureAndClaimPendingToken(pParser);
		consumeToken(pParser->pScanner, pTypeIdent);

		Assert(pTypeIdent->tokenk == TOKENK_Identifier);
		setIdentNoScope(&pType->ident, pTypeIdent);

        typid = resolveIntoTypeTableOrSetPending(pParser, pType, ppoTypePendingResolution);
	}
	else
	{
		AstErr * pErr = nullptr;
		bool funcHeaderSuccess = tryParseFuncHeaderTypeOnly(pParser, &pType->funcType, &pErr);

		Assert(Implies(funcHeaderSuccess, !pErr));

		if (!funcHeaderSuccess)
		{
			append(papNodeChildren, Up(pErr));
			return PARSETYPERESULT_ParseFailed;
		}

        typid = resolveIntoTypeTableOrSetPending(pParser, pType, ppoTypePendingResolution);
	}

	parseSucceeded = true;		// Used in Defer

    if (isTypeResolved(typid))
    {
        *poTypidResolved = typid;
        return PARSETYPERESULT_ParseSucceededTypeResolveSucceeded;
    }
    else
    {
        return PARSETYPERESULT_ParseSucceededTypeResolveFailed;
    }
}

bool tryParseFuncHeaderTypeOnly(Parser * pParser, FuncType * poFuncType, AstErr ** ppoErr)
{
	// NOTE: poFuncType should be inited by caller

	// Parse "fn"

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Fn))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		append(&pErr->aTokenkValid, TOKENK_Fn);

		*ppoErr = UpErr(pErr);
		return false;
	}

	// Subscript expressions are AstNodes that we have to attach as children if we error

	DynamicArray<AstNode *> apNodeChildren;
	init(&apNodeChildren);
	Defer(dispose(&apNodeChildren));

	auto tryParseParameterTypes = [](
		Parser * pParser,
		PARAMK paramk,
		DynamicArray<TYPID> * paTypids,			    // Value we are computing. We will set the typid's of the types we can resolve and set the others to pending
		DynamicArray<AstNode *> * papNodeChildren)	// Boookkeeping so caller can attach children to errors
		-> bool
	{
		// Parse (

		bool singleReturnWithoutParens = false;
		if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen))
		{
			if (paramk != PARAMK_Return)
			{
				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
				append(&pErr->aTokenkValid, TOKENK_OpenParen);
				append(papNodeChildren, Up(pErr));
				return false;
			}
		}

		bool isFirstParam = true;
		while (true)
		{
			if (!singleReturnWithoutParens && tryConsumeToken(pParser->pScanner, TOKENK_CloseParen))
				break;

			if (singleReturnWithoutParens && !isFirstParam)
				break;

			// ,

			if (!isFirstParam && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
			{
				auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
				append(&pErr->aTokenkValid, TOKENK_Comma);
                append(papNodeChildren, Up(pErr));
				return false;
			}

			// type

            {
                TypePendingResolution * pTypePending;
                TYPID typidResolved;
                PARSETYPERESULT parseTypeResult = tryParseType(pParser, papNodeChildren, &typidResolved, &pTypePending);
                if (parseTypeResult = PARSETYPERESULT_ParseFailed)
                {
                    Assert(
                        papNodeChildren->cItem > 0 &&
                        category((*papNodeChildren)[papNodeChildren->cItem - 1]->astk) == ASTCATK_Error
                    );

                    return false;
                }

                if (parseTypeResult == PARSETYPERESULT_ParseSucceededTypeResolveSucceeded)
                {
                    Assert(isTypeResolved(typidResolved));
                    append(paTypids, typidResolved);
                }
                else
                {
                    Assert(parseTypeResult == PARSETYPERESULT_ParseSucceededTypeResolveFailed);
                    Assert(pTypePending);

                    TYPID * pTypidPending = appendNew(paTypids);
                    *pTypidPending = TYPID_Unresolved;

                    pTypePending->pTypidUpdateWhenResolved = pTypidPending;
                }
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

			isFirstParam = false;
		}

        return true;
    };

	//
	// Parse in parameters
	//

	if (!tryParseParameterTypes(pParser, PARAMK_Param, &poFuncType->paramTypids, &apNodeChildren))
	{
		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apNodeChildren);
		*ppoErr = UpErr(pErr);
		return false;
	}

	// Parse ->

    bool arrowOmitted = false;
	if (!tryConsumeToken(pParser->pScanner, TOKENK_MinusGreater))
	{
        arrowOmitted = true;
	}

	//
	// Parse out parameters
	//

    if (!arrowOmitted)
    {
        if (!tryParseParameterTypes(pParser, PARAMK_Return, &poFuncType->returnTypids, &apNodeChildren))
        {
	        auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apNodeChildren);
	        *ppoErr = UpErr(pErr);
	        return false;
        }
    }

    return true;
}

AstNode * finishParsePrimary(Parser * pParser, AstNode * pExpr)
{
	// Parse post-fix operations and allow them to chain

	if (tryConsumeToken(pParser->pScanner, TOKENK_Dot, ensurePendingToken(pParser)))
	{
		// Member access

		return parseVarExpr(pParser, pExpr);
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_Carat, ensurePendingToken(pParser)))
	{
		// Pointer dereference

		auto * pNode = AstNew(pParser, PointerDereferenceExpr, pExpr->startLine);
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
			auto * pErr = AstNewErr2Child(pParser, BubbleErr, pExpr->startLine, pExpr, pSubscriptExpr);
			return Up(pErr);
		}
		else if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
		{
			auto * pErr = AstNewErr2Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pExpr, pSubscriptExpr);
			append(&pErr->aTokenkValid, TOKENK_CloseBracket);

			return Up(pErr);
		}

		auto * pNode = AstNew(pParser, ArrayAccessExpr, pExpr->startLine);
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

		// Error recovery variables

		bool skipCommaCheck = false;
		static const TOKENK s_aTokenkRecoverable[] = { TOKENK_CloseParen, TOKENK_Comma };

		while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			if (!isFirstArg && !tryConsumeToken(pParser->pScanner, TOKENK_Comma))
			{
				int line = peekTokenLine(pParser->pScanner);
				prepend(&apArgs, pExpr);

				auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, line, &apArgs);
				append(&pErr->aTokenkValid, TOKENK_Comma);

				return Up(pErr);
			}

			skipCommaCheck = false;

			AstNode * pExprArg = parseExpr(pParser);
			append(&apArgs, pExprArg);

			if (isErrorNode(*pExprArg))
			{
				prepend(&apArgs, pExpr);

				auto * pErrNode = AstNewErrListChildMove(pParser, BubbleErr, pExprArg->startLine, &apArgs);
				return Up(pErrNode);
			}

			isFirstArg = false;
		}

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
	s.scopek = scopek;

    if (scopek == SCOPEK_BuiltIn)
    {
        for (int i = 0; i < pParser->scopeStack.a.cItem; i++)
        {
            AssertInfo(pParser->scopeStack.a[i].id != SCOPEID_BuiltIn, "Shouldn't contain built-in scope id twice!");
        }

	    s.id = SCOPEID_BuiltIn;
    }
    else if (scopek == SCOPEK_Global)
    {
        for (int i = 0; i < pParser->scopeStack.a.cItem; i++)
        {
            AssertInfo(pParser->scopeStack.a[i].id != SCOPEID_Global, "Shouldn't contain global scope id twice!");
        }

        s.id = SCOPEID_Global;
    }
    else
    {
        s.id = pParser->scopeidNext;
	    pParser->scopeidNext = static_cast<SCOPEID>(pParser->scopeidNext + 1);
    }


	push(&pParser->scopeStack, s);
}

Scope peekScope(Parser * pParser)
{
    bool success;
    Scope s = peek(pParser->scopeStack, &success);
    Assert(success);
    return s;
}

Scope peekScopePrev(Parser * pParser)
{
    bool success;
    Scope s = peekFar(pParser->scopeStack, 1, &success);
    Assert(success);
    return s;
}

Scope popScope(Parser * pParser)
{
    bool success;
	Scope s = pop(&pParser->scopeStack, &success);
    Assert(success);

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
