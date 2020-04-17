#include "resolve.h"

#include "ast.h"
#include "error.h"
#include "parse.h"
#include "print.h"
#include "symbol.h"
#include "type.h"

#define SetBubbleIfUnresolved(typid) do { if (!isTypeResolved(typid)) (typid) = TYPID_BubbleError; } while(0)

void init(ResolvePass * pPass, Parser * pParser)
{
	pPass->varseqidSeen = VARSEQID_Zero;
	init(&pPass->unresolvedIdents);
	init(&pPass->fnCtxStack);

	pPass->hadError = false;
	pPass->cNestedBreakable = 0;

	pPass->scopeidCur = SCOPEID_Global;

	pPass->pMpScopeidPScope = &pParser->mpScopeidPScope;
	pPass->pTypeTable = &pParser->typeTable;
}

void auditDuplicateSymbols(ResolvePass * pPass)
{
	if (!auditDuplicateSymbols((*pPass->pMpScopeidPScope)[pPass->scopeidCur]))
	{
		pPass->hadError = true;
	}
}

TYPID resolveExpr(ResolvePass * pPass, AstNode * pNode)
{
	Assert(category(pNode->astk) == ASTCATK_Expr);

	TYPID typidResult = TYPID_Unresolved;

	switch (pNode->astk)
	{
		case ASTK_BinopExpr:
		{
			// NOTE: Assume that binops can only happen between
			//	exact type matches, and the result becomes that type.
			//	Will change later?

			auto * pExpr = Down(pNode, BinopExpr);

			TYPID typidLhs = resolveExpr(pPass, pExpr->pLhsExpr);
			TYPID typidRhs = resolveExpr(pPass, pExpr->pRhsExpr);

			if (!isTypeResolved(typidLhs) || !isTypeResolved(typidRhs))
			{
				typidResult = TYPID_BubbleError;
				goto LEndSetTypidAndReturn;
			}

			if (typidLhs != typidRhs)
			{
				// TODO: report error

				printfmt("Binary operator type mismatch. L: %d, R: %d\n", typidLhs, typidRhs);
				typidResult = TYPID_TypeError;
				goto LEndSetTypidAndReturn;
			}

			typidResult = typidLhs;
		} break;

		case ASTK_GroupExpr:
		{
			// This could technically still be an lvalue, but it would let you do stuff like this:
			//	(a) = 15;
			// This is semantically equivalent to:
			//	a = 15;
			// But it's kind of dumb, so I'm just going to say that groups are NOT lvalues.

			auto * pExpr = Down(pNode, GroupExpr);

			typidResult = resolveExpr(pPass, pExpr->pExpr);
			SetBubbleIfUnresolved(typidResult);
		} break;

		case ASTK_LiteralExpr:
		{
			auto * pExpr = Down(pNode, LiteralExpr);

			typidResult = typidFromLiteralk(pExpr->literalk);
			SetBubbleIfUnresolved(typidResult);
		} break;

		case ASTK_UnopExpr:
		{
			auto * pExpr = Down(pNode, UnopExpr);

			TYPID typidExpr = resolveExpr(pPass, pExpr->pExpr);

			if (!isTypeResolved(typidExpr))
			{
				typidResult = TYPID_BubbleError;
				goto LEndSetTypidAndReturn;
			}

			switch (pExpr->pOp->tokenk)
			{
				case TOKENK_Plus:
				case TOKENK_Minus:
				case TOKENK_Bang:
				{
					typidResult = typidExpr;
				} break;

				case TOKENK_Carat:
				{
					const Type * pTypeExpr = lookupType(*pPass->pTypeTable, typidExpr);
					Assert(pTypeExpr);

					Type typePtr;
					initCopy(&typePtr, *pTypeExpr);
					Defer(dispose(&typePtr));

					TypeModifier typemodPtr;
					typemodPtr.typemodk = TYPEMODK_Pointer;

					prepend(&typePtr.aTypemods, typemodPtr);
					typidResult = ensureInTypeTable(pPass->pTypeTable, &typePtr);
				} break;

				default:
				{
					reportIceAndExit("Unknown binary operator: %d", pExpr->pOp->tokenk);
				} break;
			}
		} break;

		case ASTK_SymbolExpr:
		{
			auto * pExpr = Down(pNode, SymbolExpr);

			switch (pExpr->symbexprk)
			{
				case SYMBEXPRK_Unresolved:
				{
					Assert(pExpr->unresolvedData.aCandidates.cItem == 0);

					// Lookup all funcs matching this identifier

					Scope * pScopeCur = (*pPass->pMpScopeidPScope)[pPass->scopeidCur];

					{
						lookupFuncSymbol(*pScopeCur, pExpr->ident, &pExpr->unresolvedData.aCandidates);
					}

					// Lookup var matching this identifier, and slot it in where it fits

					if (!pExpr->unresolvedData.ignoreVars)
					{
						SymbolInfo symbInfoVar = lookupVarSymbol(*pScopeCur, pExpr->ident);

						if (symbInfoVar.symbolk != SYMBOLK_Nil)
						{
							Assert(symbInfoVar.symbolk == SYMBOLK_Var);
							SCOPEID scopeidVar = symbInfoVar.varData.pVarDeclStmt->ident.scopeid;

							int iInsert = 0;
							for (int iCandidate = 0; iCandidate < pExpr->unresolvedData.aCandidates.cItem; iCandidate++)
							{
								SymbolInfo symbInfoCandidate = pExpr->unresolvedData.aCandidates[iCandidate];
								Assert(symbInfoCandidate.symbolk == SYMBOLK_Func);

								SCOPEID scopeidFunc = symbInfoCandidate.funcData.pFuncDefnStmt->ident.scopeid;
								
								if (scopeidVar >= scopeidFunc)
								{
									iInsert = iCandidate;
									break;
								}
							}

							insert(&pExpr->unresolvedData.aCandidates, symbInfoVar, iInsert);
						}
					}

					if (pExpr->unresolvedData.aCandidates.cItem == 1)
					{
						SymbolInfo symbInfo = pExpr->unresolvedData.aCandidates[0];
						if (symbInfo.symbolk == SYMBOLK_Var)
						{
							pExpr->symbexprk = SYMBEXPRK_Var;
							pExpr->varData.pDeclCached = symbInfo.varData.pVarDeclStmt;

							typidResult = pExpr->varData.pDeclCached->typidDefn;
						}
						else
						{
							Assert(symbInfo.symbolk == SYMBOLK_Func);

							pExpr->symbexprk = SYMBEXPRK_Func;
							pExpr->funcData.pDefnCached = symbInfo.funcData.pFuncDefnStmt;

							typidResult = pExpr->funcData.pDefnCached->typidDefn;
						}
					}
					else if (pExpr->unresolvedData.aCandidates.cItem > 1)
					{
						typidResult = TYPID_UnresolvedHasCandidates;
					}
					else
					{
						print("Unresolved variable ");
						print(pExpr->ident.strv);
						println();

						// HMM: Is this the right result value? Should we have a specific TYPID_UnresolvedIdentifier?

						typidResult = TYPID_Unresolved;
					}
				}
				break;

				case SYMBEXPRK_Var:
				{
					// NOTE (andrew) Undecorated symbols may refer to vars or functions, so they should start with SYMBEXPRK_Nil.
					//	It isn't until we resolve it that we can properly set their symbexprk.

					reportIceAndExit("Symbol tagged with SYMBEXPRK_Var before we resolved it?");
				} break;

				case SYMBEXPRK_MemberVar:
				{
					Assert(pExpr->memberData.pOwner);
					AssertInfo(!pExpr->memberData.pDeclCached, "This shouldn't be resolved yet because this is the code that should resolve it!");

					TYPID ownerTypid = resolveExpr(pPass, pExpr->memberData.pOwner);

					if (!isTypeResolved(ownerTypid))
					{
						typidResult = TYPID_BubbleError;
						goto LEndSetTypidAndReturn;
					}

					const Type * pOwnerType = lookupType(*pPass->pTypeTable, ownerTypid);
					Assert(pOwnerType);

					if (pOwnerType->isFuncType)
					{
						print("Trying to access data member of a function?");
						typidResult = TYPID_TypeError;
						goto LEndSetTypidAndReturn;
					}

					SymbolInfo symbInfoMember;
					{
						Scope * pScopeOwnerOuter = (*pPass->pMpScopeidPScope)[pOwnerType->nonFuncTypeData.ident.scopeid];
						SymbolInfo symbInfoOwner = lookupTypeSymbol(*pScopeOwnerOuter, pOwnerType->nonFuncTypeData.ident.lexeme);
						Assert(symbInfoOwner.symbolk == SYMBOLK_Struct);

						Scope * pScopeOwnerInner = (*pPass->pMpScopeidPScope)[symbInfoOwner.structData.pStructDefnStmt->scopeid];
						symbInfoMember = lookupVarSymbol(*pScopeOwnerInner, pExpr->ident, FSYMBQ_IgnoreParent);
					}

					if (symbInfoMember.symbolk == SYMBOLK_Nil)
					{
						// Unresolved
						// TODO: Add this to a resolve error list... don't print inline right here!

						print("Unresolved member variable ");
						print(pExpr->ident.strv);
						println();

						typidResult = TYPID_Unresolved;
						goto LEndSetTypidAndReturn;
					}

					Assert(symbInfoMember.symbolk == SYMBOLK_Var);

					// Resolve!

					pExpr->memberData.pDeclCached = symbInfoMember.varData.pVarDeclStmt;
					typidResult = symbInfoMember.varData.pVarDeclStmt->typidDefn;
				} break;

				case SYMBEXPRK_Func:
				{
					AssertInfo(!pExpr->funcData.pDefnCached, "This shouldn't be resolved yet because this is the code that should resolve it!");

					Scope * pScopeCur = (*pPass->pMpScopeidPScope)[pPass->scopeidCur];

					DynamicArray<SymbolInfo> aSymbInfoFuncCandidates;
					init(&aSymbInfoFuncCandidates);
					Defer(dispose(&aSymbInfoFuncCandidates));

					lookupFuncSymbol(*pScopeCur, pExpr->ident, &aSymbInfoFuncCandidates);

					AstFuncDefnStmt * pFuncDefnStmtMatch = nullptr;
					for (int iCandidate = 0; iCandidate < aSymbInfoFuncCandidates.cItem; iCandidate++)
					{
						Assert(aSymbInfoFuncCandidates[iCandidate].symbolk == SYMBOLK_Func);

						AstFuncDefnStmt * pCandidateDefnStmt = aSymbInfoFuncCandidates[iCandidate].funcData.pFuncDefnStmt;
						if (pCandidateDefnStmt->pParamsReturnsGrp->apParamVarDecls.cItem != pExpr->funcData.aTypidDisambig.cItem)
						{
							continue;
						}

						bool allArgsMatch = true;
						for (int iParam = 0; iParam < pExpr->funcData.aTypidDisambig.cItem; iParam++)
						{
							TYPID typidDisambig = pExpr->funcData.aTypidDisambig[iParam];
							Assert(isTypeResolved(typidDisambig));

							Assert(pCandidateDefnStmt->pParamsReturnsGrp->apParamVarDecls[iParam]->astk == ASTK_VarDeclStmt);
							auto * pNode = Down(pCandidateDefnStmt->pParamsReturnsGrp->apParamVarDecls[iParam], VarDeclStmt);

							TYPID typidCandidate = pNode->typidDefn;
							Assert(isTypeResolved(typidCandidate));

							// NOTE (andrews) Don't want any type coercion here. The disambiguating types provided should
							//	match a function exactly!

							if (typidDisambig != typidCandidate)
							{
								allArgsMatch = false;
								break;
							}
						}

						if (allArgsMatch)
						{
							pFuncDefnStmtMatch = pCandidateDefnStmt;
							break;
						}
					}


					if (pFuncDefnStmtMatch)
					{
						// Resolve!

						pExpr->funcData.pDefnCached = pFuncDefnStmtMatch;
						typidResult = pFuncDefnStmtMatch->typidDefn;
					}
					else
					{
						// Unresolved
						// TODO: Add this to a resolve error list... don't print inline right here!

						print("Unresolved func identifier ");
						print(pExpr->ident.strv);
						println();

						typidResult = TYPID_Unresolved;
					}
				} break;
			}
		} break;

		case ASTK_PointerDereferenceExpr:
		{
			auto * pExpr = Down(pNode, PointerDereferenceExpr);

			TYPID typidPtr = resolveExpr(pPass, pExpr->pPointerExpr);

			if (!isTypeResolved(typidPtr))
			{
				typidResult = TYPID_BubbleError;
				goto LEndSetTypidAndReturn;
			}

			const Type * pTypePtr = lookupType(*pPass->pTypeTable, typidPtr);
			Assert(pTypePtr);

			if (!isPointerType(*pTypePtr))
			{
				print("Trying to dereference a non-pointer\n");
				typidResult = TYPID_TypeError;
				goto LEndSetTypidAndReturn;
			}

			Type typeDereferenced;
			initCopy(&typeDereferenced, *pTypePtr);
			Defer(dispose(&typeDereferenced));

			remove(&typeDereferenced.aTypemods, 0);

			typidResult = ensureInTypeTable(pPass->pTypeTable, &typeDereferenced);
		} break;

		case ASTK_ArrayAccessExpr:
		{
			auto * pExpr = Down(pNode, ArrayAccessExpr);

			TYPID typidArray = resolveExpr(pPass, pExpr->pArrayExpr);

			if (!isTypeResolved(typidArray))
			{
				typidResult = TYPID_BubbleError;
				goto LEndSetTypidAndReturn;
			}

			TYPID typidSubscript = resolveExpr(pPass, pExpr->pSubscriptExpr);
			if (typidSubscript != TYPID_S32)
			{
				// TODO: support s8, s16, s64, u8, etc...
				// TODO: catch negative compile time constants

				// TODO: report error here, but keep on chugging with the resolve... despite the incorrect subscript, we still
				//	know what type an array access will end up as.

				print("Trying to access array with a non integer\n");
			}

			const Type * pTypeArray = lookupType(*pPass->pTypeTable, typidArray);
			Assert(pTypeArray);

			if (pTypeArray->aTypemods.cItem == 0 || pTypeArray->aTypemods[0].typemodk != TYPEMODK_Array)
			{
				// TODO: More specific type error. (trying to access non-array <identifier> as if it were an array)
				//	How am I going to report these errors? Should I just maintain a list separate from the
				//	fact that I am returning error TYPID's?

				print("Trying to access non-array as if it were an array...\n");
				typidResult = TYPID_TypeError;
				goto LEndSetTypidAndReturn;
			}

			Type typeElement;
			initCopy(&typeElement, *pTypeArray);
			Defer(dispose(&typeElement));

			remove(&typeElement.aTypemods, 0);

			typidResult = ensureInTypeTable(pPass->pTypeTable, &typeElement);
		} break;

		case ASTK_FuncCallExpr:
		{
			auto * pExpr = Down(pNode, FuncCallExpr);

			// Resolve calling expression 

			TYPID typidCallingExpr = resolveExpr(pPass, pExpr->pFunc);
			Assert(Implies(typidCallingExpr == TYPID_UnresolvedHasCandidates, pExpr->pFunc->astk == ASTK_SymbolExpr));

			bool callingExprResolvedOrHasCandidates = isTypeResolved(typidCallingExpr) || typidCallingExpr == TYPID_UnresolvedHasCandidates;

			// Resolve args

			// TODO: implement reserve(..) for dynamic array, since we already know ahead of time
			//	how many typids we will have in it.

			DynamicArray<TYPID> aTypidArg;
			init(&aTypidArg);
			Defer(dispose(&aTypidArg));

			bool allArgsResolvedOrHasCandidates = true;
			for (int i = 0; i < pExpr->apArgs.cItem; i++)
			{
				TYPID typidArg = resolveExpr(pPass, pExpr->apArgs[i]);
				if (!isTypeResolved(typidArg) && typidArg != TYPID_UnresolvedHasCandidates)
				{
					allArgsResolvedOrHasCandidates = false;
				}

				append(&aTypidArg, typidArg);
			}

			if (!callingExprResolvedOrHasCandidates || !allArgsResolvedOrHasCandidates)
			{
				typidResult = TYPID_BubbleError;
				goto LEndSetTypidAndReturn;
			}

			if (pExpr->pFunc->astk != ASTK_SymbolExpr)
			{
				const Type * pType = lookupType(*pPass->pTypeTable, typidCallingExpr);
				Assert(pType);

				if (!pType->isFuncType)
				{
					// TODO: better messaging

					print("Calling non-function as if it were a function");
					println();

					typidResult = TYPID_TypeError;
					goto LEndSetTypidAndReturn;
				}
				else
				{
					const FuncType * pFuncType = &pType->funcTypeData.funcType;

					if (pFuncType->paramTypids.cItem != aTypidArg.cItem)
					{
						// TODO: Might need to adjust aTypidArg.cItem if the candidate
						//	has default arguments. Or better yet, encode the min/max range
						//	in the funcType and check if aTypidArg.cItem falls within
						//	that range

						// TODO: better messaging

						printfmt(
							"Calling function that expects %d arguments, but only providing %d",
							pFuncType->paramTypids.cItem,
							aTypidArg.cItem
						);

						typidResult = TYPID_TypeError;
						goto LEndSetTypidAndReturn;
					}

					for (int iParam = 0; iParam < pFuncType->paramTypids.cItem; iParam++)
					{
						TYPID typidParam = pFuncType->paramTypids[iParam];
						TYPID typidArg = aTypidArg[iParam];

						if (isTypeResolved(typidArg))
						{
							if (typidArg != typidParam && !canCoerce(typidArg, typidParam))
							{
								// TODO: better messaging

								print("Function call type mismatch");

								typidResult = TYPID_TypeError;
								goto LEndSetTypidAndReturn;
							}
						}
						else
						{
							Assert(typidArg == TYPID_UnresolvedHasCandidates);

							// TODO: try to find exactly 1 candidate... just like we do if funcexpr is a symbolexpr
							AssertInfo(false, "TODO");

							// TODO: if exactly 1 is found, change the symbexprk of the underlying arg
						}
					}
				}
			}
			else
			{
				auto * pFuncSymbolExpr = Down(pExpr->pFunc, SymbolExpr);

				// Do matching between func candidates and arg candidates

				AstNode * pNodeDefnclMatch = nullptr;
				if (pFuncSymbolExpr->symbexprk == SYMBEXPRK_Func)
				{
					pNodeDefnclMatch = Up(pFuncSymbolExpr->funcData.pDefnCached);
				}
				else if (pFuncSymbolExpr->symbexprk == SYMBEXPRK_Var)
				{
					pNodeDefnclMatch = Up(pFuncSymbolExpr->varData.pDeclCached);
				}
				else if (pFuncSymbolExpr->symbexprk == SYMBEXPRK_MemberVar)
				{
					pNodeDefnclMatch = Up(pFuncSymbolExpr->memberData.pDeclCached);
				}
				else
				{
					Assert(pFuncSymbolExpr->symbexprk == SYMBEXPRK_Unresolved);

					// TODO: Move this out to be a symbol-table related query in symbol.cpp

					// NOTE (andrew) The ordering of the candidates was set by resolveExpr(..), and is important for correctness. To facilitate
					//	easy removal from the array, we want to iterate backwards. Thus, we reverse the array to allow us to iterate backwards
					//	but maintain the intended order. We reverse back after we are finished to maintain original order for posterity.

					DynamicArray<SymbolInfo> aSymbInfoExactMatch;
					init(&aSymbInfoExactMatch);
					Defer(dispose(&aSymbInfoExactMatch));

					DynamicArray<SymbolInfo> aSymbInfoLooseMatch;
					init(&aSymbInfoLooseMatch);
					Defer(dispose(&aSymbInfoLooseMatch));

					for (int iFuncCandidate = 0; iFuncCandidate < pFuncSymbolExpr->unresolvedData.aCandidates.cItem; iFuncCandidate++)
					{
						SymbolInfo symbInfoFuncCandidate = pFuncSymbolExpr->unresolvedData.aCandidates[iFuncCandidate];

						TYPID typidCandidate = TYPID_Unresolved;
						if (symbInfoFuncCandidate.symbolk == SYMBOLK_Var)
						{
							typidCandidate = symbInfoFuncCandidate.varData.pVarDeclStmt->typidDefn;	
						}
						else
						{
							Assert(symbInfoFuncCandidate.symbolk == SYMBOLK_Func);
							typidCandidate = symbInfoFuncCandidate.funcData.pFuncDefnStmt->typidDefn;
						}

						if (!isTypeResolved(typidCandidate))
						{
							continue;
						}

						const Type * pTypeCandidate = lookupType(*pPass->pTypeTable, typidCandidate);
						Assert(pTypeCandidate);

						if (!pTypeCandidate->isFuncType)
						{
							continue;
						}

						const FuncType * pFuncTypeCandidate = &pTypeCandidate->funcTypeData.funcType;

						if (pFuncTypeCandidate->paramTypids.cItem != aTypidArg.cItem)
						{
							// TODO: Might need to adjust aTypidArg.cItem if the candidate
							//	has default arguments. Or better yet, encode the min/max range
							//	in the funcType and check if aTypidArg.cItem falls within
							//	that range

							continue;
						}

						enum MATCHK
						{
							MATCHK_MatchExact,
							MATCHK_MatchLoose,
							MATCHK_NoMatch
						};

						MATCHK matchk = MATCHK_MatchExact;
						for (int iParamCandidate = 0; iParamCandidate < pFuncTypeCandidate->paramTypids.cItem; iParamCandidate++)
						{
							TYPID typidCandidateExpected = pFuncTypeCandidate->paramTypids[iParamCandidate];

							auto * pNodeArg = pExpr->apArgs[iParamCandidate];
							TYPID typidArg = aTypidArg[iParamCandidate];

							if (isTypeResolved(typidArg))
							{
								if (typidCandidateExpected == typidArg)
								{
									// Do nothing
								}
								else if (canCoerce(typidArg, typidCandidateExpected))
								{
									matchk = MATCHK_MatchLoose;
								}
								else
								{
									matchk = MATCHK_NoMatch;
									break;
								}
							}
							else 
							{
								Assert(typidArg == TYPID_UnresolvedHasCandidates);
								Assert(pNodeArg->astk == ASTK_SymbolExpr);

								auto * pNodeArgSymbExpr = Down(pNodeArg, SymbolExpr);
								Assert(pNodeArgSymbExpr->symbexprk == SYMBEXPRK_Unresolved);
								Assert(pNodeArgSymbExpr->unresolvedData.aCandidates.cItem > 1);

								int cMatchExact = 0;
								int cMatchLoose = 0;
								TYPID typidMatchExact = TYPID_Unresolved;
								TYPID typidMatchLoose = TYPID_Unresolved;
								for (int iArgCandidate = 0; iArgCandidate < pNodeArgSymbExpr->unresolvedData.aCandidates.cItem; iArgCandidate++)
								{
									SymbolInfo symbInfoArgCandidate = pNodeArgSymbExpr->unresolvedData.aCandidates[iArgCandidate];
									
									TYPID typidArgCandidate = TYPID_Unresolved;
									if (symbInfoArgCandidate.symbolk == SYMBOLK_Var)
									{
										typidArgCandidate = symbInfoArgCandidate.varData.pVarDeclStmt->typidDefn;
									}
									else
									{
										Assert(symbInfoArgCandidate.symbolk == SYMBOLK_Func);
										typidArgCandidate = symbInfoArgCandidate.funcData.pFuncDefnStmt->typidDefn;
									}

									Assert(isTypeResolved(typidArgCandidate));		// HMM: Is this actually Assertable?

									if (typidCandidateExpected == typidArgCandidate)
									{
										cMatchExact++;
										typidMatchExact = typidArgCandidate;
										Assert(cMatchExact == 1);
#if !DEBUG
										break;
#endif
									}
									else if (canCoerce(typidArg, typidCandidateExpected))
									{
										cMatchLoose++;

										if (cMatchLoose == 1)
										{
											typidMatchLoose = typidArgCandidate;
										}
										else
										{
											typidMatchLoose = TYPID_Unresolved;
										}
									}
									else
									{
										// Do nothing
									}
								}

								Assert(Implies(cMatchExact > 0, cMatchExact == 1));
								Assert(Implies(cMatchExact == 1, isTypeResolved(typidMatchExact)));

								if (cMatchExact == 1)
								{
									// Do nothing
								}
								else if (cMatchLoose == 1)
								{
									matchk = MATCHK_MatchLoose;
								}
								else if (cMatchLoose > 1)
								{
									// TODO: better reporting
									// TODO: This actually shouldn't be an error... it is only an error
									//	if the rest of the parameters match too! Otherwise we just
									//	discard this func candidate from consideration anyways.

									print("Ambiguous function call ");
									print(pFuncSymbolExpr->ident.strv);
									typidResult = TYPID_TypeError;
									goto LEndSetTypidAndReturn;
								}
								else
								{
									matchk = MATCHK_NoMatch;
									break;
								}
							}
						}

						switch (matchk)
						{
							case MATCHK_MatchExact:
							{
								append(&aSymbInfoExactMatch, symbInfoFuncCandidate);
							} break;

							case MATCHK_MatchLoose:
							{
								append(&aSymbInfoLooseMatch, symbInfoFuncCandidate);
							} break;
						}
					}

					Assert(aSymbInfoExactMatch.cItem <= 1);		// TODO: I don't think this is assertable... there could be a func var and a fn def that both exact match...

					if (aSymbInfoExactMatch.cItem == 1)
					{
						SymbolInfo symbInfoExactMatch = aSymbInfoExactMatch[0];
						if (symbInfoExactMatch.symbolk == SYMBOLK_Var)
						{
							pNodeDefnclMatch = Up(symbInfoExactMatch.varData.pVarDeclStmt);
						}
						else
						{
							Assert(symbInfoExactMatch.symbolk == SYMBOLK_Func);
							pNodeDefnclMatch = Up(symbInfoExactMatch.funcData.pFuncDefnStmt);
						}
					}
					else if (aSymbInfoLooseMatch.cItem == 1)
					{
						SymbolInfo symbInfoLooseMatch = aSymbInfoLooseMatch[0];
						if (symbInfoLooseMatch.symbolk == SYMBOLK_Var)
						{
							pNodeDefnclMatch = Up(symbInfoLooseMatch.varData.pVarDeclStmt);
						}
						else
						{
							Assert(symbInfoLooseMatch.symbolk == SYMBOLK_Func);
							pNodeDefnclMatch = Up(symbInfoLooseMatch.funcData.pFuncDefnStmt);
						}
					}
					else if (aSymbInfoLooseMatch.cItem > 1)
					{
						// TODO: better reporting

						print("Ambiguous function call ");
						print(pFuncSymbolExpr->ident.strv);
						println();
						typidResult = TYPID_TypeError;
						goto LEndSetTypidAndReturn;
					}
					else
					{
						// TODO: better reporting

						print("No func named ");
						print(pFuncSymbolExpr->ident.strv);
						print(" matches the provided parameters");
						println();
						typidResult = TYPID_TypeError;
						goto LEndSetTypidAndReturn;
					}
				}

				Assert(pNodeDefnclMatch);
				const Type * pType = nullptr;
				if (pNodeDefnclMatch->astk == ASTK_VarDeclStmt)
				{
					auto * pNodeVarDeclStmt = Down(pNodeDefnclMatch, VarDeclStmt);
					Assert(isTypeResolved(pNodeVarDeclStmt->typidDefn));

					pType = lookupType(*pPass->pTypeTable, pNodeVarDeclStmt->typidDefn);

					pFuncSymbolExpr->symbexprk = SYMBEXPRK_Var;
					pFuncSymbolExpr->varData.pDeclCached = pNodeVarDeclStmt;
				}
				else
				{
					Assert(pNodeDefnclMatch->astk == ASTK_FuncDefnStmt);

					auto * pNodeFuncDefnStmt = Down(pNodeDefnclMatch, FuncDefnStmt);
					Assert(isTypeResolved(pNodeFuncDefnStmt->typidDefn));

					pType = lookupType(*pPass->pTypeTable, pNodeFuncDefnStmt->typidDefn);

					pFuncSymbolExpr->symbexprk = SYMBEXPRK_Func;
					pFuncSymbolExpr->funcData.pDefnCached = pNodeFuncDefnStmt;
				}

				Assert(pType);
				Assert(pType->isFuncType);
				if (pType->funcTypeData.funcType.returnTypids.cItem == 0)
				{
					typidResult = TYPID_Void;
				}
				else
				{
					// TODO: multiple return values

					Assert(pType->funcTypeData.funcType.returnTypids.cItem == 1);
					typidResult = pType->funcTypeData.funcType.returnTypids[0];
				}
			}
		} break;

		case ASTK_FuncLiteralExpr:
		{
			auto * pExpr = Down(pNode, FuncLiteralExpr);

			// NOTE: Func literal expressions define a type that we should have already put into the type table when parsing,
			//	so we just need to make sure we resolve the children here.

			Assert(isTypeResolved(UpExpr(pExpr)->typidEval));
			typidResult = UpExpr(pExpr)->typidEval;

			// Push scope and resolve children

			SCOPEID scopeidRestore = pPass->scopeidCur;
			pPass->scopeidCur = pExpr->scopeid;
			Defer(pPass->scopeidCur = scopeidRestore);

			auditDuplicateSymbols(pPass);

			auto * papParamVarDecls = &pExpr->pParamsReturnsGrp->apParamVarDecls;
			for (int i = 0; i < papParamVarDecls->cItem; i++)
			{
				doResolvePass(pPass, (*papParamVarDecls)[i]);
			}

			auto * papReturnVarDecls = &pExpr->pParamsReturnsGrp->apReturnVarDecls;
			for (int i = 0; i < papReturnVarDecls->cItem; i++)
			{
				doResolvePass(pPass, (*papReturnVarDecls)[i]);
			}

			// Push ctx

			auto * pFnCtx = pushNew(&pPass->fnCtxStack);
			initExtract(&pFnCtx->aTypidReturn, *papReturnVarDecls, offsetof(AstVarDeclStmt, typidDefn));
			Defer(dispose(&pFnCtx->aTypidReturn));

			// Resolve body

			doResolvePass(pPass, pExpr->pBodyStmt);

			// Pop ctx

			pop(&pPass->fnCtxStack);
		} break;

		default:
		{
			reportIceAndExit("Unknown astk in resolveExpr: %d", pNode->astk);
		} break;
	}

LEndSetTypidAndReturn:

	AstExpr * pExpr = DownExpr(pNode);
	pExpr->typidEval = typidResult;
	return typidResult;
}

void resolveStmt(ResolvePass * pPass, AstNode * pNode)
{
	Assert(category(pNode->astk) == ASTCATK_Stmt);

	switch (pNode->astk)
	{
		case ASTK_ExprStmt:
		{
			auto * pStmt = Down(pNode, ExprStmt);
			doResolvePass(pPass, pStmt->pExpr);
		} break;

		case ASTK_AssignStmt:
		{
			// TODO: Need to figure out what the story is for lvalues and rvalues
			//	and make sure that the LHS is an lvalue

			auto * pStmt = Down(pNode, AssignStmt);
			TYPID typidLhs = resolveExpr(pPass, pStmt->pLhsExpr);
			TYPID typidRhs = resolveExpr(pPass, pStmt->pRhsExpr);

			if (!isLValue(pStmt->pLhsExpr->astk))
			{
				// TOOD: report error
				print("Assigning to non-lvalue\n");
			}
			else
			{
				if (isTypeResolved(typidLhs) && isTypeResolved(typidRhs))
				{
					if (typidLhs != typidRhs)
					{
						// TODO: report error
						printfmt("Assignment type error. L: %d, R: %d\n", typidLhs, typidRhs);
					}
				}
			}
		} break;

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = Down(pNode, VarDeclStmt);


			AssertInfo(isTypeResolved(pStmt->typidDefn), "Inferred types are TODO");
			const Type * pType = lookupType(*pPass->pTypeTable, pStmt->typidDefn);

			if (pStmt->ident.lexeme.strv.cCh > 0)
			{
#if DEBUG
				Scope * pScopeCur = (*pPass->pMpScopeidPScope)[pPass->scopeidCur];
				SymbolInfo symbInfo = lookupVarSymbol(*pScopeCur, pStmt->ident.lexeme);
				Assert(symbInfo.symbolk == SYMBOLK_Var);
#endif
				// Record symbseqid for variable

				pPass->varseqidSeen = pStmt->varseqid;
			}

			// Resolve init expr

			// TODO: Need to get the type of the initExpr and compare it to the type of the lhs...
			//	just like in assignment

			if (pStmt->pInitExpr)
			{
				TYPID typidInit = resolveExpr(pPass, pStmt->pInitExpr);

				if (isTypeResolved(typidInit) && typidInit != pStmt->typidDefn)
				{
					// TODO: report better error
					printfmt("Cannot initialize variable of type %d with expression of type %d\n", pStmt->typidDefn, typidInit);
				}
			}
		} break;

		case ASTK_StructDefnStmt:
		{
			auto * pStmt = Down(pNode, StructDefnStmt);

#if DEBUG
			Scope * pScopeOuter = (*pPass->pMpScopeidPScope)[pPass->scopeidCur];
			SymbolInfo symbInfo = lookupTypeSymbol(*pScopeOuter, pStmt->ident.lexeme);
			Assert(symbInfo.symbolk == SYMBOLK_Struct);
#endif

			// Record scope id

			SCOPEID scopeidRestore = pPass->scopeidCur;
			pPass->scopeidCur = pStmt->scopeid;
			Defer(pPass->scopeidCur = scopeidRestore);

			auditDuplicateSymbols(pPass);

			// Resolve member decls

			for (int i = 0; i < pStmt->apVarDeclStmt.cItem; i++)
			{
				doResolvePass(pPass, pStmt->apVarDeclStmt[i]);
			}
		} break;

		case ASTK_FuncDefnStmt:
		{
			auto * pStmt = Down(pNode, FuncDefnStmt);

#if DEBUG
			Scope * pScopeCur = (*pPass->pMpScopeidPScope)[pPass->scopeidCur];
			DynamicArray<SymbolInfo> aSymbInfo;
			init(&aSymbInfo);
			Defer(dispose(&aSymbInfo));
			lookupFuncSymbol(*pScopeCur, pStmt->ident.lexeme, &aSymbInfo);
			AssertInfo(aSymbInfo.cItem > 0, "We should have put this func decl in the symbol table when we parsed it...");
#endif

			// Record scope id

			SCOPEID scopeidRestore = pPass->scopeidCur;
			pPass->scopeidCur = pStmt->scopeid;
			Defer(pPass->scopeidCur = scopeidRestore);

			auditDuplicateSymbols(pPass);

			// Resolve params

			auto * papParamVarDecls = &pStmt->pParamsReturnsGrp->apParamVarDecls;
			for (int i = 0; i < papParamVarDecls->cItem; i++)
			{
				doResolvePass(pPass, (*papParamVarDecls)[i]);
			}

			// Resolve return values

			auto * papReturnVarDecls = &pStmt->pParamsReturnsGrp->apReturnVarDecls;
			for (int i = 0; i < papReturnVarDecls->cItem; i++)
			{
				doResolvePass(pPass, (*papReturnVarDecls)[i]);
			}

			// Push ctx

			auto * pFnCtx = pushNew(&pPass->fnCtxStack);
			initExtract(&pFnCtx->aTypidReturn, *papReturnVarDecls, offsetof(AstVarDeclStmt, typidDefn));
			Defer(dispose(&pFnCtx->aTypidReturn));

			// Resolve body

			doResolvePass(pPass, pStmt->pBodyStmt);

			// Pop ctx

			pop(&pPass->fnCtxStack);
		} break;

		case ASTK_BlockStmt:
		{
			auto * pStmt = Down(pNode, BlockStmt);

			// NOTE (andrew) The scope push/pop here may be a no-op if this block is from a function
			//	definition (since blocks get their scope folded into the function definition's scope)

			SCOPEID scopeidRestore = pPass->scopeidCur;
			pPass->scopeidCur = pStmt->scopeid;
			Defer(pPass->scopeidCur = scopeidRestore);

			auditDuplicateSymbols(pPass);

			for (int i = 0; i < pStmt->apStmts.cItem; i++)
			{
				doResolvePass(pPass, pStmt->apStmts[i]);
			}
		} break;

		case ASTK_WhileStmt:
		{
			auto * pStmt = Down(pNode, WhileStmt);
			doResolvePass(pPass, pStmt->pCondExpr);

			bool isBodyBlock = pStmt->pBodyStmt->astk == ASTK_BlockStmt;
			if (isBodyBlock) pPass->cNestedBreakable++;

			doResolvePass(pPass, pStmt->pBodyStmt);

			if (isBodyBlock) pPass->cNestedBreakable--;
		} break;

		case ASTK_IfStmt:
		{
			auto * pStmt = Down(pNode, IfStmt);
			doResolvePass(pPass, pStmt->pCondExpr);
			doResolvePass(pPass, pStmt->pThenStmt);
			if (pStmt->pElseStmt) doResolvePass(pPass, pStmt->pElseStmt);
		} break;

		case ASTK_ReturnStmt:
		{
			auto * pStmt = Down(pNode, ReturnStmt);

			AssertInfo(count(pPass->fnCtxStack) > 0, "I don't think I allow this statement to parse unless in a function body?");
			auto * pFnCtx = peekPtr(pPass->fnCtxStack);

			if (pFnCtx->aTypidReturn.cItem == 0 && pStmt->pExpr)
			{
				print("Cannot return value from function expecting void\n");
			}
			else if (pFnCtx->aTypidReturn.cItem != 0 && !pStmt->pExpr)
			{
				print("Function expecting a return value\n");
			}

			if (pStmt->pExpr)
			{
				TYPID typidReturn = resolveExpr(pPass, pStmt->pExpr);

				// TODO: Handle multiple return values

				if (typidReturn != pFnCtx->aTypidReturn[0])
				{
					print("Return type mismatch\n");
				}
			}
		} break;

		case ASTK_BreakStmt:
		{
			if (pPass->cNestedBreakable == 0)
			{
				print("Cannot break from current context");
			}
			else
			{
				Assert(pPass->cNestedBreakable > 0);
			}
		} break;

		case ASTK_ContinueStmt:
		{
			if (pPass->cNestedBreakable == 0)
			{
				print("Cannot continue from current context");
			}
			else
			{
				Assert(pPass->cNestedBreakable > 0);
			}
		} break;

		default:
		{
			reportIceAndExit("Unknown astk in resolveStmt: %d", pNode->astk);
		} break;
	}
}

void doResolvePass(ResolvePass * pPass, AstNode * pNode)
{
	AssertInfo(
		pNode,
		"For optional node children, it is the caller's responsibility to perform null check, because I want to catch "
		"errors if we ever accidentally have null children in spots where they shouldn't be null"
	);

	switch (category(pNode->astk))
	{
		case ASTCATK_Expr:
		{
			resolveExpr(pPass, pNode);
		} break;

		case ASTCATK_Stmt:
		{
			resolveStmt(pPass, pNode);
		} break;

		// PROGRAM

		case ASTCATK_Program:
		{
			auto * pProgram = Down(pNode, Program);
			for (int i = 0; i < pProgram->apNodes.cItem; i++)
			{
				doResolvePass(pPass, pProgram->apNodes[i]);
			}
		} break;

		default:
		{
			reportIceAndExit("Unknown astcatk in doResolvePass: %d", category(pNode->astk));
		} break;
	}
}