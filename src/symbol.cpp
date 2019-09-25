#include "symbol.h"

#include "ast.h"
#include "token.h"

const scopeid gc_unresolvedScopeid = -1;
const scopeid gc_globalAndBuiltinScopeid = 0;

void setSymbolInfo(SymbolInfo * pSymbInfo, const ResolvedIdentifier & ident, SYMBOLK symbolk, AstNode * pNode)
{
	pSymbInfo->symbolk = symbolk;
    pSymbInfo->identDefncl = ident;

	switch (symbolk)
	{
		case SYMBOLK_Var:
		{
			Assert(pNode->astk == ASTK_VarDeclStmt);
			pSymbInfo->varDecl = (AstVarDeclStmt *) pNode;
		} break;

		case SYMBOLK_Struct:
		{
			Assert(pNode->astk == ASTK_StructDefnStmt);
			pSymbInfo->structDefn = (AstStructDefnStmt *)pNode;
		} break;

		case SYMBOLK_Func:
		{
			Assert(pNode->astk == ASTK_FuncDefnStmt);
			pSymbInfo->funcDefn = (AstFuncDefnStmt *)pNode;
		} break;

		default:
			Assert(false);
	}
}

void setIdentResolved(ResolvedIdentifier * pIdentifier, Token * pToken, scopeid declScopeid)
{
	pIdentifier->pToken = pToken;
	pIdentifier->declScopeid = declScopeid;
	pIdentifier->hash = identHash(*pIdentifier);
}

void setIdentUnresolved(ResolvedIdentifier * pIdentifier, Token * pToken)
{
	pIdentifier->pToken = pToken;
	pIdentifier->declScopeid = gc_unresolvedScopeid;
}

u32 identHash(const ResolvedIdentifier & ident)
{
	// FNV-1a : http://www.isthe.com/chongo/tech/comp/fnv/

	const static u32 s_offsetBasis = 2166136261;
	const static u32 s_fnvPrime = 16777619;

	u32 result = s_offsetBasis;

	// Hash the scope identifier

	for (uint i = 0; i < sizeof(ident.declScopeid); i++)
	{
		u8 byte = *(reinterpret_cast<const u8*>(&ident.declScopeid) + i);
		result ^= byte;
		result *= s_fnvPrime;
	}

	// Hash the lexeme

	char * pChar = ident.pToken->lexeme;
	AssertInfo(pChar, "Identifier shouldn't be an empty string...");

	while (*pChar)
	{
		result ^= *pChar;
		result *= s_fnvPrime;
		pChar++;
	}

	return result;
}

u32 identHashPrecomputed(const ResolvedIdentifier & i)
{
	return i.hash;
}

bool identEq(const ResolvedIdentifier & i0, const ResolvedIdentifier & i1)
{
	if (!i0.pToken || !i1.pToken)			return false;

	if (i0.hash != i1.hash)					return false;
	if (i0.declScopeid != i1.declScopeid)	return false;

	if (i0.pToken->lexeme == i1.pToken->lexeme)		return true;

	return (strcmp(i0.pToken->lexeme, i1.pToken->lexeme) == 0);
}