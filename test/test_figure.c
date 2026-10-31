// SPDX-License-Identifier: MIT

// Figures after the hybrid of Overgrowth, see figure.c and the stages of the roadmap of the game, 27.14 and 28.5: the
// springs of the joints with their torque limit, a figure that stands animated, flinches, is knocked down with the
// velocities its bones had and the hit once, falls as an active ragdoll, lies still and gets up from its front and from
// its back, waits where it has no room, breaks off getting up when it is hit, falls when its ground goes, when debris
// hits it, a blast throws it or a body presses on it, plays a clip, and gives the same bits with any number of threads.

#include "test_macros.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

typedef struct FigureScene
{
	b3WorldId physicsWorld;
	b3BodyId groundBody;
	nbWorldId world;
	nbTaskSystem* pool;
	nbBoneDef bones[nb_humanoidBoneCount];
	nbRigDef rig;
	float standHeight;
} FigureScene;

// Ground of 100 m and a world, with the threads of a pool for more than one
static void CreateFigureScene( FigureScene* scene, int threadCount )
{
	memset( scene, 0, sizeof( FigureScene ) );
	b3WorldDef worldDef = b3DefaultWorldDef();
	if ( threadCount > 1 )
	{
		scene->pool = nbCreateTaskSystem( threadCount - 1 );
		worldDef.workerCount = (uint32_t)threadCount;
		worldDef.enqueueTask = nbEnqueueTask;
		worldDef.finishTask = nbFinishTask;
		worldDef.userTaskContext = scene->pool;
	}
	scene->physicsWorld = b3CreateWorld( &worldDef );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
	b3BodyId groundId = b3CreateBody( scene->physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( groundId, &shapeDef, &box.base );
	scene->groundBody = groundId;

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene->physicsWorld;
	def.workerCount = threadCount;
	def.taskSystem = scene->pool;
	scene->world = nbCreateWorld( &def );

	scene->standHeight = nbMakeHumanoidBones( 1.8f, 74.0f, scene->bones, nb_humanoidBoneCount );
	scene->rig = nbDefaultRigDef();
	scene->rig.bones = scene->bones;
	scene->rig.boneCount = nb_humanoidBoneCount;
}

static void DestroyFigureScene( FigureScene* scene )
{
	nbDestroyWorld( scene->world );
	b3DestroyWorld( scene->physicsWorld );
	if ( scene->pool != NULL )
	{
		nbDestroyTaskSystem( scene->pool );
	}
}

static nbFigureId AddFigure( FigureScene* scene, b3Vec3 position, float heading )
{
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene->rig;
	def.standHeight = scene->standHeight;
	def.position = b3ToPos( position );
	def.heading = heading;
	return nbCreateFigure( scene->world, &def );
}

static void StepFigures( FigureScene* scene, int steps )
{
	for ( int i = 0; i < steps; ++i )
	{
		nbWorld_Step( scene->world, 1.0f / 60.0f, 4 );
	}
}

// Step until the figure is in a state or the steps run out, and return how many steps it took, -1 if never
static int StepUntil( FigureScene* scene, nbFigureId figure, nbFigureState state, int maxSteps )
{
	for ( int i = 0; i < maxSteps; ++i )
	{
		if ( nbFigure_GetState( figure ) == state )
		{
			return i;
		}
		StepFigures( scene, 1 );
	}
	return nbFigure_GetState( figure ) == state ? maxSteps : -1;
}

static b3Vec3 BonePosition( nbFigureId figure, int bone )
{
	return b3ToVec3( b3Body_GetPosition( nbFigure_GetBody( figure, bone ) ) );
}

// A hit on the chest, horizontal, at the height of the sternum
static bool HitChest( nbFigureId figure, uint32_t id, b3Vec3 impulse )
{
	b3BodyId chest = nbFigure_GetBody( figure, nb_humanoidChest );
	nbHitDef hit = { id, nb_humanoidChest, impulse, b3Body_GetWorldPoint( chest, (b3Vec3){ 0.0f, 0.2f, 0.1f } ) };
	return nbFigure_ApplyHit( figure, &hit );
}

// The total momentum of the bones
static b3Vec3 FigureMomentum( nbFigureId figure )
{
	b3Vec3 sum = b3Vec3_zero;
	for ( int i = 0; i < nbFigure_GetBoneCount( figure ); ++i )
	{
		b3BodyId body = nbFigure_GetBody( figure, i );
		if ( b3Body_GetType( body ) == b3_dynamicBody )
		{
			sum = b3MulAdd( sum, b3Body_GetMass( body ), b3Body_GetLinearVelocity( body ) );
		}
	}
	return sum;
}

// A figure stands upright: the pelvis at its stand height and the head above it
static bool StandsUpright( const FigureScene* scene, nbFigureId figure )
{
	b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
	b3Vec3 head = BonePosition( figure, nb_humanoidHead );
	return fabsf( pelvis.y - scene->standHeight ) < 0.03f && head.y > 1.55f && fabsf( head.x - pelvis.x ) < 0.1f &&
		   fabsf( head.z - pelvis.z ) < 0.1f;
}

//----------------------------------------------------------------------------------------------------------------------

// A spring with a torque limit holds what it can and gives way to more: a pendulum on a hinge driven to level, too heavy
// for its limit, hangs below level and its spring works at the limit, with a higher limit it holds level
static int SpringTorqueLimitTest( void )
{
#if defined( B3_HAS_SPRING_TORQUE_LIMITS )
	float angles[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyDef anchorDef = b3DefaultBodyDef();
		b3BodyId anchor = b3CreateBody( worldId, &anchorDef );

		// A rod of 10 kg, a meter long, along x from the hinge: its weight pulls with 49 N m at level
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = (b3Vec3){ 0.5f, 0.0f, 0.0f };
		b3BodyId rod = b3CreateBody( worldId, &bodyDef );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3Capsule capsule = { { -0.45f, 0.0f, 0.0f }, { 0.45f, 0.0f, 0.0f }, 0.05f };
		shapeDef.density = 10.0f / ( B3_PI * 0.05f * 0.05f * 0.9f + 4.0f / 3.0f * B3_PI * 0.05f * 0.05f * 0.05f );
		b3CreateCapsuleShape( rod, &shapeDef, &capsule );

		b3RevoluteJointDef jointDef = b3DefaultRevoluteJointDef();
		jointDef.base.bodyIdA = anchor;
		jointDef.base.bodyIdB = rod;
		jointDef.base.localFrameA = (b3Transform){ b3Vec3_zero, b3Quat_identity };
		jointDef.base.localFrameB = (b3Transform){ { -0.5f, 0.0f, 0.0f }, b3Quat_identity };
		jointDef.enableSpring = true;
		jointDef.hertz = 4.0f;
		jointDef.dampingRatio = 1.0f;
		jointDef.maxSpringTorque = pass == 0 ? 30.0f : 200.0f;
		b3JointId joint = b3CreateRevoluteJoint( worldId, &jointDef );
		ENSURE( b3RevoluteJoint_GetMaxSpringTorque( joint ) == jointDef.maxSpringTorque );

		for ( int i = 0; i < 180; ++i )
		{
			b3World_Step( worldId, 1.0f / 60.0f, 4 );
		}
		angles[pass] = b3RevoluteJoint_GetAngle( joint );
		float torque = fabsf( b3RevoluteJoint_GetSpringTorque( joint ) );
		if ( pass == 0 )
		{
			// At the limit, and the rod sags far below level, where its weight still pulls more than 30 N m
			ENSURE_SMALL( torque - 30.0f, 0.5f );
			ENSURE( angles[0] < -0.5f );
		}
		else
		{
			ENSURE( torque < 200.0f );
		}
		b3DestroyWorld( worldId );
	}
	ENSURE( fabsf( angles[1] ) < 0.2f );
#endif
	return 0;
}

// The clips of the library: the get-up clips lie with the head ahead at first and stand at the height of the idle clip at
// the end, no keyframe sinks into the ground, and the baked keyframes lie at most a fifteenth of a second apart
static int HumanoidClipTest( void )
{
	nbBoneDef bones[nb_humanoidBoneCount];
	float stand = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	ENSURE_SMALL( stand - 0.98f, 1.0e-6f );
	static nbKeyframe keyframes[NB_HUMANOID_CLIP_KEYFRAMES];
	static b3Quat rotations[NB_HUMANOID_CLIP_KEYFRAMES * nb_humanoidBoneCount];
	for ( int which = 0; which < nb_figureClipCount; ++which )
	{
		nbClipDef clip;
		int count = nbMakeHumanoidClip( (nbFigureClip)which, 1.8f, keyframes, rotations, NB_HUMANOID_CLIP_KEYFRAMES, &clip );
		ENSURE( count > 0 && count == clip.keyframeCount && clip.boneCount == nb_humanoidBoneCount );
		ENSURE( nbMakeHumanoidClip( (nbFigureClip)which, 1.8f, keyframes, rotations, 1, &clip ) == ( count == 1 ? 1 : 0 ) );
		nbMakeHumanoidClip( (nbFigureClip)which, 1.8f, keyframes, rotations, NB_HUMANOID_CLIP_KEYFRAMES, &clip );
		bool getUp = which == nb_clipGetUpFront || which == nb_clipGetUpBack;
		ENSURE( clip.moveRoot == getUp );
		ENSURE( clip.loop == ( which == nb_clipIdle || which == nb_clipFall ) );
		if ( getUp == false )
		{
			continue;
		}

		const nbKeyframe* first = keyframes;
		const nbKeyframe* last = keyframes + count - 1;
		ENSURE( first->root.p.y < 0.25f );
		ENSURE( fabsf( last->root.p.y - stand ) < 0.01f && fabsf( b3GetQuatAngle( last->root.q ) ) < 0.05f );
		for ( int k = 0; k < count; ++k )
		{
			ENSURE( k == 0 || keyframes[k].time - keyframes[k - 1].time < 1.0f / 15.0f + 1.0e-4f );

			// The lowest point of every shape, as the figure has it in its frame
			b3Transform world[nb_humanoidBoneCount];
			world[0] = keyframes[k].root;
			float lowest = FLT_MAX;
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				if ( i > 0 )
				{
					b3Transform local = { bones[i].restTransform.p,
										  b3MulQuat( bones[i].restTransform.q, keyframes[k].rotations[i] ) };
					world[i] = b3MulTransforms( world[bones[i].parent], local );
				}
				b3Vec3 points[NB_BONE_POINTS];
				int pointCount = nbBonePoints( bones + i, points );
				float radius = 0.0f;
				if ( pointCount == 0 )
				{
					points[0] = bones[i].start;
					points[1] = bones[i].end;
					pointCount = 2;
					radius = bones[i].radius;
				}
				for ( int p = 0; p < pointCount; ++p )
				{
					lowest = b3MinFloat( lowest, b3TransformPoint( world[i], points[p] ).y - radius );
				}
			}
			if ( lowest <= -0.02f )
			{
				printf( "  clip %d keyframe %d at %.3f s lowest %.3f\n", which, k, keyframes[k].time, lowest );
			}
			ENSURE( lowest > -0.02f );
		}
	}
	return 0;
}

// A figure stands animated in its idle clip: kinematic bones where the clip puts them, no joint for the solver, upright
static int FigureStandTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddFigure( &scene, (b3Vec3){ 1.0f, 0.0f, -2.0f }, 0.5f );
	ENSURE( nbFigure_IsValid( figure ) );
	ENSURE( nbFigure_GetBoneCount( figure ) == nb_humanoidBoneCount );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	ENSURE( B3_IS_NULL( nbFigure_GetJoint( figure, 0 ) ) && B3_IS_NON_NULL( nbFigure_GetJoint( figure, 1 ) ) );

	StepFigures( &scene, 180 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	ENSURE( StandsUpright( &scene, figure ) );
	b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
	ENSURE_SMALL( pelvis.x - 1.0f, 1.0e-3f );
	ENSURE_SMALL( pelvis.z + 2.0f, 1.0e-3f );

	// The bones are where the last update sent them, the joints wait aside
	b3WorldTransform bones[nb_humanoidBoneCount];
	b3WorldTransform targets[nb_humanoidBoneCount];
	ENSURE( nbFigure_GetBoneTransforms( figure, bones, nb_humanoidBoneCount ) == nb_humanoidBoneCount );
	ENSURE( nbFigure_GetTargetTransforms( figure, targets, nb_humanoidBoneCount ) == nb_humanoidBoneCount );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_kinematicBody );
		ENSURE( b3Length( b3SubPos( bones[i].p, targets[i].p ) ) < 0.01f );
	}
	b3Counters counters = b3World_GetCounters( scene.physicsWorld );
	ENSURE( counters.jointCount == nb_humanoidBoneCount - 1 );

	// The figure faces where its heading turns it, as far as the angles of Box3D are exact
	b3Vec3 ahead = b3RotateVector( nbFigure_GetFrame( figure ).q, b3Vec3_axisZ );
	ENSURE_SMALL( ahead.x - sinf( 0.5f ), 5.0e-3f );
	ENSURE( nbFigure_GetUserData( figure ) == NULL );

	nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
	ENSURE( stats.figureCount == 1 && stats.animatedCount == 1 && stats.ragdollCount == 0 );

	nbDestroyFigure( figure );
	ENSURE( nbFigure_IsValid( figure ) == false );
	ENSURE( nbWorld_GetFigureStats( scene.world ).figureCount == 0 );
	DestroyFigureScene( &scene );
	return 0;
}

// A hit below the knockdown impulse turns the trunk of a standing figure away from it, and the trunk straightens again.
// A hit lands once per id.
static int FigureFlinchTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
	StepFigures( &scene, 10 );
	b3Vec3 headBefore = BonePosition( figure, nb_humanoidHead );

	ENSURE( HitChest( figure, 7, (b3Vec3){ 0.0f, 0.0f, -30.0f } ) );
	ENSURE( HitChest( figure, 7, (b3Vec3){ 0.0f, 0.0f, -30.0f } ) == false );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );

	// The trunk turns back as far as the flinch scale says, 0.008 rad per newton second, a tenth of a second later
	float largest = 0.0f;
	float back = 0.0f;
	for ( int i = 0; i < 20; ++i )
	{
		StepFigures( &scene, 1 );
		largest = b3MaxFloat( largest, b3Length( nbFigure_GetInfo( figure ).flinch ) );
		back = b3MaxFloat( back, headBefore.z - BonePosition( figure, nb_humanoidHead ).z );
	}
	nbFigureInfo info = nbFigure_GetInfo( figure );
	printf( "  flinch from 30 N s: %.3f rad, the head %.3f m back\n", largest, back );
	ENSURE( info.hitCount == 1 && info.knockdownCount == 0 );
	ENSURE_SMALL( largest - 0.24f, 0.01f );
	ENSURE( back > 0.08f );

	StepFigures( &scene, 110 );
	info = nbFigure_GetInfo( figure );
	ENSURE( b3Length( info.flinch ) < 0.01f );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	ENSURE( StandsUpright( &scene, figure ) );
	DestroyFigureScene( &scene );
	return 0;
}

// A hard hit that steps can catch, after the roadmap of the game, 26.14 and 28.3, and the balance of the branch
// ragdoll-11: the bones of the figure become dynamic, its joints hold the pose of its clip with the full strength of its
// motors, a hold keeps its pelvis upright, and it catches itself with steps. On its feet all the time, it stands animated
// again where it caught itself and steps its feet back under its body. On the upper chest it catches 60 and 70 N s from
// the front, 80 N s from behind and 60 N s from either side. 100 N s from the front and 80 N s from the side knock it
// down at once, as no step reaches far enough.
static int FigureCatchTest( void )
{
	typedef struct CatchCase
	{
		const char* name;
		b3Vec3 impulse;
		bool falls;
	} CatchCase;
	CatchCase cases[] = {
		{ "60 N s from the front", { 0.0f, 0.0f, -60.0f }, false }, { "70 N s from the front", { 0.0f, 0.0f, -70.0f }, false },
		{ "80 N s from behind", { 0.0f, 0.0f, 80.0f }, false },		{ "60 N s from its right", { 60.0f, 0.0f, 0.0f }, false },
		{ "60 N s from its left", { -60.0f, 0.0f, 0.0f }, false },	{ "100 N s from the front", { 0.0f, 0.0f, -100.0f }, true },
		{ "80 N s from its right", { 80.0f, 0.0f, 0.0f }, true },
	};
	for ( size_t c = 0; c < sizeof( cases ) / sizeof( cases[0] ); ++c )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 30 );
		b3Vec3 start = BonePosition( figure, nb_humanoidPelvis );

		ENSURE( HitChest( figure, 1, cases[c].impulse ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
		ENSURE( nbWorld_GetFigureStats( scene.world ).balancingCount == 1 );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_dynamicBody );
		}

		if ( cases[c].falls )
		{
			int steps = StepUntil( &scene, figure, nb_figureRagdoll, 10 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			printf( "  %s: falls after %d updates without a step\n", cases[c].name, steps );
			ENSURE( 0 < steps && steps <= 3 );
			ENSURE( info.stepCount == 0 && info.knockdownCount == 1 && info.catchCount == 0 );
			DestroyFigureScene( &scene );
			continue;
		}

		// On its feet all the time: the head stays high and the figure never lies
		int steps = 0;
		bool stepped = false;
		float lowestHead = FLT_MAX;
		while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 360 )
		{
			StepFigures( &scene, 1 );
			steps += 1;
			stepped = stepped || nbFigure_GetInfo( figure ).stepping;
			lowestHead = b3MinFloat( lowestHead, BonePosition( figure, nb_humanoidHead ).y );
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
		float moved = b3Length( b3Sub( (b3Vec3){ pelvis.x, 0.0f, pelvis.z }, (b3Vec3){ start.x, 0.0f, start.z } ) );
		printf( "  %s: caught with %d steps in %.2f s, %.2f m from where it stood, the head at least %.2f m high\n",
				cases[c].name, info.stepCount, (float)steps / 60.0f, moved, lowestHead );
		ENSURE( info.state == nb_figureAnimated );
		ENSURE( info.knockdownCount == 0 && info.catchCount == 1 && info.stepCount >= 1 && stepped );
		ENSURE( lowestHead > 1.3f );
		ENSURE( moved > 0.1f );
		ENSURE( nbWorld_GetFigureStats( scene.world ).catchCount == 1 );

		// Standing animated, kinematic, where it caught itself
		StepFigures( &scene, 60 );
		ENSURE( StandsUpright( &scene, figure ) );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_kinematicBody );
		}
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A hard hit knocks the figure down: its bones become dynamic with the velocities they had and the hit acts once, so their
// momentum is the impulse. It falls, lies still, gets up with the clip of the side it lies on and stands animated again,
// without a jump of any bone. Hit from the front it lies on its back, from behind on its front.
static int FigureKnockdownTest( void )
{
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 30 );
		nbWorld_ResetProfile( scene.world );

		// From the front pushes back, along -z, from behind forward. The bones become dynamic, the figure tries to catch
		// itself, but no step catches 120 N s and it falls at once.
		float direction = pass == 0 ? -1.0f : 1.0f;
		b3Vec3 impulse = { 0.0f, 0.0f, 120.0f * direction };
		ENSURE( HitChest( figure, 1, impulse ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
		b3Vec3 momentum = FigureMomentum( figure );
		ENSURE_SMALL( momentum.z - impulse.z, 1.0f );
		ENSURE_SMALL( momentum.y, 1.0f );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_dynamicBody );
		}
		int catching = StepUntil( &scene, figure, nb_figureRagdoll, 10 );
		ENSURE( 0 < catching && catching <= 3 );
		ENSURE( nbFigure_GetInfo( figure ).stepCount == 0 );

		// It falls and lies, its pelvis and its head on the ground within a second
		int fall = 0;
		while ( ( BonePosition( figure, nb_humanoidPelvis ).y > 0.25f || BonePosition( figure, nb_humanoidHead ).y > 0.25f ) &&
				fall < 60 )
		{
			StepFigures( &scene, 1 );
			fall += 1;
		}
		ENSURE( fall < 60 );
		ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );

		// Up with the clip of the side it lies on, the bones moving at most 15 cm a step, about 9 m/s for a hand
		int steps = StepUntil( &scene, figure, nb_figureGettingUp, 300 );
		ENSURE( steps >= 0 );
		int lying = steps;
		nbFigureInfo info = nbFigure_GetInfo( figure );
		ENSURE( info.onBack == ( pass == 0 ) );
		b3WorldTransform before[nb_humanoidBoneCount];
		nbFigure_GetBoneTransforms( figure, before, nb_humanoidBoneCount );
		float largest = 0.0f;
		while ( nbFigure_GetState( figure ) == nb_figureGettingUp && steps < 1200 )
		{
			StepFigures( &scene, 1 );
			steps += 1;
			b3WorldTransform after[nb_humanoidBoneCount];
			nbFigure_GetBoneTransforms( figure, after, nb_humanoidBoneCount );
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				largest = b3MaxFloat( largest, b3Length( b3SubPos( after[i].p, before[i].p ) ) );
				before[i] = after[i];
			}
		}
		printf( "  knocked down from the %s: up after %.2f s, largest move of a bone %.3f m a step\n",
				pass == 0 ? "front" : "back", (float)steps / 60.0f, largest );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		ENSURE( largest < 0.15f );

		StepFigures( &scene, 60 );
		ENSURE( StandsUpright( &scene, figure ) );
		info = nbFigure_GetInfo( figure );
		ENSURE( info.knockdownCount == 1 && info.getUpCount == 1 && info.abortCount == 0 && info.hitCount == 1 );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_kinematicBody );
		}
		nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
		ENSURE( stats.knockdownCount == 1 && stats.getUpCount == 1 && stats.animatedCount == 1 );

		// The profile counts the steps after which the ragdoll was awake, all of its fall and at most those it tried to catch
		// itself and lay
		nbProfile profile = nbWorld_GetProfile( scene.world );
		ENSURE( fall <= profile.ragdollStepCount && profile.ragdollStepCount <= catching + fall + lying );
		ENSURE( profile.maxRagdollCount == 1 );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// Under a slab low over it, a knocked down figure waits for room and does not touch the slab. Once the slab is gone it
// gets up. A hit while it gets up breaks it off, and it gets up again later.
static int FigureGetUpRoomTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.getUpDelay = 2.5f;
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	StepFigures( &scene, 10 );
	nbFigure_Knockdown( figure );
	ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );

	// Until it lies flat, before it may get up
	int fall = 0;
	while ( ( BonePosition( figure, nb_humanoidPelvis ).y > 0.25f || BonePosition( figure, nb_humanoidHead ).y > 0.25f ) &&
			fall < 120 )
	{
		StepFigures( &scene, 1 );
		fall += 1;
	}
	StepFigures( &scene, 20 );
	ENSURE( fall < 120 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );

	// A slab 0.7 m over the ground around where it lies
	b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
	b3BodyDef slabDef = b3DefaultBodyDef();
	slabDef.position = (b3Vec3){ pelvis.x, 0.75f, pelvis.z };
	b3BodyId slab = b3CreateBody( scene.physicsWorld, &slabDef );
	b3BoxHull box = b3MakeBoxHull( 1.5f, 0.05f, 1.5f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( slab, &shapeDef, &box.base );
	StepFigures( &scene, 300 );
	nbFigureInfo info = nbFigure_GetInfo( figure );
	ENSURE( info.state == nb_figureRagdoll && info.wait == nb_waitRoom );
	b3ContactData contacts[16];
	ENSURE( b3Body_GetContactData( slab, contacts, 16 ) == 0 );

	b3DestroyBody( slab );
	ENSURE( StepUntil( &scene, figure, nb_figureGettingUp, 120 ) >= 0 );
	StepFigures( &scene, 20 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureGettingUp );
	ENSURE( HitChest( figure, 2, (b3Vec3){ 15.0f, 0.0f, 0.0f } ) );
	ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );
	info = nbFigure_GetInfo( figure );
	ENSURE( info.abortCount == 1 && info.knockdownCount == 2 );
	ENSURE( StepUntil( &scene, figure, nb_figureAnimated, 900 ) >= 0 );
	ENSURE( nbFigure_GetInfo( figure ).getUpCount == 1 );
	DestroyFigureScene( &scene );
	return 0;
}

// A static box, for walls and ledges
static void AddBlock( FigureScene* scene, b3Vec3 center, b3Vec3 halfExtents )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = center;
	b3BodyId body = b3CreateBody( scene->physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( body, &shapeDef, &box.base );
}

// A hit that a standing figure cannot catch over its feet makes it step, after the roadmap of the game, 26.14 and 28.3. Hit
// on its right side it steps left with the left foot, the right one held where it stands, then the right one after it,
// and stands where the push carried it. A kick from the front makes it step back, from less than a push to its side, and
// from behind the same push does not make it step at all.
static int FigureStepTest( void )
{
	// The impulse, the direction it pushes in, along x or z, and how far the body goes there
	const float impulses[4] = { 35.0f, 30.0f, 30.0f, 20.0f };
	const b3Vec3 pushes[4] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } };
	const bool steps[4] = { true, true, false, false };
	for ( int pass = 0; pass < 4; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 10 );
		b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
		b3Vec3 left = BonePosition( figure, nb_humanoidFootLeft );
		b3Vec3 right = BonePosition( figure, nb_humanoidFootRight );

		ENSURE( HitChest( figure, 1, b3MulSV( impulses[pass], pushes[pass] ) ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		ENSURE( nbFigure_GetInfo( figure ).stepping == steps[pass] );

		// The body moves as far as the capture point lies out, the impulse over the mass and the frequency of a pendulum as
		// long as the stand height. The foot that stands does not slide while the other one is in the air.
		float distance = impulses[pass] / 74.0f / sqrtf( 9.8f / scene.standHeight );
		float slide = 0.0f;
		float largest = 0.0f;
		b3WorldTransform before[nb_humanoidBoneCount];
		nbFigure_GetBoneTransforms( figure, before, nb_humanoidBoneCount );
		for ( int i = 0; i < 90; ++i )
		{
			StepFigures( &scene, 1 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			ENSURE( info.state == nb_figureAnimated );
			if ( pass == 0 && info.stepCount == 0 )
			{
				slide = b3MaxFloat( slide, b3Length( b3Sub( BonePosition( figure, nb_humanoidFootRight ), right ) ) );
			}
			b3WorldTransform after[nb_humanoidBoneCount];
			nbFigure_GetBoneTransforms( figure, after, nb_humanoidBoneCount );
			for ( int b = nb_humanoidThighLeft; b < nb_humanoidBoneCount; ++b )
			{
				largest = b3MaxFloat( largest, b3Length( b3SubPos( after[b].p, before[b].p ) ) );
				before[b] = after[b];
			}
		}

		nbFigureInfo info = nbFigure_GetInfo( figure );
		b3Vec3 moved = b3Sub( BonePosition( figure, nb_humanoidPelvis ), pelvis );
		printf( "  pushed by %.0f N s along (%.0f, %.0f): %d steps, the body %.3f m on, the legs %.3f m a step at most\n",
				impulses[pass], pushes[pass].x, pushes[pass].z, info.stepCount, b3Length( moved ), largest );
		ENSURE( info.stepping == false && info.knockdownCount == 0 && info.hitCount == 1 );
		if ( steps[pass] )
		{
			ENSURE( info.stepCount == 2 );
			ENSURE_SMALL( b3Dot( moved, pushes[pass] ) - distance, 0.01f );
			ENSURE( b3Length( b3MulSub( moved, b3Dot( moved, pushes[pass] ), pushes[pass] ) ) < 1.0e-3f );

			// Both feet where they stood, carried along, at the same height. A knee swings ahead as its foot lifts, at about
			// 2.3 m/s.
			ENSURE( b3Length( b3Sub( b3Sub( BonePosition( figure, nb_humanoidFootLeft ), left ), moved ) ) < 2.0e-3f );
			ENSURE( b3Length( b3Sub( b3Sub( BonePosition( figure, nb_humanoidFootRight ), right ), moved ) ) < 2.0e-3f );
			ENSURE( largest < 0.05f );
		}
		else
		{
			ENSURE( info.stepCount == 0 );
			ENSURE( b3Length( moved ) < 1.0e-3f );
		}
		if ( pass == 0 )
		{
			ENSURE( slide < 1.0e-4f );
		}
		ENSURE( StandsUpright( &scene, figure ) );
		ENSURE( nbWorld_GetFigureStats( scene.world ).stepCount == info.stepCount );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A step needs room and ground, see 26.14 and 28.5 (B): a figure kicked toward a wall close behind it falls against the
// wall instead of stepping into it, and pushed away from the wall it steps. A figure on a ledge pushed over its edge falls, a
// figure pushed again and again while it steps falls once the push would carry it beyond its reach, and a figure that
// does not step only flinches.
static int FigureStepBlockedTest( void )
{
	for ( int pass = 0; pass < 5; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure;
		if ( pass < 2 )
		{
			// A wall 16 cm behind the heels, along x, closer than a step back would go
			AddBlock( &scene, (b3Vec3){ 0.0f, 1.5f, -0.25f }, (b3Vec3){ 2.0f, 1.5f, 0.05f } );
			figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		}
		else if ( pass == 2 )
		{
			// On a block 1 m high, its left foot 5 cm from the edge
			AddBlock( &scene, (b3Vec3){ -1.0f, 0.5f, 0.0f }, (b3Vec3){ 1.2f, 0.5f, 1.2f } );
			figure = AddFigure( &scene, (b3Vec3){ 0.05f, 1.0f, 0.0f }, 0.0f );
		}
		else if ( pass == 3 )
		{
			figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		}
		else
		{
			nbFigureDef def = nbDefaultFigureDef();
			def.rig = &scene.rig;
			def.standHeight = scene.standHeight;
			def.stepReach = 0.0f;
			figure = nbCreateFigure( scene.world, &def );
		}
		StepFigures( &scene, 10 );

		if ( pass == 0 )
		{
			// Kicked toward the wall it cannot step back, it tries to catch itself with the hit, falls against the wall
			// without a step and lies before it
			ENSURE( HitChest( figure, 1, (b3Vec3){ 0.0f, 0.0f, -30.0f } ) );
			ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
			ENSURE( StepUntil( &scene, figure, nb_figureRagdoll, 60 ) > 0 );
			ENSURE( nbFigure_GetInfo( figure ).stepCount == 0 );
			StepFigures( &scene, 120 );
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				ENSURE( BonePosition( figure, i ).z > -0.2f );
			}
		}
		else if ( pass == 1 )
		{
			// Pushed away from it, it steps
			ENSURE( HitChest( figure, 1, (b3Vec3){ 0.0f, 0.0f, 40.0f } ) );
			StepFigures( &scene, 60 );
			ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
			ENSURE( nbFigure_GetInfo( figure ).stepCount == 2 );
		}
		else if ( pass == 2 )
		{
			// Over the edge the step finds no ground: the figure catches itself with dynamic bones instead, without a step,
			// its foot stays on the block, and it stands animated again
			ENSURE( HitChest( figure, 1, (b3Vec3){ 35.0f, 0.0f, 0.0f } ) );
			ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
			StepFigures( &scene, 1 );
			ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
			ENSURE( StepUntil( &scene, figure, nb_figureAnimated, 120 ) > 0 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			ENSURE( info.stepCount == 0 && info.catchCount == 1 && info.knockdownCount == 0 );
			ENSURE( BonePosition( figure, nb_humanoidFootLeft ).y > 1.0f );
		}
		else if ( pass == 3 )
		{
			// Three pushes the same way, a twentieth of a second apart: the third would carry it beyond the reach of its
			// steps, and it catches itself with dynamic bones or falls
			ENSURE( HitChest( figure, 1, (b3Vec3){ 45.0f, 0.0f, 0.0f } ) );
			StepFigures( &scene, 3 );
			ENSURE( HitChest( figure, 2, (b3Vec3){ 45.0f, 0.0f, 0.0f } ) );
			ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
			StepFigures( &scene, 3 );
			ENSURE( HitChest( figure, 3, (b3Vec3){ 45.0f, 0.0f, 0.0f } ) );
			ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
			StepFigures( &scene, 240 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			printf( "  three pushes of 45 N s: %s, %d steps\n", info.knockdownCount > 0 ? "falls" : "catches itself",
					info.stepCount );
			ENSURE( info.knockdownCount + info.catchCount == 1 );
		}
		else
		{
			// Without a reach it flinches and stands where it stood
			b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
			ENSURE( HitChest( figure, 1, (b3Vec3){ 45.0f, 0.0f, 0.0f } ) );
			StepFigures( &scene, 60 );
			ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
			ENSURE( nbFigure_GetInfo( figure ).stepCount == 0 );
			ENSURE( b3Length( b3Sub( BonePosition( figure, nb_humanoidPelvis ), pelvis ) ) < 1.0e-3f );
		}
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A box, turned
static void AddTurnedBlock( FigureScene* scene, b3Vec3 center, b3Quat rotation, b3Vec3 halfExtents )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = center;
	bodyDef.rotation = rotation;
	b3BodyId body = b3CreateBody( scene->physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( body, &shapeDef, &box.base );
}

// How far the corners of the sole of a foot of the humanoid lie from a plane, the most and the least
static void SoleGaps( nbFigureId figure, int foot, b3Vec3 point, b3Vec3 normal, float* lowest, float* highest )
{
	b3BodyId body = nbFigure_GetBody( figure, foot );
	*lowest = FLT_MAX;
	*highest = -FLT_MAX;
	for ( int k = 0; k < 4; ++k )
	{
		b3Vec3 corner = { k & 1 ? 0.045f : -0.045f, k & 2 ? 0.22f : -0.04f, -0.06f };
		float gap = b3Dot( b3Sub( b3ToVec3( b3Body_GetWorldPoint( body, corner ) ), point ), normal );
		*lowest = b3MinFloat( *lowest, gap );
		*highest = b3MaxFloat( *highest, gap );
	}
}

// The feet of a standing figure stand on the ground under them, after the roadmap of the game, 26.5 and 28.3: with its
// left foot on a slab 10 cm high the left knee bends, on a slope across and along it both soles lie on the slope and the
// root goes down with the lower foot, a foot that a clip lifts stays where the clip puts it, and a step to catch a hit
// goes up onto a slab.
static int FigureFeetTest( void )
{
	for ( int pass = 0; pass < 5; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		b3Vec3 ground = b3Vec3_zero;
		b3Vec3 normal = b3Vec3_axisY;
		if ( pass == 0 )
		{
			// Under the left foot alone
			AddBlock( &scene, (b3Vec3){ 0.16f, 0.05f, 0.1f }, (b3Vec3){ 0.14f, 0.05f, 0.3f } );
		}
		else if ( pass == 1 || pass == 2 )
		{
			// A slope 30 cm over the ground where the figure stands, rising 0.2 rad toward the left, or ahead
			b3Quat slope = pass == 1 ? b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.2f ) : b3MakeQuatFromAxisAngle( b3Vec3_axisX, -0.2f );
			ground = (b3Vec3){ 0.0f, 0.3f, 0.0f };
			AddTurnedBlock( &scene, b3Add( ground, b3RotateVector( slope, (b3Vec3){ 0.0f, -1.0f, 0.0f } ) ), slope,
							(b3Vec3){ 5.0f, 1.0f, 5.0f } );
			normal = b3RotateVector( slope, b3Vec3_axisY );
		}
		else if ( pass == 4 )
		{
			// Left of the figure, where a push to the left takes its left foot
			AddBlock( &scene, (b3Vec3){ 0.6f, 0.04f, 0.1f }, (b3Vec3){ 0.45f, 0.04f, 0.4f } );
		}
		nbFigureId figure = AddFigure( &scene, ground, 0.0f );

		if ( pass == 3 )
		{
			// The left knee up, the foot 22 cm over the ground
			b3Quat poses[2 * nb_humanoidBoneCount];
			nbKeyframe keys[2];
			for ( int k = 0; k < 2; ++k )
			{
				for ( int i = 0; i < nb_humanoidBoneCount; ++i )
				{
					poses[k * nb_humanoidBoneCount + i] = b3Quat_identity;
				}
				poses[k * nb_humanoidBoneCount + nb_humanoidThighLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.8f );
				poses[k * nb_humanoidBoneCount + nb_humanoidShinLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -1.2f );
				keys[k] = (nbKeyframe){ (float)k, poses + k * nb_humanoidBoneCount, b3Transform_identity };
			}
			nbClipDef clip = { keys, 2, nb_humanoidBoneCount, true, false };
			nbFigure_PlayClip( figure, &clip, 1.0f );
		}
		StepFigures( &scene, 60 );
		if ( pass == 4 )
		{
			ENSURE( HitChest( figure, 1, (b3Vec3){ 35.0f, 0.0f, 0.0f } ) );
			StepFigures( &scene, 60 );
			ENSURE( nbFigure_GetInfo( figure ).stepCount == 2 );
		}
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );

		b3Vec3 left = BonePosition( figure, nb_humanoidFootLeft );
		b3Vec3 right = BonePosition( figure, nb_humanoidFootRight );
		b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
		float lowLeft, highLeft, lowRight, highRight;
		SoleGaps( figure, nb_humanoidFootLeft, ground, normal, &lowLeft, &highLeft );
		SoleGaps( figure, nb_humanoidFootRight, ground, normal, &lowRight, &highRight );
		printf( "  pass %d: ankles at %.3f and %.3f m, the pelvis at %.3f m, the soles %.3f to %.3f and %.3f to %.3f m over the "
				"ground\n",
				pass, left.y, right.y, pelvis.y, lowLeft, highLeft, lowRight, highRight );
		if ( pass == 0 )
		{
			ENSURE_SMALL( left.y - right.y - 0.1f, 3.0e-3f );
			ENSURE_SMALL( pelvis.y - scene.standHeight, 3.0e-3f );
			ENSURE( BonePosition( figure, nb_humanoidShinLeft ).z > BonePosition( figure, nb_humanoidShinRight ).z + 0.05f );
		}
		else if ( pass == 1 || pass == 2 )
		{
			// Both soles flat on the slope, and across it the root down as far as the lower foot goes down
			ENSURE( lowLeft > -3.0e-3f && highLeft < 3.0e-3f && lowRight > -3.0e-3f && highRight < 3.0e-3f );
			ENSURE( pass == 2 || pelvis.y < ground.y + scene.standHeight - 0.015f );
			ENSURE( pass == 1 || fabsf( pelvis.y - ground.y - scene.standHeight ) < 3.0e-3f );
		}
		else if ( pass == 3 )
		{
			ENSURE( left.y > 0.2f );
			ENSURE_SMALL( pelvis.y - scene.standHeight, 3.0e-3f );
			ENSURE( lowRight > -3.0e-3f && highRight < 3.0e-3f );
		}
		else
		{
			ENSURE_SMALL( left.y - right.y - 0.08f, 3.0e-3f );
			ENSURE( lowRight > -3.0e-3f && highRight < 3.0e-3f );
		}
		DestroyFigureScene( &scene );
	}
	return 0;
}

// Without room to get up where it lies, a figure moves aside or turns, after the roadmap of the game, 26.15: knocked down,
// a table comes over its head, and it moves back along its spine and gets up. With a wall behind its feet it turns on
// the ground, and with walls at its sides as well it waits.
static int FigureRoomTest( void )
{
	for ( int pass = 0; pass < 3; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureDef def = nbDefaultFigureDef();
		def.rig = &scene.rig;
		def.standHeight = scene.standHeight;
		def.getUpDelay = 2.5f;
		nbFigureId figure = nbCreateFigure( scene.world, &def );
		StepFigures( &scene, 10 );
		nbFigure_Knockdown( figure );
		StepFigures( &scene, 100 );
		ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );

		// A table 40 to 50 cm high from 30 cm ahead of the pelvis along the spine, where the figure would kneel
		b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
		b3Vec3 spine = b3RotateVector( b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidPelvis ) ), b3Vec3_axisY );
		b3Vec3 ahead = b3Normalize( (b3Vec3){ spine.x, 0.0f, spine.z } );
		b3Vec3 side = b3Cross( b3Vec3_axisY, ahead );
		b3Quat turn = b3MakeQuatFromAxisAngle( b3Vec3_axisY, b3Atan2( ahead.x, ahead.z ) );
		b3Vec3 floor = { pelvis.x, 0.0f, pelvis.z };
		AddTurnedBlock( &scene, b3Add( b3Add( floor, (b3Vec3){ 0.0f, 0.45f, 0.0f } ), b3MulSV( 1.3f, ahead ) ), turn,
						(b3Vec3){ 1.5f, 0.05f, 1.0f } );
		if ( pass >= 1 )
		{
			// A wall 25 cm behind the feet
			AddTurnedBlock( &scene, b3Add( b3Add( floor, (b3Vec3){ 0.0f, 0.5f, 0.0f } ), b3MulSV( -1.25f, ahead ) ), turn,
							(b3Vec3){ 2.0f, 0.5f, 0.05f } );
		}
		if ( pass == 2 )
		{
			// And walls 45 cm to both sides
			for ( int k = -1; k <= 1; k += 2 )
			{
				AddTurnedBlock( &scene, b3Add( b3Add( floor, (b3Vec3){ 0.0f, 0.5f, 0.0f } ), b3MulSV( 0.45f * (float)k, side ) ),
								turn, (b3Vec3){ 0.05f, 0.5f, 1.2f } );
			}
		}

		int shifting = 0;
		for ( int i = 0; i < 900; ++i )
		{
			StepFigures( &scene, 1 );
			shifting += nbFigure_GetInfo( figure ).shifting ? 1 : 0;
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		b3Vec3 moved = b3Sub( BonePosition( figure, nb_humanoidPelvis ), pelvis );
		printf( "  pass %d: %d moves in %.2f s, %d get-ups, the pelvis %.2f m along the spine and %.2f m across\n", pass,
				info.shiftCount, (float)shifting / 60.0f, info.getUpCount, b3Dot( moved, ahead ), b3Dot( moved, side ) );
		if ( pass == 0 )
		{
			// Back 60 cm along its spine, at 0.5 m/s
			ENSURE( info.state == nb_figureAnimated && info.getUpCount == 1 && info.shiftCount == 1 );
			ENSURE( shifting >= 70 && shifting <= 74 );
		}
		else if ( pass == 1 )
		{
			// A quarter turn in 0.79 s
			ENSURE( info.state == nb_figureAnimated && info.getUpCount == 1 && info.shiftCount == 1 );
			ENSURE( shifting >= 46 && shifting <= 49 );
		}
		else
		{
			ENSURE( info.state == nb_figureRagdoll && info.wait == nb_waitRoom && info.shiftCount == 0 );
		}
		ENSURE( nbWorld_GetFigureStats( scene.world ).shiftCount == info.shiftCount );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A ragdoll that comes to rest standing on its feet wakes up standing, as WakeUp of Overgrowth tells standing from falling,
// see the roadmap of the game 26.15. Its right arm reaches ahead into a tight tunnel of four fixed blocks, as the arm of
// figure 8 of the benchmark caught in a crack of its wall: it hangs on the arm with its feet on the ground and its pelvis
// too high over the ground to get up from it. It wakes up where its feet stand, straight to the stand clip without a
// get-up clip, without a jump of a bone, and stands. A figure that lies is no figure that stands: knocked down it gets
// up with its clip, see FigureKnockdownTest.
static int FigureWakeStandingTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );

	// The stand clip with the right arm ahead, so the figure starts as a ragdoll in that pose
	b3Quat rotations[nb_humanoidBoneCount];
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		rotations[i] = b3Quat_identity;
	}
	rotations[nb_humanoidUpperArmRight] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.5f );
	nbKeyframe key = { 0.0f, rotations, b3Transform_identity };
	nbClipDef idle = { &key, 1, nb_humanoidBoneCount, true, false };
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.position = b3ToPos( b3Vec3_zero );
	def.startAsRagdoll = true;
	def.clips[nb_clipIdle] = &idle;
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );

	// The tunnel along the forearm: a block over it, under it and at each side, each 5 mm into it
	b3AABB arm = b3Body_ComputeAABB( nbFigure_GetBody( figure, nb_humanoidForearmRight ) );
	b3Vec3 center = b3MulSV( 0.5f, b3Add( arm.lowerBound, arm.upperBound ) );
	float half = 0.5f * ( arm.upperBound.z - arm.lowerBound.z );
	float thick = 0.1f;
	float press = 0.005f;
	b3Vec3 positions[4] = {
		{ center.x, arm.upperBound.y + thick - press, center.z },
		{ center.x, arm.lowerBound.y - thick + press, center.z },
		{ arm.upperBound.x + thick - press, center.y, center.z },
		{ arm.lowerBound.x - thick + press, center.y, center.z },
	};
	b3Vec3 halves[4] = { { 0.3f, thick, half }, { 0.3f, thick, half }, { thick, 0.3f, half }, { thick, 0.3f, half } };
	for ( int k = 0; k < 4; ++k )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = b3ToPos( positions[k] );
		b3BodyId block = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3BoxHull box = b3MakeBoxHull( halves[k].x, halves[k].y, halves[k].z );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3CreateHullShape( block, &shapeDef, &box.base );
	}

	// It sags onto the arm and comes to rest with its pelvis high, then wakes up standing
	int steps = 0;
	float lowest = FLT_MAX;
	while ( nbFigure_GetState( figure ) == nb_figureRagdoll && steps < 600 )
	{
		StepFigures( &scene, 1 );
		steps += 1;
		lowest = b3MinFloat( lowest, BonePosition( figure, nb_humanoidPelvis ).y );
	}
	nbFigureInfo info = nbFigure_GetInfo( figure );
	printf( "  hung on its arm, the pelvis at least %.2f m high, woke up standing after %.2f s\n", lowest, (float)steps / 60.0f );
	ENSURE( info.state == nb_figureAnimated );
	ENSURE( info.getUpCount == 1 && info.knockdownCount == 0 && info.wait == nb_waitNone );
	ENSURE( lowest > 0.6f );
	ENSURE( steps < 180 );

	// Kinematic at once, the stand clip blends in, the legs too, and the feet step under the body, no bone faster than
	// 3 m/s: without the legs blending in a knee the slack ragdoll bent in jumped 11 cm in a step
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_kinematicBody );
	}
	b3WorldTransform before[nb_humanoidBoneCount];
	nbFigure_GetBoneTransforms( figure, before, nb_humanoidBoneCount );
	float largest = 0.0f;
	for ( int step = 0; step < 90; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		b3WorldTransform after[nb_humanoidBoneCount];
		nbFigure_GetBoneTransforms( figure, after, nb_humanoidBoneCount );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			largest = b3MaxFloat( largest, b3Length( b3SubPos( after[i].p, before[i].p ) ) );
			before[i] = after[i];
		}
	}
	printf( "  stood up, the largest move of a bone %.3f m a step\n", largest );
	ENSURE( largest < 0.05f );
	ENSURE( StandsUpright( &scene, figure ) );
	DestroyFigureScene( &scene );
	return 0;
}

// A figure on a box falls when the box goes, and does not hang in the air
static int FigureGroundTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	b3BodyDef boxDef = b3DefaultBodyDef();
	boxDef.position = (b3Vec3){ 0.0f, 0.5f, 0.0f };
	b3BodyId stand = b3CreateBody( scene.physicsWorld, &boxDef );
	b3BoxHull box = b3MakeBoxHull( 0.6f, 0.5f, 0.6f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( stand, &shapeDef, &box.base );

	nbFigureId figure = AddFigure( &scene, (b3Vec3){ 0.0f, 1.0f, 0.0f }, 0.0f );
	StepFigures( &scene, 60 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	ENSURE_SMALL( BonePosition( figure, nb_humanoidPelvis ).y - ( 1.0f + scene.standHeight ), 0.01f );

	b3DestroyBody( stand );
	StepFigures( &scene, 2 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );
	StepFigures( &scene, 90 );
	ENSURE( BonePosition( figure, nb_humanoidPelvis ).y < 0.5f );
	DestroyFigureScene( &scene );
	return 0;
}

// Debris that hits a standing figure hard knocks it down, a light piece makes it flinch, a heavy body that comes to rest
// on it presses it down, and a blast throws it as it throws the debris
static int FigureWorldTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId light = AddFigure( &scene, (b3Vec3){ -3.0f, 0.0f, 0.0f }, 0.0f );
	nbFigureId heavy = AddFigure( &scene, (b3Vec3){ 0.0f, 0.0f, 0.0f }, 0.0f );
	nbFigureId pressed = AddFigure( &scene, (b3Vec3){ 3.0f, 0.0f, 0.0f }, 0.0f );
	nbFigureId blasted = AddFigure( &scene, (b3Vec3){ 6.0f, 0.0f, 0.0f }, 0.0f );
	nbFigureId far = AddFigure( &scene, (b3Vec3){ 12.0f, 0.0f, 0.0f }, 0.0f );
	StepFigures( &scene, 10 );

	// A brick of 2 kg and a block of 40 kg dropped from 1.5 m over the heads, and a block of 80 kg set down on the head,
	// 5 mm over it: its hit is too light, its weight knocks the figure down
	b3Vec3 drops[3] = { { -3.0f, 3.2f, 0.02f }, { 0.0f, 3.2f, 0.02f }, { 3.0f, 1.955f, 0.02f } };
	float masses[3] = { 2.0f, 40.0f, 80.0f };
	for ( int k = 0; k < 3; ++k )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = drops[k];
		b3BodyId body = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3BoxHull box = b3MakeBoxHull( 0.15f, 0.15f, 0.15f );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = masses[k] / ( 0.3f * 0.3f * 0.3f );
		shapeDef.enableHitEvents = true;
		b3CreateHullShape( body, &shapeDef, &box.base );
	}

	nbImpactDef blast = { 0 };
	blast.point = b3ToPos( (b3Vec3){ 7.0f, 1.0f, 0.0f } );
	blast.radius = 1.3f;
	blast.explosionImpulse = 480.0f;
	nbWorld_ApplyImpact( scene.world, &blast );
	ENSURE( nbFigure_GetState( blasted ) == nb_figureBalancing );
	ENSURE( nbFigure_GetState( far ) == nb_figureAnimated );
	ENSURE( b3Length( FigureMomentum( blasted ) ) > 50.0f );

	StepFigures( &scene, 90 );
	printf( "  blasted: %d knockdowns, heavy %d (state %d), pressed %d (state %d)\n", nbFigure_GetInfo( blasted ).knockdownCount,
			nbFigure_GetInfo( heavy ).knockdownCount, (int)nbFigure_GetState( heavy ), nbFigure_GetInfo( pressed ).knockdownCount,
			(int)nbFigure_GetState( pressed ) );
	ENSURE( nbFigure_GetInfo( blasted ).knockdownCount == 1 );
	ENSURE( nbFigure_GetState( light ) == nb_figureAnimated );
	ENSURE( nbFigure_GetInfo( light ).hitCount >= 1 );
	ENSURE( nbFigure_GetInfo( light ).knockdownCount == 0 );
	ENSURE( nbFigure_GetInfo( heavy ).knockdownCount == 1 );
	ENSURE( nbFigure_GetInfo( pressed ).knockdownCount == 1 );
	ENSURE( nbFigure_GetState( far ) == nb_figureAnimated );
	DestroyFigureScene( &scene );
	return 0;
}

// A clip of the program plays while the figure stands, blended in, and the figure goes back to idle
static int FigureClipTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );

	// The left arm up ahead and down again
	b3Quat poses[3 * nb_humanoidBoneCount];
	for ( int i = 0; i < 3 * nb_humanoidBoneCount; ++i )
	{
		poses[i] = b3Quat_identity;
	}
	poses[nb_humanoidBoneCount + nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.3f );
	nbKeyframe keys[3];
	for ( int k = 0; k < 3; ++k )
	{
		keys[k] = (nbKeyframe){ 0.5f * (float)k, poses + k * nb_humanoidBoneCount, b3Transform_identity };
	}
	nbClipDef clip = { keys, 3, nb_humanoidBoneCount, true, false };
	nbFigure_PlayClip( figure, &clip, 1.0f );
	StepFigures( &scene, 30 );

	// Half a second in, the arm points ahead
	b3Quat arm = b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidUpperArmLeft ) );
	b3Vec3 down = b3RotateVector( arm, b3Vec3_axisY );
	ENSURE( down.z > 0.9f );
	ENSURE( StandsUpright( &scene, figure ) );

	nbFigure_PlayClip( figure, NULL, 1.0f );
	StepFigures( &scene, 30 );
	arm = b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidUpperArmLeft ) );
	ENSURE( b3RotateVector( arm, b3Vec3_axisY ).y < -0.9f );
	DestroyFigureScene( &scene );
	return 0;
}

// Two figures knocked down and hit while they fly, one of them while it steps and later without room to get up beside the
// other, a third that steps aside, debris on it and a blast: the same bits with 1 and 4 threads
static uint32_t RunFigureScenario( int threadCount )
{
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	nbFigureId figures[3];
	for ( int k = 0; k < 3; ++k )
	{
		figures[k] = AddFigure( &scene, (b3Vec3){ 2.0f * (float)k, 0.0f, 0.0f }, 0.3f * (float)k );
	}
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = (b3Vec3){ 4.0f, 3.5f, 0.05f };
	b3BodyId block = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 0.2f, 0.2f, 0.2f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = 4000.0f;
	b3CreateHullShape( block, &shapeDef, &box.base );

	uint32_t hash = 2166136261u;
	for ( int step = 0; step < 600; ++step )
	{
		// The second steps back from a kick and is knocked down while it steps, the third steps aside
		if ( step == 5 )
		{
			HitChest( figures[1], 4, b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.3f ), (b3Vec3){ 0.0f, 0.0f, -30.0f } ) );
		}
		if ( step == 8 )
		{
			HitChest( figures[2], 5, b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.6f ), (b3Vec3){ 35.0f, 0.0f, 0.0f } ) );
		}
		if ( step == 20 )
		{
			HitChest( figures[0], 1, (b3Vec3){ 0.0f, 0.0f, -90.0f } );
			HitChest( figures[1], 2, (b3Vec3){ 70.0f, 0.0f, 20.0f } );
		}
		if ( step == 35 )
		{
			HitChest( figures[0], 3, (b3Vec3){ 20.0f, 30.0f, 0.0f } );
		}
		if ( step == 50 )
		{
			nbImpactDef blast = { 0 };
			blast.point = b3ToPos( (b3Vec3){ 1.0f, 0.8f, 1.0f } );
			blast.radius = 1.0f;
			blast.explosionImpulse = 300.0f;
			nbWorld_ApplyImpact( scene.world, &blast );
		}
		nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
	}

	for ( int k = 0; k < 3; ++k )
	{
		b3WorldTransform transforms[nb_humanoidBoneCount];
		int count = nbFigure_GetBoneTransforms( figures[k], transforms, nb_humanoidBoneCount );
		for ( int i = 0; i < count; ++i )
		{
			const uint8_t* bytes = (const uint8_t*)( transforms + i );
			for ( size_t b = 0; b < sizeof( b3WorldTransform ); ++b )
			{
				hash = ( hash ^ bytes[b] ) * 16777619u;
			}
		}
		nbFigureInfo info = nbFigure_GetInfo( figures[k] );
		hash = ( hash ^ (uint32_t)( info.state + 4 * info.knockdownCount + 64 * info.getUpCount + 1024 * info.stepCount +
									65536 * info.catchCount ) ) *
			   16777619u;
	}
	DestroyFigureScene( &scene );
	return hash;
}

//----------------------------------------------------------------------------------------------------------------------
// Ragdolls against destructible walls, see the roadmap of the game, 26.16, 26.19 C and 30.9

// A wall 6 m wide and 3.5 m high along x on the ground, its face toward the figure at z = 0: boards of wood, light and
// weak, or concrete as in the demo. Without a pre-fracture, it breaks where it is hit.
static nbDestructibleId AddWall( FigureScene* scene, bool wood, float thickness, uint32_t seed )
{
	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.position = b3ToPos( (b3Vec3){ 0.0f, 1.75f, -0.5f * thickness } );
	def.seed = seed;
	if ( wood )
	{
		def.material.density = 600.0f;
		def.material.strength = 1.0e5f;
		def.material.fragmentSize = 0.15f;
	}
	else
	{
		def.material.density = 2400.0f;
		def.material.strength = 1.1e6f;
		def.material.fragmentSize = 0.13f;
	}
	return nbCreateBox( scene->world, &def, (b3Vec3){ 3.0f, 1.75f, 0.5f * thickness } );
}

// How a figure that flew at a wall ended up
typedef struct WallThrow
{
	// Where its chest and its pelvis ended along z, the wall has its face at z = 0, and whether the chest went through the
	// wall, not around or over it
	float chestZ;
	float pelvisZ;
	bool through;
	int debrisCount;

	// Its collisions with the wall, the most in one step, the steps in which several of its bones hit the wall, and the
	// most of those that made more than one collision
	int collisionCount;
	int mostCollisionsInAStep;
	int sharedSteps;
	int splitSteps;

	// The largest share of its speed it kept going through, and whether its momentum toward the wall ever grew in a step it
	// went through
	float passThrough;
	bool gained;

	// The most bones that lay inside a part of the wall that stands, in any step
	int bonesInWall;

	uint32_t hash;
} WallThrow;

// The bones of a figure that lie inside a static chunk of the wall: the closest point of the chunk is the center itself
static int BonesInWall( nbFigureId figure, nbDestructibleId wall )
{
	nbChunkId chunks[1024];
	int chunkCount = nbDestructible_GetChunks( wall, chunks, 1024 );
	int count = 0;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		b3Vec3 center = b3ToVec3( b3Body_GetWorldCenter( nbFigure_GetBody( figure, i ) ) );
		for ( int c = 0; c < chunkCount; ++c )
		{
			if ( nbChunk_IsDynamic( chunks[c] ) )
			{
				continue;
			}
			b3Vec3 closest = b3Shape_GetClosestPoint( nbChunk_GetShape( chunks[c] ), center );
			if ( b3Length( b3Sub( closest, center ) ) < 1.0e-4f )
			{
				count += 1;
				break;
			}
		}
	}
	return count;
}

// A figure 2 m in front of the wall, facing it with its back or its side, is kicked at the chest toward the wall and a
// little up, and flies into it. Three seconds later, with the figure left lying.
static WallThrow ThrowAtWall( int threadCount, bool wood, float thickness, uint32_t seed, float impulse, bool side )
{
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	nbDestructibleId wall = AddWall( &scene, wood, thickness, seed );
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.position = b3ToPos( (b3Vec3){ 0.0f, 0.0f, 2.0f } );
	def.heading = side ? 0.5f * B3_PI : 0.0f;
	def.canGetUp = false;
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	StepFigures( &scene, 10 );
	HitChest( figure, 1, (b3Vec3){ 0.0f, 0.3f * impulse, -impulse } );

	WallThrow result = { 0 };
	float threshold = nbDefaultWorldDef().collisionSpeedThreshold;
	for ( int step = 0; step < 180; ++step )
	{
		b3Vec3 before = FigureMomentum( figure );
		StepFigures( &scene, 1 );

		// The bones that hit the wall hard enough to damage it
		b3ContactEvents contactEvents = b3World_GetContactEvents( scene.physicsWorld );
		int hitBones[nb_humanoidBoneCount] = { 0 };
		for ( int i = 0; i < contactEvents.hitCount; ++i )
		{
			const b3ContactHitEvent* hit = contactEvents.hitEvents + i;
			if ( hit->approachSpeed < threshold || b3Shape_IsValid( hit->shapeIdA ) == false ||
				 b3Shape_IsValid( hit->shapeIdB ) == false )
			{
				continue;
			}
			for ( int k = 0; k < 2; ++k )
			{
				int bone = 0;
				b3BodyId body = b3Shape_GetBody( k == 0 ? hit->shapeIdA : hit->shapeIdB );
				b3BodyId other = b3Shape_GetBody( k == 0 ? hit->shapeIdB : hit->shapeIdA );
				nbFigureId owner = nbWorld_GetFigureFromBody( scene.world, body, &bone );
				nbFigureId otherOwner = nbWorld_GetFigureFromBody( scene.world, other, NULL );
				if ( B3_ID_EQUALS( owner, figure ) && NB_IS_NULL( otherOwner ) && b3Body_GetType( other ) == b3_staticBody &&
					 B3_ID_EQUALS( other, scene.groundBody ) == false )
				{
					hitBones[bone] = 1;
				}
			}
		}
		int boneCount = 0;
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			boneCount += hitBones[i];
		}

		// The collisions of the figure with the part of the wall that stands, its pieces are parts of their own
		nbEvents events = nbWorld_GetEvents( scene.world );
		int collisions = 0;
		for ( int i = 0; i < events.collisionCount; ++i )
		{
			const nbCollisionEvent* event = events.collisions + i;
			if ( B3_ID_EQUALS( event->figureId, figure ) == false || B3_ID_EQUALS( event->destructibleId, wall ) == false ||
				 b3Body_GetType( event->partBodyId ) != b3_staticBody )
			{
				continue;
			}
			collisions += 1;
			result.passThrough = b3MaxFloat( result.passThrough, event->passThrough );

			// The momentum toward the wall, against the normal out of it
			float towardBefore = -b3Dot( before, event->normal );
			float towardAfter = -b3Dot( FigureMomentum( figure ), event->normal );
			// Gravity adds its impulse of the step toward a wall the figure falls onto from above
			float gravity = 74.0f * 10.0f / 60.0f * b3MaxFloat( event->normal.y, 0.0f );
			result.gained = result.gained ||
							( event->passThrough > 0.0f && towardAfter > 1.001f * ( towardBefore + gravity ) + 0.5f );
		}
		result.collisionCount += collisions;
		result.mostCollisionsInAStep = collisions > result.mostCollisionsInAStep ? collisions : result.mostCollisionsInAStep;
		result.sharedSteps += boneCount > 1 && collisions == 1 ? 1 : 0;
		result.splitSteps += boneCount > 1 && collisions > 1 ? 1 : 0;
		int inWall = BonesInWall( figure, wall );
		result.bonesInWall = inWall > result.bonesInWall ? inWall : result.bonesInWall;
	}

	b3Vec3 chest = BonePosition( figure, nb_humanoidChest );
	result.chestZ = chest.z;
	result.pelvisZ = BonePosition( figure, nb_humanoidPelvis ).z;
	result.through = chest.z < -thickness && fabsf( chest.x ) < 2.8f && chest.y < 3.3f;
	result.debrisCount = nbWorld_GetStats( scene.world ).debrisCount;

	uint32_t hash = 2166136261u;
	b3WorldTransform transforms[nb_humanoidBoneCount];
	int count = nbFigure_GetBoneTransforms( figure, transforms, nb_humanoidBoneCount );
	for ( int i = 0; i < count; ++i )
	{
		const uint8_t* bytes = (const uint8_t*)( transforms + i );
		for ( size_t b = 0; b < sizeof( b3WorldTransform ); ++b )
		{
			hash = ( hash ^ bytes[b] ) * 16777619u;
		}
	}
	result.hash = ( hash ^ (uint32_t)( nbDestructible_GetChunkCount( wall ) + 4096 * result.debrisCount ) ) * 16777619u;
	DestroyFigureScene( &scene );
	return result;
}

// A figure kicked very hard, 1000 N s, into a wall of boards of wood, 5 cm with its back or 3 cm with its side, flies on
// through the hole it breaks in three or four of eight fracture patterns, in the others the edge of the hole stops it. A
// wall of concrete of 20 cm and boards of 20 cm hold it, though the concrete loses pieces where the trunk slams into it at
// 20 m/s, and the thin wall holds against a kick that only knocks the figure down. In every
// throw the bones that hit the wall in one step make one collision of the figure with the wall, steps with several bones
// in it come up, the momentum of the figure toward the wall never grows in a step it goes through, and no bone ends up
// inside the wall that stands. See the roadmap of the game, 26.16 and 26.19 C.
static int FigureWallTest( void )
{
	typedef struct WallCase
	{
		const char* name;
		bool wood;
		float thickness;
		float impulse;
		bool side;
		int minThrough;
		int maxThrough;
		int maxDebris;
	} WallCase;
	const WallCase cases[] = {
		{ "boards of 5 cm, back first", true, 0.05f, 1000.0f, false, 3, 8, 40 },
		{ "boards of 3 cm, side first", true, 0.03f, 1000.0f, true, 4, 8, 90 },
		{ "boards of 20 cm, back first", true, 0.2f, 1000.0f, false, 0, 0, 80 },
		{ "concrete of 20 cm, back first", false, 0.2f, 1000.0f, false, 0, 0, 60 },
		{ "concrete of 20 cm, side first", false, 0.2f, 1000.0f, true, 0, 0, 10 },
		{ "boards of 5 cm, a weak kick", true, 0.05f, 150.0f, false, 0, 0, 2 },
	};
	int sharedSteps = 0;
	for ( size_t c = 0; c < sizeof( cases ) / sizeof( cases[0] ); ++c )
	{
		const WallCase* wallCase = cases + c;
		int through = 0;
		int shared = 0;
		int mostDebris = 0;
		for ( uint32_t seed = 1; seed <= 8; ++seed )
		{
			WallThrow result =
				ThrowAtWall( 1, wallCase->wood, wallCase->thickness, seed, wallCase->impulse, wallCase->side );
			through += result.through ? 1 : 0;
			shared += result.sharedSteps;
			mostDebris = result.debrisCount > mostDebris ? result.debrisCount : mostDebris;
			ENSURE( result.mostCollisionsInAStep <= 1 );
			ENSURE( result.splitSteps == 0 );
			ENSURE( result.gained == false );
			ENSURE( result.passThrough <= 1.0f );
			ENSURE( result.bonesInWall == 0 );
			ENSURE( wallCase->maxThrough > 0 || result.passThrough == 0.0f || result.through == false );
			ENSURE( wallCase->impulse < 500.0f || result.collisionCount >= 1 );
		}
		printf( "  %s, %.0f N s: through %d of 8 times, %d steps with several bones in one collision, up to %d pieces\n",
				wallCase->name, wallCase->impulse, through, shared, mostDebris );
		ENSURE( wallCase->minThrough <= through && through <= wallCase->maxThrough );
		ENSURE( mostDebris <= wallCase->maxDebris );
		sharedSteps += shared;
	}
	ENSURE( sharedSteps > 0 );
	return 0;
}

// A figure that broke through a wall of boards lies behind it among the pieces, its bones touch them again, and no bone
// ends up inside the wall that stands. It gets up there once nothing lies on it. A block that falls on it while it gets
// up breaks that off, and it gets up again later, see the roadmap of the game, 26.15, 26.19 C and 28.5 (E).
static int FigureWallDebrisTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbDestructibleId wall = AddWall( &scene, true, 0.05f, 1 );
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.position = b3ToPos( (b3Vec3){ 0.0f, 0.0f, 2.0f } );
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	StepFigures( &scene, 10 );
	HitChest( figure, 1, (b3Vec3){ 0.0f, 300.0f, -1000.0f } );

	// No step catches it, through the wall, and back among the pieces of it that lie behind it
	int touches = 0;
	int steps = StepUntil( &scene, figure, nb_figureRagdoll, 10 );
	ENSURE( 0 < steps && steps <= 3 );
	while ( nbFigure_GetState( figure ) == nb_figureRagdoll && steps < 900 )
	{
		StepFigures( &scene, 1 );
		steps += 1;
		ENSURE( BonesInWall( figure, wall ) == 0 );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			b3ContactData contacts[16];
			int count = b3Body_GetContactData( nbFigure_GetBody( figure, i ), contacts, 16 );
			for ( int c = 0; c < count; ++c )
			{
				b3BodyId a = b3Shape_GetBody( contacts[c].shapeIdA );
				b3BodyId b = b3Shape_GetBody( contacts[c].shapeIdB );
				b3BodyId other = B3_ID_EQUALS( a, nbFigure_GetBody( figure, i ) ) ? b : a;
				touches += steps > 60 && b3Body_GetType( other ) == b3_dynamicBody &&
								   NB_IS_NULL( nbWorld_GetFigureFromBody( scene.world, other, NULL ) )
							   ? 1
							   : 0;
			}
		}
	}
	ENSURE( nbFigure_GetState( figure ) == nb_figureGettingUp );
	b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
	printf( "  through the wall to %.2f m behind it, touching pieces of it in %d bone steps, up after %.1f s\n", -pelvis.z,
			touches, (float)steps / 60.0f );
	ENSURE( pelvis.z < -0.5f );
	ENSURE( touches > 0 );

	// A block falls on it while it gets up
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	b3Vec3 chest = BonePosition( figure, nb_humanoidChest );
	bodyDef.position = (b3Vec3){ chest.x, chest.y + 1.0f, chest.z };
	bodyDef.linearVelocity = (b3Vec3){ 0.0f, -4.0f, 0.0f };
	b3BodyId block = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 0.2f, 0.2f, 0.2f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = 2400.0f;
	b3CreateHullShape( block, &shapeDef, &box.base );
	ENSURE( StepUntil( &scene, figure, nb_figureRagdoll, 60 ) >= 0 );
	ENSURE( nbFigure_GetInfo( figure ).abortCount == 1 );

	// And up again where it lies, standing behind the wall
	ENSURE( StepUntil( &scene, figure, nb_figureAnimated, 1200 ) >= 0 );
	StepFigures( &scene, 60 );
	ENSURE( StandsUpright( &scene, figure ) );
	ENSURE( BonePosition( figure, nb_humanoidPelvis ).z < -0.3f );
	ENSURE( BonesInWall( figure, wall ) == 0 );
	DestroyFigureScene( &scene );
	return 0;
}

// The throw through the wall of boards ends in the same bits with one thread and with four, on every platform
#define NB_EXPECTED_WALL_HASH 0xa9825adcu

static int FigureWallDeterminismTest( void )
{
	WallThrow one = ThrowAtWall( 1, true, 0.05f, 1, 1000.0f, false );
	WallThrow four = ThrowAtWall( 4, true, 0.05f, 1, 1000.0f, false );
	printf( "  wall hash: 0x%08x\n", one.hash );
	ENSURE( one.through );
	ENSURE( one.hash == four.hash );
	ENSURE( one.hash == NB_EXPECTED_WALL_HASH );
	return 0;
}

// A board of 3 cm without pre-fracture cracks under a ball of 20 kg at 10 m/s, and at 20 m/s the ball knocks a piece out
// and goes on through. The sites of the fracture kept a margin from its faces larger than half of it, it got none and
// stayed one piece, and the samples of the damage sphere could all miss it.
static int ThinBoardTest( void )
{
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		float thickness = 0.03f;
		nbDestructibleId board = AddWall( &scene, true, thickness, 3 );
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = (b3Vec3){ 0.0f, 1.5f, 1.0f };
		bodyDef.linearVelocity = (b3Vec3){ 0.0f, 0.0f, pass == 0 ? -10.0f : -20.0f };
		b3BodyId ball = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3Sphere sphere = { b3Vec3_zero, 0.15f };
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 1400.0f;
		shapeDef.enableHitEvents = true;
		b3CreateSphereShape( ball, &shapeDef, &sphere );

		int collisions = 0;
		for ( int step = 0; step < 60; ++step )
		{
			StepFigures( &scene, 1 );
			nbEvents events = nbWorld_GetEvents( scene.world );
			for ( int i = 0; i < events.collisionCount; ++i )
			{
				const nbCollisionEvent* event = events.collisions + i;
				ENSURE( NB_IS_NULL( event->figureId ) );
				collisions += B3_ID_EQUALS( event->bodyId, ball ) && B3_ID_EQUALS( event->destructibleId, board ) ? 1 : 0;
			}
		}
		int chunks = nbDestructible_GetChunkCount( board );
		int debris = nbWorld_GetStats( scene.world ).debrisCount;
		printf( "  a ball of %.0f kg at %.0f m/s into a board of %.0f cm: %d collisions, %d chunks, %d pieces off\n",
				b3Body_GetMass( ball ), pass == 0 ? 10.0f : 20.0f, 100.0f * thickness, collisions, chunks, debris );
		ENSURE( collisions >= 1 );
		ENSURE( chunks > 1 );
		ENSURE( pass == 0 || debris >= 1 );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The hash of the figure scenario on every platform, see RunFigureScenario
#define NB_EXPECTED_FIGURE_HASH 0x3f52c5f9u

static int FigureDeterminismTest( void )
{
	uint32_t one = RunFigureScenario( 1 );
	uint32_t again = RunFigureScenario( 1 );
	uint32_t four = RunFigureScenario( 4 );
	printf( "  figure hash: 0x%08x\n", one );
	ENSURE( one == again );
	ENSURE( one == four );
	ENSURE( one == NB_EXPECTED_FIGURE_HASH );
	return 0;
}

int FigureTest( void );

int FigureTest( void )
{
	RUN_TEST( SpringTorqueLimitTest );
	RUN_TEST( HumanoidClipTest );
	RUN_TEST( FigureStandTest );
	RUN_TEST( FigureFlinchTest );
	RUN_TEST( FigureCatchTest );
	RUN_TEST( FigureKnockdownTest );
	RUN_TEST( FigureGetUpRoomTest );
	RUN_TEST( FigureRoomTest );
	RUN_TEST( FigureWakeStandingTest );
	RUN_TEST( FigureStepTest );
	RUN_TEST( FigureStepBlockedTest );
	RUN_TEST( FigureFeetTest );
	RUN_TEST( FigureGroundTest );
	RUN_TEST( FigureWorldTest );
	RUN_TEST( FigureClipTest );
	RUN_TEST( FigureWallTest );
	RUN_TEST( FigureWallDebrisTest );
	RUN_TEST( ThinBoardTest );
	RUN_TEST( FigureWallDeterminismTest );
	RUN_TEST( FigureDeterminismTest );
	return 0;
}
