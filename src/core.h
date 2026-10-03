// SPDX-License-Identifier: MIT

#pragma once

#include "nebenan/base.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined( _MSC_VER ) && !defined( __clang__ )
#include <intrin.h>
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

// Linear scratch allocator. Memory is released all at once with nbArena_Reset.
// Allocations never move, the arena grows by chaining blocks and coalesces them on reset.
typedef struct nbArenaBlock nbArenaBlock;

typedef struct nbArena
{
	nbArenaBlock* head;
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
uint32_t nbRandomU32( nbRandom* rng );

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
