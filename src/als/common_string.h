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
	char * pBuffer;
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
		char * pBufferReallocd = static_cast<char *>(realloc(pStr->pBuffer, actualRequestCapacity * sizeof(char)));
		if (!pBufferReallocd)
		{
			ALS_COMMON_STRING_Assert(false);		// Realloc failed! TODO (andrew) How to handle this?
		}

		pStr->pBuffer = pBufferReallocd;
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

	pStr->cChar = (int)(cursor - chars);
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

struct StringView
{
	int cCh = 0;
	const char * pCh = nullptr;
};

//inline StringView makeStringView(const char * pStr)
//{
//	StringView result;
//	result.pCh = pStr;
//	result.cCh = 0;
//
//	const char * pCursor = pStr;
//	while (*pCursor)
//	{
//		result.cCh++;
//		pCursor++;
//	}
//
//	return result;
//}

inline bool operator==(const StringView& strv0, const StringView& strv1)
{
	if (strv0.cCh != strv1.cCh)		return false;
	for (int iCh = 0; iCh < strv0.cCh; iCh++)
	{
		// NOTE (andrew) Capitalization matters

		if (strv0.pCh[iCh] != strv1.pCh[iCh])		return false;
	}

	return true;
}

inline bool operator!=(const StringView& strv0, const StringView& strv1)
{
	return !(strv0 == strv1);
}

inline bool operator==(const StringView& strv, const char* pchz)
{
	if (pchz[strv.cCh] != '\0')		return false;

	for (int iCh = 0; iCh < strv.cCh; iCh++)
	{
		// NOTE (andrew) Capitalization matters

		if (pchz[iCh] == '\0')				return false;
		if (strv.pCh[iCh] != pchz[iCh])		return false;
	}

	return true;
}

inline bool operator!=(const StringView& strv, const char* pchz)
{
	return !(strv == pchz);
}

inline void trim(StringView* pStrv)
{
	// Trim start

	while (pStrv->cCh > 0)
	{
		// TODO: Better IsWhitespace test

		switch (pStrv->pCh[0])
		{
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			{
				pStrv->pCh++;
				pStrv->cCh--;
			} break;

			default:
				goto LTrimStartDone;
		}
	}

LTrimStartDone:

	// Trim end

	while (pStrv->cCh > 0)
	{
		// TODO: Better IsWhitespace test

		switch (pStrv->pCh[pStrv->cCh - 1])
		{
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			{
				pStrv->cCh--;
			} break;

			default:
				goto LTrimEndDone;
		}
	}

LTrimEndDone:

	return;
}

inline bool startsWith(const StringView& strv, const char* pChz)
{
	for (int iCh = 0; iCh < strv.cCh; iCh++)
	{
		// NOTE (andrew) Capitalization matters

		if (pChz[iCh] == '\0')				return true;
		if (strv.pCh[iCh] != pChz[iCh])		return false;
	}

	return pChz[strv.cCh] == '\0';
}

#undef ALS_COMMON_STRING_StaticAssert
#undef ALS_COMMON_STRING_Assert
#undef ALS_COMMON_STRING_Verify
