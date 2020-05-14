#include "bytecode.h"

#include "ast.h"
#include "ast_decorate.h"
#include "error.h"
#include "global_context.h"
#include "interp.h"
#include "print.h"

#include <inttypes.h>

//const int gc_mpBcopCByte[] = {
//	1,		// BCOP_Return
//	2,		// BCOP_PushImmediate8
//	3,		// BCOP_PushImmediate16
//	5,		// BCOP_PushImmediate32
//	9,		// BCOP_PushImmediate64
//	1,		// BCOP_PushImmediateTrue
//	1,		// BCOP_PushImmediateFalse
//	1 + sizeof(uintptr),	// BCOP_PushLocalValue8
//	1 + sizeof(uintptr),	// BCOP_PushLocalValue16
//	1 + sizeof(uintptr),	// BCOP_PushLocalValue32
//	1 + sizeof(uintptr),	// BCOP_PushLocalValue64
//	1 + sizeof(uintptr),	// BCOP_PushGlobalValue8
//	1 + sizeof(uintptr),	// BCOP_PushGlobalValue16
//	1 + sizeof(uintptr),	// BCOP_PushGlobalValue32
//	1 + sizeof(uintptr),	// BCOP_PushGlobalValue64
//	1 + sizeof(uintptr),	// BCOP_PushLocalAddr
//	1 + sizeof(uintptr),	// BCOP_PushGlobalAddr
//	1 + sizeof(uintptr),	// BCOP_AddToAddr
//	1,		// BCOP_AddInt8
//	1,		// BCOP_AddInt16
//	1,		// BCOP_AddInt32
//	1,		// BCOP_AddInt64
//	1,		// BCOP_SubInt8
//	1,		// BCOP_SubInt16
//	1,		// BCOP_SubInt32
//	1,		// BCOP_SubInt64
//	1,		// BCOP_MulInt8
//	1,		// BCOP_MulInt16
//	1,		// BCOP_MulInt32
//	1,		// BCOP_MulInt64
//	1,		// BCOP_DivS8
//	1,		// BCOP_DivS16
//	1,		// BCOP_DivS32
//	1,		// BCOP_DivS64
//	1,		// BCOP_DivU8
//	1,		// BCOP_DivU16
//	1,		// BCOP_DivU32
//	1,		// BCOP_DivU64
//	1,		// BCOP_AddFloat32
//	1,		// BCOP_AddFloat64
//	1,		// BCOP_SubFloat32
//	1,		// BCOP_SubFloat64
//	1,		// BCOP_MulFloat32
//	1,		// BCOP_MulFloat64
//	1,		// BCOP_DivFloat32
//	1,		// BCOP_DivFloat64
//	1,		// BCOP_NegateS8
//	1,		// BCOP_NegateS16
//	1,		// BCOP_NegateS32
//	1,		// BCOP_NegateS64
//	1,		// BCOP_NegateFloat32
//	1,		// BCOP_NegateFloat64
//	// HMM: Do these need the lhs address in the bytecode? It should probably just be on the stack, no?
//	1 + sizeof(uintptr),	// BCOP_Assign8
//	1 + sizeof(uintptr),	// BCOP_Assign16
//	1 + sizeof(uintptr),	// BCOP_Assign32
//	1 + sizeof(uintptr),	// BCOP_Assign64
//	1,		// BCOP_Pop8
//	1,		// BCOP_Pop16
//	1,		// BCOP_Pop32
//	1,		// BCOP_Pop64
//	5,		// BCOP_PopX
//	1,		// BCOP_Reserve8
//	1,		// BCOP_Reserve16
//	1,		// BCOP_Reserve32
//	1,		// BCOP_Reserve64
//	5,		// BCOP_ReserveX
//};
//StaticAssert(ArrayLen(gc_mpBcopCByte) == BCOP_Max);

static const char * c_mpBcopStrName[] = {
	"LoadImmediate8",
	"LoadImmediate16",
	"LoadImmediate32",
	"LoadImmediate64",
	"LoadTrue",
	"LoadFalse",
	"Load8",
	"Load16",
	"Load32",
	"Load64",
	"Store8",
	"Store16",
	"Store32",
	"Store64",
	"Duplicate8",
	"Duplicate16",
	"Duplicate32",
	"Duplicate64",
	"AddInt8",
	"AddInt16",
	"AddInt32",
	"AddInt64",
	"SubInt8",
	"SubInt16",
	"SubInt32",
	"SubInt64",
	"MulInt8",
	"MulInt16",
	"MulInt32",
	"MulInt64",
	"DivS8",
	"DivS16",
	"DivS32",
	"DivS64",
	"DivU8",
	"DivU16",
	"DivU32",
	"DivU64",
	"AddFloat32",
	"AddFloat64",
	"SubFloat32",
	"SubFloat64",
	"MulFloat32",
	"MulFloat64",
	"DivFloat32",
	"DivFloat64",
	"TestEqInt8",
	"TestEqInt16",
	"TestEqInt32",
	"TestEqInt64",
	"TestLtS8",
	"TestLtS16",
	"TestLtS32",
	"TestLtS64",
	"TestLtU8",
	"TestLtU16",
	"TestLtU32",
	"TestLtU64",
	"TestLteS8",
	"TestLteS16",
	"TestLteS32",
	"TestLteS64",
	"TestLteU8",
	"TestLteU16",
	"TestLteU32",
	"TestLteU64",
	"TestEqFloat32",
	"TestEqFloat64",
	"TestLtFloat32",
	"TestLtFloat64",
	"TestLteFloat32",
	"TestLteFloat64",
	"Not",
	"NegateS8",
	"NegateS16",
	"NegateS32",
	"NegateS64",
	"NegateFloat32",
	"NegateFloat64",
	"Jump",
	"JumpIfFalse",
	"JumpIfPeekFalse",
	"JumpIfPeekTrue",
	"StackAlloc",
	"StackFree",
	"Call",
	"Return",
	"DebugPrint",
	"DebugExit",
};
StaticAssert(ArrayLen(c_mpBcopStrName) == BCOP_Max);

BCOP bcopSized(SIZEDBCOP sizedBcop, int cBit)
{
	Assert(cBit == 8 || cBit == 16 || cBit == 32 || cBit == 64);

	switch (sizedBcop)
	{
		case SIZEDBCOP_LoadImmediate:
		{
			switch (cBit)
			{
				case 8:			return BCOP_LoadImmediate8;
				case 16:		return BCOP_LoadImmediate16;
				case 32:		return BCOP_LoadImmediate32;
				case 64:		return BCOP_LoadImmediate64;
				default:		AssertNotReached;	return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_Load:
		{
			switch (cBit)
			{
				case 8:			return BCOP_Load8;
				case 16:		return BCOP_Load16;
				case 32:		return BCOP_Load32;
				case 64:		return BCOP_Load64;
				default:		AssertNotReached;	return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_Store:
		{
			switch (cBit)
			{
				case 8:			return BCOP_Store8;
				case 16:		return BCOP_Store16;
				case 32:		return BCOP_Store32;
				case 64:		return BCOP_Store64;
				default:		AssertNotReached;	return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_Duplicate:
		{
			switch (cBit)
			{
				case 8:			return BCOP_Duplicate8;
				case 16:		return BCOP_Duplicate16;
				case 32:		return BCOP_Duplicate32;
				case 64:		return BCOP_Duplicate64;
				default:		AssertNotReached;	return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_AddInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_AddInt8;
				case 16:	return BCOP_AddInt16;
				case 32:	return BCOP_AddInt32;
				case 64:	return BCOP_AddInt64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_SubInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_SubInt8;
				case 16:	return BCOP_SubInt16;
				case 32:	return BCOP_SubInt32;
				case 64:	return BCOP_SubInt64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_MulInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_MulInt8;
				case 16:	return BCOP_MulInt16;
				case 32:	return BCOP_MulInt32;
				case 64:	return BCOP_MulInt64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_DivSignedInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_DivS8;
				case 16:	return BCOP_DivS16;
				case 32:	return BCOP_DivS32;
				case 64:	return BCOP_DivS64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_DivUnsignedInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_DivU8;
				case 16:	return BCOP_DivU16;
				case 32:	return BCOP_DivU32;
				case 64:	return BCOP_DivU64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_AddFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_AddFloat32;
				case 64:	return BCOP_AddFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_SubFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_SubFloat32;
				case 64:	return BCOP_SubFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_MulFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_MulFloat32;
				case 64:	return BCOP_MulFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_DivFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_DivFloat32;
				case 64:	return BCOP_DivFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_NegateSigned:
		{
			switch (cBit)
			{
				case 8:		return BCOP_NegateS8;
				case 16:	return BCOP_NegateS16;
				case 32:	return BCOP_NegateS32;
				case 64:	return BCOP_NegateS64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_NegateFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_NegateFloat32;
				case 64:	return BCOP_NegateFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestEqInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_TestEqInt8;
				case 16:	return BCOP_TestEqInt16;
				case 32:	return BCOP_TestEqInt32;
				case 64:	return BCOP_TestEqInt64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestLtSignedInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_TestLtS8;
				case 16:	return BCOP_TestLtS16;
				case 32:	return BCOP_TestLtS32;
				case 64:	return BCOP_TestLtS64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestLtUnsignedInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_TestLtU8;
				case 16:	return BCOP_TestLtU16;
				case 32:	return BCOP_TestLtU32;
				case 64:	return BCOP_TestLtU64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestLteSignedInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_TestLteS8;
				case 16:	return BCOP_TestLteS16;
				case 32:	return BCOP_TestLteS32;
				case 64:	return BCOP_TestLteS64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestLteUnsignedInt:
		{
			switch (cBit)
			{
				case 8:		return BCOP_TestLteU8;
				case 16:	return BCOP_TestLteU16;
				case 32:	return BCOP_TestLteU32;
				case 64:	return BCOP_TestLteU64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestEqFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_TestEqFloat32;
				case 64:	return BCOP_TestEqFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestLtFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_TestLtFloat32;
				case 64:	return BCOP_TestLtFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		case SIZEDBCOP_TestLteFloat:
		{
			switch (cBit)
			{
				case 32:	return BCOP_TestLteFloat32;
				case 64:	return BCOP_TestLteFloat64;
				default:	AssertNotReached;		return BCOP_Nil;
			}
		} break;

		default:
							AssertNotReached;		return BCOP_Nil;
	}
}

void init(BytecodeFunction * pBcf)
{
	init(&pBcf->bytes);
	init(&pBcf->sourceLineNumbers);
}

void dispose(BytecodeFunction * pBcf)
{
	dispose(&pBcf->bytes);
	dispose(&pBcf->sourceLineNumbers);
}

void init(BytecodeBuilder * pBuilder, MeekCtx * pCtx)
{
	pBuilder->pCtx = pCtx;
	init(&pBuilder->aBytecodeFunc);
	init(&pBuilder->nodeCtxStack);
}

void dispose(BytecodeBuilder * pBuilder)
{
	dispose(&pBuilder->aBytecodeFunc);
}

void init(BytecodeBuilder::NodeCtx * pNodeCtx, AstNode * pNode)
{
	ClearStruct(pNodeCtx);

	pNodeCtx->pNode = pNode;
	pNodeCtx->wantsChildExprAddr = false;
}

void compileBytecode(BytecodeBuilder * pBuilder)
{
	MeekCtx * pCtx = pBuilder->pCtx;

	for (int i = 0; i < pCtx->apFuncDefnAndLiteral.cItem; i++)
	{
		AstNode * pNode = pCtx->apFuncDefnAndLiteral[i];
		AstParamsReturnsGrp * pParamsReturns = nullptr;
		// AstNode * pBodyStmt = nullptr;

		if (pNode->astk == ASTK_FuncDefnStmt)
		{
			// auto * pStmt = Down(pNode, FuncDefnStmt);
			// pParamsReturns = pStmt->pParamsReturnsGrp;
			// pBodyStmt = pStmt->pBodyStmt;
		}
		else
		{
			Assert(pNode->astk == ASTK_FuncLiteralExpr);

			// auto * pExpr = Down(pNode, FuncLiteralExpr);
			// pParamsReturns = pExpr->pParamsReturnsGrp;
			// pBodyStmt = pExpr->pBodyStmt;
		}

		BytecodeFunction * pBytecodeFunc = appendNew(&pBuilder->aBytecodeFunc);
		init(pBytecodeFunc);

		pBuilder->pBytecodeFuncMain = pBytecodeFunc;		// TODO: Actually detect main
		pBuilder->pBytecodeFuncCompiling = pBytecodeFunc;

		pBuilder->root = true;
		walkAst(
			pCtx,
			pNode,
			&visitBytecodeBuilderPreorder,
			&visitBytecodeBuilderHook,
			&visitBytecodeBuilderPostOrder,
			pBuilder);
	}
}

void emitOp(BytecodeFunction * bcf, BCOP byteEmit, int lineNumber)
{
	StaticAssert(sizeof(BCOP) == sizeof(u8));

	append(&bcf->bytes, u8(byteEmit));
	append(&bcf->sourceLineNumbers, lineNumber);
	
}

void emit(BytecodeFunction * bcf, u8 byteEmit)
{
	append(&bcf->bytes, byteEmit);
}

void emit(BytecodeFunction * bcf, s8 byteEmit)
{
	u8 * pDst = appendNew(&bcf->bytes);
	memcpy(pDst, &byteEmit, 1);
}

void emit(BytecodeFunction * bcf, u16 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, s16 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, u32 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, s32 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, u64 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, s64 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, f32 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, f64 bytesEmit)
{
	emit(bcf, &bytesEmit, sizeof(bytesEmit));
}

void emit(BytecodeFunction * bcf, void * pBytesEmit, int cBytesEmit)
{
	for (int i = 0; i < cBytesEmit; i++)
	{
		appendNew(&bcf->bytes);
	}

	u8 * pDst = &bcf->bytes[bcf->bytes.cItem - cBytesEmit];
	memcpy(pDst, pBytesEmit, cBytesEmit);
}

void emitByteRepeat(BytecodeFunction * bcf, u8 byteEmit, int cRepeat)
{
	for (int i = 0; i < cRepeat; i++)
	{
		emit(bcf, byteEmit);
	}
}

void backpatchJumpArg(BytecodeFunction * bcf, int iBytePatch, int ipZero, int ipTarget)
{
	if (ipTarget - ipZero > S16_MAX || ipTarget - ipZero < S16_MIN)
	{
		reportIceAndExit("Cannot store %d in jump argument");
	}

	s16 bytesNew = ipTarget - ipZero;
	backpatch(bcf, iBytePatch, &bytesNew, sizeof(bytesNew));
}

void backpatch(BytecodeFunction * bcf, int iBytePatch, void * pBytesNew, int cBytesNew)
{
	Assert(iBytePatch <= bcf->bytes.cItem - cBytesNew);

	u8 * pDst = &bcf->bytes[iBytePatch];
	memcpy(pDst, pBytesNew, cBytesNew);
}

bool visitBytecodeBuilderPreorder(AstNode * pNode, void * pBuilder_)
{
	Assert(pNode);
	Assert(!isErrorNode(*pNode));

	BytecodeBuilder * pBuilder = reinterpret_cast<BytecodeBuilder *>(pBuilder_);
	BytecodeFunction * pBytecodeFunc = pBuilder->pBytecodeFuncCompiling;
	MeekCtx * pCtx = pBuilder->pCtx;

	Assert(Implies(pBuilder->root, pNode->astk == ASTK_FuncDefnStmt || pNode->astk == ASTK_FuncLiteralExpr));

	int startLine = getStartLine(*pCtx, pNode->astid);

	BytecodeBuilder::NodeCtx * pNodeCtx = pushNew(&pBuilder->nodeCtxStack);
	init(pNodeCtx, pNode);

	// Override on case by case basis

	switch (pNode->astk)
	{
		case ASTK_UnopExpr:
		{
			// TODO: wantsAddress = true if address-of operator

			return true;
		} break;

		case ASTK_BinopExpr:
		case ASTK_LiteralExpr:
		case ASTK_GroupExpr:
			return true;

		case ASTK_SymbolExpr:
		{
			auto * pExpr = Down(pNode, SymbolExpr);
			if (pExpr->symbexprk == SYMBEXPRK_MemberVar)
			{
				pNodeCtx->wantsChildExprAddr = true;
			}
			return true;
		} break;

		case ASTK_PointerDereferenceExpr:
		case ASTK_ArrayAccessExpr:
		case ASTK_FuncCallExpr:
			return true;

		case ASTK_FuncLiteralExpr:
		{
			bool result = pBuilder->root;
			pBuilder->root = false;
			return result;
		}

		case ASTK_ExprStmt:
			return true;

		case ASTK_AssignStmt:
		{
			pNodeCtx->wantsChildExprAddr = true;
			return true;
		}

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = Down(pNode, VarDeclStmt);

			Scope * pScope = pCtx->mpScopeidPScope[pStmt->ident.scopeid];
			SymbolInfo symbInfo = lookupVarSymbol(*pScope, pStmt->ident.lexeme, FSYMBQ_IgnoreParent);
			Assert(symbInfo.symbolk == SYMBOLK_Var);

			uintptr virtualAddress = virtualAddressStart(*pScope) + symbInfo.varData.byteOffset;

			emitOp(pBytecodeFunc, BCOP_LoadImmediatePtr, startLine);
			emit(pBytecodeFunc, virtualAddress);

			return true;
		}

		case ASTK_FuncDefnStmt:
		{
			bool result = pBuilder->root;
			pBuilder->root = false;
			return result;
		}

		case ASTK_StructDefnStmt:
			return false;

		case ASTK_IfStmt:
			return true;

		case ASTK_WhileStmt:
		{
			pNodeCtx->whileStmtData.ipJumpToTopOfLoop = pBytecodeFunc->bytes.cItem;
			return true;
		}

		case ASTK_BlockStmt:
		{
			auto * pStmt = Down(pNode, BlockStmt);

			// HMM: Maybe this should only alloc/free if we don't inherit parent's scope id?

			Scope * pScope = pCtx->mpScopeidPScope[pStmt->scopeid];

			uintptr cByteLocal = cByteLocalVars(*pScope);
			if (cByteLocal > 0)
			{
				emitOp(pBytecodeFunc, BCOP_StackAlloc, startLine);
				emit(pBytecodeFunc, cByteLocal);
			}

			return true;
		}

		case ASTK_ReturnStmt:
		case ASTK_BreakStmt:
		case ASTK_ContinueStmt:
		case ASTK_PrintStmt:
		case ASTK_ParamsReturnsGrp:
			return true;

		case ASTK_Program:
			AssertNotReached;
			return false;

		default:
			AssertNotReached;
			return false;
	}
}

void visitBytecodeBuilderHook(AstNode * pNode, AWHK awhk, void * pBuilder_)
{
	Assert(pNode);
	Assert(!isErrorNode(*pNode));

	BytecodeBuilder * pBuilder = reinterpret_cast<BytecodeBuilder *>(pBuilder_);
	BytecodeFunction * pBytecodeFunc = pBuilder->pBytecodeFuncCompiling;
	MeekCtx * pCtx = pBuilder->pCtx;

	int startLine = getStartLine(*pCtx, pNode->astid);

	BytecodeBuilder::NodeCtx * pNodeCtx = peekPtr(pBuilder->nodeCtxStack);

	switch (awhk)
	{
		case AWHK_AssignPostLhs:
		{
			Assert(pNode->astk == ASTK_AssignStmt);

			auto * pStmt = Down(pNode, AssignStmt);
			TYPID typidLhs = DownExpr(pStmt->pLhsExpr)->typidEval;
			const Type * pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);

			pNodeCtx->wantsChildExprAddr = false;

			switch (pStmt->pAssignToken->tokenk)
			{
				case TOKENK_PlusEqual:
				case TOKENK_MinusEqual:
				case TOKENK_StarEqual:
				case TOKENK_SlashEqual:
				case TOKENK_PercentEqual:
				{
					int cBitSize = pTypeLhs->info.size * 8;
					int cBitPtr = MeekCtx::s_cBytePtr * 8;

					// TODO: I'm assuming the bytecode for the LHS put an address on the stack... is that
					//	a safe assumption to make?

					BCOP bcopDuplicate = bcopSized(SIZEDBCOP_Duplicate, cBitPtr);
					BCOP bcopLoad = bcopSized(SIZEDBCOP_Load, cBitSize);

					emitOp(pBytecodeFunc, bcopDuplicate, startLine);
					emitOp(pBytecodeFunc, bcopLoad, startLine);
				} break;
			}
		} break;

		case AWHK_IfPostCondition:
		{
			Assert(pNode->astk == ASTK_IfStmt);

			s16 placeholder = 0;
			emitOp(pBytecodeFunc, BCOP_JumpIfFalse, startLine);

			pNodeCtx->ifStmtData.iJumpArgPlaceholder = pBytecodeFunc->bytes.cItem;
			emit(pBytecodeFunc, placeholder);

			pNodeCtx->ifStmtData.ipZero = pBytecodeFunc->bytes.cItem;
		} break;

		case AWHK_IfPreElse:
		{
			Assert(pNode->astk == ASTK_IfStmt);

			auto * pStmt = Down(pNode, IfStmt);
			Assert(pStmt->pElseStmt);

			// Emit jump over else

			s16 placeholder = 0;
			emitOp(pBytecodeFunc, BCOP_Jump, startLine);	// Would be better if line mapped to the line of the "else" token

			int iJumpOverElseBackpatch = pBytecodeFunc->bytes.cItem;
			emit(pBytecodeFunc, placeholder);

			// Backpatch jump over if

			backpatchJumpArg(
				pBytecodeFunc,
				pNodeCtx->ifStmtData.iJumpArgPlaceholder,
				pNodeCtx->ifStmtData.ipZero,
				pBytecodeFunc->bytes.cItem);

			// Setup jump over else

			pNodeCtx->ifStmtData.iJumpArgPlaceholder = iJumpOverElseBackpatch;
			pNodeCtx->ifStmtData.ipZero = pBytecodeFunc->bytes.cItem;
		} break;

		case AWHK_WhilePostCondition:
		{
			Assert(pNode->astk == ASTK_WhileStmt);

			s16 placeholder = 0;
			emitOp(pBytecodeFunc, BCOP_JumpIfFalse, startLine);

			pNodeCtx->whileStmtData.iJumpPastLoopArgPlaceholder = pBytecodeFunc->bytes.cItem;
			emit(pBytecodeFunc, placeholder);

			pNodeCtx->whileStmtData.ipZeroJumpPastLoop = pBytecodeFunc->bytes.cItem;
		} break;

		case AWHK_BinopPostFirstOperand:
		{
			Assert(pNode->astk == ASTK_BinopExpr);

			auto * pExpr = Down(pNode, BinopExpr);

			// Set up jump for short circuit evaluation

			BCOP bcopJump = BCOP_Nil;
			switch (pExpr->pOp->tokenk)
			{
				case TOKENK_AmpAmp:
				{
					bcopJump = BCOP_JumpIfPeekFalse;
				} break;

				case TOKENK_PipePipe:
				{
					bcopJump = BCOP_JumpIfPeekTrue;
				} break;
			}

			if (bcopJump != BCOP_Nil)
			{
				emitOp(pBytecodeFunc, bcopJump, startLine);
				pNodeCtx->binopExprData.iJumpArgPlaceholder = pBytecodeFunc->bytes.cItem;

				s16 placeholder = 0;
				emit(pBytecodeFunc, placeholder);
				pNodeCtx->binopExprData.ipZero = pBytecodeFunc->bytes.cItem;

				// If we take the jump, we leave the eager result on the stack as the result of the entire
				//	expression. If we don't take the jump, we pop the first value off the stack and the
				//	second value (once evaluated) will become the result of the entire expression.

				uintptr bytesToPop = sizeof(bool);
				emitOp(pBytecodeFunc, BCOP_StackFree, startLine);
				emit(pBytecodeFunc, bytesToPop);
			}
		} break;
	}
}

void visitBytecodeBuilderPostOrder(AstNode * pNode, void * pBuilder_)
{	
	Assert(pNode);
	Assert(!isErrorNode(*pNode));

	BytecodeBuilder * pBuilder = reinterpret_cast<BytecodeBuilder *>(pBuilder_);
	BytecodeFunction * pBytecodeFunc = pBuilder->pBytecodeFuncCompiling;
	MeekCtx * pCtx = pBuilder->pCtx;

	Assert(count(pBuilder->nodeCtxStack) > 0);
	BytecodeBuilder::NodeCtx * pNodeCtx = peekPtr(pBuilder->nodeCtxStack);

	NULLABLE BytecodeBuilder::NodeCtx * pNodeCtxParent = nullptr;
	if (count(pBuilder->nodeCtxStack) > 1)
	{
		pNodeCtxParent = peekFarPtr(pBuilder->nodeCtxStack, 1);
	}

	int startLine = getStartLine(*pCtx, pNode->astid);

	switch (pNode->astk)
	{
		case ASTK_UnopExpr:
		{
			auto * pExpr = Down(pNode, UnopExpr);

			TYPID typid = UpExpr(pExpr)->typidEval;
			const Type * pType = lookupType(*pCtx->pTypeTable, typid);

			switch (pExpr->pOp->lexeme.strv.pCh[0])
			{
				case '+':		// No-op
					break;

				case '-':
				{
					// BB: write a more general version of bcopSized that works on float or int?

					switch (typid)
					{
						case TYPID_S8:		emitOp(pBytecodeFunc, BCOP_NegateS8, startLine); break;
						case TYPID_S16:		emitOp(pBytecodeFunc, BCOP_NegateS16, startLine); break;
						case TYPID_S32:		emitOp(pBytecodeFunc, BCOP_NegateS32, startLine); break;
						case TYPID_S64:		emitOp(pBytecodeFunc, BCOP_NegateS64, startLine); break;
						case TYPID_F32:		emitOp(pBytecodeFunc, BCOP_NegateFloat32, startLine); break;
						case TYPID_F64:		emitOp(pBytecodeFunc, BCOP_NegateFloat64, startLine); break;
						default:
							AssertNotReached;
					} break;
				} break;

				case '^':
				{
					Assert(isAddressable(pExpr->pExpr->astk));

					// TODO: I think we need a way of telling the interpreter which LValue we are taking the address of.
					//	Maybe each LValue in a scope has its own id? Note that we need to handle taking address of an
					//	index into an array...

					AssertTodo;
					// uintptr ptr = ???;
					// emit(paByte, Push ptr);
				} break;
			}

		} break;

		case ASTK_BinopExpr:
		{
			auto * pExpr = Down(pNode, BinopExpr);

			// Backpatch short circuit evaluation

			if (pExpr->pOp->tokenk == TOKENK_AmpAmp || pExpr->pOp->tokenk == TOKENK_PipePipe)
			{
				backpatchJumpArg(
					pBytecodeFunc,
					pNodeCtx->binopExprData.iJumpArgPlaceholder,
					pNodeCtx->binopExprData.ipZero,
					pBytecodeFunc->bytes.cItem);
			}

			// Determine if widening is needed

			TYPID typidLhs = DownExpr(pExpr->pLhsExpr)->typidEval;
			TYPID typidRhs = DownExpr(pExpr->pRhsExpr)->typidEval;
			const Type * pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);
			const Type * pTypeRhs = lookupType(*pCtx->pTypeTable, typidRhs);

			if (typidLhs != typidRhs)
			{
				AssertTodo;
			}

			const Type * pTypeBig = (pTypeLhs->info.size > pTypeRhs->info.size) ? pTypeLhs : pTypeRhs;
			const Type * pTypeSmall = (pTypeLhs->info.size <= pTypeRhs->info.size) ? pTypeLhs : pTypeRhs;

			bool widenLhs = false;
			bool widenRhs = false;
			if (pTypeBig != pTypeSmall)
			{
				if (pTypeLhs != pTypeBig)
				{
					Assert(pTypeLhs == pTypeSmall);
					Assert(pTypeRhs == pTypeBig);

					widenLhs = true;
				}
				else
				{
					Assert(pTypeLhs == pTypeBig);
					Assert(pTypeRhs == pTypeSmall);

					widenRhs = true;
				}
			}

			Assert(!(widenLhs && widenRhs));

			if (widenRhs)
			{
				AssertTodo;
			}
			else if (widenLhs)
			{
				AssertTodo;
			}

			// TODO: Emit float operations, unsigned vs signed div operations, etc.

			bool shouldEmitNotAtEnd = false;

			SIZEDBCOP sizedbcop = SIZEDBCOP_Nil;
			switch (pExpr->pOp->tokenk)
			{
				case TOKENK_Plus:
				{
					sizedbcop = SIZEDBCOP_AddInt;
				} break;

				case TOKENK_Minus:
				{
					sizedbcop = SIZEDBCOP_SubInt;
				} break;

				case TOKENK_Star:
				{
					sizedbcop = SIZEDBCOP_MulInt;
				} break;

				case TOKENK_Slash:
				{
					sizedbcop = SIZEDBCOP_DivSignedInt;
				} break;

				case TOKENK_EqualEqual:
				{
					sizedbcop = SIZEDBCOP_TestEqInt;
				} break;

				case TOKENK_BangEqual:
				{
					sizedbcop = SIZEDBCOP_TestEqInt;
					shouldEmitNotAtEnd = true;
				} break;

				case TOKENK_Lesser:
				{
					sizedbcop = SIZEDBCOP_TestLtSignedInt;
				} break;

				case TOKENK_LesserEqual:
				{
					sizedbcop = SIZEDBCOP_TestLteSignedInt;
				} break;

				case TOKENK_Greater:
				{
					sizedbcop = SIZEDBCOP_TestLteSignedInt;
					shouldEmitNotAtEnd = true;
				} break;

				case TOKENK_GreaterEqual:
				{
					sizedbcop = SIZEDBCOP_TestLtSignedInt;
					shouldEmitNotAtEnd = true;
				} break;

				case TOKENK_AmpAmp:
				case TOKENK_PipePipe:
				{
					// Simply leave the value on the stack. The actual logic is handled by the
					//	short-circuit jump that we already emitted.

					Assert(typidLhs == TYPID_Bool);
					Assert(typidRhs == TYPID_Bool);
				} break;

				default:
					AssertNotReached;
					break;
			}

			if (sizedbcop != SIZEDBCOP_Nil)
			{
				int cBitSize = pTypeBig->info.size * 8;
				if (cBitSize != 32) AssertTodo;

				BCOP bcop = bcopSized(sizedbcop, cBitSize);
				emitOp(pBytecodeFunc, bcop, startLine);

				if (shouldEmitNotAtEnd)
				{
					emitOp(pBytecodeFunc, BCOP_Not, startLine);
				}
			}
		} break;

		case ASTK_LiteralExpr:
		{
			Assert(pNodeCtxParent);
			Assert(!pNodeCtxParent->wantsChildExprAddr);

			auto * pExpr = Down(pNode, LiteralExpr);

			// TODO: Literals should be untyped and slot into whatever type they must, like in Go.
			// TODO: Should we peek at our parent and widen here if need be? I need to figure out who is
			//	responsible for emitting the bytecode to cast narrow values to wider ones.

			switch (pExpr->literalk)
			{
				case LITERALK_Int:
				{
					emitOp(pBytecodeFunc, BCOP_LoadImmediate32, startLine);
					emit(pBytecodeFunc, intValue(pExpr));
				} break;

				case LITERALK_Float:
				{
					emitOp(pBytecodeFunc, BCOP_LoadImmediate32, startLine);
					emit(pBytecodeFunc, floatValue(pExpr));
				} break;

				case LITERALK_Bool:
				{
					if (boolValue(pExpr))
					{
						emitOp(pBytecodeFunc, BCOP_LoadTrue, startLine);
					}
					else
					{
						emitOp(pBytecodeFunc, BCOP_LoadFalse, startLine);
					}
				} break;

				case LITERALK_String:
				{
					AssertTodo;
				} break;
			}
		} break;

		case ASTK_GroupExpr:
			break;

		case ASTK_SymbolExpr:
		{
			auto * pExpr = Down(pNode, SymbolExpr);

			switch (pExpr->symbexprk)
			{
				case SYMBEXPRK_Var:
				{
					Assert(pNodeCtxParent);

					Scope * pScope = pCtx->mpScopeidPScope[pExpr->varData.pDeclCached->ident.scopeid];
					SymbolInfo symbInfo = lookupVarSymbol(*pScope, pExpr->ident, FSYMBQ_IgnoreParent);
					Assert(symbInfo.symbolk == SYMBOLK_Var);

					uintptr virtualAddress = virtualAddressStart(*pScope) + symbInfo.varData.byteOffset;

					emitOp(pBytecodeFunc, BCOP_LoadImmediatePtr, startLine);
					emit(pBytecodeFunc, virtualAddress);

					if (!pNodeCtxParent->wantsChildExprAddr)
					{
						TYPID typid = DownExpr(pNode)->typidEval;
						const Type * pType = lookupType(*pCtx->pTypeTable, typid);

						int cBitSize = pType->info.size * 8;

						AssertInfo(
							cBitSize == 8 || cBitSize == 16 || cBitSize == 32 || cBitSize == 64,
							"not wanting address is not valid for non-primitive types");

						BCOP bcop = bcopSized(SIZEDBCOP_Load, cBitSize);
						emitOp(pBytecodeFunc, bcop, startLine);
					}
				} break;

				case SYMBEXPRK_MemberVar:
				{
					AssertTodo;

					//Assert(pCtx->mpScopeidPScope[pExpr->memberData.pDeclCached->ident.scopeid]->scopek == SCOPEK_StructDefn);

					//TYPID typid = DownExpr(pNode)->typidEval;

					//const Type * pType = lookupType(*pCtx->pTypeTable, typid);
					//BCOP bcop = BCOP_Nil;

					//if (pBuilder->parentwantsAddress)
					//{
					//	bcop = BCOP_AddToAddr;
					//}
					//else
					//{

					//	// AddToAddr and dereference to pType->info.cByte... yuck

					//	StaticAssertTodo;
					//	int cBitSize = pType->info.size * 8;
					//	bcop = (pScope->id == SCOPEID_Global) ?
					//		bcopSized(SIZEDBCOP_PushGlobalValue, cBitSize) :
					//		bcopSized(SIZEDBCOP_PushLocalValue, cBitSize);
					//}

					//emit(paByte, bcop);

					//SymbolInfo symbInfo = lookupVarSymbol(*pScope, pExpr->ident, FSYMBQ_IgnoreParent);
					//emit(paByte, symbInfo.varData.byteOffset);
				} break;

				case SYMBEXPRK_Func:
				{
					// Do nothing -- func call expr is responsible for this case, since there is nothing for us to
					//	compute and put on the stack.

					AssertTodo;		// I think I don't need to do anything here, but keeping this assert until I actually run a purposeful test.
				} break;
			}
		} break;

		case ASTK_PointerDereferenceExpr:
		case ASTK_ArrayAccessExpr:
		case ASTK_FuncCallExpr:
			break;

		case ASTK_FuncLiteralExpr:
			AssertTodo;
			break;

		case ASTK_ExprStmt:
			break;

		case ASTK_AssignStmt:
		{
			auto * pStmt = Down(pNode, AssignStmt);

			TYPID typidLhs = DownExpr(pStmt->pLhsExpr)->typidEval;
			TYPID typidRhs = DownExpr(pStmt->pRhsExpr)->typidEval;
			const Type * pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);
			const Type * pTypeRhs = lookupType(*pCtx->pTypeTable, typidRhs);

			switch (pStmt->pAssignToken->tokenk)
			{
				case TOKENK_PlusEqual:
				case TOKENK_MinusEqual:
				case TOKENK_StarEqual:
				case TOKENK_SlashEqual:
				case TOKENK_PercentEqual:
				{
					// @CopyPaste - mostly identical to binop expr. Factor out into common function?

					if (typidLhs != typidRhs)
					{
						AssertTodo;
					}

					if (typidLhs != TYPID_S32)
					{
						AssertTodo;
					}

					Assert(pTypeLhs->info.size >= pTypeRhs->info.size);

					bool widenRhs = false;
					if (pTypeLhs->info.size > pTypeRhs->info.size)
					{
						widenRhs = true;
					}

					if (widenRhs)
					{
						AssertTodo;
					}

					// TODO: Emit float operations, unsigned vs signed div operations, etc.

					SIZEDBCOP sizedbcop = SIZEDBCOP_Nil;
					switch (pStmt->pAssignToken->tokenk)
					{
						case TOKENK_PlusEqual:
						{
							sizedbcop = SIZEDBCOP_AddInt;
						} break;

						case TOKENK_MinusEqual:
						{
							sizedbcop = SIZEDBCOP_SubInt;
						} break;

						case TOKENK_StarEqual:
						{
							sizedbcop = SIZEDBCOP_MulInt;
						} break;

						case TOKENK_SlashEqual:
						{
							sizedbcop = SIZEDBCOP_DivSignedInt;
						} break;

						case TOKENK_PercentEqual:
						{
							AssertTodo;
						} break;

						default:
							AssertNotReached;
							break;
					}

					int cBitSize = pTypeLhs->info.size * 8;
					if (cBitSize != 32) AssertTodo;

					BCOP bcop = bcopSized(sizedbcop, cBitSize);
					emitOp(pBytecodeFunc, bcop, startLine);

					typidRhs = typidLhs;
					pTypeRhs = pTypeLhs;

				} // fallthrough

				case TOKENK_Equal:
				{
					const Type * pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);
					int cBitSize = pTypeLhs->info.size * 8;

					if (cBitSize != 32) AssertTodo;

					BCOP bcop = bcopSized(SIZEDBCOP_Store, cBitSize);
					emitOp(pBytecodeFunc, bcop, startLine);
				} break;
			}
		} break;

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = Down(pNode, VarDeclStmt);

			Scope * pScope = pCtx->mpScopeidPScope[pStmt->ident.scopeid];
			SymbolInfo symbInfo = lookupVarSymbol(*pScope, pStmt->ident.lexeme, FSYMBQ_IgnoreParent);
			Assert(symbInfo.symbolk == SYMBOLK_Var);

			TYPID typid = pStmt->typidDefn;
			const Type * pType = lookupType(*pCtx->pTypeTable, typid);
			int cByteSize = pType->info.size;
			int cBitSize = cByteSize * 8;

			uintptr virtualAddress = virtualAddressStart(*pScope) + symbInfo.varData.byteOffset;
			if (!pStmt->pInitExpr)
			{
				emitOp(
					pBytecodeFunc,
					bcopSized(SIZEDBCOP_LoadImmediate, cBitSize),
					startLine);

				emitByteRepeat(pBytecodeFunc, 0, cByteSize);
			}

			emitOp(
				pBytecodeFunc,
				bcopSized(SIZEDBCOP_Store, cBitSize),
				startLine);
		} break;

		case ASTK_FuncDefnStmt:
		{
			// Temporary for testing... in reality we will emit return code here (if necessary)

			emitOp(pBytecodeFunc, BCOP_DebugExit, startLine);
		}

		case ASTK_StructDefnStmt:
			break;

		case ASTK_IfStmt:
		{
			auto * pStmt = Down(pNode, IfStmt);

			// Backpatch jump over if (or else)

			backpatchJumpArg(
				pBytecodeFunc,
				pNodeCtx->ifStmtData.iJumpArgPlaceholder,
				pNodeCtx->ifStmtData.ipZero,
				pBytecodeFunc->bytes.cItem);
		} break;

		case ASTK_WhileStmt:
		{
			auto * pStmt = Down(pNode, WhileStmt);

			// Emit jump back to top of loop
			// Placeholder/backpatch isn't strictly necessary here, but backpatchJumpArg handles the math/validation

			s16 placeholder = 0;
			emitOp(pBytecodeFunc, BCOP_Jump, startLine);

			int iPlaceholder = pBytecodeFunc->bytes.cItem;
			emit(pBytecodeFunc, placeholder);

			int ipZero = pBytecodeFunc->bytes.cItem;
			backpatchJumpArg(
				pBytecodeFunc,
				iPlaceholder,
				ipZero,
				pNodeCtx->whileStmtData.ipJumpToTopOfLoop
			);

			// Backpatch jump over loop

			backpatchJumpArg(
				pBytecodeFunc,
				pNodeCtx->whileStmtData.iJumpPastLoopArgPlaceholder,
				pNodeCtx->whileStmtData.ipZeroJumpPastLoop,
				pBytecodeFunc->bytes.cItem
			);
		} break;

		case ASTK_BlockStmt:
		{
			auto * pStmt = Down(pNode, BlockStmt);

			// HMM: Maybe this should only alloc/free if we don't inherit parent's scope id?

			Scope * pScope = pCtx->mpScopeidPScope[pStmt->scopeid];

			uintptr cByteLocal = cByteLocalVars(*pScope);
			if (cByteLocal > 0)
			{
				emitOp(pBytecodeFunc, BCOP_StackFree, startLine);
				emit(pBytecodeFunc, cByteLocal);
			}
		} break;

		case ASTK_ReturnStmt:
		case ASTK_BreakStmt:
		case ASTK_ContinueStmt:
			break;

		case ASTK_PrintStmt:
		{
			auto * pStmt = Down(pNode, PrintStmt);
			TYPID typid = DownExpr(pStmt->pExpr)->typidEval;

			emitOp(pBytecodeFunc, BCOP_DebugPrint, startLine);
			emit(pBytecodeFunc, typid);
		} break;

		case ASTK_ParamsReturnsGrp:
			break;

		case ASTK_Program:
			AssertNotReached;
			break;

		default:
			AssertNotReached;
			break;
	}

	pop(&pBuilder->nodeCtxStack);
}

#ifdef DEBUG
void disassemble(const BytecodeFunction & bcf)
{
	int byteOffset = 0;
	int linePrev = -1;
	int iOp = 0;
	
	while (byteOffset < bcf.bytes.cItem)
	{
		printfmt("%08d ", byteOffset);

		u8 bcop = bcf.bytes[byteOffset];
		byteOffset++;

		Assert(bcop < BCOP_Max);
		AssertInfo(iOp < bcf.sourceLineNumbers.cItem, "Mismatch between # of ops and # of line numbers. Did we use emit instead of emitOp?");
		int line = bcf.sourceLineNumbers[iOp];

		if (line == linePrev)
		{
			print("     |  ");
		}
		else
		{
			printfmt("%6d  ", line);
		}

		print(c_mpBcopStrName[bcop]);
		println();

		// TODO: Document args for each op in a data-driven way... would eliminate the need for this switch statement.

		switch (bcop)
		{
			case BCOP_Return:
				break;

			case BCOP_LoadImmediate8:
			{
				printfmt("%08d ", byteOffset);

				u8 value = bcf.bytes[byteOffset];
				byteOffset += 1;

				print("     |  ");
				print(" -> ");
				printfmt("%#x", value);
				println();
			} break;

			case BCOP_LoadImmediate16:
			{
				printfmt("%08d ", byteOffset);

				u16 value = *reinterpret_cast<u16 *>(bcf.bytes.pBuffer + byteOffset);
				byteOffset += sizeof(u16);

				print("     |  ");
				print(" -> ");
				printfmt("%#x", value);
				println();
			} break;

			case BCOP_LoadImmediate32:
			{
				printfmt("%08d ", byteOffset);

				u32 value = *reinterpret_cast<u32 *>(bcf.bytes.pBuffer + byteOffset);
				byteOffset += sizeof(u32);

				print("     |  ");
				print(" -> ");
				printfmt("%#x", value);
				println();
			} break;

			case BCOP_LoadImmediate64:
			{
				printfmt("%08d ", byteOffset);

				u64 value = *reinterpret_cast<u64 *>(bcf.bytes.pBuffer + byteOffset);
				byteOffset += sizeof(u64);

				print("     |  ");
				print(" -> ");
				printfmt("%#x", value);
				println();
			} break;

			case BCOP_LoadTrue:
			case BCOP_LoadFalse:
			case BCOP_Load8:
			case BCOP_Load16:
			case BCOP_Load32:
			case BCOP_Load64:
			case BCOP_Store8:
			case BCOP_Store16:
			case BCOP_Store32:
			case BCOP_Store64:
			case BCOP_Duplicate8:
			case BCOP_Duplicate16:
			case BCOP_Duplicate32:
			case BCOP_Duplicate64:
			case BCOP_AddInt8:
			case BCOP_AddInt16:
			case BCOP_AddInt32:
			case BCOP_AddInt64:
			case BCOP_SubInt8:
			case BCOP_SubInt16:
			case BCOP_SubInt32:
			case BCOP_SubInt64:
			case BCOP_MulInt8:
			case BCOP_MulInt16:
			case BCOP_MulInt32:
			case BCOP_MulInt64:
			case BCOP_DivS8:
			case BCOP_DivS16:
			case BCOP_DivS32:
			case BCOP_DivS64:
			case BCOP_DivU8:
			case BCOP_DivU16:
			case BCOP_DivU32:
			case BCOP_DivU64:
			case BCOP_AddFloat32:
			case BCOP_AddFloat64:
			case BCOP_SubFloat32:
			case BCOP_SubFloat64:
			case BCOP_MulFloat32:
			case BCOP_MulFloat64:
			case BCOP_DivFloat32:
			case BCOP_DivFloat64:
			case BCOP_TestEqInt8:
			case BCOP_TestEqInt16:
			case BCOP_TestEqInt32:
			case BCOP_TestEqInt64:
			case BCOP_TestLtS8:
			case BCOP_TestLtS16:
			case BCOP_TestLtS32:
			case BCOP_TestLtS64:
			case BCOP_TestLtU8:
			case BCOP_TestLtU16:
			case BCOP_TestLtU32:
			case BCOP_TestLtU64:
			case BCOP_TestLteS8:
			case BCOP_TestLteS16:
			case BCOP_TestLteS32:
			case BCOP_TestLteS64:
			case BCOP_TestLteU8:
			case BCOP_TestLteU16:
			case BCOP_TestLteU32:
			case BCOP_TestLteU64:
			case BCOP_TestEqFloat32:
			case BCOP_TestEqFloat64:
			case BCOP_TestLtFloat32:
			case BCOP_TestLtFloat64:
			case BCOP_TestLteFloat32:
			case BCOP_TestLteFloat64:
			case BCOP_Not:
			case BCOP_NegateS8:
			case BCOP_NegateS16:
			case BCOP_NegateS32:
			case BCOP_NegateS64:
			case BCOP_NegateFloat32:
			case BCOP_NegateFloat64:
				break;

			case BCOP_Jump:
			case BCOP_JumpIfFalse:
			case BCOP_JumpIfPeekFalse:
			case BCOP_JumpIfPeekTrue:
			{
				printfmt("%08d ", byteOffset);

				s16 bytesToJump = *reinterpret_cast<s16 *>(bcf.bytes.pBuffer + byteOffset);
				byteOffset += sizeof(s16);

				print("     |  ");
				print(" -> ");
				printfmt("%d", bytesToJump);
				println();
			} break;

			case BCOP_StackAlloc:
			case BCOP_StackFree:
			{
				printfmt("%08d ", byteOffset);

				uintptr cByte = *reinterpret_cast<uintptr *>(bcf.bytes.pBuffer + byteOffset);
				byteOffset += sizeof(uintptr);

				print("     |  ");
				print(" -> ");
				printfmt("%" PRIuPTR, cByte);
				println();
			} break;

			case BCOP_DebugPrint:
			{
				printfmt("%08d ", byteOffset);

				TYPID value = *reinterpret_cast<TYPID *>(bcf.bytes.pBuffer + byteOffset);
				byteOffset += sizeof(TYPID);

				print("     |  ");
				print(" -> ");
				printfmt("%#x", value);
				println();
			} break;

			case BCOP_DebugExit:
				break;

			default:
				AssertNotReached;
				break;
		}

		linePrev = line;
		iOp++;
	}

	AssertInfo(iOp == bcf.sourceLineNumbers.cItem, "Mismatch between # of ops and # of line numbers. Did we use emit instead of emitOp?");
}
#endif
