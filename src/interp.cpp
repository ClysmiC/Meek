#include "interp.h"

#include "bytecode.h"
#include "global_context.h"
#include "parse.h"
#include "print.h"
#include "symbol.h"

#include <inttypes.h>

void init(Interpreter * pInterp, MeekCtx * pCtx)
{
	constexpr int c_MiB = 1024 * 1024;

	Scope * pScopeGlobal = pCtx->parser->pScopeGlobal;		// TODO: put this somewhere other than parser...

	constexpr u64 cByteStack = c_MiB;
	uintptr cByteGlobal = pScopeGlobal->globalData.cByteGlobalVariable;

	pInterp->pVirtualAddressSpace = new u8[cByteGlobal + cByteStack];

	pInterp->pGlobals = pInterp->pVirtualAddressSpace;

	pInterp->pStackBase = pInterp->pVirtualAddressSpace + cByteGlobal;
	pInterp->pStack = pInterp->pStackBase;
	pInterp->pStackFrame = pInterp->pStackBase;

	// NOTE: We set this in interpret(..) ... might make more sense to set it here?

	pInterp->ip = nullptr;
}

void dispose(Interpreter * pInterp)
{
	delete[] pInterp->pVirtualAddressSpace;

	pInterp->pStackBase = nullptr;
	pInterp->pStack = nullptr;
	pInterp->pStackFrame = nullptr;
}

void interpret(Interpreter * pInterp, const BytecodeProgram & bcp, int iByteIpStart)
{
	pInterp->ip = bcp.bytes.pBuffer + iByteIpStart;

#define ReadVarFromBytecode(type, var) \
	do { \
		memcpy(&var, pInterp->ip, sizeof(type)); \
		pInterp->ip += sizeof(type); } while (0)

#define WriteBytecodeBytesToStack(type) \
	do { \
		memcpy(pInterp->pStack, pInterp->ip, sizeof(type)); \
		pInterp->ip += sizeof(type); \
		pInterp->pStack += sizeof(type); } while(0)

#define WriteVarToStack(type, var) \
	do { \
		memcpy(pInterp->pStack, &var, sizeof(type)); \
		pInterp->pStack += sizeof(type); } while (0)

#define ReadVarFromStack(type, var) \
	do { \
		pInterp->pStack = pInterp->pStack - sizeof(type); \
		memcpy(&var, pInterp->pStack, sizeof(type)); } while (0)

#define PeekVarFromStack(type, var) \
	do { \
		memcpy(&var, pInterp->pStack - sizeof(type), sizeof(type)); } while (0)


	bool run = true;
	while (run)
	{
		BCOP bcop = BCOP(*pInterp->ip);
		pInterp->ip++;

		switch (bcop)
		{
			case BCOP_LoadImmediate8:
			{
				WriteBytecodeBytesToStack(u8);
			} break;

			case BCOP_LoadImmediate16:
			{
				WriteBytecodeBytesToStack(u16);
			} break;

			case BCOP_LoadImmediate32:
			{
				WriteBytecodeBytesToStack(u32);
			} break;

			case BCOP_LoadImmediate64:
			{
				WriteBytecodeBytesToStack(u64);
			} break;

			case BCOP_LoadTrue:
			{
				*pInterp->pStack = true;
				pInterp->pStack++;
			} break;

			case BCOP_LoadFalse:
			{
				*pInterp->pStack = false;
				pInterp->pStack++;
			} break;


#define Load(type) \
	do { \
		uintptr _virtAddr; \
		ReadVarFromStack(uintptr, _virtAddr); \
		type _value = *reinterpret_cast<type *>(pInterp->pVirtualAddressSpace + _virtAddr); \
		WriteVarToStack(type, _value); } while(0)

			case BCOP_Load8:
			{
				Load(u8);
			} break;

			case BCOP_Load16:
			{
				Load(u16);
			} break;

			case BCOP_Load32:
			{
				Load(u32);
			} break;

			case BCOP_Load64:
			{
				Load(u64);
			} break;

#undef Load

#define Store(type) \
	do { \
		type _value; \
		uintptr _virtAddr; \
		ReadVarFromStack(type, _value); \
		ReadVarFromStack(uintptr, _virtAddr); \
		*reinterpret_cast<type *>(pInterp->pVirtualAddressSpace + _virtAddr) = _value; } while (0)

			case BCOP_Store8:
			{
				Store(u8);
			} break;

			case BCOP_Store16:
			{
				Store(u16);
			} break;

			case BCOP_Store32:
			{
				Store(u32);
			} break;

			case BCOP_Store64:
			{
				Store(u64);
			} break;

#undef Store

#define Duplicate(type) \
	do { \
		type _value; \
		PeekVarFromStack(type, _value); \
		WriteVarToStack(type, _value); } while (0)

			case BCOP_Duplicate8:
			{
				Duplicate(u8);
			} break;

			case BCOP_Duplicate16:
			{
				Duplicate(u16);
			} break;

			case BCOP_Duplicate32:
			{
				Duplicate(u32);
			} break;

			case BCOP_Duplicate64:
			{
				Duplicate(u64);
			} break;

#undef Duplicate

#define Binop(type, op) \
	do { \
		type _rhs; \
		type _lhs; \
		ReadVarFromStack(type, _rhs); \
		ReadVarFromStack(type, _lhs); \
		type _result = _lhs op _rhs; \
		WriteVarToStack(type, _result); } while (0)

			case BCOP_AddInt8:
			{
				Binop(u8, +);
			} break;

			case BCOP_AddInt16:
			{
				Binop(u16, +);
			} break;

			case BCOP_AddInt32:
			{
				Binop(u32, +);
			} break;

			case BCOP_AddInt64:
			{
				Binop(u64, +);
			} break;

			case BCOP_SubInt8:
			{
				Binop(u8, -);
			} break;

			case BCOP_SubInt16:
			{
				Binop(u16, -);
			} break;

			case BCOP_SubInt32:
			{
				Binop(u32, -);
			} break;

			case BCOP_SubInt64:
			{
				Binop(u64, -);
			} break;

			case BCOP_MulInt8:
			{
				Binop(u8, *);
			} break;

			case BCOP_MulInt16:
			{
				Binop(u16, *);
			} break;

			case BCOP_MulInt32:
			{
				Binop(u32, *);
			} break;

			case BCOP_MulInt64:
			{
				Binop(u64, *);
			} break;

			case BCOP_DivS8:
			{
				Binop(s8, /);
			} break;

			case BCOP_DivS16:
			{
				Binop(s16, /);
			} break;

			case BCOP_DivS32:
			{
				Binop(s32, /);
			} break;

			case BCOP_DivS64:
			{
				Binop(s64, /);
			} break;

			case BCOP_DivU8:
			{
				Binop(u8, /);
			} break;

			case BCOP_DivU16:
			{
				Binop(u16, /);
			} break;

			case BCOP_DivU32:
			{
				Binop(u32, /);
			} break;

			case BCOP_DivU64:
			{
				Binop(u64, /);
			} break;

			case BCOP_AddFloat32:
			{
				Binop(f32, +);
			} break;

			case BCOP_AddFloat64:
			{
				Binop(f64, +);
			} break;

			case BCOP_SubFloat32:
			{
				Binop(f32, -);
			} break;

			case BCOP_SubFloat64:
			{
				Binop(f64, -);
			} break;

			case BCOP_MulFloat32:
			{
				Binop(f32, *);
			} break;

			case BCOP_MulFloat64:
			{
				Binop(f64, *);
			} break;

			case BCOP_DivFloat32:
			{
				Binop(f32, /);
			} break;

			case BCOP_DivFloat64:
			{
				Binop(f64, /);
			} break;

#undef Binop

#define CompareOp(type, op) \
	do { \
		type _rhs; \
		type _lhs; \
		ReadVarFromStack(type, _rhs); \
		ReadVarFromStack(type, _lhs); \
		bool _result = _lhs op _rhs; \
		WriteVarToStack(bool, _result); } while (0)

			case BCOP_TestEqInt8:
			{
				CompareOp(u8, ==);
			} break;

			case BCOP_TestEqInt16:
			{
				CompareOp(u16, ==);
			} break;

			case BCOP_TestEqInt32:
			{
				CompareOp(u32, ==);
			} break;

			case BCOP_TestEqInt64:
			{
				CompareOp(u64, ==);
			} break;

			case BCOP_TestLtS8:
			{
				CompareOp(s8, <);
			} break;

			case BCOP_TestLtS16:
			{
				CompareOp(s16, <);
			} break;

			case BCOP_TestLtS32:
			{
				CompareOp(s32, <);
			} break;

			case BCOP_TestLtS64:
			{
				CompareOp(s64, <);
			} break;

			case BCOP_TestLtU8:
			{
				CompareOp(u8, < );
			} break;

			case BCOP_TestLtU16:
			{
				CompareOp(u16, < );
			} break;

			case BCOP_TestLtU32:
			{
				CompareOp(u32, < );
			} break;

			case BCOP_TestLtU64:
			{
				CompareOp(u64, < );
			} break;

			case BCOP_TestLteS8:
			{
				CompareOp(s8, <=);
			} break;

			case BCOP_TestLteS16:
			{
				CompareOp(s16, <=);
			} break;

			case BCOP_TestLteS32:
			{
				CompareOp(s32, <=);
			} break;

			case BCOP_TestLteS64:
			{
				CompareOp(s64, <=);
			} break;

			case BCOP_TestLteU8:
			{
				CompareOp(u8, <=);
			} break;

			case BCOP_TestLteU16:
			{
				CompareOp(u16, <=);
			} break;

			case BCOP_TestLteU32:
			{
				CompareOp(u32, <=);
			} break;

			case BCOP_TestLteU64:
			{
				CompareOp(u64, <=);
			} break;

			case BCOP_TestEqFloat32:
			{
				CompareOp(f32, ==);
			} break;

			case BCOP_TestEqFloat64:
			{
				CompareOp(f64, ==);
			} break;

			case BCOP_TestLtFloat32:
			{
				CompareOp(f32, <);
			} break;

			case BCOP_TestLtFloat64:
			{
				CompareOp(f64, <);
			} break;

			case BCOP_TestLteFloat32:
			{
				CompareOp(f32, <=);
			} break;

			case BCOP_TestLteFloat64:
			{
				CompareOp(f64, <= );
			} break;

#undef CompareOp

			case BCOP_Not:
			{
				bool val;
				ReadVarFromStack(bool, val);
				val = !val;
				WriteVarToStack(bool, val);
			} break;

#define Negate(type) \
	do { \
		type _val; \
		ReadVarFromStack(type, _val); \
		_val = -_val; \
		WriteVarToStack(type, _val); } while (0)

			case BCOP_NegateS8:
			{
				Negate(s8);
			} break;

			case BCOP_NegateS16:
			{
				Negate(s16);
			} break;

			case BCOP_NegateS32:
			{
				// HMM: Possibility for overflow when negating INT_MIN. What do I want to do here?

				Negate(s32);
			} break;

			case BCOP_NegateS64:
			{
				Negate(s64);
			} break;

			case BCOP_NegateFloat32:
			{
				Negate(f32);
			} break;

			case BCOP_NegateFloat64:
			{
				Negate(f64);
			} break;

#undef Negate

			case BCOP_Jump:
			{
				s16 bytesToJump;
				ReadVarFromBytecode(s16, bytesToJump);
				pInterp->ip += bytesToJump;
			} break;

			case BCOP_JumpIfFalse:
			{
				s16 bytesToJump;
				ReadVarFromBytecode(s16, bytesToJump);

				u8 boolVal;
				ReadVarFromStack(u8, boolVal);

				if (!boolVal)
				{
					pInterp->ip += bytesToJump;
				}
			} break;

			case BCOP_JumpIfPeekFalse:
			{
				s16 bytesToJump;
				ReadVarFromBytecode(s16, bytesToJump);

				u8 boolVal;
				PeekVarFromStack(u8, boolVal);

				if (!boolVal)
				{
					pInterp->ip += bytesToJump;
				}
			} break;

			case BCOP_JumpIfPeekTrue:
			{
				s16 bytesToJump;
				ReadVarFromBytecode(s16, bytesToJump);

				u8 boolVal;
				PeekVarFromStack(u8, boolVal);

				if (boolVal)
				{
					pInterp->ip += bytesToJump;
				}
			} break;

			case BCOP_StackAlloc:
			{
				uintptr bytesToReserve;
				ReadVarFromBytecode(uintptr, bytesToReserve);

				pInterp->pStack += bytesToReserve;
			} break;

			case BCOP_StackFree:
			{
				uintptr bytesToFree;
				ReadVarFromBytecode(uintptr, bytesToFree);

				pInterp->pStack -= bytesToFree;
			} break;

			case BCOP_Call:
			{
				AssertTodo;
			} break;

			case BCOP_Return0:
			{
				AssertTodo;
			} break;

			case BCOP_Return8:
			{
				AssertTodo;
			} break;

			case BCOP_Return16:
			{
				AssertTodo;
			} break;

			case BCOP_Return32:
			{
				AssertTodo;
			} break;

			case BCOP_Return64:
			{
				AssertTodo;
			} break;

			case BCOP_DebugPrint:
			{
				TypeId typid;
				ReadVarFromBytecode(TypeId, typid);

				switch (typid)
				{
					case TypeId::U8:
					{
						u8 val;
						ReadVarFromStack(u8, val);
						printfmt("%" PRIu8, val);
						println();
					} break;

					case TypeId::U16:
					{
						u16 val;
						ReadVarFromStack(u16, val);
						printfmt("%" PRIu16, val);
						println();
					} break;

					case TypeId::U32:
					{
						u32 val;
						ReadVarFromStack(u32, val);
						printfmt("%" PRIu32, val);
						println();
					} break;

					case TypeId::U64:
					{
						u64 val;
						ReadVarFromStack(u64, val);
						printfmt("%" PRIu64, val);
						println();
					} break;

					case TypeId::S8:
					{
						s8 val;
						ReadVarFromStack(s8, val);
						printfmt("%" PRId8, val);
						println();
					} break;

					case TypeId::S16:
					{
						s16 val;
						ReadVarFromStack(s16, val);
						printfmt("%" PRId16, val);
						println();
					} break;

					case TypeId::S32:
					{
						s32 val;
						ReadVarFromStack(s32, val);
						printfmt("%" PRId32, val);
						println();
					} break;

					case TypeId::S64:
					{
						s64 val;
						ReadVarFromStack(s64, val);
						printfmt("%" PRId64, val);
						println();
					} break;

					case TypeId::F32:
					{
						f32 val;
						ReadVarFromStack(f32, val);
						printfmt("%f", val);
						println();
					} break;

					case TypeId::F64:
					{
						f64 val;
						ReadVarFromStack(f64, val);
						printfmt("%f", val);
						println();
					} break;

					default:
						AssertTodo;
						break;
				}
			} break;

			case BCOP_DebugExit:
			{
				run = false;
			} break;

			default:
			{
				AssertNotReached;
			} break;
		}

#undef ReadVarFromBytecode
#undef WriteBytecodeBytesToStack
#undef WriteVarToStack
#undef ReadVarFromStack
#undef PeekVarFromStack
	}
}

uintptr virtualAddressStart(const Scope & scope)
{
	uintptr offset = 0;
	Scope * pScopeParent = scope.pScopeParent;

	while (pScopeParent)
	{
		// FIXME: This will break. Need to consider params, return values, return address, etc. all
		//	on the stack.

		offset += cByteLocalVars(*pScopeParent);
		pScopeParent = pScopeParent->pScopeParent;
	}

	return offset;
}
