// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

// Added for Nebenan: the convex contact constraints, wide in B3_SIMD_WIDTH lanes. contact_solver.c compiles them in four
// lanes and wide8.c in eight with AVX2, chosen at run time, see b3GetSIMDWidth. B3_WIDE names the functions after the width,
// and the including file brings the wide floats, simd.h for four lanes and simd_avx2.h for eight. As in upstream Box3D
// (#174), a lane computes the same bits in either width: Contacts of one color share no moving body, so how they are
// bundled changes nothing, and an empty lane never writes back.

// The wide types are named after the width too, so the two widths never share a name
#define b3Vec2W B3_WIDE( b3Vec2 )
#define b3Vec3W B3_WIDE( b3Vec3 )
#define b3QuatW B3_WIDE( b3Quat )
#define b3SymMatrix2W B3_WIDE( b3SymMatrix2 )
#define b3SymMatrix3W B3_WIDE( b3SymMatrix3 )
#define b3Matrix3W B3_WIDE( b3Matrix3 )
#define b3BodyStateW B3_WIDE( b3BodyState )
#define b3ContactConstraintPointWide B3_WIDE( b3ContactConstraintPoint )
#define b3ContactConstraintWide B3_WIDE( b3ContactConstraint )
#define b3PushConstantsW B3_WIDE( b3PushConstants )
#define b3RelaxConstantsW B3_WIDE( b3RelaxConstants )
#define b3RestitutionConstantsW B3_WIDE( b3RestitutionConstants )

// Wide vec2
typedef struct b3Vec2W
{
	b3FloatW x, y;
} b3Vec2W;

// Wide vec3
typedef struct b3Vec3W
{
	b3FloatW X, Y, Z;
} b3Vec3W;

// Wide quaternion
typedef struct b3QuatW
{
	b3Vec3W V;
	b3FloatW S;
} b3QuatW;

// Wide symmetric matrix2
typedef struct b3SymMatrix2W
{
	b3FloatW cxx, cxy, cyy;
} b3SymMatrix2W;

// Wide symmetric matrix3
typedef struct b3SymMatrix3W
{
	b3FloatW cxx, cxy, cxz, cyy, cyz, czz;
} b3SymMatrix3W;

typedef struct b3Matrix3W
{
	b3Vec3W cx, cy, cz;
} b3Matrix3W;

// s * a
static inline b3Vec3W b3MulSVW( b3FloatW s, b3Vec3W a )
{
	return (b3Vec3W){ b3MulW( s, a.X ), b3MulW( s, a.Y ), b3MulW( s, a.Z ) };
}

// a - s * b
static inline b3Vec3W b3MulSubSVW( b3Vec3W a, b3FloatW s, b3Vec3W b )
{
	return (b3Vec3W){ b3SubW( a.X, b3MulW( s, b.X ) ), b3SubW( a.Y, b3MulW( s, b.Y ) ), b3SubW( a.Z, b3MulW( s, b.Z ) ) };
}

// a + s * b
static inline b3Vec3W b3MulAddSVW( b3Vec3W a, b3FloatW s, b3Vec3W b )
{
	return (b3Vec3W){ b3AddW( a.X, b3MulW( s, b.X ) ), b3AddW( a.Y, b3MulW( s, b.Y ) ), b3AddW( a.Z, b3MulW( s, b.Z ) ) };
}

// a + b
static inline b3Vec2W b3AddV2W( b3Vec2W a, b3Vec2W b )
{
	return (b3Vec2W){
		b3AddW( a.x, b.x ),
		b3AddW( a.y, b.y ),
	};
}

// a - b
static inline b3Vec3W b3SubVW( b3Vec3W a, b3Vec3W b )
{
	return (b3Vec3W){
		b3SubW( a.X, b.X ),
		b3SubW( a.Y, b.Y ),
		b3SubW( a.Z, b.Z ),
	};
}

// a + b
static inline b3Vec3W b3AddVW( b3Vec3W a, b3Vec3W b )
{
	return (b3Vec3W){
		b3AddW( a.X, b.X ),
		b3AddW( a.Y, b.Y ),
		b3AddW( a.Z, b.Z ),
	};
}

// m * a
static inline b3Vec2W b3MulMV2W( b3SymMatrix2W m, b3Vec2W a )
{
	b3Vec2W b = {
		b3AddW( b3MulW( m.cxx, a.x ), b3MulW( m.cxy, a.y ) ),
		b3AddW( b3MulW( m.cxy, a.x ), b3MulW( m.cyy, a.y ) ),
	};

	return b;
}

// m * a
static inline b3Vec3W b3MulMVW( b3SymMatrix3W m, b3Vec3W a )
{
	b3Vec3W b = {
		b3AddW( b3MulW( m.cxx, a.X ), b3AddW( b3MulW( m.cxy, a.Y ), b3MulW( m.cxz, a.Z ) ) ),
		b3AddW( b3MulW( m.cxy, a.X ), b3AddW( b3MulW( m.cyy, a.Y ), b3MulW( m.cyz, a.Z ) ) ),
		b3AddW( b3MulW( m.cxz, a.X ), b3AddW( b3MulW( m.cyz, a.Y ), b3MulW( m.czz, a.Z ) ) ),
	};

	return b;
}

// a - m * b
static inline b3Vec3W b3MulSubMVW( b3Vec3W a, b3SymMatrix3W m, b3Vec3W b )
{
	b3Vec3W c = {
		b3AddW( b3MulW( m.cxx, b.X ), b3AddW( b3MulW( m.cxy, b.Y ), b3MulW( m.cxz, b.Z ) ) ),
		b3AddW( b3MulW( m.cxy, b.X ), b3AddW( b3MulW( m.cyy, b.Y ), b3MulW( m.cyz, b.Z ) ) ),
		b3AddW( b3MulW( m.cxz, b.X ), b3AddW( b3MulW( m.cyz, b.Y ), b3MulW( m.czz, b.Z ) ) ),
	};

	return (b3Vec3W){ b3SubW( a.X, c.X ), b3SubW( a.Y, c.Y ), b3SubW( a.Z, c.Z ) };
}

// a + m * b
static inline b3Vec3W b3MulAddMVW( b3Vec3W a, b3SymMatrix3W m, b3Vec3W b )
{
	b3Vec3W c = {
		b3AddW( b3MulW( m.cxx, b.X ), b3AddW( b3MulW( m.cxy, b.Y ), b3MulW( m.cxz, b.Z ) ) ),
		b3AddW( b3MulW( m.cxy, b.X ), b3AddW( b3MulW( m.cyy, b.Y ), b3MulW( m.cyz, b.Z ) ) ),
		b3AddW( b3MulW( m.cxz, b.X ), b3AddW( b3MulW( m.cyz, b.Y ), b3MulW( m.czz, b.Z ) ) ),
	};

	return (b3Vec3W){ b3AddW( a.X, c.X ), b3AddW( a.Y, c.Y ), b3AddW( a.Z, c.Z ) };
}

static inline b3FloatW b3DotW( b3Vec3W a, b3Vec3W b )
{
	return b3AddW( b3AddW( b3MulW( a.X, b.X ), b3MulW( a.Y, b.Y ) ), b3MulW( a.Z, b.Z ) );
}

static inline b3Vec3W b3CrossW( b3Vec3W a, b3Vec3W b )
{
	b3Vec3W c;
	c.X = b3SubW( b3MulW( a.Y, b.Z ), b3MulW( a.Z, b.Y ) );
	c.Y = b3SubW( b3MulW( a.Z, b.X ), b3MulW( a.X, b.Z ) );
	c.Z = b3SubW( b3MulW( a.X, b.Y ), b3MulW( a.Y, b.X ) );
	return c;
}

static inline b3Matrix3W b3MakeMatrixFromQuatW( b3QuatW q )
{
	b3FloatW x2 = b3AddW( q.V.X, q.V.X );
	b3FloatW y2 = b3AddW( q.V.Y, q.V.Y );
	b3FloatW z2 = b3AddW( q.V.Z, q.V.Z );
	b3FloatW xx2 = b3MulW( q.V.X, x2 );
	b3FloatW yy2 = b3MulW( q.V.Y, y2 );
	b3FloatW zz2 = b3MulW( q.V.Z, z2 );
	b3FloatW xy2 = b3MulW( q.V.X, y2 );
	b3FloatW xz2 = b3MulW( q.V.X, z2 );
	b3FloatW yz2 = b3MulW( q.V.Y, z2 );
	b3FloatW xw2 = b3MulW( q.S, x2 );
	b3FloatW yw2 = b3MulW( q.S, y2 );
	b3FloatW zw2 = b3MulW( q.S, z2 );
	b3FloatW one = b3SplatW( 1.0f );

	b3Matrix3W m;
	m.cx.X = b3SubW( one, b3AddW( yy2, zz2 ) );
	m.cx.Y = b3AddW( xy2, zw2 );
	m.cx.Z = b3SubW( xz2, yw2 );
	m.cy.X = b3SubW( xy2, zw2 );
	m.cy.Y = b3SubW( one, b3AddW( xx2, zz2 ) );
	m.cy.Z = b3AddW( yz2, xw2 );
	m.cz.X = b3AddW( xz2, yw2 );
	m.cz.Y = b3SubW( yz2, xw2 );
	m.cz.Z = b3SubW( one, b3AddW( xx2, yy2 ) );
	return m;
}

static inline b3Vec3W b3MulM3VW( b3Matrix3W m, b3Vec3W a )
{
	b3Vec3W b = {
		b3AddW( b3MulW( m.cx.X, a.X ), b3AddW( b3MulW( m.cy.X, a.Y ), b3MulW( m.cz.X, a.Z ) ) ),
		b3AddW( b3MulW( m.cx.Y, a.X ), b3AddW( b3MulW( m.cy.Y, a.Y ), b3MulW( m.cz.Y, a.Z ) ) ),
		b3AddW( b3MulW( m.cx.Z, a.X ), b3AddW( b3MulW( m.cy.Z, a.Y ), b3MulW( m.cz.Z, a.Z ) ) ),
	};

	return b;
}

static inline b3Vec3W b3InvRotateVectorW( b3QuatW q, b3Vec3W a )
{
	b3Vec3W t = b3CrossW( q.V, a );
	t = (b3Vec3W){ b3AddW( t.X, t.X ), b3AddW( t.Y, t.Y ), b3AddW( t.Z, t.Z ) };
	b3Vec3W u = b3CrossW( q.V, t );
	return (b3Vec3W){
		b3AddW( b3SubW( a.X, b3MulW( q.S, t.X ) ), u.X ),
		b3AddW( b3SubW( a.Y, b3MulW( q.S, t.Y ) ), u.Y ),
		b3AddW( b3SubW( a.Z, b3MulW( q.S, t.Z ) ), u.Z ),
	};
}

// Soft contact constraints with sub-stepping support
// Uses fixed anchors for Jacobians for better behavior on rolling shapes (circles & capsules)
// http://mmacklin.com/smallsteps.pdf
// https://box2d.org/files/ErinCatto_SoftConstraints_GDC2011.pdf

typedef struct b3ContactConstraintPointWide
{
	b3Vec3W anchorAs, anchorBs;
	b3FloatW baseSeparations;
	b3FloatW normalImpulses;
	b3FloatW totalNormalImpulses;
	b3FloatW normalMasses;
	b3FloatW leverArms;
	b3FloatW relativeVelocities;
	b3FloatW restitutionImpulses;
} b3ContactConstraintPointWide;

// Solves one contact per lane
typedef struct b3ContactConstraintWide
{
	// These are base 1
	int indexA[B3_SIMD_WIDTH];
	int indexB[B3_SIMD_WIDTH];

	int pointCounts[B3_SIMD_WIDTH];

	b3FloatW invMassA, invMassB;
	b3SymMatrix3W invIA, invIB;
	b3Vec3W normal;

	// todo test computing the tangents on the fly, at least tangent2
	b3Vec3W tangent1;
	b3Vec3W tangent2;

	// Friction centers
	b3Vec3W centerA, centerB;
	b3FloatW twistMass;
	b3FloatW twistImpulse;
	b3SymMatrix2W tangentMass;
	b3Vec2W frictionImpulse;
	b3SymMatrix3W rollingMass;
	b3Vec3W rollingImpulse;
	b3FloatW friction;
	b3FloatW rollingResistance;
	b3FloatW tangentVelocity1;
	b3FloatW tangentVelocity2;
	b3FloatW restitution;

	b3Manifold* manifolds[B3_SIMD_WIDTH];

	// todo store the maximum point count per wide constraint
	// to make this work I need zero initialization which is too
	// expensive for all the wide constraint data. Instead
	// the graph color should store the point count as a compact secondary
	// transient array with zero initialization.
	b3ContactConstraintPointWide points[B3_MAX_MANIFOLD_POINTS];

} b3ContactConstraintWide;

int B3_WIDE( b3GetWideContactConstraintByteCount )( void )
{
	return sizeof( b3ContactConstraintWide );
}

// wide version of b3BodyState
typedef struct b3BodyStateW
{
	b3Vec3W v;
	b3Vec3W w;
	b3Vec3W dp;
	b3QuatW dq;
} b3BodyStateW;

_Static_assert( sizeof( b3BodyState ) == 64, "body state layout" );
_Static_assert( offsetof( b3BodyState, linearVelocity ) == 0 && offsetof( b3BodyState, angularVelocity ) == 16 &&
					offsetof( b3BodyState, deltaPosition ) == 32 && offsetof( b3BodyState, deltaRotation ) == 48,
				"body state layout" );

#if B3_SIMD_WIDTH == 8

// Added for Nebenan: eight lanes with AVX2, from upstream Box3D (#174). One load holds the linear velocity, the angular
// velocity and their padding, and the stores write the same bytes as the two of four lanes.
B3_FORCE_INLINE b3BodyStateW b3GatherBodies( const b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices )
{
	const float* identity = (const float*)&b3_identityBodyState;

	// Indices are 0 for null
	const float* p1 = indices[0] == 0 ? identity : (const float*)( states + indices[0] - 1 );
	const float* p2 = indices[1] == 0 ? identity : (const float*)( states + indices[1] - 1 );
	const float* p3 = indices[2] == 0 ? identity : (const float*)( states + indices[2] - 1 );
	const float* p4 = indices[3] == 0 ? identity : (const float*)( states + indices[3] - 1 );
	const float* p5 = indices[4] == 0 ? identity : (const float*)( states + indices[4] - 1 );
	const float* p6 = indices[5] == 0 ? identity : (const float*)( states + indices[5] - 1 );
	const float* p7 = indices[6] == 0 ? identity : (const float*)( states + indices[6] - 1 );
	const float* p8 = indices[7] == 0 ? identity : (const float*)( states + indices[7] - 1 );

	b3BodyStateW s;
	b3FloatW pad;
	b3TransposeW( b3LoadW( p1 ), b3LoadW( p2 ), b3LoadW( p3 ), b3LoadW( p4 ), b3LoadW( p5 ), b3LoadW( p6 ), b3LoadW( p7 ),
				  b3LoadW( p8 ), &s.v.X, &s.v.Y, &s.v.Z, &pad, &s.w.X, &s.w.Y, &s.w.Z, &pad );
	b3TransposeW( b3LoadW( p1 + 8 ), b3LoadW( p2 + 8 ), b3LoadW( p3 + 8 ), b3LoadW( p4 + 8 ), b3LoadW( p5 + 8 ), b3LoadW( p6 + 8 ),
				  b3LoadW( p7 + 8 ), b3LoadW( p8 + 8 ), &s.dp.X, &s.dp.Y, &s.dp.Z, &pad, &s.dq.V.X, &s.dq.V.Y, &s.dq.V.Z, &s.dq.S );
	return s;
}

B3_FORCE_INLINE void b3StoreBodyVelocity( b3BodyState* B3_RESTRICT states, int index, b3FloatW vw )
{
	// Indices are 0 for null
	if ( index == 0 )
	{
		return;
	}

	b3BodyState* state = states + index - 1;
	uint32_t flags = state->flags;
	if ( ( flags & b3_dynamicFlag ) == 0 )
	{
		return;
	}

	b3StoreW( (float*)state, vw );
}

// This writes only the velocities back to the solver bodies
B3_FORCE_INLINE void b3ScatterBodies( b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices,
									  const b3BodyStateW* B3_RESTRICT simdBody )
{
	// I don't use any dummy body in the body array because this will lead to multithreaded sharing and the
	// associated cache flushing.
	b3FloatW zero = b3ZeroW();
	b3FloatW vw1, vw2, vw3, vw4, vw5, vw6, vw7, vw8;
	b3TransposeW( simdBody->v.X, simdBody->v.Y, simdBody->v.Z, zero, simdBody->w.X, simdBody->w.Y, simdBody->w.Z, zero, &vw1, &vw2,
				  &vw3, &vw4, &vw5, &vw6, &vw7, &vw8 );

	b3StoreBodyVelocity( states, indices[0], vw1 );
	b3StoreBodyVelocity( states, indices[1], vw2 );
	b3StoreBodyVelocity( states, indices[2], vw3 );
	b3StoreBodyVelocity( states, indices[3], vw4 );
	b3StoreBodyVelocity( states, indices[4], vw5 );
	b3StoreBodyVelocity( states, indices[5], vw6 );
	b3StoreBodyVelocity( states, indices[6], vw7 );
	b3StoreBodyVelocity( states, indices[7], vw8 );
}

#elif defined( B3_SIMD_SSE2 ) || defined( B3_SIMD_NEON )

B3_FORCE_INLINE b3BodyStateW b3GatherBodies( const b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices )
{
	const float* identity = (const float*)&b3_identityBodyState;

	// Indices are 0 for null
	const float* p1 = indices[0] == 0 ? identity : (const float*)( states + indices[0] - 1 );
	const float* p2 = indices[1] == 0 ? identity : (const float*)( states + indices[1] - 1 );
	const float* p3 = indices[2] == 0 ? identity : (const float*)( states + indices[2] - 1 );
	const float* p4 = indices[3] == 0 ? identity : (const float*)( states + indices[3] - 1 );

	b3BodyStateW s;
	b3FloatW pad;
	b3TransposeW( b3LoadW( p1 ), b3LoadW( p2 ), b3LoadW( p3 ), b3LoadW( p4 ), &s.v.X, &s.v.Y, &s.v.Z, &pad );
	b3TransposeW( b3LoadW( p1 + 4 ), b3LoadW( p2 + 4 ), b3LoadW( p3 + 4 ), b3LoadW( p4 + 4 ), &s.w.X, &s.w.Y, &s.w.Z, &pad );
	b3TransposeW( b3LoadW( p1 + 8 ), b3LoadW( p2 + 8 ), b3LoadW( p3 + 8 ), b3LoadW( p4 + 8 ), &s.dp.X, &s.dp.Y, &s.dp.Z, &pad );
	b3TransposeW( b3LoadW( p1 + 12 ), b3LoadW( p2 + 12 ), b3LoadW( p3 + 12 ), b3LoadW( p4 + 12 ), &s.dq.V.X, &s.dq.V.Y,
				  &s.dq.V.Z, &s.dq.S );
	return s;
}

B3_FORCE_INLINE void b3StoreBodyVelocity( b3BodyState* B3_RESTRICT states, int index, b3FloatW v, b3FloatW w )
{
	// Indices are 0 for null
	if ( index == 0 )
	{
		return;
	}

	b3BodyState* state = states + index - 1;
	uint32_t flags = state->flags;
	if ( ( flags & b3_dynamicFlag ) == 0 )
	{
		return;
	}

	b3StoreW( (float*)state, v );
	b3StoreW( (float*)state + 4, w );
}

// This writes only the velocities back to the solver bodies
B3_FORCE_INLINE void b3ScatterBodies( b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices,
									  const b3BodyStateW* B3_RESTRICT simdBody )
{
	// I don't use any dummy body in the body array because this will lead to multithreaded sharing and the
	// associated cache flushing.
	b3FloatW zero = b3ZeroW();
	b3FloatW v1, v2, v3, v4;
	b3TransposeW( simdBody->v.X, simdBody->v.Y, simdBody->v.Z, zero, &v1, &v2, &v3, &v4 );
	b3FloatW w1, w2, w3, w4;
	b3TransposeW( simdBody->w.X, simdBody->w.Y, simdBody->w.Z, zero, &w1, &w2, &w3, &w4 );

	b3StoreBodyVelocity( states, indices[0], v1, w1 );
	b3StoreBodyVelocity( states, indices[1], v2, w2 );
	b3StoreBodyVelocity( states, indices[2], v3, w3 );
	b3StoreBodyVelocity( states, indices[3], v4, w4 );
}

#else // non-simd

B3_FORCE_INLINE b3BodyStateW b3GatherBodies( const b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices )
{
	b3BodyState identity = b3_identityBodyState;

	b3BodyState s1 = indices[0] == 0 ? identity : states[indices[0] - 1];
	b3BodyState s2 = indices[1] == 0 ? identity : states[indices[1] - 1];
	b3BodyState s3 = indices[2] == 0 ? identity : states[indices[2] - 1];
	b3BodyState s4 = indices[3] == 0 ? identity : states[indices[3] - 1];

	b3BodyStateW simdBody;
	simdBody.v.X = (b3FloatW){ s1.linearVelocity.x, s2.linearVelocity.x, s3.linearVelocity.x, s4.linearVelocity.x };
	simdBody.v.Y = (b3FloatW){ s1.linearVelocity.y, s2.linearVelocity.y, s3.linearVelocity.y, s4.linearVelocity.y };
	simdBody.v.Z = (b3FloatW){ s1.linearVelocity.z, s2.linearVelocity.z, s3.linearVelocity.z, s4.linearVelocity.z };
	simdBody.w.X = (b3FloatW){ s1.angularVelocity.x, s2.angularVelocity.x, s3.angularVelocity.x, s4.angularVelocity.x };
	simdBody.w.Y = (b3FloatW){ s1.angularVelocity.y, s2.angularVelocity.y, s3.angularVelocity.y, s4.angularVelocity.y };
	simdBody.w.Z = (b3FloatW){ s1.angularVelocity.z, s2.angularVelocity.z, s3.angularVelocity.z, s4.angularVelocity.z };
	simdBody.dp.X = (b3FloatW){ s1.deltaPosition.x, s2.deltaPosition.x, s3.deltaPosition.x, s4.deltaPosition.x };
	simdBody.dp.Y = (b3FloatW){ s1.deltaPosition.y, s2.deltaPosition.y, s3.deltaPosition.y, s4.deltaPosition.y };
	simdBody.dp.Z = (b3FloatW){ s1.deltaPosition.z, s2.deltaPosition.z, s3.deltaPosition.z, s4.deltaPosition.z };
	simdBody.dq.V.X = (b3FloatW){ s1.deltaRotation.v.x, s2.deltaRotation.v.x, s3.deltaRotation.v.x, s4.deltaRotation.v.x };
	simdBody.dq.V.Y = (b3FloatW){ s1.deltaRotation.v.y, s2.deltaRotation.v.y, s3.deltaRotation.v.y, s4.deltaRotation.v.y };
	simdBody.dq.V.Z = (b3FloatW){ s1.deltaRotation.v.z, s2.deltaRotation.v.z, s3.deltaRotation.v.z, s4.deltaRotation.v.z };
	simdBody.dq.S = (b3FloatW){ s1.deltaRotation.s, s2.deltaRotation.s, s3.deltaRotation.s, s4.deltaRotation.s };

	return simdBody;
}

// This writes only the velocities back to the solver bodies
B3_FORCE_INLINE void b3ScatterBodies( b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices,
									  const b3BodyStateW* B3_RESTRICT simdBody )
{
	int index1 = indices[0] - 1;
	if ( index1 != -1 && ( states[index1].flags & b3_dynamicFlag ) != 0 )
	{
		b3BodyState* state = states + index1;
		state->linearVelocity.x = simdBody->v.X.x;
		state->linearVelocity.y = simdBody->v.Y.x;
		state->linearVelocity.z = simdBody->v.Z.x;
		state->angularVelocity.x = simdBody->w.X.x;
		state->angularVelocity.y = simdBody->w.Y.x;
		state->angularVelocity.z = simdBody->w.Z.x;
	}

	int index2 = indices[1] - 1;
	if ( index2 != -1 && ( states[index2].flags & b3_dynamicFlag ) != 0 )
	{
		b3BodyState* state = states + index2;
		state->linearVelocity.x = simdBody->v.X.y;
		state->linearVelocity.y = simdBody->v.Y.y;
		state->linearVelocity.z = simdBody->v.Z.y;
		state->angularVelocity.x = simdBody->w.X.y;
		state->angularVelocity.y = simdBody->w.Y.y;
		state->angularVelocity.z = simdBody->w.Z.y;
	}

	int index3 = indices[2] - 1;
	if ( index3 != -1 && ( states[index3].flags & b3_dynamicFlag ) != 0 )
	{
		b3BodyState* state = states + index3;
		state->linearVelocity.x = simdBody->v.X.z;
		state->linearVelocity.y = simdBody->v.Y.z;
		state->linearVelocity.z = simdBody->v.Z.z;
		state->angularVelocity.x = simdBody->w.X.z;
		state->angularVelocity.y = simdBody->w.Y.z;
		state->angularVelocity.z = simdBody->w.Z.z;
	}

	int index4 = indices[3] - 1;
	if ( index4 != -1 && ( states[index4].flags & b3_dynamicFlag ) != 0 )
	{
		b3BodyState* state = states + index4;
		state->linearVelocity.x = simdBody->v.X.w;
		state->linearVelocity.y = simdBody->v.Y.w;
		state->linearVelocity.z = simdBody->v.Z.w;
		state->angularVelocity.x = simdBody->w.X.w;
		state->angularVelocity.y = simdBody->w.Y.w;
		state->angularVelocity.z = simdBody->w.Z.w;
	}
}
#endif

_Static_assert( offsetof( b3ManifoldPoint, anchorA ) == 0, "manifold point layout" );
_Static_assert( offsetof( b3ManifoldPoint, anchorB ) == 12, "manifold point layout" );
_Static_assert( offsetof( b3ManifoldPoint, separation ) == 24, "manifold point layout" );
_Static_assert( offsetof( b3ManifoldPoint, normalImpulse ) == 28, "manifold point layout" );
_Static_assert( offsetof( b3Manifold, twistImpulse ) == offsetof( b3Manifold, normal ) + 12, "manifold layout" );
_Static_assert( offsetof( b3Manifold, frictionImpulse ) == offsetof( b3Manifold, normal ) + 16, "manifold layout" );
_Static_assert( offsetof( b3Manifold, rollingImpulse ) == offsetof( b3Manifold, normal ) + 28, "manifold layout" );
_Static_assert( offsetof( b3Manifold, pointCount ) == offsetof( b3Manifold, normal ) + 40, "manifold layout" );
_Static_assert( offsetof( b3Matrix3, cy ) == 12 && offsetof( b3Matrix3, cz ) == 24 && sizeof( b3Matrix3 ) == 36,
				"matrix layout" );

static const b3Contact b3_zeroContact = { 0 };
static const b3Manifold b3_zeroManifold = { 0 };
static const b3BodySim b3_zeroBodySim = { 0 };

#if B3_SIMD_WIDTH == 8
#define B3_GATHER_LANES( wide, lanes, field )                                                                                    \
	wide = b3SetW( lanes[0]->field, lanes[1]->field, lanes[2]->field, lanes[3]->field, lanes[4]->field, lanes[5]->field,         \
				   lanes[6]->field, lanes[7]->field )
#else
#define B3_GATHER_LANES( wide, lanes, field ) wide = b3SetW( lanes[0]->field, lanes[1]->field, lanes[2]->field, lanes[3]->field )
#endif

static inline b3SymMatrix3W b3GatherInvInertiaW( const b3BodySim* const* simLanes )
{
	const float* i0 = &simLanes[0]->invInertiaWorld.cx.x;
	const float* i1 = &simLanes[1]->invInertiaWorld.cx.x;
	const float* i2 = &simLanes[2]->invInertiaWorld.cx.x;
	const float* i3 = &simLanes[3]->invInertiaWorld.cx.x;
#if B3_SIMD_WIDTH == 8
	const float* i4 = &simLanes[4]->invInertiaWorld.cx.x;
	const float* i5 = &simLanes[5]->invInertiaWorld.cx.x;
	const float* i6 = &simLanes[6]->invInertiaWorld.cx.x;
	const float* i7 = &simLanes[7]->invInertiaWorld.cx.x;
#endif

	b3SymMatrix3W m;
	b3FloatW unused;
#if B3_SIMD_WIDTH == 8
	b3TransposeW( b3LoadW( i0 ), b3LoadW( i1 ), b3LoadW( i2 ), b3LoadW( i3 ), b3LoadW( i4 ), b3LoadW( i5 ), b3LoadW( i6 ),
				  b3LoadW( i7 ), &m.cxx, &m.cxy, &m.cxz, &unused, &m.cyy, &m.cyz, &unused, &unused );
	m.czz = b3SetW( i0[8], i1[8], i2[8], i3[8], i4[8], i5[8], i6[8], i7[8] );
#else
	b3TransposeW( b3LoadW( i0 ), b3LoadW( i1 ), b3LoadW( i2 ), b3LoadW( i3 ), &m.cxx, &m.cxy, &m.cxz, &unused );
	b3TransposeW( b3LoadW( i0 + 4 ), b3LoadW( i1 + 4 ), b3LoadW( i2 + 4 ), b3LoadW( i3 + 4 ), &m.cyy, &m.cyz, &unused, &unused );
	m.czz = b3SetW( i0[8], i1[8], i2[8], i3[8] );
#endif
	return m;
}

// Added for Nebenan: s * m
static inline b3SymMatrix3W b3ScaleSymW( b3FloatW s, b3SymMatrix3W m )
{
	return (b3SymMatrix3W){
		b3MulW( s, m.cxx ), b3MulW( s, m.cxy ), b3MulW( s, m.cxz ), b3MulW( s, m.cyy ), b3MulW( s, m.cyz ), b3MulW( s, m.czz ),
	};
}

static inline b3SymMatrix3W b3AddSymW( b3SymMatrix3W a, b3SymMatrix3W b )
{
	return (b3SymMatrix3W){
		b3AddW( a.cxx, b.cxx ), b3AddW( a.cxy, b.cxy ), b3AddW( a.cxz, b.cxz ),
		b3AddW( a.cyy, b.cyy ), b3AddW( a.cyz, b.cyz ), b3AddW( a.czz, b.czz ),
	};
}

static inline b3SymMatrix3W b3InvertSymW( b3SymMatrix3W m )
{
	b3FloatW cxx = b3SubW( b3MulW( m.cyy, m.czz ), b3MulW( m.cyz, m.cyz ) );
	b3FloatW cxy = b3SubW( b3MulW( m.cxz, m.cyz ), b3MulW( m.cxy, m.czz ) );
	b3FloatW cxz = b3SubW( b3MulW( m.cxy, m.cyz ), b3MulW( m.cxz, m.cyy ) );
	b3FloatW cyy = b3SubW( b3MulW( m.cxx, m.czz ), b3MulW( m.cxz, m.cxz ) );
	b3FloatW cyz = b3SubW( b3MulW( m.cxy, m.cxz ), b3MulW( m.cxx, m.cyz ) );
	b3FloatW czz = b3SubW( b3MulW( m.cxx, m.cyy ), b3MulW( m.cxy, m.cxy ) );

	b3FloatW det = b3AddW( b3MulW( m.cxx, cxx ), b3AddW( b3MulW( m.cxy, cxy ), b3MulW( m.cxz, cxz ) ) );
	b3FloatW valid = b3GreaterThanW( b3AbsW( det ), b3SplatW( 1000.0f * FLT_MIN ) );
	b3FloatW invDet = b3BlendW( b3ZeroW(), b3DivW( b3SplatW( 1.0f ), det ), valid );

	return (b3SymMatrix3W){
		b3MulW( invDet, cxx ), b3MulW( invDet, cxy ), b3MulW( invDet, cxz ),
		b3MulW( invDet, cyy ), b3MulW( invDet, cyz ), b3MulW( invDet, czz ),
	};
}

static inline b3Vec3W b3PerpW( b3Vec3W a )
{
	b3FloatW zero = b3ZeroW();
	b3FloatW half = b3SplatW( 0.5f );
	b3FloatW mask = b3OrW( b3LessThanW( a.X, b3NegW( half ) ), b3GreaterThanW( a.X, half ) );

	b3Vec3W p;
	p.X = b3BlendW( zero, a.Y, mask );
	p.Y = b3BlendW( a.Z, b3NegW( a.X ), mask );
	p.Z = b3BlendW( b3NegW( a.Y ), zero, mask );

	b3FloatW lengthSquared = b3DotW( p, p );
	b3FloatW valid = b3GreaterThanW( lengthSquared, b3SplatW( 1000.0f * FLT_MIN ) );
	b3FloatW s = b3BlendW( zero, b3DivW( b3SplatW( 1.0f ), b3SqrtW( lengthSquared ) ), valid );
	return b3MulSVW( s, p );
}

// Added for Nebenan: b3PrepareContacts_Convex reads each contact from the body indices and the manifold pointer to the
// friction, the restitution and the tangent velocity at its end. This prefetches every cache line of that range. Forced
// inline, GCC 13 drops the prefetches of a helper that only reads.
B3_FORCE_INLINE void b3PrefetchPrepareContact( const b3Contact* contact )
{
	const char* start = (const char*)&contact->encodedBodySimA;
	b3Prefetch( start );
	b3Prefetch( start + 64 );
	b3Prefetch( start + 128 );
	b3Prefetch( (const char*)( contact + 1 ) - 1 );
}

// Added for Nebenan: next to the contact, b3PrepareContacts_Convex reads its manifold and the mass and the inertia of its
// awake bodies. This prefetches them, reading the contact, which must be in the cache already.
B3_FORCE_INLINE void b3PrefetchPrepareData( const b3Contact* contact, const b3BodySim* sims )
{
	const char* manifold = (const char*)contact->manifolds;
	for ( int offset = 0; offset < (int)sizeof( b3Manifold ); offset += 64 )
	{
		b3Prefetch( manifold + offset );
	}
	b3Prefetch( manifold + sizeof( b3Manifold ) - 1 );

	int indices[2] = { b3DecodeAwakeIndex( contact->encodedBodySimA ), b3DecodeAwakeIndex( contact->encodedBodySimB ) };
	for ( int k = 0; k < 2; ++k )
	{
		if ( indices[k] != B3_NULL_INDEX )
		{
			const b3BodySim* sim = sims + indices[k];
			b3Prefetch( &sim->invMass );
			b3Prefetch( &sim->invInertiaWorld );
			b3Prefetch( (const char*)( &sim->invInertiaWorld + 1 ) - 1 );
		}
	}
}

// Added for Nebenan: one wide constraint from the contacts in its lanes, for b3PrepareContacts_Convex and
// b3PreparePackContacts_Convex. A lane from laneCount on or with a null id stays empty. The scales, null or one per lane,
// multiply the inverse mass and inertia of the bodies, for the copies of a knot, see b3KnotBatch, or of a domain. The
// slots, null or one per lane, are the states the bodies solve on in a domain, see b3Domains. Returns whether a lane has
// restitution.
B3_FORCE_INLINE bool b3PrepareContactWide( b3World* world, b3BodySim* sims, b3BodyState* states, b3ContactConstraintWide* c,
										   const int* contactIds, int laneCount, b3FloatW warmStartScale, const float* scaleA,
										   const float* scaleB, const int* slotA, const int* slotB )
{
#if B3_ENABLE_VALIDATION
	b3Body* bodies = world->bodies.data;
#endif
	b3FloatW zeroW = b3ZeroW();
	b3FloatW oneW = b3SplatW( 1.0f );
	b3FloatW twoW = b3SplatW( 2.0f );
	b3FloatW invTau = b3SplatW( 1.0f / B3_SPECULATIVE_DISTANCE );
	b3FloatW minFrictionWeight = b3SplatW( B3_MIN_FRICTION_WEIGHT );
	b3FloatW minDet = b3SplatW( 1000.0f * FLT_MIN );

	const b3Contact* contactLanes[B3_SIMD_WIDTH];
	const b3Manifold* manifoldLanes[B3_SIMD_WIDTH];
	const b3BodySim* simLanesA[B3_SIMD_WIDTH];
	const b3BodySim* simLanesB[B3_SIMD_WIDTH];
	int hitEventLanes = 0;

	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		// Added for Nebenan: a null id is an empty lane at the end of a pack of islands, see b3OrderPackContacts
		if ( lane < laneCount && contactIds[lane] != B3_NULL_INDEX )
		{
			b3Contact* contact = b3Array_Get( world->contacts, contactIds[lane] );
			B3_ASSERT( contact->manifoldCount == 1 );
			b3Manifold* manifold = contact->manifolds;

			int indexA = b3DecodeAwakeIndex( contact->encodedBodySimA );
			int indexB = b3DecodeAwakeIndex( contact->encodedBodySimB );

#if B3_ENABLE_VALIDATION
			b3Body* bodyA = bodies + contact->edges[0].bodyId;
			b3Body* bodyB = bodies + contact->edges[1].bodyId;
			B3_ASSERT( contact->encodedBodySimA == b3EncodeBodySimIndex( bodyA ) );
			B3_ASSERT( contact->encodedBodySimB == b3EncodeBodySimIndex( bodyB ) );
#endif

			c->indexA[lane] = indexA + 1;
			c->indexB[lane] = indexB + 1;
			c->pointCounts[lane] = manifold->pointCount;
			c->manifolds[lane] = manifold;

			contactLanes[lane] = contact;
			manifoldLanes[lane] = manifold;
			simLanesA[lane] = indexA == B3_NULL_INDEX ? &b3_zeroBodySim : sims + indexA;
			simLanesB[lane] = indexB == B3_NULL_INDEX ? &b3_zeroBodySim : sims + indexB;
			hitEventLanes |= ( contact->flags & b3_simEnableHitEvent ) != 0 ? 1 << lane : 0;
		}
		else
		{
			c->indexA[lane] = 0;
			c->indexB[lane] = 0;
			c->pointCounts[lane] = 0;
			c->manifolds[lane] = NULL;

			contactLanes[lane] = &b3_zeroContact;
			manifoldLanes[lane] = &b3_zeroManifold;
			simLanesA[lane] = &b3_zeroBodySim;
			simLanesB[lane] = &b3_zeroBodySim;
		}
	}

	b3FloatW mA, mB;
	B3_GATHER_LANES( mA, simLanesA, invMass );
	B3_GATHER_LANES( mB, simLanesB, invMass );
	b3SymMatrix3W iA = b3GatherInvInertiaW( simLanesA );
	b3SymMatrix3W iB = b3GatherInvInertiaW( simLanesB );

	// Added for Nebenan: a copy of a knot carries its share of the mass, see b3KnotBatch
	if ( scaleA != NULL )
	{
		b3FloatW sA = b3LoadW( scaleA );
		b3FloatW sB = b3LoadW( scaleB );
		mA = b3MulW( sA, mA );
		mB = b3MulW( sB, mB );
		iA = b3ScaleSymW( sA, iA );
		iB = b3ScaleSymW( sB, iB );
	}

	c->invMassA = mA;
	c->invMassB = mB;
	c->invIA = iA;
	c->invIB = iB;

	b3Vec3W tangentVelocity;
	B3_GATHER_LANES( c->friction, contactLanes, friction );
	B3_GATHER_LANES( c->rollingResistance, contactLanes, rollingResistance );
	B3_GATHER_LANES( c->restitution, contactLanes, restitution );
	B3_GATHER_LANES( tangentVelocity.X, contactLanes, tangentVelocity.x );
	B3_GATHER_LANES( tangentVelocity.Y, contactLanes, tangentVelocity.y );
	B3_GATHER_LANES( tangentVelocity.Z, contactLanes, tangentVelocity.z );

	b3Vec3W normal, frictionImpulse, rollingImpulse;
	b3FloatW twistImpulse;
	{
		const float* m0 = &manifoldLanes[0]->normal.x;
		const float* m1 = &manifoldLanes[1]->normal.x;
		const float* m2 = &manifoldLanes[2]->normal.x;
		const float* m3 = &manifoldLanes[3]->normal.x;
#if B3_SIMD_WIDTH == 8
		const float* m4 = &manifoldLanes[4]->normal.x;
		const float* m5 = &manifoldLanes[5]->normal.x;
		const float* m6 = &manifoldLanes[6]->normal.x;
		const float* m7 = &manifoldLanes[7]->normal.x;
		b3TransposeW( b3LoadW( m0 ), b3LoadW( m1 ), b3LoadW( m2 ), b3LoadW( m3 ), b3LoadW( m4 ), b3LoadW( m5 ), b3LoadW( m6 ),
					  b3LoadW( m7 ), &normal.X, &normal.Y, &normal.Z, &twistImpulse, &frictionImpulse.X, &frictionImpulse.Y,
					  &frictionImpulse.Z, &rollingImpulse.X );
		rollingImpulse.Y = b3SetW( m0[8], m1[8], m2[8], m3[8], m4[8], m5[8], m6[8], m7[8] );
		rollingImpulse.Z = b3SetW( m0[9], m1[9], m2[9], m3[9], m4[9], m5[9], m6[9], m7[9] );
#else
		b3TransposeW( b3LoadW( m0 ), b3LoadW( m1 ), b3LoadW( m2 ), b3LoadW( m3 ), &normal.X, &normal.Y, &normal.Z,
					  &twistImpulse );
		b3TransposeW( b3LoadW( m0 + 4 ), b3LoadW( m1 + 4 ), b3LoadW( m2 + 4 ), b3LoadW( m3 + 4 ), &frictionImpulse.X,
					  &frictionImpulse.Y, &frictionImpulse.Z, &rollingImpulse.X );
		rollingImpulse.Y = b3SetW( m0[8], m1[8], m2[8], m3[8] );
		rollingImpulse.Z = b3SetW( m0[9], m1[9], m2[9], m3[9] );
#endif
	}

	b3Vec3W tangent1 = b3PerpW( normal );
	b3Vec3W tangent2 = b3CrossW( tangent1, normal );
	c->normal = normal;
	c->tangent1 = tangent1;
	c->tangent2 = tangent2;
	c->tangentVelocity1 = b3DotW( tangentVelocity, tangent1 );
	c->tangentVelocity2 = b3DotW( tangentVelocity, tangent2 );

	b3FloatW rollingMask = b3GreaterThanW( c->rollingResistance, zeroW );
	c->twistImpulse = b3MulW( warmStartScale, twistImpulse );
	c->rollingImpulse.X = b3BlendW( zeroW, b3MulW( warmStartScale, rollingImpulse.X ), rollingMask );
	c->rollingImpulse.Y = b3BlendW( zeroW, b3MulW( warmStartScale, rollingImpulse.Y ), rollingMask );
	c->rollingImpulse.Z = b3BlendW( zeroW, b3MulW( warmStartScale, rollingImpulse.Z ), rollingMask );
	c->frictionImpulse.x = b3MulW( warmStartScale, b3DotW( frictionImpulse, tangent1 ) );
	c->frictionImpulse.y = b3MulW( warmStartScale, b3DotW( frictionImpulse, tangent2 ) );

#if B3_SIMD_WIDTH == 8
	b3FloatW pointCountW =
		b3SetW( (float)c->pointCounts[0], (float)c->pointCounts[1], (float)c->pointCounts[2], (float)c->pointCounts[3],
				(float)c->pointCounts[4], (float)c->pointCounts[5], (float)c->pointCounts[6], (float)c->pointCounts[7] );
#else
	b3FloatW pointCountW =
		b3SetW( (float)c->pointCounts[0], (float)c->pointCounts[1], (float)c->pointCounts[2], (float)c->pointCounts[3] );
#endif

	b3Vec3W centerA = { zeroW, zeroW, zeroW };
	b3Vec3W centerB = { zeroW, zeroW, zeroW };
	b3FloatW totalFrictionWeight = zeroW;

	for ( int pointIndex = 0; pointIndex < B3_MAX_MANIFOLD_POINTS; ++pointIndex )
	{
		b3ContactConstraintPointWide* cp = c->points + pointIndex;
		b3FloatW pointMask = b3GreaterThanW( pointCountW, b3SplatW( (float)pointIndex ) );

		const float* p0 = (const float*)( manifoldLanes[0]->points + pointIndex );
		const float* p1 = (const float*)( manifoldLanes[1]->points + pointIndex );
		const float* p2 = (const float*)( manifoldLanes[2]->points + pointIndex );
		const float* p3 = (const float*)( manifoldLanes[3]->points + pointIndex );
#if B3_SIMD_WIDTH == 8
		const float* p4 = (const float*)( manifoldLanes[4]->points + pointIndex );
		const float* p5 = (const float*)( manifoldLanes[5]->points + pointIndex );
		const float* p6 = (const float*)( manifoldLanes[6]->points + pointIndex );
		const float* p7 = (const float*)( manifoldLanes[7]->points + pointIndex );
#endif

		b3Vec3W rA, rB;
		b3FloatW separation, normalImpulse;
#if B3_SIMD_WIDTH == 8
		b3TransposeW( b3LoadW( p0 ), b3LoadW( p1 ), b3LoadW( p2 ), b3LoadW( p3 ), b3LoadW( p4 ), b3LoadW( p5 ), b3LoadW( p6 ),
					  b3LoadW( p7 ), &rA.X, &rA.Y, &rA.Z, &rB.X, &rB.Y, &rB.Z, &separation, &normalImpulse );
#else
		b3TransposeW( b3LoadW( p0 ), b3LoadW( p1 ), b3LoadW( p2 ), b3LoadW( p3 ), &rA.X, &rA.Y, &rA.Z, &rB.X );
		b3TransposeW( b3LoadW( p0 + 4 ), b3LoadW( p1 + 4 ), b3LoadW( p2 + 4 ), b3LoadW( p3 + 4 ), &rB.Y, &rB.Z, &separation,
					  &normalImpulse );
#endif

		rA.X = b3BlendW( zeroW, rA.X, pointMask );
		rA.Y = b3BlendW( zeroW, rA.Y, pointMask );
		rA.Z = b3BlendW( zeroW, rA.Z, pointMask );
		rB.X = b3BlendW( zeroW, rB.X, pointMask );
		rB.Y = b3BlendW( zeroW, rB.Y, pointMask );
		rB.Z = b3BlendW( zeroW, rB.Z, pointMask );
		separation = b3BlendW( zeroW, separation, pointMask );
		normalImpulse = b3BlendW( zeroW, normalImpulse, pointMask );

		// C0 friction center decay. Needed to prevent spinning top drift (GyroscopicPrecession sample).
		// See details in b3PrepareContacts_Mesh. This code should stay in sync.
		b3FloatW weight = b3MinW( b3MaxW( b3SubW( twoW, b3MulW( separation, invTau ) ), minFrictionWeight ), oneW );
		weight = b3BlendW( zeroW, weight, pointMask );
		centerA = b3MulAddSVW( centerA, weight, rA );
		centerB = b3MulAddSVW( centerB, weight, rB );
		totalFrictionWeight = b3AddW( totalFrictionWeight, weight );

		cp->anchorAs = rA;
		cp->anchorBs = rB;
		cp->baseSeparations = b3SubW( separation, b3DotW( b3SubVW( rB, rA ), normal ) );
		cp->normalImpulses = b3MulW( warmStartScale, normalImpulse );
		cp->totalNormalImpulses = zeroW;
		cp->relativeVelocities = zeroW;
		cp->restitutionImpulses = zeroW;

		b3Vec3W rnA = b3CrossW( rA, normal );
		b3Vec3W rnB = b3CrossW( rB, normal );
		b3FloatW kNormal = b3AddW( mA, mB );
		kNormal = b3AddW( kNormal, b3DotW( rnA, b3MulMVW( iA, rnA ) ) );
		kNormal = b3AddW( kNormal, b3DotW( rnB, b3MulMVW( iB, rnB ) ) );
		b3FloatW valid = b3AndW( b3GreaterThanW( kNormal, zeroW ), pointMask );
		cp->normalMasses = b3BlendW( zeroW, b3DivW( oneW, kNormal ), valid );
	}

	b3FloatW invWeight = b3BlendW( zeroW, b3DivW( oneW, totalFrictionWeight ), b3GreaterThanW( totalFrictionWeight, zeroW ) );
	centerA = b3MulSVW( invWeight, centerA );
	centerB = b3MulSVW( invWeight, centerB );
	c->centerA = centerA;
	c->centerB = centerB;

	for ( int pointIndex = 0; pointIndex < B3_MAX_MANIFOLD_POINTS; ++pointIndex )
	{
		b3ContactConstraintPointWide* cp = c->points + pointIndex;
		b3FloatW pointMask = b3GreaterThanW( pointCountW, b3SplatW( (float)pointIndex ) );
		b3Vec3W d = b3SubVW( cp->anchorAs, centerA );
		cp->leverArms = b3BlendW( zeroW, b3SqrtW( b3DotW( d, d ) ), pointMask );
	}

	{
		b3Vec3W rtA1 = b3CrossW( centerA, tangent1 );
		b3Vec3W rtA2 = b3CrossW( centerA, tangent2 );
		b3Vec3W rtB1 = b3CrossW( centerB, tangent1 );
		b3Vec3W rtB2 = b3CrossW( centerB, tangent2 );
		b3Vec3W iArtA1 = b3MulMVW( iA, rtA1 );
		b3Vec3W iArtA2 = b3MulMVW( iA, rtA2 );
		b3Vec3W iBrtB1 = b3MulMVW( iB, rtB1 );
		b3Vec3W iBrtB2 = b3MulMVW( iB, rtB2 );

		b3FloatW kxx = b3AddW( b3AddW( b3AddW( mA, mB ), b3DotW( rtA1, iArtA1 ) ), b3DotW( rtB1, iBrtB1 ) );
		b3FloatW kyy = b3AddW( b3AddW( b3AddW( mA, mB ), b3DotW( rtA2, iArtA2 ) ), b3DotW( rtB2, iBrtB2 ) );
		b3FloatW kxy = b3AddW( b3DotW( rtA1, iArtA2 ), b3DotW( rtB1, iBrtB2 ) );

		b3FloatW det = b3SubW( b3MulW( kxx, kyy ), b3MulW( kxy, kxy ) );
		b3FloatW valid = b3GreaterThanW( b3AbsW( det ), minDet );
		b3FloatW invDet = b3BlendW( zeroW, b3DivW( oneW, det ), valid );
		c->tangentMass.cxx = b3MulW( invDet, kyy );
		c->tangentMass.cxy = b3NegW( b3MulW( invDet, kxy ) );
		c->tangentMass.cyy = b3MulW( invDet, kxx );
	}

	b3SymMatrix3W invIAB = b3AddSymW( iA, iB );

	{
		b3FloatW kTwist = b3DotW( normal, b3MulMVW( invIAB, normal ) );
		c->twistMass = b3BlendW( zeroW, b3DivW( oneW, kTwist ), b3GreaterThanW( kTwist, zeroW ) );
	}

	if ( b3AllZeroW( c->rollingResistance ) == false )
	{
		c->rollingMass = b3InvertSymW( invIAB );
	}
	else
	{
		c->rollingMass = ( b3SymMatrix3W ){ zeroW, zeroW, zeroW, zeroW, zeroW, zeroW };
	}

	// Only sample contact point normal velocity if needed.
	bool haveRestitution = b3AnyTrueW( b3GreaterThanW( c->restitution, zeroW ) );

	if ( hitEventLanes != 0 || haveRestitution )
	{
		b3BodyStateW bA = b3GatherBodies( states, c->indexA );
		b3BodyStateW bB = b3GatherBodies( states, c->indexB );

		for ( int pointIndex = 0; pointIndex < B3_MAX_MANIFOLD_POINTS; ++pointIndex )
		{
			b3ContactConstraintPointWide* cp = c->points + pointIndex;

			b3Vec3W vrA = b3AddVW( bA.v, b3CrossW( bA.w, cp->anchorAs ) );
			b3Vec3W vrB = b3AddVW( bB.v, b3CrossW( bB.w, cp->anchorBs ) );
			b3FloatW vn = b3DotW( normal, b3SubVW( vrB, vrA ) );
			cp->relativeVelocities = vn;

			if ( hitEventLanes != 0 )
			{
				float normalVelocities[B3_SIMD_WIDTH];
				b3StoreW( normalVelocities, vn );

				for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
				{
					if ( ( hitEventLanes & ( 1 << lane ) ) != 0 && pointIndex < c->pointCounts[lane] )
					{
						c->manifolds[lane]->points[pointIndex].normalVelocity = normalVelocities[lane];
					}
				}
			}
		}
	}

	// Added for Nebenan: the bodies solve on their copies in a domain, the velocities above came from the bodies
	if ( slotA != NULL )
	{
		for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
		{
			if ( c->manifolds[lane] != NULL )
			{
				c->indexA[lane] = slotA[lane];
				c->indexB[lane] = slotB[lane];
			}
		}
	}

	return haveRestitution;
}

// Added for Nebenan: for the lanes of a wide constraint of a domain the states their bodies solve on plus one and the
// scales of their mass: a body with contacts in several domains solves on its copy in this one, which carries its share of
// the mass, see b3Domains in src/solver.c
B3_FORCE_INLINE void b3GetDomainLanes( const b3StepContext* context, const b3Contact* contacts, int domain, const int* contactIds,
									   int laneCount, int* slotA, int* slotB, float* scaleA, float* scaleB )
{
	const uint8_t* masks = context->domainBodyMasks;
	const int* copies = context->domainBodyCopies;
	int copyBase = context->domainAwakeBodyCount + 1;
	uint32_t before = ( 1u << domain ) - 1u;
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		slotA[lane] = 0;
		slotB[lane] = 0;
		scaleA[lane] = 1.0f;
		scaleB[lane] = 1.0f;
		if ( lane >= laneCount || contactIds[lane] == B3_NULL_INDEX )
		{
			continue;
		}

		const b3Contact* contact = contacts + contactIds[lane];
		int bodies[2] = { b3DecodeAwakeIndex( contact->encodedBodySimA ), b3DecodeAwakeIndex( contact->encodedBodySimB ) };
		int slots[2] = { 0, 0 };
		float scales[2] = { 1.0f, 1.0f };
		for ( int e = 0; e < 2; ++e )
		{
			int body = bodies[e];
			if ( body == B3_NULL_INDEX )
			{
				continue;
			}

			uint32_t mask = masks[body];
			if ( ( mask & ( mask - 1 ) ) == 0 )
			{
				slots[e] = body + 1;
				continue;
			}

			slots[e] = copyBase + copies[body] + b3PopCount64( mask & before );
			scales[e] = (float)b3PopCount64( mask );
		}

		slotA[lane] = slots[0];
		slotB[lane] = slots[1];
		scaleA[lane] = scales[0];
		scaleB[lane] = scales[1];
	}
}

void B3_WIDE( b3PrepareContacts_Convex )( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneNC( prepare_contact, "Prepare Contact", b3_colorYellow, true );
	b3World* world = context->world;
	b3BodySim* sims = context->sims;
	b3BodyState* states = context->states;
	b3WidePrepareSpan* spans = context->widePrepareSpans;
	b3ContactConstraintWide* wideBase = context->wideConstraints;

	b3FloatW warmStartScale = world->enableWarmStarting ? b3SplatW( 1.0f ) : b3ZeroW();
	bool anyRestitution = false;

	int wideIndex = block.startIndex;
	int endWideIndex = block.startIndex + block.count;

	// Find color for start index. Linear search but fast.
	int colorIndex = 0;
	while ( spans[colorIndex + 1].start <= wideIndex )
	{
		colorIndex += 1;
	}

	// Loop over block
	while ( wideIndex < endWideIndex )
	{
		int colorWideStart = spans[colorIndex].start;
		int colorWideEndIndex = b3MinInt( spans[colorIndex + 1].start, endWideIndex );
		int colorContactCount = spans[colorIndex].count;
		int* contactIds = spans[colorIndex].contacts;
		const float* scaleA = spans[colorIndex].scaleA;
		const float* scaleB = spans[colorIndex].scaleB;
		const int* slotA = spans[colorIndex].slotA;
		const int* slotB = spans[colorIndex].slotB;
		int domain = spans[colorIndex].domain;

		// Loop over color
		for ( ; wideIndex < colorWideEndIndex; ++wideIndex )
		{
			b3ContactConstraintWide* c = wideBase + wideIndex;
			int localWideIndex = wideIndex - colorWideStart;

			// Added for Nebenan: the contacts come from scattered memory, and so do their manifolds and bodies, and the
			// loop waited for them most. Two wide constraints ahead it prefetches the contacts of this color, and one
			// ahead, with those contacts in the cache, their manifolds and bodies. Each constraint is prepared as before.
			for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
			{
				int farIndex = B3_SIMD_WIDTH * ( localWideIndex + 2 ) + lane;
				if ( farIndex < colorContactCount && contactIds[farIndex] != B3_NULL_INDEX )
				{
					b3PrefetchPrepareContact( world->contacts.data + contactIds[farIndex] );
				}

				int nearIndex = B3_SIMD_WIDTH * ( localWideIndex + 1 ) + lane;
				if ( nearIndex < colorContactCount && contactIds[nearIndex] != B3_NULL_INDEX )
				{
					b3PrefetchPrepareData( world->contacts.data + contactIds[nearIndex], sims );
				}
			}

			int laneCount = b3MinInt( B3_SIMD_WIDTH, colorContactCount - B3_SIMD_WIDTH * localWideIndex );
			int laneStart = B3_SIMD_WIDTH * localWideIndex;
			bool haveRestitution;
			if ( domain >= 0 )
			{
				// Added for Nebenan: in a domain the bodies may solve on their copies, see b3GetDomainLanes
				int laneSlotA[B3_SIMD_WIDTH];
				int laneSlotB[B3_SIMD_WIDTH];
				float laneScaleA[B3_SIMD_WIDTH];
				float laneScaleB[B3_SIMD_WIDTH];
				b3GetDomainLanes( context, world->contacts.data, domain, contactIds + laneStart, laneCount, laneSlotA, laneSlotB,
								  laneScaleA, laneScaleB );
				haveRestitution = b3PrepareContactWide( world, sims, states, c, contactIds + laneStart, laneCount, warmStartScale,
														laneScaleA, laneScaleB, laneSlotA, laneSlotB );
			}
			else
			{
				haveRestitution = b3PrepareContactWide(
					world, sims, states, c, contactIds + laneStart, laneCount, warmStartScale,
					scaleA != NULL ? scaleA + laneStart : NULL, scaleB != NULL ? scaleB + laneStart : NULL,
					slotA != NULL ? slotA + laneStart : NULL, slotB != NULL ? slotB + laneStart : NULL );
			}
			anyRestitution = anyRestitution || haveRestitution;
		}

		// Advance to next color
		colorIndex += 1;
	}

	if ( anyRestitution )
	{
		b3AtomicStoreInt( &context->anyRestitution, 1 );
	}

	b3TracyCZoneEnd( prepare_contact );
}

// Added for Nebenan: moves a position in the wide constraints of a pack one ahead, see b3PreparePackContacts_Convex
static void b3AdvancePackPosition( int* segment, int* index, const int* segmentCounts, int segmentCount )
{
	if ( *segment < segmentCount )
	{
		*index += 1;
		if ( *index == segmentCounts[*segment] )
		{
			*segment += 1;
			*index = 0;
		}
	}
}

// Added for Nebenan: the convex contact constraints of one pack of islands, color by color, see b3SolvePack. A pack has
// only a few wide constraints in most colors, so the prefetches of b3PrepareContacts_Convex run ahead across the colors
// here: two wide constraints ahead for the contacts and one ahead for their manifolds and bodies. Each constraint is
// prepared as there.
void B3_WIDE( b3PreparePackContacts_Convex )( b3StepContext* context, int pack )
{
	b3TracyCZoneNC( prepare_contact, "Prepare Contact", b3_colorYellow, true );
	b3World* world = context->world;
	b3Contact* contacts = world->contacts.data;
	b3BodySim* sims = context->sims;
	b3BodyState* states = context->states;
	const b3WidePrepareSpan* spans = context->widePrepareSpans;
	b3ContactConstraintWide* wideBase = context->wideConstraints;

	b3FloatW warmStartScale = world->enableWarmStarting ? b3SplatW( 1.0f ) : b3ZeroW();
	bool anyRestitution = false;

	// The wide constraints of the pack in each color that has some: the ids of their lanes, the index of the first
	// across all colors and how many
	const int* segmentIds[B3_GRAPH_COLOR_COUNT];
	int segmentStarts[B3_GRAPH_COLOR_COUNT];
	int segmentCounts[B3_GRAPH_COLOR_COUNT];
	int segmentCount = 0;
	for ( int i = 0; i < context->activeColorCount; ++i )
	{
		const int* starts = context->packWideStarts + i * ( context->packCount + 1 );
		if ( starts[pack + 1] > starts[pack] )
		{
			segmentIds[segmentCount] = spans[i].contacts + B3_SIMD_WIDTH * starts[pack];
			segmentStarts[segmentCount] = spans[i].start + starts[pack];
			segmentCounts[segmentCount] = starts[pack + 1] - starts[pack];
			segmentCount += 1;
		}
	}

	if ( segmentCount == 0 )
	{
		b3TracyCZoneEnd( prepare_contact );
		return;
	}

	// The positions one and two wide constraints ahead. The first two have nothing before them to fetch their contacts.
	int nearSegment = 0, nearIndex = 0;
	b3AdvancePackPosition( &nearSegment, &nearIndex, segmentCounts, segmentCount );
	int farSegment = nearSegment, farIndex = nearIndex;
	b3AdvancePackPosition( &farSegment, &farIndex, segmentCounts, segmentCount );

	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		int firstId = segmentIds[0][lane];
		if ( firstId != B3_NULL_INDEX )
		{
			b3PrefetchPrepareContact( contacts + firstId );
		}

		int secondId = nearSegment < segmentCount ? segmentIds[nearSegment][B3_SIMD_WIDTH * nearIndex + lane] : B3_NULL_INDEX;
		if ( secondId != B3_NULL_INDEX )
		{
			b3PrefetchPrepareContact( contacts + secondId );
		}
	}

	for ( int segment = 0; segment < segmentCount; ++segment )
	{
		for ( int index = 0; index < segmentCounts[segment]; ++index )
		{
			if ( farSegment < segmentCount )
			{
				const int* farIds = segmentIds[farSegment] + B3_SIMD_WIDTH * farIndex;
				for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
				{
					if ( farIds[lane] != B3_NULL_INDEX )
					{
						b3PrefetchPrepareContact( contacts + farIds[lane] );
					}
				}

				b3AdvancePackPosition( &farSegment, &farIndex, segmentCounts, segmentCount );
			}

			if ( nearSegment < segmentCount )
			{
				const int* nearIds = segmentIds[nearSegment] + B3_SIMD_WIDTH * nearIndex;
				for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
				{
					if ( nearIds[lane] != B3_NULL_INDEX )
					{
						b3PrefetchPrepareData( contacts + nearIds[lane], sims );
					}
				}

				b3AdvancePackPosition( &nearSegment, &nearIndex, segmentCounts, segmentCount );
			}

			b3ContactConstraintWide* c = wideBase + segmentStarts[segment] + index;
			bool haveRestitution = b3PrepareContactWide( world, sims, states, c, segmentIds[segment] + B3_SIMD_WIDTH * index,
														 B3_SIMD_WIDTH, warmStartScale, NULL, NULL, NULL, NULL );
			anyRestitution = anyRestitution || haveRestitution;
		}
	}

	if ( anyRestitution )
	{
		b3AtomicStoreInt( &context->anyRestitution, 1 );
	}

	b3TracyCZoneEnd( prepare_contact );
}

// Added for Nebenan: the warm start of one wide constraint on the states of its bodies, for b3WarmStartContacts_Convex and
// b3WarmStartKnots
B3_FORCE_INLINE void b3WarmStartContactW( b3ContactConstraintWide* c, b3BodyStateW* bA, b3BodyStateW* bB )
{
	int pointCount = 0;
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		pointCount = b3MaxInt( pointCount, c->pointCounts[lane] );
	}
	B3_VALIDATE( 0 < pointCount && pointCount <= B3_MAX_MANIFOLD_POINTS );

	b3FloatW zeroW = b3ZeroW();
	b3FloatW totalNormalImpulse = zeroW;
	b3Vec3W momentA = { zeroW, zeroW, zeroW };
	b3Vec3W momentB = { zeroW, zeroW, zeroW };

	for ( int j = 0; j < pointCount; ++j )
	{
		b3ContactConstraintPointWide* cp = c->points + j;
		b3FloatW normalImpulse = cp->normalImpulses;
		totalNormalImpulse = b3AddW( totalNormalImpulse, normalImpulse );
		momentA = b3MulAddSVW( momentA, normalImpulse, cp->anchorAs );
		momentB = b3MulAddSVW( momentB, normalImpulse, cp->anchorBs );
	}

	b3Vec3W normal = c->normal;
	b3Vec3W frictionImpulse = b3MulSVW( c->frictionImpulse.x, c->tangent1 );
	frictionImpulse = b3MulAddSVW( frictionImpulse, c->frictionImpulse.y, c->tangent2 );

	b3Vec3W linearImpulse = b3MulAddSVW( frictionImpulse, totalNormalImpulse, normal );

	b3Vec3W twistImpulse = b3MulSVW( c->twistImpulse, normal );
	b3Vec3W angularImpulseA =
		b3AddVW( b3AddVW( b3CrossW( momentA, normal ), b3CrossW( c->centerA, frictionImpulse ) ), twistImpulse );
	b3Vec3W angularImpulseB =
		b3AddVW( b3AddVW( b3CrossW( momentB, normal ), b3CrossW( c->centerB, frictionImpulse ) ), twistImpulse );

	if ( b3AllZeroW( c->rollingResistance ) == false )
	{
		angularImpulseA = b3AddVW( angularImpulseA, c->rollingImpulse );
		angularImpulseB = b3AddVW( angularImpulseB, c->rollingImpulse );
	}

	bA->w = b3MulSubMVW( bA->w, c->invIA, angularImpulseA );
	bA->v = b3MulSubSVW( bA->v, c->invMassA, linearImpulse );
	bB->w = b3MulAddMVW( bB->w, c->invIB, angularImpulseB );
	bB->v = b3MulAddSVW( bB->v, c->invMassB, linearImpulse );
}

void B3_WIDE( b3WarmStartContacts_Convex )( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneDetailNC( warm_start_contact, "Warm Start", b3_colorGreen, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->graph->colors[block.colorIndex].wideConstraints;

	for ( int i = block.startIndex; i < block.startIndex + block.count; ++i )
	{
		b3ContactConstraintWide* c = constraints + i;
		b3BodyStateW bA = b3GatherBodies( states, c->indexA );
		b3BodyStateW bB = b3GatherBodies( states, c->indexB );
		b3WarmStartContactW( c, &bA, &bB );
		b3ScatterBodies( states, c->indexA, &bA );
		b3ScatterBodies( states, c->indexB, &bB );
	}

	b3TracyCZoneDetailEnd( warm_start_contact );
}

// Added for Nebenan: the constants of b3PushContacts_Convex, for b3PushContactW
typedef struct b3PushConstantsW
{
	b3FloatW inv_h;
	b3FloatW contactSpeed;
	b3FloatW oneW;
	b3FloatW dynamicBiasRate;
	b3FloatW dynamicMassScale;
	b3FloatW dynamicImpulseScale;
	b3FloatW staticBiasRate;
	b3FloatW staticMassScale;
	b3FloatW staticImpulseScale;
} b3PushConstantsW;

B3_FORCE_INLINE b3PushConstantsW b3GetPushConstantsW( const b3StepContext* context )
{
	b3PushConstantsW k;
	k.inv_h = b3SplatW( context->inv_h );
	k.contactSpeed = b3SplatW( -context->world->contactSpeed );
	k.oneW = b3SplatW( 1.0f );

	// Stiffer for static contacts to avoid bodies getting pushed through the ground. Selected per
	// lane from the null body index instead of stored per constraint.
	k.dynamicBiasRate = b3SplatW( context->contactSoftness.massScale * context->contactSoftness.biasRate );
	k.dynamicMassScale = b3SplatW( context->contactSoftness.massScale );
	k.dynamicImpulseScale = b3SplatW( context->contactSoftness.impulseScale );
	k.staticBiasRate = b3SplatW( context->staticSoftness.massScale * context->staticSoftness.biasRate );
	k.staticMassScale = b3SplatW( context->staticSoftness.massScale );
	k.staticImpulseScale = b3SplatW( context->staticSoftness.impulseScale );
	return k;
}

// Added for Nebenan: the push of one wide constraint on the states of its bodies, for b3PushContacts_Convex and
// b3PushKnots
B3_FORCE_INLINE void b3PushContactW( b3ContactConstraintWide* c, b3BodyStateW* bA, b3BodyStateW* bB, const b3PushConstantsW* k )
{
	int pointCount = 0;
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		pointCount = b3MaxInt( pointCount, c->pointCounts[lane] );
	}
	B3_VALIDATE( 0 < pointCount && pointCount <= B3_MAX_MANIFOLD_POINTS );

	b3FloatW softMask = b3SoftMaskW( c->indexA, c->indexB );
	b3FloatW biasRate = b3BlendW( k->dynamicBiasRate, k->staticBiasRate, softMask );
	b3FloatW massScale = b3BlendW( k->dynamicMassScale, k->staticMassScale, softMask );
	b3FloatW impulseScale = b3BlendW( k->dynamicImpulseScale, k->staticImpulseScale, softMask );

	b3Vec3W dp = b3SubVW( bB->dp, bA->dp );

	// Convert to normals to local space to reduce transform math.
	b3FloatW normalSeparation = b3DotW( c->normal, dp );
	b3Vec3W normalA = b3InvRotateVectorW( bA->dq, c->normal );
	b3Vec3W normalB = b3InvRotateVectorW( bB->dq, c->normal );

	// Added for Nebenan, from upstream Box3D (#189): the normal impulses move the linear velocities only along the
	// normal. Track the relative normal velocity and apply the linear impulse once after the points.
	b3FloatW invMass = b3AddW( c->invMassA, c->invMassB );
	b3FloatW linearNormalVelocity = b3DotW( c->normal, b3SubVW( bB->v, bA->v ) );
	b3FloatW linearImpulse = b3ZeroW();

	for ( int pointIndex = 0; pointIndex < pointCount; ++pointIndex )
	{
		b3ContactConstraintPointWide* cp = c->points + pointIndex;

		// Fixed anchor points for applying impulses
		b3Vec3W rA = cp->anchorAs;
		b3Vec3W rB = cp->anchorBs;

		b3FloatW s =
			b3AddW( b3AddW( normalSeparation, b3SubW( b3DotW( normalB, rB ), b3DotW( normalA, rA ) ) ), cp->baseSeparations );

		// Apply speculative bias if separation is greater than zero, otherwise apply soft constraint bias
		b3FloatW separated = b3GreaterThanW( s, b3ZeroW() );

		// Speculative bias - positive
		b3FloatW specBias = b3MulW( s, k->inv_h );

		// Overlap bias - negative
		b3FloatW overlapBias = b3MaxW( b3MulW( biasRate, s ), k->contactSpeed );
		b3FloatW velocityBias = b3BlendW( overlapBias, specBias, separated );

		b3FloatW pointMassScale = b3BlendW( massScale, k->oneW, separated );
		b3FloatW pointImpulseScale = b3BlendW( impulseScale, b3ZeroW(), separated );

		// Angular directions of the normal impulse
		b3Vec3W uA = b3CrossW( rA, c->normal );
		b3Vec3W uB = b3CrossW( rB, c->normal );
		b3Vec3W gA = b3MulMVW( c->invIA, uA );
		b3Vec3W gB = b3MulMVW( c->invIB, uB );

		// Relative normal velocity at contact
		b3FloatW vn = b3AddW( linearNormalVelocity, b3SubW( b3DotW( uB, bB->w ), b3DotW( uA, bA->w ) ) );

		// Compute normal impulse
		b3FloatW negImpulse = b3AddW( b3MulW( cp->normalMasses, b3AddW( b3MulW( pointMassScale, vn ), velocityBias ) ),
									  b3MulW( pointImpulseScale, cp->normalImpulses ) );

		// Clamp the accumulated impulse. Not as upstream #189 with b3MaxW( -negImpulse, -normalImpulses ), where two
		// zeros of opposite sign can meet, and SSE, NEON and the scalar path return different ones.
		b3FloatW newImpulse = b3MaxW( b3SubW( cp->normalImpulses, negImpulse ), b3ZeroW() );
		b3FloatW deltaImpulse = b3SubW( newImpulse, cp->normalImpulses );
		cp->normalImpulses = newImpulse;

		// Apply contact impulse
		linearNormalVelocity = b3MulAddW( linearNormalVelocity, invMass, deltaImpulse );
		linearImpulse = b3AddW( linearImpulse, deltaImpulse );
		bA->w = b3MulSubSVW( bA->w, deltaImpulse, gA );
		bB->w = b3MulAddSVW( bB->w, deltaImpulse, gB );
	}

	b3Vec3W P = b3MulSVW( linearImpulse, c->normal );
	bA->v = b3MulSubSVW( bA->v, c->invMassA, P );
	bB->v = b3MulAddSVW( bB->v, c->invMassB, P );
}

// Solve the non-penetration constraints with the soft bias. No friction and no restitution.
void B3_WIDE( b3PushContacts_Convex )( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneDetailNC( push_contact, "Push Contact", b3_colorAliceBlue, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->graph->colors[block.colorIndex].wideConstraints;
	b3PushConstantsW k = b3GetPushConstantsW( context );

	for ( int wideIndex = block.startIndex; wideIndex < block.startIndex + block.count; ++wideIndex )
	{
		b3ContactConstraintWide* c = constraints + wideIndex;
		b3BodyStateW bA = b3GatherBodies( states, c->indexA );
		b3BodyStateW bB = b3GatherBodies( states, c->indexB );
		b3PushContactW( c, &bA, &bB, &k );
		b3ScatterBodies( states, c->indexA, &bA );
		b3ScatterBodies( states, c->indexB, &bB );
	}

	b3TracyCZoneDetailEnd( push_contact );
}

// Added for Nebenan: the constants of b3SolveContacts_Convex, for b3SolveContactW
typedef struct b3RelaxConstantsW
{
	b3FloatW inv_h;
	b3FloatW oneW;
	b3FloatW epsilonW;
} b3RelaxConstantsW;

B3_FORCE_INLINE b3RelaxConstantsW b3GetRelaxConstantsW( const b3StepContext* context )
{
	b3RelaxConstantsW k;
	k.inv_h = b3SplatW( context->inv_h );
	k.oneW = b3SplatW( 1.0f );
	k.epsilonW = b3SplatW( FLT_EPSILON );
	return k;
}

// Added for Nebenan: the normal constraint, friction and rolling resistance of one wide constraint on the states of its
// bodies, for b3SolveContacts_Convex and b3SolveKnots
B3_FORCE_INLINE void b3SolveContactW( b3ContactConstraintWide* c, b3BodyStateW* bA, b3BodyStateW* bB, const b3RelaxConstantsW* k )
{
	int pointCount = 0;
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		pointCount = b3MaxInt( pointCount, c->pointCounts[lane] );
	}
	B3_VALIDATE( 0 < pointCount && pointCount <= B3_MAX_MANIFOLD_POINTS );

	b3Vec3W dp = b3SubVW( bB->dp, bA->dp );
	b3FloatW normalSeparation = b3DotW( c->normal, dp );
	b3Vec3W normalA = b3InvRotateVectorW( bA->dq, c->normal );
	b3Vec3W normalB = b3InvRotateVectorW( bB->dq, c->normal );

	// Added for Nebenan, from upstream Box3D (#189): the normal impulses move the linear velocities only along the
	// normal. Track the relative normal velocity and apply the linear impulse with the friction.
	b3FloatW invMass = b3AddW( c->invMassA, c->invMassB );
	b3FloatW linearNormalVelocity = b3DotW( c->normal, b3SubVW( bB->v, bA->v ) );
	b3FloatW linearImpulse = b3ZeroW();

	b3FloatW totalNormalImpulse = b3ZeroW();
	b3FloatW totalTwistLimit = b3ZeroW();

	for ( int pointIndex = 0; pointIndex < pointCount; ++pointIndex )
	{
		b3ContactConstraintPointWide* cp = c->points + pointIndex;

		// Fixed anchor points for applying impulses
		b3Vec3W rA = cp->anchorAs;
		b3Vec3W rB = cp->anchorBs;

		b3FloatW s =
			b3AddW( b3AddW( normalSeparation, b3SubW( b3DotW( normalB, rB ), b3DotW( normalA, rA ) ) ), cp->baseSeparations );

		// Speculative bias, positive if separated and zero if overlapped
		b3FloatW velocityBias = b3MaxW( b3ZeroW(), b3MulW( s, k->inv_h ) );

		// Angular directions of the normal impulse
		b3Vec3W uA = b3CrossW( rA, c->normal );
		b3Vec3W uB = b3CrossW( rB, c->normal );
		b3Vec3W gA = b3MulMVW( c->invIA, uA );
		b3Vec3W gB = b3MulMVW( c->invIB, uB );

		// Relative normal velocity at contact
		b3FloatW vn = b3AddW( linearNormalVelocity, b3SubW( b3DotW( uB, bB->w ), b3DotW( uA, bA->w ) ) );

		// Compute normal impulse
		b3FloatW negImpulse = b3MulW( cp->normalMasses, b3AddW( vn, velocityBias ) );

		// Clamp the accumulated impulse. Not as upstream #189 with b3MaxW( -negImpulse, -normalImpulses ), where two
		// zeros of opposite sign can meet, and SSE, NEON and the scalar path return different ones.
		b3FloatW newImpulse = b3MaxW( b3SubW( cp->normalImpulses, negImpulse ), b3ZeroW() );
		b3FloatW deltaImpulse = b3SubW( newImpulse, cp->normalImpulses );
		cp->normalImpulses = newImpulse;
		cp->totalNormalImpulses = b3AddW( cp->totalNormalImpulses, newImpulse );
		totalNormalImpulse = b3AddW( totalNormalImpulse, newImpulse );
		totalTwistLimit = b3AddW( totalTwistLimit, b3MulW( cp->leverArms, newImpulse ) );

		// Apply contact impulse
		linearNormalVelocity = b3MulAddW( linearNormalVelocity, invMass, deltaImpulse );
		linearImpulse = b3AddW( linearImpulse, deltaImpulse );
		bA->w = b3MulSubSVW( bA->w, deltaImpulse, gA );
		bB->w = b3MulAddSVW( bB->w, deltaImpulse, gB );
	}

	// Rolling resistance
	if ( b3AllZeroW( c->rollingResistance ) == false )
	{
		// flip A/B order to negate
		b3Vec3W deltaImpulse = b3MulMVW( c->rollingMass, b3SubVW( bA->w, bB->w ) );
		b3Vec3W oldImpulse = c->rollingImpulse;
		c->rollingImpulse = b3AddVW( oldImpulse, deltaImpulse );

		b3FloatW maxImpulse = b3MulW( c->rollingResistance, totalNormalImpulse );
		b3FloatW lengthSquared = b3DotW( c->rollingImpulse, c->rollingImpulse );

		// if ( magSqr > maxLambda * maxLambda + FLT_EPSILON )
		//{
		//	c->rollingImpulse *= maxLambda / sqrtf( magSqr );
		// }

		b3FloatW mask = b3GreaterThanW( lengthSquared, b3MulAddW( k->epsilonW, maxImpulse, maxImpulse ) );

		// No approximate _mm_rsqrt_ps here to maintain cross-platform determinism
		b3FloatW normalize = b3DivW( maxImpulse, b3AddW( b3SqrtW( lengthSquared ), k->epsilonW ) );
		b3FloatW scale = b3BlendW( k->oneW, normalize, mask );

		// Ensure zero rolling resistance yields no impulse
		b3FloatW rollingMask = b3GreaterThanW( c->rollingResistance, b3ZeroW() );

		// Blend the result to ensure there are no +/-0 determinism problems, since 0 * -1 == -0. Eight lanes mix
		// contacts with and without rolling resistance that four lanes keep apart, from upstream Box3D (#174).
		b3Vec3W scaledImpulse = b3MulSVW( scale, c->rollingImpulse );
		c->rollingImpulse.X = b3BlendW( b3ZeroW(), scaledImpulse.X, rollingMask );
		c->rollingImpulse.Y = b3BlendW( b3ZeroW(), scaledImpulse.Y, rollingMask );
		c->rollingImpulse.Z = b3BlendW( b3ZeroW(), scaledImpulse.Z, rollingMask );

		deltaImpulse = b3SubVW( c->rollingImpulse, oldImpulse );

		bA->w = b3MulSubMVW( bA->w, c->invIA, deltaImpulse );
		bB->w = b3MulAddMVW( bB->w, c->invIB, deltaImpulse );
	}

	// Central twist friction
	{
		b3FloatW twistSpeed = b3DotW( c->normal, b3SubVW( bB->w, bA->w ) );
		b3FloatW maxLambda = b3MulW( c->friction, totalTwistLimit );
		b3FloatW deltaImpulse = b3NegW( b3MulW( c->twistMass, twistSpeed ) );
		b3FloatW oldImpulse = c->twistImpulse;
		c->twistImpulse = b3SymClampW( b3AddW( oldImpulse, deltaImpulse ), maxLambda );
		deltaImpulse = b3SubW( c->twistImpulse, oldImpulse );

		b3Vec3W L = b3MulSVW( deltaImpulse, c->normal );
		bA->w = b3MulSubMVW( bA->w, c->invIA, L );
		bB->w = b3MulAddMVW( bB->w, c->invIB, L );
	}

	// Central friction
	{
		b3Vec3W tangent1 = c->tangent1;
		b3Vec3W tangent2 = c->tangent2;

		// Fixed anchor points for applying impulses
		b3Vec3W rA = c->centerA;
		b3Vec3W rB = c->centerB;

		// Relative tangent velocity at contact
		b3Vec3W vrA = b3AddVW( bA->v, b3CrossW( bA->w, rA ) );
		b3Vec3W vrB = b3AddVW( bB->v, b3CrossW( bB->w, rB ) );
		b3Vec3W vr = b3SubVW( vrB, vrA );
		b3Vec2W vt = {
			b3SubW( b3DotW( vr, tangent1 ), c->tangentVelocity1 ),
			b3SubW( b3DotW( vr, tangent2 ), c->tangentVelocity2 ),
		};

		// Incremental tangent impulse
		b3Vec2W deltaImpulse = b3MulMV2W( c->tangentMass, vt );
		deltaImpulse = (b3Vec2W){ b3NegW( deltaImpulse.x ), b3NegW( deltaImpulse.y ) };
		b3Vec2W newImpulse = b3AddV2W( c->frictionImpulse, deltaImpulse );

		b3FloatW friction = c->friction;
		b3FloatW maxImpulse = b3MulW( friction, totalNormalImpulse );

		// Clamp the accumulated impulse
		b3FloatW lengthSquared = b3AddW( b3MulW( newImpulse.x, newImpulse.x ), b3MulW( newImpulse.y, newImpulse.y ) );

		// Max impulse can be zero
		b3FloatW mask = b3GreaterThanW( lengthSquared, b3MulW( maxImpulse, maxImpulse ) );

		// No approximate _mm_rsqrt_ps here to maintain cross-platform determinism. Add epsilon to avoid divide by
		// zero.
		b3FloatW normalize = b3DivW( maxImpulse, b3AddW( b3SqrtW( lengthSquared ), k->epsilonW ) );
		b3FloatW scale = b3BlendW( k->oneW, normalize, mask );
		newImpulse = (b3Vec2W){
			b3MulW( scale, newImpulse.x ),
			b3MulW( scale, newImpulse.y ),
		};

		deltaImpulse = (b3Vec2W){
			b3SubW( newImpulse.x, c->frictionImpulse.x ),
			b3SubW( newImpulse.y, c->frictionImpulse.y ),
		};

		c->frictionImpulse = newImpulse;

		// Apply delta impulse, the linear part with the normal impulse
		b3Vec3W P = b3AddVW( b3MulSVW( deltaImpulse.x, tangent1 ), b3MulSVW( deltaImpulse.y, tangent2 ) );
		bA->w = b3MulSubMVW( bA->w, c->invIA, b3CrossW( rA, P ) );
		bB->w = b3MulAddMVW( bB->w, c->invIB, b3CrossW( rB, P ) );

		P = b3MulAddSVW( P, linearImpulse, c->normal );
		bA->v = b3MulSubSVW( bA->v, c->invMassA, P );
		bB->v = b3MulAddSVW( bB->v, c->invMassB, P );
	}
}

// Solve the normal constraint, friction, and rolling resistance.
void B3_WIDE( b3SolveContacts_Convex )( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneDetailNC( solve_contact, "Solve Contact", b3_colorAliceBlue, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->graph->colors[block.colorIndex].wideConstraints;
	b3RelaxConstantsW k = b3GetRelaxConstantsW( context );

	for ( int wideIndex = block.startIndex; wideIndex < block.startIndex + block.count; ++wideIndex )
	{
		b3ContactConstraintWide* c = constraints + wideIndex;
		b3BodyStateW bA = b3GatherBodies( states, c->indexA );
		b3BodyStateW bB = b3GatherBodies( states, c->indexB );
		b3SolveContactW( c, &bA, &bB, &k );
		b3ScatterBodies( states, c->indexA, &bA );
		b3ScatterBodies( states, c->indexB, &bB );
	}

	b3TracyCZoneDetailEnd( solve_contact );
}

// Added for Nebenan: the constants of b3ApplyRestitution_Convex, for b3ApplyRestitutionW
typedef struct b3RestitutionConstantsW
{
	b3FloatW inv_h;
	b3FloatW negRestitutionThreshold;
	b3FloatW zeroW;
	bool propagate;
} b3RestitutionConstantsW;

B3_FORCE_INLINE b3RestitutionConstantsW b3GetRestitutionConstantsW( const b3StepContext* context )
{
	b3RestitutionConstantsW k;
	k.inv_h = b3SplatW( context->inv_h );
	k.negRestitutionThreshold = b3SplatW( -context->world->restitutionThreshold );
	k.zeroW = b3ZeroW();
	k.propagate = context->world->enableRestitutionPropagation;
	return k;
}

// Added for Nebenan: the restitution of one wide constraint on the states of its bodies, for b3ApplyRestitution_Convex and
// b3ApplyKnotRestitution
B3_FORCE_INLINE void b3ApplyRestitutionW( b3ContactConstraintWide* c, b3BodyStateW* bA, b3BodyStateW* bB,
										  const b3RestitutionConstantsW* k )
{
	int pointCount = 0;
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		pointCount = b3MaxInt( pointCount, c->pointCounts[lane] );
	}
	B3_VALIDATE( 0 < pointCount && pointCount <= B3_MAX_MANIFOLD_POINTS );

	b3FloatW restitutionMask = b3GreaterThanW( c->restitution, k->zeroW );
	b3Vec3W dp = b3SubVW( bB->dp, bA->dp );
	b3Matrix3W dqA = b3MakeMatrixFromQuatW( bA->dq );
	b3Matrix3W dqB = b3MakeMatrixFromQuatW( bB->dq );

	for ( int pointIndex = 0; pointIndex < pointCount; ++pointIndex )
	{
		b3ContactConstraintPointWide* cp = c->points + pointIndex;

		b3Vec3W rA = cp->anchorAs;
		b3Vec3W rB = cp->anchorBs;

		b3FloatW normalMass = k->propagate ? cp->normalMasses : b3BlendW( k->zeroW, cp->normalMasses, restitutionMask );

		b3FloatW compressionImpulse = b3SubW( cp->totalNormalImpulses, cp->restitutionImpulses );
		b3FloatW armed = b3AndW( b3AndW( restitutionMask, b3LessThanW( cp->relativeVelocities, k->negRestitutionThreshold ) ),
								 b3GreaterThanW( compressionImpulse, k->zeroW ) );

		b3Vec3W rsA = b3MulM3VW( dqA, rA );
		b3Vec3W rsB = b3MulM3VW( dqB, rB );
		b3Vec3W ds = b3AddVW( dp, b3SubVW( rsB, rsA ) );
		b3FloatW s = b3AddW( b3DotW( c->normal, ds ), cp->baseSeparations );

		b3FloatW specBias = b3MaxW( k->zeroW, b3MulW( s, k->inv_h ) );
		b3FloatW velocityBias = b3BlendW( specBias, b3MulW( c->restitution, cp->relativeVelocities ), armed );

		b3Vec3W vrA = b3AddVW( bA->v, b3CrossW( bA->w, rA ) );
		b3Vec3W vrB = b3AddVW( bB->v, b3CrossW( bB->w, rB ) );
		b3FloatW vn = b3DotW( b3SubVW( vrB, vrA ), c->normal );

		b3FloatW negImpulse = b3MulW( normalMass, b3AddW( vn, velocityBias ) );

		b3FloatW newImpulse = b3MaxW( b3SubW( cp->normalImpulses, negImpulse ), k->zeroW );
		b3FloatW impulse = b3SubW( newImpulse, cp->normalImpulses );

		b3FloatW approachImpulse = b3MinW( b3MaxW( b3NegW( b3MulW( normalMass, vn ) ), k->zeroW ), b3MaxW( impulse, k->zeroW ) );
		b3FloatW allowance =
			b3SubW( b3MulW( c->restitution, b3AddW( compressionImpulse, approachImpulse ) ), cp->restitutionImpulses );
		b3FloatW maxImpulse = b3AddW( approachImpulse, b3MaxW( allowance, k->zeroW ) );
		impulse = b3BlendW( impulse, b3MinW( impulse, maxImpulse ), armed );

		cp->normalImpulses = b3AddW( cp->normalImpulses, impulse );
		cp->restitutionImpulses = b3AddW( cp->restitutionImpulses, b3SubW( impulse, approachImpulse ) );
		cp->totalNormalImpulses = b3AddW( cp->totalNormalImpulses, impulse );

		b3Vec3W P = b3MulSVW( impulse, c->normal );
		bA->w = b3MulSubMVW( bA->w, c->invIA, b3CrossW( rA, P ) );
		bA->v = b3MulSubSVW( bA->v, c->invMassA, P );
		bB->w = b3MulAddMVW( bB->w, c->invIB, b3CrossW( rB, P ) );
		bB->v = b3MulAddSVW( bB->v, c->invMassB, P );
	}
}

void B3_WIDE( b3ApplyRestitution_Convex )( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneDetailNC( restitution, "Restitution", b3_colorDodgerBlue, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->graph->colors[block.colorIndex].wideConstraints;
	b3RestitutionConstantsW k = b3GetRestitutionConstantsW( context );

	for ( int wideIndex = block.startIndex; wideIndex < block.startIndex + block.count; ++wideIndex )
	{
		b3ContactConstraintWide* c = constraints + wideIndex;
		if ( k.propagate == false && b3AllZeroW( c->restitution ) )
		{
			continue;
		}

		b3BodyStateW bA = b3GatherBodies( states, c->indexA );
		b3BodyStateW bB = b3GatherBodies( states, c->indexB );
		b3ApplyRestitutionW( c, &bA, &bB, &k );
		b3ScatterBodies( states, c->indexA, &bA );
		b3ScatterBodies( states, c->indexB, &bB );
	}

	b3TracyCZoneDetailEnd( restitution );
}

// Added for Nebenan: one pass of the wide constraints of a domain, [start, end) of all wide constraints, as the stages of
// the colors run it, see b3Domains
void B3_WIDE( b3SolveDomainContacts )( b3SolverStageType type, b3StepContext* context, int start, int end )
{
	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->wideConstraints;

	switch ( type )
	{
		case b3_stageWarmStart:
			for ( int i = start; i < end; ++i )
			{
				b3ContactConstraintWide* c = constraints + i;
				b3BodyStateW bA = b3GatherBodies( states, c->indexA );
				b3BodyStateW bB = b3GatherBodies( states, c->indexB );
				b3WarmStartContactW( c, &bA, &bB );
				b3ScatterBodies( states, c->indexA, &bA );
				b3ScatterBodies( states, c->indexB, &bB );
			}
			break;

		case b3_stageSolve:
		{
			b3PushConstantsW k = b3GetPushConstantsW( context );
			for ( int i = start; i < end; ++i )
			{
				b3ContactConstraintWide* c = constraints + i;
				b3BodyStateW bA = b3GatherBodies( states, c->indexA );
				b3BodyStateW bB = b3GatherBodies( states, c->indexB );
				b3PushContactW( c, &bA, &bB, &k );
				b3ScatterBodies( states, c->indexA, &bA );
				b3ScatterBodies( states, c->indexB, &bB );
			}
			break;
		}

		case b3_stageRelax:
		{
			b3RelaxConstantsW k = b3GetRelaxConstantsW( context );
			for ( int i = start; i < end; ++i )
			{
				b3ContactConstraintWide* c = constraints + i;
				b3BodyStateW bA = b3GatherBodies( states, c->indexA );
				b3BodyStateW bB = b3GatherBodies( states, c->indexB );
				b3SolveContactW( c, &bA, &bB, &k );
				b3ScatterBodies( states, c->indexA, &bA );
				b3ScatterBodies( states, c->indexB, &bB );
			}
			break;
		}

		case b3_stageRestitution:
		{
			b3RestitutionConstantsW k = b3GetRestitutionConstantsW( context );
			for ( int i = start; i < end; ++i )
			{
				b3ContactConstraintWide* c = constraints + i;
				if ( k.propagate == false && b3AllZeroW( c->restitution ) )
				{
					continue;
				}

				b3BodyStateW bA = b3GatherBodies( states, c->indexA );
				b3BodyStateW bB = b3GatherBodies( states, c->indexB );
				b3ApplyRestitutionW( c, &bA, &bB, &k );
				b3ScatterBodies( states, c->indexA, &bA );
				b3ScatterBodies( states, c->indexB, &bB );
			}
			break;
		}

		default:
			B3_ASSERT( false );
			break;
	}
}

// Added for Nebenan: the wide constraints of a batch of a knot, see b3KnotBatch
#define B3_KNOT_WIDE ( B3_KNOT_LANES / B3_SIMD_WIDTH )

// Added for Nebenan: the knot of a batch as all its lanes see it, see b3KnotBatch. A knot in several groups goes on from
// its copy in the group of the batch, the first batch of the group from the knot.
B3_FORCE_INLINE b3KnotCopies b3BeginKnotBatch( const b3BodyState* states, const b3KnotCopy* knotCopies, const b3KnotBatch* batch )
{
	const b3BodyState* state = states + batch->knot - 1;
	b3KnotCopies copies = { state->linearVelocity, state->angularVelocity, b3Vec3_zero, b3Vec3_zero };
	if ( batch->copy != B3_NULL_INDEX && batch->first == false )
	{
		const b3KnotCopy* copy = knotCopies + batch->copy;
		copies.v0 = copy->linearVelocity;
		copies.w0 = copy->angularVelocity;
	}
	return copies;
}

// Added for Nebenan: the bodies of a wide constraint of a batch, the lanes of the knot with the knot as the batch began
// it, see b3BeginKnotBatch. Only a copy differs from the knot.
B3_FORCE_INLINE b3BodyStateW b3GatherKnotBodies( const b3BodyState* B3_RESTRICT states, const int* B3_RESTRICT indices,
												 const b3KnotBatch* batch, const b3KnotCopies* copies )
{
	b3BodyStateW bodies = b3GatherBodies( states, indices );
	if ( batch->copy == B3_NULL_INDEX || batch->first )
	{
		return bodies;
	}

	float lanes[B3_SIMD_WIDTH];
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		lanes[lane] = indices[lane] == batch->knot ? 1.0f : 0.0f;
	}

	b3FloatW mask = b3EqualsW( b3LoadW( lanes ), b3SplatW( 1.0f ) );
	bodies.v.X = b3BlendW( bodies.v.X, b3SplatW( copies->v0.x ), mask );
	bodies.v.Y = b3BlendW( bodies.v.Y, b3SplatW( copies->v0.y ), mask );
	bodies.v.Z = b3BlendW( bodies.v.Z, b3SplatW( copies->v0.z ), mask );
	bodies.w.X = b3BlendW( bodies.w.X, b3SplatW( copies->w0.x ), mask );
	bodies.w.Y = b3BlendW( bodies.w.Y, b3SplatW( copies->w0.y ), mask );
	bodies.w.Z = b3BlendW( bodies.w.Z, b3SplatW( copies->w0.z ), mask );
	return bodies;
}

// Added for Nebenan: writes the partners of a wide constraint of a knot back and adds up how its lanes changed their copies
// of the knot, lane after lane
B3_FORCE_INLINE void b3ScatterKnotBodies( b3BodyState* B3_RESTRICT states, const b3ContactConstraintWide* c,
										  const b3BodyStateW* bA, const b3BodyStateW* bB, int knot, b3KnotCopies* copies )
{
	float lanes[12][B3_SIMD_WIDTH];
	b3StoreW( lanes[0], bA->v.X );
	b3StoreW( lanes[1], bA->v.Y );
	b3StoreW( lanes[2], bA->v.Z );
	b3StoreW( lanes[3], bA->w.X );
	b3StoreW( lanes[4], bA->w.Y );
	b3StoreW( lanes[5], bA->w.Z );
	b3StoreW( lanes[6], bB->v.X );
	b3StoreW( lanes[7], bB->v.Y );
	b3StoreW( lanes[8], bB->v.Z );
	b3StoreW( lanes[9], bB->w.X );
	b3StoreW( lanes[10], bB->w.Y );
	b3StoreW( lanes[11], bB->w.Z );

	int partnersA[B3_SIMD_WIDTH];
	int partnersB[B3_SIMD_WIDTH];
	for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
	{
		int side = c->indexA[lane] == knot ? 0 : ( c->indexB[lane] == knot ? 6 : -1 );
		partnersA[lane] = side == 0 ? 0 : c->indexA[lane];
		partnersB[lane] = side == 6 ? 0 : c->indexB[lane];
		if ( side >= 0 )
		{
			copies->dv.x += lanes[side][lane] - copies->v0.x;
			copies->dv.y += lanes[side + 1][lane] - copies->v0.y;
			copies->dv.z += lanes[side + 2][lane] - copies->v0.z;
			copies->dw.x += lanes[side + 3][lane] - copies->w0.x;
			copies->dw.y += lanes[side + 4][lane] - copies->w0.y;
			copies->dw.z += lanes[side + 5][lane] - copies->w0.z;
		}
	}

	b3ScatterBodies( states, partnersA, bA );
	b3ScatterBodies( states, partnersB, bB );
}

// Added for Nebenan: the knot takes the mean velocity of its copies, see b3KnotBatch, or the copy in the group of the
// batch for a knot in several groups. A kinematic knot keeps its own.
B3_FORCE_INLINE void b3EndKnotBatch( b3BodyState* states, b3KnotCopy* knotCopies, const b3KnotBatch* batch,
									 const b3KnotCopies* copies )
{
	float share = 1.0f / (float)batch->laneCount;
	if ( batch->copy != B3_NULL_INDEX )
	{
		b3KnotCopy* copy = knotCopies + batch->copy;
		copy->linearVelocity = b3MulAdd( copies->v0, share, copies->dv );
		copy->angularVelocity = b3MulAdd( copies->w0, share, copies->dw );
		return;
	}

	b3BodyState* state = states + batch->knot - 1;
	if ( ( state->flags & b3_dynamicFlag ) == 0 )
	{
		return;
	}

	state->linearVelocity = b3MulAdd( copies->v0, share, copies->dv );
	state->angularVelocity = b3MulAdd( copies->w0, share, copies->dw );
}

void B3_WIDE( b3WarmStartKnots )( b3StepContext* context, int startBatch, int endBatch )
{
	b3TracyCZoneDetailNC( warm_start_knots, "Warm Start Knots", b3_colorGreen, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->knotConstraints;
	const b3KnotBatch* batches = context->knotBatches;
	b3KnotCopy* knotCopies = context->knotCopies;

	for ( int i = startBatch; i < endBatch; ++i )
	{
		const b3KnotBatch* batch = batches + i;
		b3KnotCopies copies = b3BeginKnotBatch( states, knotCopies, batch );
		for ( int j = 0; j < B3_KNOT_WIDE && B3_SIMD_WIDTH * j < batch->laneCount; ++j )
		{
			b3ContactConstraintWide* c = constraints + B3_KNOT_WIDE * i + j;
			b3BodyStateW bA = b3GatherKnotBodies( states, c->indexA, batch, &copies );
			b3BodyStateW bB = b3GatherKnotBodies( states, c->indexB, batch, &copies );
			b3WarmStartContactW( c, &bA, &bB );
			b3ScatterKnotBodies( states, c, &bA, &bB, batch->knot, &copies );
		}
		b3EndKnotBatch( states, knotCopies, batch, &copies );
	}

	b3TracyCZoneDetailEnd( warm_start_knots );
}

void B3_WIDE( b3PushKnots )( b3StepContext* context, int startBatch, int endBatch )
{
	b3TracyCZoneDetailNC( push_knots, "Push Knots", b3_colorAliceBlue, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->knotConstraints;
	const b3KnotBatch* batches = context->knotBatches;
	b3KnotCopy* knotCopies = context->knotCopies;
	b3PushConstantsW k = b3GetPushConstantsW( context );

	for ( int i = startBatch; i < endBatch; ++i )
	{
		const b3KnotBatch* batch = batches + i;
		b3KnotCopies copies = b3BeginKnotBatch( states, knotCopies, batch );
		for ( int j = 0; j < B3_KNOT_WIDE && B3_SIMD_WIDTH * j < batch->laneCount; ++j )
		{
			b3ContactConstraintWide* c = constraints + B3_KNOT_WIDE * i + j;
			b3BodyStateW bA = b3GatherKnotBodies( states, c->indexA, batch, &copies );
			b3BodyStateW bB = b3GatherKnotBodies( states, c->indexB, batch, &copies );
			b3PushContactW( c, &bA, &bB, &k );
			b3ScatterKnotBodies( states, c, &bA, &bB, batch->knot, &copies );
		}
		b3EndKnotBatch( states, knotCopies, batch, &copies );
	}

	b3TracyCZoneDetailEnd( push_knots );
}

void B3_WIDE( b3SolveKnots )( b3StepContext* context, int startBatch, int endBatch )
{
	b3TracyCZoneDetailNC( solve_knots, "Solve Knots", b3_colorAliceBlue, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->knotConstraints;
	const b3KnotBatch* batches = context->knotBatches;
	b3KnotCopy* knotCopies = context->knotCopies;
	b3RelaxConstantsW k = b3GetRelaxConstantsW( context );

	for ( int i = startBatch; i < endBatch; ++i )
	{
		const b3KnotBatch* batch = batches + i;
		b3KnotCopies copies = b3BeginKnotBatch( states, knotCopies, batch );
		for ( int j = 0; j < B3_KNOT_WIDE && B3_SIMD_WIDTH * j < batch->laneCount; ++j )
		{
			b3ContactConstraintWide* c = constraints + B3_KNOT_WIDE * i + j;
			b3BodyStateW bA = b3GatherKnotBodies( states, c->indexA, batch, &copies );
			b3BodyStateW bB = b3GatherKnotBodies( states, c->indexB, batch, &copies );
			b3SolveContactW( c, &bA, &bB, &k );
			b3ScatterKnotBodies( states, c, &bA, &bB, batch->knot, &copies );
		}
		b3EndKnotBatch( states, knotCopies, batch, &copies );
	}

	b3TracyCZoneDetailEnd( solve_knots );
}

// Added for Nebenan: as b3ApplyRestitution_Convex, a wide constraint without restitution leaves its copies of the knot as
// they were
void B3_WIDE( b3ApplyKnotRestitution )( b3StepContext* context, int startBatch, int endBatch )
{
	b3TracyCZoneDetailNC( knot_restitution, "Knot Restitution", b3_colorDodgerBlue, true );

	b3BodyState* states = context->states;
	b3ContactConstraintWide* constraints = context->knotConstraints;
	const b3KnotBatch* batches = context->knotBatches;
	b3KnotCopy* knotCopies = context->knotCopies;
	b3RestitutionConstantsW k = b3GetRestitutionConstantsW( context );

	for ( int i = startBatch; i < endBatch; ++i )
	{
		const b3KnotBatch* batch = batches + i;
		b3KnotCopies copies = b3BeginKnotBatch( states, knotCopies, batch );
		for ( int j = 0; j < B3_KNOT_WIDE && B3_SIMD_WIDTH * j < batch->laneCount; ++j )
		{
			b3ContactConstraintWide* c = constraints + B3_KNOT_WIDE * i + j;
			if ( k.propagate == false && b3AllZeroW( c->restitution ) )
			{
				continue;
			}

			b3BodyStateW bA = b3GatherKnotBodies( states, c->indexA, batch, &copies );
			b3BodyStateW bB = b3GatherKnotBodies( states, c->indexB, batch, &copies );
			b3ApplyRestitutionW( c, &bA, &bB, &k );
			b3ScatterKnotBodies( states, c, &bA, &bB, batch->knot, &copies );
		}
		b3EndKnotBatch( states, knotCopies, batch, &copies );
	}

	b3TracyCZoneDetailEnd( knot_restitution );
}

#undef B3_KNOT_WIDE

// Store impulses by contact constraint
void B3_WIDE( b3StoreImpulses_Convex )( b3SolverBlock block, b3StepContext* context, int workerIndex )
{
	b3TracyCZoneDetailNC( store_impulses, "Store", b3_colorFireBrick, true );

	b3World* world = context->world;
	b3WidePrepareSpan* spans = context->widePrepareSpans;
	const b3ContactConstraintWide* wideBase = context->wideConstraints;
	b3TaskContext* taskContext = world->taskContexts.data + workerIndex;
	b3BitSet* hitEventBitSet = &taskContext->hitEventBitSet;
	bool hasHitEvents = taskContext->hasHitEvents;
	float negHitThreshold = -world->hitEventThreshold;

	int wideIndex = block.startIndex;
	int endWideIndex = block.startIndex + block.count;

	// Find color for start index
	int colorIndex = 0;
	while ( spans[colorIndex + 1].start <= wideIndex )
	{
		colorIndex += 1;
	}

	while ( wideIndex < endWideIndex )
	{
		int colorWideStart = spans[colorIndex].start;
		int colorWideEndIndex = b3MinInt( spans[colorIndex + 1].start, endWideIndex );
		int colorContactCount = spans[colorIndex].count;
		int* contactIds = spans[colorIndex].contacts;

		for ( ; wideIndex < colorWideEndIndex; ++wideIndex )
		{
			const b3ContactConstraintWide* c = wideBase + wideIndex;
			const float* frictionImpulse1 = (float*)&c->frictionImpulse.x;
			const float* frictionImpulse2 = (float*)&c->frictionImpulse.y;
			const float* tangent1X = (float*)&c->tangent1.X;
			const float* tangent1Y = (float*)&c->tangent1.Y;
			const float* tangent1Z = (float*)&c->tangent1.Z;
			const float* tangent2X = (float*)&c->tangent2.X;
			const float* tangent2Y = (float*)&c->tangent2.Y;
			const float* tangent2Z = (float*)&c->tangent2.Z;
			const float* twistImpulse = (float*)&c->twistImpulse;
			const float* rollingImpulseX = (float*)&c->rollingImpulse.X;
			const float* rollingImpulseY = (float*)&c->rollingImpulse.Y;
			const float* rollingImpulseZ = (float*)&c->rollingImpulse.Z;

			int localWideIndex = wideIndex - colorWideStart;

			for ( int lane = 0; lane < B3_SIMD_WIDTH; ++lane )
			{
				int contactIndex = B3_SIMD_WIDTH * localWideIndex + lane;
				if ( contactIndex >= colorContactCount )
				{
					break;
				}

				b3Manifold* m = c->manifolds[lane];
				if ( m == NULL )
				{
					continue;
				}

				float f1 = frictionImpulse1[lane];
				float f2 = frictionImpulse2[lane];
				m->frictionImpulse = (b3Vec3){
					f1 * tangent1X[lane] + f2 * tangent2X[lane],
					f1 * tangent1Y[lane] + f2 * tangent2Y[lane],
					f1 * tangent1Z[lane] + f2 * tangent2Z[lane],
				};
				m->twistImpulse = twistImpulse[lane];
				m->rollingImpulse = (b3Vec3){
					rollingImpulseX[lane],
					rollingImpulseY[lane],
					rollingImpulseZ[lane],
				};

				int pointCount = m->pointCount;
				for ( int pointIndex = 0; pointIndex < pointCount; ++pointIndex )
				{
					const b3ContactConstraintPointWide* cp = c->points + pointIndex;
					const float* normalImpulse = (float*)&cp->normalImpulses;
					const float* totalNormalImpulse = (float*)&cp->totalNormalImpulses;

					b3ManifoldPoint* mp = m->points + pointIndex;
					mp->normalImpulse = normalImpulse[lane];
					mp->totalNormalImpulse = totalNormalImpulse[lane];
				}

				int contactId = contactIds[contactIndex];
				b3Contact* contact = b3Array_Get( world->contacts, contactId );
				if ( ( contact->flags & b3_simEnableHitEvent ) != 0 )
				{
					for ( int k = 0; k < pointCount; ++k )
					{
						b3ManifoldPoint* mp = m->points + k;

						// Need to check total impulse because the point may be speculative and not colliding
						if ( mp->normalVelocity < negHitThreshold && mp->totalNormalImpulse > 0.0f )
						{
							b3SetBit( hitEventBitSet, contact->contactId );
							hasHitEvents = true;
							break;
						}
					}
				}
			}
		}

		colorIndex += 1;
	}

	taskContext->hasHitEvents = hasHitEvents;

	b3TracyCZoneDetailEnd( store_impulses );
}

#undef B3_GATHER_LANES
#undef b3Vec2W
#undef b3Vec3W
#undef b3QuatW
#undef b3SymMatrix2W
#undef b3SymMatrix3W
#undef b3Matrix3W
#undef b3BodyStateW
#undef b3ContactConstraintPointWide
#undef b3ContactConstraintWide
#undef b3PushConstantsW
#undef b3RelaxConstantsW
#undef b3RestitutionConstantsW
