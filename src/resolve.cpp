#include "resolve.h"

#include "ast.h"
#include "error.h"
#include "global_context.h"
#include "parse.h"
#include "print.h"
#include "symbol.h"
#include "type.h"

#define SetBubbleIfUnresolved(typid) do { if (!isTypeResolved(typid)) (typid) = TypeId::BubbleError; } while(0)

void init(ResolvePass * pPass, MeekCtx * pCtx)
{
	pPass->pCtx = pCtx;

	pPass->varseqidSeen = VARSEQID_Zero;
	init(&pPass->unresolvedIdents);
	init(&pPass->fnCtxStack);
	init(&pPass->scopeidStack);

	pPass->hadError = false;
	pPass->cNestedBreakable = 0;
}

void pushAndProcessScope(ResolvePass * pPass, SCOPEID scopeid)
{
	MeekCtx * pCtx = pPass->pCtx;

	push(&pPass->scopeidStack, scopeid);
	auditDuplicateSymbols(pCtx, pCtx->scopes[scopeid]);
	computeScopedVariableOffsets(pCtx, pCtx->scopes[scopeid]);
}

void resolveExpr(ResolvePass * pPass, AstNode * pNode)
{
	Assert(category(pNode->astk) == ASTCATK_Expr);

	MeekCtx * pCtx = pPass->pCtx;
	TypeTable * typeTable = pCtx->typeTable;

	TypeId typidResult = TypeId::Unresolved;

	switch (pNode->astk)
	{
		case ASTK_BinopExpr:
		{
			// NOTE: Assume that binops can only happen between
			//	exact type matches, and the result becomes that type
			//	(or a boolean). Will change later?

			auto * pExpr = Down(pNode, BinopExpr);

			TypeId typidLhs = DownExpr(pExpr->pLhsExpr)->typidEval;
			TypeId typidRhs = DownExpr(pExpr->pRhsExpr)->typidEval;

			if (!isTypeResolved(typidLhs) || !isTypeResolved(typidRhs))
			{
				typidResult = TypeId::BubbleError;
				goto LEndSetTypidAndReturn;
			}

			if (typidLhs != typidRhs)
			{
				// TODO: report error

				printfmt("Binary operator type mismatch. L: %d, R: %d\n", typidLhs, typidRhs);
				typidResult = TypeId::TypeError;
				goto LEndSetTypidAndReturn;
			}

			// TODO: AmpAmp and PipePipe should only be allowed to have boolean operands

			switch (pExpr->pOp->tokenk)
			{
				case TOKENK_EqualEqual:
				case TOKENK_BangEqual:
				case TOKENK_Lesser:
				case TOKENK_LesserEqual:
				case TOKENK_Greater:
				case TOKENK_GreaterEqual:
				case TOKENK_AmpAmp:
				case TOKENK_PipePipe:
				{
					typidResult = TypeId::Bool;
				} break;

				default:
				{
					typidResult = typidLhs;
				} break;
			}
		} break;

		case ASTK_GroupExpr:
		{
			auto * pExpr = Down(pNode, GroupExpr);
			typidResult = DownExpr(pExpr->pExpr)->typidEval;

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
			TypeId typidExpr = DownExpr(pExpr->pExpr)->typidEval;

			if (!isTypeResolved(typidExpr))
			{
				typidResult = TypeId::BubbleError;
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
					Type typePtr;
					init(&typePtr, TYPEK_Mod);
					Defer(dispose(&typePtr));

					typePtr.modTypeData.typemod.typemodk = TYPEMODK_Pointer;
					typePtr.modTypeData.typidModified = typidExpr;

					auto ensureResult = ensureInTypeTable(typeTable, &typePtr);
					Assert(ensureResult.typeInfoComputed);

					typidResult = ensureResult.typid;
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

					SCOPEID scopeidCur = peek(pPass->scopeidStack);
					Scope * pScopeCur = pCtx->scopes[scopeidCur];
					lookupFuncSymbol(*pScopeCur, pExpr->ident, &pExpr->unresolvedData.aCandidates);

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
						typidResult = TypeId::UnresolvedHasCandidates;
					}
					else
					{
						print("Unresolved variable ");
						print(pExpr->ident.strv);
						println();

						// HMM: Is this the right result value? Should we have a specific TypeId::UnresolvedIdentifier?

						typidResult = TypeId::Unresolved;
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

					TypeId ownerTypid = DownExpr(pExpr->memberData.pOwner)->typidEval;

					if (!isTypeResolved(ownerTypid))
					{
						typidResult = TypeId::BubbleError;
						goto LEndSetTypidAndReturn;
					}

					const Type * pOwnerType = lookupType(*typeTable, ownerTypid);
					Assert(pOwnerType);

					if (pOwnerType->typek == TYPEK_Func)
					{
						print("Trying to access data member of a function?");
						typidResult = TypeId::TypeError;
						goto LEndSetTypidAndReturn;
					}
					else if (pOwnerType->typek == TYPEK_Mod)
					{
						// We should error if it's an array mod.
						// I need to figure out what I want to do if it's a pointer mod. Maybe auto-dereference it. Only do 1 level of this though?
						//	Or should a ^^foo f be allowed to say f.member and double auto-dereference?

						AssertTodo;
					}

					SymbolInfo symbInfoMember;
					{
						Scope * pScopeOwnerOuter = pCtx->scopes[pOwnerType->namedTypeData.ident.scopeid];
						SymbolInfo symbInfoOwner = lookupTypeSymbol(*pScopeOwnerOuter, pOwnerType->namedTypeData.ident.lexeme);
						Assert(symbInfoOwner.symbolk == SYMBOLK_Struct);

						Scope * pScopeOwnerInner = pCtx->scopes[symbInfoOwner.structData.pStructDefnStmt->scopeid];
						symbInfoMember = lookupVarSymbol(*pScopeOwnerInner, pExpr->ident, FSYMBQ_IgnoreParent);
					}

					if (symbInfoMember.symbolk == SYMBOLK_Nil)
					{
						// Unresolved
						// TODO: Add this to a resolve error list... don't print inline right here!

						print("Unresolved member variable ");
						print(pExpr->ident.strv);
						println();

						typidResult = TypeId::Unresolved;
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

					SCOPEID scopeidCur = peek(pPass->scopeidStack);
					Scope * pScopeCur = pCtx->scopes[scopeidCur];

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
							TypeId typidDisambig = pExpr->funcData.aTypidDisambig[iParam];
							Assert(isTypeResolved(typidDisambig));

							Assert(pCandidateDefnStmt->pParamsReturnsGrp->apParamVarDecls[iParam]->astk == ASTK_VarDeclStmt);
							auto * pNode = Down(pCandidateDefnStmt->pParamsReturnsGrp->apParamVarDecls[iParam], VarDeclStmt);

							TypeId typidCandidate = pNode->typidDefn;
							Assert(isTypeResolved(typidCandidate));

							// NOTE: Don't want any type coercion here. The disambiguating types provided should
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

						typidResult = TypeId::Unresolved;
					}
				} break;
			}
		} break;

		case ASTK_PointerDereferenceExpr:
		{
			auto * pExpr = Down(pNode, PointerDereferenceExpr);
			TypeId typidPtr = DownExpr(pExpr->pPointerExpr)->typidEval;

			if (!isTypeResolved(typidPtr))
			{
				typidResult = TypeId::BubbleError;
				goto LEndSetTypidAndReturn;
			}

			const Type * pTypePtr = lookupType(*typeTable, typidPtr);
			Assert(pTypePtr);

			if (!isPointerType(*pTypePtr))
			{
				print("Trying to dereference a non-pointer\n");
				typidResult = TypeId::TypeError;
				goto LEndSetTypidAndReturn;
			}

			typidResult = pTypePtr->modTypeData.typidModified;
		} break;

		case ASTK_ArrayAccessExpr:
		{
			auto * pExpr = Down(pNode, ArrayAccessExpr);

			TypeId typidArray = DownExpr(pExpr->pArrayExpr)->typidEval;
			TypeId typidSubscript = DownExpr(pExpr->pSubscriptExpr)->typidEval;

			if (!isTypeResolved(typidArray))
			{
				typidResult = TypeId::BubbleError;
				goto LEndSetTypidAndReturn;
			}

			if (typidSubscript != TypeId::S32)
			{
				// TODO: support s8, s16, s64, u8, etc...
				// TODO: catch negative compile time constants

				// TODO: report error here, but keep on chugging with the resolve... despite the incorrect subscript, we still
				//	know what type an array access will end up as.

				print("Trying to access array with a non integer\n");
			}

			const Type * pTypeArray = lookupType(*typeTable, typidArray);
			Assert(pTypeArray);

			if (!isArrayType(*pTypeArray))
			{
				// TODO: More specific type error. (trying to access non-array <identifier> as if it were an array)
				//	How am I going to report these errors? Should I just maintain a list separate from the
				//	fact that I am returning error TYPID's?

				print("Trying to access non-array as if it were an array...\n");
				typidResult = TypeId::TypeError;
				goto LEndSetTypidAndReturn;
			}

			typidResult = pTypeArray->modTypeData.typidModified;
		} break;

		case ASTK_FuncCallExpr:
		{
			auto * pExpr = Down(pNode, FuncCallExpr);

			// Calling expression 

			TypeId typidCallingExpr = DownExpr(pExpr->pFunc)->typidEval;
			Assert(Implies(typidCallingExpr == TypeId::UnresolvedHasCandidates, pExpr->pFunc->astk == ASTK_SymbolExpr));

			bool callingExprResolvedOrHasCandidates = isTypeResolved(typidCallingExpr) || typidCallingExpr == TypeId::UnresolvedHasCandidates;

			// Args

			DynamicArray<TypeId> aTypidArg;
			init(&aTypidArg);
			ensureCapacity(&aTypidArg, pExpr->apArgs.cItem);
			Defer(dispose(&aTypidArg));

			bool allArgsResolvedOrHasCandidates = true;
			for (int iArg = 0; iArg < pExpr->apArgs.cItem; iArg++)
			{
				TypeId typidArg = DownExpr(pExpr->apArgs[iArg])->typidEval;
				if (!isTypeResolved(typidArg) && typidArg != TypeId::UnresolvedHasCandidates)
				{
					allArgsResolvedOrHasCandidates = false;
				}

				append(&aTypidArg, typidArg);
			}

			if (!callingExprResolvedOrHasCandidates || !allArgsResolvedOrHasCandidates)
			{
				typidResult = TypeId::BubbleError;
				goto LEndSetTypidAndReturn;
			}

			if (pExpr->pFunc->astk != ASTK_SymbolExpr)
			{
				const Type * pType = lookupType(*typeTable, typidCallingExpr);
				Assert(pType);

				switch (pType->typek)
				{
					case TYPEK_Named:
					case TYPEK_Mod:
					{
						// TODO: better messaging

						print("Calling non-function as if it were a function");
						println();

						typidResult = TypeId::TypeError;
						goto LEndSetTypidAndReturn;
					} break;

					case TYPEK_Func:
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

							typidResult = TypeId::TypeError;
							goto LEndSetTypidAndReturn;
						}

						for (int iParam = 0; iParam < pFuncType->paramTypids.cItem; iParam++)
						{
							TypeId typidParam = pFuncType->paramTypids[iParam];
							TypeId typidArg = aTypidArg[iParam];

							if (isTypeResolved(typidArg))
							{
								if (typidArg != typidParam && !canCoerce(typidArg, typidParam))
								{
									// TODO: better messaging

									print("Function call type mismatch");

									typidResult = TypeId::TypeError;
									goto LEndSetTypidAndReturn;
								}
							}
							else
							{
								Assert(typidArg == TypeId::UnresolvedHasCandidates);

								// TODO: try to find exactly 1 candidate... just like we do if funcexpr is a symbolexpr
								AssertInfo(false, "TODO");

								// TODO: if exactly 1 is found, change the symbexprk of the underlying arg
							}
						}
					} break;

					default:
						AssertNotReached;
						break;
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

						TypeId typidCandidate = TypeId::Unresolved;
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

						const Type * pTypeCandidate = lookupType(*typeTable, typidCandidate);
						Assert(pTypeCandidate);

						if (pTypeCandidate->typek != TYPEK_Func)
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
							TypeId typidCandidateExpected = pFuncTypeCandidate->paramTypids[iParamCandidate];

							auto * pNodeArg = pExpr->apArgs[iParamCandidate];
							TypeId typidArg = aTypidArg[iParamCandidate];

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
								Assert(typidArg == TypeId::UnresolvedHasCandidates);
								Assert(pNodeArg->astk == ASTK_SymbolExpr);

								auto * pNodeArgSymbExpr = Down(pNodeArg, SymbolExpr);
								Assert(pNodeArgSymbExpr->symbexprk == SYMBEXPRK_Unresolved);
								Assert(pNodeArgSymbExpr->unresolvedData.aCandidates.cItem > 1);

								int cMatchExact = 0;
								int cMatchLoose = 0;
								TypeId typidMatchExact = TypeId::Unresolved;
								TypeId typidMatchLoose = TypeId::Unresolved;
								for (int iArgCandidate = 0; iArgCandidate < pNodeArgSymbExpr->unresolvedData.aCandidates.cItem; iArgCandidate++)
								{
									SymbolInfo symbInfoArgCandidate = pNodeArgSymbExpr->unresolvedData.aCandidates[iArgCandidate];
									
									TypeId typidArgCandidate = TypeId::Unresolved;
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
											typidMatchLoose = TypeId::Unresolved;
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
									typidResult = TypeId::TypeError;
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
						typidResult = TypeId::TypeError;
						goto LEndSetTypidAndReturn;
					}
					else
					{
						// TODO: better reporting

						print("No func named ");
						print(pFuncSymbolExpr->ident.strv);
						print(" matches the provided parameters");
						println();
						typidResult = TypeId::TypeError;
						goto LEndSetTypidAndReturn;
					}
				}

				Assert(pNodeDefnclMatch);
				const Type * pType = nullptr;
				if (pNodeDefnclMatch->astk == ASTK_VarDeclStmt)
				{
					auto * pNodeVarDeclStmt = Down(pNodeDefnclMatch, VarDeclStmt);
					Assert(isTypeResolved(pNodeVarDeclStmt->typidDefn));

					pType = lookupType(*typeTable, pNodeVarDeclStmt->typidDefn);

					pFuncSymbolExpr->symbexprk = SYMBEXPRK_Var;
					pFuncSymbolExpr->varData.pDeclCached = pNodeVarDeclStmt;
				}
				else
				{
					Assert(pNodeDefnclMatch->astk == ASTK_FuncDefnStmt);

					auto * pNodeFuncDefnStmt = Down(pNodeDefnclMatch, FuncDefnStmt);
					Assert(isTypeResolved(pNodeFuncDefnStmt->typidDefn));

					pType = lookupType(*typeTable, pNodeFuncDefnStmt->typidDefn);

					pFuncSymbolExpr->symbexprk = SYMBEXPRK_Func;
					pFuncSymbolExpr->funcData.pDefnCached = pNodeFuncDefnStmt;
				}

				Assert(pType);
				Assert(pType->typek == TYPEK_Func);
				if (pType->funcTypeData.funcType.returnTypids.cItem == 0)
				{
					typidResult = TypeId::Void;
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

			// TODO: Test that this assert actually holds... I think it shouldn't?

			Assert(isTypeResolved(UpExpr(pExpr)->typidEval));
			typidResult = UpExpr(pExpr)->typidEval;

			ResolvePass::FnCtx fnCtx = pop(&pPass->fnCtxStack);
			dispose(&fnCtx.aTypidReturn);

			pop(&pPass->scopeidStack);
		} break;

		default:
		{
			reportIceAndExit("Unknown astk in resolveExpr: %d", pNode->astk);
		} break;
	}

LEndSetTypidAndReturn:

	AstExpr * pExpr = DownExpr(pNode);
	pExpr->typidEval = typidResult;
}

void resolveStmt(ResolvePass * pPass, AstNode * pNode)
{
	Assert(category(pNode->astk) == ASTCATK_Stmt);

	MeekCtx * pCtx = pPass->pCtx;
	TypeTable * typeTable = pCtx->typeTable;

	switch (pNode->astk)
	{
		case ASTK_ExprStmt:
			break;

		case ASTK_AssignStmt:
		{
			// TODO: Need to figure out what the story is for lvalues and rvalues
			//	and make sure that the LHS is an lvalue

			auto * pStmt = Down(pNode, AssignStmt);

			TypeId typidLhs = DownExpr(pStmt->pLhsExpr)->typidEval;
			TypeId typidRhs = DownExpr(pStmt->pLhsExpr)->typidEval;

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
			const Type * pType = lookupType(*typeTable, pStmt->typidDefn);

			if (pStmt->ident.lexeme.strv.cCh > 0)
			{
#if DEBUG
				SCOPEID scopeidCur = peek(pPass->scopeidStack);
				Scope * pScopeCur = pCtx->scopes[scopeidCur];
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
				TypeId typidInit = DownExpr(pStmt->pInitExpr)->typidEval;
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
			SCOPEID scopeidCur = peek(pPass->scopeidStack);
			Scope * pScopeOuter = pCtx->scopes[scopeidCur];
			SymbolInfo symbInfo = lookupTypeSymbol(*pScopeOuter, pStmt->ident.lexeme);
			Assert(symbInfo.symbolk == SYMBOLK_Struct);
#endif
			pop(&pPass->scopeidStack);
		} break;

		case ASTK_FuncDefnStmt:
		{
			auto * pStmt = Down(pNode, FuncDefnStmt);

#if DEBUG
			{
				SCOPEID scopeidCur = peek(pPass->scopeidStack);
				Scope * pScopeCur = pCtx->scopes[scopeidCur];
				DynamicArray<SymbolInfo> aSymbInfo;
				init(&aSymbInfo);
				Defer(dispose(&aSymbInfo));
				lookupFuncSymbol(*pScopeCur, pStmt->ident.lexeme, &aSymbInfo);
				AssertInfo(aSymbInfo.cItem > 0, "We should have put this func decl in the symbol table when we parsed it...");
			}
#endif
			
			pop(&pPass->scopeidStack);
		} break;

		case ASTK_BlockStmt:
		{
			auto * pStmt = Down(pNode, BlockStmt);
			if (!pStmt->inheritsParentScopeid)
			{
				pop(&pPass->scopeidStack);
			}
		} break;

		case ASTK_WhileStmt:
		{
			auto * pStmt = Down(pNode, WhileStmt);

			if (DownExpr(pStmt->pCondExpr)->typidEval != TypeId::Bool)
			{
				print("Expression in while condition must be a bool");
			}

			pPass->cNestedBreakable--;
		} break;

		case ASTK_IfStmt:
		{
			auto * pStmt = Down(pNode, IfStmt);

			if (DownExpr(pStmt->pCondExpr)->typidEval != TypeId::Bool)
			{
				print("Expression in if condition must be a bool");
			}
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
				TypeId typidReturn = DownExpr(pStmt->pExpr)->typidEval;

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

		case ASTK_PrintStmt:
			break;

		default:
		{
			reportIceAndExit("Unknown astk in resolveStmt: %d", pNode->astk);
		} break;
	}
}

void doResolvePass(ResolvePass * pPass, AstNode * pNode)
{
	pushAndProcessScope(pPass, SCOPEID_BuiltIn);
	pushAndProcessScope(pPass, SCOPEID_Global);

	walkAst(pPass->pCtx, pNode, &visitResolvePreorder, visitResolveHook, visitResolvePostorder, pPass);
}

bool visitResolvePreorder(AstNode * pNode, void * pPass_)
{
	Assert(pNode);

	if (isErrorNode(*pNode))
		return true;	// Should we continue here? An error might have some well-formed children...

	ResolvePass * pPass = reinterpret_cast<ResolvePass *>(pPass_);
	switch (pNode->astk)
	{
		case ASTK_UnopExpr:
		case ASTK_BinopExpr:
		case ASTK_LiteralExpr:
		case ASTK_GroupExpr:
		case ASTK_SymbolExpr:
		case ASTK_PointerDereferenceExpr:
		case ASTK_ArrayAccessExpr:
		case ASTK_FuncCallExpr:
			break;

		case ASTK_FuncLiteralExpr:
		{
			/*pop(&pPass->scopeidStack);
			ResolvePassCtx::FnCtx fnCtx = pop(&pPass->fnCtxStack);
			dispose(&fnCtx.aTypidReturn);*/

			auto * pExpr = Down(pNode, FuncLiteralExpr);
			pushAndProcessScope(pPass, pExpr->scopeid);
		} break;

		case ASTK_ExprStmt:
		case ASTK_AssignStmt:
		case ASTK_VarDeclStmt:
			break;

		case ASTK_FuncDefnStmt:
		{
			auto * pStmt = Down(pNode, FuncDefnStmt);
			pushAndProcessScope(pPass, pStmt->scopeid);
		} break;

		case ASTK_StructDefnStmt:
		{
			auto * pStmt = Down(pNode, StructDefnStmt);
			pushAndProcessScope(pPass, pStmt->scopeid);
		} break;

		case ASTK_IfStmt:
			break;

		case ASTK_WhileStmt:
		{
			pPass->cNestedBreakable++;
		} break;

		case ASTK_BlockStmt:
		{
			auto * pStmt = Down(pNode, BlockStmt);
			if (!pStmt->inheritsParentScopeid)
			{
				pushAndProcessScope(pPass, pStmt->scopeid);
			}
		} break;

		case ASTK_ReturnStmt:
		case ASTK_BreakStmt:
		case ASTK_ContinueStmt:
		case ASTK_PrintStmt:
		case ASTK_ParamsReturnsGrp:
		case ASTK_Program:
			break;

		default:
			AssertNotReached;
			break;
	}

	return true;
}

void visitResolvePostorder(AstNode * pNode, void * pPass_)
{
	Assert(pNode);

	ResolvePass * pPass = reinterpret_cast<ResolvePass *>(pPass_);
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
			break;

		case ASTCATK_Grp:
			break;

		default:
			AssertNotReached;
	}
}

void visitResolveHook(AstNode * pNode, AWHK awhk, void * pPass_)
{
	if (awhk != AWHK_FuncPostFormalReturnVardecls)
		return;

	ResolvePass * pPass = reinterpret_cast<ResolvePass *>(pPass_);
	AstParamsReturnsGrp * pGrp = nullptr;
	if (pNode->astk == ASTK_FuncLiteralExpr)
	{
		pGrp = Down(pNode, FuncLiteralExpr)->pParamsReturnsGrp;
	}
	else
	{
		Assert(pNode->astk == ASTK_FuncDefnStmt);
		pGrp = Down(pNode, FuncDefnStmt)->pParamsReturnsGrp;
	}

	// Push ctx

	auto * pFnCtx = pushNew(&pPass->fnCtxStack);
	initExtract(&pFnCtx->aTypidReturn, pGrp->apReturnVarDecls, offsetof(AstVarDeclStmt, typidDefn));
}
