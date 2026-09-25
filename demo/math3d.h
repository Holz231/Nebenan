// SPDX-License-Identifier: MIT
// Minimal matrix helpers for the demo renderer. Matrices are column major like GLSL.

#pragma once

#include "box3d/math_functions.h"

#include <math.h>

struct Vec4
{
	float x, y, z, w;
};

struct Mat4
{
	float m[16];
};

inline Mat4 Mat4Identity()
{
	Mat4 r = {};
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

inline Mat4 Mat4Mul( const Mat4& a, const Mat4& b )
{
	Mat4 r;
	for ( int c = 0; c < 4; ++c )
	{
		for ( int row = 0; row < 4; ++row )
		{
			float sum = 0.0f;
			for ( int k = 0; k < 4; ++k )
			{
				sum += a.m[k * 4 + row] * b.m[c * 4 + k];
			}
			r.m[c * 4 + row] = sum;
		}
	}
	return r;
}

// Right handed view matrix, the camera looks down -z
inline Mat4 Mat4LookAt( b3Vec3 eye, b3Vec3 target, b3Vec3 up )
{
	b3Vec3 f = b3Normalize( b3Sub( target, eye ) );
	b3Vec3 s = b3Normalize( b3Cross( f, up ) );
	b3Vec3 u = b3Cross( s, f );

	Mat4 r = Mat4Identity();
	r.m[0] = s.x;
	r.m[4] = s.y;
	r.m[8] = s.z;
	r.m[1] = u.x;
	r.m[5] = u.y;
	r.m[9] = u.z;
	r.m[2] = -f.x;
	r.m[6] = -f.y;
	r.m[10] = -f.z;
	r.m[12] = -b3Dot( s, eye );
	r.m[13] = -b3Dot( u, eye );
	r.m[14] = b3Dot( f, eye );
	return r;
}

// Clip space depth in [0, 1] for D3D11 and Metal, [-1, 1] for OpenGL
inline Mat4 Mat4Perspective( float fovY, float aspect, float nearZ, float farZ, bool zeroToOne )
{
	float f = 1.0f / tanf( 0.5f * fovY );
	Mat4 r = {};
	r.m[0] = f / aspect;
	r.m[5] = f;
	r.m[11] = -1.0f;
	if ( zeroToOne )
	{
		r.m[10] = farZ / ( nearZ - farZ );
		r.m[14] = nearZ * farZ / ( nearZ - farZ );
	}
	else
	{
		r.m[10] = ( farZ + nearZ ) / ( nearZ - farZ );
		r.m[14] = 2.0f * farZ * nearZ / ( nearZ - farZ );
	}
	return r;
}

inline Mat4 Mat4Ortho( float left, float right, float bottom, float top, float nearZ, float farZ, bool zeroToOne )
{
	Mat4 r = {};
	r.m[0] = 2.0f / ( right - left );
	r.m[5] = 2.0f / ( top - bottom );
	r.m[12] = -( right + left ) / ( right - left );
	r.m[13] = -( top + bottom ) / ( top - bottom );
	r.m[15] = 1.0f;
	if ( zeroToOne )
	{
		r.m[10] = 1.0f / ( nearZ - farZ );
		r.m[14] = nearZ / ( nearZ - farZ );
	}
	else
	{
		r.m[10] = -2.0f / ( farZ - nearZ );
		r.m[14] = -( farZ + nearZ ) / ( farZ - nearZ );
	}
	return r;
}
