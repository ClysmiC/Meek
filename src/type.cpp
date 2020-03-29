#include "ast.h"
#include "error.h"
#include "literal.h"
#include "parse.h"
#include "type.h"

void init(Type * pType, bool isFuncType)
{
	pType->isFuncType = isFuncType;
	pType->isInferred = false;
	pType->info.size = Type::TypeInfo::s_unset;
	pType->info.alignment = Type::TypeInfo::s_unset;

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

void initCopy(Type * pType, const Type & typeSrc)
{
	pType->isFuncType = typeSrc.isFuncType;
	pType->isInferred = typeSrc.isInferred;
	pType->info = typeSrc.info;
	initCopy(&pType->aTypemods, typeSrc.aTypemods);

	if (pType->isFuncType)
	{
		initCopy(&pType->funcTypeData.funcType, typeSrc.funcTypeData.funcType);
	}
	else
	{
		// META: This assignment is fine since ScopedIdentifier doesn't own any resources. How would I make this more robust
		//	if I wanted to change ScopedIdentifier to allow it to own resources in the future? Writing an initCopy for ident
		//	that just does this assignment seems like too much boilerplate. I would also like to prefer avoiding C++ constructor
		//	craziness. Is there a middle ground? How do I want to handle this in Meek?

		pType->nonFuncTypeData.ident = typeSrc.nonFuncTypeData.ident;
	}
}

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
	Assert(isTypeResolved(defnStmt.typidDefn));
	const Type * pType = lookupType(typeTable, defnStmt.typidDefn);
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

PENDINGTYPID registerPendingNonFuncType(
	TypeTable * pTable,
	Scope * pScope,
	Lexeme ident,
	const DynamicArray<TypeModifier> & aTypemod,
	NULLABLE TYPID * pTypidUpdateOnResolve)
{
	PENDINGTYPID result = PENDINGTYPID(pTable->typesPendingResolution.cItem);

	const bool isFuncType = false;
	TypeTable::TypePendingResolve * pTypePending = appendNew(&pTable->typesPendingResolution);
	init(pTypePending, pScope, isFuncType);
	pTypePending->type.nonFuncTypeData.ident.lexeme = ident;
	pTypePending->type.nonFuncTypeData.ident.scopeid = SCOPEID_Nil;		// Not yet known

	if (pTypidUpdateOnResolve)
	{
		pTypePending->apTypidUpdateOnResolve[0] = pTypidUpdateOnResolve;
		pTypePending->cPTypidUpdateOnResolve++;
	}

	// @Slow - move?

	reinitCopy(&pTypePending->type.aTypemods, aTypemod);

	return result;
}

PENDINGTYPID registerPendingFuncType(
	TypeTable * pTable,
	Scope * pScope,
	const DynamicArray<TypeModifier> & aTypemod,
	const DynamicArray<PENDINGTYPID> & aPendingTypidParam,
	const DynamicArray<PENDINGTYPID> & aPendingTypidReturn,
	NULLABLE TYPID * pTypidUpdateOnResolve)
{
	PENDINGTYPID result = PENDINGTYPID(pTable->typesPendingResolution.cItem);

	const bool isFuncType = true;
	TypeTable::TypePendingResolve * pTypePending = appendNew(&pTable->typesPendingResolution);
	init(pTypePending, pScope, isFuncType);
	
	if (pTypidUpdateOnResolve)
	{
		pTypePending->apTypidUpdateOnResolve[0] = pTypidUpdateOnResolve;
		pTypePending->cPTypidUpdateOnResolve++;
	}

	// @Slow - move?

	reinitCopy(&pTypePending->type.aTypemods, aTypemod);

	for (int iParam = 0; iParam < aPendingTypidParam.cItem; iParam++)
	{
		TYPID * pTypidParam = appendNew(&pTypePending->type.funcTypeData.funcType.paramTypids);
		*pTypidParam = TYPID_Unresolved;
		setPendingTypeUpdateOnResolvePtr(pTable, aPendingTypidParam[iParam], pTypidParam);
	}

	for (int iReturn = 0; iReturn < aPendingTypidReturn.cItem; iReturn++)
	{
		TYPID * pTypidReturn = appendNew(&pTypePending->type.funcTypeData.funcType.returnTypids);
		*pTypidReturn = TYPID_Unresolved;
		setPendingTypeUpdateOnResolvePtr(pTable, aPendingTypidReturn[iReturn], pTypidReturn);
	}

	return result;
}

void setPendingTypeUpdateOnResolvePtr(TypeTable * pTable, PENDINGTYPID pendingTypid, TYPID * pTypidUpdateOnResolve)
{
	Assert(pendingTypid < PENDINGTYPID(pTable->typesPendingResolution.cItem));

	TypeTable::TypePendingResolve * pTypePending = &pTable->typesPendingResolution[pendingTypid];
	Assert(pTypePending->cPTypidUpdateOnResolve < TypeTable::TypePendingResolve::s_cTypidUpdateOnResolveMax);

	pTypePending->apTypidUpdateOnResolve[pTypePending->cPTypidUpdateOnResolve] = pTypidUpdateOnResolve;
	pTypePending->cPTypidUpdateOnResolve++;
}

bool isUnmodifiedType(const Type & type)
{
	return type.aTypemods.cItem == 0;
}

bool isPointerType(const Type & type)
{
	return type.aTypemods.cItem > 0 && type.aTypemods[0].typemodk == TYPEMODK_Pointer;
}

Lexeme getDealiasedTypeLexeme(const Lexeme & lexeme)
{
	Lexeme result;
	if (lexeme.strv == "int")
	{
		setLexeme(&result, "s32");
	}
	else if (lexeme.strv == "uint")
	{
		setLexeme(&result, "u32");
	}
	else if (lexeme.strv == "float")
	{
		setLexeme(&result, "f32");
	}
	else
	{
		result = lexeme;
	}

	return result;
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
		ScopedIdentifier identDealiased0 = t0.nonFuncTypeData.ident;
		identDealiased0.lexeme = getDealiasedTypeLexeme(identDealiased0.lexeme);

		ScopedIdentifier identDealiased1 = t1.nonFuncTypeData.ident;
		identDealiased1.lexeme = getDealiasedTypeLexeme(identDealiased1.lexeme);

		return scopedIdentEq(identDealiased0, identDealiased1);
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
		ScopedIdentifier identDealiased = t.nonFuncTypeData.ident;
		identDealiased.lexeme = getDealiasedTypeLexeme(identDealiased.lexeme);

		return combineHash(hash, scopedIdentHash(identDealiased));
	}
}

void init(FuncType * pFuncType)
{
	init(&pFuncType->paramTypids);
	init(&pFuncType->returnTypids);
}

//void initMove(FuncType * pFuncType, FuncType * pFuncTypeSrc)
//{
//	initMove(&pFuncType->paramTypids, &pFuncTypeSrc->paramTypids);
//	initMove(&pFuncType->returnTypids, &pFuncTypeSrc->returnTypids);
//}

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

		if (!isTypeResolved(pStmt->typidDefn))
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

		if (pDecl0->typidDefn != pDecl1->typidDefn) return false;
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
		typeHashPtr,
		typeEqPtr);

	init(&pTable->typesPendingResolution);
	init(&pTable->typeAlloc);

	auto insertBuiltInType = [](TypeTable * pTable, const char * strIdent, TYPID typidExpected, int size)
	{
		ScopedIdentifier ident;
		setLexeme(&ident.lexeme, strIdent);
		ident.scopeid = SCOPEID_BuiltIn;

		Type type;
		init(&type, false /* isFuncType */);
		type.nonFuncTypeData.ident = ident;
		type.info.size = size;
		type.info.alignment = size;

		Verify(isTypeResolved(type));
		Verify(ensureInTypeTable(pTable, &type) == typidExpected);
	};

	insertBuiltInType(pTable, "void", TYPID_Void, 0);

	insertBuiltInType(pTable, "s8", TYPID_S8, 1);
	insertBuiltInType(pTable, "s16", TYPID_S16, 2);
	insertBuiltInType(pTable, "s32", TYPID_S32, 4);
	insertBuiltInType(pTable, "s64", TYPID_S64, 8);

	insertBuiltInType(pTable, "u8", TYPID_U8, 1);
	insertBuiltInType(pTable, "u16", TYPID_U16, 2);
	insertBuiltInType(pTable, "u32", TYPID_U32, 4);
	insertBuiltInType(pTable, "u64", TYPID_U64, 8);

	insertBuiltInType(pTable, "f32", TYPID_F32, 4);
	insertBuiltInType(pTable, "f64", TYPID_F64, 8);

	insertBuiltInType(pTable, "bool", TYPID_Bool, 1);

	insertBuiltInType(pTable, "string", TYPID_String, -1);	// TODO: Pass actual size once I figure out the in-memory representation.
}

void init(TypeTable::TypePendingResolve * pTypePending, Scope * pScope, bool isFuncType)
{
	init(&pTypePending->type, isFuncType);
	pTypePending->pScope = pScope;
	pTypePending->cPTypidUpdateOnResolve = 0;
}

void dispose(TypeTable::TypePendingResolve * pTypePending)
{
	dispose(&pTypePending->type);
}

NULLABLE const Type * lookupType(const TypeTable & table, TYPID typid)
{
	auto ppType = lookupByKey(table.table, typid);
	if (!ppType)
		return nullptr;

	return *ppType;
}

void setTypeInfo(const TypeTable & table, TYPID typid, const Type::TypeInfo & typeInfo)
{
	Assert(isTypeResolved(typid));
	
	auto ppType = lookupByKeyRaw(table.table, typid);
	Assert(ppType);
	Assert(*ppType);

	(*ppType)->info = typeInfo;
}

TYPID ensureInTypeTable(TypeTable * pTable, Type * pType, bool debugAssertIfAlreadyInTable)
{
	AssertInfo(isTypeResolved(*pType), "Shouldn't be inserting an unresolved type into the type table...");

	// SLOW: Could write a combined lookup + insertNew if not found query

	const TYPID * pTypid = lookupByValue(pTable->table, pType);
	if (pTypid)
	{
		Assert(!debugAssertIfAlreadyInTable);
		return *pTypid;
	}

	Type * pTypeCopy = allocate(&pTable->typeAlloc);
	initCopy(pTypeCopy, *pType);

	TYPID typidInsert = pTable->typidNext;
	pTable->typidNext = static_cast<TYPID>(pTable->typidNext + 1);

	Verify(insert(&pTable->table, typidInsert, pTypeCopy));
	return typidInsert;
}

bool tryResolveAllTypes(Parser * pParser)
{
	auto tryResolvePendingType = [](TypeTable::TypePendingResolve * pTypePending)
	{
		if (!pTypePending->type.isFuncType)
		{
			// Non-func type

			SymbolInfo symbInfo = lookupTypeSymbol(*pTypePending->pScope, pTypePending->type.nonFuncTypeData.ident.lexeme);
			if (symbInfo.symbolk == SYMBOLK_Nil)
				return false;

			pTypePending->type.nonFuncTypeData.ident.scopeid = scopeidFromSymbolInfo(symbInfo);
			return true;
		}
		else
		{
			// Func type

			for (int i = 0; i < pTypePending->type.funcTypeData.funcType.paramTypids.cItem; i++)
			{
				if (!isTypeResolved(pTypePending->type.funcTypeData.funcType.paramTypids[i]))
				{
					return false;
				}
			}

			for (int i = 0; i < pTypePending->type.funcTypeData.funcType.returnTypids.cItem; i++)
			{
				if (!isTypeResolved(pTypePending->type.funcTypeData.funcType.returnTypids[i]))
				{
					return false;
				}
			}

			return true;
		}
	};

	auto tryResolvePendingTypeInfo = [](const TypeTable & typeTable, const DynamicArray<Scope *> & mpScopeidPScope, TYPID typid)
	{
		static const int s_targetWordSize = 64;		// TODO: configurable target
		static const int s_cBytePtr = (s_targetWordSize + 7) / 8;

		const Type * pType = lookupType(typeTable, typid);

		Type::TypeInfo typeInfoResult;
		typeInfoResult.size = Type::TypeInfo::s_unset;
		typeInfoResult.alignment = Type::TypeInfo::s_unset;

		Assert(pType);
		AssertInfo(pType->info.size == Type::TypeInfo::s_unset, "Trying to resolve pending type info that has already been resolved?");

		if (pType->aTypemods.cItem > 0)
		{
			TypeModifier typeMod = pType->aTypemods[0];
			switch (typeMod.typemodk)
			{
				case TYPEMODK_Array:
				{
					AssertTodo;
				} break;

				case TYPEMODK_Pointer:
				{
					typeInfoResult.size = s_cBytePtr;
					typeInfoResult.alignment = s_cBytePtr;
				} break;

				default:
				{
					AssertNotReached;
				} break;
			}
		}
		else
		{
			if (pType->isFuncType)
			{
				typeInfoResult.size = s_cBytePtr;
				typeInfoResult.alignment = s_cBytePtr;
			}
			else
			{
				SymbolInfo symbInfo =
					lookupTypeSymbol(
						*mpScopeidPScope[pType->nonFuncTypeData.ident.scopeid],
						pType->nonFuncTypeData.ident.lexeme);
				
				Assert(symbInfo.symbolk == SYMBOLK_Struct);

				auto * pStructDefn = symbInfo.structData.pStructDefnStmt;
				Assert(pStructDefn);

				if (pStructDefn->apVarDeclStmt.cItem == 0)
				{
					// Empty struct

					typeInfoResult.size = 1;
					typeInfoResult.alignment = 1;
				}
				else
				{
					bool allMembersCounted = true;
					u32 sizeWithPadding = 0;
					u32 alignmentMax = 1;

					for (int iVarDeclStmt = 0; iVarDeclStmt < pStructDefn->apVarDeclStmt.cItem; iVarDeclStmt++)
					{
						auto * pVarDeclStmt = Down(pStructDefn->apVarDeclStmt[iVarDeclStmt], VarDeclStmt);
						if (!isTypeResolved(pVarDeclStmt->typidDefn))
						{
							allMembersCounted = false;
							break;
						}

						const Type * pTypeVarDecl = lookupType(typeTable, pVarDeclStmt->typidDefn);
						Assert(pTypeVarDecl);

						u32 sizeMember = pTypeVarDecl->info.size;
						u32 alignmentMember = pTypeVarDecl->info.alignment;

						Assert(Iff(sizeMember == Type::TypeInfo::s_unset, alignmentMember == Type::TypeInfo::s_unset));
						if (sizeMember == Type::TypeInfo::s_unset)
						{
							allMembersCounted = false;
							break;
						}

						// Using C-like struct packing for now.
						// http://www.catb.org/esr/structure-packing/

						if (iVarDeclStmt > 0)
						{
							int bytesPastAlignment = sizeWithPadding % alignmentMember;
							int padding = (bytesPastAlignment == 0) ? 0 : alignmentMember - bytesPastAlignment;
							sizeWithPadding += padding;
						}

						sizeWithPadding += sizeMember;
						alignmentMax = Max(alignmentMax, alignmentMember);
					}

					if (allMembersCounted)
					{
						typeInfoResult.alignment = alignmentMax;

						int bytesPastAlignment = sizeWithPadding % typeInfoResult.alignment;
						int endPadding = (bytesPastAlignment == 0) ? 0 : typeInfoResult.alignment - bytesPastAlignment;
						sizeWithPadding += endPadding;

						typeInfoResult.size = sizeWithPadding;
					}
				}
			}
		}

		Assert(typeInfoResult.alignment <= typeInfoResult.size);
		Assert(typeInfoResult.size % typeInfoResult.alignment == 0);
		Assert(Iff(typeInfoResult.size == Type::TypeInfo::s_unset, typeInfoResult.alignment == Type::TypeInfo::s_unset));
		if (typeInfoResult.size == Type::TypeInfo::s_unset)
			return false;

		setTypeInfo(typeTable, typid, typeInfoResult);
		return true;
	};

	// Resolve named types (and eagerly resolve type infos where we can)

	DynamicArray<TYPID> aTypidInfoPending;
	init(&aTypidInfoPending);
	Defer(dispose(&aTypidInfoPending));

	int cTypeUnresolved = 0;
	{
		for (int i = 0; i < pParser->typeTable.typesPendingResolution.cItem; i++)
		{
			TypeTable::TypePendingResolve * pTypePending = &pParser->typeTable.typesPendingResolution[i];
			if (tryResolvePendingType(pTypePending))
			{
				TYPID typid = ensureInTypeTable(&pParser->typeTable, &pTypePending->type);
				for (int iTypidUpdate = 0; iTypidUpdate < pTypePending->cPTypidUpdateOnResolve; iTypidUpdate++)
				{
					TYPID * pTypidUpdate = pTypePending->apTypidUpdateOnResolve[iTypidUpdate];
					*pTypidUpdate = typid;
				}

				const Type * pType = lookupType(pParser->typeTable, typid);
				if (pType->info.size == Type::TypeInfo::s_unset)
				{
					if (tryResolvePendingTypeInfo(pParser->typeTable, pParser->mpScopeidPScope, typid))
					{
						Assert(pType->info.size != Type::TypeInfo::s_unset);
					}
					else
					{
						append(&aTypidInfoPending, typid);
					}
				}
			}
			else
			{
				// TODO: print specific error

				cTypeUnresolved++;
			}

			dispose(pTypePending);
		}

		dispose(&pParser->typeTable.typesPendingResolution);
	}

	// Resolve type infos we weren't able to resolve eagerly

	bool madeProgress = true;
	while (madeProgress)
	{
		madeProgress = false;

		for (int i = aTypidInfoPending.cItem - 1; i >= 0; i--)
		{
			TYPID typidInfoPending = aTypidInfoPending[i];
			Assert(isTypeResolved(typidInfoPending));

			const Type * pType = lookupType(pParser->typeTable, typidInfoPending);
			if (pType->info.size != Type::TypeInfo::s_unset)
			{
				// NOTE (andrew) This can happen when multiple copies of the same typid are added to
				//	this list. This makes it kind of @Slow, we should probably use a HashSet instead
				//	of a DynamicArray for the pending typeinfo typid's...

				unorderedRemove(&aTypidInfoPending, i);
			}
			else if (tryResolvePendingTypeInfo(pParser->typeTable, pParser->mpScopeidPScope, typidInfoPending))
			{
				madeProgress = true;
				unorderedRemove(&aTypidInfoPending, i);
			}
			else
			{
				// Still not enough info to resolve. Keep around for next iteration.
			}
		}
	}

	// TODO: Better distinction between type resolve failing and type *info* resolve failing.
	// TODO: Better terminology...

	return cTypeUnresolved == 0 && aTypidInfoPending.cItem == 0;
}

TYPID typidFromLiteralk(LITERALK literalk)
{
	if (literalk < LITERALK_Min || literalk >= LITERALK_Max)
	{
		reportIceAndExit("Unexpected literalk value: %d", literalk);
	}

	// TODO: "untyped" numeric literals that shapeshift into whatever context
	//	they are used, like in Go.

	static const TYPID s_mpLiteralkTypid[] =
	{
		TYPID_S32,
		TYPID_F32,
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
		const Type * pType = *(it.pValue);
		debugPrintType(*pType);

		println();
	}
}

#endif