#include "ast.h"
#include "parse.h"
#include "type.h"

#include <string.h>

extern const typid gc_typidUnresolved = 0;
extern const typid gc_typidUnresolvedInferred = 1;

// SYNC: Built in types should be inserted into the type table in the order that
//  results in the following typids!

extern const typid gc_typidInt      = 2;
extern const typid gc_typidFloat    = 3;
extern const typid gc_typidBool     = 4;

void init(Type * pType, bool isFuncType)
{
	pType->isFuncType = isFuncType;
	init(&pType->aTypemods);

	if (pType->isFuncType)
	{
		init(&pType->funcType);
	}
	else
	{
		Token * pToken = nullptr;
		setIdentNoScope(&pType->ident, pToken);
	}
}

void initMove(Type * pType, Type * pTypeSrc)
{
	pType->isFuncType = pTypeSrc->isFuncType;
	initMove(&pType->aTypemods, &pTypeSrc->aTypemods);

	if (pType->isFuncType)
	{
		initMove(&pType->funcType, &pTypeSrc->funcType);
	}
}

void dispose(Type * pType)
{
	dispose(&pType->aTypemods);
	if (pType->isFuncType)
	{
		dispose(&pType->funcType);
	}
}

bool isTypeResolved(const Type & type)
{
	if (type.isFuncType)
	{
        return isFuncTypeResolved(type.funcType);
	}

	return isScopeSet(type.ident);
}

bool isFuncTypeResolved(const FuncType & funcType)
{
    for (int i = 0; i < funcType.paramTypids.cItem; i++)
    {
        if (!isTypeResolved(funcType.paramTypids[i])) return false;
    }

    for (int i = 0; i < funcType.returnTypids.cItem; i++)
    {
        if (!isTypeResolved(funcType.returnTypids[i])) return false;
    }

    return true;
}

bool isTypeInferred(const Type & type)
{
	bool result = (strcmp(type.ident.pToken->lexeme, "var") == 0);
    return result;
}

bool isUnmodifiedType(const Type & type)
{
	return type.aTypemods.cItem == 0;
}

bool typeEq(const Type & t0, const Type & t1)
{
	if (t0.isFuncType != t1.isFuncType) return false;
	if (t0.aTypemods.cItem != t1.aTypemods.cItem) return false;

	// Check typemods are same

	for (int i = 0; i < t0.aTypemods.cItem; i++)
	{
		TypeModifier tmod0 = t0.aTypemods[i];
		TypeModifier tmod1 = t1.aTypemods[i];
		if (tmod0.typemodk != tmod1.typemodk)
		{
			return false;
		}

		if (tmod0.typemodk == TYPEMODK_Array)
		{
			// TODO: Support arbitrary compile time expressions here... not just int literals

			AssertInfo(tmod0.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");
			AssertInfo(tmod1.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");

			auto * pIntLit0 = Down(tmod0.pSubscriptExpr, LiteralExpr);
			auto * pIntLit1 = Down(tmod0.pSubscriptExpr, LiteralExpr);

			AssertInfo(pIntLit0->literalk == LITERALK_Int, "Parser should enforce this... for now");
			AssertInfo(pIntLit1->literalk == LITERALK_Int, "Parser should enforce this... for now");

			int intValue0 = intValue(pIntLit0);
			int intValue1 = intValue(pIntLit1);

			if (intValue0 != intValue1)
			{
				return false;
			}
		}
	}

	if (t0.isFuncType)
	{
		return funcTypeEq(t0.funcType, t1.funcType);
	}
	else
	{
		return scopedIdentEq(t0.ident, t1.ident);
	}
}

uint typeHash(const Type & t)
{
	auto hash = startHash();

	for (int i = 0; i < t.aTypemods.cItem; i++)
	{
		TypeModifier tmod = t.aTypemods[i];
		hash = buildHash(&tmod.typemodk, sizeof(tmod.typemodk), hash);

		if (tmod.typemodk == TYPEMODK_Array)
		{
			AssertInfo(tmod.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");

			auto * pIntLit = Down(tmod.pSubscriptExpr, LiteralExpr);
			AssertInfo(pIntLit->literalk == LITERALK_Int, "Parser should enforce this... for now");

			int intVal = intValue(pIntLit);
			hash = buildHash(&intVal, sizeof(intVal), hash);
		}
	}

	if (t.isFuncType)
	{
		return combineHash(hash, funcTypeHash(t.funcType));
	}
	else
	{
		Assert(isScopeSet(t.ident));
		return combineHash(hash, scopedIdentHashPrecomputed(t.ident));
	}
}

void init(FuncType * pFuncType)
{
    init(&pFuncType->paramTypids);
    init(&pFuncType->returnTypids);
}

void initMove(FuncType * pFuncType, FuncType * pFuncTypeSrc)
{
	initMove(&pFuncType->paramTypids, &pFuncTypeSrc->paramTypids);
	initMove(&pFuncType->returnTypids, &pFuncTypeSrc->returnTypids);
}

void dispose(FuncType * pFuncType)
{
    dispose(&pFuncType->paramTypids);
    dispose(&pFuncType->returnTypids);
}

bool funcTypeEq(const FuncType & f0, const FuncType & f1)
{
    AssertInfo(
        isFuncTypeResolved(f0) && isFuncTypeResolved(f1),
        "Can't really determine if function types are equal if they aren't yet resolved. "
        "This function is used by the type table which should only be dealing with types that ARE resolved!"
    );

    if (f0.paramTypids.cItem != f1.paramTypids.cItem) return false;
    if (f0.returnTypids.cItem != f1.returnTypids.cItem) return false;

    for (int i = 0; i < f0.paramTypids.cItem; i++)
    {
        if (f0.paramTypids[i] != f1.paramTypids[i])
            return false;
    }

    for (int i = 0; i < f0.returnTypids.cItem; i++)
    {
        if (f0.returnTypids[i] != f1.returnTypids[i])
            return false;
    }

    return true;
}

uint funcTypeHash(const FuncType & f)
{
    AssertInfo(
        isFuncTypeResolved(f),
        "This function is used by the type table which should only be dealing with types that ARE resolved!"
    );

	uint hash = startHash();

    for (int i = 0; i < f.paramTypids.cItem; i++)
    {
        typid typid = f.paramTypids[i];
        hash = buildHash(&typid, sizeof(typid), hash);
    }

    for (int i = 0; i < f.returnTypids.cItem; i++)
    {
        typid typid = f.returnTypids[i];
        hash = buildHash(&typid, sizeof(typid), hash);
    }

	return hash;
}

bool areVarDeclListTypesFullyResolved(const DynamicArray<AstNode*>& apVarDecls)
{
    for (int i = 0; i < apVarDecls.cItem; i++)
    {
        Assert(apVarDecls[i]->astk == ASTK_VarDeclStmt);
        auto * pStmt = Down(apVarDecls[i], VarDeclStmt);

        if (!isTypeResolved(pStmt->typid))
        {
            return false;
        }
    }

    return true;
}

bool areVarDeclListTypesEq(const DynamicArray<AstNode *> & apVarDecls0, const DynamicArray<AstNode *> & apVarDecls1)
{
    Assert(areVarDeclListTypesFullyResolved(apVarDecls0));
    Assert(areVarDeclListTypesFullyResolved(apVarDecls1));

	if (apVarDecls0.cItem != apVarDecls1.cItem) return false;

	for (int i = 0; i < apVarDecls0.cItem; i++)
	{
		AstVarDeclStmt * pDecl0;
		AstVarDeclStmt * pDecl1;

		{
			AstNode * pNode0 = apVarDecls0[i];
			AstNode * pNode1 = apVarDecls1[i];

			Assert(pNode0->astk == ASTK_VarDeclStmt);
			Assert(pNode1->astk == ASTK_VarDeclStmt);

			pDecl0 = Down(pNode0, VarDeclStmt);
			pDecl1 = Down(pNode1, VarDeclStmt);
		}

        if (pDecl0->typid != pDecl1->typid) return false;
	}

	return true;
}

void init(TypeTable * pTable)
{
	init(&pTable->table,
        typidHash,
        typidEq,
        typeHash,
        typeEq);

	init(&pTable->typesPendingResolution);
}

void insertBuiltInTypes(TypeTable * pTable)
{
    Stack<Scope> scopeStack;
    init(&scopeStack);
    Defer(dispose(&scopeStack));

    Scope builtInScope;
    builtInScope.id = gc_builtInScopeid;
    builtInScope.scopek = SCOPEK_BuiltIn;

    push(&scopeStack, builtInScope);

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

        Type intType;
        init(&intType, false /* isFuncType */);
        intType.ident = intIdent;

        Verify(isTypeResolved(intType));
        Verify(ensureInTypeTable(pTable, intType) == gc_typidInt);
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

        Type floatType;
        init(&floatType, false /* isFuncType */);
        floatType.ident = floatIdent;

        Verify(isTypeResolved(floatType));
        Verify(ensureInTypeTable(pTable, floatType) == gc_typidFloat);
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

        Type boolType;
        init(&boolType, false /* isFuncType */);
        boolType.ident = boolIdent;

        Verify(isTypeResolved(boolType));
        Verify(ensureInTypeTable(pTable, boolType) == gc_typidBool);
    }
}

const Type * lookupType(TypeTable * pTable, typid typid)
{
    return lookupByKey(pTable->table, typid);
}

typid ensureInTypeTable(TypeTable * pTable, const Type & type, bool debugAssertIfAlreadyInTable)
{
	AssertInfo(isTypeResolved(type), "Shouldn't be inserting an unresolved type into the type table...");

	// SLOW: Could write a combined lookup + insertNew if not found query

	const typid * pTypid = lookupByValue(pTable->table, type);
	if (pTypid)
	{
		Assert(!debugAssertIfAlreadyInTable);
		return *pTypid;
	}

    typid typidInsert = pTable->typidNext;
    pTable->typidNext++;

	Verify(insert(&pTable->table, typidInsert, type));
    return typidInsert;
}

bool tryResolveType(Type * pType, const SymbolTable & symbolTable, const Stack<Scope> & scopeStack)
{
	if (!pType->isFuncType)
	{
		// Non-func type

		ScopedIdentifier candidate = pType->ident;

		for (int i = 0; i < count(scopeStack); i++)
		{
            Scope candidateScope;
			Verify(peekFar(scopeStack, i, &candidateScope));

            candidate.defnclScopeid = candidateScope.id;
            candidate.hash = scopedIdentHash(candidate);

			if (lookupType(symbolTable, candidate))
			{
				pType->ident = candidate;
				return true;
			}
		}

        return false;
	}
	else
	{
		// Func type

        for (int i = 0; i < pType->funcType.paramTypids.cItem; i++)
        {
            if (!isTypeResolved(pType->funcType.paramTypids[i]))
            {
                return false;
            }
		}

		for (int i = 0; i < pType->funcType.returnTypids.cItem; i++)
		{
            if (!isTypeResolved(pType->funcType.returnTypids[i]))
            {
				return false;
            }
		}

		return true;
	}
}

typid resolveIntoTypeTableOrSetPending(
	Parser * pParser,
	Type * pType,
	TypePendingResolution ** ppoTypePendingResolution)
{
	Assert(!isTypeResolved(*pType));
    AssertInfo(ppoTypePendingResolution, "You must have a plan for handling types that are pending resolution if you call this function!");

	if (tryResolveType(pType, pParser->symbolTable, pParser->scopeStack))
	{
		typid typid = ensureInTypeTable(&pParser->typeTable, *pType);
        Assert(isTypeResolved(typid));

        *ppoTypePendingResolution = nullptr;

		releaseType(pParser, pType);

        return typid;
	}
	else
	{
        TypePendingResolution * pTypePending = appendNew(&pParser->typeTable.typesPendingResolution);
        pTypePending->pType = pType;
        pTypePending->pTypidUpdateWhenResolved = nullptr;    // NOTE: Caller sets this value via the pointer that we return in an out param
		initCopy(&pTypePending->scopeStack, pParser->scopeStack);

        *ppoTypePendingResolution = pTypePending;

        return gc_typidUnresolved;
	}
}

bool tryResolveAllPendingTypesIntoTypeTable(Parser * pParser)
{
	bool madeProgress = true;
	while (madeProgress && pParser->typeTable.typesPendingResolution.cItem > 0)
	{
		madeProgress = false;

		// Iterate backwards to perform removals w/o any fuss

		for (int i = pParser->typeTable.typesPendingResolution.cItem - 1; i >= 0; i--)
		{
			TypePendingResolution * pTypePending = &pParser->typeTable.typesPendingResolution[i];

			if (tryResolveType(pTypePending->pType, pParser->symbolTable, pTypePending->scopeStack))
			{
				madeProgress = true;

				typid typid = ensureInTypeTable(&pParser->typeTable, *(pTypePending->pType));
				*(pTypePending->pTypidUpdateWhenResolved) = typid;

				dispose(&pTypePending->scopeStack);
				releaseType(pParser, pTypePending->pType);

				unorderedRemove(&pParser->typeTable.typesPendingResolution, i);
			}
		}
	}

	return (pParser->typeTable.typesPendingResolution.cItem == 0);
}
