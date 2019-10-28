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

// FNV-1a : http://www.isthe.com/chongo/tech/comp/fnv/

inline unsigned int buildHash(const void * pBytes, int cBytes, unsigned int runningHash)
{
	unsigned int result = runningHash;
	const static unsigned int s_fnvPrime = 16777619;

	for (int i = 0; i < cBytes; i++)
	{
		result ^= static_cast<const char*>(pBytes)[i];
		result *= s_fnvPrime;
	}

	return result;
}

inline unsigned int buildHashCString(const char * cString, unsigned int runningHash)
{
	unsigned int result = runningHash;
	const static unsigned int s_fnvPrime = 16777619;

	while (*cString)
	{
		result ^= *cString;
		result *= s_fnvPrime;
		cString++;
	}

	return result;
}

inline unsigned int startHash(const void * pBytes=nullptr, int cBytes=0)
{
	const static unsigned int s_fnvOffsetBasis = 2166136261;
	return buildHash(pBytes, cBytes, s_fnvOffsetBasis);
}

inline unsigned int startHashCString(const char * cString)
{
	const static unsigned int s_fnvOffsetBasis = 2166136261;
	return buildHashCString(cString, s_fnvOffsetBasis);
}

inline unsigned int combineHash(unsigned int hash0, unsigned int hash1)
{
	return hash0 ^ 37 * hash1;
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
	int32_t cCapacity;

	uint32_t (*hashFn)(const K & key);
	bool (*equalFn)(const K & key0, const K & key1);
};

template <typename K, typename V>
struct HashMapIter
{
	const HashMap<K, V> * pHashmap;
	int iItem;

	const K * pKey;
	V * pValue;
};

template <typename K, typename V>
void iterNext(HashMapIter<K, V> * pIter)
{
	typedef HashMap<K, V> hm;

	bool hadNext = false;
	for (pIter->iItem += 1; pIter->iItem < pIter->pHashmap->cCapacity; pIter->iItem += 1)
	{
		hm::Bucket * pBucket = pIter->pHashmap->pBuffer + pIter->iItem;
		if (pBucket->infoBits & AlsHash::s_infoOccupiedMask)
		{
			pIter->pKey = &pBucket->key;
			pIter->pValue = &pBucket->value;
			hadNext = true;
			break;
		}
	}

	if (!hadNext)
	{
		pIter->pKey = nullptr;
		pIter->pValue = nullptr;
	}
}

template <typename K, typename V>
HashMapIter<K, V> iter(const HashMap<K, V> & hashmap)
{
	HashMapIter<K, V> it;
	it.pHashmap = &hashmap;
	it.iItem = -1;

	iterNext(&it);

	return it;
}


template <typename K, typename V>
V * insertNew(
	HashMap<K, V> * pHashmap,
	const K & key,
	K ** ppoKeyInTable=nullptr)		// Only really useful for BiHashMap... don't set this in most cases!
{
	typedef HashMap<K, V> hm;

	ALS_COMMON_HASH_Assert(pHashmap->pBuffer);

	// H is the neighborhood size

	const static uint32_t H = AlsHash::s_neighborhoodSize;

	uint32_t hash = pHashmap->hashFn(key);
	uint32_t hashMsbs = (hash >> 24) & AlsHash::s_infoHashMsbs;

	// i is the ideal index for the key

    ALS_COMMON_HASH_Assert(
        pHashmap->cCapacity != 0 &&
        (pHashmap->cCapacity & (pHashmap->cCapacity - 1)) == 0);  // Test that it is a power of 2

    // Mask out lower bits instead of modulus since we know capacity is power of 2

	uint32_t i = hash & (pHashmap->cCapacity - 1);

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
			// Check if this key is already in our table. If it is, just return the existing value

			if ((pBucket->infoBits & AlsHash::s_infoHashMsbs) == hashMsbs && pHashmap->equalFn(key, pBucket->key))
			{
				return &pBucket->value;
			}
		}

		if (j - i > H * 2)
		{
			// We looked way past neighborhood and still can't find an empty slot.
			//	Regrow/rehash.

			// Double in size!

			growHashmap(pHashmap, (pHashmap->cCapacity << 1));
			return insertNew(pHashmap, key);
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
			return insertNew(pHashmap, key);
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

	if (ppoKeyInTable)
		*ppoKeyInTable = &pBucketEmpty->key;

	return &pBucketEmpty->value;
}

template <typename K, typename V>
void insert(
	HashMap<K, V> * pHashmap,
	const K & key,
	const V & value,
	K ** ppoKeyInTable=nullptr)		// Only really useful for BiHashMap... don't set this in most cases!
{
	V * valueNew = insertNew(pHashmap, key, ppoKeyInTable);
	*valueNew = value;
}

enum _ALSHASHOPK
{
	_ALSHASHOPK_Lookup,
	_ALSHASHOPK_Update,
	_ALSHASHOPK_Remove
};

template <typename K, typename V>
inline bool _alsHashHelper(
	HashMap<K, V> * pHashmap,
	const K & key,
	_ALSHASHOPK hashopk,
	const V * pValueNew=nullptr,
	V ** ppoValueLookup=nullptr,
	V * poValueRemoved=nullptr,
	K ** ppoKeyFound=nullptr)		// Only really useful for BiHashMap... don't set this in most cases!
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
			if (ppoKeyFound)
			{
				*ppoKeyFound = &pBucketCandidate->key;
			}

			switch (hashopk)
			{
				case _ALSHASHOPK_Lookup:
				{
					if (ppoValueLookup)
					{
						*ppoValueLookup = &pBucketCandidate->value;
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
					if (poValueRemoved)
					{
						*poValueRemoved = pBucketCandidate->value;
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
V * lookup(
	const HashMap<K, V> & pHashmap,
	const K & key,
	K ** ppoKeyFound=nullptr)		// Only really useful for BiHashMap... don't set this in most cases!
{
	V * pResult = nullptr;

	_alsHashHelper(
		const_cast<HashMap<K, V> *>(&pHashmap),   // Helper is a more general function so the parameter can't be const, but the lookup code path maintains constness
		key,
		_ALSHASHOPK_Lookup,
		static_cast<const V*>(nullptr),		// Update
		&pResult,							// Lookup
		static_cast<V *>(nullptr),			// Remove
		ppoKeyFound							// Address of found key
	);

	return pResult;
}

template <typename K, typename V>
bool remove(
	HashMap<K, V> * pHashmap,
	const K & key,
	V * poValueRemoved=nullptr)
{
	return _alsHashHelper(
		pHashmap,
		key,
		_ALSHASHOPK_Remove,
		static_cast<const V*>(nullptr),	// Update
		static_cast<V*>(nullptr),		// Lookup
		poValueRemoved,					// Remove
		static_cast<K **>(nullptr)		// Address of found key
	);
}

template <typename K, typename V>
bool update(
	HashMap<K, V> * pHashmap,
	const K & key,
	const V & value)
{
	return _alsHashHelper(
		pHashmap,
		key,
		_ALSHASHOPK_Update,
		&value,						// Update
		static_cast<V*>(nullptr),	// Lookup
		static_cast<V*>(nullptr)	// Remove
		static_cast<K **>(nullptr)	// Address of found key
	);
}

template <typename K, typename V>
void growHashmap(
	HashMap<K, V> * pHashmap,
	int newCapacity)
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
	pHashmap->cCapacity = 0;
	pHashmap->hashFn = hashFn;
	pHashmap->equalFn = equalFn;

	growHashmap(pHashmap, startingCapacity);
}

template <typename K, typename V>
void dispose(HashMap<K, V> * pHashmap)
{
	if (pHashmap->pBuffer) free(pHashmap->pBuffer);
	pHashmap->pBuffer = nullptr;
}


// Bi-directional hashmap where all keys are unique and all values are unique.

template <typename K, typename V>
struct BiHashMap
{
	HashMap<K, V*> mapKV;
	HashMap<V, K*> mapVK;
};

template <typename K, typename V>
void init(
	BiHashMap<K, V> * pBimap,
	uint32_t (*keyHashFn)(const K & key),
	bool (*keyEqualFn)(const K & key0, const K & key1),
	uint32_t (*valueHashFn)(const V & value),
	bool (*valueEqualFn)(const V & value0, const V & value1))
{
	init(
		&pBimap->mapKV,
		keyHashFn,
		keyEqualFn
	);

	init(
		&pBimap->mapVK,
		valueHashFn,
		valueEqualFn
	);
}

template <typename K, typename V>
void dispose(BiHashMap<K, V> * pBimap)
{
	dispose(&pBimap->mapKV);
	dispose(&pBimap->mapVK);
}

template <typename K, typename V>
bool insert(BiHashMap<K, V> * pBimap, const K & key, const V & value)
{
	// SLOW: (kinda)... could write a version of insert(..) or change its semantics
	//	so that we don't do an update if the item is already in the table.
	//	Maybe have insert which never does updates and then have "upsert" ?

	if (lookup(pBimap->mapKV, key)) return false;

	ALS_COMMON_HASH_Assert(!lookup(pBimap->mapVK, value));

	K * pKeyInTable;
	V * pValueInTable;

	V ** ppV = insertNew(&pBimap->mapKV, key, &pKeyInTable);
	K ** ppK = insertNew(&pBimap->mapVK, value, &pValueInTable);

	*ppK = pKeyInTable;
	*ppV = pValueInTable;

    return true;
}

template <typename K, typename V>
const V * lookupByKey(const BiHashMap<K, V> & bimap, const K & key)
{
	V ** ppResult = lookup(bimap.mapKV, key);

    if (ppResult) return *ppResult;

    return nullptr;
}

template <typename K, typename V>
const K * lookupByValue(BiHashMap<K, V> & bimap, const V & value)
{
    K ** ppResult = lookup(bimap.mapVK, value);

    if (ppResult) return *ppResult;

    return nullptr;
}

template <typename K, typename V>
bool removeByKey(BiHashMap<K, V> * pBimap, const K & key, V * poValueRemoved=nullptr)
{
	V * pValueInTable;

	bool result = remove(&pBimap->mapKV, key, &pValueInTable);

	if (!result) return false;

	// Copy actual value before we remove it from the other table

	if (poValueRemoved)
		*poValueRemoved = *pValueInTable;


	// Could optimize this since we already have a pointer to the value we want to remove. But that isn't
	//	easily exposed in the hash table API

	ALS_COMMON_HASH_Verify(remove(&pBimap->mapVK, *pValueInTable));

	return true;
}

template <typename K, typename V>
bool removeByValue(BiHashMap<K, V> * pBimap, const V & value, K * poKeyRemoved=nullptr)
{
	K * pKeyInTable;

	bool result = remove(&pBimap->mapVK, value, &pKeyInTable);

	if (!result) return false;

	// Copy actual key before we remove it from the other table

	if (poKeyRemoved)
		*poKeyRemoved = *pKeyInTable;


	// Could optimize this since we already have a pointer to the key we want to remove. But that isn't
	//	easily exposed in the hash table API

	ALS_COMMON_HASH_Verify(remove(&pBimap->mapKV, *pKeyInTable));

	return true;
}

template <typename K, typename V>
bool updateByKey(BiHashMap<K, V> * pBimap, const K & key, const V & value)
{
	K * pKeyInTable;
	V * pValueInTableOld = lookup(&pBimap->mapKV, key, &pKeyInTable);
	if (!pValueInTableOld) return false;

	ALS_COMMON_HASH_Verify(
		remove(&pBimap->mapVK, *pValueInTableOld)
	);

	V * pValueInTableNew;
	insert(&pBimap->mapVK, value, pKeyInTable, &pValueInTableNew);

	ALS_COMMON_HASH_Verify(
		update(&pBimap->mapKV, key, pValueInTableNew)
	);

	return true;
}

template <typename K, typename V>
bool updateByValue(BiHashMap<K, V> * pBimap, const V & value, const K & key)
{
	V * pValueInTable;
	K * pKeyInTableOld = lookup(&pBimap->mapVK, value, &pValueInTable);
	if (!pKeyInTableOld) return false;

	ALS_COMMON_HASH_Verify(
		remove(&pBimap->mapKV, *pKeyInTableOld)
	);

	K * pKeyInTableNew;
	insert(&pBimap->mapKV, key, pValueInTable, &pKeyInTableNew);

	ALS_COMMON_HASH_Verify(
		update(&pBimap->mapVK, value, pKeyInTableNew)
	);

	return true;
}

#undef ALS_COMMON_HASH_StaticAssert
#undef ALS_COMMON_HASH_Assert
#undef ALS_COMMON_HASH_Verify
