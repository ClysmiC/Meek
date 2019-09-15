#include "parse.h"

#include "scan.h"

// The downside to using a union instead of inheritance is that I can't implicitly upcast. Since there is quite
//	a bit of pointer casting required, these helpers make it slightly more succinct

#define Up(pNode) reinterpret_cast<AstNode *>(pNode)
#define UpConst(pNode) reinterpret_cast<const AstNode *>(pNode)
#define UpErr(pNode) reinterpret_cast<AstErr *>(pNode)
#define UpErrConst(pNode) reinterpret_cast<const AstErr *>(pNode)
#define Down(pNode, astk) reinterpret_cast<Ast##astk *>(pNode)
#define DownConst(pNode, astk) reinterpret_cast<const Ast##astk *>(pNode)
#define DownErr(pNode) reinterpret_cast<AstErr *>(pNode)
#define DownErrConst(pNode) reinterpret_cast<const AstErr *>(pNode)
#define AstNew(pParser, astk, line) reinterpret_cast<Ast##astk *>(astNew(pParser, ASTK_##astk, line))

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
	init(&pParser->parseTypeAlloc);
	init(&pParser->parseFuncTypeAlloc);
	init(&pParser->astNodes);

	return true;
}

AstNode * parseProgram(Parser * pParser, bool * poSuccess)
{
	// NOTE: Empty program is valid to parse, but I may still decide that
	//	that is a semantic error.

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

		return parseStructDefnStmt(pParser);
	}
	else if (tokenkNext == TOKENK_Func && tokenkNextNext == TOKENK_Identifier)
	{
		// Func defn

		return parseFuncDefnStmt(pParser);
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
	else if (tokenk == TOKENK_OpenBrace)
	{
		if (!doStmt) return parseBlockStmt(pParser);
		else
		{
			// Error!
			return nullptr;	// TODO
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

		if (!doStmt) return parseVarDeclStmt(pParser);
		else
		{
			// Error!
			return nullptr;	// TODO
		}
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

	if (tryConsumeToken(pParser->pScanner, TOKENK_Equal))
	{
		isAssignment = true;

		// Parse rhs expression

		pRhsExpr = parseExpr(pParser);
		if (isErrorNode(*pRhsExpr))
		{
			auto * pErr = AstNewErr2Child(pParser, BubbleErr, pRhsExpr->startLine, pLhsExpr, pRhsExpr);
			return Up(pErr);
		}

		if (tryConsumeToken(pParser->pScanner, TOKENK_Equal))
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
			pErr->tokenk = TOKENK_Semicolon;
			return Up(pErr);
		}
		else
		{
			auto * pErr = AstNewErr1Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner), pLhsExpr);
			pErr->tokenk = TOKENK_Semicolon;
			return Up(pErr);
		}
	}

	// Success!

	if (isAssignment)
	{
		auto * pNode = AstNew(pParser, AssignStmt, pLhsExpr->startLine);
		pNode->pLhsExpr = pLhsExpr;
		pNode->pRhsExpr = pRhsExpr;
		return Up(pNode);
	}
	else
	{
		Assert(!pRhsExpr);

		auto * pNode = AstNew(pParser, ExprStmt, pLhsExpr->startLine);
		pNode->pExpr = pLhsExpr;
		return Up(pNode);
	}
}

AstNode * parseStructDefnStmt(Parser * pParser)
{
	if (!tryConsumeToken(pParser->pScanner, TOKENK_Struct))
	{
		// TODO: Add a peekTokenLine(..) function because I am doing this a lot just to get
		//	line # for error tokens

		Token errToken;
		peekToken(pParser->pScanner, &errToken);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, errToken.line);
		pErr->tokenk = TOKENK_Struct;
		return Up(pErr);
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		Token errToken;
		peekToken(pParser->pScanner, &errToken);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, errToken.line);
		pErr->tokenk = TOKENK_Identifier;
		return Up(pErr);
	}

	Token * pIdent = claimPendingToken(pParser);
	Assert(pIdent->tokenk == TOKENK_Identifier);

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, pIdent->line);
		pErr->tokenk = TOKENK_OpenBrace;
		return Up(pErr);
	}

	DynamicArray<AstNode *> apVarDeclStmt;
	init(&apVarDeclStmt);
	Defer(Assert(apVarDeclStmt.pBuffer == nullptr));

	while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBrace))
	{
		// TODO: Do I need this? What does the error message look like without it and
		//	would this be a better error message??

		// if (peekToken(pParser->pScanner) == TOKENK_Eof)
		// {
		//	   Token * pEof = ensureAndClaimPendingToken(pParser);
		//	   consumeToken(pParser->pScanner, pEof);
		//	   auto * pErr = AstNewErrListChildMove(pParser, UnexpectedTokenkErr, pEof->line, &apVarDeclStmt);
		//	   pErr->pErrToken = pEof;
		//	   return Up(pErr);
		// }

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
	auto * pNode = AstNew(pParser, StructDefnStmt, pIdent->line);
	pNode->pIdent = pIdent;
	initMove(&pNode->apVarDeclStmt, &apVarDeclStmt);

	return Up(pNode);
}

AstNode * parseFuncDefnStmt(Parser * pParser)
{
	int startingLine = peekTokenLine(pParser->pScanner);

	// TODO: This solution still isn't *that* great since if we error we don't attach any of the func header
	//	to the AST which will lead to a bunch more errors since the variables that we would expect to
	//	find in the header will not be anywhere in the AST!

	bool addedIntermediatesToAst = false;

	ParseFuncType * pPft = nullptr;
	Token * pIdent = nullptr;
	Defer(
		if (!addedIntermediatesToAst)
		{
			if (pPft) releaseParseFuncType(pParser, pPft);
			if (pIdent) releaseToken(pParser, pIdent);
		}
	);

	// Parse header

	{
		AstNode * pErrFuncHeader = nullptr;
		if (!tryParseFuncHeader(pParser, FUNCHEADERK_Defn, &pPft, &pErrFuncHeader, &pIdent))
		{
			Assert(pErrFuncHeader);
			Assert(!pPft);

			return pErrFuncHeader;
		}

		Assert(pIdent);
		Assert(pPft);
		Assert (!pErrFuncHeader);
	}

	// // Parse {

	// if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	// {
	// 	int line = peekTokenLine(pParser->pScanner);
	// 	auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
	// 	pErr->tokenk = TOKENK_OpenBrace;
	// 	return Up(pErr);
	// }

	// // Parse statements until }

	// DynamicArray<AstNode *> apStmts;
	// init(&apStmts);
	// Defer(Assert(!apStmts.pBuffer));		// buffer should get "moved" into AST

	// while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBrace))
	// {
	// 	AstNode * pStmt = parseStmt(pParser);
	// 	append(&apStmts, pStmt);

	// 	if (isErrorNode(*pStmt))
	// 	{
	// 		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, peekTokenLine(pParser->pScanner), &apStmts);
	// 		return Up(pErr);
	// 	}

	// 	Assert(category(*pStmt) == ASTCATK_Stmt);
	// }

	AstNode * pBody = parseDoStmtOrBlockStmt(pParser);

	if (isErrorNode(pBody))
	{
		return pBody;
	}

	// Success!

	addedIntermediatesToAst = true;

	auto * pNode = AstNew(pParser, FuncDefnStmt, startingLine);
	pNode->pIdent = pIdent;
	pNode->pFuncType = pPft;
	pNode->pBody = pBody;
	return Up(pNode);
}

AstNode * parseVarDeclStmt(Parser * pParser, EXPECTK expectkName, EXPECTK expectkInit, EXPECTK expectkSemicolon)
{
	AssertInfo(expectkName != EXPECTK_Forbidden, "Function does not (currently) support being called with expectkName forbidden");
	AssertInfo(expectkSemicolon != EXPECTK_Optional, "Semicolon should either be required or forbidden");

	DynamicArray<ParseTypeModifier> aModifiers;
	init(&aModifiers);
	Defer(
		if (aModifiers.pBuffer != nullptr)
		{
			destroy(&aModifiers);
		}
	);

	// This list already kind of exists embedded in the modifiers, but we store it
	//	separately here to make it easier to attach them to an error node should
	//	we ultimately produce an error.

	DynamicArray<AstNode *> apNodeChildren;
	init(&apNodeChildren);
	Defer(
		if (apNodeChildren.pBuffer != nullptr)
		{
			destroy(&apNodeChildren);
		}
	);

	// Remember line # of the first token

	int line;
	{
		Token tokenNext;
		peekToken(pParser->pScanner, &tokenNext);
		line = tokenNext.line;
	}

	// BEGIN PARSE TYPE.... move this to separate function!


	while (peekToken(pParser->pScanner, ensurePendingToken(pParser)) != TOKENK_Identifier &&
            peekToken(pParser->pScanner, ensurePendingToken(pParser)) != TOKENK_Func)
	{
		if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket, ensurePendingToken(pParser)))
		{
			// [

			auto * pSubscriptExpr = parseExpr(pParser);
			append(&apNodeChildren, pSubscriptExpr);

			if (isErrorNode(*pSubscriptExpr))
			{
				auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, pSubscriptExpr->startLine, &apNodeChildren);

				// TODO: panic error recovery?

				return Up(pErr);
			}

			if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
			{
				auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, pSubscriptExpr->startLine, &apNodeChildren);
				pErr->tokenk = TOKENK_CloseBracket;
				return Up(pErr);
			}

			ParseTypeModifier mod;
			mod.typemodk = TYPEMODK_Array;
			mod.pSubscriptExpr = pSubscriptExpr;
			append(&aModifiers, mod);
		}
		else if (tryConsumeToken(pParser->pScanner, TOKENK_Carat, ensurePendingToken(pParser)))
		{
			// ^

			ParseTypeModifier mod;
			mod.typemodk = TYPEMODK_Pointer;
			append(&aModifiers, mod);
		}
		else
		{
			return handleScanOrUnexpectedTokenkErr(pParser, &apNodeChildren);
		}
	}

    Token * pTypeIdent = nullptr;       // Only for non-func type

	ParseFuncType * pPft = nullptr;
	bool addedPftToAst = false;
	Defer(
		if (!addedPftToAst && pPft)
		{
			releaseParseFuncType(pParser, pPft);
		}
	);

	bool isFuncType = false;
	if (pParser->pPendingToken->tokenk == TOKENK_Identifier)
	{
        // We want to claim and consume the token if it is an identifier,
        //  but if it is a function we only want to have peeked it, since
        //  tryParseFuncHeader will be expecting "func" as its first token

		isFuncType = false;
        pTypeIdent = claimPendingToken(pParser);
        consumeToken(pParser->pScanner, pTypeIdent);
	}
	else if (pParser->pPendingToken->tokenk == TOKENK_Func)
	{
		isFuncType = true;
        int lineOfFunc = pParser->pPendingToken->line;

		AstNode * pErrFuncHeader = nullptr;
		if (!tryParseFuncHeader(pParser, FUNCHEADERK_VarType, &pPft, &pErrFuncHeader))
		{
			Assert(pErrFuncHeader);
            Assert(!pPft);

			append(&apNodeChildren, pErrFuncHeader);

			auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, lineOfFunc, &apNodeChildren);
			return Up(pErr);
		}

		Assert(pPft);	// Got a function header, nice!
        Assert(!pErrFuncHeader);
	}
	else
	{
		Assert(false);
	}

	// END PARSE TYPE


	// Parse name

	Token * pVarIdent = nullptr;

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
			pErr->tokenk = TOKENK_Identifier;
			return Up(pErr);
		}
	}
	else
	{
		Assert(false);
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
			pErr->tokenk = TOKENK_Semicolon;
			return Up(pErr);
		}
	}

	// Success!

	ParseType * pParseType = newParseType(pParser);

	if (isFuncType)
	{
		pParseType->isFuncType = true;
		pParseType->pParseFuncType = pPft;
		addedPftToAst = true;
	}
	else
	{
		Assert(pTypeIdent->tokenk == TOKENK_Identifier);
		pParseType->isFuncType = false;
		pParseType->pType = pTypeIdent;
	}

	initMove(&pParseType->aTypemods, &aModifiers);

	auto * pNode = AstNew(pParser, VarDeclStmt, line);
	pNode->pIdent = pVarIdent;
	pNode->pType = pParseType;
	pNode->pInitExpr = pInitExpr;

	return Up(pNode);
}

AstNode * parseIfStmt(Parser * pParser)
{
	return nullptr;		// TODO
}

AstNode * parseWhileStmt(Parser * pParser)
{
	// Parse 'while'

	if (!tryConsumeToken(pParser->pScanner, TOKENK_While))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		pErr->tokenk = TOKENK_While;
		return Up(pErr);
	}

	int whileLine = prevTokenLine(pParser->pScanner);

	// Parse cond expr

	AstNode * pCondExpr = parseExpr(Parser * pParser);
	if (isErrorNode(pCondExpr))
	{
		return pCondExpr;
	}

	// Parse 'do <stmt>' or block stmt

	AstNode * pBodyStmt = parseDoStmtOrBlockStmt(pParser);
	if (isErrorNode(pBodyStmt))
	{
		auto * pErr = AstNewErr2Child(pParser, BubbleErr, pStmt->startLine, pBodyStmt->startLine, pCondExpr, pBodyStmt);
		return Up(pErr);
	}

	// Success!

	auto * pNode = AstNew(pParser, WhileStmt, whileLine);
	pNode->pCondExpr = pCondExpr;
	pNode->pBodyStmt = pBodyStmt;
	return Up(pNode);
}

AstNode * parseDoStmtOrBlockStmt(Parser * pParser)
{
	TOKENK tokenk = peekToken(pParser->pScanner);
	if (tokenk != TOKENK_OpenBrace &&
		tokenk != TOKENK_Do)
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		pErr->tokenkValid[0] = TOKENK_OpenBrace;
		pErr->tokenkValid[1] = TOKENK_Do;
		pErr->cTokenkValid = 2;
		return Up(pErr);
	}

	AstNode * pStmt = nullptr;
	if (tokenk == TOKENK_OpenBrace)
	{
		AstNode * pStmt = parseBlockStmt(Parser * pParser);

		if (isErrorNode(pStmt))
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
		AstNode * pStmt = parseStmt(pParser, isDoStmt);

		Assert(pStmt->astk != ASTK_BlockStmt);
		Assert(pStmt->astk != ASTK_VarDeclStmt);

		if (isErrorNode(pStmt))
		{
			auto * pErr = AstNewErr1Child(pParser, BubbleErr, pStmt->startLine, pStmt);
			return Up(pErr);
		}
	}

	return pStmt;
}

AstNode * parseBlockStmt(Parser * pParser)
{
	// Parse {

	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenBrace))
	{
		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, peekTokenLine(pParser->pScanner));
		pErr->tokenkValid[0] = TOKENK_OpenBrace;
		pErr->cTokenkValid = 1;
		return Up(pErr);
	}

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
				pErr->tokenk = TOKENK_CloseParen;
				return Up(pErr);
			}
		}

	finishGroup:
		auto * pNode = AstNew(pParser, GroupExpr, pOpenParen->line);
		pNode->pExpr = pExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, g_aTokenkLiteral, g_cTokenkLiteral, ensurePendingToken(pParser)))
	{
		// Literal

		Token * pLiteralToken = claimPendingToken(pParser);

		auto * pNode = AstNew(pParser, LiteralExpr, pLiteralToken->line);
		pNode->pToken = pLiteralToken;
		return Up(pNode);
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		// Identifier

		Token * pIdent = claimPendingToken(pParser);

		auto * pNode = AstNew(pParser, VarExpr, pIdent->line);
		pNode->pOwner = nullptr;
		pNode->pIdent = pIdent;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else
	{
		return handleScanOrUnexpectedTokenkErr(pParser);
	}
}

bool tryParseFuncHeader(
	Parser * pParser,
	FUNCHEADERK funcheaderk,
	ParseFuncType ** ppoFuncType,
	AstNode ** ppoErrNode,
	Token ** ppoDefnIdent)
{
	EXPECTK expectkName = (funcheaderk == FUNCHEADERK_Defn) ? EXPECTK_Required : EXPECTK_Forbidden;

	AssertInfo(Implies(expectkName == EXPECTK_Forbidden, !ppoDefnIdent), "Don't provide an ident pointer in a context where a name is illegal!");
	AssertInfo(Implies(expectkName != EXPECTK_Forbidden, ppoDefnIdent), "Should provide an ident pointer in a context where a name is legal!");

	// NOTE: If function succeeds, we assign ppoFuncType to the resulting func type that we create
	//	If it fails, we assign ppoErrorNode to the resulting error node that we generate

	*ppoFuncType = nullptr;
	*ppoErrNode = nullptr;

	// Parse "func"

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Func))
	{
		int line = peekTokenLine(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
		pErr->tokenk = TOKENK_Func;
		*ppoErrNode = Up(pErr);
		return false;
	}

	// Parse definition name

	if (expectkName == EXPECTK_Required)
	{
		if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			int line = peekTokenLine(pParser->pScanner);

			auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
			pErr->tokenk = TOKENK_Identifier;
			*ppoErrNode = Up(pErr);
			return false;
		}

		*ppoDefnIdent = claimPendingToken(pParser);
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
			*ppoErrNode = Up(pErr);
			return false;
		}
	}

	// Parse "in" parameters

	DynamicArray<AstNode *> apInParamVarDecls;
	init(&apInParamVarDecls);
	Defer(Assert(!apInParamVarDecls.pBuffer));		// buffer should get "moved" into AST

	if (!tryParseFuncHeaderParamList(pParser, funcheaderk, &apInParamVarDecls))
	{
		Assert(containsErrorNode(apInParamVarDecls));

		int line = peekTokenLine(pParser->pScanner);
		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, line, &apInParamVarDecls);
		*ppoErrNode = Up(pErr);
		return false;
	}

	// TODO: Maybe add syntax for optional -> here (between input params and out params)
	//	which could be useful in the following case
	//
	// func(func(int, func(int) -> (func(float)->(int))) -> (func(float) -> (int))) -> (int)
	//
	// Uhhh maybe this actually doesn't help at all lol


	// Parse "out" parameters (a.k.a. return types)

	DynamicArray<AstNode *> apOutParamVarDecls;
	init(&apOutParamVarDecls);
	Defer(Assert(!apOutParamVarDecls.pBuffer));		// buffer should get "moved" into AST

	if (!tryParseFuncHeaderParamList(pParser, funcheaderk, &apOutParamVarDecls))
	{
		Assert(containsErrorNode(apOutParamVarDecls));

		// NOTE: Append output parameters to the list of input parameters to make our life
		//	easier using the AstNewErr macro. Note that this means we have to manually destroy
		//	the out param list since only the (now combined) in param list is getting "moved"
		//	into the error node.

		appendMultiple(&apInParamVarDecls, apOutParamVarDecls.pBuffer, apOutParamVarDecls.cItem);
		destroy(&apOutParamVarDecls);

		int line = peekTokenLine(pParser->pScanner);
		auto * pErr = AstNewErrListChildMove(pParser, BubbleErr, line, &apInParamVarDecls);
		*ppoErrNode = Up(pErr);
		return false;
	}

	// Success!

	ParseFuncType * pft = newParseFuncType(pParser);
	initMove(&pft->apParamVarDecls, &apInParamVarDecls);
	initMove(&pft->apReturnVarDecls, &apOutParamVarDecls);
    *ppoFuncType = pft;
	return true;
}

bool tryParseFuncHeaderParamList(Parser * pParser, FUNCHEADERK funcheaderk, DynamicArray<AstNode *> * papParamVarDecls)
{
	if (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen))
	{
		int line = peekTokenLine(pParser->pScanner);

		auto * pErr = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
		pErr->tokenk = TOKENK_OpenParen;
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
			pErr->tokenk = TOKENK_Comma;
			append(papParamVarDecls, Up(pErr));
			return false;
		}

		const static EXPECTK s_expectkVarName = EXPECTK_Optional;
		const static EXPECTK s_expectkSemicolon = EXPECTK_Forbidden;

		EXPECTK expectkVarInit = (funcheaderk == FUNCHEADERK_VarType) ? EXPECTK_Forbidden : EXPECTK_Optional;

		AstNode * pNode = parseVarDeclStmt(pParser, s_expectkVarName, expectkVarInit, s_expectkSemicolon);
		append(papParamVarDecls, pNode);

		if (isErrorNode(*pNode))
		{
			return false;
		}

        isFirstParam = false;
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
			Assert(Down(pExpr, VarExpr)->pIdent->tokenk == TOKENK_Identifier);
		} break;

		default:
			Assert(false);
	}

	if (tryConsumeToken(pParser->pScanner, TOKENK_Dot, ensurePendingToken(pParser)))
	{
		// Member access

		if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
		{
			Token * errToken = ensurePendingToken(pParser);
			auto * pErrNode = AstNewErr1Child(pParser, ExpectedTokenkErr, errToken->line, pExpr);
			pErrNode->tokenk = TOKENK_Identifier;

			return Up(pErrNode);
		}

		// COPYPASTE: from parsePrimary

		Token * pIdent = claimPendingToken(pParser);
		Assert(pIdent->tokenk == TOKENK_Identifier);

		auto * pNode = AstNew(pParser, VarExpr, pIdent->line);
		pNode->pOwner = pExpr;
		pNode->pIdent = pIdent;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket, ensurePendingToken(pParser)))
	{
		// Array access

		AstNode * pSubscriptExpr = parseExpr(pParser);
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
			Token * pErrToken = ensurePendingToken(pParser);
			peekToken(pParser->pScanner, pErrToken);

			auto * pErrNode = AstNewErr2Child(pParser, ExpectedTokenkErr, pErrToken->line, pExpr, pSubscriptExpr);
			pErrNode->tokenk = TOKENK_CloseBracket;

			return Up(pErrNode);
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
					auto * pErrNode = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
					pErrNode->tokenk = TOKENK_Comma;

					append(&apArgs, Up(pErrNode));

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

					auto * pErrNode = AstNewErrListChildMove(pParser, ExpectedTokenkErr, line, &apArgs);
					pErrNode->tokenk = TOKENK_Comma;

					return Up(pErrNode);
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
	Defer(destroy(&mpLevelSkip));

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

	auto debugPrintParseFuncType = [printTabs, printChildren](const ParseFuncType & parseFuncType, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
	{
		if (parseFuncType.apParamVarDecls.cItem == 0)
		{
			printf("\n");
			printTabs(level, true, false, pMapLevelSkip);
			printf("(no params)");
		}
		else
		{
            printChildren(parseFuncType.apParamVarDecls, level, "param", false, pMapLevelSkip);
		}

		if (parseFuncType.apReturnVarDecls.cItem == 0)
		{
			printf("\n");
			printTabs(level, true, skipAfterArrow, pMapLevelSkip);
			printf("(no return vals)");
		}
		else
		{
            printChildren(parseFuncType.apParamVarDecls, level, "return val", skipAfterArrow, pMapLevelSkip);
		}
	};

	auto debugPrintParseType = [printTabs, debugPrintParseFuncType, setSkip](const ParseType & parseType, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
	{
        setSkip(pMapLevelSkip, level, false);

		int levelNext = level + 1;

		for (uint i = 0; i < parseType.aTypemods.cItem; i++)
		{
			printf("\n");
			printTabs(level, false, false, pMapLevelSkip);

			ParseTypeModifier ptm = parseType.aTypemods[i];

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

		if (parseType.isFuncType)
		{
            printf("\n");
            printTabs(level, false, false, pMapLevelSkip);
            printf("(func)");

            printf("\n");
            printTabs(level, false, false, pMapLevelSkip);

			debugPrintParseFuncType(*parseType.pParseFuncType, level, skipAfterArrow, pMapLevelSkip);
		}
		else
		{
			printf("\n");
			printTabs(level, true, skipAfterArrow, pMapLevelSkip);
			printf("%s", parseType.pType->lexeme);
		}
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
			Defer(destroy(&errMsgs));

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

			printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pErr->tokenk]);

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
				printf(". %s", pExpr->pIdent->lexeme);
				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("(owner):");
				debugPrintSubAst(*pExpr->pOwner, levelNext, true, pMapLevelSkip);
			}
			else
			{
				printf("%s", pExpr->pIdent->lexeme);
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

			printf("=");
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

			if (pStmt->pIdent)
			{
				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("(ident):");
				printf("\n");

				printTabs(levelNext, true, false, pMapLevelSkip);
				printf("%s", pStmt->pIdent->lexeme);
				printf("\n");

				printTabs(levelNext, false, false, pMapLevelSkip);
				printf("\n");
			}

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(type):");

            bool hasInitExpr = (pStmt->pInitExpr != nullptr);

			debugPrintParseType(*pStmt->pType, levelNext, !hasInitExpr, pMapLevelSkip);
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
            printf("%s", pStmt->pIdent->lexeme);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

			printChildren(pStmt->apVarDeclStmt, levelNext, "vardecl", true, pMapLevelSkip);
		} break;

		case ASTK_FuncDefnStmt:
		{
			// TODO
			auto * pStmt = DownConst(&node, FuncDefnStmt);

			printf("(func defn)");

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(name):");

            printf("\n");
            printTabs(levelNext, true, false, pMapLevelSkip);
            printf("%s", pStmt->pIdent->lexeme);

			printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

			debugPrintParseFuncType(*pStmt->pFuncType, levelNext, false, pMapLevelSkip);

			printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

			printChildren(pStmt->apStmts, levelNext, "stmt", true, pMapLevelSkip);
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
