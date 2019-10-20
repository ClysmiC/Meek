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

namespace _Als_Helper
{
    inline constexpr int max(int a, int b)
    {
        return a > b ? a : b;
    }
}

// General purpose fixed-capacity pool allocator

template <typename T, unsigned int Capacity>
struct FixedPoolAllocator
{
    // Free list is doubly linked list embedded inside of unallocated slots.

    struct FreeListNode
    {
        FreeListNode * pNext = nullptr;
    };

    ALS_COMMON_ALLOC_StaticAssert(sizeof(T) >= sizeof(FreeListNode));        // Pool allocated type must be >= 2 * size of a pointer so that unallocated slots may maintain a doubly linked free list

    static constexpr unsigned int s_capacity = Capacity;

    T aPool[Capacity];
	FreeListNode * pFree = reinterpret_cast<FreeListNode *>(aPool);
};

template <typename T, unsigned int Capacity>
void reinit(FixedPoolAllocator<T, Capacity> * pAlloc)
{
	init(pAlloc);
}

template <typename T, unsigned int Capacity>
void init(FixedPoolAllocator<T, Capacity> * pAlloc)
{
    typedef FixedPoolAllocator<T, Capacity> fpa;

    pAlloc->pFree = reinterpret_cast<fpa::FreeListNode *>(pAlloc->aPool);

	for (unsigned int i = 0; i < Capacity; i++)
	{
		fpa::FreeListNode * pFreeNode = reinterpret_cast<fpa::FreeListNode *>(pAlloc->aPool + i);
		fpa::FreeListNode * pFreeNodeNext = (i < Capacity - 1) ? reinterpret_cast<fpa::FreeListNode *>(pAlloc->aPool + i + 1) : nullptr;

        pFreeNode->pNext = pFreeNodeNext;
	}
}

// Can only allocate 1 item at a time, as we can't guarantee multiple adjacent slots
//  even exist due to the nature of a pool allocator.

template <typename T, unsigned int Capacity>
T * allocate(FixedPoolAllocator<T, Capacity> * pAlloc)
{
    T * result = nullptr;

    if (pAlloc->pFree)
    {
        // Store result

        result = reinterpret_cast<T *>(pAlloc->pFree);

        // Zero initialize result
        // NOTE: It kinda sucks that this is required for the way that I am using this allocator in
        //  my compiler. Since it is allocating a struct with union members there is no real good
        //  way to initialize the one that I care about. So as a rule of thumb I will make it that
        //  anything that doesn't work when zero-initialized should require an init(..) function.

        memset(result, 0, sizeof(T));

        // Advance the free-list

        pAlloc->pFree = pAlloc->pFree->pNext;
    }

    return result;
}

template <typename T, unsigned int Capacity>
void release(FixedPoolAllocator<T, Capacity> * pAlloc, T * pItem)
{
    typedef FixedPoolAllocator<T, Capacity> fpa;

	// Detect releasing item that isn't aligned in a slot

	char * pBytePool = reinterpret_cast<char *>(pAlloc->aPool);
	char * pByteItem = reinterpret_cast<char *>(pItem);
	if ((pByteItem - pBytePool) % sizeof(T) != 0)
	{
		ALS_COMMON_ALLOC_Assert(false);

		// User has a bug in their program. Don't actually free anything so that we won't
		//	cause the bug to manifest as valid data changing under their nose. Insetad, we
		//	assert in debug and silently fail in release, in which case the bug manifests
		//	itself as their fixed pool allocator getting clogged up even though they are
		//	"""releasing""" items.

		return;
	}

#ifdef ALS_DEBUG
	{

        // Detect releasing item that is already on the free list. For this, we assert
		//	so that hopefully the user can catch this bug during development. In release,
		//	this is a silent failure which will result in the free list becoming a cycle,
		//	meaning future allocations in this buggy state may clobber data from older
		//	ones. This is pretty nasty, but if we wanted to perform this check quickly
		//	in release we would need to add more bookkeeping. Maybe that would be worth
		//	it, but for now I am fine with this behavior.

        fpa::FreeListNode * pFreeNode = pAlloc->pFree;
        while (pFreeNode)
        {
            ALS_COMMON_ALLOC_Assert(pFreeNode != reinterpret_cast<fpa::FreeListNode *>(pItem));
            pFreeNode = pFreeNode->pNext;
        }
    }
#endif

    fpa::FreeListNode * pFreeNewHead = reinterpret_cast<fpa::FreeListNode *>(pItem);
    pFreeNewHead->pNext = pAlloc->pFree;
    pAlloc->pFree = pFreeNewHead;
}

#if defined(ALS_DEBUG) && 1
template <typename T, unsigned int Capacity>
unsigned int debug_countAllocated(FixedPoolAllocator<T, Capacity> * pAlloc)
{
    typedef FixedPoolAllocator<T, Capacity> fpa;

    unsigned int countFree = 0;
    fpa::FreeListNode * pFreeNode = pAlloc->pFree;

    while (pFreeNode)
    {
        pFreeNode = pFreeNode->pNext;
        countFree++;
    }

    ALS_COMMON_ALLOC_Assert(countFree <= Capacity);
    return Capacity - countFree;
}

template <typename T, unsigned int Capacity>
bool debug_hasCycle(FixedPoolAllocator<T, Capacity> * pAlloc)
{
	typedef FixedPoolAllocator<T, Capacity> fpa;

	if (!pAlloc->pFree || !pAlloc->pFree->pNext) return false;

	fpa::FreeListNode * pTortoise	= pAlloc->pFree;
	fpa::FreeListNode * pHare		= pAlloc->pFree->pNext;

	while (true)
	{
		if (pTortoise == pHare) return true;

		pTortoise = pTortoise->pNext;

		// Don't need to check tortoise for null since it is just re-treading hare's ground

		if (!pHare->pNext || !pHare->pNext->pNext)	return false;

		pHare = pHare->pNext->pNext;
	}
}
#endif



// General purpose dynamic-capacity pool allocator. Uses a linked list of
//  fixed pool allocators underneath the hood

// TODO: Come up with a better bookkeeping strategy to determine which bucket a released pointer comes from.
//  Currently, we iterate over every bucket and check the memory address ranges to find the right one.
//  To make this cheaper, we only do it after a bunch of items have been released and we do it all in a big
//  batch. To make this batch faster, we need to keep the bucket list sorted by address, and also sort
//  all of the arrays in the batch by address. We only pay these sorting costs when we allocate a new bucket
//  or do a batch release so it isn't a *huge* deal, but surely there must be a better way!

template <typename T, unsigned int CapacityPerBucket=512>
struct DynamicPoolAllocator
{
    struct Bucket
    {
        FixedPoolAllocator<T, CapacityPerBucket> alloc;
		Bucket * pNextBucket = nullptr;     // List of all buckets that we own. Sorted by memory address, low to high
		Bucket * pNextFree = nullptr;       // Only valid for bucket on the free list
    };

    Bucket * pBuckets = nullptr;     // Linked list of all buckets
    Bucket * pFree = nullptr;        // Linked list of just buckets w/ capacity

    // We need to traverse the linked list to know which bucket the released
    //  item is in. Batch this operation to make it happen infrequently

	static constexpr int s_cRecentlyReleasedMax = _Als_Helper::max(16, CapacityPerBucket / 4);

	T * aRecentlyReleased[s_cRecentlyReleasedMax];
	int cRecentlyReleased = 0;
};

template <typename T, unsigned int CapacityPerBucket>
void init(DynamicPoolAllocator<T, CapacityPerBucket> * pAlloc)
{
	typedef DynamicPoolAllocator<T, CapacityPerBucket> dpa;

	pAlloc->pFree = nullptr;
}

template <typename T, unsigned int CapacityPerBucket>
void destroy(DynamicPoolAllocator<T, CapacityPerBucket> * pAlloc)
{
	// Free all buckets

	Bucket * pBucket = pAlloc->pBuckets;
	while (pBucket)
	{
		Bucket * pBucketNext = pBucket->pNextBucket;
		delete pBucket;
		pBucket = pBucketNext;
	}
}

template <typename T, unsigned int CapacityPerBucket>
T * allocate(DynamicPoolAllocator<T, CapacityPerBucket> * pAlloc)
{
	typedef DynamicPoolAllocator<T, CapacityPerBucket> dpa;

	T * result = nullptr;

	if (!pAlloc->pFree)
	{
		dpa::Bucket * pBucketNew = new dpa::Bucket;
        init(&pBucketNew->alloc);

		// Insert new bucket into bucket list maintaining sort order

        if (!pAlloc->pBuckets)
        {
            pAlloc->pBuckets = pBucketNew;
        }
        else
        {
            dpa::Bucket * pBucketPrev = nullptr;
            dpa::Bucket * pBucket = pAlloc->pBuckets;
            ALS_COMMON_ALLOC_Assert(pBucket);

            bool inserted = false;
            while (true)
            {
                if (!pBucket)
                {
                    // We are the new tail

                    ALS_COMMON_ALLOC_Assert(pBucketPrev);

                    pBucketPrev->pNextBucket = pBucketNew;
                    pBucketNew->pNextBucket = nullptr;

                    break;
                }
                else if (pBucketNew < pBucket)
                {
                    pBucketNew->pNextBucket = pBucket;

                    if (pBucketPrev)
                    {
                        // We are somewhere in the middle

                        pBucketPrev->pNextBucket = pBucketNew;
                    }
                    else
                    {
                        // We are the new head

                        pAlloc->pBuckets = pBucketNew;
                    }

                    break;
                }
                else
                {
                    // Still searching...

                    pBucketPrev = pBucket;
                    pBucket = pBucket->pNextBucket;
                }
            }
        }

		// Add to free list

		pAlloc->pFree = pBucketNew;
	}

	ALS_COMMON_ALLOC_Verify(result = allocate(&pAlloc->pFree->alloc));

	bool isBucketFull = !pAlloc->pFree->alloc.pFree;
	if (isBucketFull)
	{
		pAlloc->pFree = pAlloc->pFree->pNextFree;
	}

    return result;
}

template <typename T, unsigned int CapacityPerBucket = 512>
void release(DynamicPoolAllocator<T, CapacityPerBucket> * pAlloc, T * pItem, bool forceFlushBatch=false)
{
	typedef DynamicPoolAllocator<T, CapacityPerBucket> dpa;

	ALS_COMMON_ALLOC_Verify(pAlloc->cRecentlyReleased < dpa::s_cRecentlyReleasedMax);
	pAlloc->aRecentlyReleased[pAlloc->cRecentlyReleased] = pItem;
	pAlloc->cRecentlyReleased++;

	// Batch a bunch of releases at once to reduce the number of times we have to walk
    //  the list of buckets

	if (pAlloc->cRecentlyReleased == dpa::s_cRecentlyReleasedMax || forceFlushBatch)
	{
		// TODO: Faster sort algorithm. For now just use bubble sort

        for (int i = 0; i < pAlloc->cRecentlyReleased - 1; i++)
        {
            bool didSwap = false;

            for (int j = i + 1; j < pAlloc->cRecentlyReleased; j++)
            {
                T ** pp0 = &pAlloc->aRecentlyReleased[j - 1];
                T ** pp1 = &pAlloc->aRecentlyReleased[j];

                if (*pp0 > *pp1)
                {
                    T * pTemp = *pp0;
                    *pp0 = *pp1;
                    *pp1 = pTemp;
                    didSwap = true;
                }
            }

            if (!didSwap) break;
        }

		// Iterate through all buckets

		int cItemsReleased = 0;
		dpa::Bucket * pBucket = pAlloc->pBuckets;
		while (pBucket && cItemsReleased < dpa::s_cRecentlyReleasedMax)
		{
            ALS_COMMON_ALLOC_Assert(!pBucket->pNextBucket || pBucket < pBucket->pNextBucket);     // Assert that buckets are sorted low to high

			bool isBucketFull = !pBucket->alloc.pFree;
			bool releasedItem = false;

			T * pFpaStart = pBucket->alloc.aPool;
			T * pFpaEnd = pBucket->alloc.aPool + CapacityPerBucket;

			T * pItem =
				(cItemsReleased < dpa::s_cRecentlyReleasedMax) ?
				pAlloc->aRecentlyReleased[cItemsReleased] :
				nullptr;

            // Release all items that are in this bucket

			while (pItem >= pFpaStart && pItem < pFpaEnd)
			{
				ALS_COMMON_ALLOC_Assert(pItem);

				release(&pBucket->alloc, pItem);

				releasedItem = true;
				cItemsReleased++;

				pItem =
					(cItemsReleased < dpa::s_cRecentlyReleasedMax) ?
					pAlloc->aRecentlyReleased[cItemsReleased] :
					nullptr;
			}

			// Move bucket into free-list if necessary

			if (isBucketFull && releasedItem)
			{
				pBucket->pNextFree = pAlloc->pFree;
				pAlloc->pFree = pBucket;
			}

			pBucket = pBucket->pNextBucket;
		}

        ALS_COMMON_ALLOC_Assert(cItemsReleased == pAlloc->cRecentlyReleased);
        pAlloc->cRecentlyReleased = 0;
	}
}

template <typename T, unsigned int CapacityPerBucket = 512>
void reinit(DynamicPoolAllocator<T, CapacityPerBucket> * pAlloc)
{
	typedef DynamicPoolAllocator<T, CapacityPerBucket> dpa;

	// Free all of our buckets

	destroy(pAlloc);

	// Reinitialize

	memset(pAlloc, 0, sizeof(dpa));
	init(pAlloc);
}

#if defined(ALS_DEBUG) && 1
template <typename T, unsigned int CapacityPerBucket>
bool debug_hasCycle(DynamicPoolAllocator<T, CapacityPerBucket> * pAlloc)
{
	typedef DynamicPoolAllocator<T, CapacityPerBucket> dpa;

	bool freeEndReached = false;
	bool bucketEndReached = false;

	if (!pAlloc->pFree || !pAlloc->pFree->pNextFree)			freeEndReached = true;
	if (!pAlloc->pBuckets || !pAlloc->pBuckets->pNextBucket)	bucketEndReached = true;

	dpa::Bucket * pTortoiseFree	    = (!freeEndReached) ? pAlloc->pFree : nullptr;
	dpa::Bucket * pHareFree         = (!freeEndReached) ? pAlloc->pFree->pNextFree : nullptr;

	dpa::Bucket * pTortoiseBucket	= (!bucketEndReached) ? pAlloc->pBuckets : nullptr;
	dpa::Bucket * pHareBucket       = (!bucketEndReached) ? pAlloc->pBuckets->pNextBucket : nullptr;

	while (!freeEndReached || !bucketEndReached)
	{
		if (!freeEndReached && pTortoiseFree == pHareFree) return true;
		if (!bucketEndReached && pTortoiseBucket == pHareBucket) return true;

		pTortoiseBucket = pTortoiseBucket->pNextBucket;

		if (!freeEndReached)
		{
		    pTortoiseFree = pTortoiseFree->pNextFree;
            if (!pHareFree->pNextFree || !pHareFree->pNextFree->pNextFree)
            {
                freeEndReached = true;;
            }
		    else
		    {
			    pHareFree = pHareFree->pNextFree->pNextFree;
		    }
		}

        if (!bucketEndReached)
        {
            pTortoiseBucket = pTortoiseBucket->pNextBucket;
            if (!pHareBucket->pNextBucket || !pHareBucket->pNextBucket->pNextBucket)
            {
                bucketEndReached = true;;
            }
            else
            {
                pHareBucket = pHareBucket->pNextBucket->pNextBucket;
            }
        }
	}

	return false;
}
#endif
