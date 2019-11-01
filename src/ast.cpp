#include "ast.h"

#include "error.h"

int intValue(AstLiteralExpr * pLiteralExpr)
{
	Assert(pLiteralExpr->literalk == LITERALK_Int);

	if (!pLiteralExpr->isValueSet)
	{
		char * lexeme = pLiteralExpr->pToken->lexeme;
		int base = 10;

		// NOTE: Scanner will assign at most one '-' to
		//	a number. Any other '-' (as in ---5) will be
		//	considered unary minus operator.

		int iCursor = 0;
		bool isNeg = lexeme[0] == '-';
		if (isNeg) iCursor++;

		if (lexeme[iCursor] == '0')
		{
			iCursor++;
			switch (lexeme[iCursor])
			{
				case 'b': base = 2; iCursor++; break;
				case 'o': base = 8; iCursor++; break;
				case 'x': base = 16; iCursor++; break;
				default: iCursor--;		// the leading 0 is part of the actual number
			}
		}

		char * end;
		long value = strtol(lexeme, &end, base);

		if (errno == ERANGE || value > S32_MAX || value < S32_MIN)
		{
			// TODO: Report out of range error

			pLiteralExpr->isValueErroneous = true;
		}
		else if (errno || end == lexeme)
		{
			// TODO: Report unspecified error

			pLiteralExpr->isValueErroneous = true;
		}
		else
		{
			pLiteralExpr->intValue = (int)value;
			pLiteralExpr->isValueErroneous = false;
		}

		pLiteralExpr->isValueSet = true;
	}

	return pLiteralExpr->intValue;
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
		case ASTK_VarExpr:
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
