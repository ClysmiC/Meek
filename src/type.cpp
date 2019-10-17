#include "ast.h"
#include "type.h"

#include <string.h>

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
		return funcTypeEq(*t0.pFuncType, *t1.pFuncType);
	}
	else
	{
		return scopedIdentEq(t0.ident, t1.ident);
	}
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

void init(FuncType * pFuncType)
{
    init(&pFuncType->apParamType);
    init(&pFuncType->apReturnType);
}

void dispose(FuncType * pFuncType)
{
    dispose(&pFuncType->apParamType);
    dispose(&pFuncType->apReturnType);
}

bool funcTypeEq(const FuncType & f0, const FuncType & f1)
{
    if (f0.apParamType.cItem != f1.apParamType.cItem) return false;
    if (f0.apReturnType.cItem != f1.apReturnType.cItem) return false;

    for (int i = 0; i < f0.apParamType.cItem; i++)
    {
        if (!typeEq(*f0.apParamType[i], *f1.apParamType[i]))
            return false;
    }

    for (int i = 0; i < f0.apReturnType.cItem; i++)
    {
        if (!typeEq(*f0.apReturnType[i], *f1.apReturnType[i]))
            return false;
    }

    return true;
}

bool areVarDeclListTypesEq(const DynamicArray<AstNode *> & apVarDecls0, const DynamicArray<AstNode *> & apVarDecls1)
{
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

		AssertInfo(pDecl0->pType, "I don't really handle type inference yet... try again later!");
		AssertInfo(pDecl1->pType, "I don't really handle type inference yet... try again later!");

		if (!typeEq(*pDecl0->pType, *pDecl1->pType)) return false;
	}

	return true;
}
