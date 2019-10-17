#include "ast_print.h"

#include "ast.h"
#include "type.h"

#include <stdio.h>

#if DEBUG


void debugPrintAst(const AstNode & root)
{
    DynamicArray<bool> mpLevelSkip;
    init(&mpLevelSkip);
    Defer(dispose(&mpLevelSkip));

    debugPrintSubAst(root, 0, false, &mpLevelSkip);
    printf("\n");
}

void setSkip(DynamicArray<bool> * pMapLevelSkip, int level, bool skip) {
    Assert(level <= pMapLevelSkip->cItem);

    if (level == pMapLevelSkip->cItem)
    {
        append(pMapLevelSkip, skip);
    }
    else
    {
        (*pMapLevelSkip)[level] = skip;
    }
}

void printTabs(int level, bool printArrows, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip) {
    Assert(Implies(skipAfterArrow, printArrows));
    for (int i = 0; i < level; i++)
    {
        if ((*pMapLevelSkip)[i])
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
                    setSkip(pMapLevelSkip, level - 1, skipAfterArrow);
                }
            }
            else
            {
                printf("  ");
            }
        }
    }
};

void printChildren(const DynamicArray<AstNode *> & apChildren, int level, const char * label, bool setSkipOnLastChild, DynamicArray<bool> * pMapLevelSkip)
{
    for (int i = 0; i < apChildren.cItem; i++)
    {
        bool isLastChild = i == apChildren.cItem - 1;
        bool shouldSetSkip = setSkipOnLastChild && isLastChild;

        printf("\n");
        printTabs(level, false, false, pMapLevelSkip);

        printf("(%s %d):", label, i);
        debugPrintSubAst(
            *apChildren[i],
            level,
            shouldSetSkip,
            pMapLevelSkip
        );

        if (!isLastChild)
        {
            printf("\n");
            printTabs(level, false, false, pMapLevelSkip);
        }
    }
};

void printErrChildren(const AstErr & node, int level, DynamicArray<bool> * pMapLevelSkip)
{
    printChildren(node.apChildren, level, "child", true, pMapLevelSkip);
};

void debugPrintFuncHeader(const DynamicArray<AstNode *> & apParamVarDecls, const DynamicArray<AstNode *> & apReturnVarDecls, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
{
    if (apParamVarDecls.cItem == 0)
    {
        printf("\n");
        printTabs(level, true, false, pMapLevelSkip);
        printf("(no params)");
    }
    else
    {
        printChildren(apParamVarDecls, level, "param", false, pMapLevelSkip);
    }

    if (apReturnVarDecls.cItem == 0)
    {
        printf("\n");
        printTabs(level, true, skipAfterArrow, pMapLevelSkip);
        printf("(no return vals)");
    }
    else
    {
        printChildren(apReturnVarDecls, level, "return val", skipAfterArrow, pMapLevelSkip);
    }
};

void debugPrintType(const Type & type, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
{
    setSkip(pMapLevelSkip, level, false);

    int levelNext = level + 1;

    for (int i = 0; i < type.aTypemods.cItem; i++)
    {
        printf("\n");
        printTabs(level, false, false, pMapLevelSkip);

        TypeModifier ptm = type.aTypemods[i];

        if (ptm.typemodk == TYPEMODK_Array)
        {
            printf("(array of)");
            debugPrintSubAst(*ptm.pSubscriptExpr, levelNext, false, pMapLevelSkip);
        }
        else
        {
            Assert(ptm.typemodk == TYPEMODK_Pointer);
            printf("(pointer to)");
        }
    }

    if (type.isFuncType)
    {
        printf("\n");
        printTabs(level, false, false, pMapLevelSkip);
        printf("(func)");

        printf("\n");
        printTabs(level, false, false, pMapLevelSkip);

        printf("\n");
        printTabs(level, false, false, pMapLevelSkip);

        printf("(params)");
        printTabs(level, false, false, pMapLevelSkip);

        for (int i = 0; i < type.pFuncType->apParamType.cItem; i++)
        {
            bool isLastChild = i == type.pFuncType->apParamType.cItem - 1;

            printf("\n");
            printTabs(level, false, false, pMapLevelSkip);

            printf("(%s %d):", "param", i);

            debugPrintType(*type.pFuncType->apParamType[i], levelNext, false, pMapLevelSkip);

            if (!isLastChild)
            {
                printf("\n");
                printTabs(level, false, false, pMapLevelSkip);
            }
        }

        printf("\n");
        printTabs(level, false, false, pMapLevelSkip);

        printf("(return values)");
        printTabs(level, false, false, pMapLevelSkip);

        for (int i = 0; i < type.pFuncType->apReturnType.cItem; i++)
        {
            bool isLastChild = i == type.pFuncType->apReturnType.cItem - 1;
            bool shouldSetSkip = skipAfterArrow && isLastChild;

            printf("\n");
            printTabs(level, false, false, pMapLevelSkip);

            printf("(%s %d):", "return", i);

            debugPrintType(*type.pFuncType->apReturnType[i], levelNext, isLastChild, pMapLevelSkip);

            if (!isLastChild)
            {
                printf("\n");
                printTabs(level, false, false, pMapLevelSkip);
            }
        }
    }
    else
    {
        printf("\n");
        printTabs(level, true, skipAfterArrow, pMapLevelSkip);
        printf("%s", type.ident.pToken->lexeme);
    }
};

void debugPrintSubAst(const AstNode & node, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip)
{
    static const char * scanErrorString = "[scan error]:";
    static const char * parseErrorString = "[parse error]:";

    setSkip(pMapLevelSkip, level, false);
    printf("\n");
    printTabs(level, true, skipAfterArrow, pMapLevelSkip);

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
                    printTabs(level, true, skipAfterArrow, pMapLevelSkip);
                    printf("- %s", errMsgs[i].aBuffer);
                }
            }

            printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
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

                printTabs(levelNext, false, false, pMapLevelSkip);
                printErrChildren(*pErrCasted, levelNext, pMapLevelSkip);
            }
        } break;



        // EXPR

        case ASTK_BinopExpr:
        {
            auto * pExpr = DownConst(&node, BinopExpr);

            printf("%s ", pExpr->pOp->lexeme);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            debugPrintSubAst(*pExpr->pLhsExpr, levelNext, false, pMapLevelSkip);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            debugPrintSubAst(*pExpr->pRhsExpr, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_GroupExpr:
        {
            auto * pExpr = DownConst(&node, GroupExpr);

            printf("()");
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            debugPrintSubAst(*pExpr->pExpr, levelNext, true, pMapLevelSkip);
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

            printTabs(levelNext, false, false, pMapLevelSkip);
            debugPrintSubAst(*pExpr->pExpr, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_VarExpr:
        {
            auto * pExpr = DownConst(&node, VarExpr);

            if (pExpr->pOwner)
            {
                printf(". %s", pExpr->pTokenIdent->lexeme);
                printf("\n");

                printTabs(levelNext, false, false, pMapLevelSkip);
                printf("\n");

                printTabs(levelNext, false, false, pMapLevelSkip);
                printf("(owner):");
                debugPrintSubAst(*pExpr->pOwner, levelNext, true, pMapLevelSkip);
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

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(array):");
            debugPrintSubAst(*pExpr->pArray, levelNext, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(subscript):");
            debugPrintSubAst(*pExpr->pSubscript, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_FuncCallExpr:
        {
            auto * pExpr = DownConst(&node, FuncCallExpr);
            printf("(func call)");
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(func):");

            bool noArgs = pExpr->apArgs.cItem == 0;

            debugPrintSubAst(*pExpr->pFunc, levelNext, noArgs, pMapLevelSkip);

            printChildren(pExpr->apArgs, levelNext, "arg", true, pMapLevelSkip);
        } break;

        case ASTK_FuncLiteralExpr:
        {
            auto * pStmt = DownConst(&node, FuncLiteralExpr);

            printf("(func literal)");

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            debugPrintFuncHeader(pStmt->apParamVarDecls, pStmt->apReturnVarDecls, levelNext, false, pMapLevelSkip);

            debugPrintSubAst(*pStmt->pBodyStmt, levelNext, true, pMapLevelSkip);
        } break;



        // STMT;

        case ASTK_ExprStmt:
        {
            auto * pStmt = DownConst(&node, ExprStmt);

            printf("(expr stmt)");
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);

            debugPrintSubAst(*pStmt->pExpr, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_AssignStmt:
        {
            auto * pStmt = DownConst(&node, AssignStmt);

            printf("%s", pStmt->pAssignToken->lexeme);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(lhs):");

            debugPrintSubAst(*pStmt->pLhsExpr, levelNext, false, pMapLevelSkip);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(rhs):");
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            debugPrintSubAst(*pStmt->pRhsExpr, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_VarDeclStmt:
        {
            auto * pStmt = DownConst(&node, VarDeclStmt);

            printf("(var decl)");
            printf("\n");

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("\n");

            if (pStmt->ident.pToken)
            {
                printTabs(levelNext, false, false, pMapLevelSkip);
                printf("(ident):");
                printf("\n");

                printTabs(levelNext, true, false, pMapLevelSkip);
                printf("%s", pStmt->ident.pToken->lexeme);
                printf("\n");

                printTabs(levelNext, false, false, pMapLevelSkip);
                printf("\n");
            }

            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(type):");

            bool hasInitExpr = (pStmt->pInitExpr != nullptr);

            debugPrintType(*pStmt->pType, levelNext, !hasInitExpr, pMapLevelSkip);
            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);


            if (hasInitExpr)
            {
                printf("\n");
                printTabs(levelNext, false, false, pMapLevelSkip);
                printf("(init):");
                debugPrintSubAst(*pStmt->pInitExpr, levelNext, true, pMapLevelSkip);
            }
        } break;

        case ASTK_StructDefnStmt:
        {
            auto * pStmt = DownConst(&node, StructDefnStmt);

            printf("(struct defn)");

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(name):");

            printf("\n");
            printTabs(levelNext, true, false, pMapLevelSkip);
            printf("%s", pStmt->ident.pToken->lexeme);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            printChildren(pStmt->apVarDeclStmt, levelNext, "vardecl", true, pMapLevelSkip);
        } break;

        case ASTK_FuncDefnStmt:
        {
            auto * pStmt = DownConst(&node, FuncDefnStmt);

            printf("(func defn)");

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(name):");

            printf("\n");
            printTabs(levelNext, true, false, pMapLevelSkip);
            printf("%s", pStmt->ident.pToken->lexeme);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            debugPrintFuncHeader(pStmt->apParamVarDecls, pStmt->apReturnVarDecls, levelNext, false, pMapLevelSkip);

            debugPrintSubAst(*pStmt->pBodyStmt, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_BlockStmt:
        {
            auto * pStmt = DownConst(&node, BlockStmt);

            printf("(block)");

            if (pStmt->apStmts.cItem > 0)
            {
                printf("\n");
                printTabs(levelNext, false, false, pMapLevelSkip);
            }

            printChildren(pStmt->apStmts, levelNext, "stmt", true, pMapLevelSkip);
        } break;

        case ASTK_WhileStmt:
        {
            auto * pStmt = DownConst(&node, WhileStmt);

            printf("(while)");

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(cond):");

            debugPrintSubAst(*pStmt->pCondExpr, levelNext, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            debugPrintSubAst(*pStmt->pBodyStmt, levelNext, true, pMapLevelSkip);
        } break;

        case ASTK_IfStmt:
        {
            auto * pStmt = DownConst(&node, IfStmt);

            printf("(if)");

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);
            printf("(cond):");

            debugPrintSubAst(*pStmt->pCondExpr, levelNext, false, pMapLevelSkip);

            printf("\n");
            printTabs(levelNext, false, false, pMapLevelSkip);

            debugPrintSubAst(*pStmt->pThenStmt, levelNext, !pStmt->pElseStmt, pMapLevelSkip);

            if (pStmt->pElseStmt)
            {
                printf("\n");
                printTabs(levelNext, false, false, pMapLevelSkip);

                printf("\n");
                printTabs(levelNext, false, false, pMapLevelSkip);
                printf("(else):");

                debugPrintSubAst(*pStmt->pElseStmt, levelNext, true, pMapLevelSkip);
            }
        } break;

        case ASTK_ReturnStmt:
        {
            auto * pStmt = DownConst(&node, ReturnStmt);
            printf("(return)");

            if (pStmt->pExpr)
            {
                printf("\n");
                printTabs(levelNext, false, false, pMapLevelSkip);

                debugPrintSubAst(*pStmt->pExpr, levelNext, true, pMapLevelSkip);
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

            printTabs(levelNext, false, false, pMapLevelSkip);

            printChildren(pNode->apNodes, levelNext, "stmt", true, pMapLevelSkip);
        } break;

        default:
        {
            // Not implemented!

            Assert(false);
        }
    }
}

#endif