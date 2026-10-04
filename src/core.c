// SPDX-License-Identifier: MIT

// MAP_ANONYMOUS for the large blocks below, also in strict C modes
#if defined( __linux__ ) && !defined( _DEFAULT_SOURCE )
#define _DEFAULT_SOURCE
#endif

#include "core.h"

#include <stdio.h>
#include <stdlib.h>

#if defined( _MSC_VER )
#include <intrin.h>
#include <malloc.h>
#endif

#if defined( NB_LARGE_BLOCKS )
#if defined( _WIN64 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#if !defined( MAP_ANONYMOUS )
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif
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

#if defined( NB_LARGE_BLOCKS )

// A large block reserves a wide range of address space and commits pages as it grows. Growing never copies it and
// never holds the old and the new block at once, 64 bit processes have address space to spare and only the pages in
// use take memory. 16 GB is more than any scene fills.
#define NB_LARGE_RESERVE ( (size_t)1 << 34 )

// Pages are committed in steps of 64 KB, a multiple of the page size on every platform
#define NB_LARGE_STEP ( (size_t)1 << 16 )

// The header in front of a large block, 64 bytes keep the block aligned to a cache line
#define NB_LARGE_HEADER 64

typedef struct nbLargeHeader
{
	size_t reserved;
	size_t size;
} nbLargeHeader;

static size_t nbLargeCommitSize( size_t size )
{
	return ( NB_LARGE_HEADER + size + NB_LARGE_STEP - 1 ) & ~( NB_LARGE_STEP - 1 );
}

static char* nbReserveAddresses( size_t size )
{
#if defined( _WIN64 )
	return VirtualAlloc( NULL, size, MEM_RESERVE, PAGE_NOACCESS );
#else
	void* ptr = mmap( NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	return ptr != MAP_FAILED ? ptr : NULL;
#endif
}

static bool nbCommitAddresses( char* base, size_t begin, size_t end )
{
#if defined( _WIN64 )
	return VirtualAlloc( base + begin, end - begin, MEM_COMMIT, PAGE_READWRITE ) != NULL;
#else
	return mprotect( base + begin, end - begin, PROT_READ | PROT_WRITE ) == 0;
#endif
}

static void nbReleaseAddresses( char* base, size_t size )
{
#if defined( _WIN64 )
	NB_UNUSED( size );
	VirtualFree( base, 0, MEM_RELEASE );
#else
	munmap( base, size );
#endif
}

static void* nbAllocLarge( size_t size )
{
	// Take less address space if the system does not give the full range
	size_t commitSize = nbLargeCommitSize( size );
	for ( size_t reserveSize = NB_LARGE_RESERVE;; reserveSize /= 4 )
	{
		reserveSize = reserveSize > commitSize ? reserveSize : commitSize;
		char* base = nbReserveAddresses( reserveSize );
		if ( base != NULL )
		{
			if ( nbCommitAddresses( base, 0, commitSize ) == false )
			{
				nbReleaseAddresses( base, reserveSize );
				return NULL;
			}

			nbLargeHeader* header = (nbLargeHeader*)base;
			header->reserved = reserveSize;
			header->size = size;
			return base + NB_LARGE_HEADER;
		}

		if ( reserveSize == commitSize )
		{
			return NULL;
		}
	}
}

static void nbFreeLarge( void* mem, size_t size )
{
	char* base = (char*)mem - NB_LARGE_HEADER;
	nbLargeHeader* header = (nbLargeHeader*)base;
	NB_ASSERT( header->size == size );
	NB_UNUSED( size );
	nbReleaseAddresses( base, header->reserved );
}

// Grows a large block in place if its address space has room
static bool nbGrowLarge( void* mem, size_t oldSize, size_t newSize )
{
	char* base = (char*)mem - NB_LARGE_HEADER;
	nbLargeHeader* header = (nbLargeHeader*)base;
	NB_ASSERT( header->size == oldSize );

	size_t oldCommitSize = nbLargeCommitSize( oldSize );
	size_t newCommitSize = nbLargeCommitSize( newSize );
	if ( newCommitSize > header->reserved )
	{
		return false;
	}

	if ( newCommitSize > oldCommitSize && nbCommitAddresses( base, oldCommitSize, newCommitSize ) == false )
	{
		return false;
	}

	header->size = newSize;
	return true;
}

#endif

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
#if defined( NB_LARGE_BLOCKS )
	else if ( alignedSize >= NB_LARGE_SIZE )
	{
		ptr = nbAllocLarge( alignedSize );
	}
#endif
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
#if defined( NB_LARGE_BLOCKS )
	else if ( alignedSize >= NB_LARGE_SIZE )
	{
		nbFreeLarge( mem, alignedSize );
	}
#endif
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

#if defined( NB_LARGE_BLOCKS )
	// A large block grows in place while its address space has room
	if ( nb_allocFcn == NULL && oldSize > 0 )
	{
		size_t oldAlignedSize = ( ( oldSize - 1 ) | ( NB_ALIGNMENT - 1 ) ) + 1;
		size_t newAlignedSize = ( ( newSize - 1 ) | ( NB_ALIGNMENT - 1 ) ) + 1;
		if ( oldAlignedSize >= NB_LARGE_SIZE && nbGrowLarge( oldMem, oldAlignedSize, newAlignedSize ) )
		{
			nbAtomicAdd64( &nb_byteCount, (int64_t)( newAlignedSize - oldAlignedSize ) );
			return oldMem;
		}
	}
#endif

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
