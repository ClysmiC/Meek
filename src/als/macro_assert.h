#pragma once

#if _DEBUG

#define Assert(x) do { if (!(x))  __debugbreak(); } while(0)
#define Verify(x) Assert(x)

#else

#define Assert(x)
#define Verify(x) x

#endif

#define StaticAssert(x) _STATIC_ASSERT(x)
