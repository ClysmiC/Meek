#include "parse.h"
#include "scan.h"


// The downside to using a union instead of inheritance is that I can't implicitly upcast. Since there is quite
//	a bit of pointer casting required, these helpers make it slightly more succinct

#define Up(pNode) reinterpret_cast<AstNode *>(pNode)
#define AstNew(pParser, astk) reinterpret_cast<Ast##astk *>(astNew(pParser, ASTK_##astk))

// TODO: should assignment statement be in here?
// What about . operator?

static ParseOp s_aParseOp[] = {
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
	return parseOp(pParser, s_aParseOp[s_iParseOpMax]);
}

AstNode * parseOp(Parser * pParser, const ParseOp & op)
{
	if (op.precedence == 0)
	{
        return parsePrimary(pParser);
	}
	else
	{
		ParseOp & opNext = s_aParseOp[op.precedence - 1];
		AstNode * pExpr = parseOp(pParser, opNext);
		if (isErrorNode(pExpr)) return pExpr;

		while (tryConsumeToken(pParser->pScanner, op.aTokenkMatch, op.cTokenMatch, ensurePendingToken(pParser)))
		{
			AstNode * pRhsExpr = parseOp(pParser, opNext);
			if (isErrorNode(pRhsExpr))
			{
				auto * pError = AstNew(pParser, Error);
				pError->pChildren[0] = pExpr;
				pError->pChildren[1] = pRhsExpr;
				return Up(pError);
			}

			auto pNode = AstNew(pParser, BinopExpr);
			pNode->pOp = claimPendingToken(pParser);
			pNode->pLhsExpr = pExpr;
			pNode->pRhsExpr = pRhsExpr;

			pExpr = Up(pNode);
		}

		return pExpr;
	}
}

AstNode * parsePrimary(Parser * pParser)
{
	if (tryConsumeToken(pParser->pScanner, TOKENK_OpenParen, ensurePendingToken(pParser)))
	{
		// Group ( )

		AstNode * pExpr = parseExpr(pParser);

		if (isErrorNode(pExpr))
		{
			auto * pError = AstNew(pParser, Error);
			pError->pChildren[0] = pExpr;
			return Up(pError);
		}

		if (!tryConsumeToken(pParser->pScanner, TOKENK_CloseParen, ensurePendingToken(pParser)))
		{
			auto * pError = AstNew(pParser, Error);
			pError->asterrk = ASTERRK_ExpectedCloseParen;
			pError->pChildren[0] = pExpr;
			return Up(pError);
		}

		auto * pNode = AstNew(pParser, GroupExpr);
		pNode->pExpr = pExpr;
		return Up(pNode);
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

		// TODO: also handle chaining dots, functions (), array access []...

        return nullptr;
	}
	else
	{
		// Error

		if (tryConsumeToken(pParser->pScanner, TOKENK_Eof, ensurePendingToken(pParser)))
		{
			auto * pError = AstNew(pParser, Error);
			pError->asterrk = ASTERRK_UnexpectedEof;
			return Up(pError);
		}
		else
		{
			auto * pError = AstNew(pParser, Error);
			pError->asterrk = ASTERRK_ExpectedExpr;
			return Up(pError);
		}
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
