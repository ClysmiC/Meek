#include "resolve_pass.h"

#include "ast.h"
#include "error.h"
#include "symbol.h"
#include "type.h"

scopeid resolveVarExpr(ResolvePass * pPass, AstNode * pVarExpr)
{
	Assert(pVarExpr);

	if (pVarExpr->astk != ASTK_VarExpr)
	{
		return gc_unresolvedScopeid;
	}
    
    // StaticAssert(false); // START HERE TOMORROW!
	auto * pExpr = Down(pVarExpr, VarExpr);

	if (pExpr->pOwner)
	{
		scopeid ownerScopeid = resolveVarExpr(pPass, pExpr->pOwner);

		AssertInfo(!pExpr->pResolvedDecl, "This shouldn't be resolved yet because this is the code that should resolve it!");

		ScopedIdentifier candidate;
        setIdent(&candidate, pExpr->pTokenIdent, ownerScopeid);
		SymbolTableEntry * pEntry = lookupVar(pPass->pSymbTable, candidate);

		if (pEntry)
		{
			// Resolve!

            pExpr->pResolvedDecl = pEntry->symbInfo.varDecl;

			// TODO: Look up type of the variable and return the type declaration's corresponding scopeid
		}
		else
		{
			// Unresolved
		}
	}
	else
	{
	}

	// do lookup + resolve and then return scopeid?

    AssertInfo(false, "TODO: finish writing this function");
    return gc_unresolvedScopeid;
}

void doResolvePass(ResolvePass * pPass, AstNode * pNodeSuper)
{
	AssertInfo(
		pNodeSuper,
		"For optional node children, it is the caller's responsibility to perform null check, because I want to catch "
		"errors if we ever accidentally have null children in spots where they shouldn't be null"
	);

    switch (pNodeSuper->astk)
    {
        // EXPR

        case ASTK_BinopExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, BinopExpr);
            doResolvePass(pPass, pExpr->pLhsExpr);
            doResolvePass(pPass, pExpr->pRhsExpr);
        } break;

        case ASTK_GroupExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, GroupExpr);
            doResolvePass(pPass, pExpr->pExpr);
        } break;

        case ASTK_LiteralExpr:
        {
        } break;

        case ASTK_UnopExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, UnopExpr);
            doResolvePass(pPass, pExpr->pExpr);
        } break;

        case ASTK_VarExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, VarExpr);

			// TODO: Need to look up variables in the owner struct's in the structs namespace....
			//	we should probably recurse into the owner first, and maybe it will return the symbolid of the
			//	scope that the owner's shit lives in?

        } break;

        case ASTK_ArrayAccessExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, ArrayAccessExpr);
            doResolvePass(pPass, pExpr->pArray);
            doResolvePass(pPass, pExpr->pSubscript);
        } break;

        case ASTK_FuncCallExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, FuncCallExpr);
            doResolvePass(pPass, pExpr->pFunc);

            for (uint i = 0; i < pExpr->apArgs.cItem; i++)
            {
                doResolvePass(pPass, pExpr->apArgs[i]);
            }
        } break;

        case ASTK_FuncLiteralExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, FuncLiteralExpr);

			push(&pPass->scopeidStack, pExpr->scopeid);
			Defer(pop(&pPass->scopeidStack));

			for (uint i = 0; i < pExpr->pFuncType->apParamVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pExpr->pFuncType->apParamVarDecls[i]);
			}

			for (uint i = 0; i < pExpr->pFuncType->apReturnVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pExpr->pFuncType->apReturnVarDecls[i]);
			}
        } break;



        // STMT

        case ASTK_ExprStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, ExprStmt);
			doResolvePass(pPass, pStmt->pExpr);
        } break;

        case ASTK_AssignStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, AssignStmt);
			doResolvePass(pPass, pStmt->pLhsExpr);
			doResolvePass(pPass, pStmt->pRhsExpr);
        } break;

        case ASTK_VarDeclStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, VarDeclStmt);
			AssertInfo(isScopeSet(pStmt->ident), "The name of the thing you are declaring should be resolved to itself...");

			AssertInfo(pStmt->pType, "Inferred types are TODO");

			if (pStmt->pType->isFuncType)
			{
				// Resolve types of params

				for (uint i = 0; i < pStmt->pType->pFuncType->apParamVarDecls.cItem; i++)
				{
					doResolvePass(pPass, pStmt->pType->pFuncType->apParamVarDecls[i]);
				}

				for (uint i = 0; i < pStmt->pType->pFuncType->apReturnVarDecls.cItem; i++)
				{
					doResolvePass(pPass, pStmt->pType->pFuncType->apReturnVarDecls[i]);
				}
			}
			else
			{
				// Resolve type

				AssertInfo(!isScopeSet(pStmt->pType->ident), "This shouldn't be resolved yet because this is the code that should resolve it!");

				ScopedIdentifier candidate = pStmt->pType->ident;

				for (uint i = 0; i < count(pPass->scopeidStack); i++)
				{
					scopeid scopeid;
					Verify(peekFar(pPass->scopeidStack, i, &scopeid));
					resolveIdentScope(&candidate, scopeid);

                    SymbolTableEntry * pSymbEntry = lookupStruct(pPass->pSymbTable, candidate);

					if(pSymbEntry)
					{
						// Resolve it!

                        resolveIdentScope(&pStmt->pType->ident, scopeid);
						break;
					}
				}

				if (!isScopeSet(pStmt->pType->ident))
				{
					reportUnresolvedIdentError(pPass, pStmt->pType->ident);
				}
			}

			// Record symbolid for variable

			if (pStmt->ident.pToken)
			{
                SymbolTableEntry * pSymbEntry = lookupVar(pPass->pSymbTable, pStmt->ident);
                AssertInfo(pSymbEntry, "We should have put this var decl in the symbol table when we parsed it...");
				pPass->lastSequenceId = pSymbEntry->sequenceId;
			}

			if (pStmt->pInitExpr) doResolvePass(pPass, pStmt->pInitExpr);
        } break;

        case ASTK_StructDefnStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, StructDefnStmt);

			SymbolTableEntry * pSymbEntry = lookupStruct(pPass->pSymbTable, pStmt->ident);
            AssertInfo(pSymbEntry, "We should have put this struct decl in the symbol table when we parsed it...");
			pPass->lastSequenceId = pSymbEntry->sequenceId;

			push(&pPass->scopeidStack, pStmt->scopeid);
			Defer(pop(&pPass->scopeidStack));

			for (uint i = 0; i < pStmt->apVarDeclStmt.cItem; i++)
			{
				doResolvePass(pPass, pStmt->apVarDeclStmt[i]);
			}
        } break;

        case ASTK_FuncDefnStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, FuncDefnStmt);

            // TODO: Should be similar to StructDefnStatement above, but need to find the our function among overloads...

			SymbolTableEntry * pSymbEntry;
			Verify(lookup(&pPass->pSymbTable, pStmt->resolvedIdent, pSymbEntry));
			pPass->lastSequenceId = pSymbEntry->symbolId;

			push(&pPass->scopeidStack, pStmt->scopeid);
			Defer(pop(&pPass->scopeidStack));

			for (uint i = 0; i < pStmt->pFuncType->apParamVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pStmt->pFuncType->apParamVarDecls[i]);
			}

			for (uint i = 0; i < pStmt->pFuncType->apReturnVarDecls.cItem; i++)
			{
				doResolvePass(pPass, pStmt->pFuncType->apReturnVarDecls[i]);
			}

			doResolvePass(pPass, pStmt->pBodyStmt);
        } break;

        case ASTK_BlockStmt:
        {
			// NOTE: For functions, block statements aren't responsible for pushing a new scope, since
			//	the block scope should be the same as the scope of the params. That's why we only
			//	push/pop if we are actually a different scope!

			auto * pStmt = DownConst(pNodeSuper, BlockStmt);

			bool shouldPushPop = true;
			{
				scopeid scopeidPrev;
				if (peek(&pPass->scopeidStack, &scopeidPrev))
				{
					shouldPushPop = (scopeidPrev != pStmt->scopeid);
				}
			}

			if (shouldPushPop) push(&pPass->scopeidStack, pStmt->scopeid);
			Defer(if (shouldPushPop) pop(&pPass->scopeidStack));

			doResolvePass(pPass, pStmt);
        } break;

        case ASTK_WhileStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, WhileStmt);
			doResolvePass(pPass, pStmt->pCondExpr);
			doResolvePass(pPass, pStmt->pBodyStmt);
        } break;

        case ASTK_IfStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, IfStmt);
			doResolvePass(pPass, pStmt->pCondExpr);
			doResolvePass(pPass, pStmt->pThenStmt);
			if (pStmt->pElseStmt) doResolvePass(pPass, pStmt->pElseStmt);
        } break;

        case ASTK_ReturnStmt:
        {
			auto * pStmt = DownConst(pNodeSuper, ReturnStmt);
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



        // PROGRAM

        case ASTK_Program:
        {
            auto * pNode = DownConst(pNodeSuper, Program);
            for (uint i = 0; i < pNode->apNodes.cItem; i++)
            {
                doResolvePass(pPass, pNode->apNodes[i]);
            }
        } break;

        default:
        {
            reportIceAndExit("Unknown astk in doResolvePass: %d", pNodeSuper->astk);
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
