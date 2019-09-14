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
