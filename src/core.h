// SPDX-License-Identifier: MIT

#pragma once

#include "nebenan/base.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined( _MSC_VER ) && !defined( __clang__ )
#include <intrin.h>
#if defined( _M_X64 ) || defined( _M_IX86 )
#include <xmmintrin.h>
#endif
#endif

#if defined( _MSC_VER )
#define NB_BREAKPOINT __debugbreak()
#elif defined( __GNUC__ ) || defined( __clang__ )
#define NB_BREAKPOINT __builtin_trap()
#else
#include <assert.h>
#define NB_BREAKPOINT assert( 0 )
#endif

int nbInternalAssert( const char* condition, const char* fileName, int lineNumber );

#if !defined( NDEBUG ) || defined( NB_ENABLE_ASSERT )
#define NB_ASSERT( condition )                                                                                                   \
	do                                                                                                                           \
	{                                                                                                                            \
		if ( !( condition ) && nbInternalAssert( #condition, __FILE__, (int)( __LINE__ ) ) )                                    \
		{                                                                                                                        \
			NB_BREAKPOINT;                                                                                                       \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( 0 )
#else
#define NB_ASSERT( ... ) ( (void)0 )
#endif

#define NB_UNUSED( x ) (void)( x )
#define NB_NULL_INDEX ( -1 )
#define NB_ARRAY_COUNT( A ) (int)( sizeof( A ) / sizeof( A[0] ) )
#define NB_SECRET_COOKIE 0x4E42414E

// Zones of the Tracy profiler with NEBENAN_PROFILE, nothing without it. Tracy runs with a manual lifetime, the first
// physics world or pool starts it, see b3StartProfiler.
#if defined( NB_PROFILE )
#include <tracy/TracyC.h>
#define NB_ZONE( ctx, name ) TracyCZoneN( ctx, name, 1 )
#define NB_ZONE_END( ctx ) TracyCZoneEnd( ctx )
#define NB_FRAME( name ) ___tracy_emit_frame_mark( name )
#define NB_THREAD_NAME( name ) ___tracy_set_thread_name( name )
#else
#define NB_ZONE( ctx, name )
#define NB_ZONE_END( ctx )
#define NB_FRAME( name )
#define NB_THREAD_NAME( name )
#endif

// Every allocation starts on this many bytes and is padded to a multiple of them. A cache line, so the actors of an array
// lie on lines of their own, see nbActor::restPose.
#define NB_ALIGNMENT 64

void* nbAlloc( size_t size );
void nbFree( void* mem, size_t size );
void* nbGrowAlloc( void* oldMem, size_t oldSize, size_t newSize );

// Large blocks grow in place where 64 bit address space can be reserved for them, see core.c. Under ASan blocks
// keep coming from malloc, so ASan keeps checking their bounds.
#if defined( __SANITIZE_ADDRESS__ )
#define NB_ADDRESS_SANITIZER
#elif defined( __has_feature )
#if __has_feature( address_sanitizer )
#define NB_ADDRESS_SANITIZER
#endif
#endif

#if !defined( NB_ADDRESS_SANITIZER ) &&                                                                                          \
	( defined( _WIN64 ) || ( defined( __LP64__ ) && ( defined( __linux__ ) || defined( __APPLE__ ) ) ) )
#define NB_LARGE_BLOCKS

// Blocks of 1 MB or more are large
#define NB_LARGE_SIZE ( (size_t)1 << 20 )
#endif

// Atomic add that returns the previous value. Used to hand out work items to worker threads.
int nbAtomicFetchAddInt( int* value, int delta );

// Atomic load and store, for a value that other threads read without a lock
static inline int nbAtomicLoadInt( int* value )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	int result = __iso_volatile_load32( (volatile __int32*)value );
#if defined( _M_ARM ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
	__dmb( 0xB );
#else
	_ReadWriteBarrier();
#endif
	return result;
#else
	return __atomic_load_n( value, __ATOMIC_SEQ_CST );
#endif
}

static inline void nbAtomicStoreInt( int* value, int desired )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	(void)_InterlockedExchange( (volatile long*)value, desired );
#else
	__atomic_store_n( value, desired, __ATOMIC_SEQ_CST );
#endif
}

// Atomic compare and swap: writes desired if the value is expected and returns whether it did
static inline bool nbAtomicCompareExchangeInt( int* value, int expected, int desired )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	return _InterlockedCompareExchange( (volatile long*)value, desired, expected ) == expected;
#else
	return __atomic_compare_exchange_n( value, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
#endif
}

// A number of its own for the calling thread, never zero, see nbWorld_Step
int nbGetThreadToken( void );

// Keeps a rare path out of the function that calls it, so that one needs no stack frame
#if defined( _MSC_VER )
#define NB_NOINLINE __declspec( noinline )
#elif defined( __GNUC__ ) || defined( __clang__ )
#define NB_NOINLINE __attribute__( ( noinline ) )
#else
#define NB_NOINLINE
#endif

// A growable array of plain old data. The element type is part of the macro argument so
// the arrays stay type safe. Growth doubles the capacity.
#define NB_ARRAY_DECLARE( T, name )                                                                                              \
	typedef struct name                                                                                                          \
	{                                                                                                                            \
		T* data;                                                                                                                 \
		int count;                                                                                                               \
		int capacity;                                                                                                            \
	} name

#define nbArray_Reserve( a, n )                                                                                                  \
	do                                                                                                                           \
	{                                                                                                                            \
		if ( ( a ).capacity < ( n ) )                                                                                            \
		{                                                                                                                        \
			int nbNewCap_ = ( a ).capacity < 8 ? 8 : 2 * ( a ).capacity;                                                         \
			while ( nbNewCap_ < ( n ) )                                                                                          \
				nbNewCap_ *= 2;                                                                                                  \
			( a ).data = nbGrowAlloc( ( a ).data, (size_t)( a ).capacity * sizeof( *( a ).data ),                               \
									  (size_t)nbNewCap_ * sizeof( *( a ).data ) );                                              \
			( a ).capacity = nbNewCap_;                                                                                          \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( 0 )

#define nbArray_Push( a, value )                                                                                                 \
	do                                                                                                                           \
	{                                                                                                                            \
		nbArray_Reserve( a, ( a ).count + 1 );                                                                                   \
		( a ).data[( a ).count++] = ( value );                                                                                   \
	}                                                                                                                            \
	while ( 0 )

#define nbArray_Free( a )                                                                                                        \
	do                                                                                                                           \
	{                                                                                                                            \
		nbFree( ( a ).data, (size_t)( a ).capacity * sizeof( *( a ).data ) );                                                    \
		( a ).data = NULL;                                                                                                       \
		( a ).count = 0;                                                                                                         \
		( a ).capacity = 0;                                                                                                      \
	}                                                                                                                            \
	while ( 0 )

NB_ARRAY_DECLARE( int, nbIntArray );

// Bits packed 64 to a word
NB_ARRAY_DECLARE( uint64_t, nbBitArray );

// Index of the lowest set bit of a word that is not zero
static inline int nbCTZ64( uint64_t word )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	unsigned long index;
#if defined( _WIN64 )
	_BitScanForward64( &index, word );
#else
	if ( (uint32_t)word != 0 )
	{
		_BitScanForward( &index, (uint32_t)word );
	}
	else
	{
		_BitScanForward( &index, (uint32_t)( word >> 32 ) );
		index += 32;
	}
#endif
	return (int)index;
#else
	return __builtin_ctzll( word );
#endif
}

// Start loading the cache line at an address, for a loop that reads scattered memory. Only a hint, it changes nothing.
#if defined( __GNUC__ ) || defined( __clang__ )
#define NB_PREFETCH( address ) __builtin_prefetch( (const void*)( address ), 0, 3 )
#elif defined( _MSC_VER ) && ( defined( _M_X64 ) || defined( _M_IX86 ) )
#define NB_PREFETCH( address ) _mm_prefetch( (const char*)( address ), _MM_HINT_T0 )
#elif defined( _MSC_VER )
#define NB_PREFETCH( address ) __prefetch( (const void*)( address ) )
#else
#define NB_PREFETCH( address ) ( (void)( address ) )
#endif

// Linear scratch allocator. Memory is released all at once with nbArena_Reset.
// Allocations never move, the arena grows by chaining blocks and coalesces them on reset.
typedef struct nbArenaBlock nbArenaBlock;

typedef struct nbArena
{
	// Every arena on a cache line of its own. The workers allocate from theirs at the same time, and three of them shared a
	// line, which went from core to core with every allocation, see nbSettleDebris.
	_Alignas( 64 ) nbArenaBlock* head;
	size_t used;
	size_t peak;
} nbArena;

void nbArena_Create( nbArena* arena, size_t capacity );
void nbArena_Destroy( nbArena* arena );
void* nbArena_Alloc( nbArena* arena, size_t size );
void nbArena_Reset( nbArena* arena );

#define nbArena_AllocArray( arena, T, count ) ( (T*)nbArena_Alloc( arena, sizeof( T ) * (size_t)( count ) ) )

// Deterministic random numbers (PCG32). The same seed gives the same fracture on every platform.
typedef struct nbRandom
{
	uint64_t state;
} nbRandom;

nbRandom nbMakeRandom( uint64_t seed, uint64_t stream );

// Inline: drawing fracture sites and estimating overlaps take thousands of numbers per impact
static inline uint32_t nbRandomU32( nbRandom* rng )
{
	// PCG-XSH-RR with a fixed odd increment
	uint64_t old = rng->state;
	rng->state = old * 6364136223846793005ull + 1442695040888963407ull;
	uint32_t xorShifted = (uint32_t)( ( ( old >> 18u ) ^ old ) >> 27u );
	uint32_t rot = (uint32_t)( old >> 59u );
	return ( xorShifted >> rot ) | ( xorShifted << ( ( 0u - rot ) & 31u ) );
}

// Uniform float in [0, 1)
static inline float nbRandomFloat( nbRandom* rng )
{
	return (float)( nbRandomU32( rng ) >> 8 ) * ( 1.0f / 16777216.0f );
}

// Uniform float in [lower, upper)
static inline float nbRandomRange( nbRandom* rng, float lower, float upper )
{
	return lower + ( upper - lower ) * nbRandomFloat( rng );
}

// Mix bits of several integers into a well distributed seed.
uint64_t nbHashSeed( uint64_t a, uint64_t b );

// Cube root from basic arithmetic only. libm's cbrtf may round differently across platforms,
// this gives the same bits everywhere, which keeps fracture patterns deterministic.
float nbCbrt( float x );
