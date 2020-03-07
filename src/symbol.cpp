#include "symbol.h"

#include "ast.h"
#include "literal.h"
#include "token.h"
#include "type.h"

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

void init(FuncSymbolPendingResolution * pPending, const SymbolInfo & symbolInfo, const Stack<Scope> & scopeStack)
{
    pPending->symbolInfo = symbolInfo;
    initCopy(&pPending->scopeStack, scopeStack);
}

void init(SymbolTable * pSymbTable)
{
	init(&pSymbTable->varTable, scopedIdentHashPrecomputed, scopedIdentEq);
	init(&pSymbTable->funcTable, scopedIdentHashPrecomputed, scopedIdentEq);
	init(&pSymbTable->typeTable, scopedIdentHashPrecomputed, scopedIdentEq);

    init(&pSymbTable->funcSymbolsPendingResolution);

	init(&pSymbTable->redefinedVars);
    init(&pSymbTable->redefinedFuncs);
    init(&pSymbTable->redefinedTypes);
    pSymbTable->symbseqidNext = SYMBSEQID_SetStart;
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
	Stack<Scope> scopeStack;
	init(&scopeStack);
	Defer(dispose(&scopeStack));
	Scope builtInScope;
	builtInScope.id = SCOPEID_BuiltIn;
	builtInScope.scopek = SCOPEK_BuiltIn;
	push(&scopeStack, builtInScope);

    // int
    {
        static Token intToken;
        intToken.id = -1;
        intToken.startEnd = gc_startEndBuiltInPseudoToken;
        intToken.tokenk = TOKENK_Identifier;
        intToken.lexeme = makeStringView("int");

        ScopedIdentifier intIdent;
        setIdent(&intIdent, &intToken, SCOPEID_BuiltIn);

        SymbolInfo intInfo;
        setBuiltinTypeSymbolInfo(&intInfo, intIdent, TYPID_Int);

        Verify(tryInsert(pSymbolTable, intIdent, intInfo, scopeStack));
    }

    // float
    {
        static Token floatToken;
        floatToken.id = -1;
        floatToken.startEnd = gc_startEndBuiltInPseudoToken;
        floatToken.tokenk = TOKENK_Identifier;
        floatToken.lexeme = makeStringView("float");

        ScopedIdentifier floatIdent;
        setIdent(&floatIdent, &floatToken, SCOPEID_BuiltIn);

        SymbolInfo floatInfo;
        setBuiltinTypeSymbolInfo(&floatInfo, floatIdent, TYPID_Float);

        Verify(tryInsert(pSymbolTable, floatIdent, floatInfo, scopeStack));
    }

    // bool
    {
        static Token boolToken;
        boolToken.id = -1;
        boolToken.startEnd = gc_startEndBuiltInPseudoToken;
        boolToken.tokenk = TOKENK_Identifier;
        boolToken.lexeme = makeStringView("bool");

        ScopedIdentifier boolIdent;
        setIdent(&boolIdent, &boolToken, SCOPEID_BuiltIn);

        SymbolInfo boolInfo;
        setBuiltinTypeSymbolInfo(&boolInfo, boolIdent, TYPID_Bool);

        Verify(tryInsert(pSymbolTable, boolIdent, boolInfo, scopeStack));
    }
}

bool tryInsert(
	SymbolTable * pSymbolTable,
	const ScopedIdentifier & ident,
	const SymbolInfo & symbInfo,
	const Stack<Scope> & scopeStack)
{
	if (!ident.pToken) return false;

	Assert(ident.defnclScopeid == peek(scopeStack).id);
    Assert(symbInfo.ident.hash == ident.hash);      // Ehh passing idents around is kind of redundant if it is already embedded in the symbol info

	if (symbInfo.symbolk == SYMBOLK_Var || symbInfo.symbolk == SYMBOLK_Struct || symbInfo.symbolk == SYMBOLK_BuiltInType)
	{
		auto * pTable = (symbInfo.symbolk == SYMBOLK_Var) ? &pSymbolTable->varTable : &pSymbolTable->typeTable;
		auto * pRedefinedArray = (symbInfo.symbolk == SYMBOLK_Var) ? &pSymbolTable->redefinedVars : &pSymbolTable->redefinedTypes;

        // Check for duplicate in own scope

		if (lookup(*pTable, ident))
		{
			append(pRedefinedArray, symbInfo);
			return false;
		}

		// Check for shadowing of struct or builtin (only vars can be shadowed)
		// NOTE: i = 1 because the top scope in the stack matches the ident's scope, which was just checked in the previous step

		if (symbInfo.symbolk == SYMBOLK_Struct || symbInfo.symbolk == SYMBOLK_BuiltInType)
		{
			for (int i = 1; i < count(scopeStack); i++)
			{
				ScopedIdentifier candidateIdent;
				setIdent(&candidateIdent, ident.pToken, peekFar(scopeStack, i).id);
				if (lookup(*pTable, ident))
				{
					append(pRedefinedArray, symbInfo);
					return false;
				}
			}
		}

        // Add sequence id to AST node

        if (symbInfo.symbolk == SYMBOLK_Var)
        {
            symbInfo.pVarDeclStmt->symbseqid = pSymbolTable->symbseqidNext;
        }
        else if (symbInfo.symbolk == SYMBOLK_Struct)
        {
            symbInfo.pStructDefnStmt->symbseqid = pSymbolTable->symbseqidNext;
        }

        pSymbolTable->symbseqidNext = static_cast<SYMBSEQID>(pSymbolTable->symbseqidNext + 1);

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
        
        if (!areVarDeclListTypesFullyResolved(pFuncDefnStmt->pParamsReturnsGrp->apParamVarDecls))
        {
            FuncSymbolPendingResolution * pPending = appendNew(&pSymbolTable->funcSymbolsPendingResolution);
            init(pPending, symbInfo, scopeStack);
            return false;
        }

        // Check for duplicate w/ same parameter signature

		{
			DynamicArray<SymbolInfo> aFuncsWithSameName;
			init(&aFuncsWithSameName);
			Defer(dispose(&aFuncsWithSameName));

			lookupFuncSymb(*pSymbolTable, symbInfo.ident.pToken, scopeStack, &aFuncsWithSameName);

			for (int i = 0; i < aFuncsWithSameName.cItem; i++)
			{
				SymbolInfo * pSymbInfoCandidate = &aFuncsWithSameName[i];
				Assert(pSymbInfoCandidate->symbolk == SYMBOLK_Func);

				AstFuncDefnStmt * pFuncDefnStmtOther = pSymbInfoCandidate->pFuncDefnStmt;

				if (areVarDeclListTypesEq(pFuncDefnStmt->pParamsReturnsGrp->apParamVarDecls, pFuncDefnStmtOther->pParamsReturnsGrp->apParamVarDecls))
				{
					// Duplicate

					append(&pSymbolTable->redefinedFuncs, symbInfo);
					return false;
				}
			}
		}

        // Get array of entries or create new one

		DynamicArray<SymbolInfo> * pEntries = lookup(pSymbolTable->funcTable, ident);
		if (!pEntries)
		{
			pEntries = insertNew(&pSymbolTable->funcTable, ident);
			init(pEntries);
		}

        // Add sequence id to AST node

		symbInfo.pFuncDefnStmt->symbseqid = pSymbolTable->symbseqidNext;
        pSymbolTable->symbseqidNext = static_cast<SYMBSEQID>(pSymbolTable->symbseqidNext + 1);

        // Insert into table

		append(pEntries, symbInfo);
		return true;
	}
}

bool tryResolvePendingFuncSymbolsAfterTypesResolved(SymbolTable * pSymbTable)
{
	for (int i = pSymbTable->funcSymbolsPendingResolution.cItem - 1; i >= 0; i--)
	{
		auto * pPending = &pSymbTable->funcSymbolsPendingResolution[i];
		if (tryInsert(pSymbTable, pPending->symbolInfo.ident, pPending->symbolInfo, pPending->scopeStack))
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

void lookupFuncSymb(const SymbolTable & symbTable, Token * pFuncToken, const Stack<Scope> scopeStack, DynamicArray<SymbolInfo> * poResults)
{
	for (int i = 0; i < count(scopeStack); i++)
	{
		ScopedIdentifier candidateIdent;
		setIdent(&candidateIdent, pFuncToken, peekFar(scopeStack, i).id);

		auto * pArray = lookupFuncSymb(symbTable, candidateIdent);

        if (pArray)
        {
            appendMultiple(poResults, pArray->pBuffer, pArray->cItem);
        }
	}
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
	hash = buildHash(ident.pToken->lexeme.pCh, ident.pToken->lexeme.cCh, hash);

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

	return (i0.pToken->lexeme == i1.pToken->lexeme);
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

#include "print.h"

void debugPrintSymbolTable(const SymbolTable & symbTable)
{
	print("===============\n");
	print("====SYMBOLS====\n");
	print("===============\n\n");
    print("===Variables===\n\n");

    for (auto it = iter(symbTable.varTable); it.pValue; iterNext(&it))
    {
		const ScopedIdentifier * pIdent = it.pKey;
		SymbolInfo * pSymbInfo = it.pValue;

		Assert(pSymbInfo->symbolk == SYMBOLK_Var);

		print("name: ");
		print(pIdent->pToken->lexeme);
		println();

		printfmt("typid: %u\n", pSymbInfo->pVarDeclStmt->typid);
		printfmt("scopeid: %d\n", pIdent->defnclScopeid);
		println();
    }

	print("\n===Functions===\n\n");

	for (auto it = iter(symbTable.funcTable); it.pValue; iterNext(&it))
	{
		const ScopedIdentifier * pIdent = it.pKey;
		DynamicArray<SymbolInfo> * paSymbInfo = it.pValue;

		print("name: ");
		print(pIdent->pToken->lexeme);
		println();

        for (int i = 0; i < paSymbInfo->cItem; i++)
        {
            SymbolInfo * pSymbInfo = &(*paSymbInfo)[i];
		    Assert(pSymbInfo->symbolk == SYMBOLK_Func);

            AstFuncDefnStmt * pFuncDefnStmt = Down(pSymbInfo->pFuncDefnStmt, FuncDefnStmt);
            DynamicArray<AstNode *> * papParamVarDecls = &pFuncDefnStmt->pParamsReturnsGrp->apParamVarDecls;
            DynamicArray<AstNode *> * papReturnVarDecls = &pFuncDefnStmt->pParamsReturnsGrp->apReturnVarDecls;

            if (paSymbInfo->cItem > 1)
            {
                printfmt("\t(overload %d)\n", i);
            }

		    for (int j = 0; j < papParamVarDecls->cItem; j++)
		    {
                auto * pNode = (*papParamVarDecls)[j];
                Assert(pNode->astk == ASTK_VarDeclStmt);
                auto * pVarDecl = Down(pNode, VarDeclStmt);

                if (paSymbInfo->cItem > 1) print("\t\t"); else print("\t");
			    printfmt("param %d typeid: %d\n", j, pVarDecl->typid);
		    }

		    for (int j = 0; j < papReturnVarDecls->cItem; j++)
		    {
                auto * pNode = (*papReturnVarDecls)[j];
                Assert(pNode->astk == ASTK_VarDeclStmt);
                auto * pVarDecl = Down(pNode, VarDeclStmt);

                if (paSymbInfo->cItem > 1) print("\t\t"); else print("\t");
			    printfmt("return %d typid: %d\n", j, pVarDecl->typid);
		    }

            if (paSymbInfo->cItem > 1) print("\t\t"); else print("\t");
            printfmt("scopeid defn: %d\n", pIdent->defnclScopeid);

            if (paSymbInfo->cItem > 1) printfmt("\t\t"); else printfmt("\t");
            printfmt("scopeid body: %d\n", pFuncDefnStmt->scopeid);
        }

        println();
	}

    print("\n===Types===\n\n");

    for (auto it = iter(symbTable.typeTable); it.pValue; iterNext(&it))
    {
        const ScopedIdentifier * pIdent = it.pKey;
        SymbolInfo * pSymbInfo = it.pValue;

        Assert(pSymbInfo->symbolk == SYMBOLK_Struct || pSymbInfo->symbolk == SYMBOLK_BuiltInType);

        if (pSymbInfo->symbolk == SYMBOLK_Struct)
        {
            print("name: ");
			print(pIdent->pToken->lexeme);
			println();

            printfmt("typid: %u\n", pSymbInfo->pStructDefnStmt->typidSelf);
            printfmt("scopeid: %d\n", pIdent->defnclScopeid);
        }
        else
        {
            Assert(pSymbInfo->symbolk == SYMBOLK_BuiltInType);

            print("name: ");
			print(pIdent->pToken->lexeme);
			println();

            printfmt("typid: %u\n", pSymbInfo->typid);
            printfmt("scopeid: %d\n", pIdent->defnclScopeid);
        }

        println();
    }
}
#endif
