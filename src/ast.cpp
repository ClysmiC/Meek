#include "ast.h"

#include "error.h"

int intValue(AstLiteralExpr * pLiteralExpr)
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

bool containsErrorNode(const DynamicArray<AstNode *> & apNodes)
{
	for (int i = 0; i < apNodes.cItem; i++)
	{
		const AstNode * pNode = apNodes[i];
		if (category(*pNode) == ASTCATK_Error)
		{
			return true;
		}
	}

	return false;
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
