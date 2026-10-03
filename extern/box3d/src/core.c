// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

// Added for Nebenan: MAP_ANONYMOUS for the large blocks below, also in strict C modes
#if defined( __linux__ ) && !defined( _DEFAULT_SOURCE )
#define _DEFAULT_SOURCE
#endif

#if defined( B3_COMPILER_MSVC )
// CRTDBG requires these to be included first
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

#include "core.h"
#include "rapidhash.h"

#include "box3d/constants.h"
#include "box3d/math_functions.h"

#include <stdarg.h>
#include <string.h>

#ifdef BOX3D_PROFILE

#include <tracy/TracyC.h>
#define b3TracyCAlloc( ptr, size ) TracyCAlloc( ptr, size )
#define b3TracyCFree( ptr ) TracyCFree( ptr )

#else

#define b3TracyCAlloc( ptr, size )
#define b3TracyCFree( ptr )

#endif

#include "platform.h"

#include <stdio.h>

// Added for Nebenan: large blocks grow in place. A large block reserves a wide range of address space and commits
// pages as it grows, so growing never copies it and never holds the old and the new block at once. 64 bit processes
// have address space to spare and only the pages in use take memory. Under ASan blocks keep coming from malloc, so
// ASan keeps checking their bounds.
#if defined( __SANITIZE_ADDRESS__ )
#define B3_ADDRESS_SANITIZER
#elif defined( __has_feature )
#if __has_feature( address_sanitizer )
#define B3_ADDRESS_SANITIZER
#endif
#endif

#if !defined( B3_ADDRESS_SANITIZER ) &&                                                                                          \
	( defined( _WIN64 ) || ( defined( __LP64__ ) && ( defined( __linux__ ) || defined( __APPLE__ ) ) ) )

#define B3_LARGE_BLOCKS

#if defined( _WIN64 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#if !defined( MAP_ANONYMOUS )
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

// Blocks of 1 MB or more are large
#define B3_LARGE_SIZE ( (size_t)1 << 20 )

// Address space a large block reserves. Box3D grows blocks by int sizes, so 4 GB holds the largest one.
#define B3_LARGE_RESERVE ( (size_t)1 << 32 )

// Pages are committed in steps of 64 KB, a multiple of the page size on every platform
#define B3_LARGE_STEP ( (size_t)1 << 16 )

// The header in front of a large block, 64 bytes keep the block aligned to a cache line
#define B3_LARGE_HEADER 64

typedef struct b3LargeHeader
{
	size_t reserved;
	size_t size;
} b3LargeHeader;

static size_t b3LargeCommitSize( size_t size )
{
	return ( B3_LARGE_HEADER + size + B3_LARGE_STEP - 1 ) & ~( B3_LARGE_STEP - 1 );
}

static char* b3ReserveAddresses( size_t size )
{
#if defined( _WIN64 )
	return VirtualAlloc( NULL, size, MEM_RESERVE, PAGE_NOACCESS );
#else
	void* ptr = mmap( NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	return ptr != MAP_FAILED ? ptr : NULL;
#endif
}

static bool b3CommitAddresses( char* base, size_t begin, size_t end )
{
#if defined( _WIN64 )
	return VirtualAlloc( base + begin, end - begin, MEM_COMMIT, PAGE_READWRITE ) != NULL;
#else
	return mprotect( base + begin, end - begin, PROT_READ | PROT_WRITE ) == 0;
#endif
}

static void b3ReleaseAddresses( char* base, size_t size )
{
#if defined( _WIN64 )
	B3_UNUSED( size );
	VirtualFree( base, 0, MEM_RELEASE );
#else
	munmap( base, size );
#endif
}

static void* b3AllocLarge( size_t size )
{
	// Take less address space if the system does not give the full range
	size_t commitSize = b3LargeCommitSize( size );
	for ( size_t reserveSize = B3_LARGE_RESERVE;; reserveSize /= 4 )
	{
		reserveSize = reserveSize > commitSize ? reserveSize : commitSize;
		char* base = b3ReserveAddresses( reserveSize );
		if ( base != NULL )
		{
			if ( b3CommitAddresses( base, 0, commitSize ) == false )
			{
				b3ReleaseAddresses( base, reserveSize );
				return NULL;
			}

			b3LargeHeader* header = (b3LargeHeader*)base;
			header->reserved = reserveSize;
			header->size = size;
			return base + B3_LARGE_HEADER;
		}

		if ( reserveSize == commitSize )
		{
			return NULL;
		}
	}
}

static void b3FreeLarge( void* mem, size_t size )
{
	char* base = (char*)mem - B3_LARGE_HEADER;
	b3LargeHeader* header = (b3LargeHeader*)base;
	B3_ASSERT( header->size == size );
	B3_UNUSED( size );
	b3ReleaseAddresses( base, header->reserved );
}

// Grows a large block in place if its address space has room. New pages are zero.
static bool b3GrowLarge( void* mem, size_t oldSize, size_t newSize )
{
	char* base = (char*)mem - B3_LARGE_HEADER;
	b3LargeHeader* header = (b3LargeHeader*)base;
	B3_ASSERT( header->size == oldSize );

	size_t oldCommitSize = b3LargeCommitSize( oldSize );
	size_t newCommitSize = b3LargeCommitSize( newSize );
	if ( newCommitSize > header->reserved )
	{
		return false;
	}

	if ( newCommitSize > oldCommitSize && b3CommitAddresses( base, oldCommitSize, newCommitSize ) == false )
	{
		return false;
	}

	header->size = newSize;
	return true;
}

#endif

_Static_assert( B3_MAX_MANIFOLD_POINTS >= 4, "B3_MAX_MANIFOLD_POINTS must be at least 4" );

// This allows the user to change the length units at runtime
static float b3_lengthUnitsPerMeter = 1.0f;

void b3SetLengthUnitsPerMeter( float lengthUnits )
{
	B3_ASSERT( b3IsValidFloat( lengthUnits ) && lengthUnits > 0.0f );
	b3_lengthUnitsPerMeter = lengthUnits;
}

float b3GetLengthUnitsPerMeter( void )
{
	return b3_lengthUnitsPerMeter;
}

static float b3_stallThreshold = FLT_MAX;

void b3SetStallThreshold( float seconds )
{
	B3_ASSERT( b3IsValidFloat( seconds ) && seconds > 0.0f );
	b3_stallThreshold = seconds;
}

float b3GetStallThreshold( void )
{
	return b3_stallThreshold;
}

static int b3DefaultAssertFcn( const char* condition, const char* fileName, int lineNumber )
{
	printf( "BOX3D ASSERTION: %s, %s, line %d\n", condition, fileName, lineNumber );

	// return non-zero to break to debugger
	return 1;
}

b3AssertFcn* b3AssertHandler = b3DefaultAssertFcn;

void b3SetAssertFcn( b3AssertFcn* assertFcn )
{
	B3_ASSERT( assertFcn != NULL );
	b3AssertHandler = assertFcn;
}

#if !defined( NDEBUG ) || defined( B3_ENABLE_ASSERT )
int b3InternalAssert( const char* condition, const char* fileName, int lineNumber )
{
	int result = b3AssertHandler( condition, fileName, lineNumber );
	if ( result )
	{
		B3_BREAKPOINT;
	}
	return result;
}
#endif

static void b3DefaultLogFcn( const char* message )
{
	printf( "Box3D: %s\n", message );
}

b3LogFcn* b3LogHandler = b3DefaultLogFcn;

void b3SetLogFcn( b3LogFcn* logFcn )
{
	B3_ASSERT( logFcn != NULL );
	b3LogHandler = logFcn;
}

void b3Log( const char* format, ... )
{
	va_list args;
	va_start( args, format );
	char buffer[512];
	vsnprintf( buffer, sizeof( buffer ), format, args );
	b3LogHandler( buffer );
	va_end( args );
}

b3Version b3GetVersion( void )
{
	return (b3Version){ 0, 2, 0 };
}

bool b3IsDoublePrecision( void )
{
#if defined( BOX3D_DOUBLE_PRECISION )
	return true;
#else
	return false;
#endif
}

int b3GetMaxManifoldPoints( void )
{
	return B3_MAX_MANIFOLD_POINTS;
}

static b3AllocFcn* b3_allocFcn = NULL;
static b3FreeFcn* b3_freeFcn = NULL;

static b3AtomicI64 b3_byteCount;

void b3SetAllocator( b3AllocFcn* allocFcn, b3FreeFcn* freeFcn )
{
	b3_allocFcn = allocFcn;
	b3_freeFcn = freeFcn;
}

void* b3Alloc( size_t size )
{
	if ( size == 0 )
	{
		return NULL;
	}

	b3AtomicFetchAddI64( &b3_byteCount, (int)size );

	// Allocation must be a multiple of B3_ALIGNMENT (required by spec).
	// https://en.cppreference.com/w/c/memory/aligned_alloc
	size_t alignedSize = ( ( size - 1 ) | ( B3_ALIGNMENT - 1 ) ) + 1;

	if ( b3_allocFcn != NULL )
	{
		void* ptr = b3_allocFcn( alignedSize, B3_ALIGNMENT );
		b3TracyCAlloc( ptr, size );

		B3_ASSERT( ptr != NULL );
		B3_ASSERT( ( (uintptr_t)ptr & ( B3_ALIGNMENT - 1 ) ) == 0 );

		return ptr;
	}

#if defined( B3_LARGE_BLOCKS )
	// Added for Nebenan, see the large blocks above
	if ( alignedSize >= B3_LARGE_SIZE )
	{
		void* ptr = b3AllocLarge( alignedSize );
		b3TracyCAlloc( ptr, size );

		B3_ASSERT( ptr != NULL );

		return ptr;
	}
#endif

#ifdef B3_PLATFORM_WINDOWS
	void* ptr = _aligned_malloc( alignedSize, B3_ALIGNMENT );
#elif defined( B3_PLATFORM_ANDROID )
	void* ptr = NULL;
	if ( posix_memalign( &ptr, B3_ALIGNMENT, alignedSize ) != 0 )
	{
		// allocation failed, exit the application
		exit( EXIT_FAILURE );
	}
#else
	void* ptr = aligned_alloc( B3_ALIGNMENT, alignedSize );
#endif

	b3TracyCAlloc( ptr, size );

	B3_ASSERT( ptr != NULL );
	B3_ASSERT( ( (uintptr_t)ptr & ( B3_ALIGNMENT - 1 ) ) == 0 );

	return ptr;
}

void b3Free( void* mem, size_t size )
{
	if ( mem == NULL )
	{
		return;
	}

	b3TracyCFree( mem );

	size_t alignedSize = ( ( size - 1 ) | ( B3_ALIGNMENT - 1 ) ) + 1;

	if ( b3_freeFcn != NULL )
	{
		b3_freeFcn( mem, alignedSize );
	}
#if defined( B3_LARGE_BLOCKS )
	else if ( alignedSize >= B3_LARGE_SIZE )
	{
		b3FreeLarge( mem, alignedSize );
	}
#endif
	else
	{
#ifdef B3_PLATFORM_WINDOWS
		_aligned_free( mem );
#else
		free( mem );
#endif
	}

	b3AtomicFetchAddI64( &b3_byteCount, -(int64_t)size );
}

void* b3GrowAlloc( void* oldMem, int oldSize, int newSize )
{
	B3_ASSERT( newSize > oldSize );

#if defined( B3_LARGE_BLOCKS )
	// Added for Nebenan: a large block grows in place while its address space has room
	if ( b3_allocFcn == NULL && oldSize > 0 )
	{
		size_t oldAlignedSize = ( ( (size_t)oldSize - 1 ) | ( B3_ALIGNMENT - 1 ) ) + 1;
		size_t newAlignedSize = ( ( (size_t)newSize - 1 ) | ( B3_ALIGNMENT - 1 ) ) + 1;
		if ( oldAlignedSize >= B3_LARGE_SIZE && b3GrowLarge( oldMem, oldAlignedSize, newAlignedSize ) )
		{
			b3AtomicFetchAddI64( &b3_byteCount, (int64_t)newSize - oldSize );
			b3TracyCFree( oldMem );
			b3TracyCAlloc( oldMem, newSize );
			return oldMem;
		}
	}
#endif

	void* newMem = b3Alloc( newSize );
	if ( oldSize > 0 )
	{
		memcpy( newMem, oldMem, oldSize );
		b3Free( oldMem, oldSize );
	}
	return newMem;
}

void* b3GrowAllocZeroed( void* oldMem, int oldSize, int newSize )
{
	void* newMem = b3GrowAlloc( oldMem, oldSize, newSize );

	// Added for Nebenan: a block that grew in place got fresh pages, which are zero already
	if ( newMem != oldMem )
	{
		memset( (char*)newMem + oldSize, 0, (size_t)( newSize - oldSize ) );
	}
	return newMem;
}

int64_t b3GetByteCount( void )
{
	return b3AtomicLoadI64( &b3_byteCount );
}

void* b3AllocZero( size_t size )
{
	void* mem = b3Alloc( size );
	memset( mem, 0, size );
	return mem;
}

// Not used. Keeping around in case I need this.
void b3StrCpy( char* dst, int size, const char* src )
{
	B3_ASSERT( size > 0 );

	if ( src != NULL )
	{
#if defined( _MSC_VER )
		strncpy_s( dst, size, src, size - 1 );
#else
		strncpy( dst, src, size - 1 );
		dst[size - 1] = 0;
#endif
	}
	else
	{
		memset( dst, 0, size );
	}
}

uint64_t b3Hash64NonZero( const uint8_t* bytes, int n )
{
	if ( n <= 0 )
	{
		return 1;
	}

	uint64_t h = rapidhash( bytes, n );
	return h == 0 ? 1 : h;
}
