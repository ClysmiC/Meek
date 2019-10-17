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
	init(&pSymbTable->typeTable, scopedIdentHashPrecomputed, scopedIdentEq);

	init(&pSymbTable->redefinedVars);
    init(&pSymbTable->redefinedFuncs);
    init(&pSymbTable->redefinedTypes);
    pSymbTable->sequenceIdNext = 0;
}

void dispose(SymbolTable * pSymbTable)
{
	dispose(&pSymbTable->varTable);
	dispose(&pSymbTable->typeTable);

	// Func table entries all own memory on the heap.

	void (*disposeFn)(DynamicArray<SymbolInfo> *) = &dispose;
	doForEachValue(&pSymbTable->funcTable, disposeFn);

	dispose(&pSymbTable->funcTable);

    dispose(&pSymbTable->redefinedVars);
    dispose(&pSymbTable->redefinedFuncs);
    dispose(&pSymbTable->redefinedTypes);
}

void insertBuiltInSymbols(SymbolTable * pSymbolTable)
{
    // int
    {
        static Token intToken;
        intToken.id = -1;
        intToken.line = -1;
        intToken.column = -1;
        intToken.tokenk = TOKENK_Identifier;
        intToken.lexeme = "int";

        ScopedIdentifier intIdent;
        setIdent(&intIdent, &intToken, gc_builtInScopeid);

        SymbolInfo intInfo;
        intInfo.ident = intIdent;
        intInfo.symbolk = SYMBOLK_BuiltInType;

        Verify(tryInsert(pSymbolTable, intIdent, intInfo));
    }

    // float
    {
        static Token floatToken;
        floatToken.id = -1;
        floatToken.line = -1;
        floatToken.column = -1;
        floatToken.tokenk = TOKENK_Identifier;
        floatToken.lexeme = "float";

        ScopedIdentifier floatIdent;
        setIdent(&floatIdent, &floatToken, gc_builtInScopeid);

        SymbolInfo floatInfo;
        floatInfo.ident = floatIdent;
        floatInfo.symbolk = SYMBOLK_BuiltInType;

        Verify(tryInsert(pSymbolTable, floatIdent, floatInfo));
    }

    // bool
    {
        static Token boolToken;
        boolToken.id = -1;
        boolToken.line = -1;
        boolToken.column = -1;
        boolToken.tokenk = TOKENK_Identifier;
        boolToken.lexeme = "bool";

        ScopedIdentifier boolIdent;
        setIdent(&boolIdent, &boolToken, gc_builtInScopeid);

        SymbolInfo boolInfo;
        boolInfo.ident = boolIdent;
        boolInfo.symbolk = SYMBOLK_BuiltInType;

        Verify(tryInsert(pSymbolTable, boolIdent, boolInfo));
    }
}

bool tryInsert(
	SymbolTable * pSymbolTable,
	const ScopedIdentifier & ident,
	const SymbolInfo & symbInfo)
{
	if (!ident.pToken) return false;

    Assert(symbInfo.ident.hash == ident.hash);

	if (symbInfo.symbolk == SYMBOLK_Var || symbInfo.symbolk == SYMBOLK_Struct || symbInfo.symbolk == SYMBOLK_BuiltInType)
	{
		auto * pTable = (symbInfo.symbolk == SYMBOLK_Var) ? &pSymbolTable->varTable : &pSymbolTable->typeTable;
		auto * pRedefinedArray = (symbInfo.symbolk == SYMBOLK_Var) ? &pSymbolTable->redefinedVars : &pSymbolTable->redefinedTypes;

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
        else if (symbInfo.symbolk == SYMBOLK_Struct)
        {
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

        AstFuncDefnStmt * pFuncDefnStmt = symbInfo.pFuncDefnStmt;

        // Check for duplicate w/ same parameter signature

		for (int i = 0; i < pEntries->cItem; i++)
		{
			SymbolInfo * pSymbInfoCandidate = &(*pEntries)[i];
			Assert(pSymbInfoCandidate->symbolk == SYMBOLK_Func);

			AstFuncDefnStmt * pFuncDefnStmtOther = pSymbInfoCandidate->pFuncDefnStmt;

			if (areVarDeclListTypesEq(pFuncDefnStmt->apParamVarDecls, pFuncDefnStmtOther->apParamVarDecls))
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
    return lookup(&pSymbTable->typeTable, ident);
}

DynamicArray<SymbolInfo> * lookupFunc(SymbolTable * pSymbTable, const ScopedIdentifier & ident)
{
    return lookup(&pSymbTable->funcTable, ident);
}

void setSymbolInfo(SymbolInfo * pSymbInfo, const ScopedIdentifier & ident, SYMBOLK symbolk, AstNode * pNode)
{
    Assert(Iff(!pNode, symbolk == SYMBOLK_BuiltInType));
    Assert(Iff(ident.defnclScopeid == gc_builtInScopeid, symbolk == SYMBOLK_BuiltInType));

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

        case SYMBOLK_BuiltInType:
        {
            // No extra info
        } break;

		default:
        {
			Assert(false);
        } break;
	}
}

void setIdent(ScopedIdentifier * pIdentifier, Token * pToken, scopeid declScopeid)
{
    Assert(Implies(declScopeid == gc_builtInScopeid, pToken));

    if (pToken)
    {
        Assert(Iff(declScopeid == gc_builtInScopeid, pToken->id == -1));
        Assert(Iff(declScopeid == gc_builtInScopeid, pToken->line == -1));
        Assert(Iff(declScopeid == gc_builtInScopeid, pToken->column == -1));
    }

	pIdentifier->pToken = pToken;
	pIdentifier->defnclScopeid = declScopeid;

    if (pIdentifier->pToken)
    {
	    pIdentifier->hash = scopedIdentHash(*pIdentifier);
    }
    else
    {
        pIdentifier->hash = 0;
    }
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

#if DEBUG

#include <stdio.h>

void debugPrintType(const Type & type)
{
	for (int i = 0; i < type.aTypemods.cItem; i++)
	{
		TypeModifier tmod = type.aTypemods[i];

		switch (tmod.typemodk)
		{
			case TYPEMODK_Array:
			{
				// TODO: Change this when arbitrary compile time expressions are allowed
				//	as the array size expression

				printf("[");

				Assert(tmod.pSubscriptExpr && tmod.pSubscriptExpr->astk == ASTK_LiteralExpr);

				auto * pNode = Down(tmod.pSubscriptExpr, LiteralExpr);
				Assert(pNode->literalk == LITERALK_Int);

				int value = intValue(pNode);
				Assert(pNode->isValueSet);
				AssertInfo(pNode->isValueErroneous, "Just for sake of testing... in reality if it was erroneous then we don't really care/bother about the symbol table");

				printf("%d", value);

				printf("]");
			} break;

			case TYPEMODK_Pointer:
			{
				printf("^");
			} break;
		}
	}

	if (!type.isFuncType)
	{
		printf("%s (scopeid: %d)", type.ident.pToken->lexeme, type.ident.defnclScopeid);
	}
	else
	{
		// TODO: :) ... will need to set up tab/new line stuff so that it
		//	can nest arbitrarily. Can't be bothered right now!!!

		printf("(function type... CBA to print)");
	}
}

void debugPrintSymbolTable(const SymbolTable & symbTable)
{
    printf("===Variables===\n\n");

    for (auto it = iter(symbTable.varTable); it.pValue; iterNext(&it))
    {
		const ScopedIdentifier * pIdent = it.pKey;
		SymbolInfo * pSymbInfo = it.pValue;

		Assert(pSymbInfo->symbolk == SYMBOLK_Var);

		printf("name: %s\n", pIdent->pToken->lexeme);
		printf("type: "); debugPrintType(*pSymbInfo->pVarDeclStmt->pType); printf("\n");
		printf("scopeid: %d\n", pIdent->defnclScopeid);
		printf("\n");
    }

	printf("\n===Functions===\n\n");

	for (auto it = iter(symbTable.funcTable); it.pValue; iterNext(&it))
	{
		const ScopedIdentifier * pIdent = it.pKey;
		DynamicArray<SymbolInfo> * paSymbInfo = it.pValue;

		printf("name: %s\n", pIdent->pToken->lexeme);

        for (int i = 0; i < paSymbInfo->cItem; i++)
        {
            SymbolInfo * pSymbInfo = &(*paSymbInfo)[i];
		    Assert(pSymbInfo->symbolk == SYMBOLK_Func);

            AstFuncDefnStmt * pFuncDefnStmt = Down(pSymbInfo->pFuncDefnStmt, FuncDefnStmt);

            if (paSymbInfo->cItem > 1)
            {
                printf("\t(overload %d)\n", i);
            }

		    for (int j = 0; j < pFuncDefnStmt->apParamVarDecls.cItem; j++)
		    {
                auto * pNode = pFuncDefnStmt->apParamVarDecls[j];
                Assert(pNode->astk == ASTK_VarDeclStmt);
                auto * pVarDecl = Down(pNode, VarDeclStmt);

                if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
			    printf("param %d type: ", j); debugPrintType(*pVarDecl->pType); printf("\n");
		    }

		    for (int j = 0; j < pFuncDefnStmt->apReturnVarDecls.cItem; j++)
		    {
                auto * pNode = pFuncDefnStmt->apReturnVarDecls[j];
                Assert(pNode->astk == ASTK_VarDeclStmt);
                auto * pVarDecl = Down(pNode, VarDeclStmt);

                if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
			    printf("return %d type: ", j); debugPrintType(*pVarDecl->pType); printf("\n");
		    }

            if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
            printf("scopeid defn: %d\n", pIdent->defnclScopeid);

            if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
            printf("scopeid body: %d\n", pFuncDefnStmt->scopeid);
        }

		printf("\n");
	}
}
#endif
