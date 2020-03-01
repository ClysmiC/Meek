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
	int cItem;
	int capacity;

	constexpr static float gc_growthFactor = 1.5f;

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
void init(DynamicArray<T> * pArray)
{
    pArray->cItem = 0;
    pArray->capacity = 0;
    pArray->pBuffer = nullptr;
}

template <typename T>
void reinit(DynamicArray<T> * pArray)
{
	dispose(pArray);
	init(pArray);
}

template <typename T>
void initMove(DynamicArray<T> * pArray, DynamicArray<T> * pArraySrc)
{
	*pArray = *pArraySrc;

	pArraySrc->pBuffer = nullptr;
	pArraySrc->cItem = 0;
	pArraySrc->capacity = 0;
}

template <typename T>
void reinitMove(DynamicArray<T> * pArray, DynamicArray<T> * pArraySrc)
{
	dispose(pArray);
	initMove(pArray, pArraySrc);
}

template <typename T>
void initCopy(DynamicArray<T> * pArray, const DynamicArray<T> & arraySrc)
{
	init(pArray);
	for (int i = 0; i < arraySrc.cItem; i++)
	{
		append(pArray, arraySrc[i]);
	}
}

template <typename T>
void reinitCopy(DynamicArray<T> * pArray, DynamicArray<T> * pArraySrc)
{
	dispose(pArray);
	initCopy(pArray, pArraySrc);
}

// These extract functions feel a bit too clever/unsafe... if they bite me once or twice I might just delete them.

template <typename T, typename E>
void initExtract(DynamicArray<T> * pArray, const DynamicArray<E> & arrayExtractee, int byteOffset)
{
	init(pArray);
	for (int i = 0; i < arrayExtractee.cItem; i++)
	{
		T * pItem = reinterpret_cast<T *>(reinterpret_cast<char *>(arrayExtractee.pBuffer + i) + byteOffset);
		append(pArray, *pItem);
	}
}

// This version dereferences the pointer then applies the byte offset...

 template <typename T, typename E>
 void initExtract(DynamicArray<T> * pArray, const DynamicArray<E *> & arrayExtractee, int byteOffset)
 {
 	init(pArray);
 	for (int i = 0; i < arrayExtractee.cItem; i++)
 	{
 		T * pItem = reinterpret_cast<T *>(reinterpret_cast<char *>(*(arrayExtractee.pBuffer + i)) + byteOffset);
 		append(pArray, *pItem);
 	}
 }

template <typename T>
void dispose(DynamicArray<T> * pArray)
{
	if (pArray->pBuffer)
	{
		if (pArray->pBuffer) free(pArray->pBuffer);
		pArray->pBuffer = nullptr;
	}
}


template <typename T>
void ensureCapacity(DynamicArray<T> * pArray, int requestedCapacity)
{
    typedef DynamicArray<T> da;

	if (requestedCapacity <= pArray->capacity) return;

    static const int s_minCapacity = 8;
	int newCapacity = (pArray->capacity > s_minCapacity) ? pArray->capacity : s_minCapacity;

	while (newCapacity < requestedCapacity)
	{
		int prevNewCapacity = newCapacity;
		newCapacity = static_cast<int>((newCapacity * da::gc_growthFactor) + 1.0f);       // + 1 to force round up

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

	T * pNew = appendNew(pArray);
	*pNew = t;
}

template <typename T>
T * appendNew(DynamicArray<T> * pArray)
{
	ensureCapacity(pArray, pArray->cItem + 1);
	pArray->cItem++;

	return &pArray->pBuffer[pArray->cItem - 1];
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
void appendMultiple(DynamicArray<T> * pArray, const DynamicArray<T> & src)
{
	ensureCapacity(pArray, pArray->cItem + src.cItem);

	for (int i = 0; i < src.cItem; i++)
	{
		pArray->pBuffer[pArray->cItem] = src.pBuffer[i];
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
int indexOf(const DynamicArray<T> & array, const T & item)
{
    for (int i = 0; i < array.cItem; i++)
    {
        if (array[i] == item) return i;
    }

    return -1;
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
void removeLast(DynamicArray<T> * pArray)
{
    if (pArray->cItem > 0) pArray->cItem--;
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
	if (pArray->cItem >= pArray->s_capacity)
	{
		ALS_COMMON_ARRAY_Assert(false);
		return;
	}

	pArray->aBuffer[pArray->cItem] = item;
	pArray->cItem++;
}

template <typename T, int CAPACITY>
void appendMultiple(FixedArray<T, CAPACITY> * pArray, const T * aItem, int cItem)
{
	for (int i = 0; i < cItem; i++)
	{
		append(pArray, aItem[i]);
	}
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
void initCopy(Stack<T> * pStack, const Stack<T> & stackSrc)
{
	init(&pStack->a);

	for (int i = 0; i < stackSrc.a.cItem; i++)
	{
		append(&pStack->a, stackSrc.a[i]);
	}
}

template<typename T>
void dispose(Stack<T> * pStack)
{
	dispose(&pStack->a);
}

template<typename T>
void push(Stack<T> * pStack, const T & item)
{
	append(&pStack->a, item);
}

template<typename T>
T * pushNew(Stack<T> * pStack)
{
	return appendNew(&pStack->a);
}

template<typename T>
bool isEmpty(const Stack<T> & stack)
{
	return stack.a.cItem == 0;
}

template<typename T>
bool contains(const Stack<T> & stack, const T & item)
{
    return indexOf(stack.a, item) != -1;
}

template<typename T>
T peekFar(const Stack<T> & stack, int peekIndex, bool * poSuccess=nullptr)
{
	T result;
	if (count(stack) <= peekIndex)
	{
        ALS_COMMON_ARRAY_Assert(false);
		if (poSuccess) *poSuccess = false;
	}
	else
	{
		// META: This requires copy constructor for the general case. Maybe have an "owned memory" notation that disallows
		//	direct assignment unless you have annotated something as the copy routine?

		if (poSuccess) *poSuccess = true;
		result = stack.a[stack.a.cItem - 1 - peekIndex];
	}

	return result;
}

template<typename T>
T * peekFarPtr(const Stack<T> & stack, int peekIndex)
{
    T * result = nullptr;
    if (count(stack) <= peekIndex)
    {
        ALS_COMMON_ARRAY_Assert(false);
    }
    else
    {
        // const ref is just a convenient way to pass argument immutably by value. I'm not modifying the value
        //  here, but I would like to let the caller modify the value through the pointer I return to them (if
        //  they so choose)... I am not really a const correct zealot so this doesn't bother me.

        result = const_cast<T*>(&stack.a[stack.a.cItem - 1 - peekIndex]);
    }

    return result;
}

template<typename T>
T peek(const Stack<T> & stack, bool * poSuccess=nullptr)
{
	return peekFar(stack, 0, poSuccess);
}

template<typename T>
T * peekPtr(const Stack<T> & stack)
{
    return peekFarPtr(stack, 0);
}

template<typename T>
T pop(Stack<T> * pStack, bool * poSuccess=nullptr)
{
    T result;

    if (isEmpty(*pStack))
    {
        ALS_COMMON_ARRAY_Assert(false);
        if (poSuccess) *poSuccess = false;
    }
    else
    {
        result = pStack->a[pStack->a.cItem - 1];
	    pStack->a.cItem--;

        if (poSuccess) *poSuccess = true;
    }

	return result;
}

template<typename T>
int count(const Stack<T> & stack)
{
	return stack.a.cItem;
}

#undef ALS_COMMON_ARRAY_StaticAssert
#undef ALS_COMMON_ARRAY_Assert
#undef ALS_COMMON_ARRAY_Verify
