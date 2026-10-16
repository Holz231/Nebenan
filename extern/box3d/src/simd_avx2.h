// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

// Added for Nebenan: the wide floats of simd.h in eight lanes with AVX2, from upstream Box3D (#174), for the contact
// solver kernels that wide8.c compiles a second time, see contact_solver_wide.inl. Every operation rounds each lane exactly
// as its four lane counterpart in simd.h does, without FMA, so eight lanes compute the same bits as four.

#pragma once

#include "core.h"

#include <immintrin.h>
#include <stdbool.h>

// wide float holds 8 numbers
typedef __m256 b3FloatW;

static inline b3FloatW b3ZeroW( void )
{
	return _mm256_setzero_ps();
}

static inline b3FloatW b3SplatW( float scalar )
{
	return _mm256_set1_ps( scalar );
}

static inline b3FloatW b3SetW( float a, float b, float c, float d, float e, float f, float g, float h )
{
	return _mm256_setr_ps( a, b, c, d, e, f, g, h );
}

static inline b3FloatW b3LoadW( const float* data )
{
	return _mm256_loadu_ps( data );
}

static inline void b3StoreW( float* data, b3FloatW a )
{
	_mm256_storeu_ps( data, a );
}

static inline b3FloatW b3NegW( b3FloatW a )
{
	// Create a mask with the sign bit set for each element
	__m256 mask = _mm256_set1_ps( -0.0f );

	// XOR the input with the mask to negate each element
	return _mm256_xor_ps( a, mask );
}

static inline b3FloatW b3AbsW( b3FloatW a )
{
	return _mm256_andnot_ps( _mm256_set1_ps( -0.0f ), a );
}

static inline b3FloatW b3AddW( b3FloatW a, b3FloatW b )
{
	return _mm256_add_ps( a, b );
}

static inline b3FloatW b3SubW( b3FloatW a, b3FloatW b )
{
	return _mm256_sub_ps( a, b );
}

static inline b3FloatW b3MulW( b3FloatW a, b3FloatW b )
{
	return _mm256_mul_ps( a, b );
}

static inline b3FloatW b3DivW( b3FloatW a, b3FloatW b )
{
	return _mm256_div_ps( a, b );
}

static inline b3FloatW b3SqrtW( b3FloatW a )
{
	return _mm256_sqrt_ps( a );
}

// a + b * c, two roundings as in simd.h
static inline b3FloatW b3MulAddW( b3FloatW a, b3FloatW b, b3FloatW c )
{
	return _mm256_add_ps( a, _mm256_mul_ps( b, c ) );
}

static inline b3FloatW b3MinW( b3FloatW a, b3FloatW b )
{
	return _mm256_min_ps( a, b );
}

static inline b3FloatW b3MaxW( b3FloatW a, b3FloatW b )
{
	return _mm256_max_ps( a, b );
}

// clamp a to [-b, b]
static inline b3FloatW b3SymClampW( b3FloatW a, b3FloatW b )
{
	b3FloatW nb = b3NegW( b );
	b3FloatW c = b3MaxW( nb, a );
	return b3MinW( c, b );
}

static inline b3FloatW b3AndW( b3FloatW a, b3FloatW b )
{
	return _mm256_and_ps( a, b );
}

static inline b3FloatW b3OrW( b3FloatW a, b3FloatW b )
{
	return _mm256_or_ps( a, b );
}

// a & ~b
static inline b3FloatW b3AndNotW( b3FloatW a, b3FloatW b )
{
	// Arguments are reversed
	return _mm256_andnot_ps( b, a );
}

// This is used to optimize selection of contact softness.
static inline b3FloatW b3SoftMaskW( const int* indexA, const int* indexB )
{
	__m256i zero = _mm256_setzero_si256();
	__m256i a = _mm256_cmpeq_epi32( _mm256_loadu_si256( (const __m256i*)indexA ), zero );
	__m256i b = _mm256_cmpeq_epi32( _mm256_loadu_si256( (const __m256i*)indexB ), zero );
	return _mm256_castsi256_ps( _mm256_or_si256( a, b ) );
}

static inline b3FloatW b3GreaterThanW( b3FloatW a, b3FloatW b )
{
	return _mm256_cmp_ps( a, b, _CMP_GT_OQ );
}

static inline b3FloatW b3LessThanW( b3FloatW a, b3FloatW b )
{
	return _mm256_cmp_ps( a, b, _CMP_LT_OQ );
}

static inline b3FloatW b3EqualsW( b3FloatW a, b3FloatW b )
{
	return _mm256_cmp_ps( a, b, _CMP_EQ_OQ );
}

static inline bool b3AllZeroW( b3FloatW a )
{
	// Compare each element with zero
	b3FloatW zero = _mm256_setzero_ps();
	b3FloatW cmp = _mm256_cmp_ps( a, zero, _CMP_EQ_OQ );

	// Create a mask from the comparison results
	int mask = _mm256_movemask_ps( cmp );

	// If all elements are zero, the mask will be 0xFF (11111111 in binary)
	return mask == 0xFF;
}

static inline bool b3AnyTrueW( b3FloatW mask )
{
	return _mm256_movemask_ps( mask ) != 0;
}

// component-wise returns mask ? b : a
static inline b3FloatW b3BlendW( b3FloatW a, b3FloatW b, b3FloatW mask )
{
	return _mm256_or_ps( _mm256_and_ps( mask, b ), _mm256_andnot_ps( mask, a ) );
}

// Rows of eight floats in, columns out
B3_FORCE_INLINE void b3TransposeW( b3FloatW r0, b3FloatW r1, b3FloatW r2, b3FloatW r3, b3FloatW r4, b3FloatW r5, b3FloatW r6,
								   b3FloatW r7, b3FloatW* c0, b3FloatW* c1, b3FloatW* c2, b3FloatW* c3, b3FloatW* c4, b3FloatW* c5,
								   b3FloatW* c6, b3FloatW* c7 )
{
	b3FloatW t0 = _mm256_unpacklo_ps( r0, r1 );
	b3FloatW t1 = _mm256_unpackhi_ps( r0, r1 );
	b3FloatW t2 = _mm256_unpacklo_ps( r2, r3 );
	b3FloatW t3 = _mm256_unpackhi_ps( r2, r3 );
	b3FloatW t4 = _mm256_unpacklo_ps( r4, r5 );
	b3FloatW t5 = _mm256_unpackhi_ps( r4, r5 );
	b3FloatW t6 = _mm256_unpacklo_ps( r6, r7 );
	b3FloatW t7 = _mm256_unpackhi_ps( r6, r7 );
	b3FloatW tt0 = _mm256_shuffle_ps( t0, t2, _MM_SHUFFLE( 1, 0, 1, 0 ) );
	b3FloatW tt1 = _mm256_shuffle_ps( t0, t2, _MM_SHUFFLE( 3, 2, 3, 2 ) );
	b3FloatW tt2 = _mm256_shuffle_ps( t1, t3, _MM_SHUFFLE( 1, 0, 1, 0 ) );
	b3FloatW tt3 = _mm256_shuffle_ps( t1, t3, _MM_SHUFFLE( 3, 2, 3, 2 ) );
	b3FloatW tt4 = _mm256_shuffle_ps( t4, t6, _MM_SHUFFLE( 1, 0, 1, 0 ) );
	b3FloatW tt5 = _mm256_shuffle_ps( t4, t6, _MM_SHUFFLE( 3, 2, 3, 2 ) );
	b3FloatW tt6 = _mm256_shuffle_ps( t5, t7, _MM_SHUFFLE( 1, 0, 1, 0 ) );
	b3FloatW tt7 = _mm256_shuffle_ps( t5, t7, _MM_SHUFFLE( 3, 2, 3, 2 ) );

	*c0 = _mm256_permute2f128_ps( tt0, tt4, 0x20 );
	*c1 = _mm256_permute2f128_ps( tt1, tt5, 0x20 );
	*c2 = _mm256_permute2f128_ps( tt2, tt6, 0x20 );
	*c3 = _mm256_permute2f128_ps( tt3, tt7, 0x20 );
	*c4 = _mm256_permute2f128_ps( tt0, tt4, 0x31 );
	*c5 = _mm256_permute2f128_ps( tt1, tt5, 0x31 );
	*c6 = _mm256_permute2f128_ps( tt2, tt6, 0x31 );
	*c7 = _mm256_permute2f128_ps( tt3, tt7, 0x31 );
}
