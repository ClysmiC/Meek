#include "global_context.h"

#include "scan.h"
#include "parse.h"
#include "type.h"

static Scanner g_scanner;
static Parser g_parser;
static TypeTable g_typeTable;
static AstDecorations g_astDecs;

void init(MeekCtx * pMeekCtx, char * pText, uint textSize)
{
	init(&g_scanner, pText, textSize);
	init(&g_parser, pMeekCtx);

	init(&pMeekCtx->mpScopeidPScope);
	Assert(pMeekCtx->mpScopeidPScope.cItem == g_parser.pScopeBuiltin->id);
	Assert(pMeekCtx->mpScopeidPScope.cItem + 1 == g_parser.pScopeGlobal->id);
	append(&pMeekCtx->mpScopeidPScope, g_parser.pScopeBuiltin);
	append(&pMeekCtx->mpScopeidPScope, g_parser.pScopeGlobal);

	init(&g_typeTable, pMeekCtx);
	init(&g_astDecs);

	init(&pMeekCtx->apFuncDefnAndLiteral);
	pMeekCtx->pScanner = &g_scanner;
	pMeekCtx->pParser = &g_parser;
	pMeekCtx->pTypeTable = &g_typeTable;
	pMeekCtx->pAstDecs = &g_astDecs;

	pMeekCtx->pNodeRoot = nullptr;
}
