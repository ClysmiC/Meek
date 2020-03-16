#include "resolve_pass.h"

#include "ast.h"
#include "error.h"
#include "print.h"
#include "symbol.h"
#include "type.h"

#define SetBubbleIfUnresolved(typid) do { if (!isTypeResolved(typid)) (typid) = TYPID_BubbleError; } while(0)

void init(ResolvePass * pPass)
{
	pPass->lastSymbseqid = SYMBSEQID_Unset;
	init(&pPass->scopeStack);
	init(&pPass->unresolvedIdents);
	init(&pPass->fnCtxStack);

	pPass->hadError = false;
	pPass->cNestedBreakable = 0;

	// HMM: Does this belong in init?

	{
		Scope scopeBuiltIn;
		scopeBuiltIn.id = SCOPEID_BuiltIn;
		scopeBuiltIn.scopek = SCOPEK_BuiltIn;
		push(&pPass->scopeStack, scopeBuiltIn);

		Scope scopeGlobal;
		scopeGlobal.id = SCOPEID_Global;
		scopeGlobal.scopek = SCOPEK_Global;
		push(&pPass->scopeStack, scopeGlobal);
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
					typidResult = ensureInTypeTable(pPass->pTypeTable, typePtr);
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
					Assert(pExpr->unresolvedData.apCandidates.cItem == 0);

					//
					// NOTE (andrew) We are only responsible for gathering the candidates here. The parent node is responsible for
					//	for choosing one. The order that we collect the candidates matter, as the selection algorithm is as follows:
					//

					// Lookup all funcs matching this identifier

					{
						lookupFuncSymb(*pPass->pSymbTable, pExpr->pTokenIdent, pPass->scopeStack, &pExpr->unresolvedData.apCandidates);
					}

					// Lookup var matching this identifier, and slot it in where it fits

					{
						SymbolInfo * pSymbInfoVar = lookupVarSymb(*pPass->pSymbTable, pExpr->pTokenIdent, pPass->scopeStack);

						if (pSymbInfoVar)
						{
							Assert(pSymbInfoVar->symbolk == SYMBOLK_Var);
							SCOPEID scopeidVar = pSymbInfoVar->pVarDeclStmt->ident.defnclScopeid;

							int iInsert = 0;
							for (int iCandidate = 0; iCandidate < pExpr->unresolvedData.apCandidates.cItem; iCandidate++)
							{
								SymbolInfo * pSymbInfoCandidate = pExpr->unresolvedData.apCandidates[iCandidate];
								Assert(pSymbInfoCandidate->symbolk == SYMBOLK_Func);

								SCOPEID scopeidFunc = pSymbInfoCandidate->pFuncDefnStmt->ident.defnclScopeid;
								
								if (scopeidVar >= scopeidFunc)
								{
									iInsert = iCandidate;
									break;
								}
							}

							insert(&pExpr->unresolvedData.apCandidates, pSymbInfoVar, iInsert);
						}
					}

					if (pExpr->unresolvedData.apCandidates.cItem == 1)
					{
						SymbolInfo * pSymbInfo = pExpr->unresolvedData.apCandidates[0];
						if (pSymbInfo->symbolk == SYMBOLK_Var)
						{
							pExpr->symbexprk = SYMBEXPRK_Var;
							pExpr->varData.pDeclCached = pSymbInfo->pVarDeclStmt;

							typidResult = pExpr->varData.pDeclCached->typid;
						}
						else
						{
							Assert(pSymbInfo->symbolk == SYMBOLK_Func);

							pExpr->symbexprk = SYMBEXPRK_Func;
							pExpr->funcData.pDefnCached = pSymbInfo->pFuncDefnStmt;

							typidResult = pExpr->funcData.pDefnCached->typid;
						}
					}
					else if (pExpr->unresolvedData.apCandidates.cItem > 1)
					{
						typidResult = TYPID_UnresolvedHasCandidates;
					}
					else
					{
						print("Unresolved variable ");
						print(pExpr->pTokenIdent->lexeme);
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
					SCOPEID ownerScopeid = SCOPEID_Nil;

					if (!isTypeResolved(ownerTypid))
					{
						typidResult = TYPID_BubbleError;
						goto LEndSetTypidAndReturn;
					}

					const Type * pOwnerType = lookupType(*pPass->pTypeTable, ownerTypid);
					Assert(pOwnerType);

					SymbolInfo * pSymbInfoOwner = lookupTypeSymb(*pPass->pSymbTable, pOwnerType->ident);
					Assert(pSymbInfoOwner);

					if (pSymbInfoOwner->symbolk == SYMBOLK_Struct)
					{
						ownerScopeid = pSymbInfoOwner->pStructDefnStmt->scopeid;
					}

					ScopedIdentifier candidate;
					setIdent(&candidate, pExpr->pTokenIdent, ownerScopeid);
					SymbolInfo * pSymbInfo = lookupVarSymb(*pPass->pSymbTable, candidate);

					if (!pSymbInfo)
					{
						// Unresolved
						// TODO: Add this to a resolve error list... don't print inline right here!

						print("Unresolved member variable ");
						print(pExpr->pTokenIdent->lexeme );
						println();

						typidResult = TYPID_Unresolved;
						goto LEndSetTypidAndReturn;
					}

					// Resolve!

					pExpr->memberData.pDeclCached = pSymbInfo->pVarDeclStmt;
					typidResult = pSymbInfo->pVarDeclStmt->typid;
				} break;

				case SYMBEXPRK_Func:
				{
					AssertInfo(!pExpr->funcData.pDefnCached, "This shouldn't be resolved yet because this is the code that should resolve it!");

					DynamicArray<SymbolInfo *> apSymbInfoFuncCandidates;
					init(&apSymbInfoFuncCandidates);
					Defer(dispose(&apSymbInfoFuncCandidates));

					lookupFuncSymb(*pPass->pSymbTable, pExpr->pTokenIdent, pPass->scopeStack, &apSymbInfoFuncCandidates);

					AstFuncDefnStmt * pFuncDefnStmtMatch = nullptr;
					for (int iCandidate = 0; iCandidate < apSymbInfoFuncCandidates.cItem; iCandidate++)
					{
						Assert(apSymbInfoFuncCandidates[iCandidate]->symbolk == SYMBOLK_Func);

						AstFuncDefnStmt * pCandidateDefnStmt = apSymbInfoFuncCandidates[iCandidate]->pFuncDefnStmt;
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

							TYPID typidCandidate = pNode->typid;
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
						typidResult = pFuncDefnStmtMatch->typid;
					}
					else
					{
						// Unresolved
						// TODO: Add this to a resolve error list... don't print inline right here!

						print("Unresolved func identifier ");
						print(pExpr->pTokenIdent->lexeme);
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

			typidResult = ensureInTypeTable(pPass->pTypeTable, typeDereferenced);
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
			if (typidSubscript != TYPID_Int)
			{
				// TODO: support uint, etc.
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

			typidResult = ensureInTypeTable(pPass->pTypeTable, typeElement);
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
					const FuncType * pFuncType = &pType->funcType;

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

				// Defer(UpExpr(pFuncSymbolExpr)->typid = typidResult);		// This is pretty gross


				// Do matching between func candidates and arg candidates

				AstNode * pNodeDefnclMatch = nullptr;
				if (pFuncSymbolExpr->symbexprk == SYMBEXPRK_Func)
				{
					pNodeDefnclMatch = Up(pFuncSymbolExpr->funcData.pDefnCached);
				}
				else
				{
					Assert(pFuncSymbolExpr->symbexprk == SYMBEXPRK_Unresolved);

					// NOTE (andrew) The ordering of the candidates was set by resolveExpr(..), and is important for correctness. To facilitate
					//	easy removal from the array, we want to iterate backwards. Thus, we reverse the array to allow us to iterate backwards
					//	but maintain the intended order. We reverse back after we are finished to maintain original order for posterity.

					DynamicArray<SymbolInfo *> apSymbInfoExactMatch;
					init(&apSymbInfoExactMatch);
					Defer(dispose(&apSymbInfoExactMatch));

					DynamicArray<SymbolInfo *> apSymbInfoLooseMatch;
					init(&apSymbInfoLooseMatch);
					Defer(dispose(&apSymbInfoLooseMatch));

					for (int iFuncCandidate = 0; iFuncCandidate < pFuncSymbolExpr->unresolvedData.apCandidates.cItem; iFuncCandidate++)
					{
						SymbolInfo * pSymbInfoFuncCandidate = pFuncSymbolExpr->unresolvedData.apCandidates[iFuncCandidate];

						TYPID typidCandidate = TYPID_Unresolved;
						if (pSymbInfoFuncCandidate->symbolk == SYMBOLK_Var)
						{
							typidCandidate = pSymbInfoFuncCandidate->pVarDeclStmt->typid;	
						}
						else
						{
							Assert(pSymbInfoFuncCandidate->symbolk == SYMBOLK_Func);
							typidCandidate = pSymbInfoFuncCandidate->pFuncDefnStmt->typid;
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

						const FuncType * pFuncTypeCandidate = &pTypeCandidate->funcType;

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
								Assert(pNodeArgSymbExpr->unresolvedData.apCandidates.cItem > 1);

								int cMatchExact = 0;
								int cMatchLoose = 0;
								TYPID typidMatchExact = TYPID_Unresolved;
								TYPID typidMatchLoose = TYPID_Unresolved;
								for (int iArgCandidate = 0; iArgCandidate < pNodeArgSymbExpr->unresolvedData.apCandidates.cItem; iArgCandidate++)
								{
									SymbolInfo * pSymbInfoArgCandidate = pNodeArgSymbExpr->unresolvedData.apCandidates[iArgCandidate];
									
									TYPID typidArgCandidate = TYPID_Unresolved;
									if (pSymbInfoArgCandidate->symbolk == SYMBOLK_Var)
									{
										typidArgCandidate = pSymbInfoArgCandidate->pVarDeclStmt->typid;
									}
									else
									{
										Assert(pSymbInfoArgCandidate->symbolk == SYMBOLK_Func);
										typidArgCandidate = pSymbInfoArgCandidate->pFuncDefnStmt->typid;
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
									print(pFuncSymbolExpr->pTokenIdent->lexeme);
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
								append(&apSymbInfoExactMatch, pSymbInfoFuncCandidate);
							} break;

							case MATCHK_MatchLoose:
							{
								append(&apSymbInfoLooseMatch, pSymbInfoFuncCandidate);
							} break;
						}
					}

					Assert(apSymbInfoExactMatch.cItem <= 1);		// TODO: I don't think this is assertable... there could be a func var and a fn def that both exact match...

					if (apSymbInfoExactMatch.cItem == 1)
					{
						SymbolInfo * pSymbInfoExactMatch = apSymbInfoExactMatch[0];
						if (pSymbInfoExactMatch->symbolk == SYMBOLK_Var)
						{
							pNodeDefnclMatch = Up(pSymbInfoExactMatch->pVarDeclStmt);
						}
						else
						{
							Assert(pSymbInfoExactMatch->symbolk == SYMBOLK_Func);
							pNodeDefnclMatch = Up(pSymbInfoExactMatch->pFuncDefnStmt);
						}
					}
					else if (apSymbInfoLooseMatch.cItem == 1)
					{
						SymbolInfo * pSymbInfoExactMatch = apSymbInfoLooseMatch[0];
						if (pSymbInfoExactMatch->symbolk == SYMBOLK_Var)
						{
							pNodeDefnclMatch = Up(pSymbInfoExactMatch->pVarDeclStmt);
						}
						else
						{
							Assert(pSymbInfoExactMatch->symbolk == SYMBOLK_Func);
							pNodeDefnclMatch = Up(pSymbInfoExactMatch->pFuncDefnStmt);
						}
					}
					else if (apSymbInfoLooseMatch.cItem > 1)
					{
						// TODO: better reporting

						print("Ambiguous function call ");
						print(pFuncSymbolExpr->pTokenIdent->lexeme);
						typidResult = TYPID_TypeError;
						goto LEndSetTypidAndReturn;
					}
					else
					{
						// TODO: better reporting

						print("No func named ");
						print(pFuncSymbolExpr->pTokenIdent->lexeme);
						print(" matches the provided parameters");
						typidResult = TYPID_TypeError;
						goto LEndSetTypidAndReturn;
					}
				}

				Assert(pNodeDefnclMatch);
				const Type * pType = nullptr;
				if (pNodeDefnclMatch->astk == ASTK_VarDeclStmt)
				{
					auto * pNodeVarDeclStmt = Down(pNodeDefnclMatch, VarDeclStmt);
					Assert(isTypeResolved(pNodeVarDeclStmt->typid));

					pType = lookupType(*pPass->pTypeTable, pNodeVarDeclStmt->typid);

					pFuncSymbolExpr->symbexprk = SYMBEXPRK_Var;
					pFuncSymbolExpr->varData.pDeclCached = pNodeVarDeclStmt;
				}
				else
				{
					Assert(pNodeDefnclMatch->astk == ASTK_FuncDefnStmt);

					auto * pNodeFuncDefnStmt = Down(pNodeDefnclMatch, FuncDefnStmt);
					Assert(isTypeResolved(pNodeFuncDefnStmt->typid));

					pType = lookupType(*pPass->pTypeTable, pNodeFuncDefnStmt->typid);

					pFuncSymbolExpr->symbexprk = SYMBEXPRK_Func;
					pFuncSymbolExpr->funcData.pDefnCached = pNodeFuncDefnStmt;
				}

				Assert(pType);
				Assert(pType->isFuncType);
				if (pType->funcType.returnTypids.cItem == 0)
				{
					typidResult = TYPID_Void;
				}
				else
				{
					// TODO: multiple return values

					Assert(pType->funcType.returnTypids.cItem == 1);
					typidResult = pType->funcType.returnTypids[0];
				}
			}
		} break;

		case ASTK_FuncLiteralExpr:
		{
			auto * pExpr = Down(pNode, FuncLiteralExpr);

			// NOTE: Func literal expressions define a type that we should have already put into the type table when parsing,
			//	so we just need to make sure we resolve the children here.

			Assert(isTypeResolved(UpExpr(pExpr)->typid));
			typidResult = UpExpr(pExpr)->typid;

			// Push scope and resolve children

			Scope scope;
			scope.id = pExpr->scopeid;
			scope.scopek = SCOPEK_CodeBlock;

			push(&pPass->scopeStack, scope);
			Defer(pop(&pPass->scopeStack));

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
			initExtract(&pFnCtx->aTypidReturn, *papReturnVarDecls, offsetof(AstVarDeclStmt, typid));
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
	pExpr->typid = typidResult;
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
			AssertInfo(isScopeSet(pStmt->ident), "The name of the thing you are declaring should be resolved to itself...");

			AssertInfo(isTypeResolved(pStmt->typid), "Inferred types are TODO");
			const Type * pType = lookupType(*pPass->pTypeTable, pStmt->typid);

			if (pStmt->ident.pToken)
			{
#if DEBUG
				SymbolInfo * pSymbInfo = lookupVarSymb(*pPass->pSymbTable, pStmt->ident);
				AssertInfo(pSymbInfo, "We should have put this var decl in the symbol table when we parsed it...");
				Assert(pSymbInfo->symbolk == SYMBOLK_Var);

				// HMM: This assert seems like it should be fine, but it fires when we have a redefined variable, since the 2nd defn
				//  will find the first defn in the symbol table. We *do* report that error elsewhere, so it would be nice to know at this
				//  point if we have reported the error so that we could assert EITHER that it has been reported, or that the below assert
				//  is true.

				// AssertInfo(pSymbInfo->pVarDeclStmt == pStmt, "At var declaration we should be able to look up the var in the symbol table and find ourselves...");
#endif
				// Record symbseqid for variable

				pPass->lastSymbseqid = pStmt->symbseqid;
			}

			// Resolve init expr

			// TODO: Need to get the type of the initExpr and compare it to the type of the lhs...
			//	just like in assignment

			if (pStmt->pInitExpr)
			{
				TYPID typidInit = resolveExpr(pPass, pStmt->pInitExpr);

				if (isTypeResolved(typidInit) && typidInit != pStmt->typid)
				{
					// TODO: report better error
					printfmt("Cannot initialize variable of type %d with expression of type %d\n", pStmt->typid, typidInit);
				}
			}
		} break;

		case ASTK_StructDefnStmt:
		{
			auto * pStmt = Down(pNode, StructDefnStmt);

#if DEBUG
			// Verify assumptions

			SymbolInfo * pSymbInfo = lookupTypeSymb(*pPass->pSymbTable, pStmt->ident);
			AssertInfo(pSymbInfo, "We should have put this struct decl in the symbol table when we parsed it...");
			Assert(pSymbInfo->symbolk == SYMBOLK_Struct);
			AssertInfo(pSymbInfo->pStructDefnStmt == pStmt, "At struct definition we should be able to look up the struct in the symbol table and find ourselves...");
#endif

			// Record sequence id

			Assert(pStmt->symbseqid != SYMBSEQID_Unset);
			pPass->lastSymbseqid = pStmt->symbseqid;

			// Record scope id

			Scope scope;
			scope.id = pStmt->scopeid;
			scope.scopek = SCOPEK_StructDefn;

			push(&pPass->scopeStack, scope);
			Defer(pop(&pPass->scopeStack));

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
			{
				// Verify assumptions

				DynamicArray<SymbolInfo * > apSymbInfo;
				init(&apSymbInfo);
				Defer(dispose(&apSymbInfo));

				lookupFuncSymb(*pPass->pSymbTable, pStmt->ident, &apSymbInfo);

				AssertInfo(apSymbInfo.cItem > 0, "We should have put this func decl in the symbol table when we parsed it...");
				bool found = false;

				for (int i = 0; i < apSymbInfo.cItem; i++)
				{
					SymbolInfo * pSymbInfo = apSymbInfo[i];
					Assert(pSymbInfo->symbolk == SYMBOLK_Func);

					if (pStmt->typid == pSymbInfo->pFuncDefnStmt->typid)
					{
						found = true;
						break;
					}
				}

				AssertInfo(found, "At struct definition we should be able to look up the struct in the symbol table and find ourselves...");
			}
#endif

			// Record sequence id

			Assert(pStmt->symbseqid != SYMBSEQID_Unset);
			pPass->lastSymbseqid = pStmt->symbseqid;

			// Record scope id

			Scope scope;
			scope.id = pStmt->scopeid;
			scope.scopek = SCOPEK_CodeBlock;

			push(&pPass->scopeStack, scope);
			Defer(pop(&pPass->scopeStack));

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
			initExtract(&pFnCtx->aTypidReturn, *papReturnVarDecls, offsetof(AstVarDeclStmt, typid));
			Defer(dispose(&pFnCtx->aTypidReturn));

			// Resolve body

			doResolvePass(pPass, pStmt->pBodyStmt);

			// Pop ctx

			pop(&pPass->fnCtxStack);
		} break;

		case ASTK_BlockStmt:
		{
			// NOTE: For functions, block statements aren't responsible for pushing a new scope, since
			//	the block scope should be the same as the scope of the params. That's why we only
			//	push/pop if we are actually a different scope!

			auto * pStmt = Down(pNode, BlockStmt);

			bool shouldPushPop = peek(pPass->scopeStack).id != pStmt->scopeid;

			if (shouldPushPop)
			{
				Scope scope;
				scope.id = pStmt->scopeid;
				scope.scopek = SCOPEK_CodeBlock;

				push(&pPass->scopeStack, scope);
			}
			Defer(if (shouldPushPop) pop(&pPass->scopeStack));

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

void reportUnresolvedIdentError(ResolvePass * pPass, ScopedIdentifier ident)
{
	// TODO: Insert into some sort of error proxy table. But I would need all lookups to also check this table!

	// SymbolInfo info;
	// setSymbolInfo(&info, ident, SYMBOLK_ErrorProxy, nullptr);
	// Verify(tryInsert(pPass->pSymbolTable, ident, info));
}
