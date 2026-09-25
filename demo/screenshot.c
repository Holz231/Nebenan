// SPDX-License-Identifier: MIT
// Read back the default framebuffer for automated screenshots. OpenGL only, the other backends
// render through swapchain objects that are not read back here.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#if defined( SOKOL_GLCORE ) && defined( __linux__ )
#include <GL/gl.h>

bool DemoSaveScreenshot( const char* path, int width, int height )
{
	unsigned char* pixels = malloc( (size_t)width * (size_t)height * 3 );
	if ( pixels == NULL )
	{
		return false;
	}

	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glReadBuffer( GL_BACK );
	glReadPixels( 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels );

	FILE* file = fopen( path, "wb" );
	if ( file == NULL )
	{
		free( pixels );
		return false;
	}

	// PPM rows go top to bottom, GL rows bottom to top
	fprintf( file, "P6\n%d %d\n255\n", width, height );
	for ( int y = height - 1; y >= 0; --y )
	{
		fwrite( pixels + (size_t)y * (size_t)width * 3, 1, (size_t)width * 3, file );
	}
	fclose( file );
	free( pixels );
	printf( "screenshot written to %s\n", path );
	return true;
}

#else

bool DemoSaveScreenshot( const char* path, int width, int height )
{
	(void)width;
	(void)height;
	printf( "screenshots are only supported with the OpenGL backend, skipped %s\n", path );
	return false;
}

#endif
