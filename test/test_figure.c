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

		// From the front pushes back, along -z, from behind forward
		float direction = pass == 0 ? -1.0f : 1.0f;
		b3Vec3 impulse = { 0.0f, 0.0f, 120.0f * direction };
		ENSURE( HitChest( figure, 1, impulse ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureRagdoll );
		b3Vec3 momentum = FigureMomentum( figure );
		ENSURE_SMALL( momentum.z - impulse.z, 1.0f );
		ENSURE_SMALL( momentum.y, 1.0f );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_dynamicBody );
		}

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

		// The profile counts the steps after which the ragdoll was awake, all of its fall and at most those it lay
		nbProfile profile = nbWorld_GetProfile( scene.world );
		ENSURE( fall <= profile.ragdollStepCount && profile.ragdollStepCount <= fall + lying );
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
	ENSURE( nbFigure_GetState( blasted ) == nb_figureRagdoll );
	ENSURE( nbFigure_GetState( far ) == nb_figureAnimated );
	ENSURE( b3Length( FigureMomentum( blasted ) ) > 50.0f );

	StepFigures( &scene, 90 );
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

// Two figures knocked down and hit while they fly, debris on a third and a blast: the same bits with 1 and 4 threads
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
		hash = ( hash ^ (uint32_t)( info.state + 4 * info.knockdownCount + 64 * info.getUpCount ) ) * 16777619u;
	}
	DestroyFigureScene( &scene );
	return hash;
}

// The hash of the figure scenario on every platform, see RunFigureScenario
#define NB_EXPECTED_FIGURE_HASH 0x04ed47eeu

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
	RUN_TEST( FigureKnockdownTest );
	RUN_TEST( FigureGetUpRoomTest );
	RUN_TEST( FigureGroundTest );
	RUN_TEST( FigureWorldTest );
	RUN_TEST( FigureClipTest );
	RUN_TEST( FigureDeterminismTest );
	return 0;
}
