// SPDX-License-Identifier: MIT

// A figure that a program steers, see nbFigure_Steer, as a character controller after chapters 2 and 25.1 of the roadmap
// of the game. The figure moves as a capsule over the point of its frame, from its step height over the ground to the
// top of its head. In every update while it stands it gets to the velocity the program wants with its acceleration, a
// probe under the capsule finds the ground, and the capsule slides along what it touches: the mover of Box3D gathers the
// planes it touches, solves where it can go between them and sweeps it there, a few times, and its velocity loses what
// points into them, after the character mover of Box2D. Ground that is flat enough holds the frame like a critically
// damped spring, so the figure walks up steps and over rubble lower than the step without the capsule touching them, and
// keeps the ground going down. Steeper ground stops it as a wall, after the floors and walls of move_and_slide in Godot.
// Without ground it falls with gravity, steering a little, and in the air too long it falls as a ragdoll. Its feet step
// after the capsule in figure.c. The decisions take fixed thresholds, the planes come in a fixed order and the angles and
// decays come from functions with the same bits everywhere, so the same wishes give the same figure on every machine.

#include "figure.h"
#include "world.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

// The probe is a sphere of this share of the radius of the capsule, cast down from its lower center: smaller than the
// capsule, so it does not catch the walls the capsule slides along
#define NB_STEER_PROBE 0.5f

// Where the probe touches something too steep, rays look for the ground down the middle and this share of the radius of
// the capsule around it
#define NB_STEER_AROUND 0.6f

// The probe reaches this far below the ground a figure stands on, as a share of its step height, so it keeps the ground
// down steps as high as it walks up, and in the air this far in meters for the humanoid of 1.8 m
#define NB_STEER_SNAP 1.0f
#define NB_STEER_AIR_REACH 0.02f

// The spring that holds the frame on the ground, in cycles per second, critically damped: up a step of 15 cm in about a
// tenth of a second
#define NB_STEER_HERTZ 5.0f

// A figure in the air longer than a fall from this high takes, in meters for the humanoid of 1.8 m, falls as a ragdoll
#define NB_STEER_FALL_HEIGHT 1.8f

// The step height as a share of the stand height, where the steering gives none
#define NB_STEER_STEP_SHARE 0.4f

// Sweeps of the capsule per update at most, the planes it gathers at most per sweep, and how short a sweep ends them, in
// meters
#define NB_STEER_SWEEPS 4
#define NB_STEER_PLANES 32
#define NB_STEER_SETTLED 1.0e-4f

// A plane whose normal points down more than this, its cosine to up, is a ceiling, not a wall
#define NB_STEER_CEILING 0.1f

// The slope of its way, rise over run, follows the way over this many meters for the humanoid of 1.8 m, at most this
// steep up or down
#define NB_STEER_SLOPE_LENGTH 0.5f
#define NB_STEER_SLOPE_MAX 1.0f

// Facing where it moves, see nbSteer::faceVelocity, a figure goes across its heading no faster than NB_STEER_SIDE_SPEED in
// meters per second for the humanoid of 1.8 m or NB_STEER_SIDE_SHARE of the speed it wants, whichever is more, and
// backward no faster than NB_STEER_BACK_SPEED: it turns to where it wants to go and gets there as it faces it. Going off
// to the side faster than its heading turned, the capsule left the feet behind it, across each other.
#define NB_STEER_SIDE_SPEED 0.3f
#define NB_STEER_SIDE_SHARE 0.17f
#define NB_STEER_BACK_SPEED 0.3f

// The planes the capsule touches, with what orders them
typedef struct nbMoverPlane
{
	b3CollisionPlane plane;
	int shape;
	int child;
	int triangle;
	int order;
} nbMoverPlane;

typedef struct nbMoverQuery
{
	const nbWorld* world;
	int figure;
	b3Vec3 up;
	float flatEnough;
	bool walls;
	nbMoverPlane planes[NB_STEER_PLANES];
	int count;
} nbMoverQuery;

// What the capsule of a figure leaves out: its own bones and sensors
static bool nbMoverSkips( const nbMoverQuery* query, b3ShapeId shapeId )
{
	return b3Shape_IsSensor( shapeId ) || nbFigureOfBody( query->world, b3Shape_GetBody( shapeId ), NULL ) == query->figure;
}

static bool nbMoverPlaneFcn( b3ShapeId shapeId, const b3PlaneResult* planes, int planeCount, void* context )
{
	nbMoverQuery* query = context;
	if ( nbMoverSkips( query, shapeId ) )
	{
		return true;
	}
	for ( int i = 0; i < planeCount && query->count < NB_STEER_PLANES; ++i )
	{
		// On the ground, what is too steep to walk on is a wall: it holds the capsule back, but does not lift it, as the
		// floors and walls of move_and_slide in Godot. The plane stands up from it, as far out as the capsule has to go
		// to leave it.
		b3Plane surface = planes[i].plane;
		float rise = b3Dot( surface.normal, query->up );
		if ( query->walls && rise < query->flatEnough && rise > -NB_STEER_CEILING )
		{
			b3Vec3 across = b3MulSub( surface.normal, rise, query->up );
			float length = b3Length( across );
			if ( length > NB_STEER_CEILING )
			{
				surface.normal = b3MulSV( 1.0f / length, across );
				surface.offset /= length;
			}
		}
		nbMoverPlane* plane = query->planes + query->count++;
		plane->plane = (b3CollisionPlane){ surface, FLT_MAX, 0.0f, true };
		plane->shape = shapeId.index1;
		plane->child = planes[i].childIndex;
		plane->triangle = planes[i].triangleIndex;
		plane->order = i;
	}
	return true;
}

static bool nbMoverFilterFcn( b3ShapeId shapeId, void* context )
{
	return nbMoverSkips( context, shapeId ) == false;
}

// The planes in an order that does not depend on the order the tree visits the shapes in
static int nbComparePlanes( const void* a, const void* b )
{
	const nbMoverPlane* p = a;
	const nbMoverPlane* q = b;
	if ( p->shape != q->shape )
	{
		return p->shape < q->shape ? -1 : 1;
	}
	if ( p->child != q->child )
	{
		return p->child < q->child ? -1 : 1;
	}
	if ( p->triangle != q->triangle )
	{
		return p->triangle < q->triangle ? -1 : 1;
	}
	return p->order < q->order ? -1 : ( p->order > q->order ? 1 : 0 );
}

// The ground under the capsule: a hit of the probe, with the ray down its middle where the sphere touches an edge too
// steep to stand on
typedef struct nbProbe
{
	const nbWorld* world;
	int figure;
	bool hit;
	float fraction;
	b3Vec3 normal;
	b3Pos point;
	b3ShapeId shapeId;
} nbProbe;

static float nbProbeFcn( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
						 int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbProbe* probe = context;
	if ( b3Shape_IsSensor( shapeId ) || nbFigureOfBody( probe->world, b3Shape_GetBody( shapeId ), NULL ) == probe->figure ||
		 ( fraction == 0.0f && b3LengthSquared( normal ) < 0.25f ) )
	{
		return -1.0f;
	}

	// The callbacks come in any order: the closest wins, and of two at the same distance the shape of the lower index
	if ( probe->hit == false || fraction < probe->fraction ||
		 ( fraction == probe->fraction && shapeId.index1 < probe->shapeId.index1 ) )
	{
		probe->hit = true;
		probe->fraction = fraction;
		probe->normal = normal;
		probe->point = point;
		probe->shapeId = shapeId;
	}
	return fraction;
}

// Cast the probe from a point down a length, as a sphere of a radius or as a ray where the radius is zero
static nbProbe nbCastProbe( const nbWorld* world, int index, b3Pos origin, b3Vec3 down, float radius )
{
	nbProbe probe = { world, index, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId };
	if ( radius > 0.0f )
	{
		b3Vec3 center = b3Vec3_zero;
		b3ShapeProxy proxy = { &center, 1, radius };
		b3World_CastShape( world->physicsWorld, origin, &proxy, down, nbFigureQueryFilter( world, index ), nbProbeFcn, &probe );
	}
	else
	{
		b3World_CastRay( world->physicsWorld, origin, down, nbFigureQueryFilter( world, index ), nbProbeFcn, &probe );
	}
	return probe;
}

float nbWrapAngle( float angle )
{
	while ( angle > B3_PI )
	{
		angle -= 2.0f * B3_PI;
	}
	while ( angle < -B3_PI )
	{
		angle += 2.0f * B3_PI;
	}
	return angle;
}

// The points that bound a bone in its frame, and how far its shape reaches around them
static int nbBoneBounds( const nbBone* bone, b3Vec3* points, float* round )
{
	if ( bone->shape == nb_capsuleBone )
	{
		points[0] = bone->start;
		points[1] = bone->end;
		*round = bone->radius;
		return 2;
	}
	b3Vec3 c = bone->center;
	b3Vec3 h = bone->halfExtents;
	for ( int k = 0; k < 8; ++k )
	{
		points[k] = (b3Vec3){ c.x + ( k & 1 ? h.x : -h.x ), c.y + ( k & 2 ? h.y : -h.y ), c.z + ( k & 4 ? h.z : -h.z ) };
	}
	*round = 0.0f;
	return 8;
}

void nbResetSteering( nbFigure* f )
{
	f->moveVelocity = b3Vec3_zero;
	f->onGround = true;
	f->probeRate = 0.0f;
	f->airTime = 0.0f;
	f->slope = 0.0f;
	f->headingRate = 0.0f;
	f->gaitOn = false;
	f->gaitSpeed = 0.0f;
	f->gaitVelocity = b3Vec3_zero;
	f->gaitAcceleration = b3Vec3_zero;
	f->leanVelocity = b3Vec3_zero;
	f->leanAcceleration = b3Vec3_zero;
	f->runTurn = 0.0f;
	f->runTurnRate = 0.0f;
	f->gaitRun = 0.0f;
	f->legReach = 0.98f;
	f->gaitTurn = 0.0f;
	f->gaitTurnSize = 0.0f;
	f->gaitSway = 0.0f;
	f->gaitCrouch = 0.0f;
	f->rootDrop = 0.0f;
	f->rootDropRate = 0.0f;
	f->rootCrouch = 0.0f;
	f->gaitBob = 0.0f;
	f->gaitStance = 0.0f;
	f->gaitPhaseRate = 0.0f;
	f->gaitBend = 0.0f;
	f->gaitBendRate = 0.0f;
	f->stairWeight = 0.0f;
	f->stairDepth = 0.0f;
	f->stairRise = 0.0f;
	f->stairStep = 0;
	f->gaitRate = 1.0f;
	f->stairCarry = 0.0f;
	f->carryFrame = f->frame.p;
	f->stairBase = f->frame.p;
	f->carryRoot = 0.0f;
	f->carryGait = false;
	for ( int k = 0; k < 2; ++k )
	{
		f->foothold[k] = f->frame.p;
		f->liftHold[k] = f->frame.p;
		f->footholdChosen[k] = false;
		f->footSwings[k] = false;
		f->canLift[k] = true;
		f->swingAlong[k] = 0.0f;
		f->swingStart[k] = 0.0f;
		f->swingHeight[k] = 0.0f;
		f->swingRun[k] = 0.0f;
		f->swingShare[k] = 0.0f;
		f->swingNarrow[k] = 1.0f;
		f->footHeading[k] = f->heading;
		f->liftHeading[k] = f->heading;
		f->landHeading[k] = f->heading;
		f->footRoll[k] = 0.0f;
		f->swingRoll[k] = 0.0f;
		f->landRoll[k] = 0.0f;
		f->footPush[k] = 0.0f;
		f->footRollRate[k] = 0.0f;
		f->footOut[k] = FLT_MAX;
		f->outBehind[k] = false;
		f->swingHurry[k] = 1.0f;
		f->swingCalm[k] = 1.0f;
		f->swingBase[k] = b3Pos_zero;
		f->swingVelocity[k] = b3Vec3_zero;
		f->swingTurnRate[k] = 0.0f;
		f->stanceTime[k] = 0.0f;
	}
}

void nbInitSteering( nbFigure* f, const b3WorldTransform* transforms )
{
	f->steered = f->def.steered;
	f->steer = (nbSteer){ b3Vec3_zero, f->heading, false };
	f->gaitPhase = 0.0f;
	f->gaitLag = 0.0f;
	f->gaitLagRate = 0.0f;
	nbResetSteering( f );
	if ( f->steered == false )
	{
		return;
	}

	// The capsule from the bones as the figure stands created: as wide as the farthest point of a bone from the up through
	// the frame, from the step height to the top of the highest bone
	float scale = f->def.standHeight / NB_HUMANOID_STAND;
	float width = 0.0f;
	float top = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Vec3 points[8];
		float round;
		int count = nbBoneBounds( f->bones + i, points, &round );
		for ( int k = 0; k < count; ++k )
		{
			b3Vec3 local = b3InvTransformWorldPoint( f->frame, b3TransformWorldPoint( transforms[i], points[k] ) );
			width = b3MaxFloat( width, sqrtf( local.x * local.x + local.z * local.z ) + round );
			top = b3MaxFloat( top, local.y + round );
		}
	}
	const nbFigureSteering* def = &f->def.steering;
	float radius = def->radius > 0.0f ? def->radius : b3MaxFloat( width, 0.05f * scale );
	float step = def->stepHeight > 0.0f ? def->stepHeight : NB_STEER_STEP_SHARE * f->def.standHeight;
	f->moverRadius = radius;
	f->moverLow = step + radius;
	f->moverHigh = b3MaxFloat( f->moverLow, top - radius );
}

bool nbMoveSteered( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	const nbFigureSteering* def = &f->def.steering;
	float scale = f->def.standHeight / NB_HUMANOID_STAND;
	float gravity = b3Length( b3World_GetGravity( world->physicsWorld ) );
	float radius = f->moverRadius;
	float step = f->moverLow - radius;

	// The velocity it wants over the ground: it gets there with its acceleration and slows down to it with its braking, in
	// the air with a part of them
	b3Vec3 velocity = f->moveVelocity;
	float climb = b3Dot( velocity, up );
	b3Vec3 flat = b3MulSub( velocity, climb, up );
	b3Vec3 wish = b3MulSub( f->steer.velocity, b3Dot( f->steer.velocity, up ), up );
	if ( f->steer.faceVelocity )
	{
		// Facing where it moves, it goes across its heading and backward no faster than a step to the side or back, see
		// NB_STEER_SIDE_SPEED, and gets to the rest of the velocity it wants as it turns there
		b3Vec3 forward = b3RotateVector( f->frame.q, b3Vec3_axisZ );
		forward = b3Normalize( b3MulSub( forward, b3Dot( forward, up ), up ) );
		float along = b3Dot( wish, forward );
		b3Vec3 across = b3MulSub( wish, along, forward );
		float side = b3MaxFloat( NB_STEER_SIDE_SPEED * scale, NB_STEER_SIDE_SHARE * b3Length( wish ) );
		float back = NB_STEER_BACK_SPEED * scale;
		float sideLength = b3Length( across );
		if ( sideLength > side || along < -back )
		{
			if ( sideLength > side )
			{
				across = b3MulSV( side / sideLength, across );
			}
			along = b3MaxFloat( along, -back );
			wish = b3MulAdd( across, along, forward );
		}
	}
	// The gait plans with the velocity the capsule goes for, see nbSteerFeet
	f->wishVelocity = wish;
	b3Vec3 change = b3Sub( wish, flat );
	float rate = b3LengthSquared( wish ) > b3LengthSquared( flat ) ? def->acceleration : def->braking;
	rate = b3MaxFloat( rate, 0.0f ) * ( f->onGround ? 1.0f : b3ClampFloat( def->airControl, 0.0f, 1.0f ) );
	float most = rate * timeStep;
	float length = b3Length( change );
	flat = length <= most ? wish : b3MulAdd( flat, most / length, change );

	// The ground: the probe from the lower center of the capsule down to the step height below the frame where the figure
	// stands, a little below it in the air
	float probeRadius = NB_STEER_PROBE * radius;
	float reach = f->onGround ? NB_STEER_SNAP * step : NB_STEER_AIR_REACH * scale;
	float cast = f->moverLow - probeRadius + reach;
	b3Pos low = b3OffsetPos( f->frame.p, b3MulSV( f->moverLow, up ) );
	b3Vec3 down = b3MulSV( -cast, up );
	float flatEnough = b3ComputeCosSin( b3ClampFloat( def->maxSlope, 0.0f, 0.5f * B3_PI ) ).cosine;
	nbProbe probe = nbCastProbe( world, index, low, down, probeRadius );
	if ( probe.hit && b3Dot( probe.normal, up ) < flatEnough )
	{
		// The sphere touches something too steep to stand on, a slope the capsule leans against or the edge of a step: the
		// ground is where a ray down the middle finds it, or else a ray around it, the highest that is flat enough
		const b3Vec3 around[5] = {
			{ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } };
		bool found = false;
		for ( int i = 0; i < 5; ++i )
		{
			b3Vec3 offset = b3RotateVector( f->frame.q, b3MulSV( NB_STEER_AROUND * radius, around[i] ) );
			nbProbe ray = nbCastProbe( world, index, b3OffsetPos( low, offset ), down, 0.0f );
			if ( ray.hit && b3Dot( ray.normal, up ) >= flatEnough &&
				 ( found == false || b3Dot( b3SubPos( ray.point, probe.point ), up ) > 0.0f ) )
			{
				probe = ray;
				found = true;
			}
			if ( found && i == 0 )
			{
				break;
			}
		}
	}
	bool ground = probe.hit && b3Dot( probe.normal, up ) >= flatEnough && ( f->onGround || climb <= 0.0f );
	if ( ground == false && probe.hit && f->onGround && f->gaitOn )
	{
		// Under the capsule only sides too steep to stand on, as between pieces of rubble: the figure stands on the planted
		// foot nearest to the frame, if that lies within its step of it. Over rubble it fell between the pieces in one of
		// twenty updates, and the gait stopped and set it down into a crouch.
		float nearest = FLT_MAX;
		for ( int k = 0; k < 2; ++k )
		{
			b3Vec3 offset = b3SubPos( f->foothold[k], f->frame.p );
			float height = b3Dot( offset, up );
			float across = b3Length( b3MulSub( offset, height, up ) );
			if ( f->footSwings[k] == false && fabsf( height ) <= step && across < nearest )
			{
				nearest = across;
				probe.point = f->foothold[k];
				probe.normal = up;
				ground = true;
			}
		}
	}

	// Ground that moves fast does not carry it, as a standing figure falls from it
	if ( ground )
	{
		b3BodyId body = b3Shape_GetBody( probe.shapeId );
		if ( b3Body_GetType( body ) != b3_staticBody && b3Length( b3Body_GetLinearVelocity( body ) ) > NB_GROUND_SPEED )
		{
			return false;
		}
	}

	// On the ground a critically damped spring pulls the frame onto it, stepped by its exact solution, and a landing goes
	// on into it with the speed it fell with. The ground goes on as the plane the probe found, so on a slope the spring
	// holds the frame over the ground where the capsule goes, without falling behind. In the air it falls.
	float rise;
	if ( ground )
	{
		float slant = -b3Dot( probe.normal, b3MulSV( timeStep, flat ) ) / b3Dot( probe.normal, up );
		float above = b3Dot( b3SubPos( f->frame.p, probe.point ), up ) - slant;
		float omega = 2.0f * B3_PI * NB_STEER_HERTZ;
		float springRate = f->onGround ? f->probeRate : climb;
		float decay = nbDecay( omega * timeStep );
		float sum = springRate + omega * above;
		float next = decay * ( above + timeStep * sum );
		f->probeRate = decay * ( springRate - omega * timeStep * sum );
		rise = next - above + slant;
		climb = 0.0f;
		f->airTime = 0.0f;
	}
	else
	{
		climb -= gravity * timeStep;
		rise = climb * timeStep;
		f->probeRate = 0.0f;
		f->airTime += timeStep;
	}
	f->onGround = ground;

	// The capsule slides along what it touches: the planes there, where it can go between them, and the sweep there, until
	// it gets no farther
	nbMoverQuery query;
	query.world = world;
	query.figure = index;
	query.up = up;
	query.flatEnough = flatEnough;
	query.walls = ground;
	query.count = 0;
	b3QueryFilter filter = nbFigureQueryFilter( world, index );
	b3CollisionPlane planes[NB_STEER_PLANES];
	b3Capsule capsule = { b3MulSV( f->moverLow, up ), b3MulSV( f->moverHigh, up ), radius };
	b3Pos position = f->frame.p;
	b3Vec3 target = b3MulAdd( b3MulSV( timeStep, flat ), rise, up );
	b3Vec3 moved = b3Vec3_zero;
	int count = 0;
	for ( int sweep = 0; sweep < NB_STEER_SWEEPS; ++sweep )
	{
		query.count = 0;
		b3World_CollideMover( world->physicsWorld, position, &capsule, filter, nbMoverPlaneFcn, &query );
		qsort( query.planes, (size_t)query.count, sizeof( nbMoverPlane ), nbComparePlanes );
		count = query.count;
		for ( int i = 0; i < count; ++i )
		{
			planes[i] = query.planes[i].plane;
		}
		b3PlaneSolverResult solved = b3SolvePlanes( b3Sub( target, moved ), planes, count );
		float fraction =
			b3World_CastMover( world->physicsWorld, position, &capsule, solved.delta, filter, nbMoverFilterFcn, &query );
		b3Vec3 delta = b3MulSV( fraction, solved.delta );
		position = b3OffsetPos( position, delta );
		moved = b3Add( moved, delta );
		if ( b3LengthSquared( delta ) < NB_STEER_SETTLED * NB_STEER_SETTLED )
		{
			break;
		}
	}
	velocity = b3ClipVector( b3MulAdd( flat, climb, up ), planes, count );
	f->moveVelocity = ground ? b3MulSub( velocity, b3Dot( velocity, up ), up ) : velocity;

	// The slope of its way: how far the frame rose over how far it went across, over the last NB_STEER_SLOPE_LENGTH of
	// the way on the ground, at most NB_STEER_SLOPE_MAX up or down. In the air its way has no slope.
	float rose = b3Dot( moved, up );
	float across = b3Length( b3MulSub( moved, rose, up ) );
	if ( ground == false )
	{
		f->slope = 0.0f;
	}
	else if ( across > NB_STEER_SETTLED * scale )
	{
		float share = b3MinFloat( across / ( NB_STEER_SLOPE_LENGTH * scale ), 1.0f );
		float slope = b3ClampFloat( rose / across, -NB_STEER_SLOPE_MAX, NB_STEER_SLOPE_MAX );
		f->slope += share * ( slope - f->slope );
	}

	// The heading it wants, or where it moves, at its turn rate, which it gets to and slows down from at its turn
	// acceleration: as fast as it can still stop at the heading it wants, slowing down by as much in every update
	float heading = f->heading;
	float want = f->steer.heading;
	if ( f->steer.faceVelocity )
	{
		b3Quat tilt = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, up );
		b3Vec3 local = b3InvRotateVector( tilt, f->moveVelocity );
		bool moving = local.x * local.x + local.z * local.z > NB_STEER_FACE_SPEED * NB_STEER_FACE_SPEED;
		want = moving ? b3Atan2( local.x, local.z ) : heading;
	}
	float turn = nbWrapAngle( want - heading );
	float turnRate = b3MaxFloat( def->turnRate, 0.0f );
	if ( def->turnAcceleration > 0.0f && timeStep > 0.0f )
	{
		float change = def->turnAcceleration * timeStep;
		float stop = sqrtf( 2.0f * def->turnAcceleration * fabsf( turn ) + 0.25f * change * change ) - 0.5f * change;
		float aim = b3MinFloat( stop, turnRate );
		aim = turn < 0.0f ? -aim : aim;
		f->headingRate += b3ClampFloat( aim - f->headingRate, -change, change );
		if ( fabsf( turn ) <= change * timeStep && fabsf( f->headingRate ) <= change )
		{
			f->headingRate = turn / timeStep;
		}
		heading = nbWrapAngle( heading + f->headingRate * timeStep );
	}
	else
	{
		float mostTurn = turnRate * timeStep;
		float step = b3ClampFloat( turn, -mostTurn, mostTurn );
		f->headingRate = timeStep > 0.0f ? step / timeStep : 0.0f;
		heading = nbWrapAngle( heading + step );
	}
	f->heading = heading;
	f->frame = nbMakeFrame( world, position, heading );

	// In the air longer than a fall from the height of NB_STEER_FALL_HEIGHT takes, it falls as a ragdoll
	float fallTime = sqrtf( 2.0f * NB_STEER_FALL_HEIGHT * scale / b3MaxFloat( gravity, 0.1f ) );
	return f->airTime <= fallTime;
}
