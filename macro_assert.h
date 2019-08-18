#pragma once

// TODO: ifdef this so it compiles away

#if _DEBUG

#define Assert(x) do { if (!(x))  __debugbreak(); } while(0)
#define Verify(x) Assert(x)

#else

#define Assert(x)
#define Verify(x) x

#endif

// TODO: verify macro that compiles away the debug break but doesn't compile away the expression since it could have side-effects
