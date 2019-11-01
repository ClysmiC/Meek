#pragma once

#ifdef ALS_MACRO
#include "macro_assert.h"
#endif

#ifndef ALS_COMMON_STRING_StaticAssert
#ifndef ALS_DEBUG
#define ALS_COMMON_STRING_StaticAssert(X)
#elif defined(ALS_MACRO)
#define ALS_COMMON_STRING_StaticAssert(x) StaticAssert(x)
#else
// C11 _Static_assert
#define ALS_COMMON_STRING_StaticAssert(x) _Static_assert(x)
#endif
#endif

#ifndef ALS_COMMON_STRING_Assert
#ifndef ALS_DEBUG
#define ALS_COMMON_STRING_Assert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_STRING_Assert(x) Assert(x)
#else
// C assert
#include <assert.h>
#define ALS_COMMON_STRING_Assert(x) assert(x)
#endif
#endif

#ifndef ALS_COMMON_STRING_Verify
#ifndef ALS_DEBUG
#define ALS_COMMON_STRING_Verify(x) x
#else
#define ALS_COMMON_STRING_Verify(x) ALS_COMMON_STRING_Assert(x)
#endif
#endif

// A fairly dumb string implementation

struct String
{
	const static char gc_zeroString = '\0';
	constexpr static float gc_growthFactor = 1.5f;

	int cChar;
	int capacity;		// NOTE: Actual buffer is at least this + 1 to make sure we can always null terminate
	char * pBuffer;     // NOTE: If you mutate the buffer when it is pointing at the zero string then you will break everything!
};

inline void ensureCapacity(String * pStr, int requestedCapacity)
{
	if (requestedCapacity <= pStr->capacity) return;

	static const int s_minCapacity = 8;
	int newCapacity = (pStr->capacity > s_minCapacity) ? pStr->capacity : s_minCapacity;

	while (newCapacity < requestedCapacity)
	{
		int prevNewCapacity = newCapacity;
		newCapacity = static_cast<int>((newCapacity * String::gc_growthFactor) + 1.0f);       // + 1 to force round up

		// Overflow check... probably overkill

		if (newCapacity < prevNewCapacity)
		{
			newCapacity = ~0u;
		}
	}

	int actualRequestCapacity = newCapacity + 1;	// Always make sure we have room to write a null terminator, even if cChar = capacity

	if (pStr->pBuffer == &String::gc_zeroString)
	{
		pStr->pBuffer = static_cast<char *>(malloc(actualRequestCapacity * sizeof(char)));
	}
	else
	{
		ALS_COMMON_STRING_Assert(pStr->pBuffer);
		pStr->pBuffer = static_cast<char *>(realloc(pStr->pBuffer, actualRequestCapacity * sizeof(char)));
	}

	pStr->capacity = newCapacity;
}

inline void init(String * pStr)
{
	pStr->cChar = 0;
	pStr->capacity = 0;
	pStr->pBuffer = const_cast<char *>(&String::gc_zeroString);
}

inline void init(String * pStr, char * chars)
{
    pStr->pBuffer = const_cast<char *>(&String::gc_zeroString);

	char * cursor = chars;
	while (*cursor)
	{
		cursor++;
	}

	pStr->cChar = cursor - chars;
	ensureCapacity(pStr, pStr->cChar);

	for (int i = 0; i < pStr->cChar; i++)
	{
		pStr->pBuffer[i] = chars[i];
	}

	// NOTE: This is always safe due to the extra byte requested inside ensureCapacity

	pStr->pBuffer[pStr->cChar] = '\0';
}

inline void append(String * pString, const char * charsToAppend)
{
    const char * cursor = charsToAppend;
    while (*cursor)
    {
        ensureCapacity(pString, pString->cChar + 1);

        pString->pBuffer[pString->cChar] = *cursor;
        pString->cChar++;

        cursor++;
    }

    // NOTE: This is always safe due to the extra byte requested inside ensureCapacity

    pString->pBuffer[pString->cChar] = '\0';
}

inline void dispose(String * pStr)
{
	ALS_COMMON_STRING_Assert(pStr->pBuffer);		// Even the empty string should point to our global "zero" buffer

	if (pStr->pBuffer != &String::gc_zeroString)
	{
		free(pStr->pBuffer);
		pStr->pBuffer = const_cast<char *>(&String::gc_zeroString);
	}
}

#undef ALS_COMMON_STRING_StaticAssert
#undef ALS_COMMON_STRING_Assert
#undef ALS_COMMON_STRING_Verify
