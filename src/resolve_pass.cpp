#include "resolve_pass.h"

#include "ast.h"
#include "error.h"
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

	TYPID typidResult;

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
				goto end;
			}

			if (typidLhs != typidRhs)
			{
				// TODO: report error

				printf("Binary operator type mismatch. L: %d, R: %d\n", typidLhs, typidRhs);
				typidResult = TYPID_TypeError;
				goto end;
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
				goto end;
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

        case ASTK_VarExpr:
        {
			// HMM: How to handle func names as VarExprs... FuncCallExpr has special resolve code to took up varexpr's in the func symbol table,
			//	but if I ever want a func varExpr outside of a function call (like passing a function to an argument or assigning it to a
			//	variable) then I will need to be smarter here... it's hard though because you need to have some concept of an "expected type"
			//	to properly handle overloaded function names...

            auto * pExpr = Down(pNode, VarExpr);

			AssertInfo(!pExpr->pResolvedDecl, "This shouldn't be resolved yet because this is the code that should resolve it!");

            if (pExpr->pOwner)
            {
                TYPID ownerTypid = resolveExpr(pPass, pExpr->pOwner);
				SCOPEID ownerScopeid = SCOPEID_Nil;

				if (!isTypeResolved(ownerTypid))
				{
					typidResult = TYPID_BubbleError;
                    goto end;
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

					// TODO: report unresolved member ident
					printf("Unresolved member variable %s\n", pExpr->pTokenIdent->lexeme);

					typidResult = TYPID_Unresolved;
					goto end;
				}

				// Resolve!

				pExpr->pResolvedDecl = pSymbInfo->pVarDeclStmt;
				typidResult = pSymbInfo->pVarDeclStmt->typid;
            }
            else
            {
				bool found = false;
				for (int i = 0; i < count(pPass->scopeStack); i++)
				{
					SCOPEID scopeidCandidate = peekFar(pPass->scopeStack, i).id;

					ScopedIdentifier candidate;
					setIdent(&candidate, pExpr->pTokenIdent, scopeidCandidate);
					SymbolInfo * pSymbInfo = lookupVarSymb(*pPass->pSymbTable, candidate);

					if (pSymbInfo && pSymbInfo->pVarDeclStmt->symbseqid <= pPass->lastSymbseqid)
					{
						// Resolve!

						pExpr->pResolvedDecl = pSymbInfo->pVarDeclStmt;
						typidResult = pSymbInfo->pVarDeclStmt->typid;

						found = true;
						break;
					}
				}

				if (!found)
				{
					// Unresolved

					// TODO: report unresolved var ident
					printf("Unresolved variable %s\n", pExpr->pTokenIdent->lexeme);

					// HMM: Is this the right result value? This isn't really a type error since it is more of an unresolved identifier error

					typidResult = TYPID_Unresolved;
				}
            }
        } break;

		case ASTK_PointerDereferenceExpr:
		{
			auto * pExpr = Down(pNode, PointerDereferenceExpr);

			TYPID typidPtr = resolveExpr(pPass, pExpr->pPointerExpr);

			if (!isTypeResolved(typidPtr))
			{
				typidResult = TYPID_BubbleError;
				goto end;
			}

			const Type * pTypePtr = lookupType(*pPass->pTypeTable, typidPtr);
			Assert(pTypePtr);

			if (!isPointerType(*pTypePtr))
			{
				printf("Trying to dereferenec a non-pointer\n");
				typidResult = TYPID_TypeError;
				goto end;
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
				goto end;
			}

			TYPID typidSubscript = resolveExpr(pPass, pExpr->pSubscriptExpr);
			if (typidSubscript != TYPID_Int)
			{
				// TODO: support uint, etc.
				// TODO: catch negative compile time constants

				// TODO: report error here, but keep on chugging with the resolve... despite the incorrect subscript, we still
				//	know what type an array access will end up as.

				printf("Trying to access array with a non integer\n");
			}

			const Type * pTypeArray = lookupType(*pPass->pTypeTable, typidArray);
			Assert(pTypeArray);

			if (pTypeArray->aTypemods.cItem == 0 || pTypeArray->aTypemods[0].typemodk != TYPEMODK_Array)
			{
				// TODO: More specific type error. (trying to access non-array <identifier> as if it were an array)
				//	How am I going to report these errors? Should I just maintain a list separate from the
				//	fact that I am returning error TYPID's?

				printf("Trying to access non-array as if it were an array...\n");
				typidResult = TYPID_TypeError;
				goto end;
			}

			Type typeElement;
			initCopy(&typeElement, *pTypeArray);
			Defer(dispose(&typeElement));

			remove(&typeElement.aTypemods, 0);

			typidResult = ensureInTypeTable(pPass->pTypeTable, typeElement);
        } break;

        case ASTK_FuncCallExpr:
        {
			// TODO: For now I am just going to assert that functions return a single value, even though the grammar allows
			//	multiple return values. I still need to figure out what I want the semantics of MRV to be... do I want a
			//	more general concept of "tuple" values that MRV leverages, or do I want MRV to be something that is just
			//	baked in as a special case to things like assignment and parameter lists.


            auto * pExpr = Down(pNode, FuncCallExpr);

			if (pExpr->pFunc->astk == ASTK_VarExpr)
			{
				auto * pFuncExpr = Down(pExpr->pFunc, VarExpr);

				// NOTE: For now I am resolving the VarExpr directly in-line, since the var expr case of this switch assumes a var
				//	and not a func. This is pretty much a hack until I think of a better way to model this.

				Defer(UpExpr(pFuncExpr)->typid = typidResult);		// This is pretty gross

				if (pFuncExpr->pOwner)
				{
					// TODO: This is where I would handle UFCS
					// TODO: report error here while I don't support UFCS
					// NOTE: We aren't ever visiting the parent(s) here or setting their typids to unresolved.... bleh.

					printf("Member function looking thingy... not supported\n");

					typidResult = TYPID_Unresolved;	// Is this right?
					goto end;
				}

				// Resolve args

				// TODO: implement reserve(..) for dynamic array, since we already know ahead of time
				//	how many typids we will have in it.
				DynamicArray<TYPID> aTypidArg;
				init(&aTypidArg);
				Defer(dispose(&aTypidArg));

				for (int i = 0; i < pExpr->apArgs.cItem; i++)
				{
					TYPID typidArg = resolveExpr(pPass, pExpr->apArgs[i]);
					append(&aTypidArg, typidArg);
				}

				// Find func with matching ident and param types

				{
					DynamicArray<SymbolInfo> aFuncCandidates;
					init(&aFuncCandidates);
					Defer(dispose(&aFuncCandidates));

					lookupFuncSymb(*pPass->pSymbTable, pFuncExpr->pTokenIdent, pPass->scopeStack, &aFuncCandidates);

					AstFuncDefnStmt * pFuncDefnStmtMatch = nullptr;
					for (int i = 0; i < aFuncCandidates.cItem; i++)
					{
						SymbolInfo * pSymbInfoCandidate = &aFuncCandidates[i];
						Assert(pSymbInfoCandidate->symbolk == SYMBOLK_Func);

						AstFuncDefnStmt * pFuncDefnStmt = pSymbInfoCandidate->pFuncDefnStmt;

						if (areTypidListAndVarDeclListTypesEq(aTypidArg, pFuncDefnStmt->apParamVarDecls))
						{
							pFuncDefnStmtMatch = pFuncDefnStmt;
							break;
						}
					}

					if (!pFuncDefnStmtMatch)
					{
						// TODO: report error

						printf("Could not find function definition for %s that had expected types\n", pFuncExpr->pTokenIdent->lexeme);
						typidResult = TYPID_Unresolved;
						goto end;
					}

					if (pFuncDefnStmtMatch->apReturnVarDecls.cItem == 0)
					{
						typidResult = TYPID_Void;
					}
					else
					{
						AssertInfo(pFuncDefnStmtMatch->apReturnVarDecls.cItem == 1, "TODO: figure out semantics for multiple return values.... for now I will just assert that there is only 1");
						Assert(pFuncDefnStmtMatch->apReturnVarDecls[0]->astk == ASTK_VarDeclStmt);
						typidResult = Down(pFuncDefnStmtMatch->apReturnVarDecls[0], VarDeclStmt)->typid;
					}
				}
			}
			else
			{
				// TODO: It's a non-var being called as a function. Resolve the calling expr,
				//	make sure it is a function type, then make sure our params match
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

			for (int i = 0; i < pExpr->apParamVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pExpr->apParamVarDecls[i]);
			}

			for (int i = 0; i < pExpr->apReturnVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pExpr->apReturnVarDecls[i]);
			}

			// Push ctx

			auto * pFnCtx = pushNew(&pPass->fnCtxStack);
			initExtract(&pFnCtx->aTypidReturn, pExpr->apReturnVarDecls, offsetof(AstVarDeclStmt, typid));
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

end:
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
				printf("Assigning to non-lvalue\n");
			}
			else
			{
				if (isTypeResolved(typidLhs) && isTypeResolved(typidRhs))
				{
					if (typidLhs != typidRhs)
					{
						// TODO: report error
						printf("Assignment type error. L: %d, R: %d\n", typidLhs, typidRhs);
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
					printf("Cannot initialize variable of type %d with expression of type %d\n", pStmt->typid, typidInit);
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

                DynamicArray<SymbolInfo> * paSymbInfo;
                paSymbInfo = lookupFuncSymb(*pPass->pSymbTable, pStmt->ident);

                AssertInfo(paSymbInfo && paSymbInfo->cItem > 0, "We should have put this func decl in the symbol table when we parsed it...");
                bool found = false;

                for (int i = 0; i < paSymbInfo->cItem; i++)
                {
                    SymbolInfo * pSymbInfo = &(*paSymbInfo)[i];
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

			for (int i = 0; i < pStmt->apParamVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pStmt->apParamVarDecls[i]);
			}

			// Resolve return values

			for (int i = 0; i < pStmt->apReturnVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pStmt->apReturnVarDecls[i]);
			}

			// Push ctx

			auto * pFnCtx = pushNew(&pPass->fnCtxStack);
			initExtract(&pFnCtx->aTypidReturn, pStmt->apReturnVarDecls, offsetof(AstVarDeclStmt, typid));
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
				printf("Cannot return value from function expecting void\n");
			}
			else if (pFnCtx->aTypidReturn.cItem != 0 && !pStmt->pExpr)
			{
				printf("Function expecting a return value\n");
			}

			if (pStmt->pExpr)
			{
				TYPID typidReturn = resolveExpr(pPass, pStmt->pExpr);

				// TODO: Handle multiple return values

				if (typidReturn != pFnCtx->aTypidReturn[0])
				{
					printf("Return type mismatch\n");
				}
			}
		} break;

		case ASTK_BreakStmt:
		{
			if (pPass->cNestedBreakable == 0)
			{
				printf("Cannot break from current context");
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
				printf("Cannot continue from current context");
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
