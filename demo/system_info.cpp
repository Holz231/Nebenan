// SPDX-License-Identifier: MIT

#include "system_info.h"

#include "sokol_gfx.h"

#include <stdio.h>
#include <string.h>

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
