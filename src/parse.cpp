#include "parse.h"
#include "scan.h"

// The downside to using a union instead of inheritance is that I can't implicitly upcast. Since there is quite
//	a bit of pointer casting required, these helpers make it slightly more succinct

#define Up(pNode) reinterpret_cast<AstNode *>(pNode)
#define AstNew(pParser, astk) reinterpret_cast<Ast##astk *>(astNew(pParser, astk))

void init(Parser * pParser, Scanner * pScanner)
{
	pParser->pScanner = pScanner;
	init(&pParser->astAlloc);
	init(&pParser->tokenAlloc);
}

AstNode * parse(Parser * pParser)
{
	return nullptr;
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
		return lhsExpr;
	}
}

AstNode * parseExpr(Parser * pParser)
{
	return parseOrExpr(pParser);
}

AstNode * parseOrExpr(Parser * pParser)
{
	AstNode * pExpr = parseAndExpr(pParser);
	if (isErrorNode(pExpr)) return pExpr;

	while (tryConsumeToken(pParser->pScanner, TOKEN_PipePipe, ensurePendingToken(pParser)))
	{
		AstNode * pRhsExpr = parseExpr(pParser);
		if (isErrorNode(pRhsExpr))
		{
			auto * pError = AstNew(pParser, Error);
			pError->pChildren[0] = pExpr;
			pError->pChildren[1] = pRhsExpr;
			return Up(pError);
		}

		auto pOrExpr = AstNew(pParser, BinopExpr);
		pNode->pOp = claimPendingToken(pParser);
		pNode->pLhsExpr = pExrl;
		pNode->pRhsExpr = pRhsExpr;

		pExpr = pOrExpr;
	}

	return pExpr;
}

AstNode * astNew(Parser * pParser, ASTK astk)
{
	AstNode * pNode = allocate(pParser->astAlloc);
	pNode->astk = astk;
	pNode->id = pParser->iNode;
	pParser->iNode++;

	pParser->apAstNodes.push_back(pNode);

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
