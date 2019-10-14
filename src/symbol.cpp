#include "symbol.h"

#include "ast.h"
#include "token.h"
#include "type.h"

const scopeid gc_unresolvedScopeid = 0;
const scopeid gc_builtInScopeid = 1;
const scopeid gc_globalScopeid = 2;

const symbseqid gc_unsetSymbseqid = 0;

//void init(ScopeStack * pScopeStack)
//{
//    init(&pScopeStack->stack);
//    pScopeStack->scopeidNext = gc_builtInScopeid;
//}
//
//void pushScope(ScopeStack * pScopeStack, SCOPEK scopek)
//{
//    Scope s;
//    s.id = pScopeStack->scopeidNext;
//    s.scopek = scopek;
//    push(&pScopeStack->stack, s);
//    pScopeStack->scopeidNext++;
//}
//
//Scope peekScope(ScopeStack * pScopeStack)
//{
//    Scope s;
//    peek(&pScopeStack->stack, &s);
//
//    return s;
//}
//
//Scope popScope(ScopeStack * pScopeStack)
//{
//    Scope s;
//    pop(&pScopeStack->stack, &s);
//
//    return s;
//}

void init(SymbolTable * pSymbTable)
{
	init(&pSymbTable->varTable, scopedIdentHashPrecomputed, scopedIdentEq);
	init(&pSymbTable->funcTable, scopedIdentHashPrecomputed, scopedIdentEq);
	init(&pSymbTable->structTable, scopedIdentHashPrecomputed, scopedIdentEq);

	init(&pSymbTable->redefinedVars);
    init(&pSymbTable->redefinedFuncs);
    init(&pSymbTable->redefinedStructs);
    pSymbTable->sequenceIdNext = 0;
}

void dispose(SymbolTable * pSymbTable)
{
	dispose(&pSymbTable->varTable);
	dispose(&pSymbTable->structTable);

	// Func table entries all own memory on the heap.

	void (*disposeFn)(DynamicArray<SymbolInfo> *) = &dispose;
	doForEachValue(&pSymbTable->funcTable, disposeFn);

	dispose(&pSymbTable->funcTable);

    dispose(&pSymbTable->redefinedVars);
    dispose(&pSymbTable->redefinedFuncs);
    dispose(&pSymbTable->redefinedStructs);
}

bool tryInsert(
	SymbolTable * pSymbolTable,
	const ScopedIdentifier & ident,
	const SymbolInfo & symbInfo)
{
	if (!ident.pToken) return false;

    Assert(symbInfo.ident.hash == ident.hash);

	if (symbInfo.symbolk == SYMBOLK_Var || symbInfo.symbolk == SYMBOLK_Struct)
	{
		auto * pTable = (symbInfo.symbolk == SYMBOLK_Var) ? &pSymbolTable->varTable : &pSymbolTable->structTable;
		auto * pRedefinedArray = (symbInfo.symbolk == SYMBOLK_Var) ? &pSymbolTable->redefinedVars : &pSymbolTable->redefinedStructs;

        // Check for duplicate

		if (lookup(pTable, ident))
		{
			append(pRedefinedArray, symbInfo);
			return false;
		}

        // Add sequence id to AST node

        if (symbInfo.symbolk == SYMBOLK_Var)
        {
            symbInfo.pVarDeclStmt->symbseqid = pSymbolTable->sequenceIdNext;
        }
        else
        {
            Assert(symbInfo.symbolk == SYMBOLK_Struct);
            symbInfo.pStructDefnStmt->symbseqid = pSymbolTable->sequenceIdNext;
        }

        // Insert into table

		insert(pTable, ident, symbInfo);
		return true;
	}
	else
	{
		Assert(symbInfo.symbolk == SYMBOLK_Func);

        // Get array of entries or create new one

		DynamicArray<SymbolInfo> * pEntries = lookup(&pSymbolTable->funcTable, ident);
		if (!pEntries)
		{
			pEntries = insertNew(&pSymbolTable->funcTable, ident);
			init(pEntries);
		}

		FuncType * pFuncType = symbInfo.pFuncDefnStmt->pFuncType;
		Assert(pFuncType);

        // Check for duplicate w/ same parameter signature

		for (uint i = 0; i < pEntries->cItem; i++)
		{
			SymbolInfo * pSymbInfoCandidate = &(*pEntries)[i];
			Assert(pSymbInfoCandidate->symbolk == SYMBOLK_Func);

			FuncType * pFuncTypeOther = pSymbInfoCandidate->pFuncDefnStmt->pFuncType;
			Assert(pFuncTypeOther);

			if (funcTypeEq(*pFuncType, *pFuncTypeOther))
			{
				// Duplicate

				append(&pSymbolTable->redefinedFuncs, symbInfo);
				return false;
			}
		}

        // Add sequence id to AST node

        symbInfo.pFuncDefnStmt->symbseqid = pSymbolTable->sequenceIdNext;
        pSymbolTable->sequenceIdNext++;

        // Insert into table

		append(pEntries, symbInfo);
		return true;
	}
}

SymbolInfo * lookupVar(SymbolTable * pSymbTable, const ScopedIdentifier & ident)
{
    return lookup(&pSymbTable->varTable, ident);
}

SymbolInfo * lookupStruct(SymbolTable * pSymbTable, const ScopedIdentifier & ident)
{
    return lookup(&pSymbTable->structTable, ident);
}

DynamicArray<SymbolInfo> * lookupFunc(SymbolTable * pSymbTable, const ScopedIdentifier & ident)
{
    return lookup(&pSymbTable->funcTable, ident);
}

void setSymbolInfo(SymbolInfo * pSymbInfo, const ScopedIdentifier & ident, SYMBOLK symbolk, AstNode * pNode)
{
	pSymbInfo->symbolk = symbolk;
    pSymbInfo->ident = ident;

	switch (symbolk)
	{
		case SYMBOLK_Var:
		{
			Assert(pNode->astk == ASTK_VarDeclStmt);
			pSymbInfo->pVarDeclStmt = Down(pNode, VarDeclStmt);
		} break;

		case SYMBOLK_Struct:
		{
			Assert(pNode->astk == ASTK_StructDefnStmt);
			pSymbInfo->pStructDefnStmt = Down(pNode, StructDefnStmt);
		} break;

		case SYMBOLK_Func:
		{
			Assert(pNode->astk == ASTK_FuncDefnStmt);
			pSymbInfo->pFuncDefnStmt = Down(pNode, FuncDefnStmt);
		} break;

		default:
			Assert(false);
	}
}

void setIdent(ScopedIdentifier * pIdentifier, Token * pToken, scopeid declScopeid)
{
	pIdentifier->pToken = pToken;
	pIdentifier->defnclScopeid = declScopeid;
	pIdentifier->hash = scopedIdentHash(*pIdentifier);
}

void setIdentNoScope(ScopedIdentifier * pIdentifier, Token * pToken)
{
	pIdentifier->pToken = pToken;
	pIdentifier->defnclScopeid = gc_unresolvedScopeid;
}

void resolveIdentScope(ScopedIdentifier * pIdentifier, scopeid declScopeid)
{
	Assert(pIdentifier->pToken);
	pIdentifier->defnclScopeid = declScopeid;
	pIdentifier->hash = scopedIdentHash(*pIdentifier);
}

u32 scopedIdentHash(const ScopedIdentifier & ident)
{
	// FNV-1a : http://www.isthe.com/chongo/tech/comp/fnv/

	const static u32 s_offsetBasis = 2166136261;
	const static u32 s_fnvPrime = 16777619;

	u32 result = s_offsetBasis;

	// Hash the scope identifier

	for (uint i = 0; i < sizeof(ident.defnclScopeid); i++)
	{
		u8 byte = *(reinterpret_cast<const u8*>(&ident.defnclScopeid) + i);
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

u32 scopedIdentHashPrecomputed(const ScopedIdentifier & i)
{
	return i.hash;
}

bool scopedIdentEq(const ScopedIdentifier & i0, const ScopedIdentifier & i1)
{
	if (!i0.pToken || !i1.pToken)			    return false;

	if (i0.hash != i1.hash)					    return false;
	if (i0.defnclScopeid != i1.defnclScopeid)	return false;

	if (i0.pToken->lexeme == i1.pToken->lexeme)		return true;

	return (strcmp(i0.pToken->lexeme, i1.pToken->lexeme) == 0);
}

bool isDeclarationOrderIndependent(const SymbolInfo & info)
{
	return isDeclarationOrderIndependent(info.symbolk);
}

bool isDeclarationOrderIndependent(SYMBOLK symbolk)
{
	return
		symbolk == SYMBOLK_Var ||
		symbolk == SYMBOLK_Struct;
}
