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

AstNode * parseStmt(Parser * pParser)
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
	else if (tokenkNext == TOKENK_OpenBracket ||
			 tokenkNext == TOKENK_Carat ||
			 (tokenkNext == TOKENK_Identifier && tokenkNextNext == TOKENK_Identifier) ||
			 tokenkNext == TOKENK_Func)
	{
		// HMM: Should I put this earlier since var decls are so common?
		//	I have to have it at least after func defn so I can be assured
		//	that if we peek "func" that it isn't part of a func defn

		// Var decl

		return parseVarDeclStmt(pParser);
	}


	return parseExprStmt(pParser);
}

AstNode * parseExprStmt(Parser * pParser)
{
	AstNode * pExpr = parseExpr(pParser);
	if (isErrorNode(*pExpr))
	{
		return pExpr;
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon, ensurePendingToken(pParser)))
	{
        Token * pErrToken = ensurePendingToken(pParser);
        peekToken(pParser->pScanner, pErrToken);

		auto * pErrNode = AstNewErr1Child(pParser, ExpectedTokenkErr, pErrToken->line, pExpr);
		pErrNode->tokenk = TOKENK_Semicolon;
	}

    auto * pNode = AstNew(pParser, ExprStmt, pExpr->startLine);
    pNode->pExpr = pExpr;
	return Up(pNode);
}

AstNode * parseStructDefnStmt(Parser * pParser)
{
    if (!tryConsumeToken(pParser->pScanner, TOKENK_Struct))
	{
        // TODO: Add a peekTokenLine(..) function because I am doing this a lot just to get
        //  line # for error tokens

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
        if (peekToken(pParser->pScanner) == TOKENK_Eof)
        {
            Token * pEof = ensureAndClaimPendingToken(pParser);
            consumeToken(pParser->pScanner, pEof);
            auto * pErr = AstNewErrListChildMove(pParser, UnexpectedTokenkErr, pEof->line, &apVarDeclStmt);
            pErr->pErrToken = pEof;
            return Up(pErr);
        }

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

}

AstNode * parseVarDeclStmt(Parser * pParser)
{
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

	bool isFuncType;

	// BEGIN PARSE TYPE.... move this to separate function!

	if (peekToken(pParser->pScanner) == TOKENK_Func)
	{
		isFuncType = true;
		ParseFuncType * pPft = newParseFuncType(pParser);

		if (tryParseFuncHeader(pParser, false, pPft))
		{
			// TODO: wrap pPft into an AST node and return it
		}
		else
		{
			releaseParseFuncType(pParser, pPft);

			// TODO: how to handle this error. Probably need to return more granular information from tryParseFuncHeader
			//	than just succed/fail. But since func header isn't its own AST node (should it be???) we can't really bubble
			//	up.
		}
	}
	else
	{
		isFuncType = false;

		while (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
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
	}

	Token * pTypeIdent = claimPendingToken(pParser);
	Assert(pTypeIdent->tokenk == TOKENK_Identifier);


	// END PARSE TYPE


	if (!tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, pTypeIdent->line, &apNodeChildren);
		pErr->tokenk = TOKENK_Identifier;
		return Up(pErr);
	}

	Token * pVarIdent = claimPendingToken(pParser);
	Assert(pVarIdent->tokenk == TOKENK_Identifier);

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
	}

	if (!tryConsumeToken(pParser->pScanner, TOKENK_Semicolon, ensurePendingToken(pParser)))
	{
		Token * pErrToken = ensurePendingToken(pParser);
		peekToken(pParser->pScanner, pErrToken);

		auto * pErr = AstNewErrListChildMove(pParser, ExpectedTokenkErr, pErrToken->line, &apNodeChildren);
		pErr->tokenk = TOKENK_Semicolon;
		return Up(pErr);
	}

	// Success!

	ParseType * pParseType = newParseType(pParser);
	pParseType->pType = pTypeIdent;
	pParseType->isFuncType = isFuncType;
	initMove(&pParseType->aTypemods, &aModifiers);

	auto * pNode = AstNew(pParser, VarDeclStmt, line);
	pNode->pIdent = pVarIdent;
	pNode->pType = pParseType;
	pNode->pInitExpr = pInitExpr;

	return Up(pNode);
}

#if LATER
AstNode * parseAssignStmt(Parser * pParser)
{
	AstNode * pLhsExpr = parseExpr(pParser);		// Semantic analysis will enforce lvalues here
	if (isErrorNode(pLhsExpr)) return pLhsExpr;

	Token token;
	if (tryConsumeToken(pParser->pScanner, TOKENK_Equal, &token))
	{
		AstNode * pRhsExpr = parseExpr(pParser);
		if (isErrorNode(pRhsExpr)) return pRhsExpr;

		auto pNode = AstNew(pParser, AssignStmt);
		pNode->pLhsExpr = pLhsExpr;
		pNode->pRhsExpr = pRhsExpr;

		return Up(pNode);
	}
	else
	{
		return pLhsExpr;
	}
}
#endif

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
			if (!isFirstArg && !tryConsumeToken(pParser->pScanner, TOKENK_Comma, ensurePendingToken(pParser)))
			{
                int line;
                {
                    Token * pErrToken = ensurePendingToken(pParser);
                    peekToken(pParser->pScanner, pErrToken);
                    line = pErrToken->line;
                }

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
	Token * pErrToken =  ensureAndClaimPendingToken(pParser);
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

AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * pChild0, AstNode * pChild1)
{
	Assert(Implies(pChild1, pChild0));

	AstNode * apChildren[2] = { pChild0, pChild1 };
	int cPChildren = (!pChild0) ? 0 : (!pChild1) ? 1 : 2;
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
// 	int bar;
// 	int foo;

// 	%bar

// 	struct Nested
// 	 {
// 		 int j;
// 		 int k;
// 	}

// 	int z;
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

	auto debugPrintParseType = [printTabs](const ParseType & parseType, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
	{
		int levelNext = level + 1;

		for (uint i = 0; i < parseType.aTypemods.cItem; i++)
		{
			printf("\n");
			printTabs(level, false, skipAfterArrow, pMapLevelSkip);

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

		printf("\n");
		printTabs(level, true, skipAfterArrow, pMapLevelSkip);
		printf("%s", parseType.pType->lexeme);
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

			printErrChildren(*DownErrConst(&node), levelNext, pMapLevelSkip);
		} break;

		case ASTK_BubbleErr:
		{
			auto * pErr = DownConst(&node, BubbleErr);
            auto * pErrCasted = UpErrConst(pErr);

			printf("%s (bubble)", parseErrorString);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //  it an empty array of children it will still just work.

                printf("\n");

                printTabs(levelNext, false, false, pMapLevelSkip);
			    printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
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
                //  it an empty array of children it will still just work.

                printf("\n");

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
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
                //  it an empty array of children it will still just work.

                printf("\n");

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
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

        case ASTK_VarDeclStmt:
        {
            auto * pStmt = DownConst(&node, VarDeclStmt);

            printf("(var decl)");
			printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(ident)");
			printf("\n");

			printTabs(levelNext, true, false, pMapLevelSkip);
			printf("%s", pStmt->pIdent->lexeme);
			printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(type)");

			debugPrintParseType(*pStmt->pType, levelNext, false, pMapLevelSkip);
			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);

			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
			printf("(init)");

			if (pStmt->pInitExpr)
			{
				debugPrintSubAst(*pStmt->pInitExpr, levelNext, true, pMapLevelSkip);
			}
			else
			{
				printf("\n");
				printTabs(levelNext, true, true, pMapLevelSkip);
				printf("(default)");
			}
        } break;

		case ASTK_StructDefnStmt:
		{
			auto * pStmt = DownConst(&node, StructDefnStmt);

			printf("(struct decl)");

			printChildren(pStmt->apVarDeclStmt, levelNext, "vardecl", true, pMapLevelSkip);
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
