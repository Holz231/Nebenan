// SPDX-License-Identifier: MIT

#include "core.h"

#include "nebenan/nebenan.h"

#include <stdio.h>
#include <stdlib.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined( __APPLE__ )
#include <sys/sysctl.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

// More threads than this gain the solver nothing, measured with the benchmark
#define NB_RECOMMENDED_MAX_WORKERS 8

int nbGetHardwareThreadCount( void )
{
#if defined( _WIN32 )
	DWORD count = GetActiveProcessorCount( ALL_PROCESSOR_GROUPS );
	return count > 0 ? (int)count : 1;
#else
	long count = sysconf( _SC_NPROCESSORS_ONLN );
	return count > 0 ? (int)count : 1;
#endif
}

#if !defined( _WIN32 ) && !defined( __APPLE__ )

#define NB_MAX_CPUS 4096

// A core of a package, the same for the hardware threads it runs
typedef struct nbCore
{
	int package;
	int core;
} nbCore;

static int nbCompareCores( const void* a, const void* b )
{
	const nbCore* x = a;
	const nbCore* y = b;
	if ( x->package != y->package )
	{
		return x->package < y->package ? -1 : 1;
	}
	return ( x->core > y->core ) - ( x->core < y->core );
}

#endif

int nbGetPerformanceCoreCount( void )
{
#if defined( _WIN32 )
	DWORD length = 0;
	GetLogicalProcessorInformationEx( RelationProcessorCore, NULL, &length );
	if ( length == 0 )
	{
		return 0;
	}

	size_t size = length;
	unsigned char* buffer = nbAlloc( size );
	int count = 0;
	if ( GetLogicalProcessorInformationEx( RelationProcessorCore, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buffer, &length ) )
	{
		// A higher efficiency class is a faster core. Without hybrid cores every core has the same class.
		int highestClass = -1;
		for ( DWORD offset = 0; offset < length; )
		{
			const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* info = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)( buffer + offset );

			// The byte after the flags, older MinGW headers still call it reserved
			int efficiencyClass = ( (const BYTE*)&info->Processor.Flags )[1];
			if ( efficiencyClass > highestClass )
			{
				highestClass = efficiencyClass;
				count = 0;
			}
			count += efficiencyClass == highestClass ? 1 : 0;
			offset += info->Size;
		}
	}
	nbFree( buffer, size );
	return count;
#elif defined( __APPLE__ )
	int count = 0;
	size_t size = sizeof( count );
	if ( sysctlbyname( "hw.perflevel0.physicalcpu", &count, &size, NULL, 0 ) == 0 && count > 0 )
	{
		return count;
	}
	size = sizeof( count );
	return sysctlbyname( "hw.physicalcpu", &count, &size, NULL, 0 ) == 0 ? count : 0;
#else
	// Hybrid Intel CPUs list their performance cores as cpu_core, the efficiency cores as cpu_atom. The list is a range
	// of logical processors like 0-11,16-19.
	uint64_t performance[NB_MAX_CPUS / 64] = { 0 };
	bool hybrid = false;
	FILE* list = fopen( "/sys/devices/cpu_core/cpus", "r" );
	if ( list != NULL )
	{
		int first = 0;
		while ( fscanf( list, "%d", &first ) == 1 && first >= 0 && first < NB_MAX_CPUS )
		{
			int last = first;
			int next = fgetc( list );
			if ( next == '-' && fscanf( list, "%d", &last ) == 1 )
			{
				next = fgetc( list );
			}
			last = last < NB_MAX_CPUS - 1 ? last : NB_MAX_CPUS - 1;
			for ( int cpu = first; cpu <= last; ++cpu )
			{
				performance[cpu >> 6] |= (uint64_t)1 << ( cpu & 63 );
				hybrid = true;
			}
			if ( next != ',' )
			{
				break;
			}
		}
		fclose( list );
	}

	// Distinct cores in the topology of the logical processors
	nbCore* cores = nbAlloc( sizeof( nbCore ) * NB_MAX_CPUS );
	int coreCount = 0;
	for ( int cpu = 0; cpu < NB_MAX_CPUS; ++cpu )
	{
		char path[128];
		int values[2] = { -1, -1 };
		const char* names[2] = { "physical_package_id", "core_id" };
		for ( int k = 0; k < 2; ++k )
		{
			snprintf( path, sizeof( path ), "/sys/devices/system/cpu/cpu%d/topology/%s", cpu, names[k] );
			FILE* file = fopen( path, "r" );
			if ( file != NULL )
			{
				if ( fscanf( file, "%d", values + k ) != 1 )
				{
					values[k] = -1;
				}
				fclose( file );
			}
		}

		if ( values[1] < 0 )
		{
			break;
		}
		if ( hybrid && ( performance[cpu >> 6] & ( (uint64_t)1 << ( cpu & 63 ) ) ) == 0 )
		{
			continue;
		}
		cores[coreCount++] = (nbCore){ values[0], values[1] };
	}

	qsort( cores, (size_t)coreCount, sizeof( nbCore ), nbCompareCores );
	int count = 0;
	for ( int i = 0; i < coreCount; ++i )
	{
		count += i == 0 || nbCompareCores( cores + i - 1, cores + i ) != 0 ? 1 : 0;
	}
	nbFree( cores, sizeof( nbCore ) * NB_MAX_CPUS );
	return count;
#endif
}

bool nbKeepFullSpeed( bool keep )
{
#if defined( _WIN32 )
	// SetProcessInformation exists from Windows 8 on and takes the power throttling from Windows 10 1709 on, older headers
	// do not have it, so it comes from kernel32 at run time
	typedef BOOL( WINAPI * nbSetProcessInformationFn )( HANDLE, int, LPVOID, DWORD );
	HMODULE kernel = GetModuleHandleW( L"kernel32.dll" );
	if ( kernel == NULL )
	{
		return false;
	}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4191 )
#endif
	nbSetProcessInformationFn setInformation =
		(nbSetProcessInformationFn)(void ( * )( void ))GetProcAddress( kernel, "SetProcessInformation" );
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
	if ( setInformation == NULL )
	{
		return false;
	}

	// PROCESS_POWER_THROTTLING_STATE for ProcessPowerThrottling, the class 4: version 1 and the bit of
	// PROCESS_POWER_THROTTLING_EXECUTION_SPEED in the control mask, but not in the state mask, keeps the speed. Neither
	// bit leaves it to the system.
	struct
	{
		ULONG version;
		ULONG controlMask;
		ULONG stateMask;
	} state = { 1, keep ? 1u : 0u, 0u };
	return setInformation( GetCurrentProcess(), 4, &state, sizeof( state ) ) != FALSE;
#else
	(void)keep;
	return false;
#endif
}

int nbGetRecommendedWorkerCount( bool asyncStep )
{
	int hardware = nbGetHardwareThreadCount();
	int performance = nbGetPerformanceCoreCount();
	int workers = performance > 0 ? performance : hardware / 2;
	workers = workers < hardware ? workers : hardware;
	if ( asyncStep && hardware > 1 )
	{
		workers = workers < hardware - 1 ? workers : hardware - 1;
	}
	return workers < 1 ? 1 : ( workers > NB_RECOMMENDED_MAX_WORKERS ? NB_RECOMMENDED_MAX_WORKERS : workers );
}
