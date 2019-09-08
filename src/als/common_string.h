#pragma once

#ifdef ALS_MACRO
#include "macro_assert.h"
#endif

#ifndef ALS_COMMON_ALLOC_StaticAssert
#ifndef ALS_DEBUG
#define ALS_COMMON_ALLOC_StaticAssert(X)
#elif defined(ALS_MACRO)
#define ALS_COMMON_ALLOC_StaticAssert(x) StaticAssert(x)
#else
// C11 _Static_assert
#define ALS_COMMON_ALLOC_StaticAssert(x) _Static_assert(x)
#endif
#endif

#ifndef ALS_COMMON_ALLOC_Assert
#ifndef ALS_DEBUG
#define ALS_COMMON_ALLOC_Assert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_ALLOC_Assert(x) Assert(x)
#else
// C assert
#include <assert.h>
#define ALS_COMMON_ALLOC_Assert(x) assert(x)
#endif
#endif

#ifndef ALS_COMMON_ALLOC_Verify
#ifndef ALS_DEBUG
#define ALS_COMMON_ALLOC_Verify(x) x
#else
#define ALS_COMMON_ALLOC_Verify(x) ALS_COMMON_ALLOC_Assert(x)
#endif
#endif

// Just a wrapper around a fixed sized buffer, so that I can do something like DynamicArray<StringBox<256>>... DynamicArray<char[256]> doesn't work
//  because you can't assign arrays by value.

template <unsigned int BufferSize>
struct StringBox
{
    char aBuffer[BufferSize];
};
