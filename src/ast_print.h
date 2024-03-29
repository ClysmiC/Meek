#pragma once

#include "als.h"
#include "type.h"

// NOTE: This is purely debug code that is hacked together without much care or thought.

#if DEBUG

struct AstNode;
struct AstErr;
struct Type;
struct TypeTable;

struct DebugPrintCtx
{
	TypeTable * typeTable;
	DynamicArray<bool> mpLevelSkip;
};

// Public

void debugPrintAst(DebugPrintCtx * pCtx, const AstNode & root);
void init(DebugPrintCtx * pPrintCtx, MeekCtx * pMeekCtx);

// Internal

void debugPrintSubAst(DebugPrintCtx * pCtx, const AstNode & node, int level, bool skipAfterArrow);

void setSkip(DebugPrintCtx * pCtx, int level, bool skip);

void printTabs(DebugPrintCtx * pCtx, int level, bool printArrows, bool skipAfterArrow);

void printChildren(DebugPrintCtx * pCtx, const DynamicArray<AstNode *> & apChildren, int level, const char * label, bool setSkipOnLastChild);

void printErrChildren(DebugPrintCtx * pCtx, const AstErr & node, int level);

void debugPrintFuncHeader(DebugPrintCtx * pCtx, const DynamicArray<AstNode *> & apParamVarDecls, const DynamicArray<AstNode *> & apReturnVarDecls, int level, bool skipAfterArrow);

void debugPrintType(DebugPrintCtx * pCtx, TypeId typid, int level, bool skipAfterArrow);

#endif
