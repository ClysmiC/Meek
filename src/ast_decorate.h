#pragma once

#include "als.h"
#include "global_context.h"
#include "id_def.h"
#include "token.h"

// Lookaside table for per-ast node information that we aren't storing on the AST itself

template<typename T>
struct AstDecorationTable
{
	struct Decoration
	{
		T decoration;
		bool isSet;
	};

	DynamicArray<Decoration> table;
};

template<typename T>
inline void init(AstDecorationTable<T> * pDecTable)
{
	init(&pDecTable->table);
}

template<typename T>
void decorate(AstDecorationTable<T> * pDecTable, ASTID astid, T decoration)
{
	// Insert dummies up to the provided id, if needed

	while (static_cast<uint>(pDecTable->table.cItem) <= astid)
	{
		auto * pDecoration = appendNew(&pDecTable->table);
		pDecoration->isSet = false;
	}

	pDecTable->table[astid].isSet = true;
	pDecTable->table[astid].decoration = decoration;
}

template<typename T>
T getDecoration(const AstDecorationTable<T> & decTable, ASTID astid, bool * poSuccess=nullptr)
{
	T result;

	if (static_cast<uint>(decTable.table.cItem) <= astid)
	{
		if (poSuccess) *poSuccess = false;
		else AssertInfo(false, "If you aren't 100% sure that the decoration is in the table, you should query for success with poSuccess");
	}
	else
	{
		if (poSuccess) *poSuccess = true;
		result = decTable.table[astid].decoration;
	}

	return result;
}


struct AstDecorations
{
	AstDecorationTable<StartEndIndices> startEndDecoration;
};

void init(AstDecorations * astDecorations);

StartEndIndices getStartEnd(const AstDecorations & astDecs, ASTID astid, bool * poSuccess=nullptr);
StartEndIndices getStartEnd(const AstDecorations & astDecs, ASTID astidStartStart, ASTID astidEndEnd, bool * poSuccess=nullptr);
int getStartLine(const MeekCtx & ctx, ASTID astid);
