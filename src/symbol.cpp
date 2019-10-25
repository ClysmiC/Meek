#include "symbol.h"

#include "ast.h"
#include "literal.h"
#include "token.h"
#include "type.h"

const symbseqid gc_symbseqidUnset = 0;

//void init(ScopeStack * pScopeStack)
//{
//    init(&pScopeStack->stack);
//    pScopeStack->scopeidNext = SCOPEID_BuiltIn;
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

    init(&pSymbTable->funcSymbolsPendingResolution);

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

    for (auto it = iter(pSymbTable->funcTable); it.pValue; iterNext(&it))
    {
        dispose(it.pValue);
    }

	dispose(&pSymbTable->funcTable);

    dispose(&pSymbTable->funcSymbolsPendingResolution);

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
        setIdent(&intIdent, &intToken, SCOPEID_BuiltIn);

        SymbolInfo intInfo;
        setBuiltinTypeSymbolInfo(&intInfo, intIdent, TYPID_Int);

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
        setIdent(&floatIdent, &floatToken, SCOPEID_BuiltIn);

        SymbolInfo floatInfo;
        setBuiltinTypeSymbolInfo(&floatInfo, floatIdent, TYPID_Float);

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
        setIdent(&boolIdent, &boolToken, SCOPEID_BuiltIn);

        SymbolInfo boolInfo;
        setBuiltinTypeSymbolInfo(&boolInfo, boolIdent, TYPID_Bool);

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

		if (lookup(*pTable, ident))
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
        AstFuncDefnStmt * pFuncDefnStmt = symbInfo.pFuncDefnStmt;

        // NOTE: If func parameter types are not yet resolved, we can't check for overload duplication,
		//	so we add it to a list to be resolved after all type params get resolved.

        if (!areVarDeclListTypesFullyResolved(pFuncDefnStmt->apParamVarDecls))
        {
			append(&pSymbolTable->funcSymbolsPendingResolution, symbInfo);
            return false;
        }

        // Get array of entries or create new one

		DynamicArray<SymbolInfo> * pEntries = lookup(pSymbolTable->funcTable, ident);
		if (!pEntries)
		{
			pEntries = insertNew(&pSymbolTable->funcTable, ident);
			init(pEntries);
		}

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

bool tryResolvePendingFuncSymbolsAfterTypesResolved(SymbolTable * pSymbTable)
{
	for (int i = pSymbTable->funcSymbolsPendingResolution.cItem - 1; i >= 0; i--)
	{
		const SymbolInfo * pSymbInfo = &pSymbTable->funcSymbolsPendingResolution[i];
		if (tryInsert(pSymbTable, pSymbInfo->ident, *pSymbInfo))
		{
			unorderedRemove(&pSymbTable->funcSymbolsPendingResolution, i);
		}
	}

	return (pSymbTable->funcSymbolsPendingResolution.cItem == 0);
}

SymbolInfo * lookupVarSymb(const SymbolTable & symbTable, const ScopedIdentifier & ident)
{
    return lookup(symbTable.varTable, ident);
}

SymbolInfo * lookupTypeSymb(const SymbolTable & symbTable, const ScopedIdentifier & ident)
{
    return lookup(symbTable.typeTable, ident);
}

DynamicArray<SymbolInfo> * lookupFuncSymb(const SymbolTable & symbTable, const ScopedIdentifier & ident)
{
    return lookup(symbTable.funcTable, ident);
}

void setSymbolInfo(SymbolInfo * pSymbInfo, const ScopedIdentifier & ident, SYMBOLK symbolk, AstNode * pNode)
{
    AssertInfo(symbolk != SYMBOLK_BuiltInType, "Call setBuiltinTypeSymbolInfo for builtin types!");
    Assert(ident.defnclScopeid != SCOPEID_BuiltIn);

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
        {
			Assert(false);
        } break;
	}
}

void setBuiltinTypeSymbolInfo(SymbolInfo * pSymbInfo, const ScopedIdentifier & ident, TYPID typid)
{
    AssertInfo(isTypeResolved(typid), "Put built in types in the type table before the symbol table so that you can provide their typid");

    pSymbInfo->symbolk = SYMBOLK_BuiltInType;
    pSymbInfo->ident = ident;
    pSymbInfo->typid = typid;
}

void setIdent(ScopedIdentifier * pIdentifier, Token * pToken, SCOPEID declScopeid)
{
    Assert(Implies(declScopeid == SCOPEID_BuiltIn, pToken));

    if (pToken)
    {
        Assert(Iff(declScopeid == SCOPEID_BuiltIn, pToken->id == -1));
        Assert(Iff(declScopeid == SCOPEID_BuiltIn, pToken->line == -1));
        Assert(Iff(declScopeid == SCOPEID_BuiltIn, pToken->column == -1));
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
	pIdentifier->defnclScopeid = SCOPEID_Unresolved;
}

void resolveIdentScope(ScopedIdentifier * pIdentifier, SCOPEID declScopeid)
{
	Assert(pIdentifier->pToken);
	Assert(pIdentifier->defnclScopeid == SCOPEID_Unresolved);
	Assert(pIdentifier->hash == 0);

	pIdentifier->defnclScopeid = declScopeid;
	pIdentifier->hash = scopedIdentHash(*pIdentifier);
}

u32 scopedIdentHash(const ScopedIdentifier & ident)
{
	auto hash = startHash(&ident.defnclScopeid, sizeof(ident.defnclScopeid));
	hash = buildHashCString(ident.pToken->lexeme, hash);

	return hash;
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
		printf("typid: %u\n", pSymbInfo->pVarDeclStmt->typid);
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
			    printf("param %d typeid: %d\n", j, pVarDecl->typid);
		    }

		    for (int j = 0; j < pFuncDefnStmt->apReturnVarDecls.cItem; j++)
		    {
                auto * pNode = pFuncDefnStmt->apReturnVarDecls[j];
                Assert(pNode->astk == ASTK_VarDeclStmt);
                auto * pVarDecl = Down(pNode, VarDeclStmt);

                if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
			    printf("return %d typid: %d\n", j, pVarDecl->typid);
		    }

            if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
            printf("scopeid defn: %d\n", pIdent->defnclScopeid);

            if (paSymbInfo->cItem > 1) printf("\t\t"); else printf("\t");
            printf("scopeid body: %d\n", pFuncDefnStmt->scopeid);
        }

        printf("\n");
	}

    printf("\n===Types===\n\n");

    for (auto it = iter(symbTable.typeTable); it.pValue; iterNext(&it))
    {
        const ScopedIdentifier * pIdent = it.pKey;
        SymbolInfo * pSymbInfo = it.pValue;

        Assert(pSymbInfo->symbolk == SYMBOLK_Struct || pSymbInfo->symbolk == SYMBOLK_BuiltInType);

        if (pSymbInfo->symbolk == SYMBOLK_Struct)
        {
            printf("name: %s\n", pIdent->pToken->lexeme);
            printf("typid: %u\n", pSymbInfo->pStructDefnStmt->typidSelf);
            printf("scopeid: %d\n", pIdent->defnclScopeid);
        }
        else
        {
            Assert(pSymbInfo->symbolk == SYMBOLK_BuiltInType);

            printf("name: %s\n", pIdent->pToken->lexeme);
            printf("typid: %u\n", pSymbInfo->typid);
            printf("scopeid: %d\n", pIdent->defnclScopeid);
        }

        printf("\n");
    }
}
#endif
