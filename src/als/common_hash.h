#pragma once

#ifdef ALS_MACRO
#include "macro_assert.h"
#endif

#ifndef ALS_COMMON_HASH_StaticAssert
#ifndef ALS_DEBUG
#define ALS_COMMON_HASH_StaticAssert(X)
#elif defined(ALS_MACRO)
#define ALS_COMMON_HASH_StaticAssert(x) StaticAssert(x)
#else
// C11 _Static_assert
#define ALS_COMMON_HASH_StaticAssert(x) _Static_assert(x)
#endif
#endif

#ifndef ALS_COMMON_HASH_Assert
#ifndef ALS_DEBUG
#define ALS_COMMON_HASH_Assert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_HASH_Assert(x) Assert(x)
#else
// C assert
#include <assert.h>
#define ALS_COMMON_HASH_Assert(x) assert(x)
#endif
#endif

#ifndef ALS_COMMON_HASH_Verify
#ifndef ALS_DEBUG
#define ALS_COMMON_HASH_Verify(x) x
#else
#define ALS_COMMON_HASH_Verify(x) ALS_COMMON_HASH_Assert(x)
#endif
#endif

#include <stdint.h>		// For uint32_t

// Hash map (not thread safe)
//	Uses hopscotch hashing: http://mcg.cs.tau.ac.il/papers/disc2008-hopscotch.pdf

namespace AlsHash
{
	const static int s_neighborhoodSize = 32;		// 'H' in the paper!

	// Offset of the entry in a bucket from it's "ideal" bucket

	const static uint8_t s_infoOffsetMask		= 0b00011111;

	// 2 Most significant bits of the hash of the entry in this bucket. If these don't match a candidate key's hash,
	//	no need to test the keys for equality (eliminates 75% of unnecessary equality checks)

	const static uint8_t s_infoHashMsbs			= 0b11000000;

	// 1 bit indicates whether the bucket is full or empty

	const static uint8_t s_infoOccupiedMask		= 0b00100000;

	ALS_COMMON_HASH_StaticAssert(s_infoOffsetMask + 1 >= s_neighborhoodSize);

}

template <typename K, typename V>
struct HashMap
{
	struct Bucket
	{
		K key;
		V value;

		uint8_t infoBits;
	};

	Bucket * pBuffer;
	uint32_t cCapacity;
	uint32_t cItem;

	uint32_t (*hashFn)(const K & key);
	bool (*equalFn)(const K & key0, const K & key1);
};

template <typename K, typename V>
void insert(
	HashMap<K, V> * pHashmap,
	const K & key,
	const V & value)
{
	typedef HashMap<K, V> hm;

	ALS_COMMON_HASH_Assert(pHashmap->pBuffer);

	// H is the neighborhood size

	const static uint32_t H = AlsHash::s_neighborhoodSize;

	uint32_t hash = pHashmap->hashFn(key);
	uint32_t hashMsbs = (hash >> 24) & AlsHash::s_infoHashMsbs;

	// i is the ideal index for the key

	uint32_t i = hash % pHashmap->cCapacity;
	hm::Bucket * pBucketIdeal = pHashmap->pBuffer + i;

	// j is the pre-modulo index of the empty bucket slot we are tracking

	uint32_t j = i;
	hm::Bucket * pBucketEmpty;

	// Linear probe to the first empty bucket.

	while (true)
	{
		hm::Bucket * pBucket = pHashmap->pBuffer + (j % pHashmap->cCapacity);

		if(!(pBucket->infoBits & AlsHash::s_infoOccupiedMask))
		{
			pBucketEmpty = pBucket;
			break;
		}
		else
		{
			// Check if this key is already in our table. If it is, just update the value and return.

			if ((pBucket->infoBits & AlsHash::s_infoHashMsbs) == hashMsbs && pHashmap->equalFn(key, pBucket->key))
			{
				pBucket->value = value;
				return;
			}
		}

		if (j - i > H * 2)
		{
			// We looked way past neighborhood and still can't find an empty slot.
			//	Regrow/rehash.

			// Double in size!

			growHashmap(pHashmap, (pHashmap->cCapacity << 1));
			insert(pHashmap, key, value);
			return;
		}

		j++;
	}

	// If the empty bucket is too far away from the ideal index, find an
	//	"earlier" bucket that is allowed to move into the empty bucket, and
	//	use the "earlier" bucket's previous position as our new empty bucket.
	//	Repeat as many times as necessary.

	while (j - i >= H)
	{
		hm::Bucket * pBucketSwappable = nullptr;

		// k is the candidate to be swapped into j

		uint32_t k = j - (H - 1);
		int kIteration = 0;
		int dOffset = 0;

		while(k < j)
		{
			hm::Bucket * pBucketCandidate = pHashmap->pBuffer + (k % pHashmap->cCapacity);

			int candidateOffset = pBucketCandidate->infoBits & AlsHash::s_infoOffsetMask;
			if (candidateOffset <= kIteration)
			{
				pBucketSwappable = pBucketCandidate;
				dOffset = j - k;
				break;
			}

			k++;
			kIteration++;
		}

		if (!pBucketSwappable)
		{
			growHashmap(pHashmap, (pHashmap->cCapacity << 1));
			insert(pHashmap, key, value);
			return;
		}

		// Copy swappable bucket into the empty bucket and update the offset

        *pBucketEmpty = *pBucketSwappable;

		int newOffset = (pBucketSwappable->infoBits & AlsHash::s_infoOffsetMask )+ dOffset;
		ALS_COMMON_HASH_Assert(newOffset < H);

		pBucketEmpty->infoBits &= ~AlsHash::s_infoOffsetMask;
		pBucketEmpty->infoBits |= newOffset;


		// Update pointer and index of empty bucket

		pBucketEmpty = pBucketSwappable;
		j = k;
	}

	int offset = j - i;
	pBucketEmpty->infoBits &= ~AlsHash::s_infoOffsetMask;
	pBucketEmpty->infoBits |= offset;

	pBucketEmpty->infoBits |= AlsHash::s_infoOccupiedMask;

	pBucketEmpty->infoBits &= ~AlsHash::s_infoHashMsbs;
	pBucketEmpty->infoBits |= hashMsbs;

	pBucketEmpty->key = key;
	pBucketEmpty->value = value;
}

enum _ALSHASHOPK
{
    _ALSHASHOPK_Lookup,
    _ALSHASHOPK_Update,
    _ALSHASHOPK_Remove
};

template <typename K, typename V>
inline bool _alsHashWorker(
	HashMap<K, V> * pHashmap,
	const K & key,
	_ALSHASHOPK hashopk,
	const V * pValueNew=nullptr,
	V * poValueLookup=nullptr)
{
	typedef HashMap<K, V> hm;

	ALS_COMMON_HASH_Assert(pHashmap->pBuffer);

	// H is the neighborhood size

	const static uint32_t H = AlsHash::s_neighborhoodSize;

	uint32_t hash = pHashmap->hashFn(key);
	uint32_t hashMsbs = (hash >> 24) & AlsHash::s_infoHashMsbs;

	// i is the ideal index for the key

	uint32_t i = hash % pHashmap->cCapacity;

	// Linear probe in the neighborhood.

	for (uint32_t iCandidate = i; iCandidate < i + H; iCandidate++)
	{
		hm::Bucket * pBucketCandidate = pHashmap->pBuffer + (iCandidate % pHashmap->cCapacity);

		// Check if candidate is occupied

		if(!(pBucketCandidate->infoBits & AlsHash::s_infoOccupiedMask))
		{
			continue;
		}

		// Check if candidate has offset we would expect

		int offset = iCandidate - i;
		if ((pBucketCandidate->infoBits & AlsHash::s_infoOffsetMask) != offset)
		{
			continue;
		}

		// Check if candidate hash matches our hash's most significant bits

		if ((pBucketCandidate->infoBits & AlsHash::s_infoHashMsbs) != hashMsbs)
		{
			continue;
		}

		// Compare the keys!

		if (pHashmap->equalFn(key, pBucketCandidate->key))
		{
			switch (hashopk)
			{
				case _ALSHASHOPK_Lookup:
				{
                    if (poValueLookup)
                    {
                        *poValueLookup = pBucketCandidate->value;
                    }

					return true;
				}

				case _ALSHASHOPK_Update:
				{
					pBucketCandidate->value = *pValueNew;
					return true;
				}

				case _ALSHASHOPK_Remove:
				{
					if (poValueLookup)
					{
						*poValueLookup = pBucketCandidate->value;
					}

					pBucketCandidate->infoBits &= ~AlsHash::s_infoOccupiedMask;
					return true;
				}

				default:
				{
					ALS_COMMON_HASH_Assert(false);
					return false;
				}
			}
		}
	}

	return false;
}

template <typename K, typename V>
bool lookup(
	HashMap<K, V> * pHashmap,
	const K & key,
	V * poValue=nullptr)
{
	return _alsHashWorker(
		pHashmap,
		key,
		_ALSHASHOPK_Lookup,
		(V*)nullptr,
		poValue
	);
}

template <typename K, typename V>
bool remove(
	HashMap<K, V> * pHashmap,
	const K & key,
	V * poValueRemoved=nullptr)
{
	return _alsHashWorker(
		pHashmap,
		key,
		_ALSHASHOPK_Remove,
		(V*)nullptr,
		poValueRemoved
	);
}

template <typename K, typename V>
bool update(
	HashMap<K, V> * pHashmap,
	const K & key,
	const V & value)
{
	return _alsHashWorker(
		pHashmap,
		key,
		_ALSHASHOPK_Update,
		&value,
		(V*)nullptr
	);
}

template <typename K, typename V>
void growHashmap(
	HashMap<K, V> * pHashmap,
	unsigned int newCapacity)
{
	typedef HashMap<K, V> hm;

	ALS_COMMON_HASH_Assert(newCapacity > pHashmap->cCapacity);

	// Store pointer to old buffer so we can copy from it

	hm::Bucket * pBufferOld = pHashmap->pBuffer;
	int cCapacityOld = pHashmap->cCapacity;

	// Create new buffer and set it and assign it to the hashmap

	unsigned int cBytesNewBuffer = newCapacity * sizeof(hm::Bucket);
	pHashmap->pBuffer = (hm::Bucket *)malloc(cBytesNewBuffer);
	memset(pHashmap->pBuffer, 0, cBytesNewBuffer);

	pHashmap->cCapacity = newCapacity;

	// Deal with old buffer!

	if (pBufferOld)
	{
		// Rehash into new buffer

		for (int i = 0; i < cCapacityOld; i++)
		{
			hm::Bucket * pBucket = pBufferOld + i;

            if (pBucket->infoBits & AlsHash::s_infoOccupiedMask)
            {
			    insert(pHashmap, pBucket->key, pBucket->value);
            }
		}

		// Free old buffer

		free(pBufferOld);
	}
}

template <typename K, typename V>
void init(
	HashMap<K, V> * pHashmap,
	uint32_t (*hashFn)(const K & key),
	bool (*equalFn)(const K & key0, const K & key1),
	unsigned int startingCapacity=AlsHash::s_neighborhoodSize)
{
	// Starting capacity can't be smaller than neighborhood size

	if (startingCapacity < AlsHash::s_neighborhoodSize)
	{
		startingCapacity = AlsHash::s_neighborhoodSize;
	}

	// Round startingCapacity to power of 2

	unsigned int power = 2;
	while (power <= startingCapacity)
		power <<= 1;

	startingCapacity = power;

	pHashmap->pBuffer = nullptr;
	pHashmap->cItem = 0;
	pHashmap->cCapacity = 0;
	pHashmap->hashFn = hashFn;
	pHashmap->equalFn = equalFn;

	growHashmap(pHashmap, startingCapacity);
}

template <typename K, typename V>
void destroy(HashMap<K, V> * pHashmap)
{
	if (pHashmap->pBuffer) free(pHashmap->pBuffer);
	pHashmap->pBuffer = nullptr;
}

#undef ALS_COMMON_HASH_StaticAssert
#undef ALS_COMMON_HASH_Assert
#undef ALS_COMMON_HASH_Verify
