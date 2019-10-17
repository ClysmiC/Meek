#include "ast.h"

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

const char * displayString(ASTK astk)
{
	switch (astk)
	{
		case ASTK_VarDeclStmt:
		{
			return "variable declaration statement";
		} break;

		case ASTK_FuncDefnStmt:
		{
			return "function definition statement";
		} break;

		case ASTK_StructDefnStmt:
		{
			return "struct definition statement";
		} break;

		case ASTK_BlockStmt:
		{
			return "statement block";
		} break;

		default:
		{
			AssertInfo(false, "Update this function with any ASTK display string that you want to be able to output in error messages");
            return "<internal compiler error>";
		}
	}
}
