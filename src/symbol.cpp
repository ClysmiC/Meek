#include "symbol.h"

#include "ast.h"
#include "error.h"
#include "global_context.h"
#include "literal.h"
#include "print.h"
#include "token.h"
#include "type.h"


u32 scopedIdentHash(const ScopedIdentifier & ident)
{
	Assert(ident.scopeid != SCOPEID_Nil);

	u32 hashLexeme = lexemeHash(ident.lexeme);

	return combineHash(hashLexeme, ident.scopeid);
}

bool scopedIdentEq(const ScopedIdentifier & ident0, const ScopedIdentifier & ident1)
{
	Assert(ident0.scopeid != SCOPEID_Nil);
	Assert(ident1.scopeid != SCOPEID_Nil);

	return lexemeEq(ident0.lexeme, ident1.lexeme) && ident0.scopeid == ident1.scopeid;
}

void init(Scope * pScope, SCOPEID scopeid, SCOPEK scopek, Scope * pScopeParent)
{
	Assert(Iff(!pScopeParent, scopek == SCOPEK_BuiltIn));
	Assert(Iff(scopeid == SCOPEID_BuiltIn, scopek == SCOPEK_BuiltIn));

	pScope->pScopeParent = pScopeParent;
	pScope->id = scopeid;
	pScope->scopek = scopek;
	
	init(&pScope->symbolsDefined, lexemeHash, lexemeEq);

	if (scopek == SCOPEK_BuiltIn)
	{
		auto insertBuiltInTypeSymbol = [](Scope * pScope, const char * strIdent, TYPID typid)
		{
			Lexeme lexeme;
			setLexeme(&lexeme, strIdent);

			SymbolInfo symbInfo;
			symbInfo.symbolk = SYMBOLK_BuiltInType;
			symbInfo.builtInData.typid = typid;

			Assert(!lookup(pScope->symbolsDefined, lexeme));

			DynamicArray<SymbolInfo> * paSymbInfo = insertNew(&pScope->symbolsDefined, lexeme);
			init(paSymbInfo);
			append(paSymbInfo, symbInfo);
		};

		// TODO: make void a symbol?

		insertBuiltInTypeSymbol(pScope, "int", TYPID_S32);
		insertBuiltInTypeSymbol(pScope, "s8", TYPID_S8);
		insertBuiltInTypeSymbol(pScope, "s16", TYPID_S16);
		insertBuiltInTypeSymbol(pScope, "s32", TYPID_S32);
		insertBuiltInTypeSymbol(pScope, "s64", TYPID_S64);

		insertBuiltInTypeSymbol(pScope, "uint", TYPID_U32);
		insertBuiltInTypeSymbol(pScope, "u8", TYPID_U8);
		insertBuiltInTypeSymbol(pScope, "u16", TYPID_U16);
		insertBuiltInTypeSymbol(pScope, "u32", TYPID_U32);
		insertBuiltInTypeSymbol(pScope, "u64", TYPID_U64);

		insertBuiltInTypeSymbol(pScope, "float", TYPID_F32);
		insertBuiltInTypeSymbol(pScope, "f32", TYPID_F32);
		insertBuiltInTypeSymbol(pScope, "f64", TYPID_F64);

		insertBuiltInTypeSymbol(pScope, "bool", TYPID_Bool);

		insertBuiltInTypeSymbol(pScope, "string", TYPID_String);
	}
}

NULLABLE Scope * owningFuncTopLevelScope(Scope * pScope)
{
	Scope * pScopeCandidate = pScope;

	while (pScopeCandidate)
	{
		if (pScopeCandidate->scopek == SCOPEK_FuncTopLevel)
		{
			return pScopeCandidate;
		}

		pScopeCandidate = pScopeCandidate->pScopeParent;
	}
	
	return nullptr;
}

NULLABLE Scope * owningFuncTopLevelScope(MeekCtx * pCtx, SCOPEID scopeid)
{
	return owningFuncTopLevelScope(pCtx->mpScopeidPScope[scopeid]);
}

void defineSymbol(MeekCtx * pCtx, Scope * pScope, const Lexeme & lexeme, const SymbolInfo & symbInfo)
{
	DynamicArray<SymbolInfo> * paSymbInfo = lookup(pScope->symbolsDefined, lexeme);
	if (!paSymbInfo)
	{
		paSymbInfo = insertNew(&pScope->symbolsDefined, lexeme);
		init(paSymbInfo);
	}

	// NOTE (andrew) No attempt is made to detect redefinitions here. That is done seperately, per-scope,
	//	in the resolve pass. A special exception is made for main, since it is a program-wide concern.

	if (symbInfo.symbolk == SYMBOLK_Func && lexeme.strv == "main")
	{
		if (pScope->id != SCOPEID_Global)
		{
			AssertTodo;		// Report error that main can only be defined at global level
		}
		else if (pCtx->funcidMain != FUNCID_Nil)
		{
			AssertTodo;		// Report error that multiple functions named main not allowed
		}
		else
		{
			pCtx->funcidMain = symbInfo.funcData.pFuncDefnStmt->funcid;
		}
	}

	append(paSymbInfo, symbInfo);
}

bool auditDuplicateSymbols(MeekCtx * pCtx, Scope * pScope)
{
	bool duplicateFound = false;
	for (auto it = iter(pScope->symbolsDefined); it.pValue; iterNext(&it))
	{
		Lexeme lexeme = *it.pKey;
		DynamicArray<SymbolInfo> * paSymbInfo = it.pValue;

		int cVar = 0;
		int cType = 0;

		for (int iSymbInfo = 0; iSymbInfo < paSymbInfo->cItem; iSymbInfo++)
		{
			SymbolInfo symbInfo = (*paSymbInfo)[iSymbInfo];

			switch (symbInfo.symbolk)
			{
				case SYMBOLK_Var:
				{
					cVar++;

					if (cVar > 1)
					{
						// TODO: better error story

						print("Duplicate variable ");
						print(lexeme.strv);
						println();

						duplicateFound = true;

						remove(paSymbInfo, iSymbInfo);
						iSymbInfo--;
					}
				}

				case SYMBOLK_Struct:
				{
					if (cType > 1)
					{
						// TODO: better error story

						print("Duplicate type ");
						print(lexeme.strv);
						println();

						duplicateFound = true;

						remove(paSymbInfo, iSymbInfo);
						iSymbInfo--;
					}
				}

				case SYMBOLK_Func:
				{
					// @Slow

					for (int iSymbInfoOther = iSymbInfo + 1; iSymbInfoOther < paSymbInfo->cItem; iSymbInfoOther++)
					{
						SymbolInfo symbInfoOther = (*paSymbInfo)[iSymbInfoOther];
						if (symbInfoOther.symbolk != SYMBOLK_Func)
							continue;

						if (symbInfo.funcData.pFuncDefnStmt->typidDefn == symbInfoOther.funcData.pFuncDefnStmt->typidDefn)
						{
							// TODO: better error story

							print("Duplicate function with same signature ");
							print(lexeme.strv);
							println();

							duplicateFound = true;

							remove(paSymbInfo, iSymbInfo);
							iSymbInfo--;
						}
					}
				}
			}
		}
	}

	return !duplicateFound;
}

void computeScopedVariableOffsets(MeekCtx * pCtx, Scope * pScope)
{
	// @Slow - Between gathering and extracting, there are lots of dynamic arrays flying around here. Could
	//	probably be improved.

	// NOTE - For now, I am treating each local scope as if it is a single struct and re-using the struct size/offset
	//	computation code.

	// Gather all vardecls

	DynamicArray<SymbolInfo> aSymbInfo;
	init(&aSymbInfo);
	Defer(dispose(&aSymbInfo));

	lookupAllVars(*pScope, &aSymbInfo, FSYMBQ_IgnoreParent | FSYMBQ_SortVarseqid);
	Assert(Implies(pScope->id == SCOPEID_BuiltIn, aSymbInfo.cItem == 0));

	int cParam = 0;
	if (pScope->scopek == SCOPEK_FuncTopLevel)
	{
		// Compute param offsets

		// If the scope is a function, all parameters get treated as if they are in their own struct (separate from the locals).
		//	This is to make things agree with how these byte offsets get interpreted by the VM.

		for (int iSymbInfo = 0; iSymbInfo < aSymbInfo.cItem; iSymbInfo++)
		{
			VARDECLK vardeclk = aSymbInfo[iSymbInfo].varData.pVarDeclStmt->vardeclk;
			if (vardeclk == VARDECLK_Local)
				break;

			Assert(vardeclk == VARDECLK_Param);
			cParam++;
		}

		if (cParam > 0)
		{

			DynamicArray<AstNode *> apVarDeclParam;
			initExtract(&apVarDeclParam, aSymbInfo.pBuffer, cParam, offsetof(SymbolInfo, varData.pVarDeclStmt));
			Defer(dispose(&apVarDeclParam));

			const bool c_includeEndPadding = false;
			Type::ComputedInfo fakeTypeInfo = tryComputeTypeInfoAndSetMemberOffsets(*pCtx, pScope->id, apVarDeclParam, c_includeEndPadding);
			Assert(fakeTypeInfo.size != Type::ComputedInfo::s_unset);
			Assert(fakeTypeInfo.alignment != Type::ComputedInfo::s_unset);

			pScope->funcTopLevelData.cByteParam = fakeTypeInfo.size;
		}
		else
		{
			pScope->funcTopLevelData.cByteParam = 0;
		}
	}


	// Compute local var offsets

	int cLocal = aSymbInfo.cItem - cParam;
	DynamicArray<AstNode *> apVarDecl;
	initExtract(&apVarDecl, aSymbInfo.pBuffer + cParam, cLocal, offsetof(SymbolInfo, varData.pVarDeclStmt));
	Defer(dispose(&apVarDecl));

	bool includeEndPadding = false;
	Type::ComputedInfo fakeTypeInfo = tryComputeTypeInfoAndSetMemberOffsets(*pCtx, pScope->id, apVarDecl, includeEndPadding);

	Assert(fakeTypeInfo.size != Type::ComputedInfo::s_unset);
	Assert(fakeTypeInfo.alignment != Type::ComputedInfo::s_unset);

	switch (pScope->scopek)
	{
		case SCOPEK_Global:
		{
			pScope->globalData.cByteGlobalVariable = fakeTypeInfo.size;
		} break;

		case SCOPEK_BuiltIn:
		{
			Assert(fakeTypeInfo.size == 0);
		} break;

		case SCOPEK_StructDefn:
		{
			// @Slow - pretty sure this is redundant, since we are already computing
			//	the size info for the type itself in an earlier phase?

			AssertInfo(false, "Debugging... do I reach this case?");
			pScope->structDefnData.cByteMember = fakeTypeInfo.size;
		} break;

		case SCOPEK_FuncTopLevel:
		{
			pScope->funcTopLevelData.cByteLocalVariable = fakeTypeInfo.size;
		} break;

		case SCOPEK_FuncInner:
		{
			pScope->funcInnerData.cByteLocalVariable = fakeTypeInfo.size;
		} break;

		default:
			AssertNotReached;
			break;
	}
}

int compareVarseqid(const SymbolInfo & s0, const SymbolInfo & s1)
{
	Assert(s0.symbolk == SYMBOLK_Var);
	Assert(s1.symbolk == SYMBOLK_Var);

	return s0.varData.pVarDeclStmt->varseqid - s1.varData.pVarDeclStmt->varseqid;
}

SCOPEID scopeidFromSymbolInfo(const SymbolInfo & symbInfo)
{
	switch (symbInfo.symbolk)
	{
		case SYMBOLK_Var:
		{
			return symbInfo.varData.pVarDeclStmt->ident.scopeid;
		} break;

		case SYMBOLK_Struct:
		{
			return symbInfo.structData.pStructDefnStmt->ident.scopeid;
		} break;

		case SYMBOLK_Func:
		{
			return symbInfo.funcData.pFuncDefnStmt->ident.scopeid;
		} break;

		case SYMBOLK_BuiltInType:
		{
			return SCOPEID_BuiltIn;
		} break;

		default:
		{
			AssertNotReached;
			return SCOPEID_Nil;
		}
	}
}

SymbolInfo lookupVarSymbol(const Scope & scope, const Lexeme & lexeme, GRFSYMBQ grfsymbq)
{
	Assert((grfsymbq & FSYMBQ_IgnoreVars) == 0);
	grfsymbq |= (FSYMBQ_IgnoreTypes | FSYMBQ_IgnoreFuncs);

	// @Slow - using dynamic array when we know we have a fixed number of results, just to
	//	conform to the API.

	DynamicArray<SymbolInfo> aSymbInfo;
	init(&aSymbInfo);
	Defer(dispose(&aSymbInfo));

	lookupSymbol(scope, lexeme, &aSymbInfo, grfsymbq);

	Assert(aSymbInfo.cItem <= 1);

	if (aSymbInfo.cItem == 1)
	{
		const SymbolInfo & symbInfo = aSymbInfo[0];
		Assert(symbInfo.symbolk == SYMBOLK_Var);

		bool ignoreResult = (grfsymbq & FSYMBQ_IgnoreParamVars) && symbInfo.varData.pVarDeclStmt->vardeclk == VARDECLK_Param;
		if (!ignoreResult)
		{
			return symbInfo;
		}
	}

	SymbolInfo resultNil;
	resultNil.symbolk = SYMBOLK_Nil;
	return resultNil;
}

SymbolInfo lookupVarSymbol(MeekCtx * pCtx, const AstVarDeclStmt & varDecl)
{
	SymbolInfo result = lookupVarSymbol(*pCtx->mpScopeidPScope[varDecl.ident.scopeid], varDecl.ident.lexeme, FSYMBQ_IgnoreParent);
	Assert(result.symbolk == SYMBOLK_Var);

	return result;
}

SymbolInfo lookupTypeSymbol(const Scope & scope, const Lexeme & lexeme, GRFSYMBQ grfsymbq)
{
	Assert((grfsymbq & FSYMBQ_IgnoreTypes) == 0);
	grfsymbq |= (FSYMBQ_IgnoreVars | FSYMBQ_IgnoreFuncs);

	// @Slow - using dynamic array when we know we have a fixed number of results, just to
	//	conform to the API.

	DynamicArray<SymbolInfo> aSymbInfo;
	init(&aSymbInfo);
	Defer(dispose(&aSymbInfo));

	lookupSymbol(scope, lexeme, &aSymbInfo, grfsymbq);

	Assert(aSymbInfo.cItem <= 1);

	if (aSymbInfo.cItem == 1)
	{
		return aSymbInfo[0];
	}
	else
	{
		SymbolInfo resultNil;
		resultNil.symbolk = SYMBOLK_Nil;
		return resultNil;
	}
}

void lookupFuncSymbol(const Scope & scope, const Lexeme & lexeme, DynamicArray<SymbolInfo> * poResult, GRFSYMBQ grfsymbq)
{
	Assert((grfsymbq & FSYMBQ_IgnoreFuncs) == 0);
	grfsymbq |= (FSYMBQ_IgnoreVars | FSYMBQ_IgnoreTypes);

	lookupSymbol(scope, lexeme, poResult, grfsymbq);
}

void lookupSymbol(const Scope & scope, const Lexeme & lexeme, DynamicArray<SymbolInfo> * poResult, GRFSYMBQ grfsymbq)
{
	const Scope * pScope = &scope;
	while (pScope)
	{
		DynamicArray<SymbolInfo> * paSymbInfoMatch = lookup(pScope->symbolsDefined, lexeme);
		if (paSymbInfoMatch)
		{
			for (int iSymbInfoMatch = 0; iSymbInfoMatch < paSymbInfoMatch->cItem; iSymbInfoMatch++)
			{
				SymbolInfo symbInfoMatch = (*paSymbInfoMatch)[iSymbInfoMatch];

				switch (symbInfoMatch.symbolk)
				{
					case SYMBOLK_Var:
					{
						if (grfsymbq & FSYMBQ_IgnoreVars)
							continue;
					} break;

					case SYMBOLK_Func:
					{
						if (grfsymbq & FSYMBQ_IgnoreFuncs)
							continue;
					} break;

					case SYMBOLK_Struct:
					case SYMBOLK_BuiltInType:
					{
						if (grfsymbq & FSYMBQ_IgnoreTypes)
							continue;
					} break;

					default:
					{
						reportIceAndExit("Unexpected symbolk: %d", symbInfoMatch.symbolk);
					} break;
				}

				append(poResult, symbInfoMatch);
			}
		}

		pScope = (grfsymbq & FSYMBQ_IgnoreParent) ? nullptr : pScope->pScopeParent;
	}
}

void lookupAllVars(const Scope & scope, DynamicArray<SymbolInfo> * poResult, GRFSYMBQ grfsymbq)
{
	bool shouldIgnoreParams = grfsymbq & FSYMBQ_IgnoreParamVars;
	bool shouldIgnoreParent = grfsymbq & FSYMBQ_IgnoreParent;
	bool shouldSortByVarseqid = grfsymbq & FSYMBQ_SortVarseqid;

	for (auto it = iter(scope.symbolsDefined); it.pKey; iterNext(&it))
	{
		for (int iSymb = 0; iSymb < it.pValue->cItem; iSymb++)
		{
			SymbolInfo symbInfo = (*it.pValue)[iSymb];
			if (symbInfo.symbolk != SYMBOLK_Var)
				continue;

			if (shouldIgnoreParams && symbInfo.varData.pVarDeclStmt->vardeclk == VARDECLK_Param)
				continue;

			append(poResult, symbInfo);
		}
	}

	if (!shouldIgnoreParent)
	{
		lookupAllVars(*scope.pScopeParent, poResult, grfsymbq);
	}

	if (shouldSortByVarseqid)
	{
		bubbleSort(poResult->pBuffer, poResult->cItem, &compareVarseqid);
	}
}

void updateVarSymbolOffset(const Scope & scope, const Lexeme & lexeme, u32 cByteOffset)
{
	DynamicArray<SymbolInfo> * paSymbInfoMatch = lookup(scope.symbolsDefined, lexeme);
	Assert(paSymbInfoMatch);

	bool updated = false;
	for (int iSymbInfoMatch = 0; iSymbInfoMatch < paSymbInfoMatch->cItem; iSymbInfoMatch++)
	{
		SymbolInfo * pSymbInfoMatch = &(*paSymbInfoMatch)[iSymbInfoMatch];
		if (pSymbInfoMatch->symbolk != SYMBOLK_Var)
			continue;

		pSymbInfoMatch->varData.byteOffset = cByteOffset;
		updated = true;
	}

	Assert(updated);
}

uintptr cByteLocalVars(const Scope & scope)
{
	switch (scope.scopek)
	{
		case SCOPEK_FuncTopLevel:		return scope.funcTopLevelData.cByteLocalVariable;
		case SCOPEK_FuncInner:			return scope.funcInnerData.cByteLocalVariable;
		default:						return 0;
	}
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
