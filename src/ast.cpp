#include "ast.h"

#include "ast_decorate.h"
#include "error.h"

FUNCID funcid(const AstNode & node)
{
	if (node.astk == ASTK_FuncDefnStmt)
	{
		return DownConst(&node, FuncDefnStmt)->funcid;
	}
	else if (node.astk == ASTK_FuncLiteralExpr)
	{
		return DownConst(&node, FuncLiteralExpr)->funcid;
	}
	else
	{
		return FUNCID_Nil;
	}
}

void walkAst(
	MeekCtx * pCtx,
	AstNode * pNodeSubtreeRoot,
	AstWalkVisitPreFn visitPreorderFn,
	AstWalkHookFn hookFn,
	AstWalkVisitPostFn visitPostorderFn,
	void * pContext)
{
	Assert(pNodeSubtreeRoot);
	Assert(visitPreorderFn);
	Assert(visitPostorderFn);

#if DEBUG
	{
		int iLine = getStartLine(*pCtx, pNodeSubtreeRoot->astid);
		if (iLine == 20)
		{
			bool brk = true;
		}
	}
#endif

	bool shouldVisitChildren = visitPreorderFn(pNodeSubtreeRoot, pContext);
	if (!shouldVisitChildren)
		return;

	if (category(pNodeSubtreeRoot->astk) == ASTCATK_Error)
	{
		AstErr * pErr = DownErr(pNodeSubtreeRoot);
		for (int iChild = 0; iChild < pErr->apChildren.cItem; iChild++)
		{
			walkAst(pCtx, pErr->apChildren[iChild], visitPreorderFn, hookFn, visitPostorderFn, pContext);
		}
	}

	switch (pNodeSubtreeRoot->astk)
	{
		case ASTK_UnopExpr:
		{
			auto * pNode = Down(pNodeSubtreeRoot, UnopExpr);
			walkAst(pCtx, pNode->pExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_BinopExpr:
		{
			auto * pNode = Down(pNodeSubtreeRoot, BinopExpr);
			walkAst(pCtx, pNode->pLhsExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			hookFn(Up(pNode), AWHK_BinopPostFirstOperand, pContext);
			walkAst(pCtx, pNode->pRhsExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_LiteralExpr:
			break;

		case ASTK_GroupExpr:
		{
			auto * pNode = Down(pNodeSubtreeRoot, GroupExpr);
			walkAst(pCtx, pNode->pExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_SymbolExpr:
		{
			auto * pNode = Down(pNodeSubtreeRoot, SymbolExpr);
			if (pNode->symbexprk == SYMBEXPRK_MemberVar)
			{
				walkAst(pCtx, pNode->memberData.pOwner, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_PointerDereferenceExpr:
		{
			auto * pNode = Down(pNodeSubtreeRoot, PointerDereferenceExpr);
			walkAst(pCtx, pNode->pPointerExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_ArrayAccessExpr:
		{
			// NOTE (andrew) in foo[bar], this evaluates foo, then bar

			auto * pNode = Down(pNodeSubtreeRoot, ArrayAccessExpr);
			walkAst(pCtx, pNode->pArrayExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			walkAst(pCtx, pNode->pSubscriptExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_FuncCallExpr:
		{
			// NOTE (andrew) in foo(bar, baz), this evaluates foo, then bar, then baz.

			auto * pNode = Down(pNodeSubtreeRoot, FuncCallExpr);
			walkAst(pCtx, pNode->pFunc, visitPreorderFn, hookFn, visitPostorderFn, pContext);

			for (int iArg = 0; iArg < pNode->apArgs.cItem; iArg++)
			{
				walkAst(pCtx, pNode->apArgs[iArg], visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_FuncLiteralExpr:
		{
			auto * pNode = Down(pNodeSubtreeRoot, FuncLiteralExpr);
			walkAst(pCtx, Up(pNode->pParamsReturnsGrp), visitPreorderFn, hookFn, visitPostorderFn, pContext);
			hookFn(Up(pNode), AWHK_FuncPostFormalReturnVardecls, pContext);
			walkAst(pCtx, pNode->pBodyStmt, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_ExprStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, ExprStmt);
			walkAst(pCtx, pNode->pExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_AssignStmt:
		{
			// NOTE (andrew) in foo = bar, this evaluates foo, then bar.

			auto * pNode = Down(pNodeSubtreeRoot, AssignStmt);
			walkAst(pCtx, pNode->pLhsExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			hookFn(Up(pNode), AWHK_AssignPostLhs, pContext);
			walkAst(pCtx, pNode->pRhsExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_VarDeclStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, VarDeclStmt);
			if (pNode->pInitExpr)
			{
				walkAst(pCtx, pNode->pInitExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_FuncDefnStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, FuncDefnStmt);
			walkAst(pCtx, Up(pNode->pParamsReturnsGrp), visitPreorderFn, hookFn, visitPostorderFn, pContext);
			hookFn(Up(pNode), AWHK_FuncPostFormalReturnVardecls, pContext);
			walkAst(pCtx, pNode->pBodyStmt, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_StructDefnStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, StructDefnStmt);
			for (int iVarDecl = 0; iVarDecl < pNode->apVarDeclStmt.cItem; iVarDecl++)
			{
				walkAst(pCtx, pNode->apVarDeclStmt[iVarDecl], visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_IfStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, IfStmt);
			walkAst(pCtx, pNode->pCondExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			hookFn(Up(pNode), AWHK_IfPostCondition, pContext);
			walkAst(pCtx, pNode->pThenStmt, visitPreorderFn, hookFn, visitPostorderFn, pContext);

			if (pNode->pElseStmt)
			{
				hookFn(Up(pNode), AWHK_IfPreElse, pContext);
				walkAst(pCtx, pNode->pElseStmt, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_WhileStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, WhileStmt);
			walkAst(pCtx, pNode->pCondExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			hookFn(Up(pNode), AWHK_WhilePostCondition, pContext);
			walkAst(pCtx, pNode->pBodyStmt, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_BlockStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, BlockStmt);
			for (int iStmt = 0; iStmt < pNode->apStmts.cItem; iStmt++)
			{
				walkAst(pCtx, pNode->apStmts[iStmt], visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_ReturnStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, ReturnStmt);
			if (pNode->pExpr)
			{
				walkAst(pCtx, pNode->pExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_BreakStmt:
			break;

		case ASTK_ContinueStmt:
			break;

		case ASTK_PrintStmt:
		{
			auto * pNode = Down(pNodeSubtreeRoot, PrintStmt);
			walkAst(pCtx, pNode->pExpr, visitPreorderFn, hookFn, visitPostorderFn, pContext);
		} break;

		case ASTK_ParamsReturnsGrp:
		{
			auto * pNode = Down(pNodeSubtreeRoot, ParamsReturnsGrp);
			for (int iParam = 0; iParam < pNode->apParamVarDecls.cItem; iParam++)
			{
				walkAst(pCtx, pNode->apParamVarDecls[iParam], visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}

			for (int iParam = 0; iParam < pNode->apReturnVarDecls.cItem; iParam++)
			{
				walkAst(pCtx, pNode->apReturnVarDecls[iParam], visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;

		case ASTK_Program:
		{
			auto * pNode = Down(pNodeSubtreeRoot, Program);
			for (int iNode = 0; iNode < pNode->apNodes.cItem; iNode++)
			{
				walkAst(pCtx, pNode->apNodes[iNode], visitPreorderFn, hookFn, visitPostorderFn, pContext);
			}
		} break;
	}

	visitPostorderFn(pNodeSubtreeRoot, pContext);
}

f32 floatValue(AstLiteralExpr * pLiteralExpr)
{
	Assert(pLiteralExpr->literalk == LITERALK_Float);

	if (!pLiteralExpr->isValueSet)
	{
		// God this is so bad...

		char * buffer = new char[u64(pLiteralExpr->pToken->lexeme.strv.cCh) + 1];
		Defer(delete [] buffer);

		memcpy(buffer, pLiteralExpr->pToken->lexeme.strv.pCh, pLiteralExpr->pToken->lexeme.strv.cCh);
		buffer[pLiteralExpr->pToken->lexeme.strv.cCh] = '\0';

		// TOOD: Error check this.... in fact just rewrite it....

		pLiteralExpr->floatValue = strtof(buffer, nullptr);
		pLiteralExpr->isValueSet = true;
		pLiteralExpr->isValueErroneous = false;
	}

	return pLiteralExpr->floatValue;
}

s32 intValue(AstLiteralExpr * pLiteralExpr)
{
	Assert(pLiteralExpr->literalk == LITERALK_Int);

	if (!pLiteralExpr->isValueSet)
	{
		StringView lexeme = pLiteralExpr->pToken->lexeme.strv;
		int base = 10;

		// NOTE: Scanner will assign at most one '-' to
		//	a number. Any other '-' (as in ---5) will be
		//	considered unary minus operator.

		int iChPostPrefix = 0;
		bool isNeg = false;
		{
			int iCh = 0;
			isNeg = lexeme.pCh[0] == '-';
			if (isNeg) iCh++;

			if (lexeme.pCh[iCh] == '0')
			{
				iCh++;
				switch (lexeme.pCh[iCh])
				{
					case 'b': base = 2; iCh++; break;
					case 'o': base = 8; iCh++; break;
					case 'x': base = 16; iCh++; break;
					default: iCh--;		// the leading 0 is part of the actual number
				}
			}

			iChPostPrefix = iCh;
		}

		// NOTE (andrew) Handle different widths

		static const char * s_int32MaxDigits = "2147483647";
		static const char * s_int32MinDigits = "2147483648";
		static int s_cDigitInt32Limit = 10;

		// Detect out of range by analyzing individual digits

		{
			int iChFirstNonZero = -1;
			int cDigitNoLeadingZeros = 0;
			for (int iCh = iChPostPrefix; iCh < lexeme.cCh; iCh++)
			{
				if (lexeme.pCh[iCh] != '0')
				{
					cDigitNoLeadingZeros = lexeme.cCh - iCh;
					iChFirstNonZero = iCh;
					break;
				}
			}

			Assert(Iff(iChFirstNonZero == -1, cDigitNoLeadingZeros == 0));

			if (cDigitNoLeadingZeros > s_cDigitInt32Limit)
			{
				goto LOutOfRangeFail;
			}
			else if (cDigitNoLeadingZeros == s_cDigitInt32Limit)
			{
				const char * int32Digits = (isNeg) ? s_int32MinDigits : s_int32MaxDigits;

				int iDigit = 0;
				for (int iCh = iChFirstNonZero; iCh < lexeme.cCh; iCh++)
				{

					if (lexeme.pCh[iCh] > int32Digits[iDigit])
					{
						goto LOutOfRangeFail;
					}
					else if (lexeme.pCh[iCh] < int32Digits[iDigit])
					{
						break;
					}

					iDigit++;
				}
			}
		}

		// Build value digit by digit now that we know it's in range

		int value = 0;
		for (int iCh = iChPostPrefix; iCh < lexeme.cCh; iCh++)
		{
			char c = lexeme.pCh[iCh];
			int nDigit = 0;

			if (c >= '0' && c <= '9')
			{
				nDigit = c - '0';
			}
			else
			{
				// NOTE (andrew) Scanner makes sure that only legal digits are in the literal based on the prefix

				Assert((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));

				if (c >= 'a')
				{
					c = c - ('a' - 'A');
				}

				nDigit = c - 'A';
			}

			value *= base;
			value += nDigit;
		}

		pLiteralExpr->isValueSet = true;
		pLiteralExpr->isValueErroneous = false;
		pLiteralExpr->intValue = value;
	}

	return pLiteralExpr->intValue;

LOutOfRangeFail:
	pLiteralExpr->isValueSet = true;
	pLiteralExpr->isValueErroneous = true;
	pLiteralExpr->intValue = 0;
	return 0;
}

bool boolValue(AstLiteralExpr * pLiteralExpr)
{
	// TOOD: This is stupid. This should be set in the parser. I need to come up with a more coherent story
	//	for exactly when all of these literals get parsed.

	Assert(pLiteralExpr->literalk == LITERALK_Bool);

	if (!pLiteralExpr->isValueSet)
	{
		pLiteralExpr->boolValue = (pLiteralExpr->pToken->lexeme.strv.pCh[0] == 't');
	}

	return pLiteralExpr->boolValue;
}

bool isLValue(ASTK astk)
{
	if (category(astk) != ASTCATK_Expr)
	{
		AssertInfo(false, "Only care about lvalue vs rvalue when dealing with expressions. Why call this with a non-expr node?");
		return false;
	}

	switch (astk)
	{
		case ASTK_SymbolExpr:
		case ASTK_PointerDereferenceExpr:
		case ASTK_ArrayAccessExpr:
			return true;

		default:
			return false;
	}
}

bool isAddressable(ASTK astk)
{
	if (category(astk) != ASTCATK_Expr)
	{
		AssertInfo(false, "Only care about isAddressable when dealing with expressions. Why call this with a non-expr node?");
		return false;
	}

	bool result = false;
	switch (astk)
	{
		case ASTK_SymbolExpr:
		case ASTK_ArrayAccessExpr:
		{
			result = true;
		} break;

		default:
		{
			result = false;
		} break;
	}

	Assert(Implies(result, isLValue(astk)));
	return result;
}


const char * displayString(ASTK astk, bool capitalizeFirstLetter)
{
	// TODO: improve this...

	switch (astk)
	{
		case ASTK_ExprStmt:
		{
			return (capitalizeFirstLetter) ?
				"Expression statement" :
				"expression statement";
		} break;

		case ASTK_AssignStmt:
		{
			return (capitalizeFirstLetter) ?
				"Assignment" :
				"assignment";
		} break;

		case ASTK_VarDeclStmt:
		{
			return (capitalizeFirstLetter) ?
				"Variable declaration" :
				"variable declaration";
		} break;

		case ASTK_FuncDefnStmt:
		{
			return (capitalizeFirstLetter) ?
				"Function definition" :
				"function definition";
		} break;

		case ASTK_StructDefnStmt:
		{
			return (capitalizeFirstLetter) ?
				"Struct definition" :
				"struct definition";
		} break;

		case ASTK_IfStmt:
		{
			// NOTE: no caps
			return "'if' statement";
		} break;

		case ASTK_WhileStmt:
		{
			// NOTE: no caps
			return "'while' statement";
		} break;

		case ASTK_BlockStmt:
		{
			return (capitalizeFirstLetter) ?
				"Statement block" :
				"statement block";
		} break;

		case ASTK_ReturnStmt:
		{
			// NOTE: no caps
			return "'return' statement";
		} break;

		case ASTK_BreakStmt:
		{
			// NOTE: no caps
			return "'break' statement";
		} break;

		case ASTK_ContinueStmt:
		{
			// NOTE: no caps
			return "'continue' statement";
		} break;

		default:
		{
			reportIceAndExit("No display string listed for astk %d", astk);
            return "<internal compiler error>";
		}
	}
}
