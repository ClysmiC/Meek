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

	init(&pMeekCtx->scopes);
	Assert(pMeekCtx->scopes.cItem == g_parser.pScopeBuiltin->id);
	Assert(pMeekCtx->scopes.cItem + 1 == g_parser.pScopeGlobal->id);
	append(&pMeekCtx->scopes, g_parser.pScopeBuiltin);
	append(&pMeekCtx->scopes, g_parser.pScopeGlobal);

	init(&g_typeTable, pMeekCtx);
	init(&g_astDecs);

	init(&pMeekCtx->functions);
	pMeekCtx->scanner = &g_scanner;
	pMeekCtx->parser = &g_parser;
	pMeekCtx->typeTable = &g_typeTable;
	pMeekCtx->astDecorations = &g_astDecs;

	pMeekCtx->rootNode = nullptr;

	pMeekCtx->mainFuncid = FuncId::Nil;
}

AstNode * funcNodeFromFuncid(MeekCtx * pCtx, FuncId funcid)
{
	Assert(funcid < FuncId(pCtx->functions.cItem));
	return pCtx->functions[(int)funcid];
}
