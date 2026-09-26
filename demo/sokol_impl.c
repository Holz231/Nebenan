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

#if defined( SOKOL_D3D11 )
#include <dxgi1_5.h>

// sokol creates a flip model swap chain, and Windows holds a flip model swap chain to the refresh rate of the
// display even at sync interval 0, unless it was created with DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING and each present
// at sync interval 0 passes DXGI_PRESENT_ALLOW_TEARING. sokol passes no flags, so the demo replaces the swap chain
// with one that allows tearing and wraps it: the wrapper adds the flags to the presents and resizes of sokol and
// forwards everything else. In a window the desktop compositor still shows whole frames, so nothing tears there.
typedef struct DemoSwapChain
{
	IDXGISwapChain base;
	IDXGISwapChain* inner;
	LONG references;
} DemoSwapChain;

static const IID s_iidUnknown = { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const IID s_iidObject = { 0xaec22fb8, 0x76f3, 0x4639, { 0x9b, 0xe0, 0x28, 0xeb, 0x43, 0xa6, 0x7a, 0x2e } };
static const IID s_iidDeviceSubObject = { 0x3d3e0379, 0xf9de, 0x4d58, { 0xbb, 0x6c, 0x18, 0xd6, 0x29, 0x92, 0xf1, 0xa6 } };
static const IID s_iidSwapChain = { 0x310d36a0, 0xd2e7, 0x4c0a, { 0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a } };
static const IID s_iidFactory2 = { 0x50c83a1c, 0xe072, 0x4c48, { 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0 } };
static const IID s_iidFactory5 = { 0x7632e1f5, 0xee65, 0x4dca, { 0x87, 0xfd, 0x84, 0xcd, 0x75, 0xf8, 0x83, 0x8d } };

static IDXGISwapChain* DemoInner( IDXGISwapChain* self )
{
	return ( (DemoSwapChain*)self )->inner;
}

static HRESULT STDMETHODCALLTYPE DemoQueryInterface( IDXGISwapChain* self, REFIID riid, void** object )
{
	if ( IsEqualIID( riid, &s_iidUnknown ) || IsEqualIID( riid, &s_iidObject ) || IsEqualIID( riid, &s_iidDeviceSubObject ) ||
		 IsEqualIID( riid, &s_iidSwapChain ) )
	{
		InterlockedIncrement( &( (DemoSwapChain*)self )->references );
		*object = self;
		return S_OK;
	}
	return DemoInner( self )->lpVtbl->QueryInterface( DemoInner( self ), riid, object );
}

static ULONG STDMETHODCALLTYPE DemoAddRef( IDXGISwapChain* self )
{
	return (ULONG)InterlockedIncrement( &( (DemoSwapChain*)self )->references );
}

static ULONG STDMETHODCALLTYPE DemoRelease( IDXGISwapChain* self )
{
	DemoSwapChain* chain = (DemoSwapChain*)self;
	ULONG count = (ULONG)InterlockedDecrement( &chain->references );
	if ( count == 0 )
	{
		chain->inner->lpVtbl->Release( chain->inner );
		free( chain );
	}
	return count;
}

static HRESULT STDMETHODCALLTYPE DemoSetPrivateData( IDXGISwapChain* self, REFGUID guid, UINT size, const void* data )
{
	return DemoInner( self )->lpVtbl->SetPrivateData( DemoInner( self ), guid, size, data );
}

static HRESULT STDMETHODCALLTYPE DemoSetPrivateDataInterface( IDXGISwapChain* self, REFGUID guid, const IUnknown* object )
{
	return DemoInner( self )->lpVtbl->SetPrivateDataInterface( DemoInner( self ), guid, object );
}

static HRESULT STDMETHODCALLTYPE DemoGetPrivateData( IDXGISwapChain* self, REFGUID guid, UINT* size, void* data )
{
	return DemoInner( self )->lpVtbl->GetPrivateData( DemoInner( self ), guid, size, data );
}

static HRESULT STDMETHODCALLTYPE DemoGetParent( IDXGISwapChain* self, REFIID riid, void** parent )
{
	return DemoInner( self )->lpVtbl->GetParent( DemoInner( self ), riid, parent );
}

static HRESULT STDMETHODCALLTYPE DemoGetDevice( IDXGISwapChain* self, REFIID riid, void** device )
{
	return DemoInner( self )->lpVtbl->GetDevice( DemoInner( self ), riid, device );
}

static HRESULT STDMETHODCALLTYPE DemoPresent( IDXGISwapChain* self, UINT syncInterval, UINT flags )
{
	// Not while the window is dragged, there sokol presents without waiting
	if ( syncInterval == 0 && ( flags & DXGI_PRESENT_DO_NOT_WAIT ) == 0 )
	{
		flags |= DXGI_PRESENT_ALLOW_TEARING;
	}
	return DemoInner( self )->lpVtbl->Present( DemoInner( self ), syncInterval, flags );
}

static HRESULT STDMETHODCALLTYPE DemoGetBuffer( IDXGISwapChain* self, UINT index, REFIID riid, void** surface )
{
	return DemoInner( self )->lpVtbl->GetBuffer( DemoInner( self ), index, riid, surface );
}

static HRESULT STDMETHODCALLTYPE DemoSetFullscreenState( IDXGISwapChain* self, BOOL fullscreen, IDXGIOutput* target )
{
	return DemoInner( self )->lpVtbl->SetFullscreenState( DemoInner( self ), fullscreen, target );
}

static HRESULT STDMETHODCALLTYPE DemoGetFullscreenState( IDXGISwapChain* self, BOOL* fullscreen, IDXGIOutput** target )
{
	return DemoInner( self )->lpVtbl->GetFullscreenState( DemoInner( self ), fullscreen, target );
}

static HRESULT STDMETHODCALLTYPE DemoGetDesc( IDXGISwapChain* self, DXGI_SWAP_CHAIN_DESC* desc )
{
	return DemoInner( self )->lpVtbl->GetDesc( DemoInner( self ), desc );
}

static HRESULT STDMETHODCALLTYPE DemoResizeBuffers( IDXGISwapChain* self, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags )
{
	// A resize has to pass the flags the swap chain was created with
	return DemoInner( self )->lpVtbl->ResizeBuffers( DemoInner( self ), count, width, height, format,
													 flags | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING );
}

static HRESULT STDMETHODCALLTYPE DemoResizeTarget( IDXGISwapChain* self, const DXGI_MODE_DESC* mode )
{
	return DemoInner( self )->lpVtbl->ResizeTarget( DemoInner( self ), mode );
}

static HRESULT STDMETHODCALLTYPE DemoGetContainingOutput( IDXGISwapChain* self, IDXGIOutput** output )
{
	return DemoInner( self )->lpVtbl->GetContainingOutput( DemoInner( self ), output );
}

static HRESULT STDMETHODCALLTYPE DemoGetFrameStatistics( IDXGISwapChain* self, DXGI_FRAME_STATISTICS* stats )
{
	return DemoInner( self )->lpVtbl->GetFrameStatistics( DemoInner( self ), stats );
}

static HRESULT STDMETHODCALLTYPE DemoGetLastPresentCount( IDXGISwapChain* self, UINT* count )
{
	return DemoInner( self )->lpVtbl->GetLastPresentCount( DemoInner( self ), count );
}

static IDXGISwapChainVtbl s_demoSwapChainVtbl = {
	.QueryInterface = DemoQueryInterface,
	.AddRef = DemoAddRef,
	.Release = DemoRelease,
	.SetPrivateData = DemoSetPrivateData,
	.SetPrivateDataInterface = DemoSetPrivateDataInterface,
	.GetPrivateData = DemoGetPrivateData,
	.GetParent = DemoGetParent,
	.GetDevice = DemoGetDevice,
	.Present = DemoPresent,
	.GetBuffer = DemoGetBuffer,
	.SetFullscreenState = DemoSetFullscreenState,
	.GetFullscreenState = DemoGetFullscreenState,
	.GetDesc = DemoGetDesc,
	.ResizeBuffers = DemoResizeBuffers,
	.ResizeTarget = DemoResizeTarget,
	.GetContainingOutput = DemoGetContainingOutput,
	.GetFrameStatistics = DemoGetFrameStatistics,
	.GetLastPresentCount = DemoGetLastPresentCount,
};

// Replaces sokol's swap chain with one that allows tearing, when the system supports it. Returns false and keeps
// sokol's swap chain otherwise. Call it before the first pass.
static bool DemoCreateTearingSwapChain( void )
{
	IDXGIAdapter* adapter = NULL;
	IDXGIFactory2* factory = NULL;
	IDXGIFactory5* factory5 = NULL;
	BOOL tearing = FALSE;
	if ( _sapp.d3d11.dxgi_device == NULL || FAILED( _sapp_dxgi_GetAdapter( _sapp.d3d11.dxgi_device, &adapter ) ) )
	{
		return false;
	}
	HRESULT hr = adapter->lpVtbl->GetParent( adapter, &s_iidFactory2, (void**)&factory );
	adapter->lpVtbl->Release( adapter );
	if ( FAILED( hr ) )
	{
		return false;
	}
	if ( SUCCEEDED( factory->lpVtbl->QueryInterface( factory, &s_iidFactory5, (void**)&factory5 ) ) )
	{
		if ( FAILED( factory5->lpVtbl->CheckFeatureSupport( factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof( tearing ) ) ) )
		{
			tearing = FALSE;
		}
		factory5->lpVtbl->Release( factory5 );
	}

	DemoSwapChain* chain = tearing ? (DemoSwapChain*)calloc( 1, sizeof( DemoSwapChain ) ) : NULL;
	if ( chain == NULL )
	{
		factory->lpVtbl->Release( factory );
		return false;
	}

	// Only one flip model swap chain may target a window, and the old one goes away only once the context lets go
	_sapp_d3d11_destroy_default_render_target();
	_sapp.d3d11.swap_chain->lpVtbl->Release( _sapp.d3d11.swap_chain );
	_sapp.d3d11.swap_chain = NULL;
	_sapp.d3d11.device_context->lpVtbl->ClearState( _sapp.d3d11.device_context );
	_sapp.d3d11.device_context->lpVtbl->Flush( _sapp.d3d11.device_context );

	// A third buffer, so a new frame never waits for the one the display still shows
	DXGI_SWAP_CHAIN_DESC1 desc;
	ZeroMemory( &desc, sizeof( desc ) );
	desc.Width = (UINT)_sapp.framebuffer_width;
	desc.Height = (UINT)_sapp.framebuffer_height;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 3;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	IDXGISwapChain1* inner = NULL;
	hr = factory->lpVtbl->CreateSwapChainForHwnd( factory, (IUnknown*)_sapp.d3d11.device, _sapp.win32.hwnd, &desc, NULL, NULL, &inner );
	bool created = SUCCEEDED( hr ) && inner != NULL;
	if ( created )
	{
		chain->base.lpVtbl = &s_demoSwapChainVtbl;
		chain->inner = (IDXGISwapChain*)inner;
		chain->references = 1;
		_sapp.d3d11.swap_chain = &chain->base;
		_sapp.d3d11.swap_chain_desc.BufferCount = desc.BufferCount;
	}
	else
	{
		// Back to the swap chain sokol had
		free( chain );
		hr = factory->lpVtbl->CreateSwapChain( factory, (IUnknown*)_sapp.d3d11.device, &_sapp.d3d11.swap_chain_desc, &_sapp.d3d11.swap_chain );
		SOKOL_ASSERT( SUCCEEDED( hr ) && _sapp.d3d11.swap_chain );
	}
	factory->lpVtbl->MakeWindowAssociation( factory, _sapp.win32.hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN );
	factory->lpVtbl->Release( factory );
	_sapp_d3d11_create_default_render_target();
	return created;
}
#endif

// Whether frames can come faster than the display refreshes with vertical sync off
static bool s_unthrottled = true;

// Turns vertical sync on or off while the app runs. sokol_app takes the swap interval only at startup and reads
// zero as the default of one, so this sets it in the sokol state. Direct3D 11 passes it to every present, OpenGL
// needs the swap control extension. Metal draws from the display link and stays in sync, there this returns false.
// Call it between frames or before the pass of a frame begins.
bool DemoSetVsync( bool enabled )
{
#if defined( SOKOL_METAL )
	(void)enabled;
	return false;
#else
	_sapp.swap_interval = enabled ? 1 : 0;
#if defined( SOKOL_D3D11 )
	static bool replaced = false;
	if ( replaced == false )
	{
		replaced = true;
		s_unthrottled = DemoCreateTearingSwapChain();
	}
#elif defined( _SAPP_GLX )
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

bool DemoUnthrottled( void )
{
	return s_unthrottled;
}

// Refresh rate of the monitor the window is on in Hz, zero if unknown
int DemoRefreshRate( void )
{
#if defined( _SAPP_WIN32 )
	HMONITOR monitor = MonitorFromWindow( _sapp.win32.hwnd, MONITOR_DEFAULTTOPRIMARY );
	MONITORINFOEXW info;
	ZeroMemory( &info, sizeof( info ) );
	( (MONITORINFO*)&info )->cbSize = sizeof( info );
	DEVMODEW mode;
	ZeroMemory( &mode, sizeof( mode ) );
	mode.dmSize = sizeof( mode );
	if ( GetMonitorInfoW( monitor, (MONITORINFO*)&info ) && EnumDisplaySettingsW( info.szDevice, ENUM_CURRENT_SETTINGS, &mode ) &&
		 mode.dmDisplayFrequency > 1 )
	{
		return (int)mode.dmDisplayFrequency;
	}
#endif
	return 0;
}
