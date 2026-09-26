// SPDX-License-Identifier: MIT
// Nebenan - polygonal real-time destruction for Box3D

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// clang-format off
#if defined( _WIN32 ) && defined( NEBENAN_BUILD_DLL )
	#define NB_EXPORT __declspec( dllexport )
#elif defined( _WIN32 ) && defined( NEBENAN_DLL )
	#define NB_EXPORT __declspec( dllimport )
#elif defined( __GNUC__ ) && defined( NEBENAN_BUILD_DLL )
	#define NB_EXPORT __attribute__( ( visibility( "default" ) ) )
#else
	#define NB_EXPORT
#endif

#ifdef __cplusplus
	#define NB_API extern "C" NB_EXPORT
	#define NB_INLINE inline
	#define NB_LITERAL( T ) T
	#define NB_ZERO_INIT {}
#else
	#define NB_API NB_EXPORT
	#define NB_INLINE static inline
	#define NB_LITERAL( T ) ( T )
	#define NB_ZERO_INIT { 0 }
#endif
// clang-format on

/// Prototype for user allocation function
/// @param size the allocation size in bytes
/// @param alignment the required alignment, guaranteed to be a power of 2
typedef void* nbAllocFcn( size_t size, int alignment );

/// Prototype for user free function
/// @param mem the memory previously allocated through `nbAllocFcn`
/// @param size the allocation size in bytes
typedef void nbFreeFcn( void* mem, size_t size );

/// Prototype for the user assert callback. Return 0 to skip the debugger break.
typedef int nbAssertFcn( const char* condition, const char* fileName, int lineNumber );

/// Override the default allocator (aligned malloc/free). Call before creating a world.
NB_API void nbSetAllocator( nbAllocFcn* allocFcn, nbFreeFcn* freeFcn );

/// Override the default assert callback.
NB_API void nbSetAssertFcn( nbAssertFcn* assertFcn );

/// Number of bytes currently allocated by Nebenan.
NB_API int64_t nbGetByteCount( void );

/// Version numbering scheme. See https://semver.org/
typedef struct nbVersion
{
	int major;
	int minor;
	int revision;
} nbVersion;

/// Get the current version of Nebenan
NB_API nbVersion nbGetVersion( void );
