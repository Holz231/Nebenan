// SPDX-License-Identifier: MIT

#include "image_file.h"

#include <stdio.h>
#include <string.h>

static uint32_t ReadU32( const uint8_t* bytes )
{
	return (uint32_t)bytes[0] | ( (uint32_t)bytes[1] << 8 ) | ( (uint32_t)bytes[2] << 16 ) | ( (uint32_t)bytes[3] << 24 );
}

static uint16_t ReadU16( const uint8_t* bytes )
{
	return (uint16_t)( bytes[0] | ( bytes[1] << 8 ) );
}

static bool ReadWholeFile( const char* path, std::vector<uint8_t>& bytes )
{
	FILE* file = fopen( path, "rb" );
	if ( file == nullptr )
	{
		return false;
	}
	fseek( file, 0, SEEK_END );
	long size = ftell( file );
	fseek( file, 0, SEEK_SET );
	bool ok = size > 0;
	if ( ok )
	{
		bytes.resize( (size_t)size );
		ok = fread( bytes.data(), 1, (size_t)size, file ) == (size_t)size;
	}
	fclose( file );
	return ok;
}

// BMP with a BITMAPINFOHEADER or newer, 24 or 32 bits per pixel, uncompressed, rows bottom up or top down
static bool LoadBmp( const std::vector<uint8_t>& bytes, int* width, int* height, std::vector<uint8_t>& pixels )
{
	if ( bytes.size() < 54 || bytes[0] != 'B' || bytes[1] != 'M' )
	{
		return false;
	}
	uint32_t offset = ReadU32( &bytes[10] );
	uint32_t headerSize = ReadU32( &bytes[14] );
	int32_t w = (int32_t)ReadU32( &bytes[18] );
	int32_t h = (int32_t)ReadU32( &bytes[22] );
	uint16_t bits = ReadU16( &bytes[28] );
	uint32_t compression = ReadU32( &bytes[30] );
	if ( headerSize < 40 || w <= 0 || h == 0 || ( bits != 24 && bits != 32 ) || ( compression != 0 && compression != 3 ) )
	{
		return false;
	}

	bool topDown = h < 0;
	int rows = topDown ? -h : h;
	size_t stride = ( ( (size_t)w * bits / 8 ) + 3 ) & ~(size_t)3;
	if ( offset + stride * (size_t)rows > bytes.size() )
	{
		return false;
	}

	pixels.resize( (size_t)w * (size_t)rows * 4 );
	for ( int y = 0; y < rows; ++y )
	{
		const uint8_t* row = &bytes[offset + stride * (size_t)( topDown ? y : rows - 1 - y )];
		uint8_t* out = &pixels[(size_t)y * (size_t)w * 4];
		for ( int x = 0; x < w; ++x )
		{
			const uint8_t* p = row + (size_t)x * bits / 8;
			out[4 * x + 0] = p[2];
			out[4 * x + 1] = p[1];
			out[4 * x + 2] = p[0];
			out[4 * x + 3] = 255;
		}
	}
	*width = w;
	*height = rows;
	return true;
}

// Binary PPM, P6, with 8 bits per channel
static bool LoadPpm( const std::vector<uint8_t>& bytes, int* width, int* height, std::vector<uint8_t>& pixels )
{
	if ( bytes.size() < 3 || bytes[0] != 'P' || bytes[1] != '6' )
	{
		return false;
	}

	// Width, height and the largest value, separated by white space and comments
	size_t at = 2;
	int values[3] = {};
	for ( int v = 0; v < 3; ++v )
	{
		while ( at < bytes.size() &&
				( bytes[at] == ' ' || bytes[at] == '\t' || bytes[at] == '\r' || bytes[at] == '\n' || bytes[at] == '#' ) )
		{
			if ( bytes[at] == '#' )
			{
				while ( at < bytes.size() && bytes[at] != '\n' )
				{
					at += 1;
				}
			}
			else
			{
				at += 1;
			}
		}
		while ( at < bytes.size() && bytes[at] >= '0' && bytes[at] <= '9' )
		{
			values[v] = 10 * values[v] + ( bytes[at] - '0' );
			at += 1;
		}
	}
	at += 1;
	int w = values[0];
	int h = values[1];
	if ( w <= 0 || h <= 0 || values[2] != 255 || at + (size_t)w * (size_t)h * 3 > bytes.size() )
	{
		return false;
	}

	pixels.resize( (size_t)w * (size_t)h * 4 );
	for ( size_t i = 0; i < (size_t)w * (size_t)h; ++i )
	{
		pixels[4 * i + 0] = bytes[at + 3 * i + 0];
		pixels[4 * i + 1] = bytes[at + 3 * i + 1];
		pixels[4 * i + 2] = bytes[at + 3 * i + 2];
		pixels[4 * i + 3] = 255;
	}
	*width = w;
	*height = h;
	return true;
}

bool LoadImageFile( const char* path, int* width, int* height, std::vector<uint8_t>& pixels )
{
	std::vector<uint8_t> bytes;
	if ( ReadWholeFile( path, bytes ) == false )
	{
		return false;
	}
	return LoadBmp( bytes, width, height, pixels ) || LoadPpm( bytes, width, height, pixels );
}
