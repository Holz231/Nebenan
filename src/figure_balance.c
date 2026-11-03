// SPDX-License-Identifier: MIT

// A figure that catches itself after a hard hit, after the balance of the branch ragdoll-11 and the roadmap of the
// game, chapters 26.13 to 26.15 and 28.3 to 28.5. Its bones are dynamic, its joints pull toward the pose of its clip
// with the full strength of the motors, and a hold keeps its pelvis upright with a bounded torque, see nbFigureMotors.
// In every update after a step it measures its center of mass, its velocity and how it turns, which feet stand on
// ground and where they face, the support of their soles and the capture point: the center of mass ahead by its
// velocity and its turn over the frequency of a pendulum as high as it is. There it would come to rest if it stopped
// it. It faces after its feet, not its pelvis. Standing, the legs hold the feet where and as they are, move the pelvis
// over the middle of the support and turn it back over the feet. The spine bends the upper body on as the pelvis tilts,
// which turns the pelvis back. When the capture point lies outside, a foot steps
// where the capture point will be when it lands, turned as the stance foot, on ground that carries it and with room for
// the foot, as far as it reaches. When that step comes short, the arms swing out in the sense of the fall, and their
// motors turn the body back against it. Five steps in a row that do not catch it, a tilt, a sinking, a step that no
// foot can take or one that comes hopelessly short, and it falls. Calm on its feet it stands animated again.
// Balance only sets targets of joints, the strength of the arms and the bounded hold, the motors and Box3D decide where
// the bodies go. The decisions take fixed thresholds in the update after every step in the order of the figures, so the
// same inputs give the same figure on every machine and with any number of threads.

#include "figure.h"
#include "world.h"

#include <float.h>
#include <math.h>
#include <string.h>

// A step lands with the middle of the sole this far beyond the capture point, so the figure stands with room to spare, in
// meters for the humanoid of 1.8 m
#define NB_STEP_OVERSHOOT 0.05f

// The capture point has to lie outside the support by this much, in meters, in this many updates in a row before a step
// starts. A figure that catches itself with its own strength steps after one update when it lies outside by
// NB_STEP_URGENT, in meters for the humanoid of 1.8 m: two updates keep a figure that only sways from stepping, a hit
// throws the capture point far out at once, and without the hold against the world a figure that waits an update for it
// falls on. Of the 1 920 hits on chest and belly from 50 to 89 N s 50 fell instead of 110, see the roadmap, F61, round
// 49.
#define NB_STEP_MARGIN 0.03f
#define NB_DANGER_UPDATES 2
#define NB_STEP_URGENT 0.1f

// A step: the farthest the ankle lands from the hip over the ground, in meters, its time in seconds, how high the foot
// lifts, in meters, and the most steps in a row before the figure gives up and falls. Five: rowing arms give a fourth
// step the time to catch a figure that three did not, and arms that swing out and hold instead of pulling hard leave
// some light hits a fifth step short. With five, none of 672 hits up to 80 N s fell. A quicker step after a harder push
// does not catch more: the swinging foot follows its target about 0.1 s late, so a step of 0.2 s back from a push of
// 64 N s took the foot 15 instead of 57 cm, see the roadmap, F60. The humanoid stands in its knees with its feet behind
// the pelvis and sinks while it steps after a hard push, so a foot that lifted 8 cm touched the ground early and landed
// short, after 120 N s from behind 37 cm before its target. Lifted 12 cm it lands farther.
#define NB_MAX_STEP_LENGTH 0.55f
#define NB_STEP_DURATION 0.3f
#define NB_STEP_HEIGHT 0.12f
#define NB_MAX_STEPS 5

// A figure that catches itself with its own strength, without the hold against the world, see nbFigureMotors::
// uprightAgainstWorld, needs more steps to bring its body to rest: the hold against the world held the body upright
// while the steps stopped it. Of the 1 920 hits on chest and belly from 50 to 89 N s, see the roadmap, F61, round 49,
// it caught 35 more with ten steps than with five.
#define NB_OWN_MAX_STEPS 10

// A figure that catches itself with its own strength braces its knees: their torque limit times this, 375 N m for the
// humanoid of the library at the default limit of the motors, about what the muscles of a strong man's knee hold while
// they brake a knee that bends under the weight, up to half again as much as they push. At 225 N m the knee of the
// stance leg gave way: of the 1 920 hits on chest and belly from 50 to 89 N s 144 fell instead of 50. The figure braces
// them only while it catches itself, so it falls as limp as before.
#define NB_KNEE_BRACE ( 2.5f / 1.5f )

// No step starts that would leave the capture point at its landing more than this far outside the support, in meters
// for the humanoid of 1.8 m: that far no step catches the figure, and it falls at once, as the roadmap of the game, 26.4,
// wants it after an extreme hit. Below it a strong hit makes the figure stumble a step or two and fall when they do not
// catch it, the stumble before the full ragdoll of 26.4 and 28.3. The prediction of nbPlanFoot runs about twice as far
// as the capture point really goes in the first step, see the roadmap, F62, so the limit lies well beyond one step. Up
// to active-ragdoll-46 it was 1.2 m, and above what it caught the figure fell at once: on the upper chest from 115 N s
// from the sides, from 125 from the front and from 135 from behind. With 1.5 m it catches up to 130 instead of 120 N s
// from the front and up to 135 instead of 130 from behind, stumbles and falls at 115 to 125 N s from the sides, 135 from
// the front and 140 to 150 from behind, and falls at once from 130, 140 and 155 N s. Of the 2 064 hits on chest and
// belly from 90 to 139 N s 345, 341, 340 and 340 fell with 1.3, 1.4, 1.5 and 2 m instead of 362.
#define NB_GIVE_UP 1.5f

// The figure falls when its pelvis tilts farther than this from upright, in radians, or its center of mass sinks below
// this part of its height standing, in the first pose of its idle clip, see nbInitBalancer
#define NB_FALL_ANGLE 0.6f
#define NB_FALL_HEIGHT 0.75f

// Ground a foot stands on: the cosine of its steepest slope, 45 degrees
#define NB_WALKABLE 0.7f

// A foot that does not swing stands on the ground also when its sole hovers this little above it, in meters for the
// humanoid of 1.8 m, see nbMeasure
#define NB_NEAR_GROUND 0.03f

// A step reaches ground this much higher or lower than where the figure stands, in meters
#define NB_MAX_RISE 0.25f

// A figure without a foot on the ground for this long falls, in seconds
#define NB_AIRBORNE_TIME 0.3f

// A foot that stays off the ground this long while the other carries the figure steps down, in seconds. On one foot it
// would never stand calm, and with the capture point less than a step margin outside it would not step either.
#define NB_ONE_FOOT_TIME 0.3f

// The arms row against a fall, see nbRowArms: while a step swings that leaves the capture point at its landing more than
// this far outside the support, in meters for the humanoid of 1.8 m, and the capture point lies outside now, an upper arm
// that does not swing into the body swings out in the sense of the fall. By this many radians for each meter the step
// comes shorter than that, up to this many, never past the cone of the shoulder and not at all if less than this many
// are left, in this many seconds with the full strength of its motor, eased at both ends. The arms of the humanoid hang
// back from their rest, and the cone lets them swing back only 40 degrees: kept 0.3 rad short of it up to
// active-ragdoll-37, an arm swung back only 0.3 rad when the figure fell forward. Eased to the cone, the arm does not
// jolt, the armature of the shoulder calms it, see nbMakeHumanoidBones.
#define NB_ROW_SHORT 0.1f
#define NB_SWING_GAIN 6.5f
#define NB_SWING_ANGLE 1.3f
#define NB_SWING_LEAST 0.1f
#define NB_SWING_TIME 0.2f

// Out, the arm holds while the figure steps, and swings back to the pose of the clip in this many seconds, eased. While
// it holds it sinks back by up to this part of the way over a second, and it strokes about where it holds by this many
// radians this many times a second, the two arms against each other, faded in over this many seconds and out with the
// way back. Its elbow bends by up to this many radians with the swing. Between swings the arms hang at this part of the
// strength of the motors and swing with the body.
#define NB_SWING_BACK 0.6f
#define NB_SWING_DROOP 0.2f
#define NB_STROKE_ANGLE 0.2f
#define NB_STROKE_RATE 1.2f
#define NB_STROKE_FADE 0.3f
#define NB_SWING_ELBOW 0.6f
#define NB_LOOSE_ARMS 0.6f

// An arm that a body of another pushes or holds with at least this many newtons at a check goes slack, see nbHoldArms:
// its joints ask for this part of the strength of the motors, so a stick lifts it while the figure keeps its balance.
// It stays slack for this many seconds after the last check that found the push and then tenses again over this many
// seconds, eased. A stick of 150 N that swept on an arc about the shoulder under the forearm lifted the upper arm by
// 13 degrees at the 0.6 of NB_LOOSE_ARMS, by 75 at 0.15, by 78 at 0.1, by 46 at 0.05, where the forearm folds away at the
// elbow and the stick slips past it, and by 40 at 0.25.
#define NB_HOLD_FORCE 10.0f
#define NB_HOLD_TIME 0.3f
#define NB_HELD_ARMS 0.15f
#define NB_REGAIN_TIME 0.6f

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
// point between them or no farther out than a step needs, and the pelvis upright within this many radians, for this
// many seconds. Then the figure stands animated again. The fastest bone would not do: a hand that rests against a thigh
// in a wide stance jitters with 0.3 m/s for good. With the capture point only between the feet, a figure that the hold
// of its pelvis kept up stood still with it up to NB_STEP_MARGIN out, neither stepping nor calm, for good: after 97 N s
// from the front right on the chest 2.8 cm.
#define NB_CALM_SPEED 0.15f
#define NB_CALM_TILT 0.3f
#define NB_CALM_TIME 0.5f

// A figure that has caught itself for this long without coming to rest, and without anything pushing it, falls, in
// seconds. With a foot under a block of rubble a figure stepped with the other one again and again, every 0.7 s for as
// long as the block lay there. Of the 5 744 hits measured on active-ragdoll-35 the longest catch took 6.3 s.
#define NB_RESTLESS_TIME 8.0f

// A hold through a leg that does not carry the figure keeps this little torque, in newton meters: a joint of Box3D with
// none keeps the impulse it last had and pushes with it in every substep
#define NB_MIN_HOLD 0.5e-3f

// A foot that slides over the ground eases the hold through its leg from this speed on, in meters per second, and frees
// it this much faster, see nbHoldUpright. From 0.3 m/s on and free at 0.8 m/s, of the 1 920 hits on chest and belly from
// 50 to 89 N s 240 instead of 298 fell, from 0.2 and 0.4 m/s on 253 and 254, free 0.3 m/s faster or 0.7 m/s faster 246
// and 263.
#define NB_SLIDE_SPEED 0.3f
#define NB_SLIDE_RANGE 0.5f

// The spine bends the upper body on with the tilt of the pelvis by this many times the tilt, and at most this far, in
// radians, see nbBendTrunk. With the hold through the legs, of the 168 light hits on the upper chest 36 fell at half
// the gain, 31 at 1, 17 at 2 and 13 at 3, without the bend 43, but at 3 more of the hits from 90 to 139 N s fell than
// at 2, 1 481 of 2 064 against 1 456, and the catches took longer. Beyond 0.5 rad the bend changed little.
#define NB_TRUNK_GAIN 2.0f
#define NB_TRUNK_BEND 0.5f

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

// The legs and the upper arms of a figure: the humanoid of the library
static const int nb_thighs[2] = { nb_humanoidThighLeft, nb_humanoidThighRight };
static const int nb_shins[2] = { nb_humanoidShinLeft, nb_humanoidShinRight };
static const int nb_feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };
static const int nb_upperArms[2] = { nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight };

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

void nbInitBalancer( nbFigure* f, float standCenter )
{
	nbBalancer* b = &f->balancer;
	memset( b, 0, sizeof( nbBalancer ) );
	b->upright = b3_nullJointId;
	b->uprightLegs[0] = b3_nullJointId;
	b->uprightLegs[1] = b3_nullJointId;
	b->stepFoot = -1;
	b->scale = f->def.standHeight / NB_HUMANOID_STAND;
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
	b->standCenter = standCenter;
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

	// The angular momentum of the figure about its center of mass: each bone turning about its own center and moving
	// about the center of the figure
	b3Vec3 spin = b3Vec3_zero;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3BodyId body = f->bones[i].bodyId;
		b3Quat q = b3Body_GetRotation( body );
		b3Matrix3 inertia = b3Body_GetLocalRotationalInertia( body );
		b3Vec3 turn = b3RotateVector( q, b3MulMV( inertia, b3InvRotateVector( q, b3Body_GetAngularVelocity( body ) ) ) );
		b3Vec3 arm = b3Sub( b3ToVec3( b3Body_GetWorldCenter( body ) ), center );
		b3Vec3 relative = b3Sub( b3Body_GetLinearVelocity( body ), velocity );
		spin = b3Add( spin, b3MulAdd( turn, f->bones[i].mass, b3Cross( arm, relative ) ) );
	}
	b->spin = spin;

	// How far the pelvis tilts
	b3WorldTransform pelvis = b3Body_GetTransform( f->bones[0].bodyId );
	float upright = b3Dot( b3RotateVector( pelvis.q, b3Vec3_axisY ), up );
	b->tilt = nbAcos( upright );

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

	// A foot that does not swing stands also when its sole hovers just above the ground: after a long step the trailing
	// foot of the wide stance often carries nothing and loses its contacts now and then, and it would touch down as
	// soon as the body moved over it. Counted as off the ground, the support shrank to the other foot, the capture
	// point lay far outside it, and the trailing foot stepped away to the outside instead of standing.
	for ( int foot = 0; foot < 2; ++foot )
	{
		if ( b->footContact[foot] || ( b->stepping && foot == b->stepFoot ) )
		{
			continue;
		}
		b3WorldTransform transform = b3Body_GetTransform( f->bones[nb_feet[foot]].bodyId );
		float ground;
		if ( nbGroundBelow( world, index, b3MulAdd( b3ToVec3( transform.p ), 0.1f * b->scale, up ), up, 0.3f * b->scale,
							&ground ) == false )
		{
			continue;
		}
		float lowest = FLT_MAX;
		for ( int k = 0; k < 4; ++k )
		{
			lowest = b3MinFloat( lowest, b3Dot( b3ToVec3( b3TransformWorldPoint( transform, b->sole[foot][k] ) ), up ) );
		}
		if ( lowest - ground < NB_NEAR_GROUND * b->scale )
		{
			b->footContact[foot] = true;
			b->footGround[foot] = ground;
		}
	}

	// Where each foot faces: where the figure would face standing on it as at rest. The figure faces after its feet,
	// the stance foot while the other swings, else the feet on the ground together, and its pelvis only without a foot
	// on the ground or with feet that face nearly opposite ways. A hit that turns the pelvis so neither turns the steps
	// nor the planted feet with it, and the legs turn the pelvis back over the feet.
	for ( int foot = 0; foot < 2; ++foot )
	{
		b3Quat rotation = b3Body_GetRotation( f->bones[nb_feet[foot]].bodyId );
		b3Vec3 facing = nbFlat( b3RotateVector( b3MulQuat( rotation, b3Conjugate( b->footRest[foot] ) ), b3Vec3_axisZ ), up );
		if ( b3Length( facing ) > 0.5f )
		{
			b->footFacing[foot] = b3Normalize( facing );
		}
	}
	b3Vec3 facing = b3Vec3_zero;
	if ( b->stepping && b->footContact[1 - b->stepFoot] )
	{
		facing = b->footFacing[1 - b->stepFoot];
	}
	else
	{
		for ( int foot = 0; foot < 2; ++foot )
		{
			facing = b->footContact[foot] ? b3Add( facing, b->footFacing[foot] ) : facing;
		}
	}
	if ( b3Length( facing ) < 0.5f )
	{
		facing = nbFlat( b3RotateVector( pelvis.q, b3Vec3_axisZ ), up );
	}
	if ( b3Length( facing ) > 0.5f )
	{
		b->forward = b3Normalize( facing );
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

	// The capture point: the center of mass ahead by its velocity over the frequency of a pendulum as long as its
	// height. A figure that turns has to stop turning as well, and the ground under its feet takes the turn out with a
	// force along the ground below the center of mass, which pushes the center of mass the way the turn goes: a turn of
	// angular momentum L takes a push of L over the height, so it counts as a velocity of L x up over mass and height.
	// 60 N s from the front on the upper chest turn the humanoid back with 25 N m s, which count as 0.35 m/s on top of
	// the 0.81 m/s of its center of mass. The hold against the world takes out part of the turn as the figure goes, and
	// the capture point follows. Without the turn the figure stepped shorter, see the roadmap, F61.
	float height = b3MaxFloat( b3Dot( center, up ) - b->groundHeight, 0.05f );
	float omega = sqrtf( b3MaxFloat( b->gravity, 0.01f ) / height );
	b3Vec3 drift = b3MulAdd( nbFlat( velocity, up ), 1.0f / ( mass * height ), b3Cross( spin, up ) );
	b3Vec3 capture = nbAtHeight( b3MulAdd( center, 1.0f / omega, drift ), up, b->groundHeight );
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
// transform, the sole flat on the ground facing a direction and the knee bending toward it. IK places, it does not
// decide, see the roadmap 28.3.
static void nbLegIK( const nbFigure* f, const nbBalancer* b, int leg, b3WorldTransform pelvis, b3Vec3 ankle, b3Vec3 facing,
					 b3Quat* pose )
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

	// The knee bends toward where the foot faces, across the line from hip to ankle
	b3Vec3 front = b3MulSub( facing, b3Dot( facing, along ), along );
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

	// The sole flat, the foot facing its way
	b3Quat footWorld = b3MulQuat( nbYaw( up, facing ), b->footRest[leg] );
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
	b3Vec3 facing = b->footFacing[stance];
	b3Vec3 left = b3Cross( up, facing );
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
	away = b3Length( away ) > 1.0e-3f ? b3Normalize( away ) : facing;
	b3Vec3 soleTarget = b3MulAdd( landing, NB_STEP_OVERSHOOT * b->scale, away );

	// The ankle above the middle of its sole, the foot facing the way the stance foot faces
	b3Quat footWorld = b3MulQuat( nbYaw( up, facing ), b->footRest[foot] );
	b3Vec3 ankle = b3Sub( soleTarget, nbFlat( b3RotateVector( footWorld, b->soleCenter[foot] ), up ) );

	// The feet stay apart sideways to the stance foot, the left one left of the other, no crossed legs
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

// A step with the foot that leaves the capture point nearest to the support, also one that comes short, or false if no
// foot can step or the best step comes hopelessly short
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

	if ( bestFoot < 0 || bestResidual > NB_GIVE_UP * b->scale )
	{
		return false;
	}

	b->lastFoot = bestFoot;
	b->stepping = true;
	b->stateTime = 0.0f;
	b->stepFoot = bestFoot;
	b->stepCount += 1;
	b->stepAnkle = bestTarget;
	b->stepResidual = bestResidual;
	b->swingStart = nbAnkle( f, bestFoot );
	b->dangerUpdates = 0;
	b->oneFootTime = 0.0f;
	f->stepCount += 1;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
// The figure catches itself

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

// The hold through the legs: the legs that carry the figure share it, the stance leg alone while the other swings, both
// on the ground alike, none without ground under them. Each joint holds the up of the world as it lies in the frame of
// its thigh in this update, so the pelvis turns upright against the thigh, and the thigh pushes against the ground.
// Where the ground cannot hold the foot, the thigh pushes it away under the body: after 74 N s from the front and left
// the stance foot slid 36 cm forward in a tenth of a second while the figure fell back. A person whose foot slides eases
// off the hip, so the hold through a leg eases while its foot slides faster than NB_SLIDE_SPEED and lets go once it
// slides NB_SLIDE_RANGE faster still. The hold through a leg is the stance hip of a person, so it pulls as hard as the
// joint of the thigh may, its torque limit, 300 N m for the humanoid of the library: with 125 N m, uprightTorque, which
// bounded it up to active-ragdoll-48, 74 of the 1 920 hits on chest and belly from 50 to 89 N s fell, with 300 N m 50.
static void nbHoldUpright( const nbFigure* f, const nbBalancer* b )
{
	if ( b3Joint_IsValid( b->uprightLegs[0] ) == false )
	{
		return;
	}

	const nbFigureMotors* motors = &f->def.motors;
	float share[2] = { 0.0f, 0.0f };
	if ( b->stepping )
	{
		int stance = 1 - b->stepFoot;
		share[stance] = b->footContact[stance] ? 1.0f : 0.0f;
	}
	else
	{
		int count = ( b->footContact[0] ? 1 : 0 ) + ( b->footContact[1] ? 1 : 0 );
		for ( int leg = 0; leg < 2 && count > 0; ++leg )
		{
			share[leg] = b->footContact[leg] ? 1.0f / (float)count : 0.0f;
		}
	}

	for ( int leg = 0; leg < 2; ++leg )
	{
		if ( share[leg] > 0.0f )
		{
			float slide = b3Length( nbFlat( b3Body_GetLinearVelocity( f->bones[nb_feet[leg]].bodyId ), b->up ) );
			share[leg] *= b3ClampFloat( 1.0f - ( slide - NB_SLIDE_SPEED ) / NB_SLIDE_RANGE, 0.0f, 1.0f );
		}
	}

	b3BodyId root = f->bones[0].bodyId;
	b3Quat frameB = b3MulQuat( b3Body_GetRotation( root ), nbQuatZTo( b3Vec3_axisY ) );
	for ( int leg = 0; leg < 2; ++leg )
	{
		b3JointId joint = b->uprightLegs[leg];
		b3BodyId thigh = f->bones[nb_thighs[leg]].bodyId;
		b3Quat upInThigh = b3InvMulQuat( b3Body_GetRotation( thigh ), nbQuatZTo( b->up ) );
		b3Joint_SetLocalFrameA( joint, (b3Transform){ b3Vec3_zero, upInThigh } );
		float stiffness = b3MaxFloat( share[leg] * motors->uprightStiffness, 1.0f );
		b3ParallelJoint_SetSpringHertz( joint, nbJointSpringHertz( thigh, root, frameB, false, stiffness, 0.0f ) );
		const nbBone* hip = f->bones + nb_thighs[leg];
		float hipTorque = hip->activation * motors->maxTorque * hip->strength;
		b3ParallelJoint_SetMaxTorque( joint, b3MaxFloat( share[leg] * hipTorque, NB_MIN_HOLD ) );
	}
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
	b->footFacing[0] = b->forward;
	b->footFacing[1] = b->forward;
	b->stepping = false;
	b->stepFoot = -1;

	nbMeasure( world, index, f, b );
	for ( int leg = 0; leg < 2; ++leg )
	{
		b->plant[leg] = nbPlant( f, b, leg );
	}
	b->stateTime = 0.0f;
	b->stepCount = 0;
	b->stepResidual = 0.0f;
	b->dangerUpdates = 0;
	b->airborneTime = 0.0f;
	b->oneFootTime = 0.0f;
	b->calmTime = 0.0f;
	b->restlessTime = 0.0f;

	// The arms hang loose until they row, with their forearms and hands, which follow the upper arm among the bones, if
	// the motors pull at all. An arm that the push which woke the figure pressed on goes slack at once, see nbHoldArms.
	for ( int k = 0; k < 2; ++k )
	{
		b->armSwing[k] = nb_armHangs;
		b->armTime[k] = 0.0f;
		b->armHold[k] = 0.0f;
		b->armFrom[k] = b3Quat_identity;
		b->armAxis[k] = b3Vec3_zero;
		b->armAngle[k] = 0.0f;
		f->armPush[k] = f->pushed ? f->armPush[k] : 0.0f;
		b->armHeld[k] = f->armPush[k] >= NB_HOLD_FORCE ? NB_HOLD_TIME : 0.0f;
		b->armSlack[k] = b->armHeld[k] > 0.0f ? 1.0f : 0.0f;
		for ( int i = nb_upperArms[k]; i <= nb_upperArms[k] + 2; ++i )
		{
			nbSetBoneSpring( f, i, b->armSlack[k] > 0.0f ? NB_HELD_ARMS : NB_LOOSE_ARMS, f->springs );
		}
	}

	// The upright hold: parallel joints keep the up axis of the pelvis along the up of the world with a bounded torque,
	// against the thighs of the legs that carry the figure or against the static anchor of the world, see
	// nbFigureMotors::uprightAgainstWorld. Without the one against the world the figure catches itself with its own
	// strength and braces its knees, see NB_KNEE_BRACE.
	const nbFigureMotors* motors = &f->def.motors;
	b->ownStrength = ( motors->uprightTorque > 0.0f && motors->uprightStiffness > 0.0f && motors->uprightAgainstWorld ) == false;
	for ( int leg = 0; leg < 2; ++leg )
	{
		f->bones[nb_shins[leg]].brace = b->ownStrength ? NB_KNEE_BRACE : 1.0f;
	}
	if ( motors->uprightTorque > 0.0f && motors->uprightStiffness > 0.0f && motors->uprightAgainstWorld == false )
	{
		// Between the pelvis and each thigh, the up of the world held in the frame of the thigh, see nbHoldUpright
		b3BodyId root = f->bones[0].bodyId;
		for ( int leg = 0; leg < 2; ++leg )
		{
			b3BodyId thigh = f->bones[nb_thighs[leg]].bodyId;
			b3Quat upInThigh = b3InvMulQuat( b3Body_GetRotation( thigh ), nbQuatZTo( b->up ) );
			b3ParallelJointDef def = b3DefaultParallelJointDef();
			def.base.bodyIdA = thigh;
			def.base.bodyIdB = root;
			def.base.localFrameA = (b3Transform){ b3Vec3_zero, upInThigh };
			def.base.localFrameB = (b3Transform){ b3Vec3_zero, nbQuatZTo( b3Vec3_axisY ) };
			def.base.collideConnected = false;
			def.dampingRatio = b3MaxFloat( 0.0f, motors->uprightDampingRatio );
			def.maxTorque = NB_MIN_HOLD;
			b->uprightLegs[leg] = b3CreateParallelJoint( world->physicsWorld, &def );
		}
		nbHoldUpright( f, b );
	}
	else if ( motors->uprightTorque > 0.0f && motors->uprightStiffness > 0.0f )
	{
		b3BodyId anchor = nbFigureAnchor( world );
		b3BodyId root = f->bones[0].bodyId;
		b3ParallelJointDef def = b3DefaultParallelJointDef();
		def.base.bodyIdA = anchor;
		def.base.bodyIdB = root;
		b3Vec3 upInAnchor = b3InvRotateVector( b3Body_GetRotation( anchor ), b->up );
		def.base.localFrameA = (b3Transform){ b3Vec3_zero, nbQuatZTo( upInAnchor ) };
		def.base.localFrameB = (b3Transform){ b3Vec3_zero, nbQuatZTo( b3Vec3_axisY ) };
		def.base.collideConnected = false;
		b3Quat frameB = b3MulQuat( pelvis, def.base.localFrameB.q );
		def.hertz = nbJointSpringHertz( anchor, root, frameB, false, motors->uprightStiffness, 0.0f );
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
	for ( int leg = 0; leg < 2; ++leg )
	{
		if ( b3Joint_IsValid( b->uprightLegs[leg] ) )
		{
			b3DestroyJoint( b->uprightLegs[leg], true );
		}
		b->uprightLegs[leg] = b3_nullJointId;
	}
	b->stepping = false;
	b->stepFoot = -1;
	for ( int leg = 0; leg < 2; ++leg )
	{
		f->bones[nb_shins[leg]].brace = 1.0f;
	}
}

// The legs on the ground carry the body above them, stage 2 of the balance from the figure's own strength, see the
// roadmap, F61, after the virtual forces of Coros (2010). The springs of the legs are soft, 800 N m per radian on their
// softest axis: a knee holds the 200 N m of a bent leg only 0.25 rad away from its target, so a leg sank that far before
// it carried the body. A leg on the ground now pushes the body above the legs toward the center of that body, as hard as
// it carries its weight, from where the ground pushes its foot: the point of the sole under that center, straight up
// while the center lies above the sole, and from the edge of the sole once it lies beyond, so the ankle holds no more
// than the sole lets it. Its joints, hip, knee and ankle, pull toward the pose of the IK turned by as much as their
// springs need to push with the torques of that push and of the weight of the bones of the leg above them. The stance
// leg carries all of the body while the other swings. Two legs on the ground share it by where its center lies between
// their ankles. A spring pushes with its rotational mass, see nbJointSpringHertz, times the square of its frequency, and
// it keeps its limit, so no joint pulls harder than its muscles.
static void nbCarryBody( const nbFigure* f, const nbBalancer* b, b3Quat* pose )
{
	float share[2] = { 0.0f, 0.0f };
	b3Vec3 ankles[2] = { nbAnkle( f, 0 ), nbAnkle( f, 1 ) };
	if ( b->stepping )
	{
		int stance = 1 - b->stepFoot;
		share[stance] = b->footContact[stance] ? 1.0f : 0.0f;
	}
	else if ( b->footContact[0] && b->footContact[1] )
	{
		b3Vec3 across = nbFlat( b3Sub( ankles[1], ankles[0] ), b->up );
		float length2 = b3Dot( across, across );
		float right = 0.5f;
		if ( length2 > 1.0e-6f )
		{
			right = b3ClampFloat( b3Dot( nbFlat( b3Sub( b->center, ankles[0] ), b->up ), across ) / length2, 0.0f, 1.0f );
		}
		share[0] = 1.0f - right;
		share[1] = right;
	}
	else
	{
		share[0] = b->footContact[0] ? 1.0f : 0.0f;
		share[1] = b->footContact[1] ? 1.0f : 0.0f;
	}

	// The body above the legs on the ground: the pelvis, the trunk, the arms, the head and a leg that swings
	float mass = 0.0f;
	b3Vec3 moment = b3Vec3_zero;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		bool carrying = false;
		for ( int leg = 0; leg < 2; ++leg )
		{
			bool inLeg = i == nb_thighs[leg] || i == nb_shins[leg] || i == nb_feet[leg];
			carrying = carrying || ( share[leg] > 0.0f && inLeg );
		}
		if ( carrying == false )
		{
			mass += f->bones[i].mass;
			moment = b3MulAdd( moment, f->bones[i].mass, b3ToVec3( b3Body_GetWorldCenter( f->bones[i].bodyId ) ) );
		}
	}
	if ( mass <= 0.0f )
	{
		return;
	}
	b3Vec3 body = b3MulSV( 1.0f / mass, moment );

	for ( int leg = 0; leg < 2; ++leg )
	{
		if ( share[leg] <= 0.0f )
		{
			continue;
		}

		// Where the ground pushes the foot: the point of the sole under the center of the body, at most the edge of the
		// sole, whose corners lie in the frame of the foot
		b3BodyId foot = f->bones[nb_feet[leg]].bodyId;
		b3Vec3 low = b->sole[leg][0];
		b3Vec3 high = b->sole[leg][0];
		for ( int k = 1; k < 4; ++k )
		{
			low = b3Min( low, b->sole[leg][k] );
			high = b3Max( high, b->sole[leg][k] );
		}
		b3Vec3 local = b3Max( low, b3Min( high, b3Body_GetLocalPoint( foot, b3ToPos( body ) ) ) );
		b3Vec3 pressure = b3ToVec3( b3Body_GetWorldPoint( foot, local ) );

		// The push of the leg from there toward the center of the body, as high as its share of the weight
		b3Vec3 along = b3Sub( body, pressure );
		float height = b3Dot( along, b->up );
		if ( height < 0.1f * b->scale )
		{
			continue;
		}
		b3Vec3 push = b3MulSV( share[leg] * mass * b->gravity / height, along );

		const int chain[3] = { nb_thighs[leg], nb_shins[leg], nb_feet[leg] };
		for ( int k = 0; k < 3; ++k )
		{
			const nbBone* bone = f->bones + chain[k];
			const nbBone* parent = f->bones + bone->parent;
			float omega = 2.0f * B3_PI * bone->springHertz;
			if ( omega <= 0.0f )
			{
				continue;
			}

			// The torque on the bone above the joint: the push on the body and the weight of the bones of the leg above
			// the joint, the thigh above the knee, the thigh and the shin above the ankle
			b3Vec3 pivot = b3ToVec3( b3Body_GetPosition( bone->bodyId ) );
			b3Vec3 torque = b3Cross( b3Sub( body, pivot ), push );
			for ( int j = 0; j < k; ++j )
			{
				const nbBone* above = f->bones + chain[j];
				b3Vec3 weight = b3MulSV( above->mass * b->gravity, b->up );
				torque = b3Add( torque, b3Cross( b3Sub( b3ToVec3( b3Body_GetWorldCenter( above->bodyId ) ), pivot ), weight ) );
			}

			// The turn of the target against the bone above as it is now that makes the spring push with that torque
			b3Matrix3 invI = b3AddMM( b3Body_GetWorldInverseRotationalInertia( parent->bodyId ),
									  b3Body_GetWorldInverseRotationalInertia( bone->bodyId ) );
			b3Vec3 turn;
			if ( bone->joint == nb_hingeJoint )
			{
				b3Vec3 axis =
					b3RotateVector( b3Body_GetRotation( bone->bodyId ), b3RotateVector( bone->jointFrame, b3Vec3_axisZ ) );
				turn = b3MulSV( -b3Dot( torque, axis ) * b3Dot( axis, b3MulMV( invI, axis ) ) / ( omega * omega ), axis );
			}
			else
			{
				turn = b3MulSV( -1.0f / ( omega * omega ), b3MulMV( invI, torque ) );
			}

			b3Quat frame = b3MulQuat( b3Body_GetRotation( parent->bodyId ), bone->rest.q );
			b3Vec3 local = b3InvRotateVector( frame, turn );
			float angle = b3Length( local );
			if ( angle > 1.0e-6f )
			{
				b3Quat shift = b3MakeQuatFromAxisAngle( b3MulSV( 1.0f / angle, local ), angle );
				pose[chain[k]] = b3NormalizeQuat( b3MulQuat( shift, pose[chain[k]] ) );
			}
		}
	}
}

// The upper body takes up the turn of a fall, stage 3 of the balance from the figure's own strength, see the roadmap,
// F61, after the hip strategy of Horak and Nashner (1986): pushed harder than the ankles hold, a person bends at the
// hips and turns the upper body on with the fall, and the legs and the pelvis turn back the other way. The spine bends
// the upper body on in the sense in which the pelvis tilts, by NB_TRUNK_GAIN times the tilt and at most NB_TRUNK_BEND,
// spread evenly over its joints. The torque that turns the upper body on turns the pelvis back, so the pelvis stays
// more upright, the stance leg stands straighter under it, and the figure tips less over the edge of its stance foot.
// As the pelvis rights itself the upper body straightens with it. Held upright against the tilt instead, the upper
// body pushed the pelvis farther over: with the hold through the legs 82 of the 168 light hits fell instead of 43.
static void nbBendTrunk( const nbFigure* f, const nbBalancer* b, b3Quat* pose )
{
	// The axis about which the pelvis tilts away from up
	b3Vec3 axis = b3Cross( b->up, b3RotateVector( b3Body_GetRotation( f->bones[0].bodyId ), b3Vec3_axisY ) );
	float s = b3Length( axis );
	if ( s < 1.0e-4f )
	{
		return;
	}

	// Every joint of the spine turns its bone by its share of the bend about that axis, in the frame of its parent as
	// the parent stands
	float bend = b3MinFloat( NB_TRUNK_GAIN * b->tilt, NB_TRUNK_BEND );
	b3Vec3 turn = b3MulSV( bend / ( (float)NB_SPINE_COUNT * s ), axis );
	for ( int i = nb_humanoidBelly; i <= nb_humanoidChest; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Quat frame = b3MulQuat( b3Body_GetRotation( f->bones[bone->parent].bodyId ), bone->rest.q );
		b3Vec3 local = b3InvRotateVector( frame, turn );
		float angle = b3Length( local );
		if ( angle > 1.0e-6f )
		{
			pose[i] = b3NormalizeQuat( b3MulQuat( b3MakeQuatFromAxisAngle( b3MulSV( 1.0f / angle, local ), angle ), pose[i] ) );
		}
	}
}

// The arms row against a fall, after the idea of Hof (2007), the equations of motion for a standing human: besides moving
// the center of pressure under the feet and stepping, a body keeps its balance by turning parts of itself about its
// center of mass. An upper arm that the motor of its shoulder turns in the sense of the fall, about up across the
// direction of the fall, turns the body back the other way. Through the feet on the ground that pushes the center of
// mass back, and the capture point runs away more slowly, as if the support reached farther. An arm can only turn as far
// as its shoulder lets it, and when it stops the turn goes back into the body. So the arms swing only while a step comes
// clearly short, the farther the shorter it comes, and hold out while the figure steps. The arm on the side of a fall to
// the side would swing into the body and stays.
// A swing is planned when it starts, so the arm moves like an arm and not like a motor that jumps to its limit: out along
// an eased ramp with the full strength of its motor, the elbow bending with it, then loose again, held out while the
// figure steps, sinking a little and stroking slowly, the two arms against each other, and back along an eased ramp to
// the pose of the clip, the elbow straightening. Pulled toward a target far ahead, as before, an arm turned more than
// twice as fast and braked three times as hard.

// From 0 to 1 and level at both ends
static float nbEase( float x )
{
	x = b3ClampFloat( x, 0.0f, 1.0f );
	return x * x * ( 3.0f - 2.0f * x );
}

// An arm that a body of another pushes or holds goes slack at once, as long as the push lasts and a little after, and
// then tenses again over the regain time, see NB_HOLD_FORCE. So a stick lifts the arm of a figure that catches itself
// as a person lets someone lift their arm, and the push goes into the arm instead of the body, which keeps its balance.
// The push is the strongest on a bone of the arm at the last check, see nbFigure::armPush.
static void nbHoldArms( const nbFigure* f, nbBalancer* b, float timeStep )
{
	for ( int k = 0; k < 2; ++k )
	{
		b->armHeld[k] = f->armPush[k] >= NB_HOLD_FORCE ? NB_HOLD_TIME : b3MaxFloat( b->armHeld[k] - timeStep, 0.0f );
		b->armSlack[k] = b->armHeld[k] > 0.0f ? 1.0f : b3MaxFloat( b->armSlack[k] - timeStep / NB_REGAIN_TIME, 0.0f );
	}
}

// The strength the joints of an arm ask for: the upper arm the full strength of its motor while it swings out, the arm
// the loose strength otherwise, less as far as it is slack, eased
static void nbDriveArm( nbFigure* f, const nbBalancer* b, int k )
{
	float slack = nbEase( b->armSlack[k] );
	for ( int i = nb_upperArms[k]; i <= nb_upperArms[k] + 2; ++i )
	{
		float strength = i == nb_upperArms[k] && b->armSwing[k] == nb_armSwingsOut ? 1.0f : NB_LOOSE_ARMS;
		nbSetBoneSpring( f, i, strength + ( NB_HELD_ARMS - strength ) * slack, f->springs );
	}
}

static void nbRowArms( nbFigure* f, nbBalancer* b, float timeStep, b3Quat* pose )
{
	// The sense of the fall: up turned toward where the capture point lies from the middle of the support
	b3Vec3 middle = b->supportCount > 0 ? b->supportMiddle : b3MulSV( 0.5f, b3Add( b->plant[0], b->plant[1] ) );
	b3Vec3 axis = b3Normalize( b3Cross( b->up, nbFlat( b3Sub( b->capture, middle ), b->up ) ) );
	bool row = b->stepping && b->stepResidual > NB_ROW_SHORT * b->scale && b->captureMargin > 0.0f &&
			   b3LengthSquared( axis ) > 0.0f;

	b3Quat chest = b3Body_GetRotation( f->bones[nb_humanoidChest].bodyId );
	b3Vec3 chestPoint = b3ToVec3( b3Body_GetPosition( f->bones[nb_humanoidChest].bodyId ) );
	for ( int k = 0; k < 2; ++k )
	{
		int arm = nb_upperArms[k];
		const nbBone* bone = f->bones + arm;
		b3Quat frame = b3MulQuat( chest, bone->rest.q );

		// A hanging arm swings from where it is, not where its motion would point into the body, and not past the cone
		// of the shoulder when the swing takes it farther from its rest. One that something holds stays where it is
		// held.
		if ( row && b->armSwing[k] == nb_armHangs && b->armSlack[k] == 0.0f )
		{
			b3Quat current = b3Body_GetRotation( bone->bodyId );
			b3Vec3 along = b3RotateVector( current, b3Vec3_axisY );
			b3Vec3 motion = b3Cross( axis, along );
			b3Vec3 shoulder = b3ToVec3( b3Body_GetPosition( bone->bodyId ) );
			b3Vec3 outward = b3Normalize( nbFlat( b3Sub( shoulder, chestPoint ), b->up ) );
			if ( b3Dot( motion, outward ) >= -0.5f * b3Length( motion ) )
			{
				b3Vec3 restAlong = b3RotateVector( frame, b3Vec3_axisY );
				float angle = NB_SWING_GAIN * ( b->stepResidual - NB_ROW_SHORT * b->scale ) / b->scale;
				angle = b3MinFloat( NB_SWING_ANGLE, angle );
				if ( b3Dot( motion, restAlong ) < 0.0f )
				{
					// The limit of the shoulder toward where the arm moves, the swing about the axis from its rest
					// direction to there in the joint frame
					float swung = nbAcos( b3Dot( along, restAlong ) );
					b3Vec3 toward = b3Normalize( b3Cross( restAlong, b3Add( along, motion ) ) );
					b3Vec3 local = b3InvRotateVector( b3MulQuat( frame, bone->jointFrame ), toward );
					float flat = sqrtf( local.x * local.x + local.y * local.y );
					float cone = flat > 0.0f ? nbSwingLimit( bone, local.x / flat, local.y / flat )
											 : nbSwingLimit( bone, 1.0f, 0.0f );
					angle = b3MinFloat( angle, cone - swung );
				}
				if ( angle > NB_SWING_LEAST )
				{
					b->armSwing[k] = nb_armSwingsOut;
					b->armTime[k] = 0.0f;
					b->armHold[k] = 0.0f;
					b->armFrom[k] = b3NormalizeQuat( b3InvMulQuat( frame, current ) );
					b->armAxis[k] = b3InvRotateVector( frame, axis );
					b->armAngle[k] = angle;
				}
			}
		}

		if ( b->armSwing[k] == nb_armSwingsOut )
		{
			// Out along the ramp, the elbow bending with it, the forearm and the hand loose
			b->armTime[k] += timeStep;
			float out = b->armTime[k] / NB_SWING_TIME;
			float eased = nbEase( out );
			b3Quat turn = b3MakeQuatFromAxisAngle( b->armAxis[k], b->armAngle[k] * eased );
			pose[arm] = b3NormalizeQuat( b3MulQuat( turn, b->armFrom[k] ) );
			pose[arm + 1] = b3MulQuat( b3MakeQuatFromAxisAngle( b3Vec3_axisX, NB_SWING_ELBOW * eased ), pose[arm + 1] );
			if ( out >= 1.0f )
			{
				b->armSwing[k] = nb_armSwingsBack;
				b->armTime[k] = 0.0f;
				b->armFrom[k] = pose[arm];
			}
		}
		else if ( b->armSwing[k] == nb_armSwingsBack )
		{
			// Back along the ramp toward the clip once the figure stands, sinking toward it while it holds
			b->armTime[k] += b->stepping ? 0.0f : timeStep;
			b->armHold[k] += timeStep;
			float way = b->armTime[k] / NB_SWING_BACK;
			float back = b3MaxFloat( nbEase( way ), NB_SWING_DROOP * b3MinFloat( b->armHold[k], 1.0f ) );
			b3Quat target = nbHalfwayAt( b->armFrom[k], pose[arm], back );
			pose[arm + 1] = b3MulQuat( b3MakeQuatFromAxisAngle( b3Vec3_axisX, NB_SWING_ELBOW * ( 1.0f - back ) ), pose[arm + 1] );

			// Strokes about where it holds, the right arm against the left, faded in and faded out with the way back
			float fade = b3MinFloat( b->armHold[k] / NB_STROKE_FADE, 1.0f ) * ( 1.0f - back );
			float stroke = NB_STROKE_ANGLE * fade * b3ComputeCosSin( 2.0f * B3_PI * NB_STROKE_RATE * b->armHold[k] ).sine;
			stroke = k == 0 ? stroke : -stroke;
			pose[arm] = b3MulQuat( b3MakeQuatFromAxisAngle( b->armAxis[k], stroke ), target );
			if ( way >= 1.0f )
			{
				b->armSwing[k] = nb_armHangs;
			}
		}
		nbDriveArm( f, b, k );
	}
}

nbBalanceResult nbUpdateBalance( nbWorld* world, int index, nbFigure* f, float timeStep, b3Quat* pose )
{
	nbBalancer* b = &f->balancer;
	nbMeasure( world, index, f, b );
	nbHoldArms( f, b, timeStep );
	b->stateTime += timeStep;

	// Decide, see the roadmap 26.14, 26.15 and 28.3
	bool fallen = b->tilt > NB_FALL_ANGLE || nbCenterHeight( b ) < NB_FALL_HEIGHT * b->standCenter;
	if ( fallen )
	{
		return nb_balanceFell;
	}

	// No rest in sight, see NB_RESTLESS_TIME. Something that pushes the figure or holds an arm keeps it up as long as it
	// pushes.
	bool held = b->armHeld[0] > 0.0f || b->armHeld[1] > 0.0f;
	b->restlessTime = f->pushed || held ? 0.0f : b->restlessTime + timeStep;
	if ( b->restlessTime > NB_RESTLESS_TIME )
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

		// A foot that stays off the ground while the other carries steps down, as if the capture point lay outside
		b->dangerUpdates = grounded && b->captureMargin > NB_STEP_MARGIN * b->scale ? b->dangerUpdates + 1 : 0;
		b->oneFootTime = b->footContact[0] != b->footContact[1] ? b->oneFootTime + timeStep : 0.0f;
		if ( b->oneFootTime >= NB_ONE_FOOT_TIME )
		{
			b->dangerUpdates = NB_DANGER_UPDATES;
		}
		bool urgent = b->ownStrength && b->dangerUpdates >= 1 && b->captureMargin > NB_STEP_URGENT * b->scale;
		if ( ( b->dangerUpdates >= NB_DANGER_UPDATES || urgent ) && nbStartCatchStep( world, index, f, b ) == false )
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
				b->stepResidual = residual;
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
			else if ( b->stepCount >= ( b->ownStrength ? NB_OWN_MAX_STEPS : NB_MAX_STEPS ) ||
					  nbStartCatchStep( world, index, f, b ) == false )
			{
				return nb_balanceFell;
			}
		}
	}

	// Calm on its feet it stands animated again, once its arms hold again
	bool calm = b->stepping == false && b->footContact[0] && b->footContact[1] && b->captureMargin <= NB_STEP_MARGIN * b->scale &&
				b->meanSpeed < NB_CALM_SPEED && b->tilt < NB_CALM_TILT && f->pushed == false && b->armSlack[0] == 0.0f &&
				b->armSlack[1] == 0.0f;
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
			nbLegIK( f, b, leg, wanted, b->plant[leg], b->footFacing[leg], pose );
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
		nbLegIK( f, b, foot, swingPelvis, swing, b->footFacing[stance], pose );
		nbLegIK( f, b, stance, wanted, b->plant[stance], b->footFacing[stance], pose );
	}

	nbCarryBody( f, b, pose );
	nbBendTrunk( f, b, pose );
	nbHoldUpright( f, b );
	nbRowArms( f, b, timeStep, pose );
	return nb_balanceGoesOn;
}

bool nbFootOnGround( const nbWorld* world, int index, const nbFigure* f, int foot, b3Vec3 up, float* height )
{
	return nbFootContact( world, index, f->bones[nb_feet[foot]].bodyId, up, height );
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
