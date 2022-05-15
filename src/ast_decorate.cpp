#include "ast_decorate.h"
#include "scan.h"

void init(AstDecorations * astDecorations)
{
	init(&astDecorations->startEndDecoration);
}

StartEndIndices getStartEnd(const AstDecorations & astDecs, ASTID astid, bool * poSuccess)
{
	return getDecoration(astDecs.startEndDecoration, astid, poSuccess);
}

StartEndIndices getStartEnd(const AstDecorations & astDecs, ASTID astidStartStart, ASTID astidEndEnd, bool * poSuccess)
{
	bool success;

	StartEndIndices result;
	result.iStart = getStartEnd(astDecs, astidStartStart, &success).iStart;

	if (!success)
	{
		if (poSuccess) *poSuccess = false;
		else AssertInfo(false, "If you aren't 100% sure that the decorations are in the table, you should query for success with poSuccess");
	}
	else
	{
		result.iEnd = getStartEnd(astDecs, astidEndEnd, &success).iEnd;

		if (!success)
		{
			if (poSuccess) *poSuccess = false;
			else AssertInfo(false, "If you aren't 100% sure that the decorations are in the table, you should query for success with poSuccess");
		}
		else
		{
			if (poSuccess) *poSuccess = true;
		}
	}

	return result;
}

int getStartLine(const MeekCtx & ctx, ASTID astid)
{
	bool success;
	StartEndIndices startEnd = getStartEnd(*ctx.astDecorations, astid, &success);

	if (!success)
		return -1;

	return lineFromI(*ctx.scanner, startEnd.iStart);
}
