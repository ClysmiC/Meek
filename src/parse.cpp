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

#define AstNewErrListChild(pParser, astkErr, line, aPChildren, cPChildren) reinterpret_cast<Ast##astkErr *>(astNewErr(pParser, ASTK_##astkErr, line, aPChildren, cPChildren))

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
	// pParser->isPanicMode = false;
	init(&pParser->astAlloc);
	init(&pParser->tokenAlloc);
	init(&pParser->astNodes);

	return true;
}

AstNode * parseProgram(Parser * pParser)
{
	DynamicArray<AstNode *> aPNodesTemp;
	init(&aPNodesTemp);
	Defer(Assert(aPNodesTemp.pBuffer == nullptr));

	parseStmts(pParser, &aPNodesTemp);

	// NOTE: Empty program is valid to parse, but I may still decide that
	//	that is a semantic error.

	if (pParser->hadError)
	{
		// @Slow since AstNewErrListChild copies when we could probably rejigger the interface to support a "move"

		auto * pNode = AstNewErrListChild(pParser, ProgramErr, aPNodesTemp[0]->startLine, aPNodesTemp.pBuffer, aPNodesTemp.cItem);
		destroy(&aPNodesTemp);
		return Up(pNode);
	}
	else
	{
		Assert(aPNodesTemp.cItem == 0 || !isErrorNode(*aPNodesTemp[0]));

		auto * pNode = AstNew(pParser, Program, 0);
		initMove(&pNode->aPNodes, &aPNodesTemp);

		return Up(pNode);
	}
}

void parseStmts(Parser * pParser, DynamicArray<AstNode *> * poAPNodes)
{
	Token throwaway;
	while (!isFinished(pParser->pScanner) && peekToken(pParser->pScanner, &throwaway) != TOKENK_Eof)
	{

		AstNode * pNode = parseStmt(pParser);

		if (isErrorNode(*pNode))
		{
			// NOTE: should move this recoverFromPanic to the parseXXXStmt functions
			//	themselves. Otherwise errors will always bubble up to here which is
			//	inconsistent with how they behave in expression contexts.

			// pParser->isPanicMode = true;
			bool recovered = tryRecoverFromPanic(pParser, TOKENK_Semicolon);

			Assert(recovered || isFinished(pParser->pScanner));		// Implies
		}

		append(poAPNodes, pNode);
	}
}

AstNode * parseStmt(Parser * pParser)
{
	// TODO: Be careful deciding how to identify the kind of statement!!!
	//	How to disambiguate between AssignStmt and ExprStmt? I want assignment
	//	to have statement semantics, but it parses an awful lot like a binop
	//	expression!

	// TODO: Support other kinds of stmt once we can identify what stmt it is....

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
		Token * pErrToken = peekPendingToken(pParser);
		auto * pErrNode = AstNewErr1Child(pParser, ExpectedTokenkErr, pErrToken->line, pExpr);
		pErrNode->tokenk = TOKENK_Semicolon;
	}

	return pExpr;
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
	else if (tryConsumeToken(pParser->pScanner, TOKENK_Error, ensurePendingToken(pParser)))
	{
		// Scan error

		Token * pErrToken = claimPendingToken(pParser);

		auto * pErr = AstNewErr0Child(pParser, ScanErr, pErrToken->line);
		pErr->pErrToken = pErrToken;
		return Up(pErr);
	}
	else
	{
		// Unexpected token error

		Token errTokenTemp;
		peekToken(pParser->pScanner, &errTokenTemp);

		auto * pErr = AstNewErr0Child(pParser, UnexpectedTokenkErr, errTokenTemp.line);
		pErr->tokenk = errTokenTemp.tokenk;

		AssertInfo(pErr->tokenk != TOKENK_Error, "This be a ScanErr!");

		return Up(pErr);
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
			Token * errToken = peekPendingToken(pParser);
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

			if (tryRecoverFromPanic(pParser, TOKENK_CloseBracket))
			{
				goto finishArrayAccess;
			}

			auto * pErr = AstNewErr2Child(pParser, BubbleErr, pExpr->startLine, pExpr, pSubscriptExpr);
			return Up(pErr);
		}
		else if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseBracket, ensurePendingToken(pParser)))
		{
			Token * pErrToken = peekPendingToken(pParser);
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

		DynamicArray<AstNode *> aPArgs;
		init(&aPArgs);

		Defer(Assert(!aPArgs.pBuffer));		// buffer should get "moved" into AST

		bool isFirstArg = true;

		// Error recovery variables

		bool skipCommaCheck = false;
		static const TOKENK s_aTokenkRecoverable[] = { TOKENK_CloseParen, TOKENK_Comma };

		while (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			if (!isFirstArg && !tryConsumeToken(pParser->pScanner, TOKENK_Comma, ensurePendingToken(pParser)))
			{
				int line = peekPendingToken(pParser)->line;

				TOKENK tokenkMatch;
				bool recovered = tryRecoverFromPanic(pParser, s_aTokenkRecoverable, ArrayLen(s_aTokenkRecoverable), &tokenkMatch);

				if (recovered)
				{
					auto * pErrNode = AstNewErr0Child(pParser, ExpectedTokenkErr, line);
					pErrNode->tokenk = TOKENK_Comma;

					append(&aPArgs, Up(pErrNode));

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
					DynamicArray<AstNode *> aPChildren;
					initMove(&aPChildren, &aPArgs);
					prepend(&aPChildren, pExpr);

					auto * pErrNode = AstNewErrListChild(pParser, ExpectedTokenkErr, line, aPChildren.pBuffer, aPChildren.cItem);
					pErrNode->tokenk = TOKENK_Comma;

					return Up(pErrNode);
				}
			}

			skipCommaCheck = false;

			// NOTE: We append the expr to args even if it is an error.

			AstNode * pExprArg = parseExpr(pParser);
			append(&aPArgs, pExprArg);

			if (isErrorNode(*pExprArg))
			{
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

					DynamicArray<AstNode *> aPChildren;
					initMove(&aPChildren, &aPArgs);
					prepend(&aPChildren, pExpr);

					auto * pErrNode = AstNewErrListChild(pParser, BubbleErr, pExprArg->startLine, aPChildren.pBuffer, aPChildren.cItem);
					return Up(pErrNode);
				}
			}

			isFirstArg = false;
		}

	finishFuncCall:
		auto * pNode = AstNew(pParser, FuncCallExpr, pExpr->startLine);
		pNode->pFunc = pExpr;
		initMove(&pNode->aPArgs, &aPArgs);

		return finishParsePrimary(pParser, Up(pNode));
	}
	else
	{
		return pExpr;
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
	Assert(!pChild1 || pChild0);	// Implies

	AstNode * aPChildren[2] = { pChild0, pChild1 };
	int cPChildren = (!pChild0) ? 0 : (!pChild1) ? 1 : 2;
	return astNewErr(pParser, astkErr, line, aPChildren, cPChildren);
}

AstNode * astNewErr(Parser * pParser, ASTK astkErr, int line, AstNode * aPChildren[], uint cPChildren)
{
	Assert(category(astkErr) == ASTCATK_Error);
	Assert(astkErr != ASTK_BubbleErr || cPChildren > 0);		// Implies
	AssertInfo(
		astkErr != ASTK_BubbleErr || cPChildren >= 2,			// Implies
		"Bubble error by definition must have 2+ children"
	);

	bool isBubble = astkErr == ASTK_BubbleErr;
	bool hasErrorChild = false;

	auto * pNode = Down(astNew(pParser, astkErr, line), Err);
	init(&pNode->aPChildren);
	appendMultiple(&pNode->aPChildren, aPChildren, cPChildren);

#if DEBUG
	if (isBubble)
	{
		for (uint i = 0; i < pNode->aPChildren.cItem; i++)
		{
			if (isErrorNode(*(pNode->aPChildren[i])))
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

bool tryRecoverFromPanic(Parser * pParser, TOKENK tokenkRecover)
{
	TOKENK throwaway;
	return tryRecoverFromPanic(pParser, &tokenkRecover, 1, &throwaway);
}

bool tryRecoverFromPanic(Parser * pParser, const TOKENK * aTokenkRecover, int cTokenkRecover, TOKENK * poTokenkMatch)
{
	pParser->hadError = true;
	*poTokenkMatch = TOKENK_Nil;

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
				*poTokenkMatch = aTokenkRecover[i];
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

Token * peekPendingToken(Parser * pParser)
{
	Assert(pParser->pPendingToken);
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

		Assert(!skipAfterArrow || printArrows);		// Implies
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

	auto printChildren = [printTabs](const DynamicArray<AstNode *> & aPChildren, int level, const char * label, bool setSkipOnLastChild, DynamicArray<bool> * pMapLevelSkip)
	{
		for (uint i = 0; i < aPChildren.cItem; i++)
		{
			bool isLastChild = i == aPChildren.cItem - 1;
			bool shouldSetSkip = setSkipOnLastChild && isLastChild;

			printf("\n");
			printTabs(
				level,
				false,
				false,
				pMapLevelSkip
			);

			printf("(%s %d):", label, i);

			debugPrintSubAst(
				*aPChildren[i],
				level,
				shouldSetSkip,
				pMapLevelSkip
			);
		}
	};

	auto printErrChildren = [printChildren](const AstErr & node, int level, DynamicArray<bool> * pMapLevelSkip)
	{
		printChildren(node.aPChildren, level, "child", true, pMapLevelSkip);
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
			printf("%s (bubble)", parseErrorString);
			printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
		} break;

		case ASTK_UnexpectedTokenkErr:
		{
			auto * pErr = DownConst(&node, UnexpectedTokenkErr);
			printf("%s unexpected %s", parseErrorString, g_mpTokenkDisplay[pErr->tokenk]);
			printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
		} break;

		case ASTK_ExpectedTokenkErr:
		{
			auto * pErr = DownConst(&node, ExpectedTokenkErr);
			printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pErr->tokenk]);
			printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
		} break;

		case ASTK_ProgramErr:
		{
			auto * pErr = DownConst(&node, ProgramErr);
			printf("(erroneous program)");
			printErrChildren(*UpErrConst(pErr), levelNext, pMapLevelSkip);
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

			printChildren(pExpr->aPArgs, levelNext, "arg", true, pMapLevelSkip);
			// for (uint i = 0; i < pExpr->aPArgs.cItem; i++)
			// {
			//	   bool isLastArg = i == pExpr->aPArgs.cItem - 1;

			//	printf("\n");
			//	printTabs(
			//		   levelNext,
			//		   false,
			//		   false,
			//		   pMapLevelSkip
			//	   );

			//	printf("(arg %d):", i);

			//	debugPrintSubAst(
			//		   *pExpr->aPArgs[i],
			//		   levelNext,
			//		   isLastArg,
			//		   pMapLevelSkip
			//	   );
			// }
		} break;



		// STMT;

		case ASTK_ExprStmt:
		{
			auto * pStmt = DownConst(&node, ExprStmt);

			printf("(expr stmt)");
			printf("\n");

			debugPrintSubAst(*pStmt->pExpr, levelNext, true, pMapLevelSkip);
		} break;



		// PROGRAM

		case ASTK_Program:
		{
			auto * pNode = DownConst(&node, Program);
			printf("(program)");
			printf("\n");

			printChildren(pNode->aPNodes, levelNext, "stmt", true, pMapLevelSkip);
		} break;

		default:
		{
			// Not implemented!

			Assert(false);
		}
	}
}

#endif
