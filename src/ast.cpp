#include "ast.h"

bool containsErrorNode(const DynamicArray<AstNode *> & apNodes)
{
	for (uint i = 0; i < apNodes.cItem; i++)
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
