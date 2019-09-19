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

	const static uint8_t s_infoOffsetMask		= 0b00001111;

	// 3 Most significant bits of the hash of the entry in this bucket. If these don't match a candidate key's hash,
	//	no need to test the keys for equality.

	const static uint8_t s_infoHashFingerprint	= 0b01110000;

	// 1 bit indicates whether the bucket is full or empty

	const static uint8_t s_infoOccupiedMask		= 0b10000000;

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

	Bucket * pBuffer = nullptr;
	unsigned int cCapacity = 0;
	unsigned int cItem = 0;

	uint32_t (*hashFn)(const K & key);
	bool (*equalFn)(const K & key0, const K & key1);
};

template <typename K, typename V>
void insert(
	HashMap<K, V> * pHashmap,
	const K & key,
	const V & value)
{
	// TODO: What do we do if we insert an existing key?? Just update the value?
	//	Or should this be "tryInsert" ???

	typedef HashMap<K, V> hm;

	ALS_COMMON_HASH_Assert(pHashmap->pBuffer);

	const static uint32_t s_hMinus1 = AlsHash::s_neighborhoodSize - 1;

	uint32_t hash = pHashmap->hashFn(key);

	// i is the ideal index for the key

	uint32_t i = hash % pHashmap->cCapacity;
	hm::Bucket * pBucketIdeal = pHashmap->pBuffer + i;

	// j is the pre-modulo index of the empty bucket slot we are tracking

	uint32_t j = i - 1;
	hm::Bucket * pBucketEmpty;

	// Linear probe to the first empty bucket

	do
	{
		j++;
		pBucketEmpty = pHashmap->pBuffer + (j % pHashmap->cCapacity);
	} while(pBucketEmpty->isOccupied);

	// If the empty bucket is too far away from the ideal index, find an
	//	"earlier" bucket that is allowed to move into the empty bucket, and
	//	use the "earlier" bucket's previous position as our new empty bucket.
	//	Repeat as many times as necessary.

	while (j - i > s_hMinus1)
	{
		hm::Bucket * pBucketOther = pHashmap->pBuffer + ((j - s_hMinus1) % pHashmap->cCapacity);

		if (pBucketOther->hopInfo == 0)
		{
			// TODO: if the hop info is 0 I should probably check the *next* bucket... there is a real chance
			//	that there are things in this bucket that have an "ideal" bucket that is the *next* one...
			//	if that is the case then there is no need to resize...

            ALS_COMMON_HASH_Assert(false);

			// TODO: Resize and rehash the array;
			// call growArray, but do some research about what a good growth factor is!

			// Oh and also make sure that we insert the pending value after we grow the
			//	array!
		}

		const static int s_cBitHopInfo = 32;

		ALS_COMMON_HASH_StaticAssert(s_cBitHopInfo == (HashMap<bool, bool>::Bucket*)->hopInfo * 8);

		// TODO: faster way to do this??
		// https://en.wikipedia.org/wiki/Find_first_set

		int iBitSet;
		for (iBitSet = 0; iBitSet < s_cBitHopInfo; iBitSet++)
		{
			if (pBucketOther->hopInfo & (1 << iBitSet)) break;
		}

		// Copy the swap candidate into the empty bucket we found
        // NOTE: Do not update the bit vector, since that is metadata associated
        //  with the bucket itself... not with the values inside it.

		hm::Bucket * pBucketSwappable = pBucketOther + iBitSet;

        pBucketEmpty->isOccupied = true;
        pBucketEmpty->key = pBucketSwappable->key;
        pBucketEmpty->value = pBucketSwappable->value;

		// Update pointer and index of empty bucket

		pBucketEmpty = pBucketSwappable;
		j = j - s_hMinus1 + iBitSet;

		// Update bit vector

		pBucketOther->hopInfo &= ~(1 << iBitSet);
		pBucketOther->hopInfo |= (1 << s_hMinus1);
	}

	pBucketEmpty->isOccupied = true;
	pBucketEmpty->key = key;
	pBucketEmpty->value = value;

	pBucketIdeal->hopInfo |= (1 << (j - i));

	// TODO: hop info bit vector stuff
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

			insert(pHashmap, pBucket->key, pBucket->value);
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
	if (startingCapacity < AlsHash::s_neighborhoodSize)
	{
		// Life is a lot easier for us if we have this guarantee...

		startingCapacity = AlsHash::s_neighborhoodSize;
	}

	pHashmap->cItem = 0;
	pHashmap->cCapacity = 0;
	pHashmap->hashFn = hashFn;
	pHashmap->equalFn = equalFn;

	// TODO: Allocate buffer and set capacity
	// pHashmap->cCapacity =
	// pHashmap->pBuffer =

	growHashmap(pHashmap, startingCapacity);
}

// Hop info bit vector must be able to point to each member in a neighborhood!

ALS_COMMON_HASH_StaticAssert(sizeof((HashMap<bool, bool>::Bucket*)->hopInfo) * 8 >= AlsHash::s_neighborhoodSize);

#undef ALS_COMMON_HASH_StaticAssert
#undef ALS_COMMON_HASH_Assert
#undef ALS_COMMON_HASH_Verify
