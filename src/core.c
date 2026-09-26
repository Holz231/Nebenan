// SPDX-License-Identifier: MIT

#include "core.h"

#include <stdio.h>
#include <stdlib.h>

#if defined( _MSC_VER )
#include <intrin.h>
#include <malloc.h>
#endif

#define NB_ALIGNMENT 32

static nbAllocFcn* nb_allocFcn = NULL;
static nbFreeFcn* nb_freeFcn = NULL;
static int64_t nb_byteCount = 0;

// MSVC's C mode has no stdatomic.h without experimental flags, so use intrinsics.
static void nbAtomicAdd64( int64_t* value, int64_t delta )
{
#if defined( _MSC_VER )
	_InterlockedExchangeAdd64( (volatile long long*)value, delta );
#else
	__atomic_fetch_add( value, delta, __ATOMIC_RELAXED );
#endif
}

static int64_t nbAtomicLoad64( int64_t* value )
{
#if defined( _MSC_VER )
	return _InterlockedOr64( (volatile long long*)value, 0 );
#else
	return __atomic_load_n( value, __ATOMIC_RELAXED );
#endif
}

int nbAtomicFetchAddInt( int* value, int delta )
{
#if defined( _MSC_VER )
	return _InterlockedExchangeAdd( (volatile long*)value, delta );
#else
	return __atomic_fetch_add( value, delta, __ATOMIC_SEQ_CST );
#endif
}

static int nbDefaultAssertFcn( const char* condition, const char* fileName, int lineNumber )
{
	printf( "NEBENAN ASSERTION: %s, %s, line %d\n", condition, fileName, lineNumber );
	return 1;
}

static nbAssertFcn* nb_assertFcn = nbDefaultAssertFcn;

void nbSetAllocator( nbAllocFcn* allocFcn, nbFreeFcn* freeFcn )
{
	nb_allocFcn = allocFcn;
	nb_freeFcn = freeFcn;
}

void nbSetAssertFcn( nbAssertFcn* assertFcn )
{
	nb_assertFcn = assertFcn != NULL ? assertFcn : nbDefaultAssertFcn;
}

int nbInternalAssert( const char* condition, const char* fileName, int lineNumber )
{
	return nb_assertFcn( condition, fileName, lineNumber );
}

int64_t nbGetByteCount( void )
{
	return nbAtomicLoad64( &nb_byteCount );
}

nbVersion nbGetVersion( void )
{
	return (nbVersion){ 0, 1, 0 };
}

void* nbAlloc( size_t size )
{
	if ( size == 0 )
	{
		return NULL;
	}

	// Pad to a multiple of the alignment so aligned_alloc accepts the size.
	size_t alignedSize = ( ( size - 1 ) | ( NB_ALIGNMENT - 1 ) ) + 1;
	nbAtomicAdd64( &nb_byteCount, (int64_t)alignedSize );

	void* ptr;
	if ( nb_allocFcn != NULL )
	{
		ptr = nb_allocFcn( alignedSize, NB_ALIGNMENT );
	}
	else
	{
#if defined( _MSC_VER ) || defined( __MINGW32__ )
		ptr = _aligned_malloc( alignedSize, NB_ALIGNMENT );
#elif defined( __ANDROID__ )
		void* mem = NULL;
		ptr = posix_memalign( &mem, NB_ALIGNMENT, alignedSize ) == 0 ? mem : NULL;
#else
		ptr = aligned_alloc( NB_ALIGNMENT, alignedSize );
#endif
	}

	NB_ASSERT( ptr != NULL );
	return ptr;
}

void nbFree( void* mem, size_t size )
{
	if ( mem == NULL )
	{
		return;
	}

	size_t alignedSize = ( ( size - 1 ) | ( NB_ALIGNMENT - 1 ) ) + 1;

	if ( nb_freeFcn != NULL )
	{
		nb_freeFcn( mem, alignedSize );
	}
	else
	{
#if defined( _MSC_VER ) || defined( __MINGW32__ )
		_aligned_free( mem );
#else
		free( mem );
#endif
	}

	nbAtomicAdd64( &nb_byteCount, -(int64_t)alignedSize );
}

void* nbGrowAlloc( void* oldMem, size_t oldSize, size_t newSize )
{
	NB_ASSERT( newSize > oldSize );
	void* newMem = nbAlloc( newSize );
	if ( oldSize > 0 )
	{
		memcpy( newMem, oldMem, oldSize );
		nbFree( oldMem, oldSize );
	}
	return newMem;
}

struct nbArenaBlock
{
	nbArenaBlock* next;
	size_t capacity;
	size_t index;
};

static nbArenaBlock* nbArena_NewBlock( size_t capacity, nbArenaBlock* next )
{
	nbArenaBlock* block = nbAlloc( sizeof( nbArenaBlock ) + capacity );
	block->next = next;
	block->capacity = capacity;
	block->index = 0;
	return block;
}

void nbArena_Create( nbArena* arena, size_t capacity )
{
	arena->head = nbArena_NewBlock( capacity, NULL );
	arena->used = 0;
	arena->peak = 0;
}

void nbArena_Destroy( nbArena* arena )
{
	nbArenaBlock* block = arena->head;
	while ( block != NULL )
	{
		nbArenaBlock* next = block->next;
		nbFree( block, sizeof( nbArenaBlock ) + block->capacity );
		block = next;
	}
	arena->head = NULL;
	arena->used = 0;
}

void* nbArena_Alloc( nbArena* arena, size_t size )
{
	// 16 byte alignment for all scratch allocations
	size = ( size + 15 ) & ~(size_t)15;
	if ( size == 0 )
	{
		size = 16;
	}

	nbArenaBlock* block = arena->head;
	if ( block->index + size > block->capacity )
	{
		size_t capacity = 2 * block->capacity;
		if ( capacity < size )
		{
			capacity = size;
		}
		block = nbArena_NewBlock( capacity, block );
		arena->head = block;
	}

	uint8_t* base = (uint8_t*)( block + 1 );
	void* ptr = base + block->index;
	block->index += size;
	arena->used += size;
	if ( arena->used > arena->peak )
	{
		arena->peak = arena->used;
	}
	return ptr;
}

void nbArena_Reset( nbArena* arena )
{
	nbArenaBlock* block = arena->head;
	if ( block->next != NULL )
	{
		// Coalesce into one block that holds the peak usage so the next operation needs no chaining.
		size_t capacity = block->capacity;
		while ( block != NULL )
		{
			nbArenaBlock* next = block->next;
			nbFree( block, sizeof( nbArenaBlock ) + block->capacity );
			block = next;
		}
		if ( capacity < arena->peak )
		{
			capacity = arena->peak;
		}
		arena->head = nbArena_NewBlock( capacity, NULL );
	}
	else
	{
		block->index = 0;
	}
	arena->used = 0;
}

nbRandom nbMakeRandom( uint64_t seed, uint64_t stream )
{
	nbRandom rng;
	rng.state = 0;
	// The stream is folded into the state since the increment is fixed.
	rng.state = seed + nbHashSeed( stream, 0x9E3779B97F4A7C15ull );
	nbRandomU32( &rng );
	return rng;
}

uint32_t nbRandomU32( nbRandom* rng )
{
	// PCG-XSH-RR with a fixed odd increment
	uint64_t old = rng->state;
	rng->state = old * 6364136223846793005ull + 1442695040888963407ull;
	uint32_t xorShifted = (uint32_t)( ( ( old >> 18u ) ^ old ) >> 27u );
	uint32_t rot = (uint32_t)( old >> 59u );
	return ( xorShifted >> rot ) | ( xorShifted << ( ( 0u - rot ) & 31u ) );
}

uint64_t nbHashSeed( uint64_t a, uint64_t b )
{
	// splitmix64 finalizer over both inputs
	uint64_t z = a + 0x9E3779B97F4A7C15ull * ( b + 1 );
	z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ull;
	z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBull;
	return z ^ ( z >> 31 );
}

float nbCbrt( float x )
{
	if ( x <= 0.0f )
	{
		return 0.0f;
	}

	// Initial guess from the exponent, then Newton iterations y -= (y^3 - x) / (3 y^2)
	uint32_t bits;
	memcpy( &bits, &x, sizeof( bits ) );
	bits = bits / 3 + 709921077u;
	float y;
	memcpy( &y, &bits, sizeof( y ) );

	for ( int i = 0; i < 4; ++i )
	{
		y = y - ( y * y * y - x ) / ( 3.0f * y * y );
	}
	return y;
}
