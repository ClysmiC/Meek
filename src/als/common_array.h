#pragma once

#ifdef ALS_MACRO
#include "macro_assert.h"
#endif

#ifndef ALS_COMMON_ARRAY_StaticAssert
#ifndef ALS_DEBUG
#define ALS_COMMON_ARRAY_StaticAssert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_ARRAY_StaticAssert(x) StaticAssert(x)
#else
// C11 _Static_assert
#define ALS_COMMON_ARRAY_StaticAssert(x) _Static_assert(x)
#endif
#endif

#ifndef ALS_COMMON_ARRAY_Assert
#ifndef ALS_DEBUG
#define ALS_COMMON_ARRAY_Assert(x)
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

#include <stdlib.h>     // realloc for DynamicArray

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
		write(pRbuf, t);
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
bool peek(const RingBuffer<T, Capacity> & pRbuf, unsigned int iItem, T * poT)
{
	if (iItem >= pRbuf.cItem) return false;

	unsigned int iBufferPeek = (pRbuf.iBufferRead + iItem) % Capacity;
	*poT = pRbuf.buffer[iBufferPeek];

	return true;
}

template <typename T, unsigned int Capacity>
bool isEmpty(const RingBuffer<T, Capacity> & pRbuf)
{
	return pRbuf.cItem != 0;
}

template <typename T, unsigned int Capacity>
unsigned int count(const RingBuffer<T, Capacity> & pRbuf)
{
	return pRbuf.cItem;
}



// Dynamic array (not thread safe)

template <typename T>
struct DynamicArray
{
	T * pBuffer;
	unsigned int cItem;
	unsigned int capacity;

	static constexpr float s_growthFactor = 1.5f;

	const T& operator[] (unsigned int i) const
	{
		return this->pBuffer[i];
	}

	T& operator[] (unsigned int i)
	{
		return this->pBuffer[i];
	}
};

template <typename T>
void init (DynamicArray<T> * pArray, unsigned int startingCapacity=16)
{
    // UGH: I wish I could assert this, but there is trickiness with unions containing
    //  dynamic arrays and my pool allocator not zero-initializing the memory.

	// ALS_COMMON_ARRAY_Assert(!pArray->pBuffer);

    pArray->cItem = 0;
    pArray->capacity = 0;
    pArray->pBuffer = nullptr;

	ensureCapacity(pArray, startingCapacity);
}

template <typename T>
void initMove (DynamicArray<T> * pArray, DynamicArray<T> * pArraySrc)
{
    // UGH: I wish I could assert this, but there is trickiness with unions containing
    //  dynamic arrays and my pool allocator not zero-initializing the memory.

	// ALS_COMMON_ARRAY_Assert(!pArray->pBuffer);
	*pArray = *pArraySrc;

	pArraySrc->pBuffer = nullptr;
	pArraySrc->cItem = 0;
	pArraySrc->capacity = 0;
}

template <typename T>
void destroy (DynamicArray<T> * pArray)
{
    // NOTE: This might help catch destroying dynamic array that was never inited too, but we can't guarantee
    //  that the allocation zeroes out the memory.

    ALS_COMMON_ARRAY_Assert(pArray->pBuffer);       // Trying to destroy dynamic array that was already destroyed? Or was moved (which performs destroy)?");

	if (pArray->pBuffer) free(pArray->pBuffer);
    pArray->pBuffer = nullptr;
}


template <typename T>
void ensureCapacity(DynamicArray<T> * pArray, unsigned int requestedCapacity)
{
    typedef DynamicArray<T> da;

	if (requestedCapacity <= pArray->capacity) return;

	unsigned int newCapacity = (pArray->capacity > 0) ? pArray->capacity : 1;

	while (newCapacity < requestedCapacity)
	{
		unsigned int prevNewCapacity = newCapacity;
		newCapacity = static_cast<unsigned int>(newCapacity * da::s_growthFactor + 1.0f);       // + 1 to force round up

		// Overflow check... probably overkill

		if (newCapacity < prevNewCapacity)
		{
			newCapacity = ~0u;
		}
	}

	pArray->pBuffer = static_cast<T *>(realloc(pArray->pBuffer, newCapacity * sizeof(T)));
    ALS_COMMON_ARRAY_Assert(pArray->pBuffer);

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
void appendMultiple(DynamicArray<T> * pArray, const T aT[], int cT)
{
	ensureCapacity(pArray, pArray->cItem + cT);

	for (int i = 0; i < cT; i++)
	{
		pArray->pBuffer[pArray->cItem] = aT[i];
		pArray->cItem++;
	}
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

	memmove(pDst, pSrc, cItemShift * sizeof(T));

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


// Fixed array (not thread safe)

template <typename T, int CAPACITY>
struct FixedArray
{
    T aBuffer[CAPACITY] = { 0 };
	unsigned int cItem = 0;

	static constexpr int s_capacity = CAPACITY;

	const T& operator[] (unsigned int i) const
	{
		ALS_COMMON_ARRAY_Assert(i < this->cItem);
		return this->aBuffer[i];
	}

	T& operator[] (unsigned int i)
	{
		ALS_COMMON_ARRAY_Assert(i < this->cItem);
		return this->aBuffer[i];
	}
};

template <typename T, int CAPACITY>
void append(FixedArray<T, CAPACITY> * pArray, const T & item)
{
	ALS_COMMON_ARRAY_Assert(pArray->cItem < pArray->s_capacity);
	pArray->aBuffer[pArray->cItem] = item;
	pArray->cItem++;
}



// Dynamic stack, wrapper around DynamicArray (not thread safe)

template <typename T>
struct Stack
{
	DynamicArray<T> a;
};

template<typename T>
void init(Stack<T> * pStack)
{
	init(&pStack->a);
}

template<typename T>
void destroy(Stack<T> * pStack)
{
	destroy(&pStack->a);
}

template<typename T>
void push(Stack<T> * pStack, const T & item)
{
	append(&pStack->a, item);
}

template<typename T>
bool isEmpty(Stack<T> * pStack)
{
	return pStack->a.cItem == 0;
}

template<typename T>
bool peek(Stack<T> * pStack, T * poItem)
{
	if (isEmpty(pStack)) return false;

	*poItem = pStack->a[pStack->a.cItem - 1];
	return true;
}

template<typename T>
bool pop(Stack<T> * pStack, T * poItem)
{
	if (isEmpty(pStack)) return false;

	*poItem = pStack->a[pStack->a.cItem - 1];
	pStack->a.cItem--;
	return true;
}