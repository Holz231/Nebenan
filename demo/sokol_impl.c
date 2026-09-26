// SPDX-License-Identifier: MIT
// Single translation unit for the sokol implementations. The backend macro (SOKOL_D3D11, SOKOL_METAL or
// SOKOL_GLCORE) comes from CMake. On macOS this file is compiled as Objective-C.

#if defined( __linux__ ) && !defined( _POSIX_C_SOURCE )
#define _POSIX_C_SOURCE 200809L
#endif

#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_log.h"

// Turns vertical sync on or off while the app runs. sokol_app takes the swap interval only at startup and reads
// zero as the default of one, so this sets it in the sokol state. Direct3D 11 passes it to every present, a flip
// model swap chain then shows the newest frame at the next refresh without tearing. OpenGL needs the swap
// control extension. Metal draws from the display link and stays in sync, there this returns false.
bool DemoSetVsync( bool enabled )
{
#if defined( SOKOL_METAL )
	(void)enabled;
	return false;
#else
	_sapp.swap_interval = enabled ? 1 : 0;
#if defined( _SAPP_GLX )
	_sapp_glx_swapinterval( _sapp.swap_interval );
#elif defined( _SAPP_WIN32 ) && defined( SOKOL_GLCORE )
	if ( _sapp.wgl.ext_swap_control )
	{
		_sapp.wgl.SwapIntervalEXT( _sapp.swap_interval );
	}
#endif
	return true;
#endif
}
