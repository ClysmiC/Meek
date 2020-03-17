#include "ast.h"
#include "error.h"
#include "literal.h"
#include "parse.h"
#include "type.h"

void init(Type * pType, bool isFuncType)
{
	pType->isFuncType = isFuncType;
	init(&pType->aTypemods);

	if (pType->isFuncType)
	{
		init(&pType->funcTypeData.funcType);
	}
	else
	{
		setLexeme(&pType->nonFuncTypeData.ident.lexeme, "");
		pType->nonFuncTypeData.ident.scopeid = SCOPEID_Nil;
	}
}

//void initMove(Type * pType, Type * pTypeSrc)
//{
//	pType->isFuncType = pTypeSrc->isFuncType;
//	initMove(&pType->aTypemods, &pTypeSrc->aTypemods);
//
//	if (pType->isFuncType)
//	{
//		initMove(&pType->funcType, &pTypeSrc->funcType);
//	}
//}
//
//void initCopy(Type * pType, const Type & typeSrc)
//{
//	pType->isFuncType = typeSrc.isFuncType;
//	initCopy(&pType->aTypemods, typeSrc.aTypemods);
//
//	if (pType->isFuncType)
//	{
//		initCopy(&pType->funcType, typeSrc.funcType);
//	}
//	else
//	{
//		// META: This assignment is fine since ScopedIdentifier doesn't own any resources. How would I make this more robust
//		//	if I wanted to change ScopedIdentifier to allow it to own resources in the future? Writing an initCopy for ident
//		//	that just does this assignment seems like too much boilerplate. I would also like to prefer avoiding C++ constructor
//		//	craziness. Is there a middle ground? How do I want to handle this in Meek?
//
//		pType->ident = typeSrc.ident;
//	}
//}

void dispose(Type * pType)
{
	dispose(&pType->aTypemods);
	if (pType->isFuncType)
	{
		dispose(&pType->funcTypeData.funcType);
	}
}

bool isTypeResolved(const Type & type)
{
	if (type.isFuncType)
	{
		return isFuncTypeResolved(type.funcTypeData.funcType);
	}
	else
	{
		return type.nonFuncTypeData.ident.scopeid != SCOPEID_Nil;
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

NULLABLE const FuncType * funcTypeFromDefnStmt(const TypeTable & typeTable, const AstFuncDefnStmt & defnStmt)
{
	Assert(isTypeResolved(defnStmt.typid));
	const Type * pType = lookupType(typeTable, defnStmt.typid);
	Assert(pType);

	if (pType->isFuncType)
	{
		return &pType->funcTypeData.funcType;
	}
	else
	{
		return nullptr;
	}
}

bool isUnmodifiedType(const Type & type)
{
	return type.aTypemods.cItem == 0;
}

bool isPointerType(const Type & type)
{
	return type.aTypemods.cItem > 0 && type.aTypemods[0].typemodk == TYPEMODK_Pointer;
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
		return funcTypeEq(t0.funcTypeData.funcType, t1.funcTypeData.funcType);
	}
	else
	{
		return scopedIdentEq(t0.nonFuncTypeData.ident, t1.nonFuncTypeData.ident);
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
		return combineHash(hash, funcTypeHash(t.funcTypeData.funcType));
	}
	else
	{
		return scopedIdentHash(t.nonFuncTypeData.ident);
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

bool areTypidListTypesEq(const DynamicArray<TYPID> & aTypid0, const DynamicArray<TYPID> & aTypid1)
{
	if (aTypid0.cItem != aTypid1.cItem)
	{
		return false;
	}

	for (int iTypid = 0; iTypid < aTypid0.cItem; iTypid++)
	{
		TYPID typid0 = aTypid0[iTypid];
		TYPID typid1 = aTypid1[iTypid];

		Assert(isTypeResolved(typid0));
		Assert(isTypeResolved(typid1));

		if (typid0 != typid1)
		{
			return false;
		}
	}

	return true;
}

//bool areTypidListAndVarDeclListTypesEq(const DynamicArray<TYPID> & aTypid, const DynamicArray<AstNode *> & apVarDecls)
//{
//	AssertInfo(areVarDeclListTypesFullyResolved(apVarDecls), "This shouldn't be called before types are resolved!");
//	AssertInfo(areTypidListTypesFullyResolved(aTypid), "This shouldn't be called before types are resolved!");
//
//	if (aTypid.cItem != apVarDecls.cItem) return false;
//
//	for (int i = 0; i < aTypid.cItem; i++)
//	{
//		TYPID typid0 = aTypid[i];
//
//		Assert(apVarDecls[i]->astk == ASTK_VarDeclStmt);
//		auto * pVarDeclStmt = Down(apVarDecls[i], VarDeclStmt);
//
//		TYPID typid1 = pVarDeclStmt->typid;
//
//		if (typid0 != typid1) return false;
//	}
//
//	return true;
//}

void init(TypeTable * pTable)
{
	init(&pTable->table,
		typidHash,
		typidEq,
		typeHash,
		typeEq);

	init (&pTable->typesPendingResolution);

	// Insert built in types

	// void

	{

		ScopedIdentifier voidIdent;
		setLexeme(&voidIdent.lexeme, "void");
		voidIdent.scopeid = SCOPEID_BuiltIn;

		Type voidType;
		init(&voidType, false /* isFuncType */);
		voidType.nonFuncTypeData.ident = voidIdent;

		Verify(isTypeResolved(voidType));
		Verify(ensureInTypeTable(pTable, voidType) == TYPID_Void);
	}

	// int

	{
		ScopedIdentifier intIdent;
		setLexeme(&intIdent.lexeme, "int");
		intIdent.scopeid = SCOPEID_BuiltIn;

		Type intType;
		init(&intType, false /* isFuncType */);
		intType.nonFuncTypeData.ident = intIdent;

		Verify(isTypeResolved(intType));
		Verify(ensureInTypeTable(pTable, intType) == TYPID_Int);
	}

	// float

	{
		ScopedIdentifier floatIdent;
		setLexeme(&floatIdent.lexeme, "float");
		floatIdent.scopeid = SCOPEID_BuiltIn;

		Type floatType;
		init(&floatType, false /* isFuncType */);
		floatType.nonFuncTypeData.ident = floatIdent;

		Verify(isTypeResolved(floatType));
		Verify(ensureInTypeTable(pTable, floatType) == TYPID_Float);
	}

	// bool

	{
		ScopedIdentifier boolIdent;
		setLexeme(&boolIdent.lexeme, "bool");
		boolIdent.scopeid = SCOPEID_BuiltIn;

		Type boolType;
		init(&boolType, false /* isFuncType */);
		boolType.nonFuncTypeData.ident = boolIdent;

		Verify(isTypeResolved(boolType));
		Verify(ensureInTypeTable(pTable, boolType) == TYPID_Bool);
	}

	// string

	{
		ScopedIdentifier stringIdent;
		setLexeme(&stringIdent.lexeme, "string");
		stringIdent.scopeid = SCOPEID_BuiltIn;

		Type stringType;
		init(&stringType, false /* isFuncType */);
		stringType.nonFuncTypeData.ident = stringIdent;

		Verify(isTypeResolved(stringType));
		Verify(ensureInTypeTable(pTable, stringType) == TYPID_String);
	}
}

NULLABLE const Type * lookupType(const TypeTable & table, TYPID typid)
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

bool tryResolveAllTypes(Parser * pParser)
{
	auto tryResolvePendingType = [](const TypePendingResolve & typePending)
	{
		Type * pType = typePending.pType;
		if (!typePending.pType->isFuncType)
		{
			// Non-func type

			ScopedIdentifier candidate = typePending.pType->nonFuncTypeData.ident;

			Scope * pScope = typePending.pScope;
			SymbolInfo symbInfo = lookupTypeSymbol(*typePending.pScope, typePending.pType->nonFuncTypeData.ident.lexeme);

			if (symbInfo.symbolk == SYMBOLK_Nil)
				return false;

			pType->nonFuncTypeData.ident.scopeid = scopeidFromSymbolInfo(symbInfo);
			return true;
		}
		else
		{
			// Func type

			for (int i = 0; i < pType->funcTypeData.funcType.paramTypids.cItem; i++)
			{
				if (!isTypeResolved(pType->funcTypeData.funcType.paramTypids[i]))
				{
					return false;
				}
			}

			for (int i = 0; i < pType->funcTypeData.funcType.returnTypids.cItem; i++)
			{
				if (!isTypeResolved(pType->funcTypeData.funcType.returnTypids[i]))
				{
					return false;
				}
			}

			return true;
		}
	};

	// NOTE: This *should* be doable in a single pass after we have inserted all declared type symbols into the symbol table.
	//	That might change if I add typedefs, since typedefs may form big dependency chains.

	for (int i = pParser->typeTable.typesPendingResolution.cItem - 1; i >= 0; i--)
	{
		TypePendingResolve * pTypePending = &pParser->typeTable.typesPendingResolution[i];
		if (tryResolvePendingType(*pTypePending))
		{
			TYPID typid = ensureInTypeTable(&pParser->typeTable, *(pTypePending->pType));
			*(pTypePending->pTypidUpdateOnResolve) = typid;

			releaseType(pParser, pTypePending->pType);		// TODO: Come up with a better strategy for this...
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

bool canCoerce(TYPID typidFrom, TYPID typidTo)
{
	// TODO

	return false;
}

#if DEBUG

#include "print.h"

void debugPrintType(const Type& type)
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

				print("[");

				Assert(tmod.pSubscriptExpr && tmod.pSubscriptExpr->astk == ASTK_LiteralExpr);

				auto* pNode = Down(tmod.pSubscriptExpr, LiteralExpr);
				Assert(pNode->literalk == LITERALK_Int);

				int value = intValue(pNode);
				Assert(pNode->isValueSet);
				AssertInfo(!pNode->isValueErroneous, "Just for sake of testing... in reality if it was erroneous then we don't really care/bother about the symbol table");

				printfmt("%d", value);

				print("]");
			} break;

			case TYPEMODK_Pointer:
			{
				print("^");
			} break;
		}
	}

	if (!type.isFuncType)
	{
		print(type.nonFuncTypeData.ident.lexeme.strv);
		printfmt(" (scopeid: %d)", type.nonFuncTypeData.ident.scopeid);
	}
	else
	{
		// TODO: :) ... will need to set up tab/new line stuff so that it
		//	can nest arbitrarily. Can't be bothered right now!!!

		print("(function type... CBA to print LOL)");
	}
}

void debugPrintTypeTable(const TypeTable& typeTable)
{
	print("===============\n");
	print("=====TYPES=====\n");
	print("===============\n\n");

	for (auto it = iter(typeTable.table); it.pValue; iterNext(&it))
	{
		const Type * pType = it.pValue;
		debugPrintType(*pType);

		println();
	}
}

#endif