#pragma once

#include "als.h"

// NOTE: This is purely debug code that is hacked together without much care or thought.

#if DEBUG

struct AstNode;
struct AstErr;
struct Type;

// Public

void debugPrintAst(const AstNode & pRoot);

// Internal

void debugPrintSubAst(const AstNode & pNode, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

void debugPrintAst(const AstNode & root);

void setSkip(DynamicArray<bool> * pMapLevelSkip, int level, bool skip);

void printTabs(int level, bool printArrows, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

void printChildren(const DynamicArray<AstNode *> & apChildren, int level, const char * label, bool setSkipOnLastChild, DynamicArray<bool> * pMapLevelSkip);

void printErrChildren(const AstErr & node, int level, DynamicArray<bool> * pMapLevelSkip);

void debugPrintFuncHeader(const DynamicArray<AstNode *> & apParamVarDecls, const DynamicArray<AstNode *> & apReturnVarDecls, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

void debugPrintType(const Type & type, int level, bool skipAfterArrow, DynamicArray<bool> * pMapLevelSkip);

#endif
