#include "ast.h"
#include "error.h"
#include "global_context.h"
#include "literal.h"
#include "parse.h"
#include "type.h"

void init(Type * pType, TYPEK typek)
{
	pType->typek = typek;
	pType->isInferred = false;	// TODO
	pType->info.size = Type::ComputedInfo::s_unset;
	pType->info.alignment = Type::ComputedInfo::s_unset;

	switch (pType->typek)
	{
		case TYPEK_Named:
		{
			setLexeme(&pType->namedTypeData.ident.lexeme, "");
			pType->namedTypeData.ident.scopeid = SCOPEID_Nil;
		} break;

		case TYPEK_Func:
		{
			init(&pType->funcTypeData.funcType);
		} break;

		case TYPEK_Mod:
		{
			pType->modTypeData.typemod.typemodk = TYPEMODK_Nil;
			pType->modTypeData.typidModified = TYPID_Unresolved;
		} break;

		default:
			AssertNotReached;
			break;
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
	pType->typek = typeSrc.typek;
	pType->isInferred = typeSrc.isInferred;
	pType->info = typeSrc.info;

	switch (pType->typek)
	{
		case TYPEK_Named:
		{
			// META: This assignment is fine since ScopedIdentifier doesn't own any resources. How would I make this more robust
			//	if I wanted to change ScopedIdentifier to allow it to own resources in the future? Writing an initCopy for ident
			//	that just does this assignment seems like too much boilerplate. I would also like to prefer avoiding C++ constructor
			//	craziness. Is there a middle ground?

			pType->namedTypeData = typeSrc.namedTypeData;
		} break;

		case TYPEK_Func:
		{
			initCopy(&pType->funcTypeData.funcType, typeSrc.funcTypeData.funcType);
		} break;

		case TYPEK_Mod:
		{
			pType->modTypeData = typeSrc.modTypeData;
		} break;
	}
}

void dispose(Type * pType)
{
	if (pType->typek == TYPEK_Func)
	{
		dispose(&pType->funcTypeData.funcType);
	}
}

bool isTypeResolved(const Type & type)
{
	switch (type.typek)
	{
		case TYPEK_Named:
			return type.namedTypeData.ident.scopeid != SCOPEID_Nil;

		case TYPEK_Func:
			return isFuncTypeResolved(type.funcTypeData.funcType);

		case TYPEK_Mod:
			return isTypeResolved(type.modTypeData.typidModified);

		default:
			AssertNotReached;
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

bool isTypeResolved(TYPID typid)
{
	return typid >= TYPID_ActualTypesStart;
}

bool isPointerType(const Type & type)
{
	return type.typek == TYPEK_Mod && type.modTypeData.typemod.typemodk == TYPEMODK_Pointer;
}

bool isArrayType(const Type & type)
{
	return type.typek == TYPEK_Mod && type.modTypeData.typemod.typemodk == TYPEMODK_Array;
}

NULLABLE const FuncType * funcTypeFromDefnStmt(const TypeTable & typeTable, const AstFuncDefnStmt & defnStmt)
{
	Assert(isTypeResolved(defnStmt.typidDefn));
	const Type * pType = lookupType(typeTable, defnStmt.typidDefn);
	Assert(pType);

	if (pType->typek == TYPEK_Func)
	{
		return &pType->funcTypeData.funcType;
	}
	else
	{
		return nullptr;
	}
}

PENDINGTYPID registerPendingNamedType(
	TypeTable * pTable,
	Scope * pScope,
	Lexeme ident,
	NULLABLE TYPID * pTypidUpdateOnResolve)
{
	PENDINGTYPID result = PENDINGTYPID(pTable->typesPendingResolution.cItem);

	TypeTable::TypePendingResolve * pTypePending = appendNew(&pTable->typesPendingResolution);
	init(pTypePending, pScope, TYPEK_Named);
	pTypePending->type.namedTypeData.ident.lexeme = ident;
	pTypePending->type.namedTypeData.ident.scopeid = SCOPEID_Nil;		// Not yet known

	if (pTypidUpdateOnResolve)
	{
		pTypePending->apTypidUpdateOnResolve[0] = pTypidUpdateOnResolve;
		pTypePending->cPTypidUpdateOnResolve++;
	}

	return result;
}

PENDINGTYPID registerPendingFuncType(
	TypeTable * pTable,
	Scope * pScope,
	const DynamicArray<PENDINGTYPID> & aPendingTypidParam,
	const DynamicArray<PENDINGTYPID> & aPendingTypidReturn,
	NULLABLE TYPID * pTypidUpdateOnResolve)
{
	PENDINGTYPID result = PENDINGTYPID(pTable->typesPendingResolution.cItem);

	TypeTable::TypePendingResolve * pTypePending = appendNew(&pTable->typesPendingResolution);
	init(pTypePending, pScope, TYPEK_Func);
	
	if (pTypidUpdateOnResolve)
	{
		pTypePending->apTypidUpdateOnResolve[0] = pTypidUpdateOnResolve;
		pTypePending->cPTypidUpdateOnResolve++;
	}

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

PENDINGTYPID registerPendingModType(
	TypeTable * pTable,
	Scope * pScope,
	TypeModifier typemod,
	PENDINGTYPID pendingTypidModified,
	NULLABLE TYPID * pTypidUpdateOnResolve)
{
	Assert(pendingTypidModified <  PENDINGTYPID(pTable->typesPendingResolution.cItem));
	AssertInfo(pendingTypidModified == pTable->typesPendingResolution.cItem - 1, "I think this should be true if we register them in the order I think we do");

	PENDINGTYPID result = PENDINGTYPID(pTable->typesPendingResolution.cItem);

	TypeTable::TypePendingResolve * pTypePending = appendNew(&pTable->typesPendingResolution);
	init(pTypePending, pScope, TYPEK_Mod);
	pTypePending->type.modTypeData.typemod = typemod;

	if (pTypidUpdateOnResolve)
	{
		pTypePending->apTypidUpdateOnResolve[0] = pTypidUpdateOnResolve;
		pTypePending->cPTypidUpdateOnResolve++;
	}

	// NOTE (andrew) This relies on the fact that the modified type should always get added to the pending list (and thus resolved) first
	// TODO (andrew) This is a bit too clever. I am tempted to rewrite how types get registered/resolved... all of this poking into a pointer
	//	is kind of hard to track.

	TypeTable::TypePendingResolve * pTypePendingModified = &pTable->typesPendingResolution[pendingTypidModified];
	pTypePendingModified->pendingTypidModifiedBy = result;

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
	AssertInfo(isTypeResolved(t0), "Asking if these types are equal is an ill-formed question if they aren't fully resolved");
	AssertInfo(isTypeResolved(t1), "Asking if these types are equal is an ill-formed question if they aren't fully resolved");

	if (t0.typek != t1.typek)
		return false;

	switch (t0.typek)
	{
		case TYPEK_Named:
		{
			ScopedIdentifier identDealiased0 = t0.namedTypeData.ident;
			identDealiased0.lexeme = getDealiasedTypeLexeme(identDealiased0.lexeme);

			ScopedIdentifier identDealiased1 = t1.namedTypeData.ident;
			identDealiased1.lexeme = getDealiasedTypeLexeme(identDealiased1.lexeme);

			return scopedIdentEq(identDealiased0, identDealiased1);
		} break;

		case TYPEK_Func:
		{
			return funcTypeEq(t0.funcTypeData.funcType, t1.funcTypeData.funcType);
		} break;

		case TYPEK_Mod:
		{
			// TODO: Refactor into typemodEq(..) ?

			TypeModifier tmod0 = t0.modTypeData.typemod;
			TypeModifier tmod1 = t1.modTypeData.typemod;
			if (tmod0.typemodk != tmod1.typemodk)
				return false;

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
					return false;
			}

			return t0.modTypeData.typidModified == t1.modTypeData.typidModified;
		} break;

		default:
			AssertNotReached;
			return false;
	}
}

uint typeHash(const Type & t)
{
	switch (t.typek)
	{
		case TYPEK_Named:
		{
			ScopedIdentifier identDealiased = t.namedTypeData.ident;
			identDealiased.lexeme = getDealiasedTypeLexeme(identDealiased.lexeme);

			return scopedIdentHash(identDealiased);
		} break;

		case TYPEK_Func:
		{
			return funcTypeHash(t.funcTypeData.funcType);
		} break;
		
		case TYPEK_Mod:
		{
			auto hash = startHash();
			TypeModifier tmod = t.modTypeData.typemod;
			hash = buildHash(&tmod.typemodk, sizeof(tmod.typemodk), hash);

			if (tmod.typemodk == TYPEMODK_Array)
			{
				AssertInfo(tmod.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");

				auto * pIntLit = Down(tmod.pSubscriptExpr, LiteralExpr);
				AssertInfo(pIntLit->literalk == LITERALK_Int, "Parser should enforce this... for now");

				int intVal = intValue(pIntLit);
				hash = buildHash(&intVal, sizeof(intVal), hash);
			}

			return hash;
		} break;

		default:
			AssertNotReached;
			return 0;
	}
}

void init(FuncType * pFuncType)
{
	init(&pFuncType->paramTypids);
	init(&pFuncType->returnTypids);
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

void init(TypeTable * pTable, MeekCtx * pCtx)
{
	pTable->pCtx = pCtx;

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
		init(&type, TYPEK_Named);
		type.namedTypeData.ident = ident;
		type.info.size = size;
		type.info.alignment = size;

		Verify(isTypeResolved(type));

		const bool debugAssertIfAlreadyInTable = true;
		auto ensureResult = ensureInTypeTable(pTable, &type, debugAssertIfAlreadyInTable);
		Assert(ensureResult.typid == typidExpected);
		Assert(ensureResult.typeInfoComputed);
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

	insertBuiltInType(pTable, "string", TYPID_String, 16);	// TODO: Pass actual size once I figure out the in-memory representation.
}

void init(TypeTable::TypePendingResolve * pTypePending, Scope * pScope, TYPEK typek)
{
	init(&pTypePending->type, typek);
	pTypePending->pScope = pScope;
	pTypePending->cPTypidUpdateOnResolve = 0;
	pTypePending->pendingTypidModifiedBy = PENDINGTYPID_Nil;
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

void setTypeInfo(const TypeTable & table, TYPID typid, const Type::ComputedInfo & typeInfo)
{
	Assert(isTypeResolved(typid));
	
	auto ppType = lookupByKeyRaw(table.table, typid);
	Assert(ppType);
	Assert(*ppType);

	(*ppType)->info = typeInfo;
}

EnsureInTypeTableResult ensureInTypeTable(TypeTable * pTable, Type * pType, bool debugAssertIfAlreadyInTable)
{
	AssertInfo(isTypeResolved(*pType), "Shouldn't be inserting an unresolved type into the type table...");

	// SLOW: Could write a combined lookup + insertNew if not found query

	const TYPID * pTypid = lookupByValue(pTable->table, pType);
	if (pTypid)
	{
		Assert(!debugAssertIfAlreadyInTable);

		const Type * pTypeInTable = *lookupByKey(pTable->table, *pTypid);
		Assert(pTypeInTable);

		EnsureInTypeTableResult result;
		result.typid = *pTypid;
		result.typeInfoComputed = pTypeInTable->info.size != Type::ComputedInfo::s_unset;

		return result;
	}

	Type * pTypeCopy = allocate(&pTable->typeAlloc);
	initCopy(pTypeCopy, *pType);

	TYPID typidInsert = pTable->typidNext;
	pTable->typidNext = TYPID(pTable->typidNext + 1);

	Verify(insert(&pTable->table, typidInsert, pTypeCopy));

	EnsureInTypeTableResult result;
	result.typid = typidInsert;

	if (pTypeCopy->info.size == Type::ComputedInfo::s_unset)
	{
		result.typeInfoComputed = tryComputeTypeInfo(*pTable, typidInsert);
	}
	else
	{
		result.typeInfoComputed = true;
	}

	return result;
}

Type::ComputedInfo tryComputeTypeInfoAndSetMemberOffsets(const MeekCtx & ctx, SCOPEID scopeid, const DynamicArray<AstNode *> & apVarDeclStmt, bool includeEndPadding)
{
	Type::ComputedInfo result;
	result.size = Type::ComputedInfo::s_unset;
	result.alignment = Type::ComputedInfo::s_unset;

	bool allMembersCounted = true;
	u32 sizeWithPadding = 0;
	u32 alignmentMax = 1;

	for (int iVarDeclStmt = 0; iVarDeclStmt < apVarDeclStmt.cItem; iVarDeclStmt++)
	{
		auto * pVarDeclStmt = Down(apVarDeclStmt[iVarDeclStmt], VarDeclStmt);
		if (!isTypeResolved(pVarDeclStmt->typidDefn))
		{
			allMembersCounted = false;
			break;
		}

		const Type * pTypeVarDecl = lookupType(*ctx.pTypeTable, pVarDeclStmt->typidDefn);
		Assert(pTypeVarDecl);

		u32 sizeMember = pTypeVarDecl->info.size;
		u32 alignmentMember = pTypeVarDecl->info.alignment;

		Assert(Iff(sizeMember == Type::ComputedInfo::s_unset, alignmentMember == Type::ComputedInfo::s_unset));
		if (sizeMember == Type::ComputedInfo::s_unset)
		{
			allMembersCounted = false;
			break;
		}

		// Using C-like struct packing.
		// http://www.catb.org/esr/structure-packing/

		if (iVarDeclStmt > 0)
		{
			int bytesPastAlignment = sizeWithPadding % alignmentMember;
			int padding = (bytesPastAlignment == 0) ? 0 : alignmentMember - bytesPastAlignment;
			sizeWithPadding += padding;
		}

		SymbolInfo symbInfoVar = lookupVarSymbol(
			*ctx.mpScopeidPScope[scopeid],
			pVarDeclStmt->ident.lexeme,
			FSYMBQ_IgnoreParent
		);

		updateVarSymbolOffset(*ctx.mpScopeidPScope[scopeid], pVarDeclStmt->ident.lexeme, sizeWithPadding);

		sizeWithPadding += sizeMember;
		alignmentMax = Max(alignmentMax, alignmentMember);
	}

	if (allMembersCounted)
	{
		result.alignment = alignmentMax;

		int bytesPastAlignment = sizeWithPadding % result.alignment;

		if (includeEndPadding)
		{
			int endPadding = (bytesPastAlignment == 0) ? 0 : result.alignment - bytesPastAlignment;
			sizeWithPadding += endPadding;
		}

		result.size = sizeWithPadding;
	}

	return result;
}

bool tryComputeTypeInfo(const TypeTable & typeTable, TYPID typid)
{
	Assert(isTypeResolved(typid));

	const Type * pType = lookupType(typeTable, typid);
	Assert(pType);
	AssertInfo(pType->info.size == Type::ComputedInfo::s_unset, "Trying to resolve pending type info that has already been resolved?");

	MeekCtx * pCtx = typeTable.pCtx;

	Type::ComputedInfo typeInfoResult;
	typeInfoResult.size = Type::ComputedInfo::s_unset;
	typeInfoResult.alignment = Type::ComputedInfo::s_unset;

	switch (pType->typek)
	{
		case TYPEK_Named:
		{
			SymbolInfo symbInfo =
				lookupTypeSymbol(
					*pCtx->mpScopeidPScope[pType->namedTypeData.ident.scopeid],
					pType->namedTypeData.ident.lexeme);

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
				typeInfoResult = tryComputeTypeInfoAndSetMemberOffsets(*pCtx, pStructDefn->scopeid, pStructDefn->apVarDeclStmt);
			}
		} break;

		case TYPEK_Func:
		{
			typeInfoResult.size = MeekCtx::s_cBytePtr;
			typeInfoResult.alignment = MeekCtx::s_cBytePtr;
		} break;

		case TYPEK_Mod:
		{
			Assert(isTypeResolved(pType->modTypeData.typidModified));

			TypeModifier typeMod = pType->modTypeData.typemod;
			switch (typeMod.typemodk)
			{
				case TYPEMODK_Array:
				{
					AssertInfo(typeMod.pSubscriptExpr->astk == ASTK_LiteralExpr, "Parser should enforce this... for now");

					auto * pIntLit = Down(typeMod.pSubscriptExpr, LiteralExpr);
					AssertInfo(pIntLit->literalk == LITERALK_Int, "Parser should enforce this... for now");

					int intVal = intValue(pIntLit);

					// TODO: look up if the modified type already has its info computed, and if not add it to the set of things that
					//	need to be computed and then return false?

					const Type * pTypeModified = lookupType(typeTable, pType->modTypeData.typidModified);
					Assert(pTypeModified);

					if (pTypeModified->info.size != Type::ComputedInfo::s_unset)
					{
						typeInfoResult.size = intVal * pTypeModified->info.size;
						typeInfoResult.alignment = pTypeModified->info.alignment;
					}
					else
					{
						// HMM: For now we just fail and wait for the next attempt. We could possibly try to recursively call
						//	this on the pType->modTypeData.typidModified, but if the modified type info is unset, then it
						//	probably just failed such a call, and re-trying probably wouldn't be successful.
					}
				} break;

				case TYPEMODK_Pointer:
				{
					typeInfoResult.size = MeekCtx::s_cBytePtr;
					typeInfoResult.alignment = MeekCtx::s_cBytePtr;
				} break;

				default:
					AssertNotReached;
					break;
			}
		} break;

		default:
			AssertNotReached;
			break;
	}

	Assert(typeInfoResult.alignment <= typeInfoResult.size);
	Assert(typeInfoResult.size % typeInfoResult.alignment == 0);
	Assert(Iff(typeInfoResult.size == Type::ComputedInfo::s_unset, typeInfoResult.alignment == Type::ComputedInfo::s_unset));
	if (typeInfoResult.size == Type::ComputedInfo::s_unset)
		return false;

	setTypeInfo(typeTable, typid, typeInfoResult);

	return true;
}

bool tryResolveAllTypes(TypeTable * pTable)
{
	auto tryResolvePendingType = [](TypeTable::TypePendingResolve * pTypePending)
	{
		switch (pTypePending->type.typek)
		{
			case TYPEK_Named:
			{
				SymbolInfo symbInfo = lookupTypeSymbol(*pTypePending->pScope, pTypePending->type.namedTypeData.ident.lexeme);
				if (symbInfo.symbolk == SYMBOLK_Nil)
					return false;

				pTypePending->type.namedTypeData.ident.scopeid = scopeidFromSymbolInfo(symbInfo);
				return true;
			} break;

			case TYPEK_Func:
			{
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
			} break;

			case TYPEK_Mod:
			{
				return isTypeResolved(pTypePending->type.modTypeData.typidModified);
			} break;

			default:
				AssertNotReached;
				return false;
		}
	};

	MeekCtx * pCtx = pTable->pCtx;

	// Resolve named types (and eagerly resolve type infos where we can)

	DynamicArray<TYPID> aTypidComputePending;
	init(&aTypidComputePending);
	Defer(dispose(&aTypidComputePending));

	int cTypeUnresolved = 0;
	{
		for (int i = 0; i < pTable->typesPendingResolution.cItem; i++)
		{
			TypeTable::TypePendingResolve * pTypePending = &pTable->typesPendingResolution[i];
			if (tryResolvePendingType(pTypePending))
			{
				auto ensureResult = ensureInTypeTable(pTable, &pTypePending->type);
				TYPID typid = ensureResult.typid;
				for (int iTypidUpdate = 0; iTypidUpdate < pTypePending->cPTypidUpdateOnResolve; iTypidUpdate++)
				{
					TYPID * pTypidUpdate = pTypePending->apTypidUpdateOnResolve[iTypidUpdate];
					*pTypidUpdate = typid;
				}

				if (pTypePending->pendingTypidModifiedBy != PENDINGTYPID_Nil)
				{
					// NOTE (andrew) This relies on the fact that the modified type should always get added to the pending list (and thus resolved) first
					// TODO (andrew) This is a bit too clever. I am tempted to rewrite how types get registered/resolved... all of this poking into a pointer
					//	is kind of hard to track.

					Assert(pTypePending->pendingTypidModifiedBy < PENDINGTYPID(pTable->typesPendingResolution.cItem));
					AssertInfo(pTypePending->pendingTypidModifiedBy > PENDINGTYPID(i), "This should be true due to the insertion order into this list");

					TypeTable::TypePendingResolve * pTypePendingModifiedBy = &pTable->typesPendingResolution[pTypePending->pendingTypidModifiedBy];

					Assert(pTypePendingModifiedBy->type.typek == TYPEK_Mod);
					Assert(!isTypeResolved(pTypePendingModifiedBy->type.modTypeData.typidModified));

					pTypePendingModifiedBy->type.modTypeData.typidModified = typid;
				}

				if (!ensureResult.typeInfoComputed)
				{
					if (!tryComputeTypeInfo(*pTable, typid))
					{
						append(&aTypidComputePending, typid);
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

		dispose(&pTable->typesPendingResolution);
	}

	// Resolve type infos we weren't able to resolve eagerly

	bool madeProgress = true;
	while (madeProgress)
	{
		madeProgress = false;

		for (int i = aTypidComputePending.cItem - 1; i >= 0; i--)
		{
			TYPID typidInfoPending = aTypidComputePending[i];
			Assert(isTypeResolved(typidInfoPending));

			const Type * pType = lookupType(*pTable, typidInfoPending);
			if (pType->info.size != Type::ComputedInfo::s_unset)
			{
				// NOTE (andrew) This can happen when multiple copies of the same typid are added to
				//	this list. This makes it kind of @Slow, we should probably use a HashSet instead
				//	of a DynamicArray for the pending typeinfo typid's. But due to the importance
				//	of the order of elements in that list, and the partial results that it holds, this
				//	would be a challenge to do.

				unorderedRemove(&aTypidComputePending, i);
			}
			else if (tryComputeTypeInfo(*pTable, typidInfoPending))
			{
				madeProgress = true;
				unorderedRemove(&aTypidComputePending, i);
			}
			else
			{
				// Still not enough info to resolve. Keep around for next iteration.
			}
		}
	}

	// TODO: Better distinction between type resolve failing and type *info* resolve failing.
	// TODO: Better terminology...

	return cTypeUnresolved == 0 && aTypidComputePending.cItem == 0;
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

bool isAnyInt(TYPID typid)
{
	return typid >= TYPID_AnyIntStart && typid <= TYPID_AnyIntEnd;
}

bool isAnyFloat(TYPID typid)
{
	return typid == TYPID_F32 || typid == TYPID_F64;
}

#if DEBUG

#include "print.h"

//void debugPrintType(const Type& type)
//{
//	for (int i = 0; i < type.aTypemods.cItem; i++)
//	{
//		TypeModifier tmod = type.aTypemods[i];
//
//		switch (tmod.typemodk)
//		{
//			case TYPEMODK_Array:
//			{
//				// TODO: Change this when arbitrary compile time expressions are allowed
//				//	as the array size expression
//
//				print("[");
//
//				Assert(tmod.pSubscriptExpr && tmod.pSubscriptExpr->astk == ASTK_LiteralExpr);
//
//				auto* pNode = Down(tmod.pSubscriptExpr, LiteralExpr);
//				Assert(pNode->literalk == LITERALK_Int);
//
//				int value = intValue(pNode);
//				Assert(pNode->isValueSet);
//				AssertInfo(!pNode->isValueErroneous, "Just for sake of testing... in reality if it was erroneous then we don't really care/bother about the symbol table");
//
//				printfmt("%d", value);
//
//				print("]");
//			} break;
//
//			case TYPEMODK_Pointer:
//			{
//				print("^");
//			} break;
//		}
//	}
//
//	if (!type.isFuncType)
//	{
//		print(type.namedTypeData.ident.lexeme.strv);
//		printfmt(" (scopeid: %d)", type.namedTypeData.ident.scopeid);
//	}
//	else
//	{
//		// TODO: :) ... will need to set up tab/new line stuff so that it
//		//	can nest arbitrarily. Can't be bothered right now!!!
//
//		print("(function type... CBA to print LOL)");
//	}
//}

//void debugPrintTypeTable(const TypeTable& typeTable)
//{
//	print("===============\n");
//	print("=====TYPES=====\n");
//	print("===============\n\n");
//
//	for (auto it = iter(typeTable.table); it.pValue; iterNext(&it))
//	{
//		const Type * pType = *(it.pValue);
//		debugPrintType(*pType);
//
//		println();
//	}
//}

#endif