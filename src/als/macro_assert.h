#pragma once

#if _DEBUG

#define Assert(x) do { if (!(x))  __debugbreak(); } while(0)
#define Verify(x) Assert(x)

// TODO: Morph this into some sort of AssertLog or AssertPrint so that it actually logs the error somehow?

#define AssertInfo(x, helpMsg) Assert(x)

#else

#define Assert(x)
#define Verify(x) x
#define AssertInfo(x)

#endif

#define Implies(p, q) (!(p)) || (q)
#define Iff(p, q) (p) == (q)
#define StaticAssert(x) _STATIC_ASSERT(x)
