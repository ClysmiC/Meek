#include "resolve_pass.h"

#include "ast.h"
#include "error.h"

void doResolvePass(ResolvePass * pPass, AstNode * pNodeSuper)
{
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
            // TODO: try to resolve in symbol table
        } break;

        case ASTK_ArrayAccessExpr:
        {
            // TODO: check that pArray is of type array.
            //  check that pSubscript evaluates to an int

            auto * pExpr = DownConst(pNodeSuper, ArrayAccessExpr);
            doResolvePass(pPass, pExpr->pArray);
            doResolvePass(pPass, pExpr->pSubscript);
        } break;

        case ASTK_FuncCallExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, FuncCallExpr);
            doResolvePass(pPass, pExpr->pFunc);

            // TODO: type check arguments vs params

            for (uint i = 0; i < pExpr->apArgs.cItem; i++)
            {
                doResolvePass(pPass, pExpr->apArgs[i]);
            }
        } break;

        case ASTK_FuncLiteralExpr:
        {
            auto * pExpr = DownConst(pNodeSuper, FuncLiteralExpr);
            
            // TODO
        } break;



        // STMT;

        case ASTK_ExprStmt:
        {
        } break;

        case ASTK_AssignStmt:
        {
        } break;

        case ASTK_VarDeclStmt:
        {
        } break;

        case ASTK_StructDefnStmt:
        {
        } break;

        case ASTK_FuncDefnStmt:
        {
        } break;

        case ASTK_BlockStmt:
        {
        } break;

        case ASTK_WhileStmt:
        {
        } break;

        case ASTK_IfStmt:
        {
        } break;

        case ASTK_ReturnStmt:
        {
        } break;

        case ASTK_BreakStmt:
        {
        } break;

        case ASTK_ContinueStmt:
        {
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
