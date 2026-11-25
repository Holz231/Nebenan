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

// With nothing under the capsule within the reach of the probe, a planted foot holds the frame only if it stands deeper
// below it than this share of its step height, see nbMoveSteered: a foot higher up stands on the ground the capsule
// leaves, as at a ledge
#define NB_STEER_FOOT_DEEP 0.5f

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

// The capsule changes its velocity faster than a step takes, and at a walk the feet do not keep up: a foot late in its
// swing lands where it aimed, a step ahead the old way, the hip then leaves across it faster than the other foot
// swings, the leg loses its foot, and the root goes down for it, see nbLowerSteered, 103 mm in 0.3 s turning by a
// quarter at 1.12 m/s. Two things hold the capsule back for a humanoid steered at a walk, at NB_WALK_STEERED meters per
// second or less in full and from NB_WALK_STEERED_FADE more not at all, for the humanoid of 1.8 m. Turning on level
// ground, with more than NB_TURN_HOLD_FROM radians still to turn and a way that slopes less than NB_TURN_HOLD_LEVEL:
// while the leg it stands on loses its foot before the swinging foot lands, it gets no faster. Everywhere at a walk
// that made turning about slower and the legs worse over rubble, where a leg loses its foot for the ground. And
// stopping after it walked for NB_STOP_WALKED seconds: with a foot later in its swing than NB_STOP_COMMIT, which lands
// where it aimed, it brakes with NB_STOP_GENTLE meters per second squared at most until that foot is down, so the hip
// gets over it. Braking so gently for every change of the velocity, the pelvis sank more in turns.
#define NB_WALK_STEERED 2.0f
#define NB_WALK_STEERED_FADE 0.5f
#define NB_TURN_HOLD_FROM ( 10.0f * ( B3_PI / 180.0f ) )
#define NB_TURN_HOLD_LEVEL 0.01f
#define NB_STOP_WALKED 1.0f
#define NB_STOP_COMMIT 0.7f
#define NB_STOP_GENTLE 4.0f

// Stopping out of a walk, a humanoid rolls out over its last step, see nbFigureSteering::stopBraking: where the
// velocity it wants is zero and it had walked for NB_STOP_WALKED seconds when it last wanted to move, the capsule
// brakes no harder than that, and as gently as the foot ahead asks for, see nbSteerFeet and NB_HALT_LOW. Braking with
// 14 m/s^2, the capsule stood 0.11 s after the stick let go, in the middle of a step, and the feet took 1.85 s and
// three to four landings to come to rest under it: the hind foot came up, the front foot stepped back to its place,
// and the first one stepped 4 cm aside. How long it had walked counts as the stick lets go, not later: at the end of a
// stop out of a run the gait walks for a second as well, and its feet came to rest with the short steps of a stop out
// of a walk, a shin 44.6 instead of 30.3 mm in an update stopping from a run sideways.

// A figure steered at a walk bends through a turn, see nbFigureSteering::walkBend, where it walks the way it faces as
// the turn begins with this share of the speed of the bend or more. Below it, from standing or a first step, it turns
// on the spot: bending from standing, it was at its speed to the side after 0.58 s and backward after 1.10 s, not 0.40
// and 0.83 s.
#define NB_WALK_BEND_FROM 0.5f

// Steered more than 150 degrees off the way it faces, this is the cosine, a figure at a walk does not bend, it turns
// about: bending, it walked on and did not go where a stick pointed that went back and forth, -0.02 of the steered
// speed in the mean of 192 runs steered fast, and 0.18 turning about. It brakes gently while a foot lands where it
// aimed, as it does stopping, see NB_STOP_GENTLE, and gets no faster while the leg it stands on loses its foot before
// the other foot lands, see NB_TURN_HOLD_FROM: braking hard and going backward at once, it left the foot that had
// landed a step ahead, and the pelvis sank 83 mm. A bend that goes on ends where it is steered that far off. A new
// turn begins, and it is decided again whether it bends, where the direction it is steered in jumps by more than 30
// degrees, NB_WALK_TURN_NEW is the cosine. For NB_TURN_HOLD_AFTER seconds after a bend or a turn about it still waits
// for the leg it stands on: setting off again at once, the pelvis jumped 22 mm in a run.
#define NB_WALK_ABOUT -0.866f

// Turning about at a walk the capsule goes across and against where it faces with NB_ABOUT_CREEP of what it may, see
// NB_STEER_SIDE_SPEED. Creeping the new way while it still faces away, the figure goes backward against its legs: with
// the heading turning at once, see nbMoveSteered, it did so on a foot that had only just landed far ahead, the feet got
// 0.34 m apart, and the root went down 5 cm until the other foot had stepped round. The pelvis got more than 30 mm
// below the steady gait on 19.5 instead of 13 of 272 courses at a walk, round 174. The slower it creeps, the less the
// pelvis sinks and the nearer the legs stand as it turns: at 0.65 on 9.5 courses, with the legs more than 20 mm into
// each other in 6.1 instead of 4.7 % of the updates going back and forth; at 0.85 on 16.5 courses and 4.8 %; at 0.8 on
// 14 courses and 4.8 %, both within the noise of four nudges, see tools/measure/eval175.py.
#define NB_ABOUT_CREEP 0.8f

// Where the velocity a figure is steered at jumps by more than NB_STEER_JUMP from one update to the next, in meters per
// second for the humanoid of 1.8 m, the root goes down for a landing gently for NB_CROUCH_EASE seconds, see
// NB_CROUCH_EASE_RATE in figure.c. The gait plans with the new velocity at once, and where a foot in the air lands can
// jump with it: let go at a run for 0.2 s and steered on, the landing of a foot late in its swing went from 0.25 m
// ahead of the hip to 0.47 m behind it, the step of setting off, the root went 30 mm down for it in that update and
// came back as the landing moved on with the capsule, and the pelvis jumped 29 mm down and 31 mm up.
#define NB_STEER_JUMP 1.0f
#define NB_CROUCH_EASE 0.1f

// A figure starts from standing where it goes slower than this, in meters per second for the humanoid of 1.8 m, and
// its gait is not on, see nbFigureSteering::startAcceleration
#define NB_START_FROM 0.3f

// A figure that faces where it moves turns no farther than its feet stay uncrossed across where it faces: the heading
// turns as far as the left foot stays NB_FACE_KEEP to the left of the right one, in meters for the humanoid of 1.8 m,
// where the feet are now, on the ground or in the air, and waits for that no longer than NB_FACE_HOLD in seconds in a
// row. So it waits until the foot behind swings past the standing one and turns over the standing foot, as people do.
// Until round 167 the heading turned at the turn rate wherever the feet stood, and the legs with it: with the foot
// ahead on the side it turned to, after a quarter turn both feet stood on the wrong side of the body and the foot
// behind swung through the standing leg. Turning by 90 and 180 degrees and steered back and forth, thighs or shins
// went deeper than 60 mm into each other in 80 of 144 runs, up to 139 mm; now in none of them, 54 mm at most, see
// SteerLegsApartTest and tools/measure/legs167.py. The price: it faces the new way later, turning by 90 degrees at a
// walk after 0.85 s at the latest instead of 0.5, 0.72 s in the mean of the turns at a walk instead of 0.59. Waiting
// 0.3 s at most, the legs still went 120 mm into each other; 0.5, 0.7 and 1 s give the same. With the foot on the
// outside of the turn stepping round at once it faced the new way after 0.72 s, and the pelvis jumped 126 mm at a run
// instead of 27. Nearer feet, 0.03 or 0.01 m, were no better. The legs under a fixed heading have their own rule, see
// NB_LEG_KEEP in figure.c.
#define NB_FACE_KEEP 0.06f
#define NB_FACE_HOLD 0.5f
#define NB_WALK_TURN_NEW 0.866f
#define NB_TURN_HOLD_AFTER 0.5f

// Two steered figures meet as their capsules, see nbMoveSteered. Closer than NB_MEET_NEAR, in meters between the
// capsules, one closes in on the other no faster than NB_MEET_SOFT times the gap between them, in meters per second for
// a meter. NB_MEET_MARGIN is how far a capsule gets in an update beyond the way of the velocity it starts with, in
// meters, see nbFindMeetings.
#define NB_MEET_NEAR 0.5f
#define NB_MEET_SOFT 6.0f
#define NB_MEET_MARGIN 0.1f

// A plane whose normal points down more than this, its cosine to up, is a ceiling, not a wall
#define NB_STEER_CEILING 0.1f

// The slope of its way, rise over run, follows the way over this many meters for the humanoid of 1.8 m, at most this
// steep up or down
#define NB_STEER_SLOPE_LENGTH 0.5f
#define NB_STEER_SLOPE_MAX 1.0f

// Facing where it moves, see nbSteer::faceVelocity, a figure goes across its heading no faster than NB_STEER_SIDE_SPEED
// in meters per second for the humanoid of 1.8 m or NB_STEER_SIDE_SHARE of the speed it wants, whichever is more, and
// backward no faster than NB_STEER_BACK_SPEED: it turns to where it wants to go and gets there as it faces it. Going
// off to the side faster than its heading turned, the capsule left the feet behind it, across each other. Not facing
// where it moves, it goes so across where its legs walk and against it, see nbLegTurnFor.
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

// A steered figure that stands animated meets the other steered figures as its capsule, not as its bones, see
// nbMoveSteered
static bool nbHasCapsule( const nbWorld* world, int index )
{
	const nbFigure* f = world->figures.data + index;
	return f->isFree == false && f->steered && f->state == nb_figureAnimated;
}

// What the capsule of a figure leaves out: its own bones, sensors, and the bones of the figures it meets as capsules
static bool nbMoverSkips( const nbMoverQuery* query, b3ShapeId shapeId )
{
	int other = nbFigureOfBody( query->world, b3Shape_GetBody( shapeId ), NULL );
	return b3Shape_IsSensor( shapeId ) || other == query->figure ||
		   ( other != NB_NULL_INDEX && nbHasCapsule( query->world, other ) );
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
	// The bones of a figure it meets as a capsule are no ground: until round 154 the probe found the swinging foot of
	// another steered figure, ground that moves too fast to stand on, and both fell
	int other = nbFigureOfBody( probe->world, b3Shape_GetBody( shapeId ), NULL );
	bool capsule = other != NB_NULL_INDEX && nbHasCapsule( probe->world, other );
	if ( b3Shape_IsSensor( shapeId ) || other == probe->figure || capsule ||
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

float nbTurnToward( float angle, float want, float* rate, const nbFigureSteering* def, float timeStep )
{
	float turn = nbWrapAngle( want - angle );
	float turnRate = b3MaxFloat( def->turnRate, 0.0f );
	if ( def->turnAcceleration > 0.0f && timeStep > 0.0f )
	{
		float change = def->turnAcceleration * timeStep;
		float stop = sqrtf( 2.0f * def->turnAcceleration * fabsf( turn ) + 0.25f * change * change ) - 0.5f * change;
		float aim = b3MinFloat( stop, turnRate );
		aim = turn < 0.0f ? -aim : aim;
		*rate += b3ClampFloat( aim - *rate, -change, change );
		if ( fabsf( turn ) <= change * timeStep && fabsf( *rate ) <= change )
		{
			*rate = turn / timeStep;
		}
		return nbWrapAngle( angle + *rate * timeStep );
	}
	float mostTurn = turnRate * timeStep;
	float step = b3ClampFloat( turn, -mostTurn, mostTurn );
	*rate = timeStep > 0.0f ? step / timeStep : 0.0f;
	return nbWrapAngle( angle + step );
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
	f->legTurn = 0.0f;
	f->legHeading = f->heading;
	f->legHeadingRate = 0.0f;
	f->legAim = 0.0f;
	f->legBack = false;
	f->legHeld = 0.0f;
	f->gaitOn = false;
	f->gaitStarting = false;
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
	f->stanceDown = 0.0f;
	f->stanceDownRate = 0.0f;
	f->legRoom = FLT_MAX;
	f->legWaits = -1;
	f->legLead = 1;
	f->swingRest[0] = 0.0f;
	f->swingRest[1] = 0.0f;
	f->askedSpeed = 0.0f;
	f->stairWeight = 0.0f;
	f->stairDepth = 0.0f;
	f->stairRise = 0.0f;
	f->stairStep = 0;
	f->gaitRate = 1.0f;
	f->rootCarry = 0.0f;
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
		f->swingBack[k] = false;
		f->stepBack[k] = false;
		f->legBlocks[k] = false;
		f->swingShare[k] = 0.0f;
		f->swingLift[k] = 0.0f;
		f->swingNarrow[k] = 1.0f;
		f->footHeading[k] = f->heading;
		f->liftHeading[k] = f->heading;
		f->landHeading[k] = f->heading;
		f->footRoll[k] = 0.0f;
		f->footRolled[k] = 0.0f;
		f->footRollGain[k] = 0.0f;
		f->heelPart[k] = 1.0f;
		f->ankleHeld[k] = -1.0f;
		f->swingRoll[k] = 0.0f;
		f->landRoll[k] = 0.0f;
		f->footPush[k] = 0.0f;
		f->pushKept[k] = 0.0f;
		f->pushKeptTime[k] = 0.0f;
		f->footRollRate[k] = 0.0f;
		f->footOut[k] = FLT_MAX;
		f->outBehind[k] = false;
		f->outToes[k] = false;
		f->swingHurry[k] = 1.0f;
		f->swingRate[k] = 1.0f;
		f->swingCalm[k] = 1.0f;
		f->swingBump[k] = 0.0f;
		f->swingBumpWas[k] = 0.0f;
		f->swingBase[k] = b3Pos_zero;
		f->swingVelocity[k] = b3Vec3_zero;
		f->swingSoft[k] = 0.0f;
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

float nbLegCap( const nbFigure* f, float theta, float speed, float legTurn, bool legBack )
{
	bool backward;
	float turned;
	nbLegTurnAt( f, theta, speed, legBack, &backward, &turned );
	return nbLegCapAcross( f, theta, speed, legTurn, legBack, turned );
}

float nbLegCapAcross( const nbFigure* f, float theta, float speed, float legTurn, bool legBack, float turned )
{
	float scale = f->def.standHeight / NB_HUMANOID_STAND;
	float legs = legTurn + ( legBack ? B3_PI : 0.0f );
	b3CosSin off = b3ComputeCosSin( nbWrapAngle( theta - legs ) );
	float side = b3MaxFloat( b3MaxFloat( NB_STEER_SIDE_SPEED * scale, NB_STEER_SIDE_SHARE * speed ), turned * speed );
	float back = NB_STEER_BACK_SPEED * scale;
	float most = speed;
	if ( fabsf( off.sine ) * most > side )
	{
		most = side / fabsf( off.sine );
	}
	if ( off.cosine * most < -back )
	{
		most = back / -off.cosine;
	}
	return most;
}

// One steered figure keeps another to meet, see nbFindMeetings
static void nbKeepMeeting( nbFigure* f, int other )
{
	if ( f->meetCount == NB_MEET_ALL || f->meetCount == NB_MEET_MOST )
	{
		f->meetCount = NB_MEET_ALL;
		return;
	}
	f->meetOthers[f->meetCount++] = other;
}

void nbFindMeetings( nbWorld* world, float timeStep, b3Vec3 up )
{
	// How far each capsule reaches in this update: across its radius and half the distance at which two slow down for
	// each other, and up its height, each with the way it goes at the velocity it has and a margin for what it gains
	int count = world->figures.count;
	nbArray_Reserve( world->figureMeets, count );
	nbMeet* meets = world->figureMeets.data;
	b3Pos origin = world->figures.data[0].frame.p;
	b3Vec3 level = b3Normalize( b3Cross( up, b3AbsFloat( up.x ) < 0.5f ? b3Vec3_axisX : b3Vec3_axisZ ) );
	int steered = 0;
	for ( int i = 0; i < count; ++i )
	{
		nbFigure* f = world->figures.data + i;
		f->meetCount = 0;
		if ( f->isFree == false && f->steered )
		{
			origin = steered == 0 ? f->frame.p : origin;
			float margin = NB_MEET_MARGIN + timeStep * b3Length( f->moveVelocity );
			b3Vec3 offset = b3SubPos( f->frame.p, origin );
			float reach = f->moverRadius + 0.5f * NB_MEET_NEAR + margin;
			float tall = f->moverHigh + f->moverRadius + margin;
			meets[steered++] = (nbMeet){ offset, b3Dot( offset, level ), b3Dot( offset, up ), reach, tall, i };
		}
	}
	world->figureMeets.count = steered;

	// Two whose frames are no farther apart than both reach across and up can meet. Each keeps the other, in the order
	// of the figures, and one with more of them than it holds asks all, see nbMoveSteered. Most pairs are farther apart
	// along the one level direction already.
	for ( int i = 0; i < steered; ++i )
	{
		nbMeet a = meets[i];
		for ( int j = i + 1; j < steered; ++j )
		{
			const nbMeet* b = meets + j;
			float reach = a.reach + b->reach;
			if ( b3AbsFloat( b->along - a.along ) > reach )
			{
				continue;
			}
			float above = b->height - a.height;
			if ( b3AbsFloat( above ) <= a.tall + b->tall &&
				 b3LengthSquared( b3Sub( b->offset, a.offset ) ) - above * above <= reach * reach )
			{
				nbKeepMeeting( world->figures.data + a.index, b->index );
				nbKeepMeeting( world->figures.data + b->index, a.index );
			}
		}
	}
}

// The capsule of another steered figure as a wall for the capsule of this one at position: it stands up between the two
// axes, its normal toward this one, gap meters from it, less than zero where the two overlap. False where the other has
// no capsule, see nbHasCapsule, where one is above the other, and farther than NB_MEET_NEAR.
static bool nbMeetCapsule( const nbWorld* world, int index, const nbFigure* f, int other, b3Pos position, b3Vec3 up,
						   b3Vec3* normal, float* gap )
{
	const nbFigure* o = world->figures.data + other;
	if ( other == index || nbHasCapsule( world, other ) == false )
	{
		return false;
	}
	b3Vec3 apart = b3SubPos( position, o->frame.p );
	float above = b3Dot( apart, up );
	if ( above + f->moverLow - f->moverRadius > o->moverHigh + o->moverRadius ||
		 above + f->moverHigh + f->moverRadius < o->moverLow - o->moverRadius )
	{
		return false;
	}
	b3Vec3 across = b3MulSub( apart, above, up );
	float distance = b3Length( across );
	*gap = distance - f->moverRadius - o->moverRadius;
	// on the same axis the figure of the lower index gives way to the left of its frame
	*normal = distance > 1.0e-4f ? b3MulSV( 1.0f / distance, across )
								 : b3RotateVector( f->frame.q, index < other ? b3Vec3_axisX : b3Neg( b3Vec3_axisX ) );
	return *gap <= NB_MEET_NEAR;
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
	f->crouchEase = b3MaxFloat( f->crouchEase - timeStep, 0.0f );
	if ( b3LengthSquared( b3Sub( f->steer.velocity, f->steerBefore ) ) > NB_STEER_JUMP * NB_STEER_JUMP * scale * scale )
	{
		f->crouchEase = NB_CROUCH_EASE;
	}
	f->steerBefore = f->steer.velocity;
	b3Vec3 wish = b3MulSub( f->steer.velocity, b3Dot( f->steer.velocity, up ), up );
	b3Vec3 asked = wish;
	f->steeredSpeed = b3Length( wish );
	bool capped = false;
	bool bends = false;
	bool about = false;
	bool aboutBrakes = false;
	bool walked = false;
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

			// Steered at a walk, see NB_WALK_STEERED, a humanoid does not wait for its heading with that little of the
			// velocity it wants: it walks on the way it faces, with nbFigureSteering::walkBend of the speed it is steered
			// at and no more than it has that way, while its heading turns to where it is steered, see below. Capped as
			// above it nearly stopped in every turn and sped up again once it faced the new way: its legs lost their
			// feet and hurried after them, a swing shorter than 0.25 s at 68 of 360 turns at a walk, and the pelvis got
			// more than 30 mm below the steady walk at 62 of them.
			// It bends where it walks the way it faces as the turn begins, with NB_WALK_BEND_FROM of the speed of the
			// bend or more: from standing it turns as above.
			float bend = b3ClampFloat( def->walkBend, 0.0f, 1.0f );
			float wanted = b3Length( asked );
			float onward = b3MaxFloat( b3Dot( flat, forward ), 0.0f );
			bool can = bend > 0.0f && f->gaitOn && f->boneCount == nb_humanoidBoneCount && f->onGround;
			// Steered far off the way it faces it turns about instead, see NB_WALK_ABOUT, once it has walked for a while:
			// from standing it turns as above
			bool within = b3Dot( asked, forward ) >= NB_WALK_ABOUT * wanted;
			bool fresh = f->turnCapped == false ||
						 b3Dot( asked, f->turnAsked ) < NB_WALK_TURN_NEW * wanted * b3Length( f->turnAsked );
			if ( fresh )
			{
				f->turnBends = can && within && onward >= NB_WALK_BEND_FROM * bend * wanted;
				f->turnAbout = false;
			}
			f->turnBends = f->turnBends && within;
			f->turnAsked = asked;
			about = can && within == false && f->turnBends == false && f->walkTime >= NB_STOP_WALKED;
			aboutBrakes = about && onward > 0.0f;
			f->turnAbout = can && ( f->turnAbout || about );
			walked = f->turnBends || f->turnAbout;
			capped = true;
			if ( f->turnBends && can )
			{
				float walks = 1.0f - f->gaitRun;
				walks *= 1.0f - b3ClampFloat( ( wanted / scale - NB_WALK_STEERED ) / NB_WALK_STEERED_FADE, 0.0f, 1.0f );
				float ahead = b3MinFloat( bend * wanted, onward );
				wish = b3Add( b3MulSV( 1.0f - walks, wish ), b3MulSV( walks * ahead, forward ) );
				bends = walks > 0.5f;
			}
			else if ( f->turnAbout && can )
			{
				// Turning about, it creeps the new way slower while it still faces away from it, see NB_ABOUT_CREEP
				float walks = 1.0f - f->gaitRun;
				wish = b3MulSV( 1.0f - walks * ( 1.0f - NB_ABOUT_CREEP ), wish );
			}
		}
		asked = wish;
	}
	else if ( f->boneCount == nb_humanoidBoneCount )
	{
		// Not facing where it moves, a humanoid goes across where its legs walk and back against it no faster than a step
		// to the side or back, but for as far across as they walk once turned there, see nbLegCap and nbLegTurnFor: it
		// keeps the direction it wants and gets as fast there as its legs carry it. Other figures have no gait that turns.
		b3Vec3 local = b3InvRotateVector( f->frame.q, wish );
		float speed = sqrtf( local.x * local.x + local.z * local.z );
		if ( speed > 0.0f )
		{
			float most = nbLegCap( f, b3Atan2( local.x, local.z ), speed, f->legTurn, f->legBack );
			wish = b3MulSV( most / speed, wish );
		}
	}
	// The gait plans with the velocity the capsule goes for, see nbSteerFeet, and with its acceleration as fast as it goes
	// for before the legs cap it, see NB_GAIT_PLAN_FROM
	f->wishVelocity = wish;
	f->askedVelocity = asked;
	b3Vec3 change = b3Sub( wish, flat );
	float rate = b3LengthSquared( wish ) > b3LengthSquared( flat ) ? def->acceleration : def->braking;
	rate = b3MaxFloat( rate, 0.0f ) * ( f->onGround ? 1.0f : b3ClampFloat( def->airControl, 0.0f, 1.0f ) );

	// From standing a humanoid gets faster no quicker than nbFigureSteering::startAcceleration. It starts where it wants
	// to go, its gait is not on yet and it goes slower than NB_START_FROM, and it has started once it goes as fast as it
	// wants or wants to go no faster: turning about or switching sides it is not standing, and acceleration decides.
	if ( b3LengthSquared( wish ) <= b3LengthSquared( flat ) )
	{
		f->starting = false;
	}
	else if ( f->gaitOn == false && b3LengthSquared( flat ) < NB_START_FROM * NB_START_FROM * scale * scale )
	{
		f->starting = true;
	}
	if ( f->starting && def->startAcceleration > 0.0f && f->onGround && f->boneCount == nb_humanoidBoneCount )
	{
		rate = b3MinFloat( rate, def->startAcceleration );
	}
	// A turn ends where the velocity it wants is no longer capped for its heading, see nbFigureSteering::walkBend
	f->turnCapped = capped;
	f->turnBends = f->turnBends && capped;
	f->turnAbout = f->turnAbout && capped;
	f->turnSince = capped && walked ? 0.0f : b3MinFloat( f->turnSince + timeStep, 10.0f );

	// At a walk it waits for its feet, see NB_WALK_STEERED: turning, turning about and for a moment after a bend or a
	// turn about, see NB_TURN_HOLD_AFTER, it gets no faster while the leg it stands on loses its foot before the other
	// foot lands, and stopping or braking to turn about, it brakes gently while a foot lands where it aimed
	f->walkTime = f->gaitOn && f->gaitRun <= 0.0f ? f->walkTime + timeStep : 0.0f;
	bool still = b3LengthSquared( wish ) < 0.01f * scale * scale;
	if ( still == false )
	{
		f->walkedTo = f->walkTime;
	}
	f->stops = still && f->walkedTo >= NB_STOP_WALKED && def->stopBraking > 0.0f;
	if ( f->gaitOn && f->boneCount == nb_humanoidBoneCount && f->onGround )
	{
		float walks = 1.0f - f->gaitRun;
		walks *= 1.0f - b3ClampFloat( ( f->steeredSpeed / scale - NB_WALK_STEERED ) / NB_WALK_STEERED_FADE, 0.0f, 1.0f );
		bool after = f->steer.faceVelocity && capped == false && f->turnSince < NB_TURN_HOLD_AFTER;
		bool turning = ( fabsf( nbWrapAngle( f->headingWant - f->heading ) ) > NB_TURN_HOLD_FROM || about || after ) &&
					   fabsf( f->slope ) < NB_TURN_HOLD_LEVEL && f->stairWeight <= 0.0f;
		bool faster = b3LengthSquared( wish ) > b3LengthSquared( flat );
		bool stopping = ( b3LengthSquared( wish ) < 0.01f * scale * scale && f->walkTime >= NB_STOP_WALKED ) || aboutBrakes;
		for ( int k = 0; k < 2 && turning && faster; ++k )
		{
			if ( f->footSwings[k] == false && f->footSwings[1 - k] && f->footOut[k] < f->swingRest[1 - k] )
			{
				rate = b3MinFloat( rate, rate - walks * rate );
			}
		}
		if ( f->stops )
		{
			// Stopping out of a walk it rolls out, see nbFigureSteering::stopBraking
			float gentle = f->haltRate > 0.0f ? f->haltRate : def->stopBraking;
			rate = b3MinFloat( rate, rate + walks * ( gentle - rate ) );
		}
		for ( int k = 0; k < 2 && stopping; ++k )
		{
			if ( f->footSwings[k] && f->swingAlong[k] > NB_STOP_COMMIT )
			{
				rate = b3MinFloat( rate, rate + walks * ( NB_STOP_GENTLE * scale - rate ) );
			}
		}
	}
	float most = rate * timeStep;
	float length = b3Length( change );
	flat = length <= most ? wish : b3MulAdd( flat, most / length, change );

	// Two steered figures that stand animated meet as their capsules: neither goes through the other, neither pushes the
	// other, and their bones are no walls and no ground for each other. Near the capsule of another, this one closes in
	// on it no faster than the gap between them allows, on top of how fast the other moves away: it slows down before
	// the two touch, turns aside where they are not head on, and behind a slower figure it goes on as fast as that.
	// The figures move in their order, so the other is where it went in this update, or where it still stands. Until
	// round 154 the capsule met the bones of the other: walking into a figure that walked toward it, both fell.
	int meetCount = f->meetCount == NB_MEET_ALL ? world->figures.count : f->meetCount;
	for ( int m = 0; m < meetCount; ++m )
	{
		int other = f->meetCount == NB_MEET_ALL ? m : f->meetOthers[m];
		b3Vec3 normal;
		float gap;
		if ( nbMeetCapsule( world, index, f, other, f->frame.p, up, &normal, &gap ) )
		{
			float into = -b3Dot( flat, normal );
			float away = b3MaxFloat( -b3Dot( world->figures.data[other].moveVelocity, normal ), 0.0f );
			float soft = away + NB_MEET_SOFT * b3MaxFloat( gap, 0.0f );
			if ( into > soft )
			{
				flat = b3MulAdd( flat, into - soft, normal );
			}
		}
	}

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
	if ( ground == false && f->onGround && f->gaitOn )
	{
		// Under the capsule only sides too steep to stand on, as between pieces of rubble, or nothing within the reach of
		// the probe, as behind the ridge of a piece the capsule rode up while no foot stood on it: the figure stands on the
		// planted foot nearest to the frame, if that lies below it or within its step above it. With nothing under the
		// capsule only a foot deeper than NB_STEER_FOOT_DEEP of the step holds it, as one higher up stands on the ground
		// the capsule leaves. Over rubble the frame fell between the pieces in one of twenty updates, and the gait stopped
		// and set it down into a crouch. Until round 104 a foot held it only within its step, and only where the probe
		// touched something, and behind a ridge the frame fell up to 33 cm.
		float nearest = FLT_MAX;
		for ( int k = 0; k < 2; ++k )
		{
			b3Vec3 offset = b3SubPos( f->foothold[k], f->frame.p );
			float height = b3Dot( offset, up );
			float across = b3Length( b3MulSub( offset, height, up ) );
			bool holds = height <= step && ( probe.hit || height < -NB_STEER_FOOT_DEEP * step );
			if ( f->footSwings[k] == false && holds && across < nearest )
			{
				nearest = across;
				probe.point = f->foothold[k];
				probe.normal = up;
				ground = true;
			}
		}
	}

	// Ground that moves fast does not carry it, as a standing figure falls from it. Held by a foot with nothing under the
	// capsule, the figure knows no shape under it to ask.
	if ( ground && probe.hit )
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
	int meets[NB_STEER_PLANES];
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
			meets[i] = NB_NULL_INDEX;
		}
		// The capsules of the other figures it meets, each a wall where the two would touch, after what the world gave
		for ( int m = 0; m < meetCount && count < NB_STEER_PLANES; ++m )
		{
			int other = f->meetCount == NB_MEET_ALL ? m : f->meetOthers[m];
			b3Vec3 normal;
			float gap;
			if ( nbMeetCapsule( world, index, f, other, position, up, &normal, &gap ) )
			{
				meets[count] = other;
				planes[count++] = (b3CollisionPlane){ { normal, -gap }, FLT_MAX, 0.0f, false };
			}
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
	// Against the capsule of another figure it keeps the velocity that moves away with, and is not pushed back by it
	for ( int i = 0; i < count; ++i )
	{
		if ( meets[i] != NB_NULL_INDEX && planes[i].push > 0.0f )
		{
			b3Vec3 normal = planes[i].plane.normal;
			float into = -b3Dot( velocity, normal );
			float away = b3MaxFloat( -b3Dot( world->figures.data[meets[i]].moveVelocity, normal ), 0.0f );
			if ( into > away )
			{
				velocity = b3MulAdd( velocity, into - away, normal );
			}
		}
	}
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

	// The heading it wants, or where it moves, see nbTurnToward
	float heading = f->heading;
	float want = f->steer.heading;
	if ( f->steer.faceVelocity )
	{
		b3Quat tilt = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, up );
		b3Vec3 local = b3InvRotateVector( tilt, f->moveVelocity );
		bool moving = local.x * local.x + local.z * local.z > NB_STEER_FACE_SPEED * NB_STEER_FACE_SPEED;
		want = moving ? b3Atan2( local.x, local.z ) : heading;
		// Turning about at a walk, see NB_WALK_ABOUT, it turns to where it is steered at once as well, not once it has
		// braked and moves the new way, but not while a foot is farther into its swing than NB_STOP_COMMIT: that one lands
		// where it aimed first. Until round 175 the heading followed the velocity here and stood for 0.2 s while the
		// figure braked: it faced back after 1.18 s in the mean of the turns, now after 1.05 s, and steered back and forth
		// every second it goes 0.26 instead of 0.20 of its speed where it is steered, see SteerWalkAboutTest. Turning at
		// once whatever the feet do, 1.01 s, the feet got 35 instead of 16 mm under the ground in sum. See NB_ABOUT_CREEP.
		bool aboutTurns = f->turnAbout && b3LengthSquared( f->steer.velocity ) > 0.0f;
		for ( int k = 0; k < 2; ++k )
		{
			aboutTurns = aboutTurns && ( f->footSwings[k] == false || f->swingAlong[k] <= NB_STOP_COMMIT );
		}
		if ( bends || aboutTurns )
		{
			// Walking a bend, it turns to where it is steered, not to where it moves: it moves the way it faces
			b3Vec3 steered = b3InvRotateVector( tilt, f->steer.velocity );
			want = b3Atan2( steered.x, steered.z );
		}
	}
	heading = nbTurnToward( heading, want, &f->headingRate, def, timeStep );

	// The heading turns no farther than the feet on the ground let it: a planted foot turns on the ground with the body
	// by NB_FOOT_PIVOT in a stance at most, and with that used up the heading waits for the feet to step round, see
	// nbSteerFeet. It never turns back for a foot, and the legs turning under a fixed heading are not held. Held back,
	// it keeps the rate it turned at and goes on with it: until round 157 it started again from standing, and turning
	// about at a walk it turned 18 degrees in the first 0.15 s after waiting instead of 34.
	f->headingWant = want;
	f->footHolds[0] = false;
	f->footHolds[1] = false;
	if ( f->gaitOn && f->boneCount == nb_humanoidBoneCount )
	{
		float change = nbWrapAngle( heading - f->heading );
		float asked = change;
		for ( int k = 0; k < 2; ++k )
		{
			if ( f->footSwings[k] )
			{
				continue;
			}
			// turning by change takes the twist of the foot against the legs to twist - change
			float room = NB_FOOT_TWIST + b3MaxFloat( NB_FOOT_PIVOT - f->footPivot[k], 0.0f );
			float twist = nbWrapAngle( f->footHeading[k] - f->legHeading );
			float most = b3MaxFloat( room, fabsf( twist ) );
			if ( change > 0.0f && twist - change < -most )
			{
				change = b3MaxFloat( twist + most, 0.0f );
				f->footHolds[k] = true;
			}
			else if ( change < 0.0f && twist - change > most )
			{
				change = b3MinFloat( twist - most, 0.0f );
				f->footHolds[k] = true;
			}
		}
		if ( change != asked )
		{
			heading = nbWrapAngle( f->heading + change );
		}
	}

	// Facing where it moves, the heading turns no farther than the feet stay uncrossed across it, see NB_FACE_KEEP
	if ( f->steer.faceVelocity && f->gaitOn && f->boneCount == nb_humanoidBoneCount )
	{
		float change = nbWrapAngle( heading - f->heading );
		float room = FLT_MAX;
		if ( change != 0.0f )
		{
			// Where the left foot is from the right one across and along the heading, as nbHoldLegs in figure.c has it
			// for the legs: its angle there, from ahead toward the left, stays between low and pi - low
			b3CosSin legs = b3ComputeCosSin( f->heading );
			b3Vec3 offset = b3SubPos( f->feet[0].plant, f->feet[1].plant );
			float across = offset.x * legs.cosine - offset.z * legs.sine;
			float along = offset.x * legs.sine + offset.z * legs.cosine;
			float apart = sqrtf( across * across + along * along );
			float keep = NB_FACE_KEEP * scale;
			if ( apart > keep )
			{
				float ratio = keep / apart;
				float low = b3Atan2( ratio, sqrtf( b3MaxFloat( 1.0f - ratio * ratio, 0.0f ) ) );
				float angle = b3Atan2( across, along );
				if ( low <= angle && angle <= B3_PI - low )
				{
					room = change > 0.0f ? angle - low : B3_PI - low - angle;
				}
				else
				{
					// Crossed, or less than keep apart across it: it turns only the way that brings the feet apart
					bool down = angle > B3_PI - low || angle < -0.5f * B3_PI;
					room = ( change > 0.0f ) == down ? FLT_MAX : 0.0f;
				}
			}
		}
		if ( fabsf( change ) <= room )
		{
			f->faceHeld = 0.0f;
		}
		else
		{
			f->faceHeld += timeStep;
			if ( f->faceHeld <= NB_FACE_HOLD )
			{
				heading = nbWrapAngle( f->heading + ( change > 0.0f ? room : -room ) );
			}
		}
	}
	f->heading = heading;
	f->frame = nbMakeFrame( world, position, heading );

	// In the air longer than a fall from the height of NB_STEER_FALL_HEIGHT takes, it falls as a ragdoll
	float fallTime = sqrtf( 2.0f * NB_STEER_FALL_HEIGHT * scale / b3MaxFloat( gravity, 0.1f ) );
	return f->airTime <= fallTime;
}
