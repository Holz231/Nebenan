// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"

#include <stdbool.h>
#include <stdint.h>

#if defined( _MSC_VER ) && ( defined( _M_ARM ) || defined( _M_ARM64 ) || defined( _M_ARM64EC ) )
#include <intrin.h>
#elif defined( _MSC_VER )
#include <intrin0.h>
// _MM_HINT_T0 lives here. Prefetch is a platform intrinsic, not part of the SIMD kernel
// selection, so it has to survive a BOX3D_DISABLE_SIMD build.
#if defined( _M_X64 ) || defined( _M_IX86 )
#include <xmmintrin.h>
#endif
#endif

#if defined( _MSC_VER )
#if defined( _M_X64 ) || defined( __x86_64__ ) || defined( _M_IX86 ) || defined( __i386__ )
#define b3Prefetch( addr ) _mm_prefetch( (const char*)( addr ), _MM_HINT_T0 )
#else
#define b3Prefetch( addr ) __prefetch( (const void*)( addr ) )
#endif
#elif defined( __GNUC__ ) || defined( __clang__ )
#define b3Prefetch( addr ) __builtin_prefetch( (const void*)( addr ), 0, 3 )
#else
#define b3Prefetch( addr ) ( (void)( addr ) )
#endif

// Added for Nebenan: a pause in a spin loop, see b3SolveDataflow
#if ( defined( __GNUC__ ) || defined( __clang__ ) ) && ( defined( __i386__ ) || defined( __x86_64__ ) )
static inline void b3SpinPause( void )
{
	__asm__ __volatile__( "pause\n" );
}
#elif ( defined( __arm__ ) && defined( __ARM_ARCH ) && __ARM_ARCH >= 7 ) || defined( __aarch64__ )
static inline void b3SpinPause( void )
{
	__asm__ __volatile__( "yield" ::: "memory" );
}
#elif defined( _MSC_VER ) && ( defined( _M_IX86 ) || defined( _M_X64 ) )
static inline void b3SpinPause( void )
{
	_mm_pause();
}
#elif defined( _MSC_VER ) && ( defined( _M_ARM ) || defined( _M_ARM64 ) )
static inline void b3SpinPause( void )
{
	__yield();
}
#else
static inline void b3SpinPause( void )
{
}
#endif

static inline void b3AtomicStoreInt( b3AtomicInt* a, int value )
{
#if defined( _MSC_VER )
	(void)_InterlockedExchange( (long*)&a->value, value );
#elif defined( __GNUC__ ) || defined( __clang__ )
	__atomic_store_n( &a->value, value, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

static inline int b3AtomicLoadInt( b3AtomicInt* a )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	int value = __iso_volatile_load32( (volatile __int32*)&a->value );
#if defined( _M_ARM ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
	__dmb( 0xB );
#else
	_ReadWriteBarrier();
#endif
	return value;
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_load_n( &a->value, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

// Added for Nebenan: a store that publishes the writes before it to a thread that reads it with b3AtomicLoadIntAcquire
static inline void b3AtomicStoreIntRelease( b3AtomicInt* a, int value )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
#if defined( _M_ARM ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
	__dmb( 0xB );
#else
	_ReadWriteBarrier();
#endif
	__iso_volatile_store32( (volatile __int32*)&a->value, value );
#elif defined( __GNUC__ ) || defined( __clang__ )
	__atomic_store_n( &a->value, value, __ATOMIC_RELEASE );
#else
#error "Unsupported platform"
#endif
}

// Added for Nebenan: a store without ordering
static inline void b3AtomicStoreIntRelaxed( b3AtomicInt* a, int value )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	__iso_volatile_store32( (volatile __int32*)&a->value, value );
#elif defined( __GNUC__ ) || defined( __clang__ )
	__atomic_store_n( &a->value, value, __ATOMIC_RELAXED );
#else
#error "Unsupported platform"
#endif
}

// Added for Nebenan: a load that sees the writes before the b3AtomicStoreIntRelease it reads
static inline int b3AtomicLoadIntAcquire( b3AtomicInt* a )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	int value = __iso_volatile_load32( (volatile __int32*)&a->value );
#if defined( _M_ARM ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
	__dmb( 0xB );
#else
	_ReadWriteBarrier();
#endif
	return value;
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_load_n( &a->value, __ATOMIC_ACQUIRE );
#else
#error "Unsupported platform"
#endif
}

// Added for Nebenan: waits until the atomic holds the value and tells whether that took longer than half a millisecond,
// far longer than any wait of the solver without a barrier per stage takes unless the thread it waits for lost its core,
// see b3SolveDataflow. Spins, and only after a while it reads the clock.
#define B3_STALL_MILLISECONDS 0.5f
static inline bool b3WaitForValue( b3AtomicInt* atomic, int value )
{
	if ( b3AtomicLoadIntAcquire( atomic ) == value )
	{
		return false;
	}

	bool stalled = false;
	uint64_t ticks = 0;
	int spins = 0;
	while ( b3AtomicLoadIntAcquire( atomic ) != value )
	{
		// Once the wait took far too long, the thread waited for has most likely lost its core, and the waiting thread gives
		// its own away to help it get one back
		if ( stalled )
		{
			b3Yield();
			continue;
		}

		b3SpinPause();
		spins += 1;
		if ( ( spins & 255 ) == 0 )
		{
			if ( ticks == 0 )
			{
				ticks = b3GetTicks();
			}
			else if ( b3GetMilliseconds( ticks ) > B3_STALL_MILLISECONDS )
			{
				stalled = true;
			}
		}
	}
	return stalled;
}

static inline int b3AtomicFetchAddInt( b3AtomicInt* a, int increment )
{
#if defined( _MSC_VER )
	return _InterlockedExchangeAdd( (long*)&a->value, (long)increment );
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_fetch_add( &a->value, increment, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

static inline bool b3AtomicCompareExchangeInt( b3AtomicInt* a, int expected, int desired )
{
#if defined( _MSC_VER )
	return _InterlockedCompareExchange( (long*)&a->value, (long)desired, (long)expected ) == expected;
#elif defined( __GNUC__ ) || defined( __clang__ )
	// The value written to expected is ignored
	return __atomic_compare_exchange_n( &a->value, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

static inline void b3AtomicStoreU32( b3AtomicU32* a, uint32_t value )
{
#if defined( _MSC_VER )
	(void)_InterlockedExchange( (long*)&a->value, value );
#elif defined( __GNUC__ ) || defined( __clang__ )
	__atomic_store_n( &a->value, value, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

static inline uint32_t b3AtomicLoadU32( b3AtomicU32* a )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	uint32_t value = (uint32_t)__iso_volatile_load32( (volatile __int32*)&a->value );
#if defined( _M_ARM ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
	__dmb( 0xB );
#else
	_ReadWriteBarrier();
#endif
	return value;
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_load_n( &a->value, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

// Relaxed load on a plain word. Lets a racing reader peek before paying for a read modify write.
static inline uint32_t b3AtomicLoadU32Raw( uint32_t* a )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	return (uint32_t)__iso_volatile_load32( (volatile __int32*)a );
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_load_n( a, __ATOMIC_RELAXED );
#else
#error "Unsupported platform"
#endif
}

static inline uint32_t b3AtomicFetchOrU32( uint32_t* a, uint32_t mask )
{
#if defined( _MSC_VER )
	return (uint32_t)_InterlockedOr( (long*)a, (long)mask );
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_fetch_or( a, mask, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

static inline int64_t b3AtomicFetchAddI64( b3AtomicI64* a, int64_t increment )
{
#if defined( _MSC_VER )
	return (int64_t)_InterlockedExchangeAdd64( (__int64*)&a->value, (__int64)increment );
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_fetch_add( &a->value, increment, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}

static inline int64_t b3AtomicLoadI64( b3AtomicI64* a )
{
#if defined( _MSC_VER ) && !defined( __clang__ ) && !defined( _M_ARM )
	int64_t value = __iso_volatile_load64( (volatile __int64*)&a->value );
#if defined( _M_ARM64 ) || defined( _M_ARM64EC )
	__dmb( 0xB );
#else
	_ReadWriteBarrier();
#endif
	return value;
#elif defined( _MSC_VER ) && !defined( __clang__ )
	// 32-bit ARM has no plain atomic 64-bit load
	return _InterlockedOr64( (__int64*)&a->value, 0 );
#elif defined( __GNUC__ ) || defined( __clang__ )
	return __atomic_load_n( &a->value, __ATOMIC_SEQ_CST );
#else
#error "Unsupported platform"
#endif
}
