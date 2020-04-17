#pragma once

#include "als.h"

// ByteCode OPeration

enum BCOP : u8
{
	BCOP_Return,

	BCOP_Constant8,
	BCOP_Constant16,
	BCOP_Constant32,
	BCOP_Constant64,

	BCOP_ConstantTrue,
	BCOP_ConstantFalse,

	BCOP_AddInt8,
	BCOP_AddInt16,
	BCOP_AddInt32,
	BCOP_AddInt64,

	BCOP_SubInt8,
	BCOP_SubInt16,
	BCOP_SubInt32,
	BCOP_SubInt64,

	BCOP_MulInt8,
	BCOP_MulInt16,
	BCOP_MulInt32,
	BCOP_MulInt64,

	BCOP_DivS8,
	BCOP_DivS16,
	BCOP_DivS32,
	BCOP_DivS64,

	BCOP_DivU8,
	BCOP_DivU16,
	BCOP_DivU32,
	BCOP_DivU64,

	BCOP_AddFloat32,
	BCOP_AddFloat64,

	BCOP_SubFloat32,
	BCOP_SubFloat64,

	BCOP_MulFloat32,
	BCOP_MulFloat64,

	BCOP_DivFloat32,
	BCOP_DivFloat64,

	BCOP_Assign8,
	BCOP_Assign16,
	BCOP_Assign32,
	BCOP_Assign64,

	BCOP_Pop8,
	BCOP_Pop16,
	BCOP_Pop32,
	BCOP_Pop64,
	BCOP_PopX,

	BCOP_Reserve8,
	BCOP_Reserve16,
	BCOP_Reserve32,
	BCOP_Reserve64,
	BCOP_ReserveX,

	// TODO: modulus

	// TODO: negate


	BCOP_Max
};

extern const int gc_mpBcopCByte[];

struct BytecodeFunction
{
	DynamicArray<u8> bytes;
	DynamicArray<int> mpIByteIStart;
};

void init(BytecodeFunction * pBcf);
void dispose(BytecodeFunction * pBcf);

void compileBytecode(AstNode * pNodeRoot);

#ifdef DEBUG
// void disassemble(const ByteCodeFunction & bcf);
#endif