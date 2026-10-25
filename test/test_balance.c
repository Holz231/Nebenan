// SPDX-License-Identifier: MIT

// Balance of the humanoid, see the roadmap of the game, chapters 26.13 to 26.15 and 28.3 to 28.5: standing still, steps
// that catch pushes from the back, the front and the side, left and right alike, a step that a wall blocks, ground that
// falls away, a knockdown and getting up with a clip without a jump of any bone, the clips of the library and of the
// program, waiting under a slab, a hit and debris that break off getting up, what the figures of a world do and cost, and
// the same bits with any number of threads on every platform.

#include "test_macros.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define STEP_TIME ( 1.0f / 60.0f )
#define SUB_STEPS 4

typedef struct BalanceScene
{
	b3WorldId physicsWorld;
	nbWorldId world;
} BalanceScene;

static BalanceScene CreateBalanceScene( int workerCount )
{
	BalanceScene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	scene.physicsWorld = b3CreateWorld( &worldDef );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = b3ToPos( (b3Vec3){ 0.0f, -1.0f, 0.0f } );
	b3BodyId ground = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.8f;
	b3CreateHullShape( ground, &shapeDef, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.workerCount = workerCount;
	def.asyncStep = false;
	scene.world = nbCreateWorld( &def );
	return scene;
}

static void DestroyBalanceScene( BalanceScene* scene )
{
	nbDestroyWorld( scene->world );
	b3DestroyWorld( scene->physicsWorld );
}

static void StepBalance( BalanceScene* scene, int steps )
{
	for ( int i = 0; i < steps; ++i )
	{
		nbWorld_Step( scene->world, STEP_TIME, SUB_STEPS );
	}
}

// A static box for walls, slabs and floors, or a dynamic one for debris
static b3BodyId CreateBlock( BalanceScene* scene, b3Vec3 center, b3Vec3 halfExtents, bool dynamic )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = b3ToPos( center );
	bodyDef.type = dynamic ? b3_dynamicBody : b3_staticBody;
	b3BodyId body = b3CreateBody( scene->physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.8f;
	b3CreateHullShape( body, &shapeDef, &box.base );
	return body;
}

// A humanoid of 1.8 m and 75 kg with the default motors, standing on ground at a point, with or without balance, settled
// for a second
static nbArticulationId CreateStander( BalanceScene* scene, b3Vec3 ground, bool balance )
{
	nbBoneDef bones[nb_humanoidBoneCount];
	float height = nbMakeHumanoidBones( 1.8f, 75.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbMotorProfile stand = nbDefaultMotorProfile();
	b3Vec3 position = { ground.x, height + ground.y, ground.z };
	nbArticulationId id = nbCreateArticulation( scene->world, &rig, b3ToPos( position ), b3Quat_identity, &stand );
	if ( balance )
	{
		nbBalanceDef def = nbDefaultBalanceDef();
		nbArticulation_EnableBalance( id, &def );
	}
	StepBalance( scene, 60 );
	return id;
}

static b3Vec3 PelvisPoint( nbArticulationId id )
{
	return b3ToVec3( b3Body_GetPosition( nbArticulation_GetBody( id, nb_humanoidPelvis ) ) );
}

static nbBalanceState State( nbArticulationId id )
{
	return nbArticulation_GetBalance( id ).state;
}

// A push at the chest, like a kick or a shove
static void Push( nbArticulationId id, uint32_t number, b3Vec3 impulse )
{
	b3BodyId torso = nbArticulation_GetBody( id, nb_humanoidTorso );
	nbHitDef hit = { 0 };
	hit.id = number;
	hit.bone = nb_humanoidTorso;
	hit.impulse = impulse;
	hit.point = b3Body_GetWorldPoint( torso, (b3Vec3){ 0.0f, 0.3f, 0.0f } );
	nbArticulation_ApplyHit( id, &hit );
}

// Step until the balance is in a state, at most some updates. False if it never got there.
static bool StepUntil( BalanceScene* scene, nbArticulationId id, nbBalanceState state, int updates )
{
	for ( int i = 0; i < updates; ++i )
	{
		if ( State( id ) == state )
		{
			return true;
		}
		StepBalance( scene, 1 );
	}
	return State( id ) == state;
}

// What a figure does after a push: its steps, the first one, whether it fell, and how it ends
typedef struct Reaction
{
	int steps;
	int firstFoot;
	b3Vec3 firstTarget;
	int firstUpdate;
	bool fell;
	nbBalanceState state;
	b3Vec3 pelvis;
} Reaction;

static Reaction Watch( BalanceScene* scene, nbArticulationId id, int updates )
{
	Reaction r = { 0 };
	r.firstFoot = -1;
	nbBalanceState last = State( id );
	int lastCount = 0;
	for ( int i = 0; i < updates; ++i )
	{
		StepBalance( scene, 1 );
		nbBalance info = nbArticulation_GetBalance( id );
		r.fell = r.fell || info.state == nb_balanceFalling;
		if ( info.state == nb_balanceStepping && ( last != nb_balanceStepping || info.stepCount != lastCount ) )
		{
			r.steps += 1;
			if ( r.firstFoot < 0 )
			{
				r.firstFoot = info.stepFoot;
				r.firstTarget = b3ToVec3( info.stepTarget );
				r.firstUpdate = i;
			}
		}
		last = info.state;
		lastCount = info.stepCount;
	}
	r.state = last;
	r.pelvis = PelvisPoint( id );
	return r;
}

// A push on a figure with or without balance, and three seconds after it
static Reaction PushStander( b3Vec3 impulse, bool balance )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, balance );
	Push( id, 1, impulse );
	Reaction r = Watch( &scene, id, 180 );
	DestroyBalanceScene( &scene );
	return r;
}

static const char* StateName( nbBalanceState state )
{
	static const char* names[] = { "off", "standing", "stepping", "falling", "down", "getting up" };
	return names[state];
}

// Off without balance, on for the humanoid, refused for legs it does not fit, and off again
static int BalanceApiTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, false );
	nbBalance off = nbArticulation_GetBalance( id );
	ENSURE( off.state == nb_balanceOff );
	ENSURE( off.stepFoot == -1 );

	// Arms are no legs
	nbBalanceDef wrong = nbDefaultBalanceDef();
	wrong.thighs[0] = nb_humanoidUpperArmLeft;
	wrong.shins[0] = nb_humanoidForearmLeft;
	wrong.feet[0] = nb_humanoidHandLeft;
	ENSURE( nbArticulation_EnableBalance( id, &wrong ) == false );
	ENSURE( State( id ) == nb_balanceOff );

	// A knockdown needs balance
	nbArticulation_Knockdown( id );
	ENSURE( State( id ) == nb_balanceOff );

	nbBalanceDef def = nbDefaultBalanceDef();
	ENSURE( nbArticulation_EnableBalance( id, &def ) );
	ENSURE( State( id ) == nb_balanceStanding );
	StepBalance( &scene, 10 );
	nbBalance on = nbArticulation_GetBalance( id );
	ENSURE( on.state == nb_balanceStanding );
	ENSURE( on.footContact[0] && on.footContact[1] );
	ENSURE( on.supportCount >= 4 );
	ENSURE( on.captureMargin < 0.0f );
	ENSURE( on.stepFoot == -1 );

	ENSURE( nbArticulation_EnableBalance( id, NULL ) );
	ENSURE( State( id ) == nb_balanceOff );

	nbDestroyArticulation( id );
	ENSURE( State( id ) == nb_balanceOff );
	DestroyBalanceScene( &scene );
	return 0;
}

// Standing still, balance keeps the figure on its feet without a step
static int BalanceStandTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
	b3Vec3 start = b3ToVec3( nbArticulation_GetBalance( id ).center );
	float lowest = FLT_MAX;
	float highest = -FLT_MAX;
	for ( int i = 0; i < 180; ++i )
	{
		StepBalance( &scene, 1 );
		ENSURE( State( id ) == nb_balanceStanding );
		float height = PelvisPoint( id ).y;
		lowest = b3MinFloat( lowest, height );
		highest = b3MaxFloat( highest, height );
	}

	b3Vec3 center = b3ToVec3( nbArticulation_GetBalance( id ).center );
	float drift = b3Length( (b3Vec3){ center.x - start.x, 0.0f, center.z - start.z } );
	printf( "standing: pelvis between %.3f and %.3f m, center of mass moved %.1f mm in 3 s\n", lowest, highest, 1000.0f * drift );
	ENSURE( lowest > 0.85f );
	ENSURE( highest < 0.97f );
	ENSURE( drift < 0.02f );

	DestroyBalanceScene( &scene );
	return 0;
}

// Chapters 26.14 and 28.3: pushes from the back, the front and the side that topple a figure without balance are caught
// by steps, and the figure stands again
static int BalanceStepTest( void )
{
	b3Vec3 pushes[3] = { { 0.0f, 0.0f, -60.0f }, { 0.0f, 0.0f, 50.0f }, { 60.0f, 0.0f, 0.0f } };
	const char* names[3] = { "from the front", "from the back", "from the side" };
	for ( int i = 0; i < 3; ++i )
	{
		Reaction without = PushStander( pushes[i], false );
		Reaction with = PushStander( pushes[i], true );
		printf( "push of %.0f N s %s: without balance the pelvis ends at %.2f m, with balance %d steps, %s, pelvis at %.2f m\n",
				b3Length( pushes[i] ), names[i], without.pelvis.y, with.steps, StateName( with.state ), with.pelvis.y );
		ENSURE( without.pelvis.y < 0.5f );
		ENSURE( with.steps >= 1 );
		ENSURE( with.fell == false );
		ENSURE( with.state == nb_balanceStanding );
		ENSURE( with.pelvis.y > 0.8f );
	}
	return 0;
}

// Chapter 28.5 A: the same push from the left and from the right gives the mirrored reaction, and the same push twice the
// same bits
static int BalanceSymmetryTest( void )
{
	Reaction left = PushStander( (b3Vec3){ 65.0f, 0.0f, 0.0f }, true );
	Reaction right = PushStander( (b3Vec3){ -65.0f, 0.0f, 0.0f }, true );
	Reaction again = PushStander( (b3Vec3){ 65.0f, 0.0f, 0.0f }, true );
	printf( "push of 65 N s from the right: foot %d steps to (%.3f %.3f) after %d updates, %d steps, %s\n", left.firstFoot,
			left.firstTarget.x, left.firstTarget.z, left.firstUpdate, left.steps, StateName( left.state ) );
	printf( "push of 65 N s from the left: foot %d steps to (%.3f %.3f) after %d updates, %d steps, %s\n", right.firstFoot,
			right.firstTarget.x, right.firstTarget.z, right.firstUpdate, right.steps, StateName( right.state ) );
	ENSURE( left.fell == false && right.fell == false );
	ENSURE( left.state == nb_balanceStanding && right.state == nb_balanceStanding );
	ENSURE( left.firstFoot == 0 && right.firstFoot == 1 );
	ENSURE( left.firstUpdate == right.firstUpdate );
	ENSURE_SMALL( left.firstTarget.x + right.firstTarget.x, 0.02f );
	ENSURE_SMALL( left.firstTarget.z - right.firstTarget.z, 0.02f );

	ENSURE( memcmp( &left.pelvis, &again.pelvis, sizeof( b3Vec3 ) ) == 0 );
	ENSURE( left.steps == again.steps );
	return 0;
}

// How deep a body reaches into another one, by the separation of their contact points, at most a depth
static float Deepest( b3BodyId body, b3BodyId other, float deepest )
{
	b3ContactData contacts[8];
	int count = b3Body_GetContactData( body, contacts, 8 );
	for ( int c = 0; c < count; ++c )
	{
		b3BodyId a = b3Shape_GetBody( contacts[c].shapeIdA );
		b3BodyId b = b3Shape_GetBody( contacts[c].shapeIdB );
		if ( B3_ID_EQUALS( a, other ) == false && B3_ID_EQUALS( b, other ) == false )
		{
			continue;
		}

		for ( int m = 0; m < contacts[c].manifoldCount; ++m )
		{
			for ( int k = 0; k < contacts[c].manifolds[m].pointCount; ++k )
			{
				deepest = b3MinFloat( deepest, contacts[c].manifolds[m].points[k].separation );
			}
		}
	}
	return deepest;
}

// What a figure does after a push next to a wall that begins at a distance to its left and is this high: its steps, the
// first one, whether a step aimed at the wall, how deep a foot went into the wall, and how it ends
typedef struct WallReaction
{
	Reaction reaction;
	bool stepIntoWall;
	float deepest;
} WallReaction;

static WallReaction PushAtWall( b3Vec3 impulse, float wall, float height )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	b3BodyId block =
		CreateBlock( &scene, (b3Vec3){ wall + 0.1f, 0.5f * height, 0.0f }, (b3Vec3){ 0.1f, 0.5f * height, 1.5f }, false );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
	Push( id, 1, impulse );

	WallReaction r = { 0 };
	r.reaction.firstFoot = -1;
	r.deepest = FLT_MAX;
	nbBalanceState last = State( id );
	int lastCount = 0;
	for ( int i = 0; i < 180; ++i )
	{
		StepBalance( &scene, 1 );
		nbBalance info = nbArticulation_GetBalance( id );
		r.reaction.fell = r.reaction.fell || info.state == nb_balanceFalling;
		if ( info.state == nb_balanceStepping )
		{
			if ( last != nb_balanceStepping || info.stepCount != lastCount )
			{
				r.reaction.steps += 1;
				if ( r.reaction.firstFoot < 0 )
				{
					r.reaction.firstFoot = info.stepFoot;
					r.reaction.firstTarget = b3ToVec3( info.stepTarget );
				}
			}

			// A step lands clear of the wall, the ankle half a foot away
			r.stepIntoWall = r.stepIntoWall || b3ToVec3( info.stepTarget ).x > wall - 0.04f;
		}
		last = info.state;
		lastCount = info.stepCount;

		r.deepest = Deepest( nbArticulation_GetBody( id, nb_humanoidFootLeft ), block, r.deepest );
		r.deepest = Deepest( nbArticulation_GetBody( id, nb_humanoidFootRight ), block, r.deepest );
	}
	r.reaction.state = last;
	r.reaction.pelvis = PelvisPoint( id );

	DestroyBalanceScene( &scene );
	return r;
}

// Chapters 28.5 B and 26.14: a wall beside the figure blocks the step a push needs. No step aims into the wall and no foot
// goes into it. At a wall the figure leans against it, over a low wall it stumbles and falls. In the open the same push is
// caught by a step to where the wall would stand.
static int BalanceBlockedStepTest( void )
{
	b3Vec3 impulse = { 60.0f, 0.0f, 0.0f };
	float wall = 0.32f;
	Reaction free = PushStander( impulse, true );
	WallReaction high = PushAtWall( impulse, wall, 2.0f );
	WallReaction low = PushAtWall( impulse, wall, 0.45f );
	printf(
		"push of 60 N s toward a wall at %.2f m: in the open foot %d steps to x %.2f m and the figure is %s; at a wall of 2 m "
		"%d steps, %s, a foot touches %s; at a wall of 0.45 m %d steps, %s, a foot touches %s\n",
		wall, free.firstFoot, free.firstTarget.x, StateName( free.state ), high.reaction.steps, StateName( high.reaction.state ),
		high.deepest < FLT_MAX ? "it" : "it not", low.reaction.steps, StateName( low.reaction.state ),
		low.deepest < FLT_MAX ? "it" : "it not" );
	if ( high.deepest < FLT_MAX || low.deepest < FLT_MAX )
	{
		printf( "deepest a foot reaches into the wall: %.1f mm and %.1f mm\n", -1000.0f * b3MinFloat( high.deepest, 0.0f ),
				-1000.0f * b3MinFloat( low.deepest, 0.0f ) );
	}
	ENSURE( free.firstFoot == 0 );
	ENSURE( free.firstTarget.x > wall + 0.1f );
	ENSURE( free.fell == false );

	ENSURE( high.stepIntoWall == false );
	ENSURE( high.deepest > -0.01f );

	ENSURE( low.stepIntoWall == false );
	ENSURE( low.deepest > -0.01f );
	ENSURE( low.reaction.fell );
	return 0;
}

// Chapter 26.14: the ground under a standing figure falls away. There is nowhere to step and the figure falls.
static int BalanceFloorGoneTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	b3BodyId floor = CreateBlock( &scene, (b3Vec3){ 0.0f, 0.75f, 0.0f }, (b3Vec3){ 1.0f, 0.25f, 1.0f }, false );
	nbArticulationId id = CreateStander( &scene, (b3Vec3){ 0.0f, 1.0f, 0.0f }, true );
	ENSURE( State( id ) == nb_balanceStanding );
	ENSURE( PelvisPoint( id ).y > 1.8f );

	b3DestroyBody( floor );
	int update = 0;
	while ( update < 60 && State( id ) == nb_balanceStanding )
	{
		StepBalance( &scene, 1 );
		update += 1;
	}
	printf( "floor gone: %s after %.2f s\n", StateName( State( id ) ), (float)update * STEP_TIME );
	ENSURE( State( id ) == nb_balanceFalling );

	DestroyBalanceScene( &scene );
	return 0;
}

// Chapters 26.13, 26.15 and 28.4: a knockdown makes the figure fall with a protective pose, forward and backward. Once it
// lies still on safe ground it gets up, and no bone jumps on the way. The foot that lifts the figure out of the kneel on
// its own sinks up to 2.5 cm into the soft contact of the ground, no bone further.
static int BalanceKnockdownTest( void )
{
	b3Vec3 shoves[2] = { { 0.0f, 0.0f, 20.0f }, { 0.0f, 0.0f, -20.0f } };
	for ( int k = 0; k < 2; ++k )
	{
		BalanceScene scene = CreateBalanceScene( 1 );
		nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
		nbArticulation_Knockdown( id );
		ENSURE( State( id ) == nb_balanceFalling );
		Push( id, 1, shoves[k] );

		b3Vec3 previous[nb_humanoidBoneCount];
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			previous[i] = b3ToVec3( b3Body_GetPosition( nbArticulation_GetBody( id, i ) ) );
		}

		float jump = 0.0f;
		float lowest = FLT_MAX;
		int down = -1;
		int gettingUp = -1;
		int standing = -1;
		bool onBack = false;
		b3Vec3 head = b3Vec3_zero;
		for ( int update = 0; update < 720 && standing < 0; ++update )
		{
			StepBalance( &scene, 1 );
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				b3BodyId body = nbArticulation_GetBody( id, i );
				b3Vec3 p = b3ToVec3( b3Body_GetPosition( body ) );
				jump = b3MaxFloat( jump, b3Length( b3Sub( p, previous[i] ) ) );
				previous[i] = p;
				lowest = b3MinFloat( lowest, b3Body_ComputeAABB( body ).lowerBound.y );
			}

			nbBalance info = nbArticulation_GetBalance( id );
			if ( info.state == nb_balanceDown && down < 0 )
			{
				down = update;
				onBack = info.onBack;
			}
			if ( info.state == nb_balanceGettingUp && gettingUp < 0 )
			{
				gettingUp = update;
				b3Quat pelvis = b3Body_GetRotation( nbArticulation_GetBody( id, nb_humanoidPelvis ) );
				b3Vec3 spine = b3RotateVector( pelvis, b3Vec3_axisY );
				head = b3Normalize( (b3Vec3){ spine.x, 0.0f, spine.z } );
			}
			standing = info.state == nb_balanceStanding && gettingUp >= 0 ? update : standing;
		}

		// It stands and stays standing, facing where its head lay
		Reaction after = Watch( &scene, id, 120 );
		b3Vec3 facing = b3RotateVector( b3Body_GetRotation( nbArticulation_GetBody( id, nb_humanoidPelvis ) ), b3Vec3_axisZ );
		float ahead = b3Dot( b3Normalize( (b3Vec3){ facing.x, 0.0f, facing.z } ), head );
		printf(
			"knockdown %s: lies on its %s after %.2f s, gets up after %.2f s, stands after %.2f s facing %.0f degrees from where "
			"its head lay, a bone moved at most %.3f m in a step and reached %.3f m below the ground\n",
			k == 0 ? "forward" : "backward", onBack ? "back" : "front", (float)down * STEP_TIME, (float)gettingUp * STEP_TIME,
			(float)standing * STEP_TIME, 57.3f * acosf( b3ClampFloat( ahead, -1.0f, 1.0f ) ), jump, -lowest );
		ENSURE( down > 0 );
		ENSURE( gettingUp > down );
		ENSURE( standing > gettingUp );
		ENSURE( onBack == ( k == 1 ) );
		ENSURE( jump < 0.12f );
		ENSURE( lowest > -0.025f );
		ENSURE( after.fell == false );
		ENSURE( after.state == nb_balanceStanding );
		ENSURE( after.pelvis.y > 0.8f );
		ENSURE( ahead > 0.9f );

		DestroyBalanceScene( &scene );
	}
	return 0;
}

// The roadmap B12: the clips a figure gets up with. Those of the library lie first, with the head ahead, and stand last,
// from the back the figure rolls onto its front first. Clips of the program, here those of the library played slower,
// get the figure up as well, and clips that do not fit the rig are refused.
static int BalanceGetUpClipTest( void )
{
	nbBalanceDef def = nbDefaultBalanceDef();
	static nbKeyframe keyframes[2][NB_GETUP_KEYFRAMES];
	static b3Quat rotations[2][NB_GETUP_KEYFRAMES * nb_humanoidBoneCount];
	int counts[2];
	float lengths[2];
	for ( int k = 0; k < 2; ++k )
	{
		counts[k] = nbMakeGetUpClip( &def, nb_humanoidBoneCount, k == 1, keyframes[k], rotations[k], NB_GETUP_KEYFRAMES );
		ENSURE( counts[k] >= 2 && counts[k] <= NB_GETUP_KEYFRAMES );
		ENSURE( keyframes[k][0].time == 0.0f );
		for ( int i = 1; i < counts[k]; ++i )
		{
			ENSURE( keyframes[k][i].time > keyframes[k][i - 1].time );
		}
		lengths[k] = keyframes[k][counts[k] - 1].time;

		b3Quat lying = keyframes[k][0].rotations[nb_humanoidPelvis];
		b3Quat standing = keyframes[k][counts[k] - 1].rotations[nb_humanoidPelvis];
		ENSURE( b3RotateVector( lying, b3Vec3_axisY ).z > 0.99f );
		ENSURE( b3RotateVector( lying, b3Vec3_axisZ ).y * ( k == 0 ? -1.0f : 1.0f ) > 0.99f );
		ENSURE( b3RotateVector( standing, b3Vec3_axisY ).y > 0.99f );
	}
	ENSURE( counts[1] > counts[0] && lengths[1] > lengths[0] );
	ENSURE( nbMakeGetUpClip( &def, nb_humanoidBoneCount, true, keyframes[1], rotations[1], counts[1] - 1 ) == 0 );
	ENSURE( nbMakeGetUpClip( &def, nb_humanoidBoneCount, true, keyframes[1], rotations[1], NB_GETUP_KEYFRAMES ) == counts[1] );

	// A quarter slower, copied: the rotations are cleared before the figure needs them
	const float slower = 1.25f;
	nbClipDef clips[2];
	for ( int k = 0; k < 2; ++k )
	{
		for ( int i = 0; i < counts[k]; ++i )
		{
			keyframes[k][i].time *= slower;
		}
		clips[k] = (nbClipDef){ keyframes[k], counts[k], nb_humanoidBoneCount, false };
		def.getUpClips[k] = clips + k;
	}
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, false );
	ENSURE( nbArticulation_EnableBalance( id, &def ) );
	memset( rotations, 0, sizeof( rotations ) );

	nbArticulation_Knockdown( id );
	Push( id, 1, (b3Vec3){ 0.0f, 0.0f, 20.0f } );
	ENSURE( StepUntil( &scene, id, nb_balanceGettingUp, 600 ) );
	int updates = 0;
	while ( State( id ) == nb_balanceGettingUp && updates < 900 )
	{
		StepBalance( &scene, 1 );
		updates += 1;
	}
	float seconds = (float)updates * STEP_TIME;
	printf( "get-up clips of the library: %d keyframes from the front, %d from the back, %.2f and %.2f s; a quarter slower "
			"the figure stands after %.2f s of getting up\n",
			counts[0], counts[1], lengths[0], lengths[1], seconds );
	ENSURE( State( id ) == nb_balanceStanding );
	ENSURE( seconds >= slower * lengths[0] );
	Reaction after = Watch( &scene, id, 120 );
	ENSURE( after.fell == false && after.state == nb_balanceStanding );

	// A clip of another rig, or of one keyframe, and no speed are refused, and the figure keeps its balance
	nbClipDef wrong = clips[0];
	wrong.boneCount = nb_humanoidBoneCount - 1;
	def.getUpClips[0] = &wrong;
	ENSURE( nbArticulation_EnableBalance( id, &def ) == false );
	wrong = clips[0];
	wrong.keyframeCount = 1;
	ENSURE( nbArticulation_EnableBalance( id, &def ) == false );
	def.getUpClips[0] = NULL;
	def.getUpClips[1] = NULL;
	def.getUpSpeed = 0.0f;
	ENSURE( nbArticulation_EnableBalance( id, &def ) == false );
	ENSURE( State( id ) == nb_balanceStanding );

	DestroyBalanceScene( &scene );
	return 0;
}

// A figure the program places lying lies without a fall and gets up once it can, here on its back as the clip from the
// back begins. The balance tells which clip plays and how far.
static int BalancePlacedLyingTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
	nbBalanceDef def = nbDefaultBalanceDef();
	nbKeyframe keyframes[NB_GETUP_KEYFRAMES];
	b3Quat rotations[NB_GETUP_KEYFRAMES * nb_humanoidBoneCount];
	int count = nbMakeGetUpClip( &def, nb_humanoidBoneCount, true, keyframes, rotations, NB_GETUP_KEYFRAMES );
	float length = keyframes[count - 1].time;
	b3WorldTransform root = { b3ToPos( (b3Vec3){ 0.0f, 0.15f, 0.0f } ), keyframes[0].rotations[nb_humanoidPelvis] };
	nbArticulation_PlacePose( id, root, keyframes[0].rotations );
	nbBalance placed = nbArticulation_GetBalance( id );
	ENSURE( placed.state == nb_balanceDown && placed.getUpTime == 0.0f );

	int lying = 0;
	while ( State( id ) == nb_balanceDown && lying < 300 )
	{
		StepBalance( &scene, 1 );
		lying += 1;
	}
	ENSURE( State( id ) == nb_balanceGettingUp );
	ENSURE( nbArticulation_GetBalance( id ).onBack && nbArticulation_GetBalance( id ).getUpClip == 1 );
	float played = 0.0f;
	bool forward = true;
	int updates = 0;
	while ( State( id ) == nb_balanceGettingUp && updates < 900 )
	{
		StepBalance( &scene, 1 );
		updates += 1;
		nbBalance info = nbArticulation_GetBalance( id );
		if ( info.state == nb_balanceGettingUp )
		{
			forward = forward && info.getUpTime >= played;
			played = info.getUpTime;
		}
	}
	nbBalance stands = nbArticulation_GetBalance( id );
	printf( "placed on its back: lies %.2f s, gets up in %.2f s, its clip played %.2f of %.2f s, %d falls\n",
			(float)lying * STEP_TIME, (float)updates * STEP_TIME, played, length, stands.fallCount );
	ENSURE( stands.state == nb_balanceStanding && stands.getUpTime == 0.0f );
	ENSURE( forward && played > length - 2.0f * STEP_TIME );
	ENSURE( stands.fallCount == placed.fallCount && stands.getUpCount == placed.getUpCount + 1 );

	DestroyBalanceScene( &scene );
	return 0;
}

// Chapters 28.5 E and 26.15: a slab over the lying figure leaves no room to stand. It waits under the slab without pushing
// into it, and gets up once the slab is gone.
static int BalanceSlabTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
	nbArticulation_Knockdown( id );
	ENSURE( StepUntil( &scene, id, nb_balanceDown, 600 ) );

	b3Vec3 pelvis = PelvisPoint( id );
	float bottom = 0.6f;
	b3BodyId slab = CreateBlock( &scene, (b3Vec3){ pelvis.x, bottom + 0.1f, pelvis.z }, (b3Vec3){ 1.0f, 0.1f, 1.5f }, false );
	float top = -FLT_MAX;
	for ( int update = 0; update < 300; ++update )
	{
		StepBalance( &scene, 1 );
		ENSURE( State( id ) == nb_balanceDown );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			top = b3MaxFloat( top, b3Body_ComputeAABB( nbArticulation_GetBody( id, i ) ).upperBound.y );
		}
	}

	b3DestroyBody( slab );
	bool up = StepUntil( &scene, id, nb_balanceGettingUp, 120 );
	bool stands = StepUntil( &scene, id, nb_balanceStanding, 600 );
	printf( "under a slab at %.2f m: down for 5 s, the highest bone at %.3f m; without the slab it %s and %s\n", bottom, top,
			up ? "gets up" : "stays down", stands ? "stands" : "does not stand" );
	ENSURE( top < bottom + 0.01f );
	ENSURE( up );
	ENSURE( stands );

	DestroyBalanceScene( &scene );
	return 0;
}

// Chapter 26.15: a hit while the figure gets up breaks it off, and so does debris that falls on it
static int BalanceAbortTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
	nbArticulation_Knockdown( id );
	ENSURE( StepUntil( &scene, id, nb_balanceGettingUp, 900 ) );
	StepBalance( &scene, 20 );
	ENSURE( State( id ) == nb_balanceGettingUp );

	b3BodyId torso = nbArticulation_GetBody( id, nb_humanoidTorso );
	nbHitDef hit = { 0 };
	hit.id = 2;
	hit.bone = nb_humanoidTorso;
	hit.impulse = (b3Vec3){ 0.0f, 0.0f, -15.0f };
	hit.point = b3Body_GetWorldPoint( torso, (b3Vec3){ 0.0f, 0.2f, 0.0f } );
	ENSURE( nbArticulation_ApplyHit( id, &hit ) );
	StepBalance( &scene, 1 );
	ENSURE( State( id ) == nb_balanceFalling );

	// It lies again, gets up again, and a block falls on it
	ENSURE( StepUntil( &scene, id, nb_balanceGettingUp, 900 ) );
	StepBalance( &scene, 20 );
	ENSURE( State( id ) == nb_balanceGettingUp );
	b3Vec3 pelvis = PelvisPoint( id );
	CreateBlock( &scene, (b3Vec3){ pelvis.x, pelvis.y + 0.8f, pelvis.z }, (b3Vec3){ 0.15f, 0.15f, 0.15f }, true );
	int update = 0;
	while ( update < 60 && State( id ) == nb_balanceGettingUp )
	{
		StepBalance( &scene, 1 );
		update += 1;
	}
	printf( "getting up: a hit breaks it off at once, a falling block after %.2f s\n", (float)update * STEP_TIME );
	ENSURE( State( id ) == nb_balanceFalling );

	DestroyBalanceScene( &scene );
	return 0;
}

// Chapters 27.14 and 32.8: what the figures of a world do and what they cost. One with balance plays a clip, one has
// balance, one has none. The stats count them by state, a knockdown counts a fall, getting up that ends standing a getting
// up, and a hit while getting up one that failed. The time of the figures in the update holds its parts.
static int BalanceStatsTest( void )
{
	BalanceScene scene = CreateBalanceScene( 1 );
	nbArticulationId playing = CreateStander( &scene, (b3Vec3){ -1.5f, 0.0f, 0.0f }, true );
	nbArticulationId id = CreateStander( &scene, b3Vec3_zero, true );
	CreateStander( &scene, (b3Vec3){ 1.5f, 0.0f, 0.0f }, false );

	// The left arm comes up and goes down again, around and around
	b3Quat poses[2][nb_humanoidBoneCount];
	for ( int k = 0; k < 2; ++k )
	{
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			poses[k][i] = b3Quat_identity;
		}
	}
	poses[1][nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.0f );
	nbKeyframe keyframes[3] = { { 0.0f, poses[0] }, { 0.5f, poses[1] }, { 1.0f, poses[0] } };
	nbClipDef clip = { keyframes, 3, nb_humanoidBoneCount, true };
	nbArticulation_PlayClip( playing, &clip, 1.0f );

	nbWorld_ResetProfile( scene.world );
	StepBalance( &scene, 30 );
	nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
	printf( "three figures: %d bones, %d joints, %d awake, %d playing, %d standing, %d without balance, %d motors at the limit, "
			"joints at most %.3f rad off\n",
			stats.boneCount, stats.jointCount, stats.awakeBoneCount, stats.playingCount, stats.stateCounts[nb_balanceStanding],
			stats.stateCounts[nb_balanceOff], stats.saturatedJointCount, stats.largestJointError );
	ENSURE( stats.figureCount == 3 && stats.boneCount == 3 * nb_humanoidBoneCount );
	ENSURE( stats.jointCount == 3 * nb_humanoidBoneCount );
	ENSURE( stats.playingCount == 1 );
	ENSURE( stats.stateCounts[nb_balanceStanding] == 2 && stats.stateCounts[nb_balanceOff] == 1 );
	ENSURE( stats.fallCount == 0 && stats.getUpCount == 0 && stats.failedGetUpCount == 0 );
	ENSURE( stats.saturatedJointCount == 0 && stats.largestJointError < 0.1f );

	// The time of the figures in the update and its parts, each lap inside the one before
	nbProfile profile = nbWorld_GetProfile( scene.world );
	float figures = profile.stages[nb_profileFigures];
	float clips = profile.stages[nb_profileFigureClips];
	float balance = profile.stages[nb_profileFigureBalance];
	float motors = profile.stages[nb_profileFigureMotors];
	printf( "the figures take %.4f ms a step: clips %.4f, balance %.4f, motors %.4f, of an update of %.4f\n", figures, clips,
			balance, motors, profile.stages[nb_profileUpdate] );
	ENSURE( clips > 0.0f && balance > 0.0f && motors > 0.0f );
	ENSURE( clips + balance + motors <= 1.001f * figures + 1.0e-5f );
	ENSURE( figures <= 1.001f * profile.stages[nb_profileUpdate] + 1.0e-5f );

	// Down, getting up, a hit that breaks it off, and getting up to the end
	nbArticulation_Knockdown( id );
	ENSURE( StepUntil( &scene, id, nb_balanceGettingUp, 900 ) );
	StepBalance( &scene, 20 );
	ENSURE( State( id ) == nb_balanceGettingUp );
	Push( id, 7, (b3Vec3){ 0.0f, 0.0f, -15.0f } );
	StepBalance( &scene, 1 );
	ENSURE( State( id ) == nb_balanceFalling );
	ENSURE( StepUntil( &scene, id, nb_balanceGettingUp, 900 ) );
	ENSURE( StepUntil( &scene, id, nb_balanceStanding, 900 ) );
	stats = nbWorld_GetFigureStats( scene.world );
	nbBalance counted = nbArticulation_GetBalance( id );
	printf( "%d falls, %d times up, %d times broken off\n", stats.fallCount, stats.getUpCount, stats.failedGetUpCount );
	ENSURE( stats.fallCount == 2 && stats.getUpCount == 1 && stats.failedGetUpCount == 1 );
	ENSURE( counted.fallCount == 2 && counted.getUpCount == 1 && counted.failedGetUpCount == 1 );

	DestroyBalanceScene( &scene );
	return 0;
}

static uint32_t HashStander( uint32_t hash, nbArticulationId id )
{
	b3WorldTransform transforms[nb_humanoidBoneCount];
	int count = nbArticulation_GetBoneTransforms( id, transforms, nb_humanoidBoneCount );
	for ( int i = 0; i < count; ++i )
	{
		b3Vec3 p = b3ToVec3( transforms[i].p );
		float values[7] = { p.x, p.y, p.z, transforms[i].q.v.x, transforms[i].q.v.y, transforms[i].q.v.z, transforms[i].q.s };
		for ( int k = 0; k < 7; ++k )
		{
			uint32_t bits;
			memcpy( &bits, values + k, sizeof( bits ) );
			hash = ( hash ^ bits ) * 16777619u;
		}
	}
	return ( hash ^ (uint32_t)State( id ) ) * 16777619u;
}

// Two figures with balance, with this many threads: one takes a push and steps, one is knocked down and gets up
static uint32_t RunBalanceScenario( int workerCount )
{
	BalanceScene scene = CreateBalanceScene( workerCount );
	nbArticulationId pushed = CreateStander( &scene, (b3Vec3){ -1.5f, 0.0f, 0.0f }, true );
	nbArticulationId knocked = CreateStander( &scene, (b3Vec3){ 1.5f, 0.0f, 0.0f }, true );
	nbArticulation_Knockdown( knocked );

	uint32_t hash = 2166136261u;
	for ( int step = 0; step < 600; ++step )
	{
		if ( step == 30 )
		{
			Push( pushed, 1, (b3Vec3){ 0.0f, 0.0f, -60.0f } );
		}
		StepBalance( &scene, 1 );
		if ( step % 30 == 29 )
		{
			hash = HashStander( hash, pushed );
			hash = HashStander( hash, knocked );
		}
	}

	DestroyBalanceScene( &scene );
	return hash;
}

// Balance gives the same bits with MSVC, GCC and Clang on x64 and ARM, with one thread and with four. This is the result
// with the pinned Box3D commit. Update it when the results change on purpose, never to make one platform pass.
#define NB_EXPECTED_BALANCE_HASH 0x1a58818au

static int BalanceDeterminismTest( void )
{
	uint32_t hash1 = RunBalanceScenario( 1 );
	uint32_t hash2 = RunBalanceScenario( 1 );
	uint32_t hash4 = RunBalanceScenario( 4 );
	printf( "balance determinism hash: 0x%08x\n", hash1 );
	ENSURE( hash1 == hash2 );
	ENSURE( hash1 == hash4 );
	ENSURE( hash1 == NB_EXPECTED_BALANCE_HASH );
	return 0;
}

int BalanceTest( void );

int BalanceTest( void )
{
	RUN_TEST( BalanceApiTest );
	RUN_TEST( BalanceStandTest );
	RUN_TEST( BalanceStepTest );
	RUN_TEST( BalanceSymmetryTest );
	RUN_TEST( BalanceBlockedStepTest );
	RUN_TEST( BalanceFloorGoneTest );
	RUN_TEST( BalanceKnockdownTest );
	RUN_TEST( BalanceGetUpClipTest );
	RUN_TEST( BalancePlacedLyingTest );
	RUN_TEST( BalanceSlabTest );
	RUN_TEST( BalanceAbortTest );
	RUN_TEST( BalanceStatsTest );
	RUN_TEST( BalanceDeterminismTest );
	return 0;
}
