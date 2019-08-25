#pragma once

#ifdef ALS_MACRO
#include "macro_assert.h"
#endif

#ifndef ALS_COMMON_ARRAY_StaticAssert
#ifndef ALS_DEBUG
#define ALS_COMMON_ARRAY StaticAssert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_ARRAY_StaticAssert(x) StaticAssert(x)
#else
// C11 _Static_assert
#define ALS_COMMON_ARRAY_StaticAssert(x) _Static_assert(x)
#endif
#endif

#ifndef ALS_COMMON_ARRAY_Assert
#ifndef ALS_DEBUG
#define ALS_COMMON_ARRAY Assert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_ARRAY_Assert(x) Assert(x)
#else
// C assert
#include <assert.h>
#define ALS_COMMON_ARRAY_Assert(x) assert(x)
#endif
#endif

#ifndef ALS_COMMON_ARRAY_Verify
#ifndef ALS_DEBUG
#define ALS_COMMON_ARRAY_Verify(x) x
#else
#define ALS_COMMON_ARRAY_Verify(x) ALS_COMMON_ARRAY_Assert(x)
#endif
#endif

// Ring Buffer (not thread safe)

template <typename T, unsigned int Capacity>
struct RingBuffer
{
	static constexpr unsigned int s_capacity = Capacity;

    T buffer[Capacity];
	unsigned int iBufferRead = 0;
	unsigned int cItem = 0;
};

template <typename T, unsigned int Capacity>
bool read(RingBuffer<T, Capacity> * pRbuf, T * poT)
{
    if (pRbuf->cItem == 0) return false;

    *poT = pRbuf->buffer[pRbuf->iBufferRead];

    pRbuf->iBufferRead++;
    pRbuf->iBufferRead %= Capacity;

    pRbuf->cItem--;

    return true;
}

template <typename T, unsigned int Capacity>
bool write(RingBuffer<T, Capacity> * pRbuf, const T & t)
{
	if (pRbuf->cItem >= Capacity)
	{
		ALS_COMMON_ARRAY_Assert(false);
		return false;
	}

	unsigned int iBufferWrite = (pRbuf->iBufferRead + pRbuf->cItem) % Capacity;
    pRbuf->buffer[iBufferWrite] = t;

    pRbuf->cItem++;

    return true;
}

template <typename T, unsigned int Capacity>
void forceWrite(RingBuffer<T, Capacity> * pRbuf, const T & t)
{
	if (pRbuf->cItem < Capacity)
	{
		write(pRbuf, pT);
	}
	else
	{
		// Overwrite first item in the buffer with the new one and
		//	bump the read index

		unsigned int iBufferWrite = pRbuf->iBufferRead;
		pRbuf->buffer[iBufferWrite] = t;

		pRbuf->iBufferRead++;
		pRbuf->iBufferRead %= Capacity;
	}
}

template <typename T, unsigned int Capacity>
bool peek(RingBuffer<T, Capacity> * pRbuf, unsigned int iItem, T * poT)
{
	if (iItem >= pRbuf->cItem) return false;

	unsigned int iBufferPeek = (pRbuf->iBufferRead + iItem) % Capacity;
	*poT = pRbuf->buffer[iBufferPeek];

	return true;
}

template <typename T, unsigned int Capacity>
bool isEmpty(RingBuffer<T, Capacity> * pRbuf)
{
	return pRbuf->cItem != 0;
}

template <typename T, unsigned int Capacity>
unsigned int count(RingBuffer<T, Capacity> * pRbuf)
{
	return pRbuf->cItem;
}



// Dynamic array (not thread safe)

template <typename T>
struct DynamicArray
{
	T * pBuffer = nullptr;
	unsigned int cItem = 0;
	unsigned int capacity = 0;

	static constexpr float s_growthFactor = 1.5f;
};

template <typename T>
void init (DynamicArray<T> * pArray, unsigned int startingCapacity=16)
{
	ALS_COMMON_ARRAY_Assert(!pBuffer);
	ensureCapacity(pArray, startingCapacity);
}

template <typename T>
void ensureCapacity(DynamicArray<T> * pArray, unsigned int requestedCapacity)
{
	if (requestedCapacity <= pArray->capacity) return;

	unsigned int newCapacity = pArray->capacity;

	while (newCapacity < requestedCapacity)
	{
		unsigned int prevNewCapacity = newCapacity;
		newCapacity *= da::s_growthFactor;

		// Overflow check... probably overkill

		if (newCapacity < prevNewCapacity)
		{
			newCapacity = ~0u;
		}
	}

	pArray->pBuffer = ALS_COMMON_ARRAY_Verify(static_cast<T *>(realloc(pArray->pBuffer, newCapacity * sizeof(T))));
	pArray->capacity = newCapacity;
}

template <typename T>
void append(DynamicArray<T> * pArray, const T & t)
{
	// Fastest way to insert

	ensureCapacity(pArray, pArray->cItem + 1);

	pArray->pBuffer[pArray->cItem] = t;
	pArray->cItem++;
}

template <typename T>
void prepend(DynamicArray<T> * pArray, const T & t)
{
	// Slowest way to insert

	// Could probably optimize this somehow by writing a version of ensureCapacity that leaves a hole
	//	at the specified index. That way we wouldn't have to do any shifting after reallocating...
	//	but then again we wouldn't be able to use realloc which already might do some allocator
	//	magic to avoid a copy. So probably not worth it...

	ensureCapacity(pArray, pArray->cItem + 1);

	unsigned int cItemShift = pArray->cItem;
	T * pDst = pArray->pBuffer + 1;
	T * pSrc = pArray->pBuffer;

	memmove(pDst, pSrc, cItemShift);

	pArray->pBuffer[0] = t;
	pArray->cItem++;
}

template <typename T>
void unorderedRemove(DynamicArray<T> * pArray, int iItem)
{
	// Fastest way to remove. Does not maintain order.

	if (iItem >= pArray->cItem) return;

	pArray->pBuffer[iItem] = pArray->pBuffer[pArray->cItem - 1];
	pArray->cItem--;
}

template <typename T>
void remove(DynamicArray<T> * pArray, int iItem)
{
	// Slowest way to remove. Maintains order.

	if (iItem >= pArray->cItem) return;

	unsigned int cItemShift = pArray->cItem - iItem - 1;
	T * pDst = pArray->pBuffer + iItem;
	T * pSrc = pDst + 1;

	memmove(pDst, pSrc, cItemShift);

	pArray->cItem--;
}
