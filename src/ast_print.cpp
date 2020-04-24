#include "ast_print.h"

#include "ast.h"
#include "print.h"
#include "type.h"

#if DEBUG


void debugPrintAst(DebugPrintCtx * pCtx, const AstNode & root)
{
	DynamicArray<bool> mpLevelSkip;
	init(&mpLevelSkip);
	Defer(dispose(&mpLevelSkip));

	debugPrintSubAst(pCtx, root, 0, false);
	println();
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
			print("   ");
		}
		else
		{
			print("|");

			if (printArrows && i == level - 1)
			{
				print("- ");

				if (skipAfterArrow)
				{
					setSkip(pCtx, level - 1, skipAfterArrow);
				}
			}
			else
			{
				print("  ");
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

		println();
		printTabs(pCtx, level, false, false);

		printfmt("(%s %d):", label, i);
		debugPrintSubAst(
			pCtx,
			*apChildren[i],
			level,
			shouldSetSkip
		);

		if (!isLastChild)
		{
			println();
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
		println();
		printTabs(pCtx, level, true, false);
		print("(no params)");
	}
	else
	{
		printChildren(pCtx, apParamVarDecls, level, "param", false);
	}

	if (apReturnVarDecls.cItem == 0)
	{
		println();
		printTabs(pCtx, level, true, skipAfterArrow);
		print("(no return vals)");
	}
	else
	{
		printChildren(pCtx, apReturnVarDecls, level, "return val", skipAfterArrow);
	}
};

void debugPrintType(DebugPrintCtx * pCtx, TYPID typid, int level, bool skipAfterArrow)
{
	setSkip(pCtx, level, false);
	int levelNext = level + 1;

	if (!isTypeResolved(typid))
	{
		println();
		printTabs(pCtx, level, false, false);

		print("!! unresolved type !!\n");

		return;
	}

	const Type * pType = lookupType(*pCtx->pTypeTable, typid);
	Assert(pType);

	while (pType->typek != TYPEK_Mod)
	{
		println();
		printTabs(pCtx, level, false, false);

		TypeModifier tmod = pType->modTypeData.typemod;

		if (tmod.typemodk == TYPEMODK_Array)
		{
			print("(array of)");
			debugPrintSubAst(pCtx, *tmod.pSubscriptExpr, levelNext, false);
		}
		else
		{
			Assert(tmod.typemodk == TYPEMODK_Pointer);
			print("(pointer to)");
		}

		pType = lookupType(*pCtx->pTypeTable, pType->modTypeData.typidModified);
		Assert(pType);
	}

	if (pType->typek == TYPEK_Func)
	{
		println();
		printTabs(pCtx, level, false, false);
		print("(func)");

		println();
		printTabs(pCtx, level, false, false);

		println();
		printTabs(pCtx, level, false, false);

		print("(params)");
		printTabs(pCtx, level, false, false);

		for (int i = 0; i < pType->funcTypeData.funcType.paramTypids.cItem; i++)
		{
			bool isLastChild = i == pType->funcTypeData.funcType.paramTypids.cItem - 1;

			println();
			printTabs(pCtx, level, false, false);

			printfmt("(%s %d):", "param", i);

			debugPrintType(pCtx, pType->funcTypeData.funcType.paramTypids[i], levelNext, false);

			if (!isLastChild)
			{
				println();
				printTabs(pCtx, level, false, false);
			}
		}

		println();
		printTabs(pCtx, level, false, false);

		print("(return values)");
		printTabs(pCtx, level, false, false);

		for (int i = 0; i < pType->funcTypeData.funcType.returnTypids.cItem; i++)
		{
			bool isLastChild = i == pType->funcTypeData.funcType.returnTypids.cItem - 1;
			bool shouldSetSkip = skipAfterArrow && isLastChild;

			println();
			printTabs(pCtx, level, false, false);

			printfmt("(%s %d):", "return", i);

			debugPrintType(pCtx, pType->funcTypeData.funcType.returnTypids[i], levelNext, isLastChild);

			if (!isLastChild)
			{
				println();
				printTabs(pCtx, level, false, false);
			}
		}
	}
	else
	{
		Assert(pType->typek == TYPEK_Named);

		println();
		printTabs(pCtx, level, true, skipAfterArrow);
		print(pType->namedTypeData.ident.lexeme.strv);
	}
};

void debugPrintSubAst(DebugPrintCtx * pCtx, const AstNode & node, int level, bool skipAfterArrow)
{
	static const char * scanErrorString = "[scan error]:";
	static const char * parseErrorString = "[parse error]:";

	setSkip(pCtx, level, false);
	println();
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

			ForFlag(FERRTOK, ferrtok)
			{
				if (pErr->pErrToken->grferrtok & ferrtok)
				{
					printfmt("%s %s", scanErrorString, errMessageFromFerrtok(ferrtok));
				}
			}

			printErrChildren(pCtx, *pErrCasted, levelNext);
		} break;

		case ASTK_BubbleErr:
		{
			auto * pErr = DownConst(&node, BubbleErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s (bubble)", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_UnexpectedTokenkErr:
		{
			auto * pErr = DownConst(&node, UnexpectedTokenkErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s unexpected %s", parseErrorString, g_mpTokenkStrDisplay[pErr->pErrToken->tokenk]);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

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
				printfmt("%s expected %s", parseErrorString, g_mpTokenkStrDisplay[pErr->aTokenkValid[0]]);
			}
			else if (pErr->aTokenkValid.cItem == 2)
			{
				printfmt("%s expected %s or %s", parseErrorString, g_mpTokenkStrDisplay[pErr->aTokenkValid[0]], g_mpTokenkStrDisplay[pErr->aTokenkValid[1]]);
			}
			else
			{
				Assert(pErr->aTokenkValid.cItem > 2);

				printfmt("%s expected %s", parseErrorString, g_mpTokenkStrDisplay[pErr->aTokenkValid[0]]);

				for (uint i = 1; i < pErr->aTokenkValid.cItem; i++)
				{
					bool isLast = (i == pErr->aTokenkValid.cItem - 1);

					print(", ");
					if (isLast) print("or ");
					printfmt("%s", g_mpTokenkStrDisplay[pErr->aTokenkValid[i]]);
				}
			}

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_InitUnnamedVarErr:
		{
			auto * pErr = DownConst(&node, InitUnnamedVarErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s attempt to assign initial value to unnamed variable", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_ChainedAssignErr:
		{
			auto * pErr = DownConst(&node, ChainedAssignErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s chaining assignments is not permitted", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_IllegalDoPseudoStmtErr:
		{
			auto * pErr = DownConst(&node, IllegalDoPseudoStmtErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s %s is not permitted following 'do'", parseErrorString, displayString(pErr->astkStmt));

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_IllegalTopLevelStmtErr:
		{
			auto * pErr = DownConst(&node, IllegalTopLevelStmtErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s %s is not permitted as a top level statement", parseErrorString, displayString(pErr->astkStmt));

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_InvokeFuncLiteralErr:
		{
			auto * pErr = DownConst(&node, InvokeFuncLiteralErr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s function literals can not be directly invoked", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;

		case ASTK_ArrowAfterFuncSymbolExpr:
		{
			auto * pErr = DownConst(&node, ArrowAfterFuncSymbolExpr);
			auto * pErrCasted = UpErrConst(pErr);

			printfmt("%s unexpected '->' after function symbol.Only parameter types may be specified", parseErrorString);

			if (pErrCasted->apChildren.cItem > 0)
			{
				// Sloppy... printChildren should probably handle the new line spacing so that if you pass
				//	it an empty array of children it will still just work.

				println();

				printTabs(pCtx, levelNext, false, false);
				printErrChildren(pCtx, *pErrCasted, levelNext);
			}
		} break;



		// EXPR

		case ASTK_BinopExpr:
		{
			auto * pExpr = DownConst(&node, BinopExpr);

			print(pExpr->pOp->lexeme.strv);
			println();

			printTabs(pCtx, levelNext, false, false);
			debugPrintSubAst(pCtx, *pExpr->pLhsExpr, levelNext, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			debugPrintSubAst(pCtx, *pExpr->pRhsExpr, levelNext, true);
		} break;

		case ASTK_GroupExpr:
		{
			auto * pExpr = DownConst(&node, GroupExpr);

			print("()");
			println();

			printTabs(pCtx, levelNext, false, false);
			debugPrintSubAst(pCtx, *pExpr->pExpr, levelNext, true);
		} break;

		case ASTK_LiteralExpr:
		{
			auto * pExpr = DownConst(&node, LiteralExpr);

			print(pExpr->pToken->lexeme.strv);
		} break;

		case ASTK_UnopExpr:
		{
			auto * pExpr = DownConst(&node, UnopExpr);

			print(pExpr->pOp->lexeme.strv);
			println();

			printTabs(pCtx, levelNext, false, false);
			debugPrintSubAst(pCtx, *pExpr->pExpr, levelNext, true);
		} break;

		case ASTK_SymbolExpr:
		{
			auto * pExpr = DownConst(&node, SymbolExpr);

			switch (pExpr->symbexprk)
			{
				case SYMBEXPRK_Var:
				{
					print("(var symbol): ");
					print(pExpr->ident.strv);
				}
				break;

				case SYMBEXPRK_MemberVar:
				{
					print("(member var symbol): ");
					print(pExpr->ident.strv);
					println();

					printTabs(pCtx, levelNext, false, false);
					println();

					printTabs(pCtx, levelNext, false, false);
					print("(owner):");
					debugPrintSubAst(pCtx, *pExpr->memberData.pOwner, levelNext, true);
				}
				break;

				case SYMBEXPRK_Func:
				{
					print("(func symbol): ");
					print(pExpr->ident.strv);

					// TODO: maybe print type disambig ??
				}
				break;

				case SYMBEXPRK_Unresolved:
				{
					print("(unresolved symbol): ");
					print(pExpr->ident.strv);
				}
				break;

				default:
				{
					AssertInfo(false, "Unknown symbexprk");
				}
			}
		} break;

		case ASTK_PointerDereferenceExpr:
		{
			auto * pExpr = DownConst(&node, PointerDereferenceExpr);
			print("<dereference>");
			println();

			printTabs(pCtx, levelNext, false, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			print("(pointer):");
			debugPrintSubAst(pCtx, *pExpr->pPointerExpr, levelNext, true);
		} break;

		case ASTK_ArrayAccessExpr:
		{
			auto * pExpr = DownConst(&node, ArrayAccessExpr);
			print("[]");
			println();

			printTabs(pCtx, levelNext, false, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			print("(array):");
			debugPrintSubAst(pCtx, *pExpr->pArrayExpr, levelNext, false);

			println();
			printTabs(pCtx, levelNext, false, false);
			print("(subscript):");
			debugPrintSubAst(pCtx, *pExpr->pSubscriptExpr, levelNext, true);
		} break;

		case ASTK_FuncCallExpr:
		{
			auto * pExpr = DownConst(&node, FuncCallExpr);
			print("(func call)");
			println();

			printTabs(pCtx, levelNext, false, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			print("(func):");

			bool noArgs = pExpr->apArgs.cItem == 0;

			debugPrintSubAst(pCtx, *pExpr->pFunc, levelNext, noArgs);

			printChildren(pCtx, pExpr->apArgs, levelNext, "arg", true);
		} break;

		case ASTK_FuncLiteralExpr:
		{
			auto * pStmt = DownConst(&node, FuncLiteralExpr);

			print("(func literal)");

			println();
			printTabs(pCtx, levelNext, false, false);

			debugPrintFuncHeader(
				pCtx,
				pStmt->pParamsReturnsGrp->apParamVarDecls,
				pStmt->pParamsReturnsGrp->apReturnVarDecls,
				levelNext,
				false);

			debugPrintSubAst(pCtx, *pStmt->pBodyStmt, levelNext, true);
		} break;



		// STMT;

		case ASTK_ExprStmt:
		{
			auto * pStmt = DownConst(&node, ExprStmt);

			print("(expr stmt)");
			println();

			printTabs(pCtx, levelNext, false, false);

			debugPrintSubAst(pCtx, *pStmt->pExpr, levelNext, true);
		} break;

		case ASTK_AssignStmt:
		{
			auto * pStmt = DownConst(&node, AssignStmt);

			print(pStmt->pAssignToken->lexeme.strv);
			println();

			printTabs(pCtx, levelNext, false, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			print("(lhs):");

			debugPrintSubAst(pCtx, *pStmt->pLhsExpr, levelNext, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			println();

			printTabs(pCtx, levelNext, false, false);
			print("(rhs):");
			println();

			printTabs(pCtx, levelNext, false, false);
			debugPrintSubAst(pCtx, *pStmt->pRhsExpr, levelNext, true);
		} break;

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = DownConst(&node, VarDeclStmt);

			print("(var decl)");
			println();

			printTabs(pCtx, levelNext, false, false);
			println();

			if (pStmt->ident.lexeme.strv.cCh > 0)
			{
				printTabs(pCtx, levelNext, false, false);
				print("(ident):");
				println();

				printTabs(pCtx, levelNext, true, false);
				print(pStmt->ident.lexeme.strv);
				println();

				printTabs(pCtx, levelNext, false, false);
				println();
			}

			printTabs(pCtx, levelNext, false, false);
			print("(type):");

			bool hasInitExpr = (pStmt->pInitExpr != nullptr);

			debugPrintType(pCtx, pStmt->typidDefn, levelNext, !hasInitExpr);
			println();
			printTabs(pCtx, levelNext, false, false);


			if (hasInitExpr)
			{
				println();
				printTabs(pCtx, levelNext, false, false);
				print("(init):");
				debugPrintSubAst(pCtx, *pStmt->pInitExpr, levelNext, true);
			}
		} break;

		case ASTK_StructDefnStmt:
		{
			auto * pStmt = DownConst(&node, StructDefnStmt);

			print("(struct defn)");

			println();
			printTabs(pCtx, levelNext, false, false);

			println();
			printTabs(pCtx, levelNext, false, false);
			print("(name):");

			println();
			printTabs(pCtx, levelNext, true, false);
			print(pStmt->ident.lexeme.strv);

			println();
			printTabs(pCtx, levelNext, false, false);

			printChildren(pCtx, pStmt->apVarDeclStmt, levelNext, "vardecl", true);
		} break;

		case ASTK_FuncDefnStmt:
		{
			auto * pStmt = DownConst(&node, FuncDefnStmt);

			print("(func defn)");

			println();
			printTabs(pCtx, levelNext, false, false);

			println();
			printTabs(pCtx, levelNext, false, false);
			print("(name):");

			println();
			printTabs(pCtx, levelNext, true, false);
			print(pStmt->ident.lexeme.strv);

			println();
			printTabs(pCtx, levelNext, false, false);

			debugPrintFuncHeader(
				pCtx,
				pStmt->pParamsReturnsGrp->apParamVarDecls,
				pStmt->pParamsReturnsGrp->apParamVarDecls,
				levelNext,
				false);

			debugPrintSubAst(pCtx, *pStmt->pBodyStmt, levelNext, true);
		} break;

		case ASTK_BlockStmt:
		{
			auto * pStmt = DownConst(&node, BlockStmt);

			print("(block)");

			if (pStmt->apStmts.cItem > 0)
			{
				println();
				printTabs(pCtx, levelNext, false, false);
			}

			printChildren(pCtx, pStmt->apStmts, levelNext, "stmt", true);
		} break;

		case ASTK_WhileStmt:
		{
			auto * pStmt = DownConst(&node, WhileStmt);

			print("(while)");

			println();
			printTabs(pCtx, levelNext, false, false);

			println();
			printTabs(pCtx, levelNext, false, false);
			print("(cond):");

			debugPrintSubAst(pCtx, *pStmt->pCondExpr, levelNext, false);

			println();
			printTabs(pCtx, levelNext, false, false);

			debugPrintSubAst(pCtx, *pStmt->pBodyStmt, levelNext, true);
		} break;

		case ASTK_IfStmt:
		{
			auto * pStmt = DownConst(&node, IfStmt);

			print("(if)");

			println();
			printTabs(pCtx, levelNext, false, false);

			println();
			printTabs(pCtx, levelNext, false, false);
			print("(cond):");

			debugPrintSubAst(pCtx, *pStmt->pCondExpr, levelNext, false);

			println();
			printTabs(pCtx, levelNext, false, false);

			debugPrintSubAst(pCtx, *pStmt->pThenStmt, levelNext, !pStmt->pElseStmt);

			if (pStmt->pElseStmt)
			{
				println();
				printTabs(pCtx, levelNext, false, false);

				println();
				printTabs(pCtx, levelNext, false, false);
				print("(else):");

				debugPrintSubAst(pCtx, *pStmt->pElseStmt, levelNext, true);
			}
		} break;

		case ASTK_ReturnStmt:
		{
			auto * pStmt = DownConst(&node, ReturnStmt);
			print("(return)");

			if (pStmt->pExpr)
			{
				println();
				printTabs(pCtx, levelNext, false, false);

				debugPrintSubAst(pCtx, *pStmt->pExpr, levelNext, true);
			}
		} break;

		case ASTK_BreakStmt:
		{
			auto * pStmt = DownConst(&node, BreakStmt);
			print("(break)");
		} break;

		case ASTK_ContinueStmt:
		{
			auto * pStmt = DownConst(&node, ContinueStmt);
			print("(continue)");
		} break;



		// PROGRAM

		case ASTK_Program:
		{
			auto * pNode = DownConst(&node, Program);

			print("(program)");
			println();

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
