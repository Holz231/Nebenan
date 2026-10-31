// SPDX-License-Identifier: MIT

// A figure that catches itself after a hard hit, after the balance of the branch ragdoll-11 and the roadmap of the game,
// chapters 26.13 to 26.15 and 28.3 to 28.5. Its bones are dynamic, its joints pull toward the pose of its clip with the
// full strength of the motors, and a hold keeps its pelvis upright with a bounded torque, see nbFigureMotors. In every
// update after a step it measures its center of mass and its velocity, which feet stand on ground, the support of their
// soles and the capture point: the center of mass ahead by its velocity over the frequency of a pendulum as high as it
// is. There it would come to rest if it stopped it. Standing, the legs hold the feet where they are and move the pelvis
// over the middle of the support. When the capture point lies outside, a foot steps where the capture point will be when
// it lands, on ground that carries it and with room for the foot. After three steps, or when no step catches it, it
// falls. Calm on its feet it stands animated again. Balance only sets targets of joints and the bounded hold, the motors
// and Box3D decide where the bodies go. The decisions take fixed thresholds in the update after every step in the order
// of the figures, so the same inputs give the same figure on every machine and with any number of threads.

#include "figure.h"
#include "world.h"

#include <float.h>
#include <math.h>
#include <string.h>

// A step lands with the middle of the sole this far beyond the capture point, so the figure stands with room to spare, in
// meters for the humanoid of 1.8 m
#define NB_STEP_OVERSHOOT 0.05f

// The capture point has to lie outside the support by this much, in meters, in this many updates in a row before a step
// starts
#define NB_STEP_MARGIN 0.03f
#define NB_DANGER_UPDATES 2

// A step: the farthest the ankle lands from the hip over the ground, in meters, its time in seconds, how high the foot
// lifts, in meters, and the most steps in a row before the figure gives up and falls
#define NB_MAX_STEP_LENGTH 0.55f
#define NB_STEP_DURATION 0.3f
#define NB_STEP_HEIGHT 0.08f
#define NB_MAX_STEPS 3

// The figure falls when its pelvis tilts farther than this from upright, in radians, or its center of mass sinks below
// this part of its height standing
#define NB_FALL_ANGLE 0.6f
#define NB_FALL_HEIGHT 0.75f

// Ground a foot stands on: the cosine of its steepest slope, 45 degrees
#define NB_WALKABLE 0.7f

// A step reaches ground this much higher or lower than where the figure stands, in meters
#define NB_MAX_RISE 0.25f

// A figure without a foot on the ground for this long falls, in seconds
#define NB_AIRBORNE_TIME 0.3f

// Standing, the pelvis stays this far below straight legs, a soft knee, in meters
#define NB_KNEE_BEND 0.05f

// The legs hold the pelvis lower while it rises and higher while it sinks, by its speed along up times this, in seconds,
// and never more than this below where it is, in meters. So a straight stiff leg does not bounce the figure off the
// ground, and legs that bend do not pull planted feet up.
#define NB_HEIGHT_DAMPING 0.1f
#define NB_MAX_DROP 0.02f

// The pelvis stands no higher than the legs reach at this part of their length, so planted feet stay on the ground in a
// wide stance, and no lower than this part of its height standing
#define NB_LEG_REACH 0.97f
#define NB_LOWEST_PELVIS 0.8f

// The most the standing pelvis is moved toward the middle of the support and against the motion, in meters
#define NB_PELVIS_SHIFT 0.12f

// Until this part of the step the target of the swinging foot follows the capture point
#define NB_REPLAN_PHASE 0.6f

// A swinging foot that touches the ground lands once it is this close to its target, in meters, or the step is over,
// and slower than this along the ground, in meters per second, so a foot that scuffs the ground on its way does not land
#define NB_LAND_DISTANCE 0.06f
#define NB_LAND_SPEED 2.0f

// Calm: the bones slower than this on average by their mass, in meters per second, both feet on the ground, the capture
// point between them and the pelvis upright within this many radians, for this many seconds. Then the figure stands
// animated again. The fastest bone would not do: a hand that rests against a thigh in a wide stance jitters with 0.3 m/s
// for good.
#define NB_CALM_SPEED 0.15f
#define NB_CALM_TILT 0.3f
#define NB_CALM_TIME 0.5f

//----------------------------------------------------------------------------------------------------------------------
// Small geometry

static float nbSmooth( float x )
{
	x = b3ClampFloat( x, 0.0f, 1.0f );
	return x * x * ( 3.0f - 2.0f * x );
}

// The angle of a cosine, from b3Atan2, which gives the same bits on every platform, unlike acosf
static float nbAcos( float cosine )
{
	cosine = b3ClampFloat( cosine, -1.0f, 1.0f );
	return b3Atan2( sqrtf( 1.0f - cosine * cosine ), cosine );
}

// e to the power of x, for x of zero or more: the series of a small part of x, squared back up. Only additions and
// multiplications, so it gives the same bits on every platform, unlike expf.
static float nbExp( float x )
{
	int halvings = 0;
	while ( x > 0.25f && halvings < 20 )
	{
		x *= 0.5f;
		halvings += 1;
	}

	float term = 1.0f;
	float sum = 1.0f;
	for ( int k = 1; k <= 6; ++k )
	{
		term *= x / (float)k;
		sum += term;
	}

	for ( int i = 0; i < halvings; ++i )
	{
		sum *= sum;
	}
	return sum;
}

// The rotation a part of the way from one to another, normalized linear interpolation on the shorter arc
static b3Quat nbHalfwayAt( b3Quat a, b3Quat b, float t )
{
	if ( a.v.x * b.v.x + a.v.y * b.v.y + a.v.z * b.v.z + a.s * b.s < 0.0f )
	{
		b = (b3Quat){ b3Neg( b.v ), -b.s };
	}
	b3Quat q = { b3Add( b3MulSV( 1.0f - t, a.v ), b3MulSV( t, b.v ) ), ( 1.0f - t ) * a.s + t * b.s };
	return b3NormalizeQuat( q );
}

// Without the part along up
static b3Vec3 nbFlat( b3Vec3 v, b3Vec3 up )
{
	return b3MulSub( v, b3Dot( v, up ), up );
}

// The point moved along up to this height
static b3Vec3 nbAtHeight( b3Vec3 p, b3Vec3 up, float height )
{
	return b3MulAdd( p, height - b3Dot( p, up ), up );
}

// Upright, facing forward: the rest frame of the pelvis turned about up
static b3Quat nbYaw( b3Vec3 up, b3Vec3 forward )
{
	b3Vec3 left = b3Normalize( b3Cross( up, forward ) );
	b3Vec3 ahead = b3Cross( left, up );
	b3Matrix3 m = { left, up, ahead };
	return b3NormalizeQuat( b3MakeQuatFromMatrix( &m ) );
}

// Two directions across up, so that they and up make a right handed frame
static void nbGroundBasis( b3Vec3 up, b3Vec3* e1, b3Vec3* e2 )
{
	b3Vec3 helper = fabsf( up.x ) < 0.9f ? b3Vec3_axisX : b3Vec3_axisZ;
	*e1 = b3Normalize( nbFlat( helper, up ) );
	*e2 = b3Cross( up, *e1 );
}

static float nbCross2( b3Vec2 o, b3Vec2 a, b3Vec2 b )
{
	return ( a.x - o.x ) * ( b.y - o.y ) - ( a.y - o.y ) * ( b.x - o.x );
}

// The convex hull of at most 8 points, counter clockwise, monotone chain. Returns the number of corners.
static int nbHull2( const b3Vec2* input, int count, b3Vec2* hull )
{
	b3Vec2 points[8];
	count = b3MinInt( count, 8 );
	memcpy( points, input, sizeof( b3Vec2 ) * (size_t)count );

	// Sorted by x, then y
	for ( int i = 1; i < count; ++i )
	{
		b3Vec2 key = points[i];
		int j = i - 1;
		while ( j >= 0 && ( points[j].x > key.x || ( points[j].x == key.x && points[j].y > key.y ) ) )
		{
			points[j + 1] = points[j];
			j -= 1;
		}
		points[j + 1] = key;
	}

	if ( count < 3 )
	{
		memcpy( hull, points, sizeof( b3Vec2 ) * (size_t)count );
		return count;
	}

	b3Vec2 chain[17];
	int k = 0;
	for ( int i = 0; i < count; ++i )
	{
		while ( k >= 2 && nbCross2( chain[k - 2], chain[k - 1], points[i] ) <= 0.0f )
		{
			k -= 1;
		}
		chain[k++] = points[i];
	}
	for ( int i = count - 2, lower = k + 1; i >= 0; --i )
	{
		while ( k >= lower && nbCross2( chain[k - 2], chain[k - 1], points[i] ) <= 0.0f )
		{
			k -= 1;
		}
		chain[k++] = points[i];
	}

	int hullCount = b3MinInt( k - 1, 8 );
	memcpy( hull, chain, sizeof( b3Vec2 ) * (size_t)hullCount );
	return hullCount;
}

// How far a point lies outside a convex polygon, counter clockwise, negative inside: minus its distance to the nearest
// edge. FLT_MAX for no polygon.
static float nbPolygonMargin( const b3Vec2* polygon, int count, b3Vec2 q )
{
	if ( count == 0 )
	{
		return FLT_MAX;
	}

	bool inside = count >= 3;
	float insideDistance = FLT_MAX;
	float outsideDistance = FLT_MAX;
	for ( int i = 0; i < count; ++i )
	{
		b3Vec2 a = polygon[i];
		b3Vec2 b = polygon[( i + 1 ) % count];
		b3Vec2 edge = { b.x - a.x, b.y - a.y };
		float lengthSquared = edge.x * edge.x + edge.y * edge.y;
		b3Vec2 d = { q.x - a.x, q.y - a.y };

		float t = lengthSquared > 0.0f ? b3ClampFloat( ( d.x * edge.x + d.y * edge.y ) / lengthSquared, 0.0f, 1.0f ) : 0.0f;
		b3Vec2 r = { d.x - t * edge.x, d.y - t * edge.y };
		outsideDistance = b3MinFloat( outsideDistance, sqrtf( r.x * r.x + r.y * r.y ) );

		if ( count >= 3 && lengthSquared > 0.0f )
		{
			// The outward normal of an edge of a counter clockwise polygon is the edge turned clockwise
			float length = sqrtf( lengthSquared );
			float side = ( d.x * edge.y - d.y * edge.x ) / length;
			if ( side > 0.0f )
			{
				inside = false;
			}
			insideDistance = b3MinFloat( insideDistance, -side );
		}
	}

	return inside ? -insideDistance : outsideDistance;
}

// The point of a convex polygon, counter clockwise, nearest to a point: the point itself inside
static b3Vec2 nbClosestOnPolygon( const b3Vec2* polygon, int count, b3Vec2 q )
{
	if ( count == 0 || ( count >= 3 && nbPolygonMargin( polygon, count, q ) <= 0.0f ) )
	{
		return q;
	}

	b3Vec2 best = polygon[0];
	float bestDistance = FLT_MAX;
	for ( int i = 0; i < count; ++i )
	{
		b3Vec2 a = polygon[i];
		b3Vec2 b = polygon[( i + 1 ) % count];
		b3Vec2 edge = { b.x - a.x, b.y - a.y };
		float lengthSquared = edge.x * edge.x + edge.y * edge.y;
		float t = lengthSquared > 0.0f
					  ? b3ClampFloat( ( ( q.x - a.x ) * edge.x + ( q.y - a.y ) * edge.y ) / lengthSquared, 0.0f, 1.0f )
					  : 0.0f;
		b3Vec2 p = { a.x + t * edge.x, a.y + t * edge.y };
		float distance = ( q.x - p.x ) * ( q.x - p.x ) + ( q.y - p.y ) * ( q.y - p.y );
		if ( distance < bestDistance )
		{
			bestDistance = distance;
			best = p;
		}
	}
	return best;
}

//----------------------------------------------------------------------------------------------------------------------
// Queries that skip the bones of the figure

typedef struct nbBalanceRay
{
	const nbWorld* world;
	int index;
	bool hit;
	float fraction;
	b3Vec3 point;
	b3Vec3 normal;
} nbBalanceRay;

static bool nbIsOwnBody( const nbWorld* world, int index, b3BodyId bodyId )
{
	return nbFigureOfBody( world, bodyId, NULL ) == index;
}

static float nbBalanceRayFcn( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
							  int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbBalanceRay* ray = context;
	if ( nbIsOwnBody( ray->world, ray->index, b3Shape_GetBody( shapeId ) ) )
	{
		return -1.0f;
	}

	if ( fraction < ray->fraction )
	{
		ray->hit = true;
		ray->fraction = fraction;
		ray->point = b3ToVec3( point );
		ray->normal = normal;
	}
	return fraction;
}

// The ground straight below a point within a distance: its height along up. False without ground or on ground too steep
// to stand on.
static bool nbGroundBelow( const nbWorld* world, int index, b3Vec3 point, b3Vec3 up, float distance, float* height )
{
	nbBalanceRay ray = { world, index, false, 1.0f, b3Vec3_zero, b3Vec3_zero };
	b3World_CastRay( world->physicsWorld, b3ToPos( point ), b3MulSV( -distance, up ), b3DefaultQueryFilter(), nbBalanceRayFcn,
					 &ray );
	if ( ray.hit == false || b3Dot( ray.normal, up ) < NB_WALKABLE )
	{
		return false;
	}

	*height = b3Dot( ray.point, up );
	return true;
}

typedef struct nbBalanceOverlap
{
	const nbWorld* world;
	int index;
	bool blocked;
} nbBalanceOverlap;

static bool nbBalanceOverlapFcn( b3ShapeId shapeId, void* context )
{
	nbBalanceOverlap* overlap = context;
	if ( nbIsOwnBody( overlap->world, overlap->index, b3Shape_GetBody( shapeId ) ) )
	{
		return true;
	}

	overlap->blocked = true;
	return false;
}

// Does anything but the figure lie in a convex shape, points relative to an origin?
static bool nbShapeBlocked( const nbWorld* world, int index, b3Vec3 origin, const b3Vec3* points, int count, float radius )
{
	b3ShapeProxy proxy = { points, count, radius };
	nbBalanceOverlap overlap = { world, index, false };
	b3World_OverlapShape( world->physicsWorld, b3ToPos( origin ), &proxy, b3DefaultQueryFilter(), nbBalanceOverlapFcn,
						  &overlap );
	return overlap.blocked;
}

static float nbBalanceCastFcn( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
							   int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( point );
	NB_UNUSED( normal );
	NB_UNUSED( fraction );
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbBalanceOverlap* cast = context;
	if ( nbIsOwnBody( cast->world, cast->index, b3Shape_GetBody( shapeId ) ) )
	{
		return -1.0f;
	}

	cast->blocked = true;
	return 0.0f;
}

// Does a sphere moving from one point to another hit anything but the figure?
static bool nbSwingBlocked( const nbWorld* world, int index, b3Vec3 from, b3Vec3 to, float radius )
{
	b3Vec3 center = b3Vec3_zero;
	b3ShapeProxy proxy = { &center, 1, radius };
	nbBalanceOverlap cast = { world, index, false };
	b3World_CastShape( world->physicsWorld, b3ToPos( from ), &proxy, b3Sub( to, from ), b3DefaultQueryFilter(),
					   nbBalanceCastFcn, &cast );
	return cast.blocked;
}

//----------------------------------------------------------------------------------------------------------------------
// The legs at rest

// The legs of a figure: the humanoid of the library
static const int nb_thighs[2] = { nb_humanoidThighLeft, nb_humanoidThighRight };
static const int nb_shins[2] = { nb_humanoidShinLeft, nb_humanoidShinRight };
static const int nb_feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };

bool nbCanBalance( const nbFigure* f )
{
	if ( f->boneCount != nb_humanoidBoneCount )
	{
		return false;
	}

	// Thigh, shin and foot one after the other from the pelvis, the knee a hinge across the figure, the foot a box, the
	// thigh pointing down
	for ( int leg = 0; leg < 2; ++leg )
	{
		const nbBone* thigh = f->bones + nb_thighs[leg];
		const nbBone* shin = f->bones + nb_shins[leg];
		const nbBone* foot = f->bones + nb_feet[leg];
		b3Vec3 kneeAxis = b3RotateVector( shin->jointFrame, b3Vec3_axisZ );
		if ( thigh->parent != 0 || shin->parent != nb_thighs[leg] || foot->parent != nb_shins[leg] ||
			 shin->joint != nb_hingeJoint || fabsf( kneeAxis.x ) < 0.9f || foot->shape != nb_boxBone ||
			 b3Dot( b3RotateVector( thigh->rest.q, b3Vec3_axisY ), b3Vec3_axisY ) > -0.8f )
		{
			return false;
		}
	}
	return true;
}

void nbInitBalancer( nbFigure* f )
{
	nbBalancer* b = &f->balancer;
	memset( b, 0, sizeof( nbBalancer ) );
	b->upright = b3_nullJointId;
	b->stepFoot = -1;
	b->scale = f->def.standHeight / 0.98f;
	b3Vec3 up = b3Vec3_axisY;

	// The legs at rest, in the frame of the pelvis
	float standHeight = 0.0f;
	for ( int leg = 0; leg < 2; ++leg )
	{
		const nbBone* thigh = f->bones + nb_thighs[leg];
		const nbBone* shin = f->bones + nb_shins[leg];
		const nbBone* foot = f->bones + nb_feet[leg];
		b->hip[leg] = thigh->rest.p;
		b->thighLength[leg] = b3Length( shin->rest.p );
		b->shinLength[leg] = b3Length( foot->rest.p );
		b->footRest[leg] = b3NormalizeQuat( b3MulQuat( b3MulQuat( thigh->rest.q, shin->rest.q ), foot->rest.q ) );

		// The sole: the face of the box that points down when the figure stands
		b3Vec3 footUp = b3InvRotateVector( b->footRest[leg], up );
		float components[3] = { footUp.x, footUp.y, footUp.z };
		float extents[3] = { foot->halfExtents.x, foot->halfExtents.y, foot->halfExtents.z };
		int axis = 0;
		for ( int k = 1; k < 3; ++k )
		{
			if ( fabsf( components[k] ) > fabsf( components[axis] ) )
			{
				axis = k;
			}
		}
		float sign = components[axis] > 0.0f ? 1.0f : -1.0f;
		int u = ( axis + 1 ) % 3;
		int v = ( axis + 2 ) % 3;
		for ( int k = 0; k < 4; ++k )
		{
			float offset[3];
			offset[axis] = -sign * extents[axis];
			offset[u] = ( k & 1 ? 1.0f : -1.0f ) * extents[u];
			offset[v] = ( k & 2 ? 1.0f : -1.0f ) * extents[v];
			b->sole[leg][k] = b3Add( foot->center, (b3Vec3){ offset[0], offset[1], offset[2] } );
		}
		b->soleCenter[leg] =
			b3MulSV( 0.25f, b3Add( b3Add( b->sole[leg][0], b->sole[leg][1] ), b3Add( b->sole[leg][2], b->sole[leg][3] ) ) );
		b->soleHeight[leg] = -b3Dot( b->soleCenter[leg], footUp );

		b3Vec3 ankle = b3Add(
			thigh->rest.p, b3RotateVector( thigh->rest.q, b3Add( shin->rest.p, b3RotateVector( shin->rest.q, foot->rest.p ) ) ) );
		standHeight += 0.5f * ( b->soleHeight[leg] - b3Dot( ankle, up ) );
	}
	b->standHeight = standHeight;
	b->footGap = 0.6f * b3Length( nbFlat( b3Sub( b->hip[0], b->hip[1] ), up ) );

	// The center of mass standing, from the bones at rest
	b3Transform rest[nb_humanoidBoneCount];
	b3Vec3 center = b3Vec3_zero;
	float mass = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		rest[i] = i == 0 ? b3Transform_identity : b3MulTransforms( rest[bone->parent], bone->rest );
		center = b3MulAdd( center, bone->mass, b3TransformPoint( rest[i], b3Body_GetLocalCenter( bone->bodyId ) ) );
		mass += bone->mass;
	}
	b->standCenter = b3Dot( b3MulSV( 1.0f / mass, center ), up ) + standHeight;
}

//----------------------------------------------------------------------------------------------------------------------
// Measure

// Does a foot stand on something that is not the figure, on ground flat enough? Its contact points give the height of
// the ground.
static bool nbFootContact( const nbWorld* world, int index, b3BodyId foot, b3Vec3 up, float* height )
{
	b3ContactData contacts[8];
	int count = b3Body_GetContactData( foot, contacts, 8 );
	b3Vec3 center = b3ToVec3( b3Body_GetWorldCenter( foot ) );
	float sum = 0.0f;
	int points = 0;
	for ( int i = 0; i < count; ++i )
	{
		const b3ContactData* contact = contacts + i;
		b3BodyId bodyA = b3Shape_GetBody( contact->shapeIdA );
		bool footIsA = B3_ID_EQUALS( bodyA, foot );
		b3BodyId other = footIsA ? b3Shape_GetBody( contact->shapeIdB ) : bodyA;
		if ( nbIsOwnBody( world, index, other ) )
		{
			continue;
		}

		for ( int m = 0; m < contact->manifoldCount; ++m )
		{
			// The normal points from shape A to shape B, here from the ground to the foot
			const b3Manifold* manifold = contact->manifolds + m;
			b3Vec3 normal = footIsA ? b3Neg( manifold->normal ) : manifold->normal;
			if ( b3Dot( normal, up ) < NB_WALKABLE )
			{
				continue;
			}

			for ( int k = 0; k < manifold->pointCount; ++k )
			{
				// Speculative points that did nothing do not carry
				const b3ManifoldPoint* point = manifold->points + k;
				if ( point->separation > 0.005f && point->totalNormalImpulse <= 0.0f )
				{
					continue;
				}

				b3Vec3 anchor = footIsA ? point->anchorA : point->anchorB;
				sum += b3Dot( b3Add( center, anchor ), up );
				points += 1;
			}
		}
	}

	if ( points == 0 )
	{
		return false;
	}

	*height = sum / (float)points;
	return true;
}

// The corners of a sole on the ground plane, in its two directions
static void nbSoleCorners( const nbBalancer* b, int foot, b3WorldTransform transform, b3Vec3 e1, b3Vec3 e2, b3Vec2* points )
{
	for ( int k = 0; k < 4; ++k )
	{
		b3Vec3 corner = b3ToVec3( b3TransformWorldPoint( transform, b->sole[foot][k] ) );
		points[k] = (b3Vec2){ b3Dot( corner, e1 ), b3Dot( corner, e2 ) };
	}
}

static void nbMeasure( const nbWorld* world, int index, const nbFigure* f, nbBalancer* b )
{
	b3Vec3 up = b->up;

	// The center of mass, its velocity and the speed of the bones on average by their mass
	float mass = 0.0f;
	b3Vec3 center = b3Vec3_zero;
	b3Vec3 velocity = b3Vec3_zero;
	float speed = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3BodyId body = f->bones[i].bodyId;
		float m = f->bones[i].mass;
		b3Vec3 v = b3Body_GetLinearVelocity( body );
		center = b3MulAdd( center, m, b3ToVec3( b3Body_GetWorldCenter( body ) ) );
		velocity = b3MulAdd( velocity, m, v );
		speed += m * b3Length( v );
		mass += m;
	}
	center = b3MulSV( 1.0f / mass, center );
	velocity = b3MulSV( 1.0f / mass, velocity );
	b->center = center;
	b->velocity = velocity;
	b->meanSpeed = speed / mass;

	// How far the pelvis tilts and where the figure faces
	b3WorldTransform pelvis = b3Body_GetTransform( f->bones[0].bodyId );
	float upright = b3Dot( b3RotateVector( pelvis.q, b3Vec3_axisY ), up );
	b->tilt = nbAcos( upright );
	b3Vec3 facing = nbFlat( b3RotateVector( pelvis.q, b3Vec3_axisZ ), up );
	if ( b3Length( facing ) > 0.5f )
	{
		b->forward = b3Normalize( facing );
	}

	// The feet on the ground and its height
	float heightSum = 0.0f;
	int heightCount = 0;
	for ( int foot = 0; foot < 2; ++foot )
	{
		float height;
		b->footContact[foot] = nbFootContact( world, index, f->bones[nb_feet[foot]].bodyId, up, &height );
		if ( b->footContact[foot] )
		{
			b->footGround[foot] = height;
			heightSum += height;
			heightCount += 1;
		}
	}
	if ( heightCount > 0 )
	{
		b->groundHeight = heightSum / (float)heightCount;
	}

	// The support: the soles of the feet on the ground
	b3Vec3 e1, e2;
	nbGroundBasis( up, &e1, &e2 );
	b3Vec2 points[8];
	int pointCount = 0;
	for ( int foot = 0; foot < 2; ++foot )
	{
		if ( b->footContact[foot] )
		{
			b3WorldTransform transform = b3Body_GetTransform( f->bones[nb_feet[foot]].bodyId );
			nbSoleCorners( b, foot, transform, e1, e2, points + pointCount );
			pointCount += 4;
		}
	}
	b3Vec2 hull[8];
	int hullCount = pointCount > 0 ? nbHull2( points, pointCount, hull ) : 0;
	b->supportCount = hullCount;
	b3Vec3 middle = b3Vec3_zero;
	for ( int k = 0; k < hullCount; ++k )
	{
		middle = b3Add( middle, b3MulAdd( b3MulAdd( b3MulSV( b->groundHeight, up ), hull[k].x, e1 ), hull[k].y, e2 ) );
	}
	b->supportMiddle = hullCount > 0 ? b3MulSV( 1.0f / (float)hullCount, middle ) : b3Vec3_zero;

	// The capture point: the center of mass ahead by its velocity over the frequency of a pendulum as long as its height
	float height = b3MaxFloat( b3Dot( center, up ) - b->groundHeight, 0.05f );
	float omega = sqrtf( b3MaxFloat( b->gravity, 0.01f ) / height );
	b3Vec3 capture = nbAtHeight( b3MulAdd( center, 1.0f / omega, nbFlat( velocity, up ) ), up, b->groundHeight );
	b->capture = capture;
	b3Vec2 q = { b3Dot( capture, e1 ), b3Dot( capture, e2 ) };
	b->captureMargin = nbPolygonMargin( hull, hullCount, q );
}

// The height of the center of mass above the ground
static float nbCenterHeight( const nbBalancer* b )
{
	return b3Dot( b->center, b->up ) - b->groundHeight;
}

//----------------------------------------------------------------------------------------------------------------------
// Inverse kinematics of a leg

// The rotations of thigh, shin and foot against their rest pose that put the ankle at a point with the pelvis at a
// transform, the knee bending forward and the sole flat on the ground. IK places, it does not decide, see the roadmap 28.3.
static void nbLegIK( const nbFigure* f, const nbBalancer* b, int leg, b3WorldTransform pelvis, b3Vec3 ankle, b3Quat* pose )
{
	const nbBone* thigh = f->bones + nb_thighs[leg];
	const nbBone* shin = f->bones + nb_shins[leg];
	const nbBone* foot = f->bones + nb_feet[leg];
	b3Vec3 up = b->up;

	b3Vec3 hip = b3ToVec3( b3TransformWorldPoint( pelvis, b->hip[leg] ) );
	float l1 = b->thighLength[leg];
	float l2 = b->shinLength[leg];
	b3Vec3 delta = b3Sub( ankle, hip );
	float distance = b3Length( delta );
	b3Vec3 along = distance > 1.0e-4f ? b3MulSV( 1.0f / distance, delta ) : b3Neg( up );
	distance = b3ClampFloat( distance, 0.25f * ( l1 + l2 ), 0.995f * ( l1 + l2 ) );

	// The knee bends toward the front of the figure, across the line from hip to ankle
	b3Vec3 front = b3MulSub( b->forward, b3Dot( b->forward, along ), along );
	if ( b3Length( front ) < 1.0e-3f )
	{
		front = b3MulSub( up, b3Dot( up, along ), along );
	}
	front = b3Normalize( front );

	float cosHip = b3ClampFloat( ( l1 * l1 + distance * distance - l2 * l2 ) / ( 2.0f * l1 * distance ), -1.0f, 1.0f );
	float sinHip = sqrtf( b3MaxFloat( 0.0f, 1.0f - cosHip * cosHip ) );
	float cosKnee = b3ClampFloat( ( l1 * l1 + l2 * l2 - distance * distance ) / ( 2.0f * l1 * l2 ), -1.0f, 1.0f );
	float bend = B3_PI - nbAcos( cosKnee );

	// The thigh: along the bone to the knee, its front across, the knee axis to the side
	b3Vec3 y = b3Add( b3MulSV( cosHip, along ), b3MulSV( sinHip, front ) );
	b3Vec3 z = b3Sub( b3MulSV( cosHip, front ), b3MulSV( sinHip, along ) );
	b3Vec3 x = b3Cross( y, z );
	b3Matrix3 m = { x, y, z };
	b3Quat thighWorld = b3NormalizeQuat( b3MakeQuatFromMatrix( &m ) );
	pose[nb_thighs[leg]] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( pelvis.q, thigh->rest.q ), thighWorld ) );

	b3Quat knee = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -bend );
	pose[nb_shins[leg]] = knee;
	b3Quat shinWorld = b3MulQuat( b3MulQuat( thighWorld, shin->rest.q ), knee );

	// The sole flat, the foot facing forward
	b3Quat footWorld = b3MulQuat( nbYaw( up, b->forward ), b->footRest[leg] );
	pose[nb_feet[leg]] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( shinWorld, foot->rest.q ), footWorld ) );
}

// The ankle of a leg as it is now
static b3Vec3 nbAnkle( const nbFigure* f, int leg )
{
	return b3ToVec3( b3Body_GetPosition( f->bones[nb_feet[leg]].bodyId ) );
}

// Where a foot plants: the ankle as it is now, as high above the ground under it as it is with the sole flat. A foot
// that tips or bounces does not carry its height into the plant.
static b3Vec3 nbPlant( const nbFigure* f, const nbBalancer* b, int leg )
{
	float ground = b->footContact[leg] ? b->footGround[leg] : b->groundHeight;
	return nbAtHeight( nbAnkle( f, leg ), b->up, ground + b->soleHeight[leg] );
}

// The height the legs hold the pelvis at: the target, lower while the pelvis rises and higher while it sinks, and never
// so far below the pelvis that the legs would pull the feet off the ground
static float nbHoldHeight( const nbFigure* f, const nbBalancer* b, float target )
{
	b3Vec3 pelvis = b3ToVec3( b3Body_GetPosition( f->bones[0].bodyId ) );
	float speed = b3Dot( b3Body_GetLinearVelocity( f->bones[0].bodyId ), b->up );
	float height = target - NB_HEIGHT_DAMPING * speed;
	return b3MaxFloat( height, b3Dot( pelvis, b->up ) - NB_MAX_DROP * b->scale );
}

// The pelvis wanted upright at a point
static b3WorldTransform nbWantedPelvis( const nbBalancer* b, b3Vec3 point )
{
	return (b3WorldTransform){ b3ToPos( point ), nbYaw( b->up, b->forward ) };
}

// How high along up the pelvis can stand upright over a point with both ankles at their targets, at most a height. In a
// wide stance it stands lower, as straight legs would hold the feet above the ground.
static float nbReachableHeight( const nbBalancer* b, b3Vec3 point, const b3Vec3* ankles, float height )
{
	b3Quat q = nbYaw( b->up, b->forward );
	for ( int leg = 0; leg < 2; ++leg )
	{
		b3Vec3 hip = b3RotateVector( q, b->hip[leg] );
		b3Vec3 across = nbFlat( b3Sub( b3Add( point, hip ), ankles[leg] ), b->up );
		float reach = NB_LEG_REACH * ( b->thighLength[leg] + b->shinLength[leg] );
		float rise = sqrtf( b3MaxFloat( reach * reach - b3Dot( across, across ), 0.0f ) );
		height = b3MinFloat( height, b3Dot( ankles[leg], b->up ) + rise - b3Dot( hip, b->up ) );
	}
	return b3MaxFloat( height, b->groundHeight + NB_LOWEST_PELVIS * b->standHeight );
}

//----------------------------------------------------------------------------------------------------------------------
// Steps

// Where the ankle of a foot lands so that the support of both feet catches the capture point, if the spot is free,
// within reach and on ground that carries it, the foot landing after a time and swinging from a point. The residual is
// how far the capture point would still lie outside.
static bool nbPlanFoot( const nbWorld* world, int index, const nbFigure* f, const nbBalancer* b, int foot, float time,
						b3Vec3 from, b3Vec3* target, float* residual )
{
	int stance = 1 - foot;
	b3Vec3 up = b->up;
	b3Vec3 left = b3Cross( up, b->forward );
	b3Vec3 e1, e2;
	nbGroundBasis( up, &e1, &e2 );

	// Until the foot lands the figure stands on the other one alone, and the capture point runs away from the center of
	// pressure on that sole, by the exponential of the frequency of the pendulum times the time of the step
	b3Vec2 stanceSole[4];
	nbSoleCorners( b, stance, b3Body_GetTransform( f->bones[nb_feet[stance]].bodyId ), e1, e2, stanceSole );
	b3Vec2 stanceHull[4];
	int stanceCount = nbHull2( stanceSole, 4, stanceHull );
	b3Vec2 xi = { b3Dot( b->capture, e1 ), b3Dot( b->capture, e2 ) };
	b3Vec2 pressure = nbClosestOnPolygon( stanceHull, stanceCount, xi );
	float growth =
		nbExp( sqrtf( b3MaxFloat( b->gravity, 0.01f ) / b3MaxFloat( nbCenterHeight( b ), 0.05f ) ) * b3MaxFloat( time, 0.0f ) );
	b3Vec2 predicted = { pressure.x + growth * ( xi.x - pressure.x ), pressure.y + growth * ( xi.y - pressure.y ) };
	b3Vec3 base = b3MulSV( b->groundHeight, up );
	b3Vec3 landing = b3MulAdd( b3MulAdd( base, predicted.x, e1 ), predicted.y, e2 );

	// The middle of the sole lands beyond it, away from the stance foot
	b3Vec3 away = nbFlat( b3Sub( landing, b3MulAdd( b3MulAdd( base, pressure.x, e1 ), pressure.y, e2 ) ), up );
	away = b3Length( away ) > 1.0e-3f ? b3Normalize( away ) : b->forward;
	b3Vec3 soleTarget = b3MulAdd( landing, NB_STEP_OVERSHOOT * b->scale, away );

	// The ankle above the middle of its sole, with the foot facing forward
	b3Quat footWorld = b3MulQuat( nbYaw( up, b->forward ), b->footRest[foot] );
	b3Vec3 ankle = b3Sub( soleTarget, nbFlat( b3RotateVector( footWorld, b->soleCenter[foot] ), up ) );

	// The feet stay apart sideways, the left one left of the other, no crossed legs
	float side = foot == 0 ? 1.0f : -1.0f;
	b3Vec3 stanceAnkle = nbAnkle( f, stance );
	float lateral = side * b3Dot( nbFlat( b3Sub( ankle, stanceAnkle ), up ), left );
	if ( lateral < b->footGap )
	{
		ankle = b3MulAdd( ankle, side * ( b->footGap - lateral ), left );
	}

	// Within reach of the hip
	b3WorldTransform pelvis = b3Body_GetTransform( f->bones[0].bodyId );
	b3Vec3 hip = b3ToVec3( b3TransformWorldPoint( pelvis, b->hip[foot] ) );
	b3Vec3 reach = nbFlat( b3Sub( ankle, hip ), up );
	float length = b3Length( reach );
	float maxLength = NB_MAX_STEP_LENGTH * b->scale;
	if ( length > maxLength )
	{
		ankle = b3MulAdd( ankle, maxLength / length - 1.0f, reach );
	}

	// Ground there that carries the foot, not too far up or down
	float ground;
	b3Vec3 probe = nbAtHeight( ankle, up, b->groundHeight + 0.5f * b->scale );
	if ( nbGroundBelow( world, index, probe, up, 1.0f * b->scale, &ground ) == false ||
		 fabsf( ground - b->groundHeight ) > NB_MAX_RISE * b->scale )
	{
		return false;
	}
	ankle = nbAtHeight( ankle, up, ground + b->soleHeight[foot] );

	// The foot fits there, a centimeter above the ground, and nothing lies on the way at the height of the ankle
	const nbBone* footBone = f->bones + nb_feet[foot];
	b3Vec3 corners[8];
	for ( int k = 0; k < 8; ++k )
	{
		b3Vec3 h = { ( k & 1 ? 1.0f : -1.0f ) * ( footBone->halfExtents.x - 0.005f ),
					 ( k & 2 ? 1.0f : -1.0f ) * ( footBone->halfExtents.y - 0.005f ),
					 ( k & 4 ? 1.0f : -1.0f ) * ( footBone->halfExtents.z - 0.005f ) };
		corners[k] = b3RotateVector( footWorld, b3Add( footBone->center, h ) );
	}
	if ( nbShapeBlocked( world, index, b3MulAdd( ankle, 0.01f, up ), corners, 8, 0.0f ) )
	{
		return false;
	}
	float lift = NB_STEP_HEIGHT * b->scale;
	if ( nbSwingBlocked( world, index, b3MulAdd( from, lift, up ), b3MulAdd( ankle, lift, up ), 0.04f * b->scale ) )
	{
		return false;
	}

	// How far the capture point at the landing stays outside the stance sole and the sole at the target
	b3Vec2 points[8];
	memcpy( points, stanceSole, sizeof( stanceSole ) );
	nbSoleCorners( b, foot, (b3WorldTransform){ b3ToPos( ankle ), footWorld }, e1, e2, points + 4 );
	b3Vec2 hull[8];
	int hullCount = nbHull2( points, 8, hull );
	*residual = nbPolygonMargin( hull, hullCount, predicted );
	*target = ankle;
	return true;
}

// A step with the foot that leaves the capture point nearest to the support, or false if no foot can step or no step
// would come close to catching the figure
static bool nbStartCatchStep( const nbWorld* world, int index, nbFigure* f, nbBalancer* b )
{
	int bestFoot = -1;
	float bestScore = FLT_MAX;
	float bestResidual = FLT_MAX;
	b3Vec3 bestTarget = b3Vec3_zero;
	for ( int foot = 0; foot < 2; ++foot )
	{
		// The other foot carries the figure while this one swings
		b3Vec3 target;
		float residual;
		if ( b->footContact[1 - foot] == false ||
			 nbPlanFoot( world, index, f, b, foot, NB_STEP_DURATION, nbAnkle( f, foot ), &target, &residual ) == false )
		{
			continue;
		}

		// Near ties go to the foot that did not step last, so the figure steps with both feet in turn
		float score = residual + ( b->stepCount > 0 && foot == b->lastFoot ? 0.03f : 0.0f );
		if ( score < bestScore - 1.0e-4f )
		{
			bestFoot = foot;
			bestScore = score;
			bestResidual = residual;
			bestTarget = target;
		}
	}

	if ( bestFoot < 0 || bestResidual > 0.5f * NB_MAX_STEP_LENGTH * b->scale )
	{
		return false;
	}

	b->lastFoot = bestFoot;
	b->stepping = true;
	b->stateTime = 0.0f;
	b->stepFoot = bestFoot;
	b->stepCount += 1;
	b->stepAnkle = bestTarget;
	b->swingStart = nbAnkle( f, bestFoot );
	b->dangerUpdates = 0;
	f->stepCount += 1;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
// The figure catches itself

// The static body every upright hold of a world holds against, made the first time a figure needs it
static b3BodyId nbUprightAnchor( nbWorld* world )
{
	if ( b3Body_IsValid( world->figureAnchor ) == false )
	{
		b3BodyDef def = b3DefaultBodyDef();
		def.type = b3_staticBody;
		world->figureAnchor = b3CreateBody( world->physicsWorld, &def );
	}
	return world->figureAnchor;
}

// The rotation that turns z into a direction
static b3Quat nbQuatZTo( b3Vec3 direction )
{
	float length = b3Length( direction );
	if ( length < FLT_EPSILON )
	{
		return b3Quat_identity;
	}
	return b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3MulSV( 1.0f / length, direction ) );
}

void nbBeginBalance( nbWorld* world, int index, nbFigure* f )
{
	nbBalancer* b = &f->balancer;
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b->gravity = b3Length( gravity );
	b->up = b->gravity > 0.0f ? b3MulSV( -1.0f / b->gravity, gravity ) : b3Vec3_axisY;

	// Where it stands: the ground under its soles, where it faces
	b->groundHeight = FLT_MAX;
	for ( int leg = 0; leg < 2; ++leg )
	{
		b3Vec3 ankle = nbAnkle( f, leg );
		b->groundHeight = b3MinFloat( b->groundHeight, b3Dot( ankle, b->up ) - b->soleHeight[leg] );
	}
	b3Quat pelvis = b3Body_GetRotation( f->bones[0].bodyId );
	b3Vec3 facing = nbFlat( b3RotateVector( pelvis, b3Vec3_axisZ ), b->up );
	b->forward = b3Length( facing ) > 0.1f ? b3Normalize( facing ) : b3RotateVector( f->frame.q, b3Vec3_axisZ );

	nbMeasure( world, index, f, b );
	for ( int leg = 0; leg < 2; ++leg )
	{
		b->plant[leg] = nbPlant( f, b, leg );
	}
	b->stepping = false;
	b->stateTime = 0.0f;
	b->stepFoot = -1;
	b->stepCount = 0;
	b->dangerUpdates = 0;
	b->airborneTime = 0.0f;
	b->calmTime = 0.0f;

	// The upright hold: a parallel joint keeps the up axis of the pelvis along the up of the world, against the static
	// anchor of the world, with a bounded torque
	const nbFigureMotors* motors = &f->def.motors;
	if ( motors->uprightTorque > 0.0f && motors->uprightStiffness > 0.0f )
	{
		b3BodyId anchor = nbUprightAnchor( world );
		b3BodyId root = f->bones[0].bodyId;
		b3ParallelJointDef def = b3DefaultParallelJointDef();
		def.base.bodyIdA = anchor;
		def.base.bodyIdB = root;
		b3Vec3 upInAnchor = b3InvRotateVector( b3Body_GetRotation( anchor ), b->up );
		def.base.localFrameA = (b3Transform){ b3Vec3_zero, nbQuatZTo( upInAnchor ) };
		def.base.localFrameB = (b3Transform){ b3Vec3_zero, nbQuatZTo( b3Vec3_axisY ) };
		def.base.collideConnected = false;
		b3Quat frameB = b3MulQuat( pelvis, def.base.localFrameB.q );
		def.hertz = nbJointSpringHertz( anchor, root, frameB, false, motors->uprightStiffness );
		def.dampingRatio = b3MaxFloat( 0.0f, motors->uprightDampingRatio );
		def.maxTorque = motors->uprightTorque;
		b->upright = b3CreateParallelJoint( world->physicsWorld, &def );
	}
}

void nbEndBalance( nbFigure* f )
{
	nbBalancer* b = &f->balancer;
	if ( b3Joint_IsValid( b->upright ) )
	{
		b3DestroyJoint( b->upright, true );
	}
	b->upright = b3_nullJointId;
	b->stepping = false;
	b->stepFoot = -1;
}

nbBalanceResult nbUpdateBalance( nbWorld* world, int index, nbFigure* f, float timeStep, b3Quat* pose )
{
	nbBalancer* b = &f->balancer;
	nbMeasure( world, index, f, b );
	b->stateTime += timeStep;

	// Decide, see the roadmap 26.14, 26.15 and 28.3
	bool fallen = b->tilt > NB_FALL_ANGLE || nbCenterHeight( b ) < NB_FALL_HEIGHT * b->standCenter;
	if ( fallen )
	{
		return nb_balanceFell;
	}

	if ( b->stepping == false )
	{
		bool grounded = b->footContact[0] || b->footContact[1];
		b->airborneTime = grounded ? 0.0f : b->airborneTime + timeStep;
		if ( b->airborneTime > NB_AIRBORNE_TIME )
		{
			return nb_balanceFell;
		}

		b->dangerUpdates = grounded && b->captureMargin > NB_STEP_MARGIN * b->scale ? b->dangerUpdates + 1 : 0;
		if ( b->dangerUpdates >= NB_DANGER_UPDATES && nbStartCatchStep( world, index, f, b ) == false )
		{
			return nb_balanceFell;
		}
	}
	else
	{
		// Early in the swing the target follows the capture point, where the step stays free and within reach. The
		// prediction of the capture point at the landing grows less wrong as the landing comes closer.
		int foot = b->stepFoot;
		float phase = b->stateTime / NB_STEP_DURATION;
		if ( phase < NB_REPLAN_PHASE )
		{
			b3Vec3 target;
			float residual;
			if ( nbPlanFoot( world, index, f, b, foot, NB_STEP_DURATION - b->stateTime, b->swingStart, &target, &residual ) )
			{
				b->stepAnkle = target;
			}
		}

		// The foot lands when it touches the ground near its target in the second half of the step, or touches it
		// after the step, or the step is long over
		float distance = b3Length( nbFlat( b3Sub( nbAnkle( f, foot ), b->stepAnkle ), b->up ) );
		float footSpeed = b3Length( nbFlat( b3Body_GetLinearVelocity( f->bones[nb_feet[foot]].bodyId ), b->up ) );
		bool landed = b->footContact[foot] && phase > 0.5f && ( distance < NB_LAND_DISTANCE * b->scale || phase >= 1.0f ) &&
					  footSpeed < NB_LAND_SPEED;
		if ( landed || phase >= 1.5f )
		{
			// Caught it stands, else the next step, up to the most steps in a row
			b->plant[foot] = nbPlant( f, b, foot );
			if ( b->captureMargin <= NB_STEP_MARGIN * b->scale )
			{
				b->stepping = false;
				b->stateTime = 0.0f;
				b->stepFoot = -1;
				b->stepCount = 0;
				b->dangerUpdates = 0;
				b->airborneTime = 0.0f;
				for ( int leg = 0; leg < 2; ++leg )
				{
					b->plant[leg] = nbPlant( f, b, leg );
				}
			}
			else if ( b->stepCount >= NB_MAX_STEPS || nbStartCatchStep( world, index, f, b ) == false )
			{
				return nb_balanceFell;
			}
		}
	}

	// Calm on its feet it stands animated again
	bool calm = b->stepping == false && b->footContact[0] && b->footContact[1] && b->captureMargin <= 0.0f &&
				b->meanSpeed < NB_CALM_SPEED && b->tilt < NB_CALM_TILT;
	b->calmTime = calm ? b->calmTime + timeStep : 0.0f;
	if ( b->calmTime >= NB_CALM_TIME )
	{
		return nb_balanceSettled;
	}

	// The pose of the clip with the legs of balance
	b3Vec3 up = b->up;
	b3WorldTransform pelvis = b3Body_GetTransform( f->bones[0].bodyId );
	b3Vec3 pelvisPoint = b3ToVec3( pelvis.p );
	float standing = b->groundHeight + b->standHeight - NB_KNEE_BEND * b->scale;
	if ( b->stepping == false )
	{
		// Planted feet stay where they stand, the pelvis moves over the middle of the support a little at a time
		for ( int leg = 0; leg < 2; ++leg )
		{
			if ( b->footContact[leg] )
			{
				b->plant[leg] = nbPlant( f, b, leg );
			}
		}

		// Toward the middle of the support and against the motion of the center of mass, like a damped spring
		b3Vec3 middle = b->supportCount > 0 ? b->supportMiddle : b3MulSV( 0.5f, b3Add( b->plant[0], b->plant[1] ) );
		b3Vec3 shift = b3MulSub( b3MulSV( 0.6f, nbFlat( b3Sub( middle, b->center ), up ) ), 0.25f, nbFlat( b->velocity, up ) );
		float length = b3Length( shift );
		float maxShift = NB_PELVIS_SHIFT * b->scale;
		if ( length > maxShift )
		{
			shift = b3MulSV( maxShift / length, shift );
		}
		b3Vec3 point = b3Add( pelvisPoint, shift );
		float height = nbHoldHeight( f, b, nbReachableHeight( b, point, b->plant, standing ) );
		b3WorldTransform wanted = nbWantedPelvis( b, nbAtHeight( point, up, height ) );
		for ( int leg = 0; leg < 2; ++leg )
		{
			nbLegIK( f, b, leg, wanted, b->plant[leg], pose );
		}
	}
	else
	{
		// The swinging ankle on an arc to its target, the stance foot planted
		int foot = b->stepFoot;
		int stance = 1 - foot;
		if ( b->footContact[stance] )
		{
			b->plant[stance] = nbPlant( f, b, stance );
		}

		float phase = b3ClampFloat( b->stateTime / NB_STEP_DURATION, 0.0f, 1.0f );
		float progress = nbSmooth( phase );
		b3Vec3 swing = b3Add( b3MulSV( 1.0f - progress, b->swingStart ), b3MulSV( progress, b->stepAnkle ) );
		swing = b3MulAdd( swing, NB_STEP_HEIGHT * b->scale * b3ComputeCosSin( B3_PI * phase ).sine, up );

		// The stance leg holds the pelvis upright at standing height. The swinging leg reaches from the pelvis turned
		// halfway between upright and as it is: from upright alone the foot misses its target when a push pitches the
		// pelvis, from the pelvis as it is the swing feeds back on itself through the roll it gives the pelvis.
		b3WorldTransform wanted = nbWantedPelvis( b, nbAtHeight( pelvisPoint, up, nbHoldHeight( f, b, standing ) ) );
		b3WorldTransform swingPelvis = { pelvis.p, nbHalfwayAt( pelvis.q, wanted.q, 0.5f ) };
		nbLegIK( f, b, foot, swingPelvis, swing, pose );
		nbLegIK( f, b, stance, wanted, b->plant[stance], pose );
	}

	return nb_balanceGoesOn;
}

bool nbBalanceStepping( const nbFigure* f )
{
	return f->state == nb_figureBalancing && f->balancer.stepping;
}

b3Vec3 nbBalanceForward( const nbFigure* f )
{
	return f->balancer.forward;
}

float nbBalanceGround( const nbFigure* f )
{
	return f->balancer.groundHeight;
}
