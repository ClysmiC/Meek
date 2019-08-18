#pragma once

#include "common_type.h"

#include "macro_assert.h"

// Ring Buffer (not thread safe)

template <typename T, uint Capacity>
struct RingBuffer
{
	static constexpr uint cCapacity = Capacity;

    T buffer[Capacity];
	uint iBufferRead = 0;
	uint cItem = 0;
};

template <typename T, uint Capacity>
bool read(RingBuffer<T, Capacity> * pRbuf, T * poT)
{
    if (pRbuf->cItem == 0) return false;

    *poT = pRbuf->buffer[pRbuf->iBufferRead];

    pRbuf->iBufferRead++;
    pRbuf->iBufferRead %= Capacity;

    pRbuf->cItem--;

    return true;
}

template <typename T, uint Capacity>
bool write(RingBuffer<T, Capacity> * pRbuf, T * pT)
{
	if (pRbuf->cItem >= Capacity)
	{
		Assert(false);
		return false;
	}

	uint iBufferWrite = (pRbuf->iBufferRead + pRbuf->cItem) % Capacity;
    pRbuf->buffer[iBufferWrite] = *pT;

    pRbuf->cItem++;

    return true;
}

template <typename T, uint Capacity>
void forceWrite(RingBuffer<T, Capacity> * pRbuf, T * pT)
{
	if (pRBuf->cItem < Capacity)
	{
		write(pRbuf, pT);
	}
	else
	{
		// Overwrite first item in the buffer with the new one and
		//	bump the read index

		uint iBufferWrite = pRbuf->iBufferRead;
		pRbuf->buffer[iBufferWrite] = *pT;

		pRbuf->iBufferRead++;
		pRbuf->iBufferRead %= Capacity;
	}
}

template <typename T, uint Capacity>
bool peek(RingBuffer<T, Capacity> * pRbuf, uint iItem, T * poT)
{
	if (iItem >= pRbuf->cItem) return false;

	uint iBufferPeek = (pRbuf->iBufferRead + iItem) % Capacity;
	*poT = pRbuf->buffer[iBufferPeek];

	return true;
}

template <typename T, uint Capacity>
bool isEmpty(RingBuffer<T, Capacity> * pRbuf)
{
	return pRbuf->cItem != 0;
}

template <typename T, uint Capacity>
uint count(RingBuffer<T, Capacity> * pRbuf)
{
	return pRbuf->cItem;
}
