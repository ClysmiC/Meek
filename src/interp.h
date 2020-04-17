#pragma once

#include "als.h"
#include "id_def.h"

struct BytecodeFunction;

namespace Interp
{

//struct Value
//{
//	TYPID typid;
//
//	union
//	{
//		bool boolean;
//
//		s8 s8;
//		s16 s16;
//		s32 s32;
//		s64 s64;
//
//		u8 u8;
//		u16 u16;
//		u32 u32;
//		u64 u64;
//
//		float float32;
//		double float64;
//
//		void * other;
//	} valueAs;
//};

struct Interpreter
{
	u8 * pStack;
	u8 * pStackBase;

	u8 * pBufferGlobals;

	u8 * ip;
};

void init(Interpreter * pInterp);
void dispose(Interpreter * pInterp);

void interpret(Interpreter * pInterp, const BytecodeFunction & bcf);

}