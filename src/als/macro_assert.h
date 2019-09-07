#pragma once

#if _DEBUG

#define Assert(x) do { if (!(x))  __debugbreak(); } while(0)

// TODO: Morph this into some sort of AssertLog or AssertPrint so that it actuallyl ogs the error somehow
#define AssertInfo(x, helpMsg) Assert(x)
#define Verify(x) Assert(x)

#else

#define Assert(x)
#define AssertPrint(x)
#define Verify(x) x

#endif

#define StaticAssert(x) _STATIC_ASSERT(x)
