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

void defineSymbol(Scope * pScope, const Lexeme & lexeme, const SymbolInfo & symbInfo)
{
	DynamicArray<SymbolInfo> * paSymbInfo = lookup(pScope->symbolsDefined, lexeme);
	if (!paSymbInfo)
	{
		paSymbInfo = insertNew(&pScope->symbolsDefined, lexeme);
		init(paSymbInfo);
	}

	// NOTE (andrew) No attempt is made to detect redefinitions here. We do this in
	//	a separate pass once all types have been resolved.

	append(paSymbInfo, symbInfo);
}

bool auditDuplicateSymbols(Scope * pScope)
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
	// Gather vardecls

	DynamicArray<SymbolInfo> aSymbInfo;
	init(&aSymbInfo);
	Defer(dispose(&aSymbInfo));

	lookupAllVars(*pScope, &aSymbInfo, FSYMBQ_IgnoreParent);

	Assert(Implies(pScope->id == SCOPEID_BuiltIn, aSymbInfo.cItem == 0));

	// For now, I am treating each local scope as if it is a single struct and re-using the struct size/offset
	//	computation code.

	// @Slow

	DynamicArray<AstNode *> apVarDecl;
	initExtract(&apVarDecl, aSymbInfo, offsetof(SymbolInfo, varData.pVarDeclStmt));
	Defer(dispose(&apVarDecl));

	bool includeEndPadding = false;
	Type::ComputedInfo fakeTypeInfo = tryComputeTypeInfoAndSetMemberOffsets(*pCtx, pScope->id, apVarDecl, includeEndPadding);

	Assert(fakeTypeInfo.size != Type::ComputedInfo::s_unset);
	Assert(fakeTypeInfo.alignment != Type::ComputedInfo::s_unset);

	pScope->cByteVariables = fakeTypeInfo.size;
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
		return aSymbInfo[0];
	}
	else
	{
		SymbolInfo resultNil;
		resultNil.symbolk = SYMBOLK_Nil;
		return resultNil;
	}
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
	for (auto it = iter(scope.symbolsDefined); it.pKey; iterNext(&it))
	{
		for (int iSymb = 0; iSymb < it.pValue->cItem; iSymb++)
		{
			SymbolInfo symbInfo = (*it.pValue)[iSymb];
			if (symbInfo.symbolk != SYMBOLK_Var)
				continue;

			append(poResult, symbInfo);
		}
	}

	if ((grfsymbq & FSYMBQ_IgnoreParent) == 0)
	{
		lookupAllVars(*scope.pScopeParent, poResult, grfsymbq);
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
