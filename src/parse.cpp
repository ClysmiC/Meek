#include "parse.h"
#include "scan.h"

// The downside to using a union instead of inheritance is that I can't implicitly upcast. Since there is quite
//	a bit of pointer casting required, these helpers make it slightly more succinct

#define Up(pNode) reinterpret_cast<AstNode *>(pNode)
#define UpErrConst(pNode) reinterpret_cast<const AstErr *>(pNode)
#define Down(pNode, astk) reinterpret_cast<Ast##astk *>(pNode)
#define DownConst(pNode, astk) reinterpret_cast<const Ast##astk *>(pNode)
#define AstNew(pParser, astk) reinterpret_cast<Ast##astk *>(astNew(pParser, ASTK_##astk))

// Absolutely sucks that I need to use 0, 1, 2 suffixes. I tried this approach to simulate default parameters in a macro but MSVC has a bug
//  with varargs expansion that made it blow up: https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros

// TODO: need to redo this to support unlimited children. Maybe a macro isn't the way to go here. But maybe I just use a macro
//	with var args that calls a function with var args???

#define AstNewErr0Child(pParser, astkErr) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, nullptr, nullptr))
#define AstNewErr1Child(pParser, astkErr, pChild0) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, pChild0, nullptr))
#define AstNewErr2Child(pParser, astkErr, pChild0, pChild1) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, pChild0, pChild1))

// NOTE: This macro is functionally equivalent to AstNewErr2Child since we rely on function overloading to choose the correct one.
//	I'm not really happy with this solution but it's the best I have right now.

#define AstNewErrListChild(pParser, astkErr, aPChildren, cPChildren) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, aPChildren, cPChildren))

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
    init(&pParser->astNodes);

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
			auto * pNode = AstNewErr2Child(pParser, BubbleErr, pExpr, pRhsExpr);
            return Up(pNode);
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
			auto * pNode = AstNewErr1Child(pParser, BubbleErr, pExpr);
            return Up(pNode);
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			auto * pNode = AstNewErr1Child(pParser, ExpectedTokenkErr, pExpr);
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

		auto * pNode = AstNewErr0Child(pParser, UnexpectedTokenkErr);
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
			auto * pNode = AstNewErr1Child(pParser, ExpectedTokenkErr, pExpr);
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
			auto * pNode = AstNewErr1Child(pParser, BubbleErr, pSubscriptExpr);
            return Up(pNode);
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
		{
			auto * pNode = AstNewErr2Child(pParser, ExpectedTokenkErr, pExpr, pSubscriptExpr);
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
        // Function call

		DynamicArray<AstNode *> aPArgs;
		init(&aPArgs);

		Defer(Assert(!aPArgs.pBuffer));     // buffer should get "moved" into AST

		bool isFirstArg = true;

		while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			if (!isFirstArg && !tryConsumeToken(pParser->pScanner, TOKENK_Comma, ensurePendingToken(pParser)))
			{
				DynamicArray<AstNode *> aPChildren;
				initMove(&aPChildren, &aPArgs);
				prepend(&aPChildren, pExpr);

				auto * pNode = AstNewErrListChild(pParser, ExpectedTokenkErr, aPChildren.pBuffer, aPChildren.cItem);
				pNode->tokenk = TOKENK_Comma;

				return Up(pNode);
			}

			// Consume expr
			// If not error, add to list of arguments
			// Once we read the close paren, create the funcCall node and put the list of arguments
			//	in it!

			AstNode * pExprArg = parseExpr(pParser);
			append(&aPArgs, pExprArg);

			if (isErrorNode(pExprArg))
			{
				DynamicArray<AstNode *> aPChildren;
				initMove(&aPChildren, &aPArgs);
				prepend(&aPChildren, pExpr);

				auto * pNode = AstNewErrListChild(pParser, BubbleErr, aPChildren.pBuffer, aPChildren.cItem);

				return Up(pNode);
			}

			isFirstArg = false;
		}

		auto * pNode = AstNew(pParser, FuncCallExpr);
		pNode->pFunc = pExpr;
		initMove(&pNode->aPArgs, &aPArgs);

		return finishParsePrimary(pParser, Up(pNode));
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
	Assert(!pChild1 || pChild0);	// pChild1 implies pChild0

	AstNode * aPChildren[2] = { pChild0, pChild1 };
	int cPChildren = (!pChild0) ? 0 : (!pChild1) ? 1 : 2;
	return astNewErr(pParser, astkErr, aPChildren, cPChildren);
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, AstNode * aPChildren[], uint cPChildren)
{
    Assert(category(astkErr) == ASTCATK_Error);
    Assert(astkErr != ASTK_BubbleErr || cPChildren > 0);        // Implies

	auto * pNode = Down(astNew(pParser, astkErr), Err);
    init(&pNode->aPChildren);
	appendMultiple(&pNode->aPChildren, aPChildren, cPChildren);

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

        Assert(!skipAfterArrow || printArrows); // Skip after arrow implies print arrows
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

    auto printErrChildren = [printTabs](const AstErr & node, int level, DynamicArray<bool> * pMapLevelSkip)
    {
        for (uint i = 0; i < node.aPChildren.cItem; i++)
        {
            bool lastChild = i == node.aPChildren.cItem - 1;

            debugPrintSubAst(*node.aPChildren[i], level + 1, lastChild, pMapLevelSkip);
            if (!lastChild)
            {
                printf("\n");
                printTabs(level + 1, false, false, pMapLevelSkip);
            }
        }
    };

	static const char * parseErrorString = "[parse error]:";

    setSkip(pMapLevelSkip, level, false);
	printf("\n");
	printTabs(level, true, skipAfterArrow, pMapLevelSkip);

	int levelNext = level + 1;

    switch (node.astk)
    {
		// ERR

		case ASTK_BubbleErr:
		{
			printf("%s (bubble)", parseErrorString);
            printErrChildren(*DownConst(&node, Err), level, pMapLevelSkip);
		} break;

		case ASTK_UnexpectedTokenkErr:
		{
			auto * pExpr = DownConst(&node, UnexpectedTokenkErr);
			printf("%s unexpected %s", parseErrorString, g_mpTokenkDisplay[pExpr->tokenk]);
            printErrChildren(*UpErrConst(&node), level, pMapLevelSkip);
		} break;

		case ASTK_ExpectedTokenkErr:
		{
			auto * pExpr = DownConst(&node, ExpectedTokenkErr);
			printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pExpr->tokenk]);
            printErrChildren(*UpErrConst(&node), level, pMapLevelSkip);
		} break;



		// EXPR

        case ASTK_BinopExpr:
        {
            auto * pExpr = DownConst(&node, BinopExpr);
            printf("%s ", pExpr->pOp->lexeme);
            debugPrintSubAst(*pExpr->pLhsExpr, levelNext, false, pMapLevelSkip);
			printf("\n");
			printTabs(levelNext, false, false, pMapLevelSkip);
            debugPrintSubAst(*pExpr->pRhsExpr, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_GroupExpr:
        {
            printf("()");
            auto * pExpr = DownConst(&node, GroupExpr);
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
			printf("(func):");

            bool noArgs = pExpr->aPArgs.cItem == 0;

            debugPrintSubAst(*pExpr->pFunc, levelNext, noArgs, pMapLevelSkip);

			for (uint i = 0; i < pExpr->aPArgs.cItem; i++)
			{
                bool isLastArg = i == pExpr->aPArgs.cItem - 1;

				printf("\n");
				printTabs(
                    levelNext,
                    false,
                    false,
                    pMapLevelSkip
                );

				printf("(arg %d):", i);

				debugPrintSubAst(
                    *pExpr->aPArgs[i],
                    levelNext,
                    isLastArg,
                    pMapLevelSkip
                );
			}
		} break;



		// STMT;

        default:
        {
            // Not implemented!

            Assert(false);
        }
    }
}

#endif
