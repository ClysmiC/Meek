#include "interp.h"

#include "bytecode.h"

namespace Interp
{

void init(Interpreter * pInterp)
{
	constexpr int c_MiB = 1024 * 1024;

	pInterp->pStackBase = new u8[c_MiB];	// TODO: tweak this? detect overrun?
	pInterp->pStack = pInterp->pStackBase;

	// TODO: Allocate constant buffer... Need to compute how big?
	// TODO: Allocate global buffer

	pInterp->ip = nullptr;
}

void dispose(Interpreter * pInterp)
{
	delete[] pInterp->pStackBase;

	free(pInterp->pBufferGlobals);

	pInterp->pStackBase = nullptr;
	pInterp->pStack = nullptr;
}

void interpret(Interpreter * pInterp, const BytecodeFunction & bcf)
{
	pInterp->ip = bcf.bytes.pBuffer;

#define ReadVarFromBytecode(type, var) \
	do { \
		memcpy(&var, pInterp->ip, sizeof(type)); \
		pInterp->ip += sizeof(type); } while (0)

#define WriteBytecodeBytesToStack(cByte) \
	do { \
		memcpy(pInterp->pStack, pInterp->ip, sizeof(u##cByte)); \
		pInterp->ip += sizeof(u##cByte); \
		pInterp->pStack += sizeof(u##cByte); } while(0)

#define WriteVarToStack(type, var) \
	do { \
		memcpy(pInterp->pStack, &var, sizeof(type)); \
		pInterp->pStack += sizeof(type); } while (0)

#define ReadVarFromStack(type, var) \
	do { \
		pInterp->pStack = pInterp->pStack - sizeof(type); \
		memcpy(&var, pInterp->pStack, sizeof(type)); } while (0)

	// TODO: make sure the lhs/rhs aren't backwards... I think they are...
#define Binop(type, op) \
	do { \
		type _lhs; \
		type _rhs; \
		ReadVarFromStack(type, _lhs); \
		ReadVarFromStack(type, _rhs); \
		type _result = _lhs op _rhs; \
		WriteVarToStack(type, _result); } while (0)

#define Assign(cByte) \
	do { \
		u##cByte _val; \
		ReadVarFromStack(u##cByte, _val); \
		u##cByte * _pVar; \
		ReadVarFromBytecode(u##cByte *, _pVar); \
		*_pVar = _val; } while (0)

	while (true)
	{
#if DEBUG
		u8 ipPrev = *pInterp->ip;
#endif

		BCOP bcop = BCOP(*pInterp->ip);
		pInterp->ip++;

		switch (bcop)
		{
			case BCOP_Return:
			{
				AssertTodo;
			} break;

			case BCOP_Constant8:
			{
				WriteBytecodeBytesToStack(8);
			} break;

			case BCOP_Constant16:
			{
				WriteBytecodeBytesToStack(16);
			} break;

			case BCOP_Constant32:
			{
				WriteBytecodeBytesToStack(32);
			} break;

			case BCOP_Constant64:
			{
				WriteBytecodeBytesToStack(64);
			} break;

			case BCOP_ConstantTrue:
			{
				*pInterp->pStack = 1;
				pInterp->pStack++;
			} break;

			case BCOP_ConstantFalse:
			{
				*pInterp->pStack = 0;
				pInterp->pStack++;
			} break;

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

			case BCOP_Assign8:
			{
				Assign(8);
			} break;

			case BCOP_Assign16:
			{
				Assign(16);
			} break;

			case BCOP_Assign32:
			{
				Assign(32);
			} break;

			case BCOP_Assign64:
			{
				Assign(64);
			} break;

			case BCOP_Pop8:
			{
				pInterp->pStack -= sizeof(u8);
			} break;

			case BCOP_Pop16:
			{
				pInterp->pStack -= sizeof(u16);
			} break;

			case BCOP_Pop32:
			{
				pInterp->pStack -= sizeof(u32);
			} break;

			case BCOP_Pop64:
			{
				pInterp->pStack -= sizeof(u64);
			} break;

			case BCOP_PopX:
			{
				u32 cByte;
				ReadVarFromBytecode(u32, cByte);
				pInterp->pStack -= cByte;
			} break;

			case BCOP_Reserve8:
			{
				pInterp->pStack += sizeof(u8);
			} break;

			case BCOP_Reserve16:
			{
				pInterp->pStack += sizeof(u16);
			} break;

			case BCOP_Reserve32:
			{
				pInterp->pStack += sizeof(u32);
			} break;

			case BCOP_Reserve64:
			{
				pInterp->pStack += sizeof(u64);
			} break;

			case BCOP_ReserveX:
			{
				u32 cByte;
				ReadVarFromBytecode(u32, cByte);
				pInterp->pStack += cByte;
			} break;

			default:
			{
				AssertNotReached;
			} break;
		}

#if DEBUG
		Assert(*pInterp->ip == ipPrev + gc_mpBcopCByte[bcop]);
		Assert(pInterp->pStack >= pInterp->pStackBase);
#endif

#undef ReadVarFromBytecode
#undef WriteBytecodeBytesToStack
#undef WriteVarToStack
#undef ReadVarFromStack
#undef Binop
#undef Assign
	}
}

}
