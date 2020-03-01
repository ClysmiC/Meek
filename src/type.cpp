#include "ast.h"
#include "error.h"
#include "literal.h"
#include "parse.h"
#include "type.h"

#include <string.h>

void init(Type * pType, TYPEK typek)
{
	pType->typek = typek;

	switch (pType->typek)
	{
		case TYPEK_Base:
		{
			Token * pToken = nullptr;
			setIdentNoScope(&pType->ident, pToken);
		}
		break;

		case TYPEK_Fn:
		{
			init(&pType->funcType);
		}
		break;

		case TYPEK_Array:
		{
			pType->pTypeArray = nullptr;
			pType->pSubscriptExpr = nullptr;
		}
		break;

		case TYPEK_Pointer:
		{
			pType->pTypePtr = nullptr;
		}
		break;
	}
}

void initMove(Type * pType, Type * pTypeSrc)
{
	pType->typek = pTypeSrc->typek;

	switch (pType->typek)
	{
		case TYPEK_Base:
		{
			pType->ident = pTypeSrc->ident;
		}
		break;

		case TYPEK_Fn:
		{
			initMove(&pType->funcType, &pTypeSrc->funcType);
		}
		break;

		case TYPEK_Array:
		{
			pType->pTypeArray = nullptr;
			pType->pSubscriptExpr = nullptr;
		}
		break;

		case TYPEK_Pointer:
		{
			pType->pTypePtr = nullptr;
		}
		break;
	}
}

void initCopy(Type * pType, const Type & typeSrc)
{
	pType->typek = typeSrc.typek;

	switch (pType->typek)
	{
		case TYPEK_Base:
		{
			pType->ident = typeSrc.ident;
		}
		break;

		case TYPEK_Fn:
		{
			initCopy(&pType->funcType, typeSrc.funcType);
		}
		break;

		case TYPEK_Array:
		{
			pType->pTypeArray = nullptr;
			pType->pSubscriptExpr = nullptr;
		}
		break;

		case TYPEK_Pointer:
		{
			pType->pTypePtr = nullptr;
		}
		break;
	}
}

//void dispose(Type * pType)
//{
//	dispose(&pType->aTypemods);
//	if (pType->isFuncType)
//	{
//		dispose(&pType->funcType);
//	}
//}

bool isTypeResolved(const Type & type)
{
    switch (type.typek)
    {
        case TYPEK_Base:
	        return isScopeSet(type.ident);

        // TODO: Check that we have a valid subscript?

        case TYPEK_Array:
            return isTypeResolved(*type.pTypeArray);

        case TYPEK_Fn:
            return isFuncTypeResolved(type.funcType);

        case TYPEK_Pointer:
            return isTypeResolved(*type.pTypePtr);

        default:
            reportIceAndExit("bad typek");
            return false;
    }
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

bool typeEq(const Type & t0, const Type & t1)
{
    if (t0.typek != t1.typek)
        return false;

    switch (t0.typek)
    {
        case TYPEK_Base:
            return scopedIdentEq(t0.ident, t1.ident);

        case TYPEK_Array:
        {
            AssertInfo(t0.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");
            AssertInfo(t1.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");

            auto * pIntLit0 = Down(t0.pSubscriptExpr, LiteralExpr);
            auto * pIntLit1 = Down(t1.pSubscriptExpr, LiteralExpr);

            AssertInfo(pIntLit0->literalk == LITERALK_Int, "Parser should enforce this... for now");
            AssertInfo(pIntLit1->literalk == LITERALK_Int, "Parser should enforce this... for now");

            int intValue0 = intValue(pIntLit0);
            int intValue1 = intValue(pIntLit1);

            if (intValue0 != intValue1)
            {
                return false;
            }

            return typeEq(*t0.pTypeArray, *t1.pTypeArray);
        }
        break;

        case TYPEK_Fn:
            return funcTypeEq(t0.funcType, t1.funcType);

        case TYPEK_Pointer:
            return typeEq(*t0.pTypePtr, *t1.pTypePtr);

        default:
            reportIceAndExit("bad typek");
            return false;
    }
}

uint typeHash(const Type & t)
{

    switch (t.typek)
    {
        case TYPEK_Base:
        {
            Assert(isScopeSet(t.ident));
            return scopedIdentHashPrecomputed(t.ident);
        }
        break;

        case TYPEK_Array:
        {
            AssertInfo(t.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");
            auto * pIntLit = Down(t.pSubscriptExpr, LiteralExpr);

            AssertInfo(pIntLit->literalk == LITERALK_Int, "Parser should enforce this... for now");
            int intVal = intValue(pIntLit);

	        auto hash = startHash();
            buildHash(&intVal, sizeof(intVal), hash);
            return combineHash(hash, typeHash(*t.pTypeArray));
        }
        break;

        case TYPEK_Fn:
            return funcTypeHash(t.funcType);

        case TYPEK_Pointer:
            return typeHash(*t.pTypePtr) * 37;

        default:
            reportIceAndExit("bad typek");
            return 0;
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

void initCopy(FuncType * pFuncType, const FuncType & funcTypeSrc)
{
	initCopy(&pFuncType->paramTypids, funcTypeSrc.paramTypids);
	initCopy(&pFuncType->returnTypids, funcTypeSrc.returnTypids);
}

void reinitCopy(FuncType * pFuncType, const FuncType & funcTypeSrc)
{
	dispose(pFuncType);
	initCopy(pFuncType, funcTypeSrc);
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
        TYPID typid = f.paramTypids[i];
        hash = buildHash(&typid, sizeof(typid), hash);
    }

    for (int i = 0; i < f.returnTypids.cItem; i++)
    {
        TYPID typid = f.returnTypids[i];
        hash = buildHash(&typid, sizeof(typid), hash);
    }

	return hash;
}

bool areTypidListTypesFullyResolved(const DynamicArray<TYPID> & aTypid)
{
	for (int i = 0; i < aTypid.cItem; i++)
    {
        if (!isTypeResolved(aTypid[i]))
        {
            return false;
        }
    }

    return true;
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

bool areTypidListAndVarDeclListTypesEq(const DynamicArray<TYPID> & aTypid, const DynamicArray<AstNode *> & apVarDecls)
{
	AssertInfo(areVarDeclListTypesFullyResolved(apVarDecls), "This shouldn't be called before types are resolved!");
	AssertInfo(areTypidListTypesFullyResolved(aTypid), "This shouldn't be called before types are resolved!");

	if (aTypid.cItem != apVarDecls.cItem) return false;

	for (int i = 0; i < aTypid.cItem; i++)
	{
		TYPID typid0 = aTypid[i];

		Assert(apVarDecls[i]->astk == ASTK_VarDeclStmt);
		auto * pVarDeclStmt = Down(apVarDecls[i], VarDeclStmt);

		TYPID typid1 = pVarDeclStmt->typid;

		if (typid0 != typid1) return false;
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
	// void
    {
        static Token voidToken;
        voidToken.id = -1;
        voidToken.startEnd = gc_startEndBuiltInPseudoToken;
        voidToken.tokenk = TOKENK_Identifier;
        voidToken.lexeme = "void";

        ScopedIdentifier voidIdent;
        setIdent(&voidIdent, &voidToken, SCOPEID_BuiltIn);

        Type voidType;
        init(&voidType, TYPEK_Base);
        voidType.ident = voidIdent;

        Verify(isTypeResolved(voidType));
        Verify(ensureInTypeTable(pTable, voidType) == TYPID_Void);
    }

    // int
    {
        static Token intToken;
        intToken.id = -1;
        intToken.startEnd = gc_startEndBuiltInPseudoToken;
        intToken.tokenk = TOKENK_Identifier;
        intToken.lexeme = "int";

        ScopedIdentifier intIdent;
        setIdent(&intIdent, &intToken, SCOPEID_BuiltIn);

        Type intType;
        init(&intType, TYPEK_Base);
        intType.ident = intIdent;

        Verify(isTypeResolved(intType));
        Verify(ensureInTypeTable(pTable, intType) == TYPID_Int);
    }

    // float
    {
        static Token floatToken;
        floatToken.id = -1;
        floatToken.startEnd = gc_startEndBuiltInPseudoToken;
        floatToken.tokenk = TOKENK_Identifier;
        floatToken.lexeme = "float";

        ScopedIdentifier floatIdent;
        setIdent(&floatIdent, &floatToken, SCOPEID_BuiltIn);

        Type floatType;
        init(&floatType, TYPEK_Base);
        floatType.ident = floatIdent;

        Verify(isTypeResolved(floatType));
        Verify(ensureInTypeTable(pTable, floatType) == TYPID_Float);
    }

    // bool
    {
        static Token boolToken;
        boolToken.id = -1;
        boolToken.startEnd = gc_startEndBuiltInPseudoToken;
        boolToken.tokenk = TOKENK_Identifier;
        boolToken.lexeme = "bool";

        ScopedIdentifier boolIdent;
        setIdent(&boolIdent, &boolToken, SCOPEID_BuiltIn);

        Type boolType;
        init(&boolType, TYPEK_Base);
        boolType.ident = boolIdent;

        Verify(isTypeResolved(boolType));
        Verify(ensureInTypeTable(pTable, boolType) == TYPID_Bool);
    }

	// string
    {
        static Token stringToken;
        stringToken.id = -1;
        stringToken.startEnd = gc_startEndBuiltInPseudoToken;
        stringToken.tokenk = TOKENK_Identifier;
        stringToken.lexeme = "string";

        ScopedIdentifier stringIdent;
        setIdent(&stringIdent, &stringToken, SCOPEID_BuiltIn);

        Type stringType;
        init(&stringType, TYPEK_Base);
        stringType.ident = stringIdent;

        Verify(isTypeResolved(stringType));
        Verify(ensureInTypeTable(pTable, stringType) == TYPID_String);
    }
}

const Type * lookupType(const TypeTable & table, TYPID typid)
{
    return lookupByKey(table.table, typid);
}

TYPID ensureInTypeTable(TypeTable * pTable, const Type & type, bool debugAssertIfAlreadyInTable)
{
	AssertInfo(isTypeResolved(type), "Shouldn't be inserting an unresolved type into the type table...");

	// SLOW: Could write a combined lookup + insertNew if not found query

	const TYPID * pTypid = lookupByValue(pTable->table, type);
	if (pTypid)
	{
		Assert(!debugAssertIfAlreadyInTable);
		return *pTypid;
	}

    TYPID typidInsert = pTable->typidNext;
    pTable->typidNext = static_cast<TYPID>(pTable->typidNext + 1);

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
            Scope candidateScope =peekFar(scopeStack, i);

            candidate.defnclScopeid = candidateScope.id;
            candidate.hash = scopedIdentHash(candidate);

			if (lookupTypeSymb(symbolTable, candidate))
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

// TYPID resolveIntoTypeTableOrSetPending(
// 	Parser * pParser,
// 	Type * pType,
// 	TypePendingResolution ** ppoTypePendingResolution)
// {
// 	Assert(!isTypeResolved(*pType));
//     AssertInfo(ppoTypePendingResolution, "You must have a plan for handling types that are pending resolution if you call this function!");

// 	if (tryResolveType(pType, pParser->symbTable, pParser->scopeStack))
// 	{
// 		TYPID typid = ensureInTypeTable(&pParser->typeTable, *pType);
//         Assert(isTypeResolved(typid));

//         *ppoTypePendingResolution = nullptr;

// 		releaseType(pParser, pType);

//         return typid;
// 	}
// 	else
// 	{
//         TypePendingResolution * pTypePending = appendNew(&pParser->typeTable.typesPendingResolution);
//         pTypePending->pType = pType;
//         pTypePending->pTypidUpdateWhenResolved = nullptr;    // NOTE: Caller sets this value via the pointer that we return in an out param
// 		initCopy(&pTypePending->scopeStack, pParser->scopeStack);

//         *ppoTypePendingResolution = pTypePending;

//         return TYPID_Unresolved;
// 	}
// }

bool tryResolveAllPendingTypesIntoTypeTable(Parser * pParser)
{
	// NOTE: This *should* be doable in a single pass after we have inserted all declared type symbols into the symbol table.
	//	That might change if I add typedefs, since typedefs may form big dependency chains.

	// Iterate backwards to perform removals w/o any fuss

	for (int i = pParser->typeTable.typesPendingResolution.cItem - 1; i >= 0; i--)
	{
		TypePendingResolution * pTypePending = &pParser->typeTable.typesPendingResolution[i];

		if (tryResolveType(pTypePending->pType, pParser->symbTable, pTypePending->scopeStack))
		{
			TYPID typid = ensureInTypeTable(&pParser->typeTable, *(pTypePending->pType));
			*(pTypePending->pTypidUpdateWhenResolved) = typid;

			dispose(&pTypePending->scopeStack);
			releaseType(pParser, pTypePending->pType);

			unorderedRemove(&pParser->typeTable.typesPendingResolution, i);
		}
	}

	return (pParser->typeTable.typesPendingResolution.cItem == 0);
}

TYPID typidFromLiteralk(LITERALK literalk)
{
	if (literalk < LITERALK_Min || literalk >= LITERALK_Max)
	{
		reportIceAndExit("Unexpected literalk value: %d", literalk);
	}

	const static TYPID s_mpLiteralkTypid[] =
	{
		TYPID_Int,
		TYPID_Float,
		TYPID_Bool,
		TYPID_String
	};
	StaticAssert(ArrayLen(s_mpLiteralkTypid) == LITERALK_Max);

	return s_mpLiteralkTypid[literalk];
}
