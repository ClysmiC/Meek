#include "parse.h"
#include "scan.h"

// The downside to using a union instead of inheritance is that I can't implicitly upcast. Since there is quite
//	a bit of pointer casting required, these helpers make it slightly more succinct

#define Up(pNode) reinterpret_cast<AstNode *>(pNode)
#define Down(pNode, astk) reinterpret_cast<Ast##astk *>(pNode)
#define DownConst(pNode, astk) reinterpret_cast<const Ast##astk *>(pNode)
#define AstNew(pParser, astk) reinterpret_cast<Ast##astk *>(astNew(pParser, ASTK_##astk))

// Absolutely sucks that I need to use 0, 1, 2 suffixes. I tried this approach to simulate default parameters in a macro but MSVC has a bug
//  with varargs expansion that made it blow up: https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros

// TODO: need to redo this to support unlimited children. Maybe a macro isn't the way to go here. But maybe I just use a macro
//	with var args that calls a function with var args???

#define AstNewErr0(pParser, astkErr) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, nullptr, nullptr))
#define AstNewErr1(pParser, astkErr, pChild0) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, pChild0, nullptr))
#define AstNewErr2(pParser, astkErr, pChild0, pChild1) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, pChild0, pChild1))

// TODO: should assignment statement be in here?
// What about . operator?

static const ParseOp s_aParseOp[] = {
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

	return true;
}

AstNode * parse(Parser * pParser)
{
	// TODO: Make this parse a list of statements. For now it is just driving some tests...

	AstNode * pNode = parseExpr(pParser);
	return pNode;
}

AstNode * parseStmt(Parser * pParser)
{
	// TODO
	return nullptr;
}

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

AstNode * parseExpr(Parser * pParser)
{
	return parseOp(pParser, s_aParseOp[s_iParseOpMax - 1]);
}

AstNode * parseOp(Parser * pParser, const ParseOp & op)
{
    auto oneStepLower = [](Parser * pParser, int precedence) -> AstNode *
    {
        Assert(precedence >= 0);

        if (precedence == 0)
        {
            return parsePrimary(pParser);
        }
        else
        {
            const ParseOp & opNext = s_aParseOp[precedence - 1];
            return parseOp(pParser, opNext);
        }
    };

    AstNode * pExpr = oneStepLower(pParser, op.precedence);

    Assert(pExpr);
	if (isErrorNode(pExpr)) return pExpr;

	while (tryConsumeToken(pParser->pScanner, op.aTokenkMatch, op.cTokenMatch, ensurePendingToken(pParser)))
	{
        Token * pOp = claimPendingToken(pParser);

        AstNode * pRhsExpr = oneStepLower(pParser, op.precedence);
		if (isErrorNode(pRhsExpr))
		{
			auto * pNode = AstNewErr2(pParser, BubbleErr, pExpr, pRhsExpr);
		}

		auto pNode = AstNew(pParser, BinopExpr);
		pNode->pOp = pOp;
		pNode->pLhsExpr = pExpr;
		pNode->pRhsExpr = pRhsExpr;

		pExpr = Up(pNode);
	}

	return pExpr;
}

AstNode * parsePrimary(Parser * pParser)
{
	if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// Group ( )

		AstNode * pExpr = parseExpr(pParser);

		if (isErrorNode(pExpr))
		{
			auto * pNode = AstNewErr0(pParser, BubbleErr);
            return Up(pNode);
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			auto * pNode = AstNewErr1(pParser, ExpectedTokenkErr, pExpr);
			pNode->tokenk = TOKENK_CloseParen;
			return Up(pNode);
		}

		auto * pNode = AstNew(pParser, GroupExpr);
		pNode->pExpr = pExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, g_aTokenkLiteral, g_cTokenkLiteral, ensurePendingToken(pParser)))
	{
		// Literal

		auto * pNode = AstNew(pParser, LiteralExpr);
		pNode->pToken = claimPendingToken(pParser);
		return Up(pNode);
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_Identifier, ensurePendingToken(pParser)))
	{
		// Identifier

		Token * pIdent = claimPendingToken(pParser);

		auto * pNode = AstNew(pParser, VarExpr);
		pNode->pOwner = nullptr;
		pNode->pIdent = pIdent;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else
	{
		// Error

        Token throwaway;

		auto * pNode = AstNewErr0(pParser, UnexpectedTokenkErr);
		pNode->tokenk = peekToken(pParser->pScanner, &throwaway);
		return Up(pNode);
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
			auto * pNode = AstNewErr1(pParser, ExpectedTokenkErr, pExpr);
			pNode->tokenk = TOKENK_Identifier;

			return Up(pNode);
		}

		// COPYPASTE: from parsePrimary

		Token * pIdent = claimPendingToken(pParser);
		Assert(pIdent->tokenk == TOKENK_Identifier);

		auto * pNode = AstNew(pParser, VarExpr);
		pNode->pOwner = pExpr;
		pNode->pIdent = pIdent;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenBracket, ensurePendingToken(pParser)))
	{
		// Array access

		AstNode * pSubscriptExpr = parseExpr(pParser);
		if (isErrorNode(pSubscriptExpr))
		{
			auto * pNode = AstNewErr1(pParser, BubbleErr, pSubscriptExpr);
            return Up(pNode);
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
		{
			auto * pNode = AstNewErr2(pParser, ExpectedTokenkErr, pExpr, pSubscriptExpr);
			pNode->tokenk = TOKENK_CloseBracket;

			return Up(pNode);
		}

		auto * pNode = AstNew(pParser, ArrayAccessExpr);
		pNode->pArray = pExpr;
		pNode->pSubscript = pSubscriptExpr;

		return finishParsePrimary(pParser, Up(pNode));
	}
	else if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// TODO

		bool isFirstArg = true;

		while (!tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
		{
			// TODO: Error node needs to have dynamic array of children since func call can have unlimited arguments!!!

			if (!isFirstArg && !tryConsumeToken(pParser->pScanner, TOKENK_Comma, ensurePendingToken(pParser)))
			{
				auto * pNode = AstNewErr1(pParser, ExpectedTokenkErr, pExpr);
				pNode->tokenk = TOKENK_Comma;

				// TODO: Add all of the nodes that we have parsed up to this point as children of this errror!
				//	Then you can uncomment the actual return value and get rid of the null placeholder :)
				return nullptr;
				// return Up(pNode);
			}

			// Consume expr
			// Bubble up error if needed.
			//	- Remember to add all of the potentially infinite children to the bubble error!
			// If not error, add to list of arguments
			// Once we read the close paren, create the funcCall node and put the list of arguments
			//	in it!

			isFirstArg = false;
		}

		// Read comma separated list of expressions
		// Read the close paren and error if we don't find one!!!

        return nullptr;
	}
	else
	{
		return pExpr;
	}
}

AstNode * astNew(Parser * pParser, ASTK astk)
{
	AstNode * pNode = allocate(&pParser->astAlloc);
	pNode->astk = astk;
	pNode->id = pParser->iNode;
	pParser->iNode++;

	append(&pParser->astNodes, pNode);

	return pNode;
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, AstNode * pChild0, AstNode * pChild1)
{
	// HMM: Maybe this should be a template so that I can statically assert this...

	Assert(category(astkErr) == ASTCATK_Error);

	auto * pNode = Down(astNew(pParser, astkErr), Err);
	pNode->aChildren[0] = pChild0;
	pNode->aChildren[1] = pChild1;

	return Up(pNode);
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



#if DEBUG

#include <stdio.h>

void debugPrintAst(const AstNode & root)
{
    debugPrintSubAst(root);
    printf("\n");
}

void debugPrintSubAst(const AstNode & node)
{
    switch (node.astk)
    {
        case ASTK_BinopExpr:
        {
            auto pExpr = DownConst(&node, BinopExpr);
            printf("(");
            debugPrintSubAst(*pExpr->pLhsExpr);
            printf(" %s ", pExpr->pOp->lexeme);
            debugPrintSubAst(*pExpr->pRhsExpr);
            printf(")");
        } break;

        case ASTK_GroupExpr:
        {
            auto pExpr = DownConst(&node, GroupExpr);
            printf("(");
            debugPrintSubAst(*pExpr->pExpr);
            printf(")");
        } break;

        case ASTK_LiteralExpr:
        {
            auto pExpr = DownConst(&node, LiteralExpr);
            printf("%s", pExpr->pToken->lexeme);
        } break;

        default:
        {
            // Not implemented!

            Assert(false);
        }
    }
}

#endif
