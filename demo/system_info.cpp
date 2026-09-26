// SPDX-License-Identifier: MIT

#include "system_info.h"

#include "sokol_gfx.h"

#include <set>
#include <stdio.h>
#include <string.h>
#include <utility>
#include <vector>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if defined( SOKOL_D3D11 )
#include <d3d11.h>
#include <dxgi.h>
#endif
#elif defined( __APPLE__ )
#include <sys/sysctl.h>
#elif defined( SOKOL_GLCORE )
#include <GL/gl.h>
#endif

#if defined( _MSC_VER ) && ( defined( _M_X64 ) || defined( _M_IX86 ) )
#include <intrin.h>
#define DEMO_CPUID_MSVC
#elif ( defined( __GNUC__ ) || defined( __clang__ ) ) && ( defined( __x86_64__ ) || defined( __i386__ ) )
#include <cpuid.h>
#define DEMO_CPUID_GNU
#endif

static std::string Trim( const char* text )
{
	std::string result = text;
	size_t first = result.find_first_not_of( ' ' );
	size_t last = result.find_last_not_of( ' ' );
	return first == std::string::npos ? std::string() : result.substr( first, last - first + 1 );
}

std::string SystemCpuName()
{
	char brand[49] = {};
#if defined( DEMO_CPUID_MSVC )
	int info[4];
	__cpuid( info, (int)0x80000000 );
	if ( (unsigned)info[0] >= 0x80000004u )
	{
		for ( int i = 0; i < 3; ++i )
		{
			__cpuid( info, (int)( 0x80000002u + (unsigned)i ) );
			memcpy( brand + 16 * i, info, 16 );
		}
	}
#elif defined( DEMO_CPUID_GNU )
	unsigned int info[4];
	if ( __get_cpuid_max( 0x80000000u, nullptr ) >= 0x80000004u )
	{
		for ( unsigned int i = 0; i < 3; ++i )
		{
			__get_cpuid( 0x80000002u + i, info, info + 1, info + 2, info + 3 );
			memcpy( brand + 16 * i, info, 16 );
		}
	}
#elif defined( __APPLE__ )
	size_t size = sizeof( brand ) - 1;
	sysctlbyname( "machdep.cpu.brand_string", brand, &size, nullptr, 0 );
#endif
	return Trim( brand );
}

std::string SystemGpuName()
{
#if defined( _WIN32 ) && defined( SOKOL_D3D11 )
	std::string name;
	ID3D11Device* device = (ID3D11Device*)sg_d3d11_device();
	IDXGIDevice* dxgiDevice = nullptr;
	if ( device != nullptr && SUCCEEDED( device->QueryInterface( IID_IDXGIDevice, (void**)&dxgiDevice ) ) )
	{
		IDXGIAdapter* adapter = nullptr;
		if ( SUCCEEDED( dxgiDevice->GetAdapter( &adapter ) ) )
		{
			DXGI_ADAPTER_DESC desc;
			if ( SUCCEEDED( adapter->GetDesc( &desc ) ) )
			{
				char utf8[256] = {};
				WideCharToMultiByte( CP_UTF8, 0, desc.Description, -1, utf8, (int)sizeof( utf8 ) - 1, nullptr, nullptr );
				char memory[64];
				snprintf( memory, sizeof( memory ), ", %d MB", (int)( desc.DedicatedVideoMemory / ( 1024 * 1024 ) ) );
				name = Trim( utf8 ) + memory;
			}
			adapter->Release();
		}
		dxgiDevice->Release();
	}
	return name;
#elif defined( SOKOL_GLCORE ) && !defined( _WIN32 ) && !defined( __APPLE__ )
	const char* renderer = (const char*)glGetString( GL_RENDERER );
	return renderer != nullptr ? Trim( renderer ) : std::string();
#elif defined( __APPLE__ )
	return "Metal";
#else
	return std::string();
#endif
}

int SystemPerformanceCores()
{
#if defined( _WIN32 )
	DWORD length = 0;
	GetLogicalProcessorInformationEx( RelationProcessorCore, nullptr, &length );
	if ( length == 0 )
	{
		return 0;
	}

	std::vector<unsigned char> buffer( length );
	auto* first = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buffer.data();
	if ( GetLogicalProcessorInformationEx( RelationProcessorCore, first, &length ) == FALSE )
	{
		return 0;
	}

	// A higher efficiency class is a faster core. Without hybrid cores every core has the same class.
	int highestClass = -1;
	int count = 0;
	for ( DWORD offset = 0; offset < length; )
	{
		auto* info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)( buffer.data() + offset );
		int efficiencyClass = info->Processor.EfficiencyClass;
		if ( efficiencyClass > highestClass )
		{
			highestClass = efficiencyClass;
			count = 0;
		}
		count += efficiencyClass == highestClass ? 1 : 0;
		offset += info->Size;
	}
	return count;
#elif defined( __APPLE__ )
	int count = 0;
	size_t size = sizeof( count );
	if ( sysctlbyname( "hw.perflevel0.physicalcpu", &count, &size, nullptr, 0 ) == 0 && count > 0 )
	{
		return count;
	}
	size = sizeof( count );
	return sysctlbyname( "hw.physicalcpu", &count, &size, nullptr, 0 ) == 0 ? count : 0;
#else
	// Distinct cores in the topology of the logical processors
	std::set<std::pair<int, int>> cores;
	for ( int cpu = 0; cpu < 4096; ++cpu )
	{
		char path[128];
		int values[2] = { -1, -1 };
		const char* names[2] = { "physical_package_id", "core_id" };
		for ( int k = 0; k < 2; ++k )
		{
			snprintf( path, sizeof( path ), "/sys/devices/system/cpu/cpu%d/topology/%s", cpu, names[k] );
			FILE* file = fopen( path, "r" );
			if ( file != nullptr )
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
		cores.insert( { values[0], values[1] } );
	}
	return (int)cores.size();
#endif
}
