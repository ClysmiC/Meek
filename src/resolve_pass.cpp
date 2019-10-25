#include "resolve_pass.h"

#include "ast.h"
#include "error.h"
#include "symbol.h"
#include "type.h"

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
			//	Will change later!

            auto * pExpr = Down(pNode, BinopExpr);
            TYPID typidLhs = resolveExpr(pPass, pExpr->pLhsExpr);
            TYPID typidRhs = resolveExpr(pPass, pExpr->pRhsExpr);

			if (typidLhs != typidRhs)
			{
				// TODO: report type error
				return TYPID_TypeError;
			}

			typidResult = typidLhs;
			pExpr->typid = typidResult;
        } break;

        case ASTK_GroupExpr:
        {
            auto * pExpr = Down(pNode, GroupExpr);
            typidResult = resolveExpr(pPass, pExpr->pExpr);
			pExpr->typid = typidResult;
        } break;

        case ASTK_LiteralExpr:
        {
			auto * pExpr = Down(pNode, LiteralExpr);
			typidResult = typidFromLiteralk(pExpr->literalk);
			pExpr->typid = typidResult;
        } break;

        case ASTK_UnopExpr:
        {
            auto * pExpr = Down(pNode, UnopExpr);
            typidResult = resolveExpr(pPass, pExpr->pExpr);
			pExpr->typid = typidResult;
        } break;

        case ASTK_VarExpr:
        {
            auto * pExpr = Down(pNode, VarExpr);
			AssertInfo(!pExpr->pResolvedDecl, "This shouldn't be resolved yet because this is the code that should resolve it!");

            if (pExpr->pOwner)
            {
                TYPID ownerTypid = resolveExpr(pPass, pExpr->pOwner);
				SCOPEID ownerScopeid = SCOPEID_Nil;

				if (!isTypeResolved(ownerTypid))
				{
					return ownerTypid;
				}

				const Type * pOwnerType = lookupType(*pPass->pTypeTable, ownerTypid);
				Assert(pOwnerType);

				SymbolInfo * pSymbInfo = lookupTypeSymb(*pPass->pSymbTable, pOwnerType->ident);
				Assert(pSymbInfo);

				if (pSymbInfo->symbolk == SYMBOLK_Struct)
				{
					ownerScopeid = pSymbInfo->pStructDefnStmt->scopeid;
				}

                ScopedIdentifier candidate;
                setIdent(&candidate, pExpr->pTokenIdent, ownerScopeid);
                SymbolInfo * pSymbInfo = lookupVarSymb(*pPass->pSymbTable, candidate);

                if (pSymbInfo)
                {
                    // Resolve!

                    pExpr->pResolvedDecl = pSymbInfo->pVarDeclStmt;
					pExpr->typid = pSymbInfo->pVarDeclStmt->typid;
					typidResult = pSymbInfo->pVarDeclStmt->typid;
                }
                else
                {
                    // Unresolved

					// TODO: report unresolved member ident

                    return TYPID_Unresolved;	// HMM: Is this the right return value?
                }
            }
            else
            {
				bool found = false;
				for (int i = 0; i < count(pPass->scopeidStack); i++)
				{
					scopeid scopeidCandidate;
					Verify(peekFar(pPass->scopeidStack, i, &scopeidCandidate));

					ScopedIdentifier candidate;
					setIdent(&candidate, pExpr->pTokenIdent, scopeidCandidate);
					SymbolInfo * pSymbInfo = lookupVarSymb(*pPass->pSymbTable, candidate);

					if (pSymbInfo)
					{
						// Resolve!

						pExpr->pResolvedDecl = pSymbInfo->pVarDeclStmt;
						pExpr->typid = pSymbInfo->pVarDeclStmt->typid;
						typidResult = pSymbInfo->pVarDeclStmt->typid;

						found = true;
						break;
					}
				}

				if (!found)
				{
					// TODO: report unresolved member ident

                    return TYPID_Unresolved;	// HMM: Is this the right return value?
				}
            }
        } break;

        case ASTK_ArrayAccessExpr:
        {
            auto * pExpr = Down(pNode, ArrayAccessExpr);
            typidResult typidArray = resolveExpr(pPass, pExpr->pArray);
            resolveExpr(pPass, pExpr->pSubscript);

			if (!isTypeResolved(typidArray))
			{
				return typidArray;
			}

			// TODO:
			// - get Type from typid
			// - construct new type
			// - copy old type into new type (don't "move"... don't want to destroy old type!)
			// - check that first typemodifier is []
			//		- else, error (can't subscript a non-array!)
			// - remove first typemodifier from our copy
			// - ensure this type in the type table! need to call ensure because it could be
			//	possible for someone to take an array of a type that they otherwise don't
			//	declare anywhere, but we still want to capture that type's existence in the
			//	type table!

        } break;

        case ASTK_FuncCallExpr:
        {
            auto * pExpr = Down(pNode, FuncCallExpr);
            doResolvePass(pPass, pExpr->pFunc);

            for (int i = 0; i < pExpr->apArgs.cItem; i++)
            {
                doResolvePass(pPass, pExpr->apArgs[i]);
            }
        } break;

        case ASTK_FuncLiteralExpr:
        {
            auto * pExpr = Down(pNode, FuncLiteralExpr);

			push(&pPass->scopeidStack, pExpr->scopeid);
			Defer(pop(&pPass->scopeidStack));

			for (int i = 0; i < pExpr->apParamVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pExpr->apParamVarDecls[i]);
			}

			for (int i = 0; i < pExpr->apReturnVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pExpr->apReturnVarDecls[i]);
			}
        } break;

		default:
		{
			reportIceAndExit("Unknown astk in resolveExpr: %d", pNode->astk);
		} break;
	}

    // TODO: return scopeid
    // should I also return typid? Or should I embed typid's into the ast nodes even for exprs... probably...
}

void resolveStmt(ResolvePass * pPass, AstNode * pNode)
{
	Assert(category(pNode->astk) == ASTCATK_Expr);

	switch (pNode->astk)
	{
		case ASTK_ExprStmt:
		{
			auto * pStmt = Down(pNode, ExprStmt);
			doResolvePass(pPass, pStmt->pExpr);
		} break;

		case ASTK_AssignStmt:
		{
			auto * pStmt = Down(pNode, AssignStmt);
			doResolvePass(pPass, pStmt->pLhsExpr);
			doResolvePass(pPass, pStmt->pRhsExpr);
		} break;

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = Down(pNode, VarDeclStmt);
			AssertInfo(isScopeSet(pStmt->ident), "The name of the thing you are declaring should be resolved to itself...");

			AssertInfo(isTypeResolved(pStmt->typid), "Inferred types are TODO");
            const Type * pType = lookupType(*pPass->pTypeTable, pStmt->typid);

			if (pType->isFuncType)
			{
				// Resolve types of params

                // I think this is unnecessarry? This is just a type declaration, so the parameters/return values have optional,
                //  unbound names. The only names that matter are the type identifiers which should already be resolved.

				/*for (uint i = 0; i < pType->funcType.apParamVarDecls.cItem; i++)
				{
					doResolvePass(pPass, pType->pFuncType->apParamVarDecls[i]);
				}

				for (uint i = 0; i < pStmt->pType->pFuncType->apReturnVarDecls.cItem; i++)
				{
					doResolvePass(pPass, pStmt->pType->pFuncType->apReturnVarDecls[i]);
				}*/
			}
			else
			{
                // DITTO for above comment....

				//// Resolve type

				//AssertInfo(!isScopeSet(pStmt->pType->ident), "This shouldn't be resolved yet because this is the code that should resolve it!");

				//ScopedIdentifier candidate = pStmt->pType->ident;

				//for (uint i = 0; i < count(pPass->scopeidStack); i++)
				//{
				//	scopeid scopeid;
				//	Verify(peekFar(pPass->scopeidStack, i, &scopeid));
				//	resolveIdentScope(&candidate, scopeid);

				//	SymbolInfo * pSymbInfo = lookupStruct(pPass->pSymbTable, candidate);

				//	if(pSymbInfo)
				//	{
				//		// Resolve it!

				//		resolveIdentScope(&pStmt->pType->ident, scopeid);
				//		break;
				//	}
				//}

				//if (!isScopeSet(pStmt->pType->ident))
				//{
				//	reportUnresolvedIdentError(pPass, pStmt->pType->ident);
				//}
			}

			// Record symbseqid for variable

			if (pStmt->ident.pToken)
			{
#if DEBUG
				SymbolInfo * pSymbInfo = lookupVarSymb(*pPass->pSymbTable, pStmt->ident);
				AssertInfo(pSymbInfo, "We should have put this var decl in the symbol table when we parsed it...");
				Assert(pSymbInfo->symbolk == SYMBOLK_Var);
				AssertInfo(pSymbInfo->pVarDeclStmt == pStmt, "At var declaration we should be able to look up the var in the symbol table and find ourselves...");
#endif
				pPass->lastSymbseqid = pStmt->symbseqid;
			}

            // Resolve init expr

            if (pStmt->pInitExpr)
            {
                doResolvePass(pPass, pStmt->pInitExpr);
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

			Assert(pStmt->symbseqid != gc_symbseqidUnset);
			pPass->lastSymbseqid = pStmt->symbseqid;

			// Record scope id

			push(&pPass->scopeidStack, pStmt->scopeid);
			Defer(pop(&pPass->scopeidStack));

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

			Assert(pStmt->symbseqid != gc_symbseqidUnset);
			pPass->lastSymbseqid = pStmt->symbseqid;

			// Record scope id

			push(&pPass->scopeidStack, pStmt->scopeid);
			Defer(pop(&pPass->scopeidStack));

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

			// Resolve body

			doResolvePass(pPass, pStmt->pBodyStmt);
		} break;

		case ASTK_BlockStmt:
		{
			// NOTE: For functions, block statements aren't responsible for pushing a new scope, since
			//	the block scope should be the same as the scope of the params. That's why we only
			//	push/pop if we are actually a different scope!

			auto * pStmt = Down(pNode, BlockStmt);

			bool shouldPushPop = true;
			{
				SCOPEID scopeidPrev;
				if (peek(pPass->scopeidStack, &scopeidPrev))
				{
					shouldPushPop = (scopeidPrev != pStmt->scopeid);
				}
			}

			if (shouldPushPop) push(&pPass->scopeidStack, pStmt->scopeid);
			Defer(if (shouldPushPop) pop(&pPass->scopeidStack));

			for (int i = 0; i < pStmt->apStmts.cItem; i++)
			{
				doResolvePass(pPass, pStmt->apStmts[i]);
			}
		} break;

		case ASTK_WhileStmt:
		{
			auto * pStmt = Down(pNode, WhileStmt);
			doResolvePass(pPass, pStmt->pCondExpr);
			doResolvePass(pPass, pStmt->pBodyStmt);
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
			doResolvePass(pPass, pStmt->pExpr);
		} break;

		case ASTK_BreakStmt:
		{
			// Nothing
		} break;

		case ASTK_ContinueStmt:
		{
			// Nothing
		} break;

	    default:
	    {
		    reportIceAndExit("Unknown astk in resolveStmt: %d", pNode->astk);
	    } break;
	}
}

void doResolvePass(ResolvePass * pPass, AstNode * pNode) // TODO: rename this param to pNode
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

        case ASTK_Program:
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
