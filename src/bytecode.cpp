#include "bytecode.h"

#include "ast.h"
#include "error.h"
#include "global_context.h"
#include "interp.h"
#include "print.h"

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
	"Return",
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
	"NegateS8",
	"NegateS16",
	"NegateS32",
	"NegateS64",
	"NegateFloat32",
	"NegateFloat64",
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

		default:
							AssertNotReached;		return BCOP_Nil;
	}
}

void init(BytecodeFunction * pBcf)
{
	init(&pBcf->bytes);
	init(&pBcf->mpIByteIStart);
}

void dispose(BytecodeFunction * pBcf)
{
	dispose(&pBcf->bytes);
	dispose(&pBcf->mpIByteIStart);
}

void init(BytecodeBuilder * pBuilder, MeekCtx * pCtx)
{
	pBuilder->pCtx = pCtx;
	init(&pBuilder->aBytecodeFunc);
}

void dispose(BytecodeBuilder * pBuilder)
{
	dispose(&pBuilder->aBytecodeFunc);
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

void emit(DynamicArray<u8> * pBytes, BCOP byteEmit)
{
	StaticAssert(sizeof(BCOP) == sizeof(u8));

	append(pBytes, u8(byteEmit));
}

void emit(DynamicArray<u8> * pBytes, u8 byteEmit)
{
	append(pBytes, byteEmit);
}

void emit(DynamicArray<u8> * pBytes, s8 byteEmit)
{
	u8 * pDst = appendNew(pBytes);
	memcpy(pDst, &byteEmit, 1);
}

void emit(DynamicArray<u8> * pBytes, u16 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, s16 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, u32 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, s32 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, u64 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, s64 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, f32 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, f64 bytesEmit)
{
	emit(pBytes, &bytesEmit, sizeof(bytesEmit));
}

void emit(DynamicArray<u8> * pBytes, void * pBytesEmit, int cBytesEmit)
{
	for (int i = 0; i < cBytesEmit; i++)
	{
		appendNew(pBytes);
	}

	u8 * pDst = &(*pBytes)[pBytes->cItem - cBytesEmit];
	memcpy(pDst, pBytesEmit, cBytesEmit);
}

bool visitBytecodeBuilderPreorder(AstNode * pNode, void * pBuilder_)
{
	Assert(pNode);
	Assert(!isErrorNode(*pNode));

	BytecodeBuilder * pBuilder = reinterpret_cast<BytecodeBuilder *>(pBuilder_);
	MeekCtx * pCtx = pBuilder->pCtx;
	DynamicArray<u8> * paByte = &pBuilder->pBytecodeFuncCompiling->bytes;

	Assert(Implies(pBuilder->root, pNode->astk == ASTK_FuncDefnStmt || pNode->astk == ASTK_FuncLiteralExpr));

	// Set defaults

	pBuilder->wantsAddress = false;

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
				pBuilder->wantsAddress = true;
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
			pBuilder->wantsAddress = true;
			return true;
		}

		case ASTK_VarDeclStmt:
		{
			auto * pStmt = Down(pNode, VarDeclStmt);

			Scope * pScope = pCtx->mpScopeidPScope[pStmt->ident.scopeid];
			SymbolInfo symbInfo = lookupVarSymbol(*pScope, pStmt->ident.lexeme, FSYMBQ_IgnoreParent);
			Assert(symbInfo.symbolk == SYMBOLK_Var);

			uintptr virtualAddress = virtualAddressStart(*pScope) + symbInfo.varData.byteOffset;

			emit(paByte, BCOP_LoadImmediatePtr);
			emit(paByte, virtualAddress);

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
		case ASTK_WhileStmt:
		case ASTK_BlockStmt:
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
	MeekCtx * pCtx = pBuilder->pCtx;
	DynamicArray<u8> * paByte = &pBuilder->pBytecodeFuncCompiling->bytes;

	switch (awhk)
	{
		case AWHK_PostAssignLhs:
		{
			Assert(pNode->astk == ASTK_AssignStmt);
			
			auto * pStmt = Down(pNode, AssignStmt);
			TYPID typidLhs = DownExpr(pStmt->pLhsExpr)->typidEval;
			const Type * pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);

			switch (pStmt->pAssignToken->tokenk)
			{
				case TOKENK_PlusEqual:
				case TOKENK_MinusEqual:
				case TOKENK_StarEqual:
				case TOKENK_SlashEqual:
				case TOKENK_PercentEqual:
				{
					int cBitSize = pTypeLhs->info.size * 8;

					BCOP bcopDuplicate = bcopSized(SIZEDBCOP_Duplicate, cBitSize);
					BCOP bcopLoad = bcopSized(SIZEDBCOP_Load, cBitSize);

					emit(paByte, bcopDuplicate);
					emit(paByte, bcopLoad);
				} break;
			}
		} break;
	}
}

void visitBytecodeBuilderPostOrder(AstNode * pNode, void * pBuilder_)
{	
	Assert(pNode);
	Assert(!isErrorNode(*pNode));

	BytecodeBuilder * pBuilder = reinterpret_cast<BytecodeBuilder *>(pBuilder_);
	MeekCtx * pCtx = pBuilder->pCtx;
	DynamicArray<u8> * paByte = &pBuilder->pBytecodeFuncCompiling->bytes;

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
						case TYPID_S8:		emit(paByte, BCOP_NegateS8); break;
						case TYPID_S16:		emit(paByte, BCOP_NegateS16); break;
						case TYPID_S32:		emit(paByte, BCOP_NegateS32); break;
						case TYPID_S64:		emit(paByte, BCOP_NegateS64); break;
						case TYPID_F32:		emit(paByte, BCOP_NegateFloat32); break;
						case TYPID_F64:		emit(paByte, BCOP_NegateFloat64); break;
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

			const Type * pTypeLhs = nullptr;
			const Type * pTypeRhs = nullptr;

			{
				TYPID typidLhs = DownExpr(pExpr->pLhsExpr)->typidEval;
				TYPID typidRhs = DownExpr(pExpr->pRhsExpr)->typidEval;
				pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);
				pTypeRhs = lookupType(*pCtx->pTypeTable, typidRhs);

				if (typidLhs != typidRhs)
				{
					AssertTodo;
				}

				if (typidLhs != TYPID_S32)
				{
					AssertTodo;
				}
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

			SIZEDBCOP sizedbcop = SIZEDBCOP_Nil;
			switch (pExpr->pOp->lexeme.strv.pCh[0])
			{
				case '+':
				{
					sizedbcop = SIZEDBCOP_AddInt;
				} break;

				case '-':
				{
					sizedbcop = SIZEDBCOP_SubInt;
				} break;

				case '*':
				{
					sizedbcop = SIZEDBCOP_MulInt;
				} break;

				case '/':
				{
					sizedbcop = SIZEDBCOP_DivSignedInt;
				} break;

				default:
					AssertNotReached;
					break;
			}

			int cBitSize = pTypeBig->info.size * 8;
			if (cBitSize != 32) AssertTodo;

			BCOP bcop = bcopSized(sizedbcop, cBitSize);
			emit(paByte, bcop);
		} break;

		case ASTK_LiteralExpr:
		{
			Assert(!pBuilder->wantsAddress);

			auto * pExpr = Down(pNode, LiteralExpr);

			// TODO: Literals should be untyped and slot into whatever type they must, like in Go.
			// TODO: Should we peek at our parent and widen here if need be? I need to figure out who is
			//	responsible for emitting the bytecode to cast narrow values to wider ones.

			switch (pExpr->literalk)
			{
				case LITERALK_Int:
				{
					emit(paByte, BCOP_LoadImmediate32);
					emit(paByte, intValue(pExpr));
				} break;

				case LITERALK_Float:
				{
					emit(paByte, BCOP_LoadImmediate32);
					emit(paByte, floatValue(pExpr));
				} break;

				case LITERALK_Bool:
				{
					if (boolValue(pExpr))
					{
						emit(paByte, BCOP_LoadTrue);
					}
					else
					{
						emit(paByte, BCOP_LoadFalse);
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
					Scope * pScope = pCtx->mpScopeidPScope[pExpr->varData.pDeclCached->ident.scopeid];
					SymbolInfo symbInfo = lookupVarSymbol(*pScope, pExpr->ident, FSYMBQ_IgnoreParent);
					Assert(symbInfo.symbolk == SYMBOLK_Var);

					uintptr virtualAddress = virtualAddressStart(*pScope) + symbInfo.varData.byteOffset;

					emit(paByte, BCOP_LoadImmediatePtr);
					emit(paByte, virtualAddress);

					if (!pBuilder->wantsAddress)
					{
						TYPID typid = DownExpr(pNode)->typidEval;
						const Type * pType = lookupType(*pCtx->pTypeTable, typid);

						int cBitSize = pType->info.size * 8;

						AssertInfo(
							cBitSize == 8 || cBitSize == 16 || cBitSize == 32 || cBitSize == 64,
							"wantsAddress is not valid for non-primitive types");

						BCOP bcop = bcopSized(SIZEDBCOP_Load, cBitSize);
						emit(paByte, bcop);
					}
				} break;

				case SYMBEXPRK_MemberVar:
				{
					AssertTodo;

					//Assert(pCtx->mpScopeidPScope[pExpr->memberData.pDeclCached->ident.scopeid]->scopek == SCOPEK_StructDefn);

					//TYPID typid = DownExpr(pNode)->typidEval;

					//const Type * pType = lookupType(*pCtx->pTypeTable, typid);
					//BCOP bcop = BCOP_Nil;

					//if (pBuilder->wantsAddress)
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
					emit(paByte, bcop);

					typidRhs = typidLhs;
					pTypeRhs = pTypeLhs;

				} // fallthrough

				case TOKENK_Equal:
				{
					const Type * pTypeLhs = lookupType(*pCtx->pTypeTable, typidLhs);
					int cBitSize = pTypeLhs->info.size * 8;

					if (cBitSize != 32) AssertTodo;

					BCOP bcop = bcopSized(SIZEDBCOP_Store, cBitSize);
					emit(paByte, bcop);
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

			if (typid != TYPID_S32) AssertTodo;

			uintptr virtualAddress = virtualAddressStart(*pScope) + symbInfo.varData.byteOffset;

			if (!pStmt->pInitExpr)
			{
				emit(paByte, BCOP_LoadImmediate32);
				emit(paByte, u32(0));
			}

			emit(paByte, BCOP_Load32);
		} break;

		case ASTK_FuncDefnStmt:
		{
			// Temporary for testing... in reality we will emit return code here (if necessary)

			emit(paByte, BCOP_DebugExit);
		}

		case ASTK_StructDefnStmt:
		case ASTK_IfStmt:
		case ASTK_WhileStmt:
		case ASTK_BlockStmt:
		case ASTK_ReturnStmt:
		case ASTK_BreakStmt:
		case ASTK_ContinueStmt:
			break;

		case ASTK_PrintStmt:
		{
			auto * pStmt = Down(pNode, PrintStmt);
			TYPID typid = DownExpr(pStmt->pExpr)->typidEval;

			emit(paByte, BCOP_DebugPrint);
			emit(paByte, typid);
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
}

#ifdef DEBUG
void disassemble(const BytecodeFunction & bcf)
{
	int byteOffset = 0;
	int linePrev = -1;
	
	while (byteOffset < bcf.bytes.cItem)
	{
		printfmt("%08d ", byteOffset);
		
		u8 bcop = bcf.bytes[byteOffset];
		// int iStart = bcf.mpIByteIStart[byteOffset];
		byteOffset++;

		int line = 1;	// TODO: compute from iStart

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
			case BCOP_NegateS8:
			case BCOP_NegateS16:
			case BCOP_NegateS32:
			case BCOP_NegateS64:
			case BCOP_NegateFloat32:
			case BCOP_NegateFloat64:
				break;

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
	}
}
#endif
