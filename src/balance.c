// SPDX-License-Identifier: MIT

// Balance of a biped: its center of mass, the support of its feet and its capture point, a step to a free spot that
// catches the figure, a fall with a protective pose when no step does, and getting up once the place is safe. The
// decisions follow the roadmap of the game, chapters 26.13 to 26.15, 27.9 and 28.3 to 28.5: fixed thresholds with
// hysteresis, taken in the update after every step in the order of the figures, so the same inputs give the same figure
// on every machine and with any number of threads. Balance only sets targets of joints and bounded forces, the motors and
// Box3D decide where the bodies go.

#include "articulation.h"
#include "world.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

// A step lands with the middle of the sole this far beyond the capture point, so the figure stands with room to spare
#define NB_STEP_OVERSHOOT 0.05f

// The capture point has to lie outside the support in this many updates in a row before a step starts
#define NB_DANGER_UPDATES 2

// Ground a foot stands on: the cosine of its steepest slope, 45 degrees
#define NB_WALKABLE 0.7f

// A step reaches ground this much higher or lower than where the figure stands, in meters
#define NB_MAX_RISE 0.25f

// Every bone slower than this, in meters per second, for this long, in seconds, and the figure is at rest
#define NB_REST_SPEED 0.25f
#define NB_REST_TIME 0.5f

// A standing figure without a foot on the ground for this long falls, in seconds
#define NB_AIRBORNE_TIME 0.3f

// How often a figure that lies checks whether it can get up, in seconds
#define NB_SAFETY_INTERVAL 0.25f

// The upright hold and the lift of getting up come and go over this time, in seconds, so no force jumps
#define NB_RAMP_TIME 0.3f

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

// Getting up, the lift pulls the center of mass over the middle of the feet, with the weight of the figure at this
// distance, and lets go once the center lies this close above it and moves slower than this, in meters per second
#define NB_CENTER_PULL 0.5f
#define NB_CENTERED 0.08f
#define NB_CENTERED_SPEED 0.3f

// A new kind of pose blends in over this many seconds, and the rest pose of a figure that lies down over the second, so
// no limb snaps to it, see the roadmap 26.15
#define NB_POSE_BLEND 0.2f
#define NB_REST_BLEND 0.5f

// The kinds of pose balance drives to, see nbPose
typedef enum nbPoseKind
{
	nb_poseBalance,
	nb_poseCatchFall,
	nb_poseCurl,
	nb_poseRest,
	nb_poseTuck,
	nb_poseRise,
} nbPoseKind;

typedef struct nbBalancer
{
	nbBalanceDef def;
	nbBalance info;

	// The legs at rest: hip pivots in the frame of the pelvis, lengths of thigh and shin, the rotation of the foot in the
	// frame of the pelvis, the corners and the middle of the sole in the frame of the foot, and how far the ankle lies
	// above the sole
	b3Vec3 hip[2];
	float thighLength[2];
	float shinLength[2];
	b3Quat footRest[2];
	b3Vec3 sole[2][4];
	b3Vec3 soleCenter[2];
	float soleHeight[2];

	// Standing: the pelvis origin and the center of mass above the soles, the least gap between the ankles sideways, and
	// the mass of the figure
	float standHeight;
	float standCenter;
	float footGap;
	float mass;

	// Where the ankles are planted, where the swinging one started, and where it lands
	b3Vec3 plant[2];
	b3Vec3 swingStart;
	b3Vec3 stepAnkle;

	// Measured in every update: up, gravity, the height of the ground under the feet along up and under each foot on the
	// ground, the direction the figure faces on the ground, the tilt of the pelvis and the speed of the fastest bone
	b3Vec3 up;
	float gravity;
	float groundHeight;
	float footGround[2];
	b3Vec3 forward;
	float tilt;
	float maxSpeed;

	// Updates in a row with the capture point outside, seconds without a foot on the ground, seconds at rest, seconds to
	// the next check whether the figure can get up
	int dangerUpdates;
	float airborneTime;
	float restTime;
	float checkTime;

	// The figure falls forward, else backward, for its protective pose
	bool fallForward;

	// Getting up: the phase (rising and turning upright, coming over the feet, letting the legs take the weight), when the
	// phase began, how high the lift wants the pelvis, the middle of the feet it pulls the center of mass over and with
	// what part of its force, how far the lift and the upright hold are on, whether the motors run the profile of the
	// program or the one for falling, and the hits seen
	int getUpPhase;
	float riseTime;
	float liftHeight;
	b3Vec3 liftMiddle;
	float liftCap;
	float liftScale;
	float uprightScale;
	bool userProfile;
	int lastHitCount;

	// The foot that stepped last, so near ties go to the other one
	int lastFoot;

	// The pose the joints drive to: the pose of the program with what balance overrides, its kind, and the targets of the
	// last update with the time since a new kind began to blend in from them
	b3Quat* pose;
	b3Quat* applied;
	b3Quat* blendFrom;
	int boneCount;
	nbPoseKind poseKind;
	float blendTime;
	float blendDuration;
} nbBalancer;

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

// The rotation halfway between two others
static b3Quat nbHalfway( b3Quat a, b3Quat b )
{
	return nbHalfwayAt( a, b, 0.5f );
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

typedef struct nbRayContext
{
	const nbWorld* world;
	int index;
	bool hit;
	float fraction;
	b3Vec3 point;
	b3Vec3 normal;
} nbRayContext;

static float nbRayCallback( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
							int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbRayContext* ray = context;
	if ( nbIsArticulationBody( ray->world, ray->index, b3Shape_GetBody( shapeId ) ) )
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
static bool nbFindGround( const nbWorld* world, int index, b3Vec3 point, b3Vec3 up, float distance, float* height )
{
	nbRayContext ray = { world, index, false, 1.0f, b3Vec3_zero, b3Vec3_zero };
	b3World_CastRay( world->physicsWorld, b3ToPos( point ), b3MulSV( -distance, up ), b3DefaultQueryFilter(), nbRayCallback,
					 &ray );
	if ( ray.hit == false || b3Dot( ray.normal, up ) < NB_WALKABLE )
	{
		return false;
	}

	*height = b3Dot( ray.point, up );
	return true;
}

typedef struct nbOverlapContext
{
	const nbWorld* world;
	int index;
	bool blocked;
} nbOverlapContext;

static bool nbOverlapCallback( b3ShapeId shapeId, void* context )
{
	nbOverlapContext* overlap = context;
	if ( nbIsArticulationBody( overlap->world, overlap->index, b3Shape_GetBody( shapeId ) ) )
	{
		return true;
	}

	overlap->blocked = true;
	return false;
}

// Does anything but the figure lie in a convex shape, points relative to an origin?
static bool nbOverlaps( const nbWorld* world, int index, b3Vec3 origin, const b3Vec3* points, int count, float radius )
{
	b3ShapeProxy proxy = { points, count, radius };
	nbOverlapContext overlap = { world, index, false };
	b3World_OverlapShape( world->physicsWorld, b3ToPos( origin ), &proxy, b3DefaultQueryFilter(), nbOverlapCallback, &overlap );
	return overlap.blocked;
}

static float nbCastCallback( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
							 int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( point );
	NB_UNUSED( normal );
	NB_UNUSED( fraction );
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbOverlapContext* cast = context;
	if ( nbIsArticulationBody( cast->world, cast->index, b3Shape_GetBody( shapeId ) ) )
	{
		return -1.0f;
	}

	cast->blocked = true;
	return 0.0f;
}

// Does a sphere moving from one point to another hit anything but the figure?
static bool nbPathBlocked( const nbWorld* world, int index, b3Vec3 from, b3Vec3 to, float radius )
{
	b3Vec3 center = b3Vec3_zero;
	b3ShapeProxy proxy = { &center, 1, radius };
	nbOverlapContext cast = { world, index, false };
	b3World_CastShape( world->physicsWorld, b3ToPos( from ), &proxy, b3Sub( to, from ), b3DefaultQueryFilter(), nbCastCallback,
					   &cast );
	return cast.blocked;
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
		if ( nbIsArticulationBody( world, index, other ) )
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
static int nbSoleCorners( const nbBalancer* b, int foot, b3WorldTransform transform, b3Vec3 e1, b3Vec3 e2, b3Vec2* points )
{
	for ( int k = 0; k < 4; ++k )
	{
		b3Vec3 corner = b3ToVec3( b3TransformWorldPoint( transform, b->sole[foot][k] ) );
		points[k] = (b3Vec2){ b3Dot( corner, e1 ), b3Dot( corner, e2 ) };
	}
	return 4;
}

static void nbMeasure( const nbWorld* world, int index, const nbArticulation* a, nbBalancer* b )
{
	b3Vec3 up = b->up;

	// The center of mass, its velocity and the fastest bone
	float mass = 0.0f;
	b3Vec3 center = b3Vec3_zero;
	b3Vec3 velocity = b3Vec3_zero;
	float maxSpeed = 0.0f;
	for ( int i = 0; i < a->boneCount; ++i )
	{
		b3BodyId body = a->bones[i].bodyId;
		float m = b3Body_GetMass( body );
		b3Vec3 v = b3Body_GetLinearVelocity( body );
		center = b3MulAdd( center, m, b3ToVec3( b3Body_GetWorldCenter( body ) ) );
		velocity = b3MulAdd( velocity, m, v );
		mass += m;
		maxSpeed = b3MaxFloat( maxSpeed, b3Length( v ) );
	}
	center = b3MulSV( 1.0f / mass, center );
	velocity = b3MulSV( 1.0f / mass, velocity );
	b->info.center = b3ToPos( center );
	b->info.velocity = velocity;
	b->maxSpeed = maxSpeed;

	// How far the pelvis tilts and where the figure faces, kept from before while the pelvis lies
	b3WorldTransform pelvis = b3Body_GetTransform( a->bones[0].bodyId );
	float upright = b3Dot( b3RotateVector( pelvis.q, a->up ), up );
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
		b->info.footContact[foot] = nbFootContact( world, index, a->bones[b->def.feet[foot]].bodyId, up, &height );
		if ( b->info.footContact[foot] )
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
		if ( b->info.footContact[foot] )
		{
			b3WorldTransform transform = b3Body_GetTransform( a->bones[b->def.feet[foot]].bodyId );
			pointCount += nbSoleCorners( b, foot, transform, e1, e2, points + pointCount );
		}
	}
	b3Vec2 hull[8];
	int hullCount = pointCount > 0 ? nbHull2( points, pointCount, hull ) : 0;
	b->info.supportCount = hullCount;
	for ( int k = 0; k < hullCount; ++k )
	{
		b3Vec3 corner = b3MulAdd( b3MulAdd( b3MulSV( b->groundHeight, up ), hull[k].x, e1 ), hull[k].y, e2 );
		b->info.support[k] = b3ToPos( corner );
	}

	// The capture point: the center of mass ahead by its velocity over the frequency of a pendulum as long as its height
	float height = b3MaxFloat( b3Dot( center, up ) - b->groundHeight, 0.05f );
	float omega = sqrtf( b3MaxFloat( b->gravity, 0.01f ) / height );
	b3Vec3 capture = nbAtHeight( b3MulAdd( center, 1.0f / omega, nbFlat( velocity, up ) ), up, b->groundHeight );
	b->info.capturePoint = b3ToPos( capture );
	b3Vec2 q = { b3Dot( capture, e1 ), b3Dot( capture, e2 ) };
	b->info.captureMargin = nbPolygonMargin( hull, hullCount, q );

	// Lying on the back: the chest faces up
	int chest = b->def.torso >= 0 ? b->def.torso : 0;
	b3Quat chestRotation = b3Body_GetRotation( a->bones[chest].bodyId );
	b->info.onBack = b3Dot( b3RotateVector( chestRotation, b3Vec3_axisZ ), up ) > 0.0f;
}

// The height of the center of mass above the ground
static float nbCenterHeight( const nbBalancer* b )
{
	return b3Dot( b3ToVec3( b->info.center ), b->up ) - b->groundHeight;
}

//----------------------------------------------------------------------------------------------------------------------
// Inverse kinematics of a leg

// The rotations of thigh, shin and foot against their rest pose that put the ankle at a point with the pelvis at a
// transform, the knee bending forward and the sole flat on the ground. IK places, it does not decide, see the roadmap 28.3.
static void nbLegIK( const nbArticulation* a, nbBalancer* b, int leg, b3WorldTransform pelvis, b3Vec3 ankle )
{
	const nbBalanceDef* def = &b->def;
	const nbBone* thigh = a->bones + def->thighs[leg];
	const nbBone* shin = a->bones + def->shins[leg];
	const nbBone* foot = a->bones + def->feet[leg];
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
	b->pose[def->thighs[leg]] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( pelvis.q, thigh->rest.q ), thighWorld ) );

	b3Quat knee = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -bend );
	b->pose[def->shins[leg]] = knee;
	b3Quat shinWorld = b3MulQuat( b3MulQuat( thighWorld, shin->rest.q ), knee );

	// The sole flat, the foot facing forward
	b3Quat footWorld = b3MulQuat( nbYaw( up, b->forward ), b->footRest[leg] );
	b->pose[def->feet[leg]] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( shinWorld, foot->rest.q ), footWorld ) );
}

// The ankle of a leg as it is now
static b3Vec3 nbAnkle( const nbArticulation* a, const nbBalancer* b, int leg )
{
	return b3ToVec3( b3Body_GetPosition( a->bones[b->def.feet[leg]].bodyId ) );
}

// Where a foot plants: the ankle as it is now, as high above the ground under it as it is with the sole flat. A foot
// that tips or bounces does not carry its height into the plant.
static b3Vec3 nbPlant( const nbArticulation* a, const nbBalancer* b, int leg )
{
	float ground = b->info.footContact[leg] ? b->footGround[leg] : b->groundHeight;
	return nbAtHeight( nbAnkle( a, b, leg ), b->up, ground + b->soleHeight[leg] );
}

// The height the legs hold the pelvis at: the target, lower while the pelvis rises and higher while it sinks, and never
// so far below the pelvis that the legs would pull the feet off the ground
static float nbHoldHeight( const nbArticulation* a, const nbBalancer* b, float target )
{
	b3Vec3 pelvis = b3ToVec3( b3Body_GetPosition( a->bones[0].bodyId ) );
	float speed = b3Dot( b3Body_GetLinearVelocity( a->bones[0].bodyId ), b->up );
	float height = target - NB_HEIGHT_DAMPING * speed;
	return b3MaxFloat( height, b3Dot( pelvis, b->up ) - NB_MAX_DROP );
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
// States

static void nbUseProfile( nbArticulation* a, nbBalancer* b, bool user )
{
	if ( b->userProfile != user )
	{
		b->userProfile = user;
		nbApplyMotorProfile( a, user ? &a->profile : &b->def.fallProfile );
	}
}

static void nbEnter( nbArticulation* a, nbBalancer* b, nbBalanceState state )
{
	b->info.state = state;
	b->info.stateTime = 0.0f;
	switch ( state )
	{
		case nb_balanceStanding:
			b->info.stepCount = 0;
			b->info.stepFoot = -1;
			b->dangerUpdates = 0;
			b->airborneTime = 0.0f;
			for ( int leg = 0; leg < 2; ++leg )
			{
				b->plant[leg] = nbPlant( a, b, leg );
			}
			nbUseProfile( a, b, true );
			break;

		case nb_balanceFalling:
		{
			// The upright hold lets go at once, the lift too
			b->info.stepFoot = -1;
			b->uprightScale = 0.0f;
			b->liftScale = 0.0f;
			b->restTime = 0.0f;
			b->fallForward = b3Dot( b->info.velocity, b->forward ) >= 0.0f;
			nbUseProfile( a, b, false );
			break;
		}

		case nb_balanceDown:
			b->restTime = 0.0f;
			b->checkTime = 0.0f;
			break;

		case nb_balanceGettingUp:
		{
			// Rise facing away from the head when on the back, toward it otherwise, as the pelvis turns upright
			b3Quat pelvis = b3Body_GetRotation( a->bones[0].bodyId );
			b3Vec3 spine = nbFlat( b3RotateVector( pelvis, a->up ), b->up );
			if ( b3Length( spine ) > 0.3f )
			{
				spine = b3Normalize( spine );
				b->forward = b->info.onBack ? b3Neg( spine ) : spine;
			}
			b->getUpPhase = 0;
			b->riseTime = 0.0f;
			b->liftCap = 1.0f;
			b->lastHitCount = a->hitCount;
			nbUseProfile( a, b, true );
			break;
		}

		default:
			break;
	}
}

// Where the ankle of a foot lands so that the support of both feet catches the capture point, if the spot is free,
// within reach and on ground that carries it, the foot landing after a time and swinging from a point. The residual is
// how far the capture point would still lie outside.
static bool nbPlanFoot( const nbWorld* world, int index, const nbArticulation* a, const nbBalancer* b, int foot, float time,
						b3Vec3 from, b3Vec3* target, float* residual )
{
	const nbBalanceDef* def = &b->def;
	int stance = 1 - foot;
	b3Vec3 up = b->up;
	b3Vec3 left = b3Cross( up, b->forward );
	b3Vec3 e1, e2;
	nbGroundBasis( up, &e1, &e2 );

	// Until the foot lands the figure stands on the other one alone, and the capture point runs away from the center of
	// pressure on that sole, by the exponential of the frequency of the pendulum times the time of the step
	b3Vec2 stanceSole[4];
	nbSoleCorners( b, stance, b3Body_GetTransform( a->bones[def->feet[stance]].bodyId ), e1, e2, stanceSole );
	b3Vec2 stanceHull[4];
	int stanceCount = nbHull2( stanceSole, 4, stanceHull );
	b3Vec3 capture = b3ToVec3( b->info.capturePoint );
	b3Vec2 xi = { b3Dot( capture, e1 ), b3Dot( capture, e2 ) };
	b3Vec2 pressure = nbClosestOnPolygon( stanceHull, stanceCount, xi );
	float growth =
		nbExp( sqrtf( b3MaxFloat( b->gravity, 0.01f ) / b3MaxFloat( nbCenterHeight( b ), 0.05f ) ) * b3MaxFloat( time, 0.0f ) );
	b3Vec2 predicted = { pressure.x + growth * ( xi.x - pressure.x ), pressure.y + growth * ( xi.y - pressure.y ) };
	b3Vec3 base = b3MulSV( b->groundHeight, up );
	b3Vec3 landing = b3MulAdd( b3MulAdd( base, predicted.x, e1 ), predicted.y, e2 );

	// The middle of the sole lands beyond it, away from the stance foot
	b3Vec3 away = nbFlat( b3Sub( landing, b3MulAdd( b3MulAdd( base, pressure.x, e1 ), pressure.y, e2 ) ), up );
	away = b3Length( away ) > 1.0e-3f ? b3Normalize( away ) : b->forward;
	b3Vec3 soleTarget = b3MulAdd( landing, NB_STEP_OVERSHOOT, away );

	// The ankle above the middle of its sole, with the foot facing forward
	b3Quat footWorld = b3MulQuat( nbYaw( up, b->forward ), b->footRest[foot] );
	b3Vec3 ankle = b3Sub( soleTarget, nbFlat( b3RotateVector( footWorld, b->soleCenter[foot] ), up ) );

	// The feet stay apart sideways, the left one left of the other, no crossed legs
	float side = foot == 0 ? 1.0f : -1.0f;
	b3Vec3 stanceAnkle = nbAnkle( a, b, stance );
	float lateral = side * b3Dot( nbFlat( b3Sub( ankle, stanceAnkle ), up ), left );
	if ( lateral < b->footGap )
	{
		ankle = b3MulAdd( ankle, side * ( b->footGap - lateral ), left );
	}

	// Within reach of the hip
	b3WorldTransform pelvis = b3Body_GetTransform( a->bones[0].bodyId );
	b3Vec3 hip = b3ToVec3( b3TransformWorldPoint( pelvis, b->hip[foot] ) );
	b3Vec3 reach = nbFlat( b3Sub( ankle, hip ), up );
	float length = b3Length( reach );
	if ( length > def->maxStepLength )
	{
		ankle = b3MulAdd( ankle, def->maxStepLength / length - 1.0f, reach );
	}

	// Ground there that carries the foot, not too far up or down
	float ground;
	b3Vec3 probe = nbAtHeight( ankle, up, b->groundHeight + 0.5f );
	if ( nbFindGround( world, index, probe, up, 1.0f, &ground ) == false || fabsf( ground - b->groundHeight ) > NB_MAX_RISE )
	{
		return false;
	}
	ankle = nbAtHeight( ankle, up, ground + b->soleHeight[foot] );

	// The foot fits there, a centimeter above the ground, and nothing lies on the way at the height of the ankle
	const nbBone* footBone = a->bones + def->feet[foot];
	b3Vec3 corners[8];
	for ( int k = 0; k < 8; ++k )
	{
		b3Vec3 h = { ( k & 1 ? 1.0f : -1.0f ) * ( footBone->halfExtents.x - 0.005f ),
					 ( k & 2 ? 1.0f : -1.0f ) * ( footBone->halfExtents.y - 0.005f ),
					 ( k & 4 ? 1.0f : -1.0f ) * ( footBone->halfExtents.z - 0.005f ) };
		corners[k] = b3RotateVector( footWorld, b3Add( footBone->center, h ) );
	}
	if ( nbOverlaps( world, index, b3MulAdd( ankle, 0.01f, up ), corners, 8, 0.0f ) )
	{
		return false;
	}
	if ( nbPathBlocked( world, index, b3MulAdd( from, def->stepHeight, up ), b3MulAdd( ankle, def->stepHeight, up ), 0.04f ) )
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
static bool nbStartStep( const nbWorld* world, int index, nbArticulation* a, nbBalancer* b )
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
		if ( b->info.footContact[1 - foot] == false ||
			 nbPlanFoot( world, index, a, b, foot, b->def.stepTime, nbAnkle( a, b, foot ), &target, &residual ) == false )
		{
			continue;
		}

		// Near ties go to the foot that did not step last, so the figure steps with both feet in turn
		float score = residual + ( b->info.stepCount > 0 && foot == b->lastFoot ? 0.03f : 0.0f );
		if ( score < bestScore - 1.0e-4f )
		{
			bestFoot = foot;
			bestScore = score;
			bestResidual = residual;
			bestTarget = target;
		}
	}

	if ( bestFoot < 0 || bestResidual > 0.5f * b->def.maxStepLength )
	{
		return false;
	}

	b->lastFoot = bestFoot;
	nbEnter( a, b, nb_balanceStepping );
	b->info.stepFoot = bestFoot;
	b->info.stepCount += 1;
	b->info.stepTarget = b3ToPos( bestTarget );
	b->stepAnkle = bestTarget;
	b->swingStart = nbAnkle( a, b, bestFoot );
	b->dangerUpdates = 0;
	return true;
}

// Is a bone touched by something that moves, or pressed from above by anything? Then it is not safe to get up.
static bool nbDebrisContact( const nbWorld* world, int index, const nbArticulation* a, b3Vec3 up )
{
	b3ContactData contacts[16];
	for ( int i = 0; i < a->boneCount; ++i )
	{
		b3BodyId bone = a->bones[i].bodyId;
		int count = b3Body_GetContactData( bone, contacts, 16 );
		for ( int c = 0; c < count; ++c )
		{
			const b3ContactData* contact = contacts + c;
			b3BodyId bodyA = b3Shape_GetBody( contact->shapeIdA );
			bool boneIsA = B3_ID_EQUALS( bodyA, bone );
			b3BodyId other = boneIsA ? b3Shape_GetBody( contact->shapeIdB ) : bodyA;
			if ( nbIsArticulationBody( world, index, other ) )
			{
				continue;
			}

			for ( int m = 0; m < contact->manifoldCount; ++m )
			{
				const b3Manifold* manifold = contact->manifolds + m;
				if ( manifold->pointCount == 0 )
				{
					continue;
				}

				if ( b3Body_GetType( other ) == b3_dynamicBody )
				{
					return true;
				}

				// From the other shape to the bone: pointing down, the other lies on the figure
				b3Vec3 normal = boneIsA ? b3Neg( manifold->normal ) : manifold->normal;
				if ( b3Dot( normal, up ) < -0.3f )
				{
					return true;
				}
			}
		}
	}
	return false;
}

// Can the figure get up here: ground under it that carries, room to stand up and nothing on it or moving against it?
// See the roadmap 26.15.
static bool nbSafeToGetUp( const nbWorld* world, int index, const nbArticulation* a, nbBalancer* b )
{
	b3Vec3 up = b->up;
	b3Vec3 pelvis = b3ToVec3( b3Body_GetWorldCenter( a->bones[0].bodyId ) );
	float ground;
	if ( nbFindGround( world, index, b3MulAdd( pelvis, 0.3f, up ), up, 2.0f, &ground ) == false )
	{
		return false;
	}

	// A capsule as tall as the figure standing, a little above the ground, as wide as its shoulders
	float height = a->restTop - a->restBottom;
	float radius = 0.12f * height;
	b3Vec3 base = nbAtHeight( pelvis, up, ground );
	b3Vec3 points[2] = { b3MulSV( radius + 0.05f, up ), b3MulSV( height - radius, up ) };
	if ( nbOverlaps( world, index, base, points, 2, radius ) )
	{
		return false;
	}

	if ( nbDebrisContact( world, index, a, up ) )
	{
		return false;
	}

	b->groundHeight = ground;
	return true;
}

// Decide the state for this update, see the roadmap 26.14, 26.15 and 28.3
static void nbDecide( const nbWorld* world, int index, nbArticulation* a, nbBalancer* b, float timeStep )
{
	const nbBalanceDef* def = &b->def;
	bool fallen = b->tilt > def->fallAngle || nbCenterHeight( b ) < def->fallHeight * b->standCenter;
	bool rest = b->maxSpeed < NB_REST_SPEED;
	b->restTime = rest ? b->restTime + timeStep : 0.0f;
	bool hit = a->hitCount != b->lastHitCount;
	b->lastHitCount = a->hitCount;

	switch ( b->info.state )
	{
		case nb_balanceStanding:
		{
			bool grounded = b->info.footContact[0] || b->info.footContact[1];
			b->airborneTime = grounded ? 0.0f : b->airborneTime + timeStep;
			if ( fallen || b->airborneTime > NB_AIRBORNE_TIME )
			{
				nbEnter( a, b, nb_balanceFalling );
				break;
			}

			b->dangerUpdates = grounded && b->info.captureMargin > def->stepMargin ? b->dangerUpdates + 1 : 0;
			if ( b->dangerUpdates >= NB_DANGER_UPDATES && nbStartStep( world, index, a, b ) == false )
			{
				nbEnter( a, b, nb_balanceFalling );
			}
			break;
		}

		case nb_balanceStepping:
		{
			if ( fallen )
			{
				nbEnter( a, b, nb_balanceFalling );
				break;
			}

			// Early in the swing the target follows the capture point, where the step stays free and within reach. The
			// prediction of the capture point at the landing grows less wrong as the landing comes closer.
			int foot = b->info.stepFoot;
			float phase = b->info.stateTime / def->stepTime;
			if ( phase < NB_REPLAN_PHASE )
			{
				b3Vec3 target;
				float residual;
				if ( nbPlanFoot( world, index, a, b, foot, def->stepTime - b->info.stateTime, b->swingStart, &target,
								 &residual ) )
				{
					b->stepAnkle = target;
					b->info.stepTarget = b3ToPos( target );
				}
			}

			// The foot lands when it touches the ground near its target in the second half of the step, or touches it
			// after the step, or the step is long over
			float distance = b3Length( nbFlat( b3Sub( nbAnkle( a, b, foot ), b->stepAnkle ), b->up ) );
			float footSpeed = b3Length( nbFlat( b3Body_GetLinearVelocity( a->bones[def->feet[foot]].bodyId ), b->up ) );
			bool landed = b->info.footContact[foot] && phase > 0.5f && ( distance < NB_LAND_DISTANCE || phase >= 1.0f ) &&
						  footSpeed < NB_LAND_SPEED;
			if ( landed == false && phase < 1.5f )
			{
				break;
			}

			b->plant[foot] = nbPlant( a, b, foot );
			if ( b->info.captureMargin <= def->stepMargin )
			{
				nbEnter( a, b, nb_balanceStanding );
			}
			else if ( b->info.stepCount >= def->maxSteps || nbStartStep( world, index, a, b ) == false )
			{
				nbEnter( a, b, nb_balanceFalling );
			}
			break;
		}

		case nb_balanceFalling:
			if ( b->restTime >= NB_REST_TIME )
			{
				nbEnter( a, b, nb_balanceDown );
			}
			break;

		case nb_balanceDown:
			b->checkTime -= timeStep;
			if ( def->getUpDelay > 0.0f && b->info.stateTime >= def->getUpDelay && b->restTime >= NB_REST_TIME &&
				 b->checkTime <= 0.0f )
			{
				b->checkTime = NB_SAFETY_INTERVAL;
				if ( nbSafeToGetUp( world, index, a, b ) )
				{
					nbEnter( a, b, nb_balanceGettingUp );
				}
			}
			break;

		case nb_balanceGettingUp:
		{
			// A hit or debris breaks off getting up, see the roadmap 26.15
			if ( hit || nbDebrisContact( world, index, a, b->up ) )
			{
				nbEnter( a, b, nb_balanceFalling );
				break;
			}

			float pelvisHeight = b3Dot( b3ToVec3( b3Body_GetPosition( a->bones[0].bodyId ) ), b->up ) - b->groundHeight;
			float phaseTime = b->info.stateTime - b->riseTime;
			if ( b->getUpPhase == 0 )
			{
				// Lifted and turned upright: come over the feet where they stand
				if ( pelvisHeight > 0.75f * b->standHeight && b->tilt < 0.4f )
				{
					b->getUpPhase = 1;
					b->riseTime = b->info.stateTime;
					for ( int leg = 0; leg < 2; ++leg )
					{
						b->plant[leg] = nbPlant( a, b, leg );
					}
				}
			}
			else if ( b->getUpPhase == 1 )
			{
				// Upright on both feet, the center of mass over them and nearly still: let the legs take the weight
				b3Vec3 offset = nbFlat( b3Sub( b3ToVec3( b->info.center ), b->liftMiddle ), b->up );
				float speed = b3Length( nbFlat( b->info.velocity, b->up ) );
				if ( b->info.footContact[0] && b->info.footContact[1] && b->tilt < 0.3f && phaseTime > 0.2f &&
					 b3Length( offset ) < NB_CENTERED && speed < NB_CENTERED_SPEED )
				{
					b->getUpPhase = 2;
					b->riseTime = b->info.stateTime;
				}
			}
			else if ( phaseTime >= def->getUpTime / 3.0f && b->info.captureMargin < 0.0f )
			{
				// The lift has faded and the capture point lies within the feet: the figure stands on its own, see the
				// roadmap 28.4
				nbEnter( a, b, nb_balanceStanding );
				break;
			}

			if ( b->info.stateTime > 4.0f * def->getUpTime )
			{
				// It did not come up, lie and try again later
				nbEnter( a, b, nb_balanceFalling );
			}
			break;
		}

		default:
			break;
	}
}

// Rotate a bone of the pose about its local x axis, for the protective pose
static void nbBend( nbBalancer* b, int bone, float angle )
{
	if ( bone >= 0 && bone < b->boneCount )
	{
		b->pose[bone] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, angle );
	}
}

// The targets of the joints for the state, and the lift of getting up. Returns the kind of pose.
static nbPoseKind nbPose( nbArticulation* a, nbBalancer* b )
{
	const nbBalanceDef* def = &b->def;
	b3Vec3 up = b->up;
	b3WorldTransform pelvis = b3Body_GetTransform( a->bones[0].bodyId );
	b3Vec3 pelvisPoint = b3ToVec3( pelvis.p );
	float standing = b->groundHeight + b->standHeight - NB_KNEE_BEND;
	nbPoseKind kind = nb_poseBalance;

	switch ( b->info.state )
	{
		case nb_balanceStanding:
		{
			// Planted feet stay where they stand, the pelvis moves over the middle of the support a little at a time
			for ( int leg = 0; leg < 2; ++leg )
			{
				if ( b->info.footContact[leg] )
				{
					b->plant[leg] = nbPlant( a, b, leg );
				}
			}

			// Toward the middle of the support and against the motion of the center of mass, like a damped spring
			b3Vec3 middle = b3MulSV( 0.5f, b3Add( b->plant[0], b->plant[1] ) );
			if ( b->info.supportCount > 0 )
			{
				middle = b3Vec3_zero;
				for ( int k = 0; k < b->info.supportCount; ++k )
				{
					middle = b3Add( middle, b3ToVec3( b->info.support[k] ) );
				}
				middle = b3MulSV( 1.0f / (float)b->info.supportCount, middle );
			}
			b3Vec3 shift = b3MulSub( b3MulSV( 0.6f, nbFlat( b3Sub( middle, b3ToVec3( b->info.center ) ), up ) ), 0.25f,
									 nbFlat( b->info.velocity, up ) );
			float length = b3Length( shift );
			if ( length > NB_PELVIS_SHIFT )
			{
				shift = b3MulSV( NB_PELVIS_SHIFT / length, shift );
			}
			b3Vec3 point = b3Add( pelvisPoint, shift );
			float height = nbHoldHeight( a, b, nbReachableHeight( b, point, b->plant, standing ) );
			b3WorldTransform wanted = nbWantedPelvis( b, nbAtHeight( point, up, height ) );
			for ( int leg = 0; leg < 2; ++leg )
			{
				nbLegIK( a, b, leg, wanted, b->plant[leg] );
			}

			// The lift of getting up fades at standing height
			b->liftHeight = b->standHeight - NB_KNEE_BEND;
			break;
		}

		case nb_balanceStepping:
		{
			// The swinging ankle on an arc to its target, the stance foot planted
			int foot = b->info.stepFoot;
			int stance = 1 - foot;
			if ( b->info.footContact[stance] )
			{
				b->plant[stance] = nbPlant( a, b, stance );
			}

			float phase = b3ClampFloat( b->info.stateTime / def->stepTime, 0.0f, 1.0f );
			float progress = nbSmooth( phase );
			b3Vec3 swing = b3Add( b3MulSV( 1.0f - progress, b->swingStart ), b3MulSV( progress, b->stepAnkle ) );
			swing = b3MulAdd( swing, def->stepHeight * b3ComputeCosSin( B3_PI * phase ).sine, up );

			// The stance leg holds the pelvis upright at standing height. The swinging leg reaches from the pelvis turned
			// halfway between upright and as it is: from upright alone the foot misses its target when a push pitches the
			// pelvis, from the pelvis as it is the swing feeds back on itself through the roll it gives the pelvis.
			b3WorldTransform wanted = nbWantedPelvis( b, nbAtHeight( pelvisPoint, up, nbHoldHeight( a, b, standing ) ) );
			b3WorldTransform swingPelvis = { pelvis.p, nbHalfway( pelvis.q, wanted.q ) };
			nbLegIK( a, b, foot, swingPelvis, swing );
			nbLegIK( a, b, stance, wanted, b->plant[stance] );
			break;
		}

		case nb_balanceFalling:
		{
			// Protective pose, see the roadmap 26.13: falling forward the arms reach out to catch the fall and the head
			// stays up, falling backward the figure curls up, chin down and arms in front
			for ( int i = 0; i < b->boneCount; ++i )
			{
				b->pose[i] = b3Quat_identity;
			}
			kind = b->fallForward ? nb_poseCatchFall : nb_poseCurl;
			if ( b->fallForward )
			{
				nbBend( b, def->upperArms[0], 1.2f );
				nbBend( b, def->upperArms[1], 1.2f );
				nbBend( b, def->forearms[0], 0.3f );
				nbBend( b, def->forearms[1], 0.3f );
				nbBend( b, def->head, -0.3f );
				nbBend( b, def->thighs[0], 0.2f );
				nbBend( b, def->thighs[1], 0.2f );
				nbBend( b, def->shins[0], -0.3f );
				nbBend( b, def->shins[1], -0.3f );
			}
			else
			{
				nbBend( b, def->head, 0.5f );
				nbBend( b, def->torso, 0.3f );
				nbBend( b, def->upperArms[0], 0.9f );
				nbBend( b, def->upperArms[1], 0.9f );
				nbBend( b, def->forearms[0], 1.3f );
				nbBend( b, def->forearms[1], 1.3f );
				nbBend( b, def->thighs[0], 0.7f );
				nbBend( b, def->thighs[1], 0.7f );
				nbBend( b, def->shins[0], -1.1f );
				nbBend( b, def->shins[1], -1.1f );
			}
			break;
		}

		case nb_balanceDown:
			// Lying, the body relaxes into its rest pose
			for ( int i = 0; i < b->boneCount; ++i )
			{
				b->pose[i] = b3Quat_identity;
			}
			kind = nb_poseRest;
			break;

		case nb_balanceGettingUp:
		{
			// The lift holds the pelvis at standing height, the upright hold turns it up. A game aid in three phases until
			// authored get-up clips drive the joints instead: lift with the knees a little bent so the feet clear the
			// ground, swing the feet under the hips, and let the legs take the weight while the lift fades. Lifted lower,
			// the upright hold cannot turn a body that lies on the ground.
			b->liftHeight = b->standHeight - NB_KNEE_BEND;
			float phaseTime = b->info.stateTime - b->riseTime;
			if ( b->getUpPhase == 0 )
			{
				for ( int i = 0; i < b->boneCount; ++i )
				{
					b->pose[i] = b3Quat_identity;
				}
				for ( int leg = 0; leg < 2; ++leg )
				{
					nbBend( b, def->upperArms[leg], 0.4f );
				}

				float height = b3Dot( pelvisPoint, up ) - b->groundHeight;
				if ( height < 0.5f * b->standHeight )
				{
					// Low: the knees a little bent, so the feet clear the ground
					for ( int leg = 0; leg < 2; ++leg )
					{
						nbBend( b, def->thighs[leg], 0.4f );
						nbBend( b, def->shins[leg], -0.7f );
					}
					kind = nb_poseTuck;
				}
				else
				{
					// Half up: a foot on the ground stays where it stands, one in the air comes down under its hip, both
					// from the pelvis as it lies now. Legs that pulled planted feet under the hips would push the pelvis
					// over against the upright hold instead.
					kind = nb_poseRise;
					b3WorldTransform level = nbWantedPelvis( b, pelvisPoint );
					for ( int leg = 0; leg < 2; ++leg )
					{
						if ( b->info.footContact[leg] )
						{
							b->plant[leg] = nbPlant( a, b, leg );
						}
						else
						{
							b3Vec3 hip = b3ToVec3( b3TransformWorldPoint( level, b->hip[leg] ) );
							b->plant[leg] = nbAtHeight( hip, up, b->groundHeight + b->soleHeight[leg] );
						}
						nbLegIK( a, b, leg, pelvis, b->plant[leg] );
					}
				}
				b->liftCap = 1.0f;
			}
			else
			{
				// Upright on the feet where they stand: the lift pulls the center of mass over their middle, the legs
				// move the pelvis toward it a little at a time, then the lift fades and the legs take the weight
				kind = nb_poseRise;
				for ( int leg = 0; leg < 2; ++leg )
				{
					if ( b->info.footContact[leg] )
					{
						b->plant[leg] = nbPlant( a, b, leg );
					}
				}
				b->liftMiddle = nbFlat( b3MulSV( 0.5f, b3Add( b->plant[0], b->plant[1] ) ), up );
				b3Vec3 shift = nbFlat( b3Sub( b->liftMiddle, pelvisPoint ), up );
				float length = b3Length( shift );
				if ( length > NB_PELVIS_SHIFT )
				{
					shift = b3MulSV( NB_PELVIS_SHIFT / length, shift );
				}
				b3Vec3 point = b3Add( pelvisPoint, shift );
				float height = nbReachableHeight( b, point, b->plant, b->groundHeight + b->standHeight - NB_KNEE_BEND );
				b->liftHeight = height - b->groundHeight;
				b3WorldTransform wanted = nbWantedPelvis( b, nbAtHeight( point, up, height ) );
				for ( int leg = 0; leg < 2; ++leg )
				{
					nbLegIK( a, b, leg, wanted, b->plant[leg] );
				}
				b->liftCap = b->getUpPhase == 1 ? 1.0f : b3MaxFloat( 0.0f, 1.0f - phaseTime / ( def->getUpTime / 3.0f ) );
			}
			break;
		}

		default:
			break;
	}

	// The lift of getting up, a bounded force toward the wanted height of the pelvis, fading after the figure stands. It
	// lifts the chest, so the body hangs from it and turns upright on its own, where a lift at the pelvis would leave the
	// upper body hanging forward.
	if ( b->liftScale > 0.0f )
	{
		b3BodyId lifted = a->bones[def->torso >= 0 ? def->torso : 0].bodyId;
		float wanted = b->liftHeight;
		float height = b3Dot( pelvisPoint, up ) - b->groundHeight;
		float speed = b3Dot( b3Body_GetLinearVelocity( a->bones[0].bodyId ), up );
		float weight = b->mass * b->gravity;
		float stiffness = weight / 0.25f;
		float damping = 2.0f * sqrtf( stiffness * b->mass );
		float cap = def->getUpForce * weight * b->liftScale * b->liftCap;
		float force = b3ClampFloat( stiffness * ( wanted - height ) - damping * speed, 0.0f, cap );

		// Across, rising, the lift damps the motion of the chest, so the body does not swing up about its feet like a lever
		// and the feet slide under it instead. Upright, it pulls the center of mass over the middle of the feet like a
		// damped spring. Bounded as well.
		b3Vec3 across;
		if ( b->info.state == nb_balanceGettingUp && b->getUpPhase > 0 )
		{
			float pull = weight / NB_CENTER_PULL;
			b3Vec3 offset = nbFlat( b3Sub( b->liftMiddle, b3ToVec3( b->info.center ) ), up );
			across = b3MulSub( b3MulSV( pull, offset ), 2.0f * sqrtf( pull * b->mass ), nbFlat( b->info.velocity, up ) );
		}
		else
		{
			across = b3MulSV( -4.0f * b->mass, nbFlat( b3Body_GetLinearVelocity( lifted ), up ) );
		}
		float acrossLength = b3Length( across );
		float acrossCap = 0.5f * weight * b->liftScale * b->liftCap;
		if ( acrossLength > acrossCap )
		{
			across = b3MulSV( acrossCap / acrossLength, across );
		}
		b3Body_ApplyForceToCenter( lifted, b3MulAdd( across, force, up ), true );
	}
	return kind;
}

// A new kind of pose blends in from the targets of the last update, so no limb snaps from one pose to the next
static void nbBlend( nbBalancer* b, nbPoseKind kind, float timeStep )
{
	if ( kind != b->poseKind )
	{
		memcpy( b->blendFrom, b->applied, sizeof( b3Quat ) * (size_t)b->boneCount );
		b->blendTime = 0.0f;
		b->blendDuration = kind == nb_poseRest ? NB_REST_BLEND : NB_POSE_BLEND;
		b->poseKind = kind;
	}

	if ( b->blendTime < b->blendDuration )
	{
		b->blendTime += timeStep;
		float weight = nbSmooth( b->blendTime / b->blendDuration );
		for ( int i = 0; i < b->boneCount; ++i )
		{
			b->pose[i] = nbHalfwayAt( b->blendFrom[i], b->pose[i], weight );
		}
	}
	memcpy( b->applied, b->pose, sizeof( b3Quat ) * (size_t)b->boneCount );
}

// The upright hold and the lift come and go over NB_RAMP_TIME, the hold lets go at once when the figure falls
static void nbRamp( nbArticulation* a, nbBalancer* b, float timeStep )
{
	nbBalanceState state = b->info.state;
	float step = timeStep / NB_RAMP_TIME;
	bool hold = state == nb_balanceStanding || state == nb_balanceStepping || state == nb_balanceGettingUp;
	float upright = hold ? b3MinFloat( b->uprightScale + step, 1.0f ) : 0.0f;
	if ( upright != b->uprightScale || b->info.stateTime == 0.0f )
	{
		// Getting up the hold may be stronger, a game aid as well, see nbBalanceDef::getUpTorque
		float torque =
			state == nb_balanceGettingUp ? b3MaxFloat( a->profile.uprightTorque, b->def.getUpTorque ) : a->profile.uprightTorque;
		b->uprightScale = upright;
		nbSetUprightTorque( a, b->userProfile ? torque * upright : 0.0f );
	}

	if ( state == nb_balanceGettingUp )
	{
		b->liftScale = b3MinFloat( b->liftScale + step, 1.0f );
	}
	else
	{
		b->liftScale = b3MaxFloat( b->liftScale - step, 0.0f );
	}
}

void nbUpdateBalance( nbWorld* world, int index, nbArticulation* a, float timeStep )
{
	nbBalancer* b = a->balancer;
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b->gravity = b3Length( gravity );
	if ( b->gravity > 0.0f )
	{
		b->up = b3MulSV( -1.0f / b->gravity, gravity );
	}

	nbMeasure( world, index, a, b );
	b->info.stateTime += timeStep;
	nbDecide( world, index, a, b, timeStep );

	// The pose of the program, with what the state overrides
	memcpy( b->pose, a->pose, sizeof( b3Quat ) * (size_t)b->boneCount );
	nbRamp( a, b, timeStep );
	nbPoseKind kind = nbPose( a, b );
	nbBlend( b, kind, timeStep );
	nbSetTargets( a, b->pose );
}

//----------------------------------------------------------------------------------------------------------------------
// API

nbBalanceDef nbDefaultBalanceDef( void )
{
	nbBalanceDef def = { 0 };
	def.thighs[0] = nb_humanoidThighLeft;
	def.thighs[1] = nb_humanoidThighRight;
	def.shins[0] = nb_humanoidShinLeft;
	def.shins[1] = nb_humanoidShinRight;
	def.feet[0] = nb_humanoidFootLeft;
	def.feet[1] = nb_humanoidFootRight;
	def.torso = nb_humanoidTorso;
	def.head = nb_humanoidHead;
	def.upperArms[0] = nb_humanoidUpperArmLeft;
	def.upperArms[1] = nb_humanoidUpperArmRight;
	def.forearms[0] = nb_humanoidForearmLeft;
	def.forearms[1] = nb_humanoidForearmRight;
	def.stepMargin = 0.03f;
	def.maxStepLength = 0.55f;
	def.stepTime = 0.3f;
	def.stepHeight = 0.08f;
	def.maxSteps = 3;
	def.fallAngle = 0.6f;
	def.fallHeight = 0.75f;
	def.fallProfile = nbPassiveMotorProfile();
	def.fallProfile.stiffness = 150.0f;
	def.fallProfile.dampingRatio = 1.0f;
	def.fallProfile.maxTorque = 40.0f;
	def.getUpDelay = 1.0f;
	def.getUpTime = 1.2f;
	def.getUpForce = 1.3f;
	def.getUpTorque = 500.0f;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

void nbFreeBalancer( nbArticulation* articulation )
{
	nbBalancer* b = articulation->balancer;
	if ( b == NULL )
	{
		return;
	}

	nbFree( b->pose, sizeof( b3Quat ) * (size_t)b->boneCount );
	nbFree( b->applied, sizeof( b3Quat ) * (size_t)b->boneCount );
	nbFree( b->blendFrom, sizeof( b3Quat ) * (size_t)b->boneCount );
	nbFree( b, sizeof( nbBalancer ) );
	articulation->balancer = NULL;
}

void nbBalanceProfileChanged( nbArticulation* articulation )
{
	nbBalancer* b = articulation->balancer;
	if ( b->userProfile )
	{
		nbApplyMotorProfile( articulation, &articulation->profile );
		nbSetUprightTorque( articulation, articulation->profile.uprightTorque * b->uprightScale );
	}
}

static bool nbValidBone( const nbArticulation* a, int bone, bool optional )
{
	return ( optional && bone == -1 ) || ( 0 < bone && bone < a->boneCount );
}

bool nbArticulation_EnableBalance( nbArticulationId id, const nbBalanceDef* def )
{
	nbWorld* world;
	nbArticulation* a = nbGetArticulationFromId( id, &world );
	if ( a == NULL )
	{
		return false;
	}

	if ( def == NULL )
	{
		// Back to the motors and the pose of the program
		nbFreeBalancer( a );
		nbApplyMotorProfile( a, &a->profile );
		nbSetTargets( a, a->pose );
		return true;
	}

	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );

	// The legs must be built as described in nbBalanceDef
	bool valid = nbValidBone( a, def->torso, true ) && nbValidBone( a, def->head, true );
	for ( int leg = 0; leg < 2 && valid; ++leg )
	{
		valid = nbValidBone( a, def->thighs[leg], false ) && nbValidBone( a, def->shins[leg], false ) &&
				nbValidBone( a, def->feet[leg], false ) && nbValidBone( a, def->upperArms[leg], true ) &&
				nbValidBone( a, def->forearms[leg], true );
		if ( valid == false )
		{
			break;
		}

		const nbBone* thigh = a->bones + def->thighs[leg];
		const nbBone* shin = a->bones + def->shins[leg];
		const nbBone* foot = a->bones + def->feet[leg];
		b3Vec3 kneeAxis = b3RotateVector( shin->jointFrame, b3Vec3_axisZ );
		valid = thigh->parent == 0 && shin->parent == def->thighs[leg] && foot->parent == def->shins[leg] &&
				shin->joint == nb_hingeJoint && fabsf( kneeAxis.x ) > 0.9f && foot->shape == nb_boxBone &&
				b3Dot( b3RotateVector( thigh->rest.q, b3Vec3_axisY ), a->up ) < -0.8f;
	}
	if ( valid == false || def->stepTime <= 0.0f || def->getUpTime <= 0.0f )
	{
		return false;
	}

	nbFreeBalancer( a );
	nbBalancer* b = nbAlloc( sizeof( nbBalancer ) );
	memset( b, 0, sizeof( nbBalancer ) );
	b->def = *def;
	b->boneCount = a->boneCount;
	b->pose = nbAlloc( sizeof( b3Quat ) * (size_t)a->boneCount );
	b->applied = nbAlloc( sizeof( b3Quat ) * (size_t)a->boneCount );
	b->blendFrom = nbAlloc( sizeof( b3Quat ) * (size_t)a->boneCount );
	memcpy( b->applied, a->pose, sizeof( b3Quat ) * (size_t)a->boneCount );
	b->poseKind = nb_poseBalance;
	a->balancer = b;

	// The legs at rest, in the frame of the pelvis
	float standHeight = 0.0f;
	for ( int leg = 0; leg < 2; ++leg )
	{
		const nbBone* thigh = a->bones + def->thighs[leg];
		const nbBone* shin = a->bones + def->shins[leg];
		const nbBone* foot = a->bones + def->feet[leg];
		b->hip[leg] = thigh->rest.p;
		b->thighLength[leg] = b3Length( shin->rest.p );
		b->shinLength[leg] = b3Length( foot->rest.p );
		b->footRest[leg] = b3NormalizeQuat( b3MulQuat( b3MulQuat( thigh->rest.q, shin->rest.q ), foot->rest.q ) );

		// The sole: the face of the box that points down when the figure stands
		b3Vec3 footUp = b3InvRotateVector( b->footRest[leg], a->up );
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
		standHeight += 0.5f * ( b->soleHeight[leg] - b3Dot( ankle, a->up ) );
	}
	b->standHeight = standHeight;
	b->footGap = 0.6f * b3Length( nbFlat( b3Sub( b->hip[0], b->hip[1] ), a->up ) );

	// The center of mass standing, from the bones at rest
	b3Transform* rest = nbAlloc( sizeof( b3Transform ) * (size_t)a->boneCount );
	b3Vec3 center = b3Vec3_zero;
	float mass = 0.0f;
	for ( int i = 0; i < a->boneCount; ++i )
	{
		const nbBone* bone = a->bones + i;
		rest[i] = i == 0 ? b3Transform_identity : b3MulTransforms( rest[bone->parent], bone->rest );
		float m = b3Body_GetMass( bone->bodyId );
		center = b3MulAdd( center, m, b3TransformPoint( rest[i], b3Body_GetLocalCenter( bone->bodyId ) ) );
		mass += m;
	}
	nbFree( rest, sizeof( b3Transform ) * (size_t)a->boneCount );
	b->mass = mass;
	b->standCenter = b3Dot( b3MulSV( 1.0f / mass, center ), a->up ) + standHeight;

	// Where the figure stands now: up, the ground under its soles, where it faces
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b->gravity = b3Length( gravity );
	b->up = b->gravity > 0.0f ? b3MulSV( -1.0f / b->gravity, gravity ) : b3Vec3_axisY;
	b->groundHeight = FLT_MAX;
	for ( int leg = 0; leg < 2; ++leg )
	{
		b3Vec3 ankle = nbAnkle( a, b, leg );
		b->groundHeight = b3MinFloat( b->groundHeight, b3Dot( ankle, b->up ) - b->soleHeight[leg] );
	}
	b3Quat pelvis = b3Body_GetRotation( a->bones[0].bodyId );
	b3Vec3 facing = nbFlat( b3RotateVector( pelvis, b3Vec3_axisZ ), b->up );
	b->forward = b3Length( facing ) > 0.1f ? b3Normalize( facing ) : b3Normalize( nbFlat( b3Vec3_axisZ, b->up ) );

	b->userProfile = true;
	b->uprightScale = 1.0f;
	b->lastHitCount = a->hitCount;
	nbApplyMotorProfile( a, &a->profile );
	nbEnter( a, b, nb_balanceStanding );
	return true;
}

nbBalance nbArticulation_GetBalance( nbArticulationId id )
{
	const nbArticulation* a = nbGetArticulationFromId( id, NULL );
	if ( a == NULL || a->balancer == NULL )
	{
		nbBalance off = { 0 };
		off.state = nb_balanceOff;
		off.stepFoot = -1;
		return off;
	}
	return a->balancer->info;
}

void nbArticulation_Knockdown( nbArticulationId id )
{
	nbArticulation* a = nbGetArticulationFromId( id, NULL );
	if ( a == NULL || a->balancer == NULL )
	{
		return;
	}

	nbBalanceState state = a->balancer->info.state;
	if ( state == nb_balanceStanding || state == nb_balanceStepping || state == nb_balanceGettingUp )
	{
		nbEnter( a, a->balancer, nb_balanceFalling );
		nbSetUprightTorque( a, 0.0f );
		b3Body_SetAwake( a->bones[0].bodyId, true );
	}
}
