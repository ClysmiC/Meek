#pragma once

#include "als.h"
#include "ast.h"

struct MeekCtx;

// ByteCode OPeration

enum BCOP : u8
{
	// - All intermediate primitive values on the stack are either 32 or 64 bits
	//		- Structs can also sit on the stack as intermediate values. They are their normal size.
	// - Locals on the stack are 8, 16, 32, or 64 bits
	//		- When loaded
	// - Values in bytecode are 8, 16, 32, or 64 bits

	// Load Immediate
	//	- Reads n-bit value from bytecode
	//	- Pushes it onto stack

	BCOP_LoadImmediate32,
	BCOP_LoadImmediate64,
	BCOP_LoadTrue,			// NOTE: Doesn't read extra value from bytecode
	BCOP_LoadFalse,			//	"

	// Load
	//	- Reads s32 offset from bytecode
	//	- Dereferences n-bit local value at frame pointer + offset
	//	- Sign-extends (SX) or zero-extends (zx) the value up to 32/64 bits
	//	- Pushes dereferenced value onto the stack

	BCOP_LoadLocalZX8,
	BCOP_LoadLocalZX16,
	BCOP_LoadLocalZX32,
	BCOP_LoadLocalZX64,
	BCOP_LoadLocalSX8,
	BCOP_LoadLocalSX16,
	BCOP_LoadLocalSX32,
	BCOP_LoadLocalSX64,

	// Store
	//	- Reads s32 offset from bytecode
	//	- Pops value off the stack
	//	- Truncates value to n-bits and stores at frame pointer + offset

	BCOP_Store8,
	BCOP_Store16,
	BCOP_Store32,
	BCOP_Store64,

	// Duplicate
	//	- Peeks value at top of the stack
	//	- Pushes a copy of it

	BCOP_Duplicate32,
	BCOP_Duplicate64,

	// Add Int
	//	- Pops 2 int values off the stack
	//	- Adds them
	//	- Pushes int result onto the stack

	BCOP_AddInt32,
	BCOP_AddInt64,

	// Sub Int
	//	- Pops int subtrahend off the stack
	//	- Pops int minuend off the stack
	//	- Subtracts them (minuend - subtrahend)
	//	- Pushes int result onto the stack

	BCOP_SubInt32,
	BCOP_SubInt64,

	// Mul Int
	//	- Pops 2 n-bit int values off the stack
	//	- Multiplies them
	//	- Pushes n-bit int result onto the stack

	BCOP_MulInt32,
	BCOP_MulInt64,

	// Div Int
	//	- Pops n-bit int divisor off the stack
	//	- Pops n-bit int dividend off the stack
	//	- Divides them (dividend / divisor) ; Integer division is performed
	//	- Pushes n-bit int result onto the stack
	//	* Use S instructions for signed division, and U instructions for unsigned

	BCOP_DivS32,
	BCOP_DivS64,

	BCOP_DivU32,
	BCOP_DivU64,

	// Add Float
	//	- Pops 2 n-bit float values off the stack
	//	- Adds them
	//	- Pushes n-bit float result onto the stack

	BCOP_AddFloat32,
	BCOP_AddFloat64,

	// Sub Float
	//	- Pops n-bit float subtrahend off the stack
	//	- Pops n-bit float minuend off the stack
	//	- Subtracts them (minuend - subtrahend)
	//	- Pushes n-bit float result onto stack

	BCOP_SubFloat32,
	BCOP_SubFloat64,

	// Mul Float
	//	- Pops 2 n-bit float values off the stack
	//	- Multiplies them
	//	- Pushes n-bit float result onto stack

	BCOP_MulFloat32,
	BCOP_MulFloat64,

	// Div Float
	//	- Pops n-bit float divisor from the stack
	//	- Pops n-bit float dividend from the stack
	//	- Divides them (dividend / divisor)
	//	- Pushes n-bit float result onto stack

	BCOP_DivFloat32,
	BCOP_DivFloat64,

	// Test Equal Int
	//	- Pops 2 n-bit int values off the stack
	//	- Pushes bool onto the stack. True if the values are equal, false otherwise.

	BCOP_TestEqInt32,
	BCOP_TestEqInt64,

	// Test Less Than Signed/Unsigned Int
	//	- Pops int value (b) off the stack
	//	- Pops int value (a) off the stack
	//	- Pushes bool onto the stack. True if the a < b, false otherwise.

	BCOP_TestLtS32,
	BCOP_TestLtS64,
	BCOP_TestLtU32,
	BCOP_TestLtU64,

	// Test Less Than or Equal Signed/Unsigned Int
	//	- Pops n-bit int value (b) off the stack
	//	- Pops n-bit int value (a) off the stack
	//	- Pushes bool onto the stack. True if the a <= b, false otherwise.

	BCOP_TestLteS8,
	BCOP_TestLteS16,
	BCOP_TestLteS32,
	BCOP_TestLteS64,
	BCOP_TestLteU8,
	BCOP_TestLteU16,
	BCOP_TestLteU32,
	BCOP_TestLteU64,

	// Test Equal Float
	//	- Pops 2 n-bit float values off the stack
	//	- Pushes bool onto the stack. True if the values are equal, false otherwise.

	BCOP_TestEqFloat32,
	BCOP_TestEqFloat64,

	// Test Less Than Float
	//	- Pops n-bit float value (b) off the stack
	//	- Pops n-bit float value (a) off the stack
	//	- Pushes bool onto the stack. True if the a < b, false otherwise.

	BCOP_TestLtFloat32,
	BCOP_TestLtFloat64,

	// Test Less Than or Equal Float
	//	- Pops n-bit float value (b) off the stack
	//	- Pops n-bit float value (a) off the stack
	//	- Pushes bool onto the stack. True if the a <= b, false otherwise.

	BCOP_TestLteFloat32,
	BCOP_TestLteFloat64,

	// Not
	//	- Pops bool off the stack
	//	- Performs logical not, then pushes it on the stack

	BCOP_Not,

	// Negate Signed Int
	//	- Pops n-bit int off the stack
	//	- Pushes its negated value onto the stack

	BCOP_NegateS8,
	BCOP_NegateS16,
	BCOP_NegateS32,
	BCOP_NegateS64,

	// Negate Float
	//	- Pops n-bit float off the stack
	//	- Pushes its negated value onto the stack

	BCOP_NegateFloat32,
	BCOP_NegateFloat64,

	// Jump
	//	- Reads s16 from bytecode
	//	- Advances IP that many bytes

	BCOP_Jump,

	// Jump
	//	- Reads (or peeks) 8 bit bool off the stack
	//	- Reads s16 from bytecode
	//	- Advances IP that many bytes if the bool is false

	BCOP_JumpIfFalse,
	BCOP_JumpIfPeekFalse,
	BCOP_JumpIfPeekTrue,

	// StackAlloc
	//	- Reads uintptr from bytecode
	//	- Reserves that many bytes on the top of the stack

	BCOP_StackAlloc,

	// StackFree
	//	- Reads uintptr from bytecode
	//	- Pops that many bytes off the top of the stack

	BCOP_StackFree,

	// Call
	//
	//	CALLER:
	//	- Reads uintptr offset from bytecode
	//	- Peeks func address (uintptr) inside the stack. The offset indicates how many bytes are on top of the address on the stack.
	//		An offset of 0 indicates that the address is on top. The bytes on top of the address are the evaluated arguments,
	//		placed on the stack by the caller. The callee will discard the address and all of the arguments above it when
	//		it discards its own call frame due to a RETURN.
	//	- Places caller's frame pointer on the stack. Set callee's FP to SP.
	//	- Places return address on the stack
	//	- Sets the IP to point to the first instruction of the function with the corresponding ID.
	//
	//	BEFORE CALL
	//                            SP
	//	STACK ->                   v
	//	[ funcAddress | arg0 | arg1
	//
	//	AFTER CALL                  FP            SP
	//	STACK ->                     v             v
	//	[ funcAddress | arg0 | arg1 | callerFP | RA 

	BCOP_Call,

	// Return
	//	- Reads uintptr from bytecode, which holds size of args on stack.
	//	- Pops n-bit RV off the stack (will be restored before return finishes)
	//	- Pops Return Address (RA) off the stack
	//		- If RA is null, terminates the program without executing the instructions below.
	//	- Pops callerFP off the stack and restores it to FP.
	//	- Pops args off the stack (using value read from bytecode)
	//	- Restores RV back onto the stack
	//	- Sets the IP to point to popped RA
	//
	//	BEFORE RETURN
	//                                                 SP
	//	STACK ->                                        v
	//	[ funcAddress | arg0 | arg1 | callerFP | RA | RV
	//
	//	AFTER RETURN
	//	STACK ->
	//	[ RV
		
	BCOP_Return0,
	BCOP_Return8,
	BCOP_Return16,
	BCOP_Return32,
	BCOP_Return64,

	// Debug Print (debug only, subject to removal!)
	//	- Reads typid from bytecode
	//	- Pops n-bit (n determined by typid) value off the stack
	//	- Prints output according to typid + value

	BCOP_DebugPrint,

	// Exit the program

	BCOP_DebugExit,

	BCOP_Max,
	BCOP_Nil = static_cast<u8>(0xFF),

//#ifdef _WIN64
//	BCOP_LoadImmediatePtr = BCOP_LoadImmediate64,
//#elif WIN32
//	BCOP_LoadImmediatePtr = BCOP_LoadImmediate32,
//#else
//	StaticAssertNotCompiled;
//#endif
};

// extern const int gc_mpBcopCByte[];

enum SIZEDBCOP
{
	SIZEDBCOP_LoadImmediate,
	SIZEDBCOP_Load,
	SIZEDBCOP_Store,
	SIZEDBCOP_Duplicate,
	SIZEDBCOP_AddInt,
	SIZEDBCOP_SubInt,
	SIZEDBCOP_MulInt,
	SIZEDBCOP_DivSignedInt,
	SIZEDBCOP_DivUnsignedInt,
	SIZEDBCOP_AddFloat,
	SIZEDBCOP_SubFloat,
	SIZEDBCOP_MulFloat,
	SIZEDBCOP_DivFloat,
	SIZEDBCOP_TestEqInt,
	SIZEDBCOP_TestLtSignedInt,
	SIZEDBCOP_TestLtUnsignedInt,
	SIZEDBCOP_TestLteSignedInt,
	SIZEDBCOP_TestLteUnsignedInt,
	SIZEDBCOP_TestEqFloat,
	SIZEDBCOP_TestLtFloat,
	SIZEDBCOP_TestLteFloat,
	SIZEDBCOP_NegateSigned,
	SIZEDBCOP_NegateFloat,

	SIZEDBCOP_Nil = -1
};
BCOP bcopSized(SIZEDBCOP sizedBcop, int cBit);

struct BytecodeFunction
{
	AstNode * pFuncNode;

	int iByte0;
	int cByte;
};

struct BytecodeProgram
{
	MeekCtx * pCtx;
	DynamicArray<u8> bytes;
	DynamicArray<BytecodeFunction> mpFuncidBcf;		// In order of appearance in bytecode
	DynamicArray<int> sourceLineNumbers;			// In order of appearance of ops in bytecode
};

struct FuncAddressPatch
{
	FUNCID funcid;
	int iByteAddrToPatch;
};

void init(BytecodeProgram * pBcp, MeekCtx * pCtx);

struct BytecodeBuilder
{
	struct NodeCtx
	{
		AstNode * pNode;
		bool wantsChildExprAddr;

		// Tag implied by pNode->astk

		union
		{
			struct UIfStmtCtx
			{
				int iJumpArgPlaceholder;	// Byte index of jump arg that needs to be backpatched
				int ipZero;					// Resulting IP if we were to jump with argument of 0
			} ifStmtData;

			struct UWhileStmtCtx
			{
				int iJumpPastLoopArgPlaceholder;
				int ipZeroJumpPastLoop;

				int ipJumpToTopOfLoop;
			} whileStmtData;

			struct UBinopExprCtx
			{
				int iJumpArgPlaceholder;	// For binops that short circuit evaluate
				int ipZero;					// "
			} binopExprData;

			struct UFuncCallExprCtx
			{
				int iByteAfterFuncAddr;
			} funcCallExprData;
		};
	};

	MeekCtx * pCtx;
	bool funcRoot;
	BytecodeProgram bytecodeProgram;
	Stack<NodeCtx> nodeCtxStack;
	DynamicArray<FuncAddressPatch> funcAddressPatches;
};

void init(BytecodeBuilder * pBuilder, MeekCtx * pCtx);

void init(BytecodeBuilder::NodeCtx * pNodeCtx, AstNode * pNode);

void compileBytecode(BytecodeBuilder * pBuilder);
void emitOp(BytecodeProgram * bcp, BCOP byteEmit, int lineNumber);
void emit(BytecodeProgram * bcp, u8 byteEmit);
void emit(BytecodeProgram * bcp, s8 byteEmit);
void emit(BytecodeProgram * bcp, u16 bytesEmit);
void emit(BytecodeProgram * bcp, s16 bytesEmit);
void emit(BytecodeProgram * bcp, u32 bytesEmit);
void emit(BytecodeProgram * bcp, s32 bytesEmit);
void emit(BytecodeProgram * bcp, u64 bytesEmit);
void emit(BytecodeProgram * bcp, s64 bytesEmit);
void emit(BytecodeProgram * bcp, f32 bytesEmit);
void emit(BytecodeProgram * bcp, f64 bytesEmit);
void emit(BytecodeProgram * bcp, void * pBytesEmit, int cBytesEmit);
void emitByteRepeat(BytecodeProgram * bcp, u8 byteEmit, int cRepeat);

void backpatchJumpArg(BytecodeProgram * bcp, int iBytePatch, int ipZero, int ipTarget);
void backpatch(BytecodeProgram * bcp, int iBytePatch, void * pBytesNew, int cBytesNew);

bool visitBytecodeBuilderPreorder(AstNode * pNode, void * pBuilder_);
void visitBytecodeBuilderPostOrder(AstNode * pNode, void * pBuilder_);
void visitBytecodeBuilderHook(AstNode * pNode, AWHK awhk, void * pBuilder_);

s32 framePointerOffset(MeekCtx * pCtx, const AstVarDeclStmt & varDecl, const BytecodeProgram & bcp);

#ifdef DEBUG
void disassemble(const BytecodeProgram & bcp);
#endif
