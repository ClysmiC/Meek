#pragma once

#include "macro_assert.h"

// Ring Buffer (not thread safe)

template <typename T, unsigned int Capacity>
struct RingBuffer
{
	static constexpr unsigned int cCapacity = Capacity;

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
bool write(RingBuffer<T, Capacity> * pRbuf, T * pT)
{
	if (pRbuf->cItem >= Capacity)
	{
		Assert(false);
		return false;
	}

	unsigned int iBufferWrite = (pRbuf->iBufferRead + pRbuf->cItem) % Capacity;
    pRbuf->buffer[iBufferWrite] = *pT;

    pRbuf->cItem++;

    return true;
}

template <typename T, unsigned int Capacity>
void forceWrite(RingBuffer<T, Capacity> * pRbuf, T * pT)
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
		pRbuf->buffer[iBufferWrite] = *pT;

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
