#include "bytecode.h"

#include "error.h"
#include "print.h"

const int gc_mpBcopCByte[] = {
	1,		// BCOP_Return
	2,		// BCOP_Constant8
	3,		// BCOP_Constant16
	5,		// BCOP_Constant32
	9,		// BCOP_Constant64
	1,		// BCOP_ConstantTrue
	1,		// BCOP_ConstantFalse
	3,		// BCOP_AddInt8
	5,		// BCOP_AddInt16
	9,		// BCOP_AddInt32
	17,		// BCOP_AddInt64
	3,		// BCOP_SubInt8
	5,		// BCOP_SubInt16
	9,		// BCOP_SubInt32
	17,		// BCOP_SubInt64
	3,		// BCOP_MulInt8
	5,		// BCOP_MulInt16
	9,		// BCOP_MulInt32
	17,		// BCOP_MulInt64
	3,		// BCOP_DivS8
	5,		// BCOP_DivS16
	9,		// BCOP_DivS32
	17,		// BCOP_DivS64
	3,		// BCOP_DivU8
	5,		// BCOP_DivU16
	9,		// BCOP_DivU32
	17,		// BCOP_DivU64
	9,		// BCOP_AddFloat32
	17,		// BCOP_AddFloat64
	9,		// BCOP_SubFloat32
	17,		// BCOP_SubFloat64
	9,		// BCOP_MulFloat32
	17,		// BCOP_MulFloat64
	9,		// BCOP_DivFloat32
	17,		// BCOP_DivFloat64
	1 + sizeof(uintptr),	// BCOP_Assign8
	1 + sizeof(uintptr),	// BCOP_Assign16
	1 + sizeof(uintptr),	// BCOP_Assign32
	1 + sizeof(uintptr),	// BCOP_Assign64
	1,		// BCOP_Pop8
	1,		// BCOP_Pop16
	1,		// BCOP_Pop32
	1,		// BCOP_Pop64
	5,		// BCOP_PopX
	1,		// BCOP_Reserve8
	1,		// BCOP_Reserve16
	1,		// BCOP_Reserve32
	1,		// BCOP_Reserve64
	5,		// BCOP_ReserveX
};
StaticAssert(ArrayLen(gc_mpBcopCByte) == BCOP_Max);

static const char * c_mpBcopStrName[] = {
	"Return",
	"Constant8",
	"Constant16",
	"Constant32",
	"Constant64",
	"ConstantTrue",
	"ConstantFalse",
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
	"Assign8",
	"Assign16",
	"Assign32",
	"Assign64",
	"Pop8",
	"Pop16",
	"Pop32",
	"Pop64",
	"PopX",
	"Reserve8",
	"Reserve16",
	"Reserve32",
	"Reserve64",
	"ReserveX",
};
StaticAssert(ArrayLen(c_mpBcopStrName) == BCOP_Max);

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

void compileBytecode(AstNode * pNodeRoot)
{

}

#ifdef DEBUG
//void disassemble(const ByteCodeFunction & bcf)
//{
//	int byteOffset = 0;
//	int linePrev = -1;
//	
//	while (byteOffset < bcf.bytes.cItem)
//	{
//		printfmt("%08d ", byteOffset);
//		
//		u8 bcop = bcf.bytes[byteOffset];
//		int iStart = bcf.mpIByteIStart[byteOffset];
//
//		int line = 1;	// TODO: compute from iStart
//
//		if (line == linePrev)
//		{
//			print("   | ");
//		}
//		else
//		{
//			printfmt("%8d", line);
//		}
//
//		print(c_mpBcopStrName[bcop]);
//
//		switch (bcop)
//		{
//			case BCOP_Return:
//			case BCOP_ConstantTrue:
//			case BCOP_ConstantFalse:
//			{
//				Assert(gc_mpBcopCByte[bcop] == 1);
//			} break;
//
//			case BCOP_Constant8:
//			case BCOP_Constant16:
//			case BCOP_Constant32:
//			case BCOP_Constant64:
//			{
//				// TODO: print literal value... might be hard since we don't store the type! We would just have to print the binary...
//			} break;
//
//			default:
//			{
//				AssertNotReached;
//			} break;
//		}
//
//		println();
//
//		byteOffset += gc_mpBcopCByte[bcop];
//	}
//}
#endif
