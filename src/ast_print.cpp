#include "ast_print.h"

#include "ast.h"
#include "type.h"

#include <stdio.h>

#if DEBUG


void debugPrintAst(DebugPrintCtx * pCtx, const AstNode & root)
{
    DynamicArray<bool> mpLevelSkip;
    init(&mpLevelSkip);
    Defer(dispose(&mpLevelSkip));

    debugPrintSubAst(pCtx, root, 0, false);
    printf("\n");
}

void setSkip(DebugPrintCtx * pCtx, int level, bool skip) {
    Assert(level <= pCtx->mpLevelSkip.cItem);

    if (level == pCtx->mpLevelSkip.cItem)
    {
        append(&pCtx->mpLevelSkip, skip);
    }
    else
    {
        pCtx->mpLevelSkip[level] = skip;
    }
}

void printTabs(DebugPrintCtx * pCtx, int level, bool printArrows, bool skipAfterArrow) {
    Assert(Implies(skipAfterArrow, printArrows));
    for (int i = 0; i < level; i++)
    {
        if (pCtx->mpLevelSkip[i])
        {
            printf("   ");
        }
        else
        {
            printf("|");

            if (printArrows && i == level - 1)
            {
                printf("- ");

                if (skipAfterArrow)
                {
                    setSkip(pCtx, level - 1, skipAfterArrow);
                }
            }
            else
            {
                printf("  ");
            }
        }
    }
};

void printChildren(DebugPrintCtx * pCtx, const DynamicArray<AstNode *> & apChildren, int level, const char * label, bool setSkipOnLastChild)
{
    for (int i = 0; i < apChildren.cItem; i++)
    {
        bool isLastChild = i == apChildren.cItem - 1;
        bool shouldSetSkip = setSkipOnLastChild && isLastChild;

        printf("\n");
        printTabs(pCtx, level, false, false);

        printf("(%s %d):", label, i);
        debugPrintSubAst(
            pCtx,
            *apChildren[i],
            level,
            shouldSetSkip
        );

        if (!isLastChild)
        {
            printf("\n");
            printTabs(pCtx, level, false, false);
        }
    }
};

void printErrChildren(DebugPrintCtx * pCtx, const AstErr & node, int level)
{
    printChildren(pCtx, node.apChildren, level, "child", true);
};

void debugPrintFuncHeader(DebugPrintCtx * pCtx, const DynamicArray<AstNode *> & apParamVarDecls, const DynamicArray<AstNode *> & apReturnVarDecls, int level, bool skipAfterArrow)
{
    if (apParamVarDecls.cItem == 0)
    {
        printf("\n");
        printTabs(pCtx, level, true, false);
        printf("(no params)");
    }
    else
    {
        printChildren(pCtx, apParamVarDecls, level, "param", false);
    }

    if (apReturnVarDecls.cItem == 0)
    {
        printf("\n");
        printTabs(pCtx, level, true, skipAfterArrow);
        printf("(no return vals)");
    }
    else
    {
        printChildren(pCtx, apReturnVarDecls, level, "return val", skipAfterArrow);
    }
};

void debugPrintType(DebugPrintCtx * pCtx, typid typid, int level, bool skipAfterArrow)
{
    setSkip(pCtx, level, false);
    int levelNext = level + 1;

    if (!isTypeResolved(typid))
    {
        printf("\n");
        printTabs(pCtx, level, false, false);

        printf("!! unresolved type !!\n");

        return;
    }

    const Type * pType = lookupType(pCtx->pTypeTable, typid);
    Assert(pType);

    for (int i = 0; i < pType->aTypemods.cItem; i++)
    {
        printf("\n");
        printTabs(pCtx, level, false, false);

        TypeModifier tmod = pType->aTypemods[i];

        if (tmod.typemodk == TYPEMODK_Array)
        {
            printf("(array of)");
            debugPrintSubAst(pCtx, *tmod.pSubscriptExpr, levelNext, false);
        }
        else
        {
            Assert(tmod.typemodk == TYPEMODK_Pointer);
            printf("(pointer to)");
        }
    }

    if (pType->isFuncType)
    {
        printf("\n");
        printTabs(pCtx, level, false, false);
        printf("(func)");

        printf("\n");
        printTabs(pCtx, level, false, false);

        printf("\n");
        printTabs(pCtx, level, false, false);

        printf("(params)");
        printTabs(pCtx, level, false, false);

        for (int i = 0; i < pType->funcType.paramTypids.cItem; i++)
        {
            bool isLastChild = i == pType->funcType.paramTypids.cItem - 1;

            printf("\n");
            printTabs(pCtx, level, false, false);

            printf("(%s %d):", "param", i);

            debugPrintType(pCtx, pType->funcType.paramTypids[i], levelNext, false);

            if (!isLastChild)
            {
                printf("\n");
                printTabs(pCtx, level, false, false);
            }
        }

        printf("\n");
        printTabs(pCtx, level, false, false);

        printf("(return values)");
        printTabs(pCtx, level, false, false);

        for (int i = 0; i < pType->funcType.returnTypids.cItem; i++)
        {
            bool isLastChild = i == pType->funcType.returnTypids.cItem - 1;
            bool shouldSetSkip = skipAfterArrow && isLastChild;

            printf("\n");
            printTabs(pCtx, level, false, false);

            printf("(%s %d):", "return", i);

            debugPrintType(pCtx, pType->funcType.returnTypids[i], levelNext, isLastChild);

            if (!isLastChild)
            {
                printf("\n");
                printTabs(pCtx, level, false, false);
            }
        }
    }
    else
    {
        printf("\n");
        printTabs(pCtx, level, true, skipAfterArrow);
        printf("%s", pType->ident.pToken->lexeme);
    }
};

void debugPrintSubAst(DebugPrintCtx * pCtx, const AstNode & node, int level, bool skipAfterArrow)
{
    static const char * scanErrorString = "[scan error]:";
    static const char * parseErrorString = "[parse error]:";

    setSkip(pCtx, level, false);
    printf("\n");
    printTabs(pCtx, level, true, skipAfterArrow);

    int levelNext = level + 1;

    switch (node.astk)
    {
        // ERR

        // TODO: Factor these error strings out into a function that can exist outside of DEBUG and have bells and whistles
        //	like reporting error line #! Maybe even print the surrounding context... might be kinda hard since I don't store it
        //	anywhere, but I could store an index into the text buffer and it could walk forward and backward until it hits
        //	new lines or some length limit and then BOOM I've got context!

        case ASTK_ScanErr:
        {
            auto * pErr = DownConst(&node, ScanErr);
            auto * pErrCasted = UpErrConst(pErr);

            DynamicArray<StringBox<256>> errMsgs;
            init(&errMsgs);
            Defer(dispose(&errMsgs));

            errMessagesFromGrferrtok(pErr->pErrToken->grferrtok, &errMsgs);
            Assert(errMsgs.cItem > 0);

            if (errMsgs.cItem == 1)
            {
                printf("%s %s", scanErrorString, errMsgs[0].aBuffer);
            }
            else
            {
                printf("%s", scanErrorString);

                for (int i = 0; i < errMsgs.cItem; i++)
                {
                    printf("\n");
                    printTabs(pCtx, level, true, skipAfterArrow);
                    printf("- %s", errMsgs[i].aBuffer);
                }
            }

            printErrChildren(pCtx, *pErrCasted, levelNext);
        } break;

        case ASTK_BubbleErr:
        {
            auto * pErr = DownConst(&node, BubbleErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s (bubble)", parseErrorString);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_UnexpectedTokenkErr:
        {
            auto * pErr = DownConst(&node, UnexpectedTokenkErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s unexpected %s", parseErrorString, g_mpTokenkDisplay[pErr->pErrToken->tokenk]);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_ExpectedTokenkErr:
        {
            auto * pErr = DownConst(&node, ExpectedTokenkErr);
            auto * pErrCasted = UpErrConst(pErr);

            if (pErr->aTokenkValid.cItem == 1)
            {
                printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pErr->aTokenkValid[0]]);
            }
            else if (pErr->aTokenkValid.cItem == 2)
            {
                printf("%s expected %s or %s", parseErrorString, g_mpTokenkDisplay[pErr->aTokenkValid[0]], g_mpTokenkDisplay[pErr->aTokenkValid[1]]);
            }
            else
            {
                Assert(pErr->aTokenkValid.cItem > 2);

                printf("%s expected %s", parseErrorString, g_mpTokenkDisplay[pErr->aTokenkValid[0]]);

                for (uint i = 1; i < pErr->aTokenkValid.cItem; i++)
                {
                    bool isLast = (i == pErr->aTokenkValid.cItem - 1);

                    printf(", ");
                    if (isLast) printf("or ");
                    printf("%s", g_mpTokenkDisplay[pErr->aTokenkValid[i]]);
                }
            }

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_InitUnnamedVarErr:
        {
            auto * pErr = DownConst(&node, InitUnnamedVarErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s attempt to assign initial value to unnamed variable", parseErrorString);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_IllegalInitErr:
        {
            auto * pErr = DownConst(&node, IllegalInitErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s variable '%s' is not allowed to be assigned an initial value", parseErrorString, pErr->pVarIdent->lexeme);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_ChainedAssignErr:
        {
            auto * pErr = DownConst(&node, ChainedAssignErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s chaining assignments is not permitted", parseErrorString);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_IllegalDoStmtErr:
        {
            auto * pErr = DownConst(&node, IllegalDoStmtErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s %s is not permitted following 'do'", parseErrorString, displayString(pErr->astkStmt));

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;

        case ASTK_InvokeFuncLiteralErr:
        {
            auto * pErr = DownConst(&node, InvokeFuncLiteralErr);
            auto * pErrCasted = UpErrConst(pErr);

            printf("%s function literals can not be directly invoked", parseErrorString);

            if (pErrCasted->apChildren.cItem > 0)
            {
                // Sloppy... printChildren should probably handle the new line spacing so that if you pass
                //	it an empty array of children it will still just work.

                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printErrChildren(pCtx, *pErrCasted, levelNext);
            }
        } break;



        // EXPR

        case ASTK_BinopExpr:
        {
            auto * pExpr = DownConst(&node, BinopExpr);

            printf("%s ", pExpr->pOp->lexeme);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            debugPrintSubAst(pCtx, *pExpr->pLhsExpr, levelNext, false);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            debugPrintSubAst(pCtx, *pExpr->pRhsExpr, levelNext, true);
        } break;

        case ASTK_GroupExpr:
        {
            auto * pExpr = DownConst(&node, GroupExpr);

            printf("()");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            debugPrintSubAst(pCtx, *pExpr->pExpr, levelNext, true);
        } break;

        case ASTK_LiteralExpr:
        {
            auto * pExpr = DownConst(&node, LiteralExpr);

            printf("%s", pExpr->pToken->lexeme);
        } break;

        case ASTK_UnopExpr:
        {
            auto * pExpr = DownConst(&node, UnopExpr);

            printf("%s ", pExpr->pOp->lexeme);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            debugPrintSubAst(pCtx, *pExpr->pExpr, levelNext, true);
        } break;

        case ASTK_VarExpr:
        {
            auto * pExpr = DownConst(&node, VarExpr);

            if (pExpr->pOwner)
            {
                printf(". %s", pExpr->pTokenIdent->lexeme);
                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printf("(owner):");
                debugPrintSubAst(pCtx, *pExpr->pOwner, levelNext, true);
            }
            else
            {
                printf("%s", pExpr->pTokenIdent->lexeme);
            }
        } break;

        case ASTK_ArrayAccessExpr:
        {
            auto * pExpr = DownConst(&node, ArrayAccessExpr);
            printf("[]");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("(array):");
            debugPrintSubAst(pCtx, *pExpr->pArray, levelNext, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);
            printf("(subscript):");
            debugPrintSubAst(pCtx, *pExpr->pSubscript, levelNext, true);
        } break;

        case ASTK_FuncCallExpr:
        {
            auto * pExpr = DownConst(&node, FuncCallExpr);
            printf("(func call)");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("(func):");

            bool noArgs = pExpr->apArgs.cItem == 0;

            debugPrintSubAst(pCtx, *pExpr->pFunc, levelNext, noArgs);

            printChildren(pCtx, pExpr->apArgs, levelNext, "arg", true);
        } break;

        case ASTK_FuncLiteralExpr:
        {
            auto * pStmt = DownConst(&node, FuncLiteralExpr);

            printf("(func literal)");

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            debugPrintFuncHeader(pCtx, pStmt->apParamVarDecls, pStmt->apReturnVarDecls, levelNext, false);

            debugPrintSubAst(pCtx, *pStmt->pBodyStmt, levelNext, true);
        } break;



        // STMT;

        case ASTK_ExprStmt:
        {
            auto * pStmt = DownConst(&node, ExprStmt);

            printf("(expr stmt)");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);

            debugPrintSubAst(pCtx, *pStmt->pExpr, levelNext, true);
        } break;

        case ASTK_AssignStmt:
        {
            auto * pStmt = DownConst(&node, AssignStmt);

            printf("%s", pStmt->pAssignToken->lexeme);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("(lhs):");

            debugPrintSubAst(pCtx, *pStmt->pLhsExpr, levelNext, false);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("(rhs):");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            debugPrintSubAst(pCtx, *pStmt->pRhsExpr, levelNext, true);
        } break;

        case ASTK_VarDeclStmt:
        {
            auto * pStmt = DownConst(&node, VarDeclStmt);

            printf("(var decl)");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);
            printf("\n");

            if (pStmt->ident.pToken)
            {
                printTabs(pCtx, levelNext, false, false);
                printf("(ident):");
                printf("\n");

                printTabs(pCtx, levelNext, true, false);
                printf("%s", pStmt->ident.pToken->lexeme);
                printf("\n");

                printTabs(pCtx, levelNext, false, false);
                printf("\n");
            }

            printTabs(pCtx, levelNext, false, false);
            printf("(type):");

            bool hasInitExpr = (pStmt->pInitExpr != nullptr);

            debugPrintType(pCtx, pStmt->typid, levelNext, !hasInitExpr);
            printf("\n");
            printTabs(pCtx, levelNext, false, false);


            if (hasInitExpr)
            {
                printf("\n");
                printTabs(pCtx, levelNext, false, false);
                printf("(init):");
                debugPrintSubAst(pCtx, *pStmt->pInitExpr, levelNext, true);
            }
        } break;

        case ASTK_StructDefnStmt:
        {
            auto * pStmt = DownConst(&node, StructDefnStmt);

            printf("(struct defn)");

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);
            printf("(name):");

            printf("\n");
            printTabs(pCtx, levelNext, true, false);
            printf("%s", pStmt->ident.pToken->lexeme);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            printChildren(pCtx, pStmt->apVarDeclStmt, levelNext, "vardecl", true);
        } break;

        case ASTK_FuncDefnStmt:
        {
            auto * pStmt = DownConst(&node, FuncDefnStmt);

            printf("(func defn)");

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);
            printf("(name):");

            printf("\n");
            printTabs(pCtx, levelNext, true, false);
            printf("%s", pStmt->ident.pToken->lexeme);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            debugPrintFuncHeader(pCtx, pStmt->apParamVarDecls, pStmt->apReturnVarDecls, levelNext, false);

            debugPrintSubAst(pCtx, *pStmt->pBodyStmt, levelNext, true);
        } break;

        case ASTK_BlockStmt:
        {
            auto * pStmt = DownConst(&node, BlockStmt);

            printf("(block)");

            if (pStmt->apStmts.cItem > 0)
            {
                printf("\n");
                printTabs(pCtx, levelNext, false, false);
            }

            printChildren(pCtx, pStmt->apStmts, levelNext, "stmt", true);
        } break;

        case ASTK_WhileStmt:
        {
            auto * pStmt = DownConst(&node, WhileStmt);

            printf("(while)");

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);
            printf("(cond):");

            debugPrintSubAst(pCtx, *pStmt->pCondExpr, levelNext, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            debugPrintSubAst(pCtx, *pStmt->pBodyStmt, levelNext, true);
        } break;

        case ASTK_IfStmt:
        {
            auto * pStmt = DownConst(&node, IfStmt);

            printf("(if)");

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);
            printf("(cond):");

            debugPrintSubAst(pCtx, *pStmt->pCondExpr, levelNext, false);

            printf("\n");
            printTabs(pCtx, levelNext, false, false);

            debugPrintSubAst(pCtx, *pStmt->pThenStmt, levelNext, !pStmt->pElseStmt);

            if (pStmt->pElseStmt)
            {
                printf("\n");
                printTabs(pCtx, levelNext, false, false);

                printf("\n");
                printTabs(pCtx, levelNext, false, false);
                printf("(else):");

                debugPrintSubAst(pCtx, *pStmt->pElseStmt, levelNext, true);
            }
        } break;

        case ASTK_ReturnStmt:
        {
            auto * pStmt = DownConst(&node, ReturnStmt);
            printf("(return)");

            if (pStmt->pExpr)
            {
                printf("\n");
                printTabs(pCtx, levelNext, false, false);

                debugPrintSubAst(pCtx, *pStmt->pExpr, levelNext, true);
            }
        } break;

        case ASTK_BreakStmt:
        {
            auto * pStmt = DownConst(&node, BreakStmt);
            printf("(break)");
        } break;

        case ASTK_ContinueStmt:
        {
            auto * pStmt = DownConst(&node, ContinueStmt);
            printf("(continue)");
        } break;



        // PROGRAM

        case ASTK_Program:
        {
            auto * pNode = DownConst(&node, Program);

            printf("(program)");
            printf("\n");

            printTabs(pCtx, levelNext, false, false);

            printChildren(pCtx, pNode->apNodes, levelNext, "stmt", true);
        } break;

        default:
        {
            // Not implemented!

            Assert(false);
        }
    }
}

#endif
