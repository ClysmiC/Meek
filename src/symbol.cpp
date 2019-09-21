#include "symbol.h"

#include "token.h"

const scopeid gc_unresolvedScopeid = -1;

void init(SymbolInfo * pSymbolInfo, SYMBOLK symbolk, Identifier * pIdent)
{
	pSymbolInfo->pIdent = pIdent;
	pSymbolInfo->symbolk = symbolk;

	switch (symbolk)
	{
		case SYMBOLK_Var:
		{
			pSymbolInfo->varInfo.pInfo = pSymbolInfo;
			// User should manually init VarInfo based on the VARK
		} break;

		case SYMBOLK_Func:
		{
			pSymbolInfo->funcInfo.pInfo = pSymbolInfo;
			init(&pSymbolInfo->funcInfo.aParamsIn);
			init(&pSymbolInfo->funcInfo.aParamsOut);
		} break;

		case SYMBOLK_Struct:
		{
			pSymbolInfo->structInfo.pInfo = pSymbolInfo;
			init(&pSymbolInfo->structInfo.fields, identHashPrecomputed, identEq);
		} break;
	}
}

void setIdent(Identifier * pIdentifier, Token * pToken, scopeid declScopeid)
{
	pIdentifier->pToken = pToken;
	pIdentifier->declScopeid = declScopeid;
	pIdentifier->hash = identHash(*pIdentifier);
}

void setIdentUnresolved(Identifier * pIdentifier, Token * pToken)
{
	pIdentifier->pToken = pToken;
	pIdentifier->declScopeid = gc_unresolvedScopeid;
}

u32 identHash(const Identifier & ident)
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

u32 identHashPrecomputed(const Identifier & i)
{
	return i.hash;
}

bool identEq(const Identifier & i0, const Identifier & i1)
{
	// TODO: Can probably get rid of this first check as long as we make sure that we
	//	are only putting identifiers w/ tokens in the symbol table (which should be
	//	the case...)

	if (!i0.pToken || !i1.pToken)			return false;

	if (i0.hash != i1.hash)					return false;
	if (i0.declScopeid != i1.declScopeid)	return false;

	if (i0.pToken->lexeme == i1.pToken->lexeme)		return true;

	return (strcmp(i0.pToken->lexeme, i1.pToken->lexeme) == 0);
}