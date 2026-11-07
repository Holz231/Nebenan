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
#include <stdlib.h>
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

// The four bones of the spine of the humanoid, from the belly up
#define SPINE_BONES ( nb_humanoidChest - nb_humanoidBelly + 1 )

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
	nbHitDef hit = { id, nb_humanoidChest, impulse, b3Body_GetWorldPoint( chest, (b3Vec3){ 0.0f, 0.08f, 0.1f } ) };
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

// A parallel joint applies the torque its limit says, see b3ParallelJointDef::maxTorque: a rod tilted 0.3 rad from its
// target, with a spring stiff enough to want hundreds of times more, turns back in one step as fast as its limit turns
// it, the cosine of half the angle short of the full limit, and the joint reports that torque. Up to active-ragdoll-41
// the joint turned it by half: its axes are half the derivative of the turn and the bound held their impulse, so the
// 250 N m of nbFigureMotors::uprightTorque held 125.
static int ParallelJointTorqueTest( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef anchorDef = b3DefaultBodyDef();
	b3BodyId anchor = b3CreateBody( worldId, &anchorDef );

	// A rod of about 10 kg, a meter long along z, tilted about x
	const float tilt = 0.3f;
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisX, tilt );
	b3BodyId rod = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3Capsule capsule = { { 0.0f, 0.0f, -0.45f }, { 0.0f, 0.0f, 0.45f }, 0.05f };
	shapeDef.density = 10.0f / ( B3_PI * 0.05f * 0.05f * 0.9f + 4.0f / 3.0f * B3_PI * 0.05f * 0.05f * 0.05f );
	b3CreateCapsuleShape( rod, &shapeDef, &capsule );

	const float maxTorque = 20.0f;
	b3ParallelJointDef jointDef = b3DefaultParallelJointDef();
	jointDef.base.bodyIdA = anchor;
	jointDef.base.bodyIdB = rod;
	jointDef.hertz = 30.0f;
	jointDef.dampingRatio = 1.0f;
	jointDef.maxTorque = maxTorque;
	b3JointId joint = b3CreateParallelJoint( worldId, &jointDef );

	const float timeStep = 1.0f / 60.0f;
	b3World_Step( worldId, timeStep, 4 );

	// The torque that turned the rod over the step, by its rotational inertia about x
	b3Matrix3 inertia = b3Body_GetLocalRotationalInertia( rod );
	float turned = inertia.cx.x * fabsf( b3Body_GetAngularVelocity( rod ).x ) / timeStep;
	float reported = b3Length( b3Joint_GetConstraintTorque( joint ) );
	float expected = maxTorque * cosf( 0.5f * tilt );
	printf( "  a limit of %.0f N m turns the rod with %.2f N m and reports %.2f, %.2f expected\n", maxTorque, turned, reported,
			expected );
	ENSURE_SMALL( turned - expected, 0.02f * maxTorque );
	ENSURE_SMALL( reported - expected, 0.02f * maxTorque );
	b3DestroyWorld( worldId );
	return 0;
}

// A cone with its own limit in each of four directions, see b3SphericalJointDef::enableSwingLimit: a rod on a spherical
// joint that swings 160 degrees about +x, 40 about -x, 149 about +y and 29 about -y, spun hard about 16 axes in the xy
// plane, never swings more than a degree past the edge of the cone, and along the four axes it reaches it
static int SwingLimitTest( void )
{
#if defined( B3_HAS_SWING_LIMITS )
	const float lowerX = -0.7f, upperX = 2.8f, lowerY = -0.5f, upperY = 2.6f;
	// The edge along +x, +y, -x and -y, the axes of every fourth spin
	const float edges[4] = { upperX, upperY, -lowerX, -lowerY };
	float limits[4] = { b3TanQuarter( upperX ), b3TanQuarter( -lowerX ), b3TanQuarter( upperY ), b3TanQuarter( -lowerY ) };
	float worst = 0.0f;
	for ( int k = 0; k < 16; ++k )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		worldDef.gravity = b3Vec3_zero;
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyDef anchorDef = b3DefaultBodyDef();
		b3BodyId anchor = b3CreateBody( worldId, &anchorDef );
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		b3BodyId rod = b3CreateBody( worldId, &bodyDef );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3Capsule capsule = { { 0.0f, 0.0f, 0.05f }, { 0.0f, 0.0f, 0.3f }, 0.05f };
		b3CreateCapsuleShape( rod, &shapeDef, &capsule );

		b3SphericalJointDef jointDef = b3DefaultSphericalJointDef();
		jointDef.base.bodyIdA = anchor;
		jointDef.base.bodyIdB = rod;
		jointDef.enableSwingLimit = true;
		jointDef.lowerSwingX = lowerX;
		jointDef.upperSwingX = upperX;
		jointDef.lowerSwingY = lowerY;
		jointDef.upperSwingY = upperY;
		jointDef.enableTwistLimit = true;
		jointDef.lowerTwistAngle = -0.5f;
		jointDef.upperTwistAngle = 0.5f;
		b3JointId joint = b3CreateSphericalJoint( worldId, &jointDef );
		float kept[4];
		b3SphericalJoint_GetSwingLimits( joint, kept, kept + 1, kept + 2, kept + 3 );
		ENSURE( b3SphericalJoint_IsSwingLimitEnabled( joint ) );
		ENSURE( kept[0] == lowerX && kept[1] == upperX && kept[2] == lowerY && kept[3] == upperY );

		float phi = 2.0f * B3_PI * (float)k / 16.0f;
		b3Body_SetAngularVelocity( rod, (b3Vec3){ 8.0f * cosf( phi ), 8.0f * sinf( phi ), 0.0f } );
		float beyond = 0.0f;
		float reach = 0.0f;
		for ( int i = 0; i < 240; ++i )
		{
			b3World_Step( worldId, 1.0f / 60.0f, 4 );
			b3Quat q = b3Body_GetRotation( rod );
			beyond = b3MaxFloat( beyond, -b3GetSwingLimitSeparation( q, limits, NULL ) );
			reach = b3MaxFloat( reach, b3GetSwingAngle( q ) );
		}
		ENSURE( beyond < 1.0f * B3_PI / 180.0f );
		if ( k % 4 == 0 )
		{
			ENSURE( reach > edges[k / 4] - 2.0f * B3_PI / 180.0f );
		}
		worst = b3MaxFloat( worst, beyond );
		b3DestroyWorld( worldId );
	}
	printf( "  spun about 16 axes: at most %.2f degrees past the edge of the cone\n", worst * 180.0f / B3_PI );
#endif
	return 0;
}

// The armature of a joint, see b3SphericalJointDef::armature: a rod on a joint at a fixed anchor, spun by an angular
// impulse, turns as if its rotational inertia about the joint were the armature larger, on a spherical joint and on a
// revolute joint about its axis. An armature as large as that inertia halves the turn, and none leaves it as it was.
static int ArmatureTest( void )
{
#if defined( B3_HAS_JOINT_ARMATURE )
	for ( int hinge = 0; hinge < 2; ++hinge )
	{
		float rates[2] = { 0.0f, 0.0f };
		float inertia = 0.0f;
		for ( int pass = 0; pass < 2; ++pass )
		{
			b3WorldDef worldDef = b3DefaultWorldDef();
			worldDef.gravity = b3Vec3_zero;
			b3WorldId worldId = b3CreateWorld( &worldDef );
			b3BodyDef anchorDef = b3DefaultBodyDef();
			b3BodyId anchor = b3CreateBody( worldId, &anchorDef );
			// Turning slower than the threshold of sleep, the rod stays awake
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.type = b3_dynamicBody;
			bodyDef.enableSleep = false;
			b3BodyId rod = b3CreateBody( worldId, &bodyDef );
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			b3Capsule capsule = { { 0.0f, 0.0f, 0.05f }, { 0.0f, 0.0f, 0.3f }, 0.05f };
			b3CreateCapsuleShape( rod, &shapeDef, &capsule );

			// The rotational inertia of the rod about the x-axis through the joint
			float d = b3Body_GetLocalCenter( rod ).z;
			inertia = b3Body_GetLocalRotationalInertia( rod ).cx.x + b3Body_GetMass( rod ) * d * d;
			float armature = pass == 0 ? 0.0f : inertia;

			b3JointId joint;
			if ( hinge )
			{
				// The hinge turns about the z-axis of its frames, here the x-axis of the bodies
				b3RevoluteJointDef jointDef = b3DefaultRevoluteJointDef();
				jointDef.base.bodyIdA = anchor;
				jointDef.base.bodyIdB = rod;
				jointDef.base.localFrameA.q = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
				jointDef.base.localFrameB.q = jointDef.base.localFrameA.q;
				jointDef.armature = armature;
				joint = b3CreateRevoluteJoint( worldId, &jointDef );
				ENSURE( b3RevoluteJoint_GetArmature( joint ) == armature );
			}
			else
			{
				b3SphericalJointDef jointDef = b3DefaultSphericalJointDef();
				jointDef.base.bodyIdA = anchor;
				jointDef.base.bodyIdB = rod;
				jointDef.armature = armature;
				joint = b3CreateSphericalJoint( worldId, &jointDef );
				ENSURE( b3SphericalJoint_GetArmature( joint ) == armature );
			}

			// A step first, so the flywheel turns with the rod when the impulse comes
			b3World_Step( worldId, 1.0f / 60.0f, 4 );
			b3Body_ApplyAngularImpulse( rod, (b3Vec3){ 0.01f, 0.0f, 0.0f }, true );
			for ( int i = 0; i < 30; ++i )
			{
				b3World_Step( worldId, 1.0f / 60.0f, 4 );
			}
			rates[pass] = b3Body_GetAngularVelocity( rod ).x;
			b3DestroyWorld( worldId );
		}
		printf( "  %s joint: 0.01 N m s turn the rod at %.4f rad/s, %.4f with the armature, %.4f and %.4f by its inertia\n",
				hinge ? "revolute" : "spherical", rates[0], rates[1], 0.01f / inertia, 0.005f / inertia );
		ENSURE( fabsf( rates[0] * inertia / 0.01f - 1.0f ) < 0.02f );
		ENSURE( fabsf( rates[1] * inertia / 0.005f - 1.0f ) < 0.02f );
	}
#endif
	return 0;
}

// The clips of the library: the get-up clips lie with the head ahead at first and stand at the height of the idle clip at
// the end, no keyframe sinks into the ground, and the baked keyframes lie at most a fifteenth of a second apart
static int HumanoidClipTest( void )
{
	nbBoneDef bones[nb_humanoidBoneCount];
	float stand = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	ENSURE_SMALL( stand - 0.96f, 1.0e-6f );
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

// The rotation of a bone in the frame of another
static b3Quat RelativeRotation( nbFigureId figure, int parent, int bone )
{
	return b3InvMulQuat( b3Body_GetRotation( nbFigure_GetBody( figure, parent ) ),
						 b3Body_GetRotation( nbFigure_GetBody( figure, bone ) ) );
}

// How far a bone of a figure lies turned in the frame of another against the same bones of a twin, in radians
static float TurnAgainstTwin( nbFigureId figure, nbFigureId twin, int parent, int bone )
{
	b3Quat change = b3InvMulQuat( RelativeRotation( twin, parent, bone ), RelativeRotation( figure, parent, bone ) );
	return 2.0f * acosf( b3MinFloat( 1.0f, fabsf( change.s ) ) );
}

// Where a bone of a figure lies from its pelvis in the frame of the figure
static b3Vec3 FromPelvis( nbFigureId figure, int bone )
{
	b3Vec3 offset = b3Sub( BonePosition( figure, bone ), BonePosition( figure, nb_humanoidPelvis ) );
	return b3InvRotateVector( nbFigure_GetFrame( figure ).q, offset );
}

// A light hit swings the head and the arms of the humanoid by where it lands, after the roadmap of the game, 26.4 and
// 26.5, measured against a twin that plays the same clip unhit. On the face it throws the head back against the chest,
// much farther than the trunk turns. On the right forearm from behind it swings that arm forward, not the other, and
// turns the trunk less than a hit on the chest. From outside it does not swing the arm into the body, the body takes it.
// On the chest it turns the trunk as before, the head stays behind and swings after it, and the arms hang on as
// pendulums, close to the trunk and never into it. All of it comes back to the clip, and the figure stands animated
// throughout.
static int FigureSwingTest( void )
{
	typedef struct SwingHit
	{
		const char* name;
		int bone;
		b3Vec3 impulse;
		b3Vec3 local;
	} SwingHit;
	const SwingHit hits[4] = {
		{ "face, from the front", nb_humanoidHead, { 0.0f, 0.0f, -20.0f }, { 0.0f, 0.08f, 0.09f } },
		{ "right forearm, from behind", nb_humanoidForearmRight, { 0.0f, 0.0f, 20.0f }, { 0.0f, 0.12f, -0.04f } },
		{ "right forearm, from outside", nb_humanoidForearmRight, { 20.0f, 0.0f, 0.0f }, { 0.0f, 0.12f, 0.0f } },
		{ "chest, from the front", nb_humanoidChest, { 0.0f, 0.0f, -30.0f }, { 0.0f, 0.08f, 0.1f } },
	};

	for ( int k = 0; k < 4; ++k )
	{
		const SwingHit* h = hits + k;
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		nbFigureId twin = AddFigure( &scene, (b3Vec3){ 3.0f, 0.0f, 0.0f }, 0.0f );
		StepFigures( &scene, 10 );

		b3BodyId body = nbFigure_GetBody( figure, h->bone );
		nbHitDef hit = { 1, h->bone, h->impulse, b3Body_GetWorldPoint( body, h->local ) };
		ENSURE( nbFigure_ApplyHit( figure, &hit ) );

		float head = 0.0f;
		float right = 0.0f;
		float left = 0.0f;
		float trunk = 0.0f;
		float ahead = 0.0f;
		float inward = 0.0f;
		for ( int i = 0; i < 30; ++i )
		{
			StepFigures( &scene, 1 );
			ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
			head = b3MaxFloat( head, TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidHead ) );
			right = b3MaxFloat( right, TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidUpperArmRight ) );
			left = b3MaxFloat( left, TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidUpperArmLeft ) );
			trunk = b3MaxFloat( trunk, b3Length( nbFigure_GetInfo( figure ).flinch ) );
			b3Vec3 hand = FromPelvis( figure, nb_humanoidHandRight );
			b3Vec3 twinHand = FromPelvis( twin, nb_humanoidHandRight );
			ahead = b3MaxFloat( ahead, hand.z - twinHand.z );
			inward = b3MaxFloat( inward, fabsf( twinHand.x ) - fabsf( hand.x ) );
		}
		printf( "  %s, %.0f N s: head %.3f rad, right arm %.3f, left arm %.3f against the chest, trunk %.3f, right hand "
				"%.3f m ahead and %.3f m in\n",
				h->name, b3Length( h->impulse ), head, right, left, trunk, ahead, inward );

		if ( k == 0 )
		{
			ENSURE( head > 0.25f && head > 2.0f * trunk );
		}
		else if ( k == 1 )
		{
			ENSURE( right > 0.2f && left < 0.1f && ahead > 0.1f && trunk < 0.12f );
		}
		else if ( k == 2 )
		{
			ENSURE( inward < 0.02f && trunk > 0.12f );
		}
		else
		{
			ENSURE_SMALL( trunk - 0.24f, 0.01f );
			ENSURE( head > 0.08f && right < 0.1f && left < 0.1f && inward < 0.02f );
		}

		// Back in the clip
		StepFigures( &scene, 120 );
		nbFigureInfo info = nbFigure_GetInfo( figure );
		ENSURE( TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidHead ) < 0.01f );
		ENSURE( TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidUpperArmRight ) < 0.01f );
		ENSURE( TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidUpperArmLeft ) < 0.01f );
		ENSURE( b3Length( info.flinch ) < 0.01f );
		ENSURE( info.hitCount == 1 && info.knockdownCount == 0 );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated && StandsUpright( &scene, figure ) );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A hard hit that steps can catch, after the roadmap of the game, 26.14 and 28.3, and the balance of the branch
// ragdoll-11: the bones of the figure become dynamic, its joints hold the pose of its clip with the full strength of its
// motors, a hold keeps its pelvis upright, and it catches itself with steps, its arms rowing where a step comes short.
// On its feet all the time, it stands animated again where it caught itself and steps its feet back under its body. On
// the upper chest it catches 60, 70 and 100 N s from the front, 80 N s from behind and 60 and 80 N s from either side.
// 150 N s from the front is too much, it gives up at once, see NB_GIVE_UP, and 110 N s from its right: it takes two
// steps and falls within 0.4 s, its arm on the far side raised above its head. With the arms of 81 cm and the
// stance of the reference picture, round 36 fell at 100 N s from the front and caught 90 to 98, 102 and 104, round 37
// fell at 98 and 100 and caught 94, 96 and 102 to 106: hard hits from the front lie where a little more or less tips the
// balance. Round 38, which lifts its feet higher and sinks farther before it falls, catches 94 to 104 and falls at 106.
// Round 39, whose capture point counts the turn of the body, see nbMeasure, catches 100 N s from the front with 2 steps
// in 1.9 s where round 38 took 3 steps and 2.5 s, and still falls at 106. Round 40, whose legs carry the body, see
// nbCarryBody, catches 106 N s too and 64 to 88 with one step each, but takes 3 or 4 steps at 58 to 62 N s where round 39
// took one: at 60 round 39 caught itself after one step back with its center of mass 9 cm low, round 40 rises again and
// steps on until it stands 4 cm low. Round 41, whose upper body bends on with the tilt of the pelvis, see nbBendTrunk,
// catches 60 N s with one step again and 100 N s with three. From the front it catches up to 118 N s and gives up from
// 120, from behind it catches all up to 130 with three steps. From its right it catches up to 108 N s, where round 40
// fell at 100, the head sinking to 1.13 m in a deep lunge, falls at 110 after two steps and gives up from 112. From its
// left it catches up to 102 and 106 to 110 and falls at 104. Round 45, whose soles grip like rubber, see
// NB_SOLE_FRICTION, catches up to 120 N s from the front and gives up from 122, and from either side it catches every
// hit up to 110 N s and gives up from 112, where round 44 fell at 110 from its right and at 102 and 110 from its left,
// each time after two steps. The test gives 112 from its right. Round 47, which gives up only where the first step would
// leave the capture point 1.5 m out, see NB_GIVE_UP, stumbles two steps after 112 N s from its right and falls after 23
// updates, as the roadmap of the game wants after a strong hit, and still falls at once after 150 N s from the front,
// an extreme hit. Round 53, whose pelvis turns with the leg that steps and whose upper body turns back against it, see
// nbPelvisTurn, catches 130 and 134 N s from the front, where round 52 fell at 134, and gives up from 138 as before.
// From either side it catches up to 104 N s and 110 and 112, falls at 106 after two steps, from its right at 108 too,
// and stumbles from 114 into the fall, where round 52 caught every hit up to 110 and fell from 112. At 110 from its
// right the head of round 52 sank to 1.12 m before it caught itself, that of round 53 to 1.21 m. Round 61, whose stance
// foot stays planted where it last carried, see NB_PLANT_LOAD, catches every hit from its right up to 114 N s, where
// round 60 fell at 108 and 114, falls at 116 after three steps, stumbles two steps from 118 to 122 and falls after 41 to
// 47 updates, one step from 124 to 128 and falls after 20, and gives up from 130 as before. The test gives 124 from its
// right.
static int FigureCatchTest( void )
{
	typedef struct CatchCase
	{
		const char* name;
		b3Vec3 impulse;
		bool falls;
		bool stumbles;
	} CatchCase;
	CatchCase cases[] = {
		{ "60 N s from the front", { 0.0f, 0.0f, -60.0f }, false },	 { "70 N s from the front", { 0.0f, 0.0f, -70.0f }, false },
		{ "80 N s from behind", { 0.0f, 0.0f, 80.0f }, false },		 { "60 N s from its right", { 60.0f, 0.0f, 0.0f }, false },
		{ "60 N s from its left", { -60.0f, 0.0f, 0.0f }, false },	 { "100 N s from the front", { 0.0f, 0.0f, -100.0f }, false },
		{ "80 N s from its right", { 80.0f, 0.0f, 0.0f }, false },	 { "80 N s from its left", { -80.0f, 0.0f, 0.0f }, false },
		{ "150 N s from the front", { 0.0f, 0.0f, -150.0f }, true }, { "124 N s from its right", { 124.0f, 0.0f, 0.0f }, true, true },
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
			int steps = StepUntil( &scene, figure, nb_figureRagdoll, 60 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			printf( "  %s: falls after %d updates and %d steps\n", cases[c].name, steps, info.stepCount );
			ENSURE( 0 < steps && steps <= 42 );
			ENSURE( info.stepCount <= 2 && info.knockdownCount == 1 && info.catchCount == 0 );
			ENSURE( cases[c].stumbles ? info.stepCount >= 1 : info.stepCount == 0 );
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

// The arms row where a step comes short, see nbRowArms: an upper arm that does not swing into the body swings out in
// the sense of the fall, the farther the shorter the step comes, and its motor turns the body back. Hit on the upper
// chest with 100 N s from the front, both arms swing up from where they hang by more than 1.2 rad, with 62 N s from the
// front by less than 1 rad, and the figure catches itself. Below about 58 N s from the front its steps catch it without
// the arms, round 28 rowed at 60 N s but not at 56 or 64, round 35 at 72 and round 38 from 76 on. Since round 39 the
// capture point counts the turn of the body, see nbMeasure: from 58 N s on one step catches the figure where round 38
// took three up to 70 N s, and the arms row from 60 on, at 80 N s up to 1.57 rad where round 38 rowed up to 0.83.
// Round 40, whose legs carry the body, see nbCarryBody, catches itself with one step from 64 N s on and rows at 80 N s up
// to 1.49 rad. At 50 N s its second step back comes 32 cm short, where round 39's came 18 cm short, and one arm rows up
// to 2.1 rad, as at 52 and 54. Round 41, whose upper body bends on with the tilt of the pelvis, see nbBendTrunk, takes
// three steps at 100 N s from the front where round 40 took two, its chest tilts back up to 0.49 rad where round 40's
// tilted 0.34, and the arms swing up to 2.02 rad against the chest and 2.32 against the down of the world, where round
// 40's swung up to 1.56 and 1.86.
// Falling forward from 110 N s from behind they swing back to the 40 degrees a shoulder allows, from more than 0.5 rad
// to 0.8, see nbMakeHumanoidBones: with a cone of 86 degrees all round they swung back 1.3 rad, and the figure caught
// 120 N s. Round 35 caught 110 N s from behind. Rounds 36 and 37, which stand with the center of mass farther ahead
// over the soles, see FigureStepTest, and kept the arms 0.3 rad short of the cone, fell from 104 and 110 N s on. Round
// 38 swings them to the cone, lifts its feet higher and catches up to 120 N s.
// Pushed to its left with 80 N s only the right arm rows, the left one would swing into the body, the right one up to
// 1.62 rad with the chest of two parts of round 37. In round 41, whose chest bends on to the side up to 0.36 rad where
// round 40's tilted 0.15, the right one rows up to 1.77 rad against the chest, and the left one goes along with the
// chest, 0.44 rad against it and 0.35 against the down of the world, round 40's 0.33 and 0.25. A swinging arm bends
// its elbow, and the arms turn against the chest at most 16 rad/s and brake at most 450 rad/s2: they swing, they do not
// jolt. Pulled toward a target far ahead, as in round 23, they turned up to 21 rad/s and braked up to 684 rad/s2. The
// arm of 81 cm of round 36 turned up to 15.03 rad/s pushed to its left without the armature of its shoulder, see
// nbMakeHumanoidBones, and 10.1 with it. Since round 58 the stance leg lowers the pelvis late in a step, see
// NB_DROP_PHASE, in the step to the side the pelvis comes down to 0.785 instead of 0.793 m as the foot lands, and the
// arm that rows swings on against the chest with up to 15.7 instead of 14.9 rad/s. Since round 61 the stance foot stays
// planted where it last carried, see NB_PLANT_LOAD, and the right arm rows up to 1.90 instead of 1.83 rad, as the arms
// swung in round 41 at 100 N s from the front. Standing animated again, the arms hang in the pose of the clip.
static int FigureRowTest( void )
{
	typedef struct RowCase
	{
		const char* name;
		b3Vec3 impulse;
		float least[2];
		float most[2];
	} RowCase;
	const RowCase cases[] = {
		{ "100 N s from the front", { 0.0f, 0.0f, -100.0f }, { 1.2f, 1.2f }, { 2.2f, 2.2f } },
		{ "110 N s from behind", { 0.0f, 0.0f, 110.0f }, { 0.5f, 0.5f }, { 0.8f, 0.8f } },
		{ "80 N s to its left", { 80.0f, 0.0f, 0.0f }, { 0.0f, 1.2f }, { 0.5f, 2.0f } },
		{ "62 N s from the front", { 0.0f, 0.0f, -62.0f }, { 0.4f, 0.4f }, { 1.0f, 1.0f } },
	};
	const int arms[2] = { nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight };
	for ( size_t c = 0; c < sizeof( cases ) / sizeof( cases[0] ); ++c )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		nbFigureId twin = AddFigure( &scene, (b3Vec3){ 5.0f, 0.0f, 0.0f }, 0.0f );
		StepFigures( &scene, 30 );
		ENSURE( HitChest( figure, 1, cases[c].impulse ) );

		// How far each upper arm swings from hanging along the chest, how far its elbow bends, and how fast it turns and
		// brakes against the chest
		float swing[2] = { 0.0f, 0.0f };
		float bend[2] = { 0.0f, 0.0f };
		float fastest = 0.0f;
		float hardest = 0.0f;
		float previous[2] = { 0.0f, 0.0f };
		int steps = 0;
		while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 600 )
		{
			StepFigures( &scene, 1 );
			steps += 1;
			b3BodyId chest = nbFigure_GetBody( figure, nb_humanoidChest );
			b3Vec3 down = b3RotateVector( b3Body_GetRotation( chest ), (b3Vec3){ 0.0f, -1.0f, 0.0f } );
			for ( int k = 0; k < 2; ++k )
			{
				b3BodyId upper = nbFigure_GetBody( figure, arms[k] );
				b3Vec3 along = b3RotateVector( b3Body_GetRotation( upper ), b3Vec3_axisY );
				b3Vec3 fore = b3RotateVector( b3Body_GetRotation( nbFigure_GetBody( figure, arms[k] + 1 ) ), b3Vec3_axisY );
				swing[k] = b3MaxFloat( swing[k], acosf( b3ClampFloat( b3Dot( along, down ), -1.0f, 1.0f ) ) );
				bend[k] = b3MaxFloat( bend[k], acosf( b3ClampFloat( b3Dot( along, fore ), -1.0f, 1.0f ) ) );
				float speed = b3Length( b3Sub( b3Body_GetAngularVelocity( upper ), b3Body_GetAngularVelocity( chest ) ) );
				fastest = b3MaxFloat( fastest, speed );
				hardest = b3MaxFloat( hardest, ( previous[k] - speed ) * 60.0f );
				previous[k] = speed;
			}
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		printf( "  %s: the upper arms swing up to %.2f and %.2f rad, the elbows bend up to %.2f and %.2f rad, the arms turn "
				"up to %.1f rad/s and brake up to %.0f rad/s2, caught after %.2f s with %d steps\n",
				cases[c].name, swing[0], swing[1], bend[0], bend[1], fastest, hardest, (float)steps / 60.0f, info.stepCount );
		ENSURE( info.state == nb_figureAnimated && info.knockdownCount == 0 && info.catchCount == 1 );
		for ( int k = 0; k < 2; ++k )
		{
			ENSURE( cases[c].least[k] < swing[k] && swing[k] < cases[c].most[k] );
			ENSURE( cases[c].least[k] == 0.0f || bend[k] > 0.6f );
		}
		ENSURE( fastest < 16.0f );
		ENSURE( hardest < 450.0f );

		// Standing animated, the arms hang as in the clip, like those of the figure that stood all along
		StepFigures( &scene, 60 );
		ENSURE( StandsUpright( &scene, figure ) );
		ENSURE( TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidUpperArmLeft ) < 0.05f );
		ENSURE( TurnAgainstTwin( figure, twin, nb_humanoidChest, nb_humanoidUpperArmRight ) < 0.05f );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The hold through the legs, see nbFigureMotors::uprightAgainstWorld: while the figure catches itself, its pelvis is
// held upright against the thighs of the legs that carry it, which push against the ground as a person's do, and not
// against the world, whose anchor is never made. It is the balance from the figure's own strength, F61 of the roadmap:
// of the 168 light hits from 40 to 80 N s on the upper chest it lets none fall since round 49, where the hold pulls
// with the strength of the hip, the figure braces its knees and steps at once after a clear push, see NB_KNEE_BRACE, 9
// in round 45, 17 in round 41, 43 before the upper body bent on with the tilt of the pelvis, see nbBendTrunk, and 87
// before the legs carried the body in round 40, see nbCarryBody. Since round 50 it swings each foot on with the speed
// the foot already has, since round 51 the hold through a leg pulls no harder than its foot holds on the ground, see
// nbFootGrip, since round 52 the hold brakes the tipping of the pelvis and the swinging foot pulls its toes up, see
// NB_SWING_TOES. 52 N s from its right it catches with steps, the same with one thread and with four, and its knees
// hold 375 N m while it catches itself, those of a figure with the hold against the world 225. From eight directions it
// catches 50 and 70 N s on the upper chest.
static float KneeLimit( nbFigureId figure )
{
	b3BodyId shin = nbFigure_GetBody( figure, nb_humanoidShinLeft );
	b3JointId joints[8];
	int count = b3Body_GetJoints( shin, joints, 8 );
	for ( int j = 0; j < count; ++j )
	{
		if ( b3Joint_GetType( joints[j] ) == b3_revoluteJoint )
		{
			return b3RevoluteJoint_GetMaxSpringTorque( joints[j] );
		}
	}
	return 0.0f;
}

static int FigureLegHoldTest( void )
{
	// The knees of a figure that catches itself with the hold against the world are not braced
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 30 );
		ENSURE( HitChest( figure, 1, (b3Vec3){ 52.0f, 0.0f, 0.0f } ) );
		StepFigures( &scene, 2 );
		ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
		ENSURE_SMALL( KneeLimit( figure ) - 225.0f, 0.01f );
		DestroyFigureScene( &scene );
	}

	// With its own strength it catches every one of these hits, after one with fewer steps or more than five
	int mostSteps = 0;
	for ( int d = 0; d < 8; ++d )
	{
		for ( int impulse = 50; impulse <= 70; impulse += 20 )
		{
			FigureScene scene;
			CreateFigureScene( &scene, 1 );
			nbFigureDef def = nbDefaultFigureDef();
			def.rig = &scene.rig;
			def.standHeight = scene.standHeight;
			def.motors.uprightAgainstWorld = false;
			nbFigureId figure = nbCreateFigure( scene.world, &def );
			StepFigures( &scene, 30 );
			b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.25f * B3_PI * (float)d ),
											   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
			ENSURE( HitChest( figure, 1, b3MulSV( (float)impulse, direction ) ) );
			ENSURE( StepUntil( &scene, figure, nb_figureAnimated, 600 ) >= 0 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			ENSURE( info.knockdownCount == 0 );
			mostSteps = b3MaxInt( mostSteps, info.stepCount );
			DestroyFigureScene( &scene );
		}
	}
	printf( "  50 and 70 N s from eight directions with its own strength: all caught, at most %d steps\n", mostSteps );

	uint32_t hashes[2] = { 0u, 0u };
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, pass == 0 ? 1 : 4 );
		nbFigureDef def = nbDefaultFigureDef();
		def.rig = &scene.rig;
		def.standHeight = scene.standHeight;
		def.motors.uprightAgainstWorld = false;
		nbFigureId figure = nbCreateFigure( scene.world, &def );
		StepFigures( &scene, 30 );

		// Two joints more on the pelvis while it catches itself, each to a thigh, and none to the world
		b3BodyId pelvis = nbFigure_GetBody( figure, nb_humanoidPelvis );
		int standing = b3Body_GetJointCount( pelvis );
		ENSURE( HitChest( figure, 1, (b3Vec3){ 52.0f, 0.0f, 0.0f } ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
		b3JointId joints[8];
		int count = b3Body_GetJoints( pelvis, joints, 8 );
		int thighs = 0;
		for ( int j = 0; j < count; ++j )
		{
			b3BodyId other = b3Joint_GetBodyA( joints[j] );
			bool thigh = B3_ID_EQUALS( other, nbFigure_GetBody( figure, nb_humanoidThighLeft ) ) ||
						 B3_ID_EQUALS( other, nbFigure_GetBody( figure, nb_humanoidThighRight ) );
			thighs += b3Joint_GetType( joints[j] ) == b3_parallelJoint && thigh ? 1 : 0;
		}
		ENSURE( count == standing + 2 && thighs == 2 );
		ENSURE( b3Body_IsValid( nbGetWorldFromId( scene.world )->figureAnchor ) == false );
		StepFigures( &scene, 2 );
		ENSURE_SMALL( KneeLimit( figure ) - 375.0f, 0.01f );

		// On its feet all the time, and standing animated again with the joints of its skeleton alone
		int steps = 0;
		float lowestHead = FLT_MAX;
		while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 360 )
		{
			StepFigures( &scene, 1 );
			steps += 1;
			lowestHead = b3MinFloat( lowestHead, BonePosition( figure, nb_humanoidHead ).y );
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		printf( "  52 N s from its right with %s: caught with %d steps in %.2f s, the head at least %.2f m high\n",
				pass == 0 ? "one thread" : "four", info.stepCount, (float)steps / 60.0f, lowestHead );
		ENSURE( info.state == nb_figureAnimated && info.knockdownCount == 0 && info.catchCount == 1 && info.stepCount >= 1 );
		ENSURE( lowestHead > 1.3f );
		ENSURE( b3Body_GetJointCount( pelvis ) == standing );

		uint32_t hash = 2166136261u;
		b3WorldTransform transforms[nb_humanoidBoneCount];
		int boneCount = nbFigure_GetBoneTransforms( figure, transforms, nb_humanoidBoneCount );
		for ( int i = 0; i < boneCount; ++i )
		{
			const uint8_t* bytes = (const uint8_t*)( transforms + i );
			for ( size_t b = 0; b < sizeof( b3WorldTransform ); ++b )
			{
				hash = ( hash ^ bytes[b] ) * 16777619u;
			}
		}
		hashes[pass] = ( hash ^ (uint32_t)( steps + 1024 * info.stepCount ) ) * 16777619u;
		DestroyFigureScene( &scene );
	}
	printf( "  hash 0x%08x with one thread, 0x%08x with four\n", hashes[0], hashes[1] );
	ENSURE( hashes[0] == hashes[1] );
	return 0;
}

// Hits on the upper chest from eight directions, 50 to 150 N s in steps of 10: every figure catches itself and stands
// animated again or falls, none stays catching itself for good, also not on one foot after a long lunge, see
// NB_ONE_FOOT_TIME. Of the 32 hits up to 80 N s at most one falls, and it catches at least 40 of all 88, round 35 47 and
// round 36, with the arms of 81 cm and the stance of the reference picture, 43, round 40 49 and round 41, whose upper
// body bends on with the tilt of the pelvis, see nbBendTrunk, 58. Now and then a
// light hit ends in a long stumble and a fall: of 168 hits from 40 to 80 N s in steps of 2 in this scene, round 23 fell
// at 64 and 66 N s from behind, round 24 at 66 N s from behind and at 80 N s from its front right, and round 27, with
// joints that move as far as a person's, at 74 N s from its right. Round 28, where a hit across the front of the chest
// turns the trunk about its long axis, see nbTwistTrunk, fell at 58, 62, 66 and 72 N s from its right and 66 from its
// left, and with the armature of the neck it falls at 58, 64 and 76 N s from its right and 66 from its left. Round 34,
// where a foot that hovers just above the ground still stands, see nbMeasure, catches all 168. Round 26 caught those
// 80 N s from its front right after 17 steps and 7.8 s, round 33 took up to 4.0 s, round 35 3.1 s and round 36 3.7 s.
static int FigureCatchScanTest( void )
{
	int caught = 0;
	int fell = 0;
	int caughtTo80 = 0;
	int hitsTo80 = 0;
	float longest = 0.0f;
	for ( int d = 0; d < 8; ++d )
	{
		b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.25f * B3_PI * (float)d ),
										   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
		for ( int impulse = 50; impulse <= 150; impulse += 10 )
		{
			FigureScene scene;
			CreateFigureScene( &scene, 1 );
			nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
			StepFigures( &scene, 30 );
			ENSURE( HitChest( figure, 1, b3MulSV( (float)impulse, direction ) ) );
			int steps = 0;
			while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 600 )
			{
				StepFigures( &scene, 1 );
				steps += 1;
			}
			nbFigureState state = nbFigure_GetState( figure );
			ENSURE( state == nb_figureAnimated || state == nb_figureRagdoll );
			bool catches = state == nb_figureAnimated;
			caught += catches ? 1 : 0;
			fell += catches ? 0 : 1;
			hitsTo80 += impulse <= 80 ? 1 : 0;
			caughtTo80 += impulse <= 80 && catches ? 1 : 0;
			longest = b3MaxFloat( longest, (float)steps / 60.0f );
			DestroyFigureScene( &scene );
		}
	}
	printf( "  caught %d of 88 hits, %d of %d up to 80 N s, fell %d times, the longest catch or fall %.2f s\n", caught,
			caughtTo80, hitsTo80, fell, longest );
	ENSURE( caughtTo80 >= hitsTo80 - 1 );
	ENSURE( caught >= 40 );
	ENSURE( longest < 6.0f );
	return 0;
}

// Where the feet of a figure face about up, in radians, each by the side of the foot
static void FeetYaw( nbFigureId figure, float* yaw )
{
	for ( int foot = 0; foot < 2; ++foot )
	{
		b3Quat q = b3Body_GetRotation( nbFigure_GetBody( figure, foot == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight ) );
		b3Vec3 side = b3RotateVector( q, b3Vec3_axisX );
		yaw[foot] = b3Atan2( -side.z, side.x );
	}
}

// How far the feet turned since they faced as before, on average, in radians
static float FeetTurn( nbFigureId figure, const float* before )
{
	float now[2];
	FeetYaw( figure, now );
	float turn = 0.0f;
	for ( int foot = 0; foot < 2; ++foot )
	{
		float delta = now[foot] - before[foot];
		delta = delta > B3_PI ? delta - 2.0f * B3_PI : ( delta < -B3_PI ? delta + 2.0f * B3_PI : delta );
		turn += 0.5f * delta;
	}
	return turn;
}

// A hit that turns the trunk about its long axis turns the whole figure on its feet only a little, see nbMeasure: the
// balance faces after its feet, plans each step from the stance foot and turns the pelvis back over the feet. Light
// hits on the upper chest from eight directions, 40 to 80 N s in steps of 8: none falls, and the feet of a figure that
// stands again face on average less than 10 degrees and at most 40 degrees away from where they faced. Round 33, which
// faced after its pelvis, turned them by 12.4 degrees on average and up to 67, and one of the 48 fell. Round 35 turned
// them by 4.7 degrees on average and up to 32. Round 36, with the arms of 81 cm and the stance of the reference picture,
// turned them by 6.2 on average and up to 43, both times by 80 N s from behind on a side, which it caught in two steps.
// Round 38, which lifts its feet 12 cm, turns them by 5.2 degrees on average and up to 28, round 40 by 8.2 and up to 26,
// and round 41, whose upper body bends on with the tilt of the pelvis, see nbBendTrunk, by 4.7 and up to 16.
static int FigureFacingTest( void )
{
	float sum = 0.0f;
	float largest = 0.0f;
	int hits = 0;
	int caught = 0;
	int steps = 0;
	for ( int d = 0; d < 8; ++d )
	{
		b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.25f * B3_PI * (float)d ),
										   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
		for ( int impulse = 40; impulse <= 80; impulse += 8 )
		{
			FigureScene scene;
			CreateFigureScene( &scene, 1 );
			nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
			StepFigures( &scene, 30 );
			float before[2];
			FeetYaw( figure, before );
			ENSURE( HitChest( figure, 1, b3MulSV( (float)impulse, direction ) ) );
			int updates = 0;
			while ( nbFigure_GetState( figure ) == nb_figureBalancing && updates < 600 )
			{
				StepFigures( &scene, 1 );
				updates += 1;
			}
			hits += 1;
			if ( nbFigure_GetState( figure ) == nb_figureAnimated )
			{
				float turn = FeetTurn( figure, before );
				sum += fabsf( turn );
				largest = b3MaxFloat( largest, fabsf( turn ) );
				caught += 1;
				steps += nbFigure_GetInfo( figure ).stepCount;
			}
			DestroyFigureScene( &scene );
		}
	}
	float mean = sum / (float)b3MaxInt( caught, 1 );
	printf( "  caught %d of %d light hits, the feet turned by %.1f degrees on average and %.1f at most, %.2f steps on average\n",
			caught, hits, mean * 180.0f / B3_PI, largest * 180.0f / B3_PI, (float)steps / (float)b3MaxInt( caught, 1 ) );
	ENSURE( caught == hits );
	ENSURE( mean < 10.0f * B3_PI / 180.0f );
	ENSURE( largest < 40.0f * B3_PI / 180.0f );
	return 0;
}

// The torque limit of the spring of the joint of a bone, and whether the spring is on. False for a joint without a spring.
static bool MuscleTorque( nbFigureId figure, int bone, float* limit, bool* on )
{
	b3JointId joint = nbFigure_GetJoint( figure, bone );
	if ( B3_IS_NULL( joint ) )
	{
		return false;
	}
	switch ( b3Joint_GetType( joint ) )
	{
		case b3_revoluteJoint:
			*limit = b3RevoluteJoint_GetMaxSpringTorque( joint );
			*on = b3RevoluteJoint_IsSpringEnabled( joint );
			return true;
		case b3_sphericalJoint:
			*limit = b3SphericalJoint_GetMaxSpringTorque( joint );
			*on = b3SphericalJoint_IsSpringEnabled( joint );
			return true;
		default:
			return false;
	}
}

// A figure with muscles, see nbFigureMotors::muscles, hit on its chest from the front while it stands, and the hash of
// the transforms of its bones after four seconds
static uint32_t MuscleCatch( int threadCount, float impulse )
{
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.motors.muscles = true;
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	StepFigures( &scene, 30 );
	HitChest( figure, 1, (b3Vec3){ 0.0f, 0.0f, -impulse } );
	StepFigures( &scene, 240 );

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
	DestroyFigureScene( &scene );
	return hash;
}

// The throw of 90 N s on a figure with muscles ends in the same bits with one thread and with four, on every platform
#define NB_EXPECTED_MUSCLE_HASH 0x47709c27u

// The muscles, see nbFigureMotors::muscles and nbUpdateMuscles. A figure with muscles hit while it stands catches itself
// with the strength the balance asks for at once: every joint of its trunk and its legs at the full torque of the motors,
// its arms loose with a part of it. While it catches itself the joints of its trunk pull less while they turn the way
// they pull and more while they are turned against it, up to the braking strength, its legs keep the full torque, which
// they have no top speed for. Knocked down it goes slack at once, and as a ragdoll its muscles follow the strength of its
// flight as a lag, without overshooting it. It gives the same bits with any number of threads.
static int FigureMuscleTest( void )
{
	const nbFigureMotors motors = nbDefaultFigureMotors();
	ENSURE( motors.muscles == false );

	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.motors.muscles = true;
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	StepFigures( &scene, 30 );
	ENSURE( HitChest( figure, 1, (b3Vec3){ 0.0f, 0.0f, -70.0f } ) );
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		float limit = 0.0f;
		bool on = false;
		if ( MuscleTorque( figure, i, &limit, &on ) )
		{
			float full = motors.maxTorque * scene.bones[i].strength;
			bool arm = nb_humanoidUpperArmLeft <= i && i < nb_humanoidThighLeft;
			ENSURE( on );
			ENSURE( arm ? 0.0f < limit && limit < full : limit == full );
		}
	}

	bool slower = false;
	bool harder = false;
	int steps = 0;
	while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 600 )
	{
		StepFigures( &scene, 1 );
		steps += 1;
		for ( int i = 1; i < nb_humanoidBoneCount; ++i )
		{
			float limit = 0.0f;
			bool on = false;
			if ( MuscleTorque( figure, i, &limit, &on ) )
			{
				float full = motors.maxTorque * scene.bones[i].strength;
				ENSURE( on );
				if ( i < nb_humanoidUpperArmLeft )
				{
					slower = slower || limit < full;
					harder = harder || limit > full;
					ENSURE( 0.0f <= limit && limit <= motors.brakeStrength * full );
				}
				else
				{
					ENSURE( i < nb_humanoidThighLeft ? limit <= full : limit == full );
				}
			}
		}
	}
	printf( "  70 N s from the front: caught after %.2f s, the trunk pulled with less and with more than its torque at rest\n",
			(float)steps / 60.0f );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	ENSURE( slower && harder );

	// Knocked down it goes slack at once, then the muscles of a forearm follow the strength of the ragdoll
	nbFigure_Knockdown( figure );
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		float limit = 0.0f;
		bool on = true;
		if ( MuscleTorque( figure, i, &limit, &on ) )
		{
			ENSURE( on == false );
		}
	}
	float full = motors.maxTorque * scene.bones[nb_humanoidForearmLeft].strength;
	float previous = 0.0f;
	int lagged = 0;
	for ( int step = 0; step < 300 && nbFigure_GetState( figure ) == nb_figureRagdoll; ++step )
	{
		StepFigures( &scene, 1 );
		nbFigureInfo info = nbFigure_GetInfo( figure );
		if ( info.state != nb_figureRagdoll )
		{
			break;
		}
		float limit = 0.0f;
		bool on = false;
		ENSURE( MuscleTorque( figure, nb_humanoidForearmLeft, &limit, &on ) );
		float asked = info.strength >= 0.01f ? info.strength : 0.0f;
		float activation = on ? limit / full : 0.0f;
		if ( on && previous > 0.0f )
		{
			// Read back from the torque limit of the spring, an activation on its target lies within a few bits of it
			float off = fabsf( activation - asked ) > 1.0e-6f ? activation - asked : 0.0f;
			ENSURE( ( previous - asked ) * off >= 0.0f );
			ENSURE( fabsf( activation - asked ) <= fabsf( previous - asked ) + 1.0e-6f );
			lagged += fabsf( activation - asked ) > 0.01f ? 1 : 0;
		}
		previous = activation;
	}
	printf( "  knocked down: the forearm behind the strength of the ragdoll by more than 0.01 in %d updates\n", lagged );
	ENSURE( lagged > 0 );
	DestroyFigureScene( &scene );

	uint32_t one = MuscleCatch( 1, 90.0f );
	uint32_t four = MuscleCatch( 4, 90.0f );
	printf( "  muscle hash: 0x%08x\n", one );
	ENSURE( one == four );
	ENSURE( one == NB_EXPECTED_MUSCLE_HASH );
	return 0;
}

#if defined( B3_HAS_SWING_LIMITS )
// How far past its limits a joint of a figure lies, in radians: the swing of a ball joint past the edge of its cone and
// its twist past its range, the angle of a hinge past its range
static float JointPastLimits( nbFigureId figure, int bone )
{
	b3JointId joint = nbFigure_GetJoint( figure, bone );
	if ( b3Joint_GetType( joint ) == b3_revoluteJoint )
	{
		float angle = b3RevoluteJoint_GetAngle( joint );
		return b3MaxFloat( b3RevoluteJoint_GetLowerLimit( joint ) - angle, angle - b3RevoluteJoint_GetUpperLimit( joint ) );
	}
	if ( b3Joint_GetType( joint ) != b3_sphericalJoint )
	{
		return 0.0f;
	}
	float twist = b3SphericalJoint_GetTwistAngle( joint );
	float past = b3MaxFloat( b3SphericalJoint_GetLowerTwistLimit( joint ) - twist,
							 twist - b3SphericalJoint_GetUpperTwistLimit( joint ) );
	if ( b3SphericalJoint_IsSwingLimitEnabled( joint ) )
	{
		float lowerX, upperX, lowerY, upperY;
		b3SphericalJoint_GetSwingLimits( joint, &lowerX, &upperX, &lowerY, &upperY );
		float limits[4] = { b3TanQuarter( upperX ), b3TanQuarter( -lowerX ), b3TanQuarter( upperY ), b3TanQuarter( -lowerY ) };
		b3Quat frameA = b3MulQuat( b3Body_GetRotation( b3Joint_GetBodyA( joint ) ), b3Joint_GetLocalFrameA( joint ).q );
		b3Quat frameB = b3MulQuat( b3Body_GetRotation( b3Joint_GetBodyB( joint ) ), b3Joint_GetLocalFrameB( joint ).q );
		past = b3MaxFloat( past, -b3GetSwingLimitSeparation( b3InvMulQuat( frameA, frameB ), limits, NULL ) );
	}
	return past;
}

// The joints of the humanoid move as far as a person's, see nbMakeHumanoidBones. A hit of 90 N s from behind on the
// right forearm swings the arm up past the head, 162 degrees, a hit from the front swings it back 48 degrees in the
// plane of the trunk, a little out, as far as the 40 degrees of a shoulder and the ellipse toward its 160 out let it.
// With a cone of 86 degrees all round it swung up to 89 and back to 90. Box3D holds the limits softly: right after the
// hit the hand, which takes 6 N s of it on 0.8 kg, swings up to 35 degrees past its limits, from the fifth step on no
// joint lies more than 15.5 degrees past them: the elbow, which the hit from the front straightens out of its bend of
// 29 degrees in the stand of round 36, as in the reference picture, 14.5 past its limit in the fifth step, 13.4 in the
// sixth and 10.8 in the seventh, as the arm hangs from the upper part of the chest of round 37, which moves more
// lightly than the chest of one part. On active-ragdoll-36 it went 12 and 8.5 past it, with the bend of 17 degrees of
// active-ragdoll-35 10, with the 9 of active-ragdoll-34 8.
static int FigureJointRangeTest( void )
{
	float most[2] = { 0.0f, 0.0f };
	float worst = 0.0f;
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 30 );
		b3BodyId forearm = nbFigure_GetBody( figure, nb_humanoidForearmRight );
		b3Vec3 local = b3Lerp( scene.bones[nb_humanoidForearmRight].start, scene.bones[nb_humanoidForearmRight].end, 0.75f );
		nbHitDef hit = { 1, nb_humanoidForearmRight, (b3Vec3){ 0.0f, 0.0f, pass == 0 ? 90.0f : -90.0f },
						 b3Body_GetWorldPoint( forearm, local ) };
		ENSURE( nbFigure_ApplyHit( figure, &hit ) );
		for ( int step = 0; step < 120; ++step )
		{
			StepFigures( &scene, 1 );
			b3Quat chest = b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidChest ) );
			b3Vec3 arm = b3RotateVector( b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidUpperArmRight ) ), b3Vec3_axisY );
			b3Vec3 down = b3RotateVector( chest, (b3Vec3){ 0.0f, -1.0f, 0.0f } );
			b3Vec3 forward = b3RotateVector( chest, b3Vec3_axisZ );
			float ahead = b3Dot( arm, forward );
			float swing = pass == 0 ? acosf( b3ClampFloat( b3Dot( arm, down ), -1.0f, 1.0f ) )
									: atan2f( -ahead, b3Dot( arm, down ) );
			if ( ( pass == 0 ) == ( ahead > 0.0f ) )
			{
				most[pass] = b3MaxFloat( most[pass], swing );
			}
			for ( int i = 1; step >= 4 && i < nb_humanoidBoneCount; ++i )
			{
				worst = b3MaxFloat( worst, JointPastLimits( figure, i ) );
			}
		}
		DestroyFigureScene( &scene );
	}
	printf( "  90 N s on the right forearm: the arm swings up to %.0f degrees forward, %.0f back, from the fifth step on no "
			"joint more than %.1f degrees past its limits\n",
			most[0] * 180.0f / B3_PI, most[1] * 180.0f / B3_PI, worst * 180.0f / B3_PI );
	ENSURE( most[0] > 2.4f );
	ENSURE( 0.5f < most[1] && most[1] < 1.0f );
	ENSURE( worst < 0.27f );
	return 0;
}
#endif

// The parts of a hit of 10 N s on the center of a bone of a ragdoll that every bone takes, read back from the change of
// its momentum
static void SpreadParts( int bone, float* parts )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
	StepFigures( &scene, 30 );
	nbFigure_Knockdown( figure );
	b3Vec3 before[nb_humanoidBoneCount];
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		before[i] = b3Body_GetLinearVelocity( nbFigure_GetBody( figure, i ) );
	}
	nbHitDef hit = { 7, bone, (b3Vec3){ 0.0f, 0.0f, -10.0f }, b3Body_GetWorldCenter( nbFigure_GetBody( figure, bone ) ) };
	nbFigure_ApplyHit( figure, &hit );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		b3BodyId body = nbFigure_GetBody( figure, i );
		b3Vec3 change = b3Sub( b3Body_GetLinearVelocity( body ), before[i] );
		parts[i] = -b3Body_GetMass( body ) * change.z / 10.0f;
	}
	DestroyFigureScene( &scene );
}

// A hit on the dynamic bones of a figure spreads along its skeleton, see nbSpreadHit. On the right forearm the forearm
// takes half of it and the pelvis beside it little, measured straight the forearm took a fifth and the pelvis as much. On
// the left shin the shin takes three quarters and the right leg 2 %, measured straight the right leg took a fifth. On the
// middle of the upper part of the chest the two parts take 72 % together, shared by their masses as one rib cage, 42 on
// the upper one and 30 on the lower one, as the chest of one part took three quarters before round 37. All the parts add
// up to the hit.
static int FigureSpreadTest( void )
{
	float forearm[nb_humanoidBoneCount];
	float shin[nb_humanoidBoneCount];
	float chest[nb_humanoidBoneCount];
	SpreadParts( nb_humanoidForearmRight, forearm );
	SpreadParts( nb_humanoidShinLeft, shin );
	SpreadParts( nb_humanoidChest, chest );
	float sums[3] = { 0.0f, 0.0f, 0.0f };
	float rightLeg = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		sums[0] += forearm[i];
		sums[1] += shin[i];
		sums[2] += chest[i];
		ENSURE( forearm[i] <= forearm[nb_humanoidForearmRight] );
		ENSURE( shin[i] <= shin[nb_humanoidShinLeft] );
		ENSURE( chest[i] <= chest[nb_humanoidChest] );
		rightLeg += i >= nb_humanoidThighRight ? shin[i] : 0.0f;
	}
	float wholeChest = chest[nb_humanoidChest] + chest[nb_humanoidLowerChest];
	printf( "  10 N s on the right forearm: %.0f %% on it, %.0f %% on the pelvis, on the left shin: %.0f %% on it, %.1f %% on "
			"the right leg, on the chest: %.0f %% on it and %.0f %% on its lower part\n",
			100.0f * forearm[nb_humanoidForearmRight], 100.0f * forearm[nb_humanoidPelvis], 100.0f * shin[nb_humanoidShinLeft],
			100.0f * rightLeg, 100.0f * chest[nb_humanoidChest], 100.0f * chest[nb_humanoidLowerChest] );
	for ( int k = 0; k < 3; ++k )
	{
		ENSURE_SMALL( sums[k] - 1.0f, 1.0e-3 );
	}
	ENSURE( forearm[nb_humanoidForearmRight] > 0.4f && forearm[nb_humanoidPelvis] < 0.1f );
	ENSURE( shin[nb_humanoidShinLeft] > 0.7f && rightLeg < 0.03f );
	ENSURE( wholeChest > 0.7f );
	return 0;
}

// The neck weighs a kilogram between the chest and the head, and at the full stiffness of the motors its two springs
// turn it so fast that they shake the head, which is why the humanoid pulls its neck at 0.3 of it, see
// nbMakeHumanoidBones. With the armature of the humanoid on both joints, see nbBoneDef::armature, a figure with its neck
// at the full stiffness that catches hits of 60 N s from four sides keeps its head calm: its turn against the neck
// changes by about a radian per second from one step to the next and reverses in a fifth of the steps, and it stands
// again after every catch. Without the armature the turn changes by over ten times that and reverses in every other
// step. Both without the damping of neck and head while catching, see nbBoneDef::catchDamping, which calms them too, and
// with a spine that keeps its stiffness after the hits, see nbBoneDef::hitStiffness, which moves the chest under them.
// Since round 61, whose stance foot stays planted where it last carried, see NB_PLANT_LOAD, the whole catch is calmer:
// the turn changes by 0.26 instead of 0.76 rad/s and reverses in 4 instead of 10 % of the steps, without the armature
// by 2.05 instead of 15.58 rad/s and in 38 instead of 56 %, still eight times the change and nine times the reversals.
static int FigureNeckTest( void )
{
	float jitter[2] = { 0.0f, 0.0f };
	float flips[2] = { 0.0f, 0.0f };
	int standing[2] = { 0, 0 };
	for ( int pass = 0; pass < 2; ++pass )
	{
		int steps = 0;
		for ( int d = 0; d < 4; ++d )
		{
			FigureScene scene;
			CreateFigureScene( &scene, 1 );
			scene.bones[nb_humanoidNeck].stiffness = 1.0f;
			scene.bones[nb_humanoidHead].stiffness = 1.0f;
			scene.bones[nb_humanoidNeck].catchDamping = 1.0f;
			scene.bones[nb_humanoidHead].catchDamping = 1.0f;
			for ( int k = 0; k < SPINE_BONES; ++k )
			{
				scene.bones[nb_humanoidBelly + k].hitStiffness = 1.0f;
			}
			if ( pass == 1 )
			{
				scene.bones[nb_humanoidNeck].armature = 0.0f;
				scene.bones[nb_humanoidHead].armature = 0.0f;
			}
			nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
			StepFigures( &scene, 30 );
			b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI * (float)d ),
											   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
			ENSURE( HitChest( figure, 1, b3MulSV( 60.0f, direction ) ) );
			b3BodyId head = nbFigure_GetBody( figure, nb_humanoidHead );
			b3BodyId neck = nbFigure_GetBody( figure, nb_humanoidNeck );
			b3Vec3 last = b3Vec3_zero;
			for ( int i = 0; i < 600; ++i )
			{
				StepFigures( &scene, 1 );
				if ( nbFigure_GetState( figure ) != nb_figureBalancing )
				{
					break;
				}
				b3Vec3 turn = b3Sub( b3Body_GetAngularVelocity( head ), b3Body_GetAngularVelocity( neck ) );
				if ( i > 0 )
				{
					jitter[pass] += b3Length( b3Sub( turn, last ) );
					flips[pass] += b3Dot( turn, last ) < 0.0f && b3Length( turn ) > 0.5f ? 1.0f : 0.0f;
					steps += 1;
				}
				last = turn;
			}
			standing[pass] += nbFigure_GetState( figure ) == nb_figureAnimated ? 1 : 0;
			DestroyFigureScene( &scene );
		}
		jitter[pass] /= (float)b3MaxInt( steps, 1 );
		flips[pass] /= (float)b3MaxInt( steps, 1 );
	}
	printf( "  catching 60 N s: the head turns against the neck by %.2f rad/s more or less from one step to the next and "
			"reverses in %.0f %% of the steps, standing after %d of 4, without the armature %.2f rad/s, %.0f %% and %d of 4\n",
			jitter[0], 100.0f * flips[0], standing[0], jitter[1], 100.0f * flips[1], standing[1] );
	ENSURE( jitter[0] < 2.0f && flips[0] < 0.25f && standing[0] == 4 );
	ENSURE( jitter[1] > 5.0f * jitter[0] && flips[1] > 5.0f * flips[0] );
	return 0;
}

// While the humanoid catches itself, its neck and head take eight times the damping, see nbBoneDef::catchDamping: after
// hits of 60 N s on the upper chest from four sides the head turns against the neck less than half as fast at most,
// changes that turn from one step to the next by less, and still goes along with the hit, and the figure stands again
// after every catch.
static int FigureCatchDampingTest( void )
{
	float peak[2] = { 0.0f, 0.0f };
	float jitter[2] = { 0.0f, 0.0f };
	float swing[2] = { 0.0f, 0.0f };
	int standing[2] = { 0, 0 };
	for ( int pass = 0; pass < 2; ++pass )
	{
		int steps = 0;
		for ( int d = 0; d < 4; ++d )
		{
			FigureScene scene;
			CreateFigureScene( &scene, 1 );
			if ( pass == 1 )
			{
				scene.bones[nb_humanoidNeck].catchDamping = 1.0f;
				scene.bones[nb_humanoidHead].catchDamping = 1.0f;
			}
			nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
			StepFigures( &scene, 30 );
			b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI * (float)d ),
											   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
			ENSURE( HitChest( figure, 1, b3MulSV( 60.0f, direction ) ) );
			b3BodyId chest = nbFigure_GetBody( figure, nb_humanoidChest );
			b3BodyId neck = nbFigure_GetBody( figure, nb_humanoidNeck );
			b3BodyId head = nbFigure_GetBody( figure, nb_humanoidHead );
			b3Quat rest = b3InvMulQuat( b3Body_GetRotation( chest ), b3Body_GetRotation( head ) );
			b3Vec3 last = b3Vec3_zero;
			for ( int i = 0; i < 600; ++i )
			{
				StepFigures( &scene, 1 );
				if ( nbFigure_GetState( figure ) != nb_figureBalancing )
				{
					break;
				}
				b3Vec3 turn = b3Sub( b3Body_GetAngularVelocity( head ), b3Body_GetAngularVelocity( neck ) );
				if ( i < 60 )
				{
					b3Quat move =
						b3MulQuat( b3InvMulQuat( b3Body_GetRotation( chest ), b3Body_GetRotation( head ) ), b3Conjugate( rest ) );
					peak[pass] = b3MaxFloat( peak[pass], b3Length( turn ) );
					swing[pass] = b3MaxFloat( swing[pass], 2.0f * acosf( b3ClampFloat( fabsf( move.s ), 0.0f, 1.0f ) ) );
				}
				if ( i > 0 )
				{
					jitter[pass] += b3Length( b3Sub( turn, last ) );
					steps += 1;
				}
				last = turn;
			}
			standing[pass] += nbFigure_GetState( figure ) == nb_figureAnimated ? 1 : 0;
			DestroyFigureScene( &scene );
		}
		jitter[pass] /= (float)b3MaxInt( steps, 1 );
	}
	printf( "  catching 60 N s: the head turns against the neck by up to %.2f instead of %.2f rad/s, changes that by %.3f "
			"instead of %.3f rad/s from one step to the next, swings up to %.1f instead of %.1f deg against the chest, standing "
			"after %d and %d of 4\n",
			peak[0], peak[1], jitter[0], jitter[1], swing[0] * 180.0f / B3_PI, swing[1] * 180.0f / B3_PI, standing[0],
			standing[1] );
	ENSURE( peak[0] < 0.6f * peak[1] && jitter[0] < 0.7f * jitter[1] );
	ENSURE( swing[0] > 0.1f );
	ENSURE( standing[0] == 4 );
	return 0;
}

// A figure that catches itself at a slower rate than 60 Hz stands again, see NB_TUNED_STEP: light hits on the chest
// after which the standing figure bobbed up and down from one update to the next before round 65 and found no rest for
// eight seconds, 70 N s at 30 Hz and 56 N s from the side at 40 Hz, with the hold against the world.
static int FigureSlowRateTest( void )
{
	const float rates[2] = { 30.0f, 40.0f };
	const int directions[2] = { 5, 2 };
	const float impulses[2] = { 70.0f, 56.0f };
	for ( int c = 0; c < 2; ++c )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		float timeStep = 1.0f / rates[c];
		int subSteps = nbGetSubStepCount( rates[c] );
		for ( int i = 0; i < (int)( 0.5f * rates[c] + 0.5f ); ++i )
		{
			nbWorld_Step( scene.world, timeStep, subSteps );
		}
		b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.25f * B3_PI * (float)directions[c] ),
										   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
		ENSURE( HitChest( figure, 1, b3MulSV( impulses[c], direction ) ) );
		int steps = 0;
		while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < (int)( 10.0f * rates[c] ) )
		{
			nbWorld_Step( scene.world, timeStep, subSteps );
			steps += 1;
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		printf( "  %.0f N s at %.0f Hz: %s after %.2f s and %d steps\n", impulses[c], rates[c],
				info.state == nb_figureAnimated ? "standing" : "fallen", (float)steps / rates[c], info.stepCount );
		ENSURE( info.state == nb_figureAnimated && info.knockdownCount == 0 );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A figure that catches itself with its own strength at 30 Hz takes the targets of its steps ahead, see NB_STEP_LEAD:
// hard hits on the chest it fell after before round 66, 100 N s diagonally from the front and 92 N s from the side
static int FigureSlowStepTest( void )
{
	const int directions[2] = { 1, 2 };
	const float impulses[2] = { 100.0f, 92.0f };
	for ( int c = 0; c < 2; ++c )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureDef def = nbDefaultFigureDef();
		def.rig = &scene.rig;
		def.standHeight = scene.standHeight;
		def.motors.uprightAgainstWorld = false;
		nbFigureId figure = nbCreateFigure( scene.world, &def );
		float timeStep = 1.0f / 30.0f;
		int subSteps = nbGetSubStepCount( 30.0f );
		for ( int i = 0; i < 15; ++i )
		{
			nbWorld_Step( scene.world, timeStep, subSteps );
		}
		b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.25f * B3_PI * (float)directions[c] ),
										   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
		ENSURE( HitChest( figure, 1, b3MulSV( impulses[c], direction ) ) );
		int steps = 0;
		while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 300 )
		{
			nbWorld_Step( scene.world, timeStep, subSteps );
			steps += 1;
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		printf( "  %.0f N s at 30 Hz with its own strength: %s after %.2f s and %d steps\n", impulses[c],
				info.state == nb_figureAnimated ? "standing" : "fallen", (float)steps / 30.0f, info.stepCount );
		ENSURE( info.state == nb_figureAnimated && info.knockdownCount == 0 );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// Without a hold a figure at 30 Hz takes the targets of its steps ahead as well and keeps its swinging foot out of the
// stance foot, see NB_KEEP_TIME: light hits from the side it fell after before round 71, 70 N s from one side and
// 78 N s from the other
static int FigureSlowFreeStepTest( void )
{
	const int directions[2] = { 2, 6 };
	const float impulses[2] = { 70.0f, 78.0f };
	for ( int c = 0; c < 2; ++c )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureDef def = nbDefaultFigureDef();
		def.rig = &scene.rig;
		def.standHeight = scene.standHeight;
		def.motors.uprightTorque = 0.0f;
		nbFigureId figure = nbCreateFigure( scene.world, &def );
		float timeStep = 1.0f / 30.0f;
		int subSteps = nbGetSubStepCount( 30.0f );
		for ( int i = 0; i < 15; ++i )
		{
			nbWorld_Step( scene.world, timeStep, subSteps );
		}
		b3Vec3 direction = b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.25f * B3_PI * (float)directions[c] ),
										   (b3Vec3){ 0.0f, 0.0f, -1.0f } );
		ENSURE( HitChest( figure, 1, b3MulSV( impulses[c], direction ) ) );
		int steps = 0;
		while ( nbFigure_GetState( figure ) == nb_figureBalancing && steps < 300 )
		{
			nbWorld_Step( scene.world, timeStep, subSteps );
			steps += 1;
		}
		nbFigureInfo info = nbFigure_GetInfo( figure );
		printf( "  %.0f N s from the side at 30 Hz without a hold: %s after %.2f s and %d steps\n", impulses[c],
				info.state == nb_figureAnimated ? "standing" : "fallen", (float)steps / 30.0f, info.stepCount );
		ENSURE( info.state == nb_figureAnimated && info.knockdownCount == 0 );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The spring of the joint of a bone of a figure in cycles per second, zero without a spring
static float SpringHertz( nbFigureId figure, int bone )
{
	b3JointId joint = nbFigure_GetJoint( figure, bone );
	if ( B3_IS_NULL( joint ) )
	{
		return 0.0f;
	}
	switch ( b3Joint_GetType( joint ) )
	{
		case b3_revoluteJoint:
			return b3RevoluteJoint_GetSpringHertz( joint );
		case b3_sphericalJoint:
			return b3SphericalJoint_GetSpringHertz( joint );
		default:
			return 0.0f;
	}
}

// The spine gives way to a hit, see nbBoneDef::hitStiffness: right after 60 N s on the upper chest from ahead or behind its
// four joints pull with the share of their stiffness the humanoid gives them and the same torque limit, half a second
// later with all of it again, and in the first second the chest bends farther against the pelvis than with joints that
// keep their stiffness. From its right the spine keeps its stiffness and the figure moves as with stiff joints, from
// ahead and its right at once the joints give way by half. The figure stands again after every catch.
static int FigureSoftSpineTest( void )
{
	enum
	{
		directionCount = 4
	};
	const char* names[directionCount] = { "ahead", "behind", "its right", "ahead and its right" };
	const b3Vec3 impulses[directionCount] = {
		{ 0.0f, 0.0f, -60.0f }, { 0.0f, 0.0f, 60.0f }, { 60.0f, 0.0f, 0.0f }, { 42.42641f, 0.0f, -42.42641f } };
	const float shares[directionCount] = { 1.0f, 1.0f, 0.0f, 0.5f };
	float keep = 1.0f;
	float bend[2][directionCount] = { { 0.0f } };
	float hertzAfterHit[2][directionCount][SPINE_BONES];
	float hertzLater[2][directionCount][SPINE_BONES];
	float torqueAfterHit[2][directionCount][SPINE_BONES];
	int standing[2] = { 0, 0 };
	for ( int pass = 0; pass < 2; ++pass )
	{
		for ( int d = 0; d < directionCount; ++d )
		{
			FigureScene scene;
			CreateFigureScene( &scene, 1 );
			keep = scene.bones[nb_humanoidChest].hitStiffness;
			ENSURE( keep < 1.0f );
			for ( int k = 0; k < SPINE_BONES; ++k )
			{
				ENSURE( scene.bones[nb_humanoidBelly + k].hitStiffness == keep );
			}
			if ( pass == 1 )
			{
				for ( int k = 0; k < SPINE_BONES; ++k )
				{
					scene.bones[nb_humanoidBelly + k].hitStiffness = 1.0f;
				}
			}
			nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
			StepFigures( &scene, 30 );
			b3BodyId chest = nbFigure_GetBody( figure, nb_humanoidChest );
			b3BodyId pelvis = nbFigure_GetBody( figure, nb_humanoidPelvis );
			b3Quat rest = b3InvMulQuat( b3Body_GetRotation( pelvis ), b3Body_GetRotation( chest ) );
			ENSURE( HitChest( figure, 1, impulses[d] ) );
			ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
			for ( int k = 0; k < SPINE_BONES; ++k )
			{
				bool on = false;
				hertzAfterHit[pass][d][k] = SpringHertz( figure, nb_humanoidBelly + k );
				ENSURE( MuscleTorque( figure, nb_humanoidBelly + k, &torqueAfterHit[pass][d][k], &on ) && on );
			}
			for ( int i = 0; i < 600 && nbFigure_GetState( figure ) == nb_figureBalancing; ++i )
			{
				StepFigures( &scene, 1 );
				if ( i < 60 )
				{
					b3Quat change = b3MulQuat( b3InvMulQuat( b3Body_GetRotation( pelvis ), b3Body_GetRotation( chest ) ),
											   b3Conjugate( rest ) );
					float angle = acosf( b3ClampFloat( b3RotateVector( change, b3Vec3_axisY ).y, -1.0f, 1.0f ) );
					bend[pass][d] = b3MaxFloat( bend[pass][d], angle );
				}
				if ( i == 29 )
				{
					ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
					for ( int k = 0; k < SPINE_BONES; ++k )
					{
						hertzLater[pass][d][k] = SpringHertz( figure, nb_humanoidBelly + k );
					}
				}
			}
			standing[pass] += nbFigure_GetState( figure ) == nb_figureAnimated ? 1 : 0;
			DestroyFigureScene( &scene );
		}
	}
	for ( int d = 0; d < directionCount; ++d )
	{
		float ratio = hertzAfterHit[0][d][0] / hertzAfterHit[1][d][0];
		printf( "  60 N s on the upper chest from %s: the spine at %.3f of its stiffness, the chest bends up to %.1f deg against "
				"the pelvis, with stiff joints %.1f deg\n",
				names[d], ratio * ratio, bend[0][d] * 180.0f / B3_PI, bend[1][d] * 180.0f / B3_PI );
		for ( int k = 0; k < SPINE_BONES; ++k )
		{
			ratio = hertzAfterHit[0][d][k] / hertzAfterHit[1][d][k];
			ENSURE( fabsf( ratio - sqrtf( 1.0f - shares[d] * ( 1.0f - keep ) ) ) < ( d == 3 ? 0.02f : 1.0e-3f ) );
			ENSURE( torqueAfterHit[0][d][k] == torqueAfterHit[1][d][k] );
			ENSURE( fabsf( hertzLater[0][d][k] - hertzLater[1][d][k] ) <= 1.0e-4f * hertzLater[1][d][k] );
		}
	}
	ENSURE( bend[0][0] > 1.15f * bend[1][0] && bend[0][1] > 1.15f * bend[1][1] );
	ENSURE( bend[0][2] == bend[1][2] );
	ENSURE( standing[0] == directionCount && standing[1] == directionCount );
	return 0;
}

// After a hard hit the spine of a figure that catches itself without the hold against the world stays soft for less
// time, see NB_SOFT_HARD_CUT: with its own strength or without a hold its joints stiffen again from 0.075 s on after
// 100 N s on the upper chest from ahead, twice the knockdown impulse, from 0.1125 s on after 87.5 N s and from 0.15 s
// on after 60 N s, as with the hold against the world after 100 N s. The first update that finds the spine stiffer
// comes within one update after that time.
static int FigureSoftHoldTest( void )
{
	enum
	{
		caseCount = 5
	};
	const float impulses[caseCount] = { 100.0f, 87.5f, 60.0f, 100.0f, 100.0f };
	const float torques[caseCount] = { 125.0f, 125.0f, 125.0f, 125.0f, 0.0f };
	const bool againstWorld[caseCount] = { false, false, false, true, true };
	const float holds[caseCount] = { 0.075f, 0.1125f, 0.15f, 0.15f, 0.075f };
	for ( int c = 0; c < caseCount; ++c )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureDef def = nbDefaultFigureDef();
		def.rig = &scene.rig;
		def.standHeight = scene.standHeight;
		def.motors.uprightAgainstWorld = againstWorld[c];
		def.motors.uprightTorque = torques[c];
		nbFigureId figure = nbCreateFigure( scene.world, &def );
		StepFigures( &scene, 30 );
		ENSURE( HitChest( figure, 1, (b3Vec3){ 0.0f, 0.0f, -impulses[c] } ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
		float soft = SpringHertz( figure, nb_humanoidBelly );
		int stiffer = 0;
		for ( int i = 1; i <= 15 && stiffer == 0; ++i )
		{
			StepFigures( &scene, 1 );
			ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
			stiffer = SpringHertz( figure, nb_humanoidBelly ) > soft ? i : 0;
		}
		printf( "  %.1f N s on the upper chest from ahead %s: the spine stiffens again after %.3f s\n", impulses[c],
				torques[c] == 0.0f ? "without a hold"
								   : ( againstWorld[c] ? "with the hold against the world" : "with its own strength" ),
				(float)stiffer / 60.0f );
		ENSURE( (float)stiffer > 60.0f * holds[c] && (float)stiffer < 60.0f * holds[c] + 1.01f );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A pusher for FigurePushTest, without gravity, led toward the figure at the height through a motor joint whose springs
// pull it with up to the force after a kinematic hand, as a stick or a hand pushes: a cube of 20 cm and 5 kg from the
// front, or a stick from its left side, 80 cm long from front to back, 5 cm thick and 1.5 kg
typedef struct Pusher
{
	b3BodyId hand;
	b3BodyId body;
} Pusher;

static Pusher AddPusher( FigureScene* scene, float height, float force, bool stick )
{
	Pusher pusher;
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_kinematicBody;
	bodyDef.position = b3ToPos( stick ? (b3Vec3){ 0.5f, height, 0.0f } : (b3Vec3){ 0.0f, height, 0.45f } );
	pusher.hand = b3CreateBody( scene->physicsWorld, &bodyDef );
	bodyDef.type = b3_dynamicBody;
	bodyDef.gravityScale = 0.0f;
	bodyDef.enableSleep = false;
	pusher.body = b3CreateBody( scene->physicsWorld, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	if ( stick )
	{
		b3Capsule capsule = { { 0.0f, 0.0f, -0.4f }, { 0.0f, 0.0f, 0.4f }, 0.025f };
		float volume = B3_PI * 0.025f * 0.025f * 0.8f + 4.0f / 3.0f * B3_PI * 0.025f * 0.025f * 0.025f;
		shapeDef.density = 1.5f / volume;
		b3CreateCapsuleShape( pusher.body, &shapeDef, &capsule );
	}
	else
	{
		b3BoxHull cube = b3MakeBoxHull( 0.1f, 0.1f, 0.1f );
		shapeDef.density = 5.0f / 0.008f;
		b3CreateHullShape( pusher.body, &shapeDef, &cube.base );
	}
	b3MotorJointDef motor = b3DefaultMotorJointDef();
	motor.base.bodyIdA = pusher.hand;
	motor.base.bodyIdB = pusher.body;
	motor.linearHertz = 5.0f;
	motor.linearDampingRatio = 1.0f;
	motor.maxSpringForce = force;
	motor.angularHertz = 5.0f;
	motor.angularDampingRatio = 1.0f;
	motor.maxSpringTorque = 0.2f * force;
	b3CreateMotorJoint( scene->physicsWorld, &motor );
	return pusher;
}

typedef struct PushResult
{
	int wakeStep;
	int changes;
	int balanceSteps;
	int knockdowns;
	int standStep;
	float pelvisMove;
	float headTurn;
	nbFigureState end;
} PushResult;

// A figure pushed from the front, or with the stick from its left, the hand 0.3 m/s toward it for 2.5 s, then away
static PushResult Push( float height, float force, float pushForce, bool stick, int threadCount, uint32_t* hash )
{
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene.rig;
	def.standHeight = scene.standHeight;
	def.pushForce = pushForce;
	nbFigureId figure = nbCreateFigure( scene.world, &def );
	StepFigures( &scene, 30 );

	Pusher pusher = AddPusher( &scene, height, force, stick );
	PushResult result = { -1, 0, 0, 0, -1, 0.0f, 0.0f, nb_figureAnimated };
	b3Vec3 pelvis0 = BonePosition( figure, nb_humanoidPelvis );
	b3Quat head0 = b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidHead ) );
	nbFigureStats before = nbWorld_GetFigureStats( scene.world );
	nbFigureState last = nb_figureAnimated;
	for ( int i = 0; i < 600; ++i )
	{
		float speed = i < 150 ? -0.3f : ( i < 210 ? 1.5f : 0.0f );
		b3Body_SetLinearVelocity( pusher.hand, stick ? (b3Vec3){ speed, 0.0f, 0.0f } : (b3Vec3){ 0.0f, 0.0f, speed } );
		StepFigures( &scene, 1 );
		nbFigureState state = nbFigure_GetState( figure );
		result.changes += state != last ? 1 : 0;
		last = state;
		if ( result.wakeStep < 0 && state != nb_figureAnimated )
		{
			result.wakeStep = i;
		}
		if ( i >= 210 && result.standStep < 0 && result.wakeStep >= 0 && state == nb_figureAnimated )
		{
			result.standStep = i - 210;
		}
		b3Vec3 pelvis = BonePosition( figure, nb_humanoidPelvis );
		result.pelvisMove = b3MaxFloat( result.pelvisMove, b3Length( b3Sub( pelvis, pelvis0 ) ) );
		b3Quat turn = b3InvMulQuat( head0, b3Body_GetRotation( nbFigure_GetBody( figure, nb_humanoidHead ) ) );
		result.headTurn = b3MaxFloat( result.headTurn, 2.0f * acosf( b3MinFloat( 1.0f, fabsf( turn.s ) ) ) );
	}
	nbFigureStats after = nbWorld_GetFigureStats( scene.world );
	result.balanceSteps = after.stepCount - before.stepCount;
	result.knockdowns = after.knockdownCount - before.knockdownCount;
	result.end = nbFigure_GetState( figure );
	if ( hash != NULL )
	{
		*hash = 2166136261u;
		b3WorldTransform transforms[nb_humanoidBoneCount];
		int count = nbFigure_GetBoneTransforms( figure, transforms, nb_humanoidBoneCount );
		for ( int i = 0; i < count; ++i )
		{
			const uint8_t* bytes = (const uint8_t*)( transforms + i );
			for ( size_t b = 0; b < sizeof( b3WorldTransform ); ++b )
			{
				*hash = ( *hash ^ bytes[b] ) * 16777619u;
			}
		}
	}
	DestroyFigureScene( &scene );
	return result;
}

// A body that pushes a standing figure slowly, harder than nbFigureDef::pushForce, makes it catch itself instead of
// meeting a wall: it leans against a moderate push, steps away from a hard one and a push on the forehead turns its head.
// It stays dynamic while the push lasts and stands animated again after it. A push below the force, or with the force
// zero, meets a wall as before. A stick pressed into the left elbow from the side pushes the round ends of upper arm and
// forearm up and down, and the arm gives way to it all the same: a stick swept slowly across the figure in the demo
// rested on its arm before. The same with one and four threads.
static int FigurePushTest( void )
{
	const char* names[7] = { "20 N on the chest",
							 "80 N on the chest",
							 "300 N on the chest",
							 "150 N on the forehead",
							 "300 N on the chest without pushForce",
							 "80 N with a stick into the elbow",
							 "80 N with a stick into the elbow without pushForce" };
	float heights[7] = { 1.3f, 1.3f, 1.3f, 1.65f, 1.3f, 1.06f, 1.06f };
	float forces[7] = { 20.0f, 80.0f, 300.0f, 150.0f, 300.0f, 80.0f, 80.0f };
	float pushForces[7] = { 40.0f, 40.0f, 40.0f, 40.0f, 0.0f, 40.0f, 0.0f };
	bool sticks[7] = { false, false, false, false, false, true, true };
	PushResult results[7];
	for ( int k = 0; k < 7; ++k )
	{
		PushResult r = Push( heights[k], forces[k], pushForces[k], sticks[k], 1, NULL );
		results[k] = r;
		printf( "  %s: catches itself after %d steps, %d changes of state, %d balance steps, %d knockdowns, the pelvis moves "
				"%.2f m, the head turns %.0f deg, stands again %d steps after the push\n",
				names[k], r.wakeStep, r.changes, r.balanceSteps, r.knockdowns, r.pelvisMove, r.headTurn * 57.29578f,
				r.standStep );
	}

	// Below the force and without it the figure stays a wall
	ENSURE( results[0].wakeStep < 0 && results[0].pelvisMove < 0.02f );
	ENSURE( results[4].wakeStep < 0 && results[4].pelvisMove < 0.02f );
	ENSURE( results[6].wakeStep < 0 && results[6].pelvisMove < 0.02f );

	// A moderate push: it leans and holds without a step, dynamic for the whole push
	ENSURE( results[1].wakeStep > 0 && results[1].wakeStep < 90 && results[1].changes == 2 && results[1].balanceSteps == 0 );
	ENSURE( results[1].pelvisMove > 0.03f && results[1].pelvisMove < 0.2f );

	// A hard push moves it away with steps, and it does not fall
	ENSURE( results[2].wakeStep > 0 && results[2].balanceSteps >= 1 && results[2].knockdowns == 0 && results[2].pelvisMove > 0.3f );

	// On the forehead the head gives way
	ENSURE( results[3].wakeStep > 0 && results[3].headTurn > 0.35f && results[3].knockdowns == 0 );

	// The arm gives way to the stick in the elbow, and the figure leans against it as against the push on the chest
	ENSURE( results[5].wakeStep > 0 && results[5].wakeStep < 90 && results[5].changes == 2 && results[5].knockdowns == 0 );
	ENSURE( results[5].pelvisMove > 0.03f && results[5].pelvisMove < 0.3f );

	// It stands animated again after the push
	for ( int k = 1; k < 6; ++k )
	{
		if ( k == 4 )
		{
			continue;
		}
		ENSURE( results[k].end == nb_figureAnimated && results[k].standStep >= 0 && results[k].standStep < 240 );
	}

	uint32_t one = 0;
	uint32_t four = 0;
	uint32_t stickOne = 0;
	uint32_t stickFour = 0;
	Push( 1.3f, 300.0f, 40.0f, false, 1, &one );
	Push( 1.3f, 300.0f, 40.0f, false, 4, &four );
	Push( 1.06f, 80.0f, 40.0f, true, 1, &stickOne );
	Push( 1.06f, 80.0f, 40.0f, true, 4, &stickFour );
	printf( "  push hash: 0x%08x with one thread, 0x%08x with four, with the stick 0x%08x and 0x%08x\n", one, four, stickOne,
			stickFour );
	ENSURE( one == four && stickOne == stickFour );
	return 0;
}

// A stick of 150 N under the left forearm that sweeps up on an arc about the shoulder for 2.5 s, holds 2 s, sweeps down
// and slides out to the side along its length, see NB_HELD_ARMS. The push wakes the figure, the arm goes slack and the
// stick lifts it: the upper arm rises by more than a radian, the figure keeps its feet without a fall and stands animated
// again within six seconds of the stick leaving it. Before round 36 the arm hung at the strength of its motors and the
// stick swept past it, the upper arm rose by 13 degrees. Until round 40 the stick went away backwards. The slack forearm
// lay over it and was dragged along, and it slipped off only because the figure swayed: a version of round 40 whose legs
// stood stiller kept the arm on the stick for good. The same with one and four threads.
typedef struct LiftResult
{
	float arm;
	float pelvisMove;
	int steps;
	int knockdowns;
	int standStep;
	nbFigureState end;
} LiftResult;

static LiftResult LiftArm( int threadCount, uint32_t* hash )
{
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
	StepFigures( &scene, 60 );

	// The stick across, on the underside of the forearm, which faces back while the arm hangs: on an arc about the shoulder
	// half a meter out, 9 cm under the line of the arm, and out of the way of the hip
	b3Vec3 shoulder = BonePosition( figure, nb_humanoidUpperArmLeft );
	const float radius = 0.5f;
	const float offset = 0.09f;
	float angles[2] = { -0.5f * B3_PI, 0.0f };
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_kinematicBody;
	bodyDef.position = b3ToPos( (b3Vec3){ shoulder.x + 0.125f, shoulder.y - radius, shoulder.z - offset } );
	b3BodyId hand = b3CreateBody( scene.physicsWorld, &bodyDef );
	bodyDef.type = b3_dynamicBody;
	bodyDef.gravityScale = 0.0f;
	bodyDef.enableSleep = false;
	b3BodyId stick = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3Capsule capsule = { { -0.1f, 0.0f, 0.0f }, { 0.1f, 0.0f, 0.0f }, 0.025f };
	shapeDef.density = 1.5f / ( B3_PI * 0.025f * 0.025f * 0.2f );
	b3CreateCapsuleShape( stick, &shapeDef, &capsule );
	b3MotorJointDef motor = b3DefaultMotorJointDef();
	motor.base.bodyIdA = hand;
	motor.base.bodyIdB = stick;
	motor.linearHertz = 5.0f;
	motor.linearDampingRatio = 1.0f;
	motor.maxSpringForce = 150.0f;
	motor.angularHertz = 5.0f;
	motor.angularDampingRatio = 1.0f;
	motor.maxSpringTorque = 30.0f;
	b3CreateMotorJoint( scene.physicsWorld, &motor );

	LiftResult result = { 0.0f, 0.0f, 0, 0, -1, nb_figureAnimated };
	b3Vec3 pelvis0 = BonePosition( figure, nb_humanoidPelvis );
	nbFigureStats before = nbWorld_GetFigureStats( scene.world );
	const int sweep = 150;
	const int hold = 120;
	const int away = 60;
	for ( int i = 0; i < 2 * sweep + hold + away + 360; ++i )
	{
		// Where the stick goes in the next step: up the arc, held, down the arc, then out to the side along its length,
		// which leaves the arm whichever side of the stick the forearm lies on
		int k = i + 1;
		float angle = angles[0];
		float out = 0.0f;
		if ( k <= sweep )
		{
			angle = angles[0] + ( angles[1] - angles[0] ) * (float)k / (float)sweep;
		}
		else if ( k <= sweep + hold )
		{
			angle = angles[1];
		}
		else if ( k <= 2 * sweep + hold )
		{
			angle = angles[1] + ( angles[0] - angles[1] ) * (float)( k - sweep - hold ) / (float)sweep;
		}
		else
		{
			out = 0.3f * b3MinFloat( (float)( k - 2 * sweep - hold ) / (float)away, 1.0f );
		}
		b3Vec3 next = { shoulder.x + 0.125f + out, shoulder.y + radius * sinf( angle ) - offset * cosf( angle ),
						shoulder.z + radius * cosf( angle ) + offset * sinf( angle ) };
		b3Body_SetLinearVelocity( hand, b3MulSV( 60.0f, b3Sub( next, b3ToVec3( b3Body_GetPosition( hand ) ) ) ) );
		StepFigures( &scene, 1 );

		b3Vec3 along = b3Normalize(
			b3Sub( BonePosition( figure, nb_humanoidForearmLeft ), BonePosition( figure, nb_humanoidUpperArmLeft ) ) );
		result.arm = b3MaxFloat( result.arm, acosf( b3ClampFloat( -along.y, -1.0f, 1.0f ) ) );
		result.pelvisMove =
			b3MaxFloat( result.pelvisMove, b3Length( b3Sub( BonePosition( figure, nb_humanoidPelvis ), pelvis0 ) ) );
		if ( i >= 2 * sweep + hold + away && result.standStep < 0 && nbFigure_GetState( figure ) == nb_figureAnimated )
		{
			result.standStep = i - ( 2 * sweep + hold + away );
		}
	}
	nbFigureStats after = nbWorld_GetFigureStats( scene.world );
	result.steps = after.stepCount - before.stepCount;
	result.knockdowns = after.knockdownCount - before.knockdownCount;
	result.end = nbFigure_GetState( figure );
	if ( hash != NULL )
	{
		*hash = 2166136261u;
		b3WorldTransform transforms[nb_humanoidBoneCount];
		int count = nbFigure_GetBoneTransforms( figure, transforms, nb_humanoidBoneCount );
		for ( int i = 0; i < count; ++i )
		{
			const uint8_t* bytes = (const uint8_t*)( transforms + i );
			for ( size_t b = 0; b < sizeof( b3WorldTransform ); ++b )
			{
				*hash = ( *hash ^ bytes[b] ) * 16777619u;
			}
		}
	}
	DestroyFigureScene( &scene );
	return result;
}

static int FigureHoldArmTest( void )
{
	uint32_t one = 0;
	uint32_t four = 0;
	LiftResult r = LiftArm( 1, &one );
	LiftResult s = LiftArm( 4, &four );
	printf( "  the stick lifts the upper arm by %.0f degrees, the pelvis moves %.2f m, %d balance steps, %d knockdowns, stands "
			"again %d steps after the stick left, hash 0x%08x with one thread, 0x%08x with four\n",
			r.arm * 57.29578f, r.pelvisMove, r.steps, r.knockdowns, r.standStep, one, four );
	ENSURE( r.arm > 1.0f );
	ENSURE( r.knockdowns == 0 && r.steps <= 1 && r.pelvisMove < 0.2f );
	ENSURE( r.end == nb_figureAnimated && r.standStep >= 0 && r.standStep < 360 );
	ENSURE( one == four && s.arm == r.arm );
	return 0;
}

// The heading of the line from the left to the right upper arm about the vertical, in radians
static float ShoulderHeading( nbFigureId figure )
{
	b3Vec3 line =
		b3Sub( BonePosition( figure, nb_humanoidUpperArmRight ), BonePosition( figure, nb_humanoidUpperArmLeft ) );
	return atan2f( line.x, line.z );
}

// A hit in front of the right shoulder turns the trunk about its long axis, one on the middle of the chest does not, see
// nbComputeTwist and nbTwistTrunk. The upper body of the humanoid turns about the axis up its spine 2.5 times as far as
// it tilts, the square root of the ratio of its rotational inertia about the two, and as much faster back: 30 N s at
// the shoulder turn the line between the shoulders by 12 degrees, 5 before, and it straightens again within a second. A
// hit of 60 N s there turns it by 43 degrees in a tenth of a second, 3 before, and the figure catches itself. Round 52
// turned it there by 30 degrees, and round 53, whose upper body turns back against the pelvis while a foot swings, see
// nbPelvisTurn, by 32.
static int FigureTwistTest( void )
{
	const float impulses[3] = { 30.0f, 30.0f, 60.0f };
	for ( int pass = 0; pass < 3; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 30 );
		b3BodyId chest = nbFigure_GetBody( figure, nb_humanoidChest );
		b3Vec3 point = b3ToVec3( b3Body_GetWorldPoint( chest, (b3Vec3){ 0.0f, 0.08f, 0.1f } ) );
		if ( pass > 0 )
		{
			point.x += 0.8f * ( BonePosition( figure, nb_humanoidUpperArmRight ).x - point.x );
		}
		float before = ShoulderHeading( figure );
		nbHitDef hit = { 1, nb_humanoidChest, (b3Vec3){ 0.0f, 0.0f, -impulses[pass] }, b3ToPos( point ) };
		ENSURE( nbFigure_ApplyHit( figure, &hit ) );
		float most = 0.0f;
		for ( int i = 0; i < 6; ++i )
		{
			StepFigures( &scene, 1 );
			float turn = ShoulderHeading( figure ) - before;
			turn = turn > B3_PI ? turn - 2.0f * B3_PI : ( turn < -B3_PI ? turn + 2.0f * B3_PI : turn );
			most = fabsf( turn ) > fabsf( most ) ? turn : most;
		}
		int steps = StepUntil( &scene, figure, nb_figureAnimated, 600 );
		StepFigures( &scene, 60 );
		float after = ShoulderHeading( figure ) - before;
		printf( "  %.0f N s %s: the line between the shoulders turns %.1f degrees in a tenth of a second, standing %.2f s "
				"later\n",
				impulses[pass], pass == 0 ? "on the middle of the chest" : "in front of the right shoulder",
				most * 180.0f / B3_PI, (float)( steps + 6 ) / 60.0f );
		ENSURE( steps >= 0 );
		ENSURE( StandsUpright( &scene, figure ) );
		if ( pass == 0 )
		{
			ENSURE( fabsf( most ) < 0.01f );
		}
		else if ( pass == 1 )
		{
			ENSURE( 0.15f < fabsf( most ) && fabsf( most ) < 0.3f );
			ENSURE( fabsf( after ) < 0.01f );
			ENSURE( nbFigure_GetInfo( figure ).catchCount == 0 );
		}
		else
		{
			ENSURE( fabsf( most ) > 0.5f );
			ENSURE( nbFigure_GetInfo( figure ).catchCount == 1 );
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
		// itself, but no step catches 200 N s and it falls with one step at most.
		float direction = pass == 0 ? -1.0f : 1.0f;
		b3Vec3 impulse = { 0.0f, 0.0f, 200.0f * direction };
		ENSURE( HitChest( figure, 1, impulse ) );
		ENSURE( nbFigure_GetState( figure ) == nb_figureBalancing );
		b3Vec3 momentum = FigureMomentum( figure );
		ENSURE_SMALL( momentum.z - impulse.z, 1.0f );
		ENSURE_SMALL( momentum.y, 1.0f );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( b3Body_GetType( nbFigure_GetBody( figure, i ) ) == b3_dynamicBody );
		}
		int catching = StepUntil( &scene, figure, nb_figureRagdoll, 30 );
		ENSURE( 0 < catching && catching <= 20 );
		ENSURE( nbFigure_GetInfo( figure ).stepCount <= 1 );

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
// from behind a push of 20 N s does not make it step at all. Standing a little in the knees, its pelvis 4 cm ahead of its
// ankles as in the reference picture, its center of mass lies 5 cm ahead of the ankles, 10 cm ahead of the heels and 18
// behind the toes, so from behind one of 23 N s makes it step, from the front one of 15 and from a side one of 26. Before
// round 36 it lay 3 cm ahead of the ankles, and from behind one of 30 N s just made it step.
static int FigureStepTest( void )
{
	// The impulse, the direction it pushes in, along x or z, and how far the body goes there
	const float impulses[4] = { 35.0f, 30.0f, 20.0f, 20.0f };
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
			// The idle of the library with the left knee up, the ankle 21 cm over the ground. On straight legs the root
			// of a clip would stand 2 cm over the stand height, which is a little in the knees.
			static nbKeyframe keys[NB_HUMANOID_CLIP_KEYFRAMES];
			static b3Quat poses[NB_HUMANOID_CLIP_KEYFRAMES * nb_humanoidBoneCount];
			nbClipDef clip;
			int count = nbMakeHumanoidClip( nb_clipIdle, 1.8f, keys, poses, NB_HUMANOID_CLIP_KEYFRAMES, &clip );
			ENSURE( count > 0 );
			for ( int k = 0; k < count; ++k )
			{
				poses[k * nb_humanoidBoneCount + nb_humanoidThighLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.8f );
				poses[k * nb_humanoidBoneCount + nb_humanoidShinLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -1.2f );
			}
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
			// Both soles flat on the slope, and across it the root down as far as the lower foot goes down. Along it the
			// pelvis stands as high over the ankles as on flat ground, where they stand 6 cm high: the feet stand 4 cm
			// behind it, see NB_STANCE_BACK, 8 mm lower than the slope under the pelvis.
			ENSURE( lowLeft > -3.0e-3f && highLeft < 3.0e-3f && lowRight > -3.0e-3f && highRight < 3.0e-3f );
			ENSURE( pass == 2 || pelvis.y < ground.y + scene.standHeight - 0.015f );
			ENSURE( pass == 1 || fabsf( pelvis.y - 0.5f * ( left.y + right.y ) - ( scene.standHeight - 0.06f ) ) < 3.0e-3f );
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
// see the roadmap of the game 26.15. Its arms reach ahead into tight tunnels of four fixed blocks, as the arm of figure 8
// of the benchmark caught in a crack of its wall: it hangs on the arms with its feet on the ground and its pelvis too high
// over the ground to get up from it. It wakes up where its feet stand, straight to the stand clip without a get-up clip,
// without a jump of a bone, and stands. Before round 36 the right arm alone held it with its pelvis 61 to 65 cm high. The
// arm of 81 cm with the armature of the shoulder, see nbMakeHumanoidBones, lets it sink to 54 cm, below the wake height:
// it gets up from there with its clip and stands after 5.8 s. On both arms it hangs 75 cm high. A figure that lies is no
// figure that stands: knocked down it gets up with its clip, see FigureKnockdownTest.
static int FigureWakeStandingTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );

	// The stand clip with the arms ahead, so the figure starts as a ragdoll in that pose
	b3Quat rotations[nb_humanoidBoneCount];
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		rotations[i] = b3Quat_identity;
	}
	rotations[nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.5f );
	rotations[nb_humanoidUpperArmRight] = rotations[nb_humanoidUpperArmLeft];
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

	// A tunnel along each forearm: a block over it, under it and at each side, each 5 mm into it, the blocks between the
	// arms short enough to leave the tunnels apart
	const int forearms[2] = { nb_humanoidForearmLeft, nb_humanoidForearmRight };
	for ( int side = 0; side < 2; ++side )
	{
		b3AABB arm = b3Body_ComputeAABB( nbFigure_GetBody( figure, forearms[side] ) );
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
		b3Vec3 halves[4] = { { 0.1f, thick, half }, { 0.1f, thick, half }, { thick, 0.3f, half }, { thick, 0.3f, half } };
		for ( int k = 0; k < 4; ++k )
		{
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.position = b3ToPos( positions[k] );
			b3BodyId block = b3CreateBody( scene.physicsWorld, &bodyDef );
			b3BoxHull box = b3MakeBoxHull( halves[k].x, halves[k].y, halves[k].z );
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			b3CreateHullShape( block, &shapeDef, &box.base );
		}
	}

	// It sags onto the arms and comes to rest with its pelvis high, then wakes up standing
	int steps = 0;
	float lowest = FLT_MAX;
	while ( nbFigure_GetState( figure ) == nb_figureRagdoll && steps < 600 )
	{
		StepFigures( &scene, 1 );
		steps += 1;
		lowest = b3MinFloat( lowest, BonePosition( figure, nb_humanoidPelvis ).y );
	}
	nbFigureInfo info = nbFigure_GetInfo( figure );
	printf( "  hung on its arms, the pelvis at least %.2f m high, woke up standing after %.2f s\n", lowest,
			(float)steps / 60.0f );
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

	StepFigures( &scene, 2 );
	ENSURE( nbFigure_GetState( blasted ) == nb_figureRagdoll );
	ENSURE( nbFigure_GetInfo( blasted ).knockdownCount == 1 );

	// The blasted figure lands beside the block of 80 kg. Since round 34 it starts to get up a little earlier there,
	// the block presses it, and it falls again, see NB_ABORT_PART. Since round 53 the figure under the block starts to
	// get up after 1.4 s, while the block still slides off it 12 cm over the ground, round 52 after 1.8 s, the block 6 cm
	// over it. The block breaks that off, comes to rest on its legs and a hand, and the figure waits under it, as the
	// roadmap of the game asks in 26.15: each fall after the first is a getting up broken off.
	StepFigures( &scene, 88 );
	printf( "  blasted: %d knockdowns, heavy %d (state %d), pressed %d (state %d)\n", nbFigure_GetInfo( blasted ).knockdownCount,
			nbFigure_GetInfo( heavy ).knockdownCount, (int)nbFigure_GetState( heavy ), nbFigure_GetInfo( pressed ).knockdownCount,
			(int)nbFigure_GetState( pressed ) );
	ENSURE( nbFigure_GetInfo( blasted ).knockdownCount >= 1 );
	ENSURE( nbFigure_GetState( light ) == nb_figureAnimated );
	ENSURE( nbFigure_GetInfo( light ).hitCount >= 1 );
	ENSURE( nbFigure_GetInfo( light ).knockdownCount == 0 );
	ENSURE( nbFigure_GetInfo( heavy ).knockdownCount == 1 );
	ENSURE( nbFigure_GetInfo( pressed ).knockdownCount == 1 + nbFigure_GetInfo( pressed ).abortCount );
	ENSURE( nbFigure_GetInfo( pressed ).abortCount <= 1 );
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

// A fighter, a figure that follows its clip with dynamic bones near another figure, see nbFigureDef::engageRadius
static nbFigureId AddFighter( FigureScene* scene, b3Vec3 position, float heading, float engageRadius )
{
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene->rig;
	def.standHeight = scene->standHeight;
	def.position = b3ToPos( position );
	def.heading = heading;
	def.engageRadius = engageRadius;
	return nbCreateFigure( scene->world, &def );
}

// A punch of the right arm on the idle clip of the library: the upper arm swings ahead in 0.12 s, stays there 0.2 s and
// comes back in 0.4 s
typedef struct PunchClip
{
	nbKeyframe idleKeys[NB_HUMANOID_CLIP_KEYFRAMES];
	b3Quat idlePoses[NB_HUMANOID_CLIP_KEYFRAMES * nb_humanoidBoneCount];
	nbKeyframe keys[4];
	b3Quat poses[4 * nb_humanoidBoneCount];
	nbClipDef clip;
} PunchClip;

static void MakePunchClip( PunchClip* punch, float swing )
{
	nbClipDef idle;
	nbMakeHumanoidClip( nb_clipIdle, 1.8f, punch->idleKeys, punch->idlePoses, NB_HUMANOID_CLIP_KEYFRAMES, &idle );
	float times[4] = { 0.0f, 0.12f, 0.32f, 0.72f };
	for ( int k = 0; k < 4; ++k )
	{
		b3Quat* pose = punch->poses + k * nb_humanoidBoneCount;
		memcpy( pose, punch->idlePoses, sizeof( b3Quat ) * nb_humanoidBoneCount );
		if ( k == 1 || k == 2 )
		{
			pose[nb_humanoidUpperArmRight] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, swing );
			pose[nb_humanoidForearmRight] = b3Quat_identity;
		}
		punch->keys[k] = (nbKeyframe){ times[k], pose, b3Transform_identity };
	}
	punch->clip = (nbClipDef){ punch->keys, 4, nb_humanoidBoneCount, false, false };
}

// Step once and return how far the bones of a figure ended from where its clip put them for the end of the step, the
// farthest
static float StepFromClip( FigureScene* scene, nbFigureId figure )
{
	b3WorldTransform targets[nb_humanoidBoneCount];
	nbFigure_GetTargetTransforms( figure, targets, nb_humanoidBoneCount );
	StepFigures( scene, 1 );
	float farthest = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		farthest =
			b3MaxFloat( farthest, b3Length( b3SubPos( b3Body_GetPosition( nbFigure_GetBody( figure, i ) ), targets[i].p ) ) );
	}
	return farthest;
}

// Fighters near each other follow their clips with dynamic bones, within a few millimeters, a figure alone stays
// kinematic, and a fighter whose opponent left is kinematic again a second later
static int FigureFollowTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId a = AddFighter( &scene, b3Vec3_zero, 0.0f, 2.0f );
	nbFigureId b = AddFighter( &scene, (b3Vec3){ 0.0f, 0.0f, 1.0f }, B3_PI, 2.0f );
	nbFigureId alone = AddFighter( &scene, (b3Vec3){ 10.0f, 0.0f, 0.0f }, 0.0f, 2.0f );
	StepFigures( &scene, 2 );
	ENSURE( nbFigure_GetInfo( a ).following && nbFigure_GetInfo( b ).following );
	ENSURE( nbFigure_GetInfo( alone ).following == false );
	ENSURE( nbFigure_GetState( a ) == nb_figureAnimated );
	ENSURE( b3Body_GetType( nbFigure_GetBody( a, nb_humanoidHandRight ) ) == b3_dynamicBody );
	ENSURE( b3Body_GetType( nbFigure_GetBody( alone, nb_humanoidHandRight ) ) == b3_kinematicBody );
	nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
	ENSURE( stats.followingCount == 2 && stats.animatedCount == 3 );

	StepFigures( &scene, 60 );
	float worst = 0.0f;
	for ( int step = 0; step < 120; ++step )
	{
		worst = b3MaxFloat( worst, StepFromClip( &scene, a ) );
	}
	printf( "  the bones of a fighter in the idle clip at most %.1f mm off it\n", 1000.0f * worst );
	ENSURE( worst < 4.0e-3f );

	// The opponent jumps away and is kinematic at once, the fighter left behind a second later
	nbFigure_Move( b, b3ToPos( (b3Vec3){ 20.0f, 0.0f, 20.0f } ), 0.0f );
	ENSURE( nbFigure_GetInfo( b ).following == false );
	ENSURE( b3Body_GetType( nbFigure_GetBody( b, nb_humanoidHandRight ) ) == b3_kinematicBody );
	int steps = 0;
	while ( nbFigure_GetInfo( a ).following && steps < 180 )
	{
		StepFigures( &scene, 1 );
		steps += 1;
	}
	printf( "  kinematic again %d steps after the opponent left\n", steps );
	ENSURE( steps >= 59 && steps < 90 );
	ENSURE( b3Body_GetType( nbFigure_GetBody( a, nb_humanoidHandRight ) ) == b3_kinematicBody );
	ENSURE( nbFigure_GetState( a ) == nb_figureAnimated );
	StepFigures( &scene, 30 );
	ENSURE( StandsUpright( &scene, a ) );
	ENSURE( nbWorld_GetFigureStats( scene.world ).followingCount == 0 );
	DestroyFigureScene( &scene );
	return 0;
}

// A punch passes through a kinematic figure and meets the body of a fighter. The fist follows the clip within a few
// centimeters until it lands, with the force the clip takes behind it: it pushes the fighter that it meets back, and lands
// as a hit on both with the impulse of its contact, so the one that it meets flinches and the one that punches feels it.
// Neither falls. The impulse of this punch lies close to where a hit makes a figure step: with the arms of 81 cm until
// round 43 the one it met stepped back twice, 8 cm, with the arms of 78 cm since round 44 it flinches and gives 1.3 cm,
// as the arms of 81 cm did 1 cm farther away.
static int FigurePunchTest( void )
{
	static PunchClip punch;
	MakePunchClip( &punch, 1.6f );
	float reach[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		float radius = pass == 0 ? 0.0f : 2.0f;
		nbFigureId a = AddFighter( &scene, b3Vec3_zero, 0.0f, radius );
		nbFigureId b = AddFighter( &scene, (b3Vec3){ -0.2f, 0.0f, 0.62f }, B3_PI, radius );
		StepFigures( &scene, 60 );
		b3Vec3 start = BonePosition( b, nb_humanoidPelvis );
		nbFigure_PlayClip( a, &punch.clip, 1.0f );
		reach[pass] = -1.0f;
		float swingError = 0.0f;
		float flinch = 0.0f;
		float pushed = 0.0f;
		for ( int step = 0; step < 60; ++step )
		{
			bool landed = nbFigure_GetInfo( b ).hitCount > 0;
			float fromClip = StepFromClip( &scene, a );
			swingError = landed || nbFigure_GetInfo( b ).hitCount > 0 ? swingError : b3MaxFloat( swingError, fromClip );
			reach[pass] = b3MaxFloat( reach[pass], BonePosition( a, nb_humanoidHandRight ).z );
			flinch = b3MaxFloat( flinch, b3Length( nbFigure_GetInfo( b ).flinch ) );
			pushed = b3MaxFloat( pushed, BonePosition( b, nb_humanoidPelvis ).z - start.z );
		}
		nbFigureInfo ia = nbFigure_GetInfo( a );
		nbFigureInfo ib = nbFigure_GetInfo( b );
		printf( "  pass %d: the fist reaches %.3f m, until it lands at most %.1f cm off the clip, hits %d and %d, the other "
				"pushed back %.1f cm, flinch %.3f rad\n",
				pass, reach[pass], 100.0f * swingError, ia.hitCount, ib.hitCount, 100.0f * pushed, flinch );
		if ( pass == 0 )
		{
			ENSURE( ia.hitCount == 0 && ib.hitCount == 0 );
			ENSURE( pushed < 1.0e-3f );
		}
		else
		{
			ENSURE( ia.hitCount >= 1 && ib.hitCount >= 1 );
			ENSURE( flinch > 0.1f );
			ENSURE( swingError < 0.06f );
			ENSURE( pushed > 0.003f && pushed < 0.2f );
		}
		ENSURE( ia.knockdownCount == 0 && ib.knockdownCount == 0 );

		StepFigures( &scene, 180 );
		ENSURE( nbFigure_GetState( a ) == nb_figureAnimated && nbFigure_GetState( b ) == nb_figureAnimated );
		ENSURE( nbFigure_GetInfo( a ).following == ( pass == 1 ) && nbFigure_GetInfo( b ).following == ( pass == 1 ) );
		ENSURE( StandsUpright( &scene, a ) && StandsUpright( &scene, b ) );
		DestroyFigureScene( &scene );
	}
	ENSURE( reach[1] < reach[0] - 0.04f );
	return 0;
}

// A hit of the program on a fighter: as on a kinematic figure, a weaker one makes it step and a hard one makes it catch
// itself, and then it follows its clip again
static int FigureFollowHitTest( void )
{
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId a = AddFighter( &scene, b3Vec3_zero, 0.0f, 2.0f );
		nbFigureId b = AddFighter( &scene, (b3Vec3){ 0.0f, 0.0f, 1.0f }, B3_PI, 2.0f );
		StepFigures( &scene, 30 );
		ENSURE( nbFigure_GetInfo( b ).following );
		float impulse = pass == 0 ? 30.0f : 60.0f;
		ENSURE( HitChest( b, 1, (b3Vec3){ 0.0f, 0.0f, impulse } ) );
		bool caught = false;
		for ( int step = 0; step < 240; ++step )
		{
			StepFigures( &scene, 1 );
			caught = caught || nbFigure_GetState( b ) == nb_figureBalancing;
		}
		nbFigureInfo info = nbFigure_GetInfo( b );
		printf( "  %.0f N s: %d steps, caught itself %d, knocked down %d, following %d\n", impulse, info.stepCount,
				info.catchCount, info.knockdownCount, info.following );
		ENSURE( info.knockdownCount == 0 );
		ENSURE( info.stepCount >= 1 );
		ENSURE( caught == ( pass == 1 ) && info.catchCount == pass );
		ENSURE( nbFigure_GetState( b ) == nb_figureAnimated && info.following );
		ENSURE( nbFigure_GetInfo( a ).following );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The program walks a fighter into another one: the other takes the bump as a hit and steps back, the one that walks
// lets go of its clip where the other is in its way and catches itself, and neither falls
static int FigureFollowBumpTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId a = AddFighter( &scene, b3Vec3_zero, 0.0f, 2.0f );
	nbFigureId b = AddFighter( &scene, (b3Vec3){ 0.0f, 0.0f, 1.2f }, B3_PI, 2.0f );
	StepFigures( &scene, 30 );
	for ( int step = 0; step < 90; ++step )
	{
		if ( nbFigure_GetState( a ) == nb_figureAnimated )
		{
			b3Vec3 p = b3ToVec3( nbFigure_GetFrame( a ).p );
			p.z += 1.0f / 60.0f;
			nbFigure_Move( a, b3ToPos( p ), 0.0f );
		}
		StepFigures( &scene, 1 );
	}
	StepFigures( &scene, 180 );
	nbFigureInfo ia = nbFigure_GetInfo( a );
	nbFigureInfo ib = nbFigure_GetInfo( b );
	printf( "  the one that walks let go %d times and caught itself %d times, the other was hit %d times, let go %d times "
			"and caught itself %d times, states %d and %d, knocked down %d and %d\n",
			ia.letGoCount, ia.catchCount, ib.hitCount, ib.letGoCount, ib.catchCount, (int)nbFigure_GetState( a ),
			(int)nbFigure_GetState( b ), ia.knockdownCount, ib.knockdownCount );
	ENSURE( ia.letGoCount >= 1 && ia.catchCount >= 1 );
	ENSURE( ib.hitCount >= 1 && ib.stepCount >= 1 );
	ENSURE( ia.knockdownCount == 0 && ib.knockdownCount == 0 );
	ENSURE( nbFigure_GetState( a ) == nb_figureAnimated && nbFigure_GetState( b ) == nb_figureAnimated );
	ENSURE( ia.following && ib.following );
	ENSURE( nbWorld_GetFigureStats( scene.world ).letGoCount == ia.letGoCount + ib.letGoCount );
	DestroyFigureScene( &scene );
	return 0;
}

// Two fighters: a punch that lands, a hard hit of the program, a bump, the opponent leaving: the same bits with 1 and 4
// threads
static uint32_t RunFollowScenario( int threadCount )
{
	static PunchClip punch;
	MakePunchClip( &punch, 1.6f );
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	nbFigureId a = AddFighter( &scene, b3Vec3_zero, 0.0f, 2.0f );
	nbFigureId b = AddFighter( &scene, (b3Vec3){ -0.2f, 0.0f, 0.65f }, B3_PI, 2.0f );
	StepFigures( &scene, 30 );
	nbFigure_PlayClip( a, &punch.clip, 1.0f );
	StepFigures( &scene, 60 );
	HitChest( b, 1, (b3Vec3){ 0.0f, 0.0f, 60.0f } );
	StepFigures( &scene, 120 );
	for ( int step = 0; step < 60; ++step )
	{
		if ( nbFigure_GetState( a ) == nb_figureAnimated )
		{
			b3Vec3 p = b3ToVec3( nbFigure_GetFrame( a ).p );
			p.z += 1.0f / 60.0f;
			nbFigure_Move( a, b3ToPos( p ), 0.0f );
		}
		StepFigures( &scene, 1 );
	}
	StepFigures( &scene, 60 );
	nbFigure_Move( b, b3ToPos( (b3Vec3){ 20.0f, 0.0f, 20.0f } ), 0.0f );
	StepFigures( &scene, 120 );

	uint32_t hash = 2166136261u;
	nbFigureId figures[2] = { a, b };
	for ( int k = 0; k < 2; ++k )
	{
		b3WorldTransform transforms[nb_humanoidBoneCount];
		int count = nbFigure_GetBoneTransforms( figures[k], transforms, nb_humanoidBoneCount );
		for ( int i = 0; i < count; ++i )
		{
			const uint8_t* bytes = (const uint8_t*)( transforms + i );
			for ( size_t n = 0; n < sizeof( b3WorldTransform ); ++n )
			{
				hash = ( hash ^ bytes[n] ) * 16777619u;
			}
		}
		nbFigureInfo info = nbFigure_GetInfo( figures[k] );
		hash = ( hash ^ (uint32_t)( info.hitCount + 16 * info.letGoCount + 256 * info.catchCount ) ) * 16777619u;
	}
	DestroyFigureScene( &scene );
	return hash;
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
// Figures a program steers, see figure_steer.c and the acceptance of round 84, F84 in docs/Roadmap.md

// A figure the program steers by the velocity it wants
static nbFigureId AddSteeredFigure( FigureScene* scene, b3Vec3 position, float heading )
{
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &scene->rig;
	def.standHeight = scene->standHeight;
	def.position = b3ToPos( position );
	def.heading = heading;
	def.steered = true;
	return nbCreateFigure( scene->world, &def );
}

// The figure as the library keeps it, for its capsule and its feet
static const nbFigure* GetSteered( const FigureScene* scene, nbFigureId figure )
{
	return nbGetWorldFromId( scene->world )->figures.data + ( figure.index1 - 1 );
}

static void SteerVelocity( nbFigureId figure, b3Vec3 velocity )
{
	nbSteer steer = { velocity, 0.0f, true };
	nbFigure_Steer( figure, &steer );
}

// The speed of the steering over the ground
static float SteerSpeed( nbFigureId figure )
{
	b3Vec3 velocity = nbFigure_GetInfo( figure ).moveVelocity;
	return sqrtf( velocity.x * velocity.x + velocity.z * velocity.z );
}

// A steered figure gets to the speed it wants, walking and running, within 10 % in the time its acceleration takes and a
// tenth of a second, holds it, stops again as fast as it brakes, and turns on the spot to the heading it wants at its
// turn rate
static int SteerSpeedTest( void )
{
	nbFigureSteering steering = nbDefaultFigureDef().steering;
	const float speeds[2] = { 1.5f, 4.5f };
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddSteeredFigure( &scene, b3Vec3_zero, 0.0f );
		StepFigures( &scene, 10 );
		float speed = speeds[pass];
		SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, speed } );
		StepFigures( &scene, (int)ceilf( 60.0f * ( speed / steering.acceleration + 0.1f ) ) );
		float reached = SteerSpeed( figure );
		StepFigures( &scene, 120 );
		float held = SteerSpeed( figure );
		SteerVelocity( figure, b3Vec3_zero );
		StepFigures( &scene, (int)ceilf( 60.0f * ( speed / steering.braking + 0.1f ) ) );
		float stopped = SteerSpeed( figure );
		printf( "  wished %.1f m/s: %.3f after accelerating, %.3f held, %.4f after braking\n", speed, reached, held, stopped );
		ENSURE( fabsf( reached - speed ) <= 0.1f * speed );
		ENSURE( fabsf( held - speed ) <= 0.1f * speed );
		ENSURE( stopped < 0.01f );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		DestroyFigureScene( &scene );
	}

	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddSteeredFigure( &scene, b3Vec3_zero, 0.0f );
	StepFigures( &scene, 10 );
	nbSteer steer = { b3Vec3_zero, 0.5f * B3_PI, false };
	nbFigure_Steer( figure, &steer );

	// The heading as the library keeps it: the frame turns by an approximate cosine and sine, see b3ComputeCosSin
	float last = GetSteered( &scene, figure )->heading;
	float fastest = 0.0f;
	int steps = (int)ceilf( 60.0f * ( 0.5f * B3_PI / steering.turnRate + 0.1f ) );
	for ( int i = 0; i < steps; ++i )
	{
		StepFigures( &scene, 1 );
		float heading = GetSteered( &scene, figure )->heading;
		fastest = b3MaxFloat( fastest, 60.0f * fabsf( heading - last ) );
		last = heading;
	}
	printf( "  turned on the spot to %.4f rad, at most %.3f rad/s\n", last, fastest );
	ENSURE( fabsf( last - 0.5f * B3_PI ) < 0.01f );
	ENSURE( fastest <= 1.01f * steering.turnRate );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	DestroyFigureScene( &scene );
	return 0;
}

typedef struct FixedCount
{
	int count;
} FixedCount;

static bool CountFixedShape( b3ShapeId shapeId, void* context )
{
	if ( b3Body_GetType( b3Shape_GetBody( shapeId ) ) == b3_staticBody )
	{
		( (FixedCount*)context )->count += 1;
	}
	return true;
}

// Whether the capsule of a steered figure, shrunk by 1 cm, overlaps a fixed body
static bool CapsuleOverlapsFixed( const FigureScene* scene, nbFigureId figure )
{
	const nbFigure* f = GetSteered( scene, figure );
	b3Vec3 points[2] = { { 0.0f, f->moverLow, 0.0f }, { 0.0f, f->moverHigh, 0.0f } };
	b3ShapeProxy proxy = { points, 2, f->moverRadius - 0.01f };
	FixedCount fixed = { 0 };
	b3World_OverlapShape( scene->physicsWorld, nbFigure_GetFrame( figure ).p, &proxy, b3DefaultQueryFilter(), CountFixedShape,
						  &fixed );
	return fixed.count > 0;
}

// A steered figure against a wall, head on and at 30 degrees: its capsule shrunk by 1 cm never overlaps the wall or the
// ground, it stops at the wall, and at 30 degrees it slides along it with what the wall leaves of the velocity it wants
static int SteerWallTest( void )
{
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		AddBlock( &scene, (b3Vec3){ 0.0f, 1.5f, 6.0f }, (b3Vec3){ 10.0f, 1.5f, 0.2f } );
		nbFigureId figure = AddSteeredFigure( &scene, b3Vec3_zero, 0.0f );
		b3CosSin turn = b3ComputeCosSin( pass == 0 ? 0.0f : B3_PI / 6.0f );
		SteerVelocity( figure, (b3Vec3){ 1.5f * turn.sine, 0.0f, 1.5f * turn.cosine } );
		int overlaps = 0;
		for ( int i = 0; i < 360; ++i )
		{
			StepFigures( &scene, 1 );
			overlaps += CapsuleOverlapsFixed( &scene, figure ) ? 1 : 0;
		}
		b3Vec3 position = b3ToVec3( nbFigure_GetFrame( figure ).p );
		b3Vec3 velocity = nbFigure_GetInfo( figure ).moveVelocity;
		float gap = 5.8f - position.z - GetSteered( &scene, figure )->moverRadius;
		printf( "  %s: %d overlaps, %.4f m from the wall, velocity %.3f %.3f\n", pass == 0 ? "head on" : "at 30 degrees",
				overlaps, gap, velocity.x, velocity.z );
		ENSURE( overlaps == 0 );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		ENSURE( gap > -0.01f && gap < 0.05f );
		ENSURE( fabsf( velocity.z ) < 0.01f );
		ENSURE( fabsf( velocity.x - 1.5f * turn.sine ) < 0.1f * 1.5f * turn.sine + 0.01f );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The courses of round 84 along +z: a ramp of 20 or of 60 degrees from z = 3 with a platform on top, stairs of ten steps
// of 15 cm from z = 3, or a ledge of 1 m that ends at z = 3, and of round 85 stairs of twenty steps of 15 cm from z = 3.
// The height on top.
static float AddSteerCourse( FigureScene* scene, int course )
{
	if ( course == 0 || course == 2 )
	{
		float angle = ( course == 0 ? 20.0f : 60.0f ) * B3_PI / 180.0f;
		b3CosSin slope = b3ComputeCosSin( angle );
		float length = 8.0f;
		b3Vec3 center = { 0.0f, 0.5f * length * slope.sine - 0.2f * slope.cosine,
						  3.0f + 0.5f * length * slope.cosine + 0.2f * slope.sine };
		AddTurnedBlock( scene, center, b3MakeQuatFromAxisAngle( b3Vec3_axisX, -angle ), (b3Vec3){ 3.0f, 0.2f, 0.5f * length } );
		float top = length * slope.sine;
		float end = 3.0f + length * slope.cosine;
		AddBlock( scene, (b3Vec3){ 0.0f, 0.5f * top, end + 9.95f }, (b3Vec3){ 3.0f, 0.5f * top, 10.0f } );
		return top;
	}
	if ( course == 1 || course == 4 )
	{
		int count = course == 1 ? 10 : 20;
		for ( int i = 0; i < count; ++i )
		{
			float height = 0.15f * (float)( i + 1 );
			AddBlock( scene, (b3Vec3){ 0.0f, 0.5f * height, 13.0f + 0.3f * (float)i }, (b3Vec3){ 2.0f, 0.5f * height, 10.0f } );
		}
		return 0.15f * (float)count;
	}
	AddBlock( scene, (b3Vec3){ 0.0f, 0.5f, -2.0f }, (b3Vec3){ 3.0f, 0.5f, 5.0f } );
	return 0.0f;
}

// A steered figure on its courses: up a ramp of 20 degrees and up stairs of 15 cm it keeps its ground in every update and
// comes out on top, a ramp of 60 degrees stops it at its foot, and off a ledge of 1 m it falls, lands on its feet and
// walks on, standing all the way
static int SteerGroundTest( void )
{
	const char* names[4] = { "ramp of 20 degrees", "stairs of 15 cm", "ramp of 60 degrees", "ledge of 1 m" };
	for ( int course = 0; course < 4; ++course )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		float top = AddSteerCourse( &scene, course );
		nbFigureId figure = AddSteeredFigure( &scene, course == 3 ? (b3Vec3){ 0.0f, 1.0f, -5.0f } : b3Vec3_zero, 0.0f );
		SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, 1.5f } );
		int lost = 0;
		int falls = 0;
		for ( int i = 0; i < 600; ++i )
		{
			StepFigures( &scene, 1 );
			nbFigureInfo info = nbFigure_GetInfo( figure );
			lost += info.onGround ? 0 : 1;
			falls += info.state == nb_figureAnimated ? 0 : 1;
		}
		b3Vec3 position = b3ToVec3( nbFigure_GetFrame( figure ).p );
		float speed = SteerSpeed( figure );
		printf( "  %s: %d updates without ground, %d not standing, ends %.3f m high at z %.2f, %.3f m/s\n", names[course], lost,
				falls, position.y, position.z, speed );
		ENSURE( falls == 0 );
		if ( course < 2 )
		{
			ENSURE( lost == 0 );
			ENSURE( fabsf( position.y - top ) < 0.01f && position.z > 12.0f );
			ENSURE( fabsf( speed - 1.5f ) < 0.15f );
		}
		else if ( course == 2 )
		{
			ENSURE( position.y < 0.05f && position.z < 3.5f && speed < 0.05f );
		}
		else
		{
			ENSURE( lost > 0 && fabsf( position.y ) < 0.01f && position.z > 6.0f );
			ENSURE( fabsf( speed - 1.5f ) < 0.15f );
		}
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The edge of the toes or of the heel on the sole of a foot of the humanoid, where the figure puts the foot: the heel
// stays where it is while the foot comes down from it, the toes while the foot stands flat and while it rolls over them
static b3Vec3 SoleEdge( const FigureScene* scene, const nbFigure* f, int foot, float along )
{
	const nbBoneDef* bone = scene->bones + foot;
	b3Vec3 local = { bone->center.x, bone->center.y + along * bone->halfExtents.y, bone->center.z - bone->halfExtents.z };
	return b3ToVec3( b3TransformWorldPoint( f->targets[foot], local ) );
}

// A planted foot of a steered figure moves less than 1 cm between landing and lifting, walking and running on flat
// ground. Walking it lands on its heel, its toes up by 0.15 to 0.35 rad, and comes down from it, so its heel stays where
// it landed; running it lands flat. Its heel rises as the hip passes over it, so from then on it is the edge of its toes
// that stays.
static int SteerFootTest( void )
{
	const float speeds[2] = { 1.5f, 4.5f };
	const int feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		nbFigureId figure = AddSteeredFigure( &scene, b3Vec3_zero, 0.0f );
		SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, speeds[pass] } );
		b3Vec3 heels[2] = { b3Vec3_zero, b3Vec3_zero };
		b3Vec3 toes[2] = { b3Vec3_zero, b3Vec3_zero };
		bool standing[2] = { false, false };
		bool flat[2] = { false, false };
		float moved = 0.0f;
		float rolled = 0.0f;
		float lowest = FLT_MAX;
		float highest = -FLT_MAX;
		int stances = 0;
		for ( int i = 0; i < 360; ++i )
		{
			StepFigures( &scene, 1 );
			const nbFigure* f = GetSteered( &scene, figure );
			for ( int k = 0; k < 2; ++k )
			{
				bool planted = f->gaitOn && f->footSwings[k] == false;
				b3Vec3 heel = SoleEdge( &scene, f, feet[k], -1.0f );
				b3Vec3 toe = SoleEdge( &scene, f, feet[k], 1.0f );
				if ( planted && standing[k] == false )
				{
					// It lands: how far it lands rolled, toes up below zero
					heels[k] = heel;
					flat[k] = false;
					stances += 1;
					if ( i > 60 )
					{
						lowest = b3MinFloat( lowest, f->footRoll[k] );
						highest = b3MaxFloat( highest, f->footRoll[k] );
					}
				}
				if ( planted && f->footRoll[k] < 0.0f )
				{
					moved = b3MaxFloat( moved, b3Distance( heel, heels[k] ) );
				}
				else if ( planted && flat[k] == false )
				{
					toes[k] = toe;
					flat[k] = true;
				}
				else if ( planted )
				{
					moved = b3MaxFloat( moved, b3Distance( toe, toes[k] ) );
					rolled = b3MaxFloat( rolled, f->footRoll[k] );
				}
				standing[k] = planted;
			}
		}
		printf( "  at %.1f m/s: %d stances, landing rolled %.3f to %.3f rad, a planted foot moved %.5f m at most and rolled "
				"%.2f rad\n",
				speeds[pass], stances, lowest, highest, moved, rolled );
		ENSURE( stances > 10 );
		ENSURE( moved < 0.01f );
		ENSURE( rolled > 0.1f );
		if ( pass == 0 )
		{
			ENSURE( lowest >= -0.35f && highest <= -0.15f );
		}
		else
		{
			ENSURE( lowest >= -0.05f && highest <= 0.05f );
		}
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A steered figure walks up and down its courses at 1.5 m/s, the conditions of rounds 85 and 86 in docs/Roadmap.md: on
// the ramp of 20 degrees its pelvis moves against its frame by 3 cm per update at most and by 10 cm at most between two
// landings, its steps are at most 0.85 as long as on flat ground and it lands on its heel, up and down. On twenty stairs
// of 15 by 30 cm its pelvis goes along the line of the stairs, up and down by 6 cm at most between two landings and by
// 3 cm at most in an update, where its frame goes up and down tread by tread.
static int SteerSlopeTest( void )
{
	const char* names[4] = { "up the ramp", "down the ramp", "up the stairs", "down the stairs" };
	for ( int pass = 0; pass < 4; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		bool stairs = pass >= 2;
		bool down = pass == 1 || pass == 3;
		float top = AddSteerCourse( &scene, stairs ? 4 : 0 );
		b3Vec3 start = down ? (b3Vec3){ 0.0f, top, stairs ? 11.0f : 14.0f } : b3Vec3_zero;
		nbFigureId figure = AddSteeredFigure( &scene, start, down ? B3_PI : 0.0f );
		SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, down ? -1.5f : 1.5f } );
		float from = stairs ? 3.8f : 4.5f;
		float to = stairs ? 8.2f : 9.5f;
		float jump = 0.0f;
		float bob = 0.0f;
		float low = FLT_MAX;
		float high = -FLT_MAX;
		float last = 0.0f;
		bool lastInside = false;
		float step = 0.0f;
		float steps = 0.0f;
		float heel = -FLT_MAX;
		float landedAt = 0.0f;
		bool landed = false;
		bool swings[2] = { false, false };
		for ( int i = 0; i < 600; ++i )
		{
			StepFigures( &scene, 1 );
			const nbFigure* f = GetSteered( &scene, figure );
			float z = b3ToVec3( f->frame.p ).z;
			bool inside = z > from && z < to;
			b3Vec3 pelvis = b3ToVec3( f->targets[nb_humanoidPelvis].p );
			float h = stairs ? pelvis.y - 0.5f * ( pelvis.z - 3.0f ) : pelvis.y - b3ToVec3( f->frame.p ).y;
			if ( inside && lastInside )
			{
				jump = b3MaxFloat( jump, fabsf( h - last ) );
			}
			low = b3MinFloat( low, h );
			high = b3MaxFloat( high, h );
			for ( int k = 0; k < 2; ++k )
			{
				if ( swings[k] && f->footSwings[k] == false && inside )
				{
					// It lands: the rise and fall of the pelvis since the last landing, and the length of the step
					float at = b3ToVec3( f->feet[k].plant ).z;
					if ( landed )
					{
						bob = b3MaxFloat( bob, high - low );
						step += fabsf( at - landedAt );
						steps += 1.0f;
					}
					heel = b3MaxFloat( heel, f->footRoll[k] );
					landedAt = at;
					landed = true;
					low = h;
					high = h;
				}
				swings[k] = f->footSwings[k];
			}
			last = h;
			lastInside = inside;
		}
		float length = steps > 0.0f ? step / steps : 0.0f;
		if ( stairs == false )
		{
			length /= b3ComputeCosSin( 20.0f * B3_PI / 180.0f ).cosine;
		}
		printf( "  %s: %.0f steps, the pelvis moves %.4f m per update at most and %.3f m between two landings, steps of "
				"%.3f m, landing rolled %.3f rad at most\n",
				names[pass], steps, jump, bob, length, heel );
		ENSURE( steps >= 3.0f );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		ENSURE( jump <= 0.03f );
		ENSURE( bob <= ( stairs ? 0.06f : 0.1f ) );
		if ( stairs == false )
		{
			ENSURE( length <= 0.85f * 0.708f );
			ENSURE( heel <= -0.15f );
		}
		DestroyFigureScene( &scene );
	}
	return 0;
}

// The tread of the twenty stairs of AddSteerCourse under a point, -1 before them and 19 on the landing at their top
static int StairTread( float z )
{
	if ( z < 3.0f )
	{
		return -1;
	}
	int tread = (int)floorf( ( z - 3.0f ) / 0.3f );
	return tread < 19 ? tread : 19;
}

// A steered figure walks up and down twenty stairs of 15 by 30 cm at 1.5 m/s, the conditions of round 86 in
// docs/Roadmap.md: a foot that stands flat on the stairs stands with heel and toes on one tread, no more than 1.5 cm off
// it, and from the third landing on the stairs to the one before the last every step goes the same number of treads, see
// nbFindFoothold
static int SteerStairsTest( void )
{
	const int feet[2] = { nb_humanoidFootLeft, nb_humanoidFootRight };
	for ( int pass = 0; pass < 2; ++pass )
	{
		FigureScene scene;
		CreateFigureScene( &scene, 1 );
		bool down = pass == 1;
		float top = AddSteerCourse( &scene, 4 );
		b3Vec3 start = down ? (b3Vec3){ 0.0f, top, 11.0f } : b3Vec3_zero;
		nbFigureId figure = AddSteeredFigure( &scene, start, down ? B3_PI : 0.0f );
		SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, down ? -1.5f : 1.5f } );
		int treads[64];
		int landings = 0;
		int across = 0;
		int flat = 0;
		float off = 0.0f;
		bool swings[2] = { false, false };
		for ( int i = 0; i < 600; ++i )
		{
			StepFigures( &scene, 1 );
			const nbFigure* f = GetSteered( &scene, figure );
			for ( int k = 0; k < 2; ++k )
			{
				b3Vec3 heel = SoleEdge( &scene, f, feet[k], -1.0f );
				b3Vec3 toe = SoleEdge( &scene, f, feet[k], 1.0f );
				int heelTread = StairTread( heel.z );
				int toeTread = StairTread( toe.z );
				bool onStairs = heelTread >= 0 && toeTread >= 0 && heelTread < 19 && toeTread < 19;
				if ( f->gaitOn && f->footSwings[k] == false && f->footRoll[k] == 0.0f && onStairs )
				{
					// It stands flat: heel and toes on its tread
					float height = 0.15f * (float)( heelTread + 1 );
					across += heelTread != toeTread ? 1 : 0;
					off = b3MaxFloat( off, b3MaxFloat( fabsf( heel.y - height ), fabsf( toe.y - height ) ) );
					flat += 1;
				}
				int middle = StairTread( 0.5f * ( heel.z + toe.z ) );
				if ( swings[k] && f->footSwings[k] == false && middle >= 0 && middle < 19 && landings < 64 )
				{
					treads[landings] = middle;
					landings += 1;
				}
				swings[k] = f->footSwings[k];
			}
		}
		int each = landings >= 3 ? abs( treads[2] - treads[1] ) : 0;
		bool same = true;
		for ( int j = 3; j + 1 < landings; ++j )
		{
			same = same && abs( treads[j] - treads[j - 1] ) == each;
		}
		printf( "  %s: %d landings on the stairs, steps of %d treads from the third on%s, %d flat stances, %d across an edge, "
				"a sole edge %.4f m off its tread at most\n",
				down ? "down" : "up", landings, each, same ? "" : " and others", flat, across, off );
		ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
		ENSURE( landings >= 6 );
		ENSURE( each >= 1 && same );
		ENSURE( flat > 0 && across == 0 );
		ENSURE( off <= 0.015f );
		DestroyFigureScene( &scene );
	}
	return 0;
}

// A steered figure walks at 1.6 m/s, turns back and then to the side, after the conditions of round 87 in
// docs/Roadmap.md: walking straight its feet land 10 to 18 cm apart across its way, and from the turn on its pelvis
// changes its velocity by no more than 2.5 cm in an update and goes no more than 8 cm lower than walking straight. Until
// active-ragdoll-86 its feet landed 26 cm apart, and it sank by 24 cm in the turn, where a foot stood until its leg could
// not reach it any more.
static int SteerTurnTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddSteeredFigure( &scene, b3Vec3_zero, 0.0f );
	const int bones[3] = { nb_humanoidPelvis, nb_humanoidFootLeft, nb_humanoidFootRight };
	b3Vec3 last[3] = { b3Vec3_zero, b3Vec3_zero, b3Vec3_zero };
	b3Vec3 before[3] = { b3Vec3_zero, b3Vec3_zero, b3Vec3_zero };
	bool swung[2] = { false, false };
	float narrowest = FLT_MAX;
	float widest = 0.0f;
	float walkLow = FLT_MAX;
	float turnLow = FLT_MAX;
	float jump = 0.0f;
	int landings = 0;
	for ( int i = 0; i < 600; ++i )
	{
		// Straight on for 4.5 s, back for 3 s and to the side for 2.5 s
		b3Vec3 wish = { 0.0f, 0.0f, 1.6f };
		if ( i >= 450 )
		{
			wish = (b3Vec3){ 1.6f, 0.0f, 0.0f };
		}
		else if ( i >= 270 )
		{
			wish = (b3Vec3){ 0.0f, 0.0f, -1.6f };
		}
		SteerVelocity( figure, wish );
		StepFigures( &scene, 1 );
		const nbFigure* f = GetSteered( &scene, figure );
		b3Vec3 now[3];
		for ( int j = 0; j < 3; ++j )
		{
			now[j] = BonePosition( figure, bones[j] );
		}
		if ( i >= 120 && i < 270 )
		{
			// Walking straight along z, how far apart across its way the feet are as one lands
			walkLow = b3MinFloat( walkLow, now[0].y );
			for ( int k = 0; k < 2; ++k )
			{
				if ( swung[k] && f->footSwings[k] == false )
				{
					float across = fabsf( now[1].x - now[2].x );
					narrowest = b3MinFloat( narrowest, across );
					widest = b3MaxFloat( widest, across );
					landings += 1;
				}
			}
		}
		if ( i >= 272 )
		{
			turnLow = b3MinFloat( turnLow, now[0].y );
			jump = b3MaxFloat( jump, b3Length( b3Add( b3Sub( now[0], last[0] ), b3Sub( before[0], last[0] ) ) ) );
		}
		for ( int k = 0; k < 2; ++k )
		{
			swung[k] = f->footSwings[k];
		}
		for ( int j = 0; j < 3; ++j )
		{
			before[j] = last[j];
			last[j] = now[j];
		}
	}
	printf( "  %d landings walking straight, feet %.3f to %.3f m apart, from the turn on the pelvis changes its velocity "
			"by %.4f m in an update at most and sinks %.3f m below walking\n",
			landings, narrowest, widest, jump, walkLow - turnLow );
	ENSURE( landings >= 4 );
	ENSURE( narrowest >= 0.1f && widest <= 0.18f );
	ENSURE( jump <= 0.025f );
	ENSURE( walkLow - turnLow <= 0.08f );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	DestroyFigureScene( &scene );
	return 0;
}

// A steered figure hit hard from behind while it runs: its pelvis leaves with the velocity of the run and that of the
// hit, within 20 %, and once it stood up again it steers again and walks on
static int SteerHitTest( void )
{
	FigureScene scene;
	CreateFigureScene( &scene, 1 );
	nbFigureId figure = AddSteeredFigure( &scene, (b3Vec3){ 0.0f, 0.0f, -40.0f }, 0.0f );
	SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, 4.5f } );
	StepFigures( &scene, 180 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );

	b3Pos back = b3Body_GetWorldPoint( nbFigure_GetBody( figure, nb_humanoidChest ), (b3Vec3){ 0.0f, 0.08f, -0.1f } );
	nbHitDef hit = { 1, nb_humanoidChest, { 0.0f, 0.0f, 120.0f }, back };
	ENSURE( nbFigure_ApplyHit( figure, &hit ) );
	SteerVelocity( figure, (b3Vec3){ 0.0f, 0.0f, 1.5f } );
	StepFigures( &scene, 1 );
	float mass = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		mass += b3Body_GetMass( nbFigure_GetBody( figure, i ) );
	}
	float expected = 4.5f + 120.0f / mass;
	float leaves = b3Body_GetLinearVelocity( nbFigure_GetBody( figure, nb_humanoidPelvis ) ).z;
	nbFigureState hitState = nbFigure_GetState( figure );

	int standsAfter = StepUntil( &scene, figure, nb_figureAnimated, 900 );
	b3Vec3 before = b3ToVec3( nbFigure_GetFrame( figure ).p );
	StepFigures( &scene, 120 );
	b3Vec3 after = b3ToVec3( nbFigure_GetFrame( figure ).p );
	float walks = 0.5f * ( after.z - before.z );
	printf( "  the pelvis leaves at %.2f m/s for %.2f, it stands again after %.2f s and walks on at %.2f m/s\n", leaves, expected,
			(float)standsAfter / 60.0f, walks );
	ENSURE( hitState != nb_figureAnimated );
	ENSURE( fabsf( leaves - expected ) <= 0.2f * expected );
	ENSURE( standsAfter >= 0 );
	ENSURE( nbFigure_GetState( figure ) == nb_figureAnimated );
	ENSURE( fabsf( walks - 1.5f ) < 0.15f );
	DestroyFigureScene( &scene );
	return 0;
}

// A course of steered figures for the same bits with any number of threads: one walks, runs at a wall at an angle, slides
// along it, stops and turns on the spot, one walks up stairs and is hit from the side, and one walks past a figure that
// only stands
static uint32_t RunSteerScenario( int threadCount )
{
	FigureScene scene;
	CreateFigureScene( &scene, threadCount );
	AddBlock( &scene, (b3Vec3){ -2.0f, 1.5f, 8.0f }, (b3Vec3){ 4.0f, 1.5f, 0.2f } );
	for ( int i = 0; i < 6; ++i )
	{
		float height = 0.15f * (float)( i + 1 );
		AddBlock( &scene, (b3Vec3){ 5.0f, 0.5f * height, 8.0f + 0.3f * (float)i }, (b3Vec3){ 1.0f, 0.5f * height, 5.0f } );
	}
	nbFigureId figures[4];
	figures[0] = AddSteeredFigure( &scene, b3Vec3_zero, 0.0f );
	figures[1] = AddSteeredFigure( &scene, (b3Vec3){ 5.0f, 0.0f, 0.0f }, 0.0f );
	figures[2] = AddSteeredFigure( &scene, (b3Vec3){ -6.0f, 0.0f, 3.3f }, 0.5f * B3_PI );
	figures[3] = AddFigure( &scene, (b3Vec3){ -3.0f, 0.0f, 3.0f }, -0.5f * B3_PI );

	uint32_t hash = 2166136261u;
	for ( int step = 0; step < 600; ++step )
	{
		if ( step == 0 )
		{
			SteerVelocity( figures[0], (b3Vec3){ 0.0f, 0.0f, 1.5f } );
			SteerVelocity( figures[1], (b3Vec3){ 0.0f, 0.0f, 1.5f } );
			SteerVelocity( figures[2], (b3Vec3){ 1.2f, 0.0f, 0.0f } );
		}
		if ( step == 120 )
		{
			SteerVelocity( figures[0], (b3Vec3){ -1.5f, 0.0f, 4.2f } );
		}
		if ( step == 200 )
		{
			b3BodyId chest = nbFigure_GetBody( figures[1], nb_humanoidChest );
			b3Pos point = b3Body_GetWorldPoint( chest, (b3Vec3){ 0.1f, 0.08f, 0.0f } );
			nbHitDef hit = { 7, nb_humanoidChest, { -100.0f, 0.0f, 0.0f }, point };
			nbFigure_ApplyHit( figures[1], &hit );
		}
		if ( step == 300 )
		{
			nbSteer turn = { b3Vec3_zero, -2.0f, false };
			nbFigure_Steer( figures[0], &turn );
		}
		nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
	}

	for ( int k = 0; k < 4; ++k )
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
		const uint8_t* bytes = (const uint8_t*)&info.moveVelocity;
		for ( size_t b = 0; b < sizeof( b3Vec3 ); ++b )
		{
			hash = ( hash ^ bytes[b] ) * 16777619u;
		}
		hash = ( hash ^ (uint32_t)( info.state + 4 * ( info.onGround ? 1 : 0 ) + 8 * info.knockdownCount ) ) * 16777619u;
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

	// The most bones that lay inside a part of the wall that stands two steps in a row, and the bone steps in which a bone
	// lay inside it for one step only
	int bonesInWall;
	int bonesPressedIn;

	uint32_t hash;
} WallThrow;

// The bones of a figure that lie inside a static chunk of the wall: the closest point of the chunk is the center itself
static int BonesInWall( nbFigureId figure, nbDestructibleId wall, bool* inside )
{
	nbChunkId chunks[1024];
	int chunkCount = nbDestructible_GetChunks( wall, chunks, 1024 );
	int count = 0;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		inside[i] = false;
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
				inside[i] = true;
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
	bool wasInside[nb_humanoidBoneCount] = { false };
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
		bool inside[nb_humanoidBoneCount];
		BonesInWall( figure, wall, inside );
		int stuck = 0;
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			stuck += inside[i] && wasInside[i] ? 1 : 0;
			result.bonesPressedIn += inside[i] && wasInside[i] == false ? 1 : 0;
			wasInside[i] = inside[i];
		}
		result.bonesInWall = stuck > result.bonesInWall ? stuck : result.bonesInWall;
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

// A figure kicked very hard, 1000 N s, into a wall of boards of wood, 5 cm with its back or 3 cm with its side, flies
// on through the hole it breaks in one to four of eight fracture patterns, in the others the edge of the hole stops it.
// Which patterns let it through is chance: of 32 round 41 flies through 15 at 5 cm with its back and 11 at 3 cm with its
// side, three of the first eight each, round 40 17 and 10, four of the first eight each, round 52 15 and 10, three of
// the first eight each, and round 53 13 and 8, three and one of the first eight: the turn of the pelvis after the
// feet, see nbPelvisTurn, changes the last bits of the few updates the figure balances before it falls, and with them
// the patterns it gets through.
// Since the joints move as far as a person's, see nbMakeHumanoidBones, its back breaks through 5 cm in two, before in
// five: the trunk and the head give way and do not strike the boards at once. A wall of concrete of 20 cm and boards of
// 20 cm hold it, though the concrete loses pieces where the trunk slams into it at 20 m/s, and the boards up to 86
// since the hit spreads along the skeleton, see nbSpreadHit, before up to 56. The thin wall holds against a kick that
// only knocks the figure down. In every throw the bones that hit the wall in one step make one collision of the figure
// with the wall, steps with several bones in it come up, the momentum of the figure toward the wall never grows in a
// step it goes through, and no bone stays inside the wall that stands. A bone pressed hard against it may lie with its
// center past the face for a step before its contact pushes it out: in one throw at concrete side first a hand crushed
// between the body and the wall at 13 m/s lies 5 mm inside it for a step, since the neck has its armature, see
// nbMakeHumanoidBones. See the roadmap of the game, 26.16 and 26.19 C.
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
		{ "boards of 5 cm, back first", true, 0.05f, 1000.0f, false, 2, 8, 40 },
		{ "boards of 3 cm, side first", true, 0.03f, 1000.0f, true, 1, 8, 90 },
		{ "boards of 20 cm, back first", true, 0.2f, 1000.0f, false, 0, 0, 100 },
		{ "concrete of 20 cm, back first", false, 0.2f, 1000.0f, false, 0, 0, 60 },
		{ "concrete of 20 cm, side first", false, 0.2f, 1000.0f, true, 0, 0, 15 },
		{ "boards of 5 cm, a weak kick", true, 0.05f, 150.0f, false, 0, 0, 2 },
	};
	int sharedSteps = 0;
	for ( size_t c = 0; c < sizeof( cases ) / sizeof( cases[0] ); ++c )
	{
		const WallCase* wallCase = cases + c;
		int through = 0;
		int shared = 0;
		int pressed = 0;
		int mostDebris = 0;
		for ( uint32_t seed = 1; seed <= 8; ++seed )
		{
			WallThrow result =
				ThrowAtWall( 1, wallCase->wood, wallCase->thickness, seed, wallCase->impulse, wallCase->side );
			through += result.through ? 1 : 0;
			shared += result.sharedSteps;
			pressed += result.bonesPressedIn;
			mostDebris = result.debrisCount > mostDebris ? result.debrisCount : mostDebris;
			ENSURE( result.mostCollisionsInAStep <= 1 );
			ENSURE( result.splitSteps == 0 );
			ENSURE( result.gained == false );
			ENSURE( result.passThrough <= 1.0f );
			ENSURE( result.bonesInWall == 0 );
			ENSURE( wallCase->maxThrough > 0 || result.passThrough == 0.0f || result.through == false );
			ENSURE( wallCase->impulse < 500.0f || result.collisionCount >= 1 );
		}
		printf( "  %s, %.0f N s: through %d of 8 times, %d steps with several bones in one collision, up to %d pieces, %d bones "
				"pressed into it for a step\n",
				wallCase->name, wallCase->impulse, through, shared, mostDebris, pressed );
		ENSURE( wallCase->minThrough <= through && through <= wallCase->maxThrough );
		ENSURE( mostDebris <= wallCase->maxDebris );
		sharedSteps += shared;
	}
	ENSURE( sharedSteps > 0 );
	return 0;
}

// A figure that broke through a wall of boards lies behind it among the pieces, its bones touch them again, and no bone
// ends up inside the wall that stands. It gets up there once nothing lies on it. A block that falls on it while it gets
// up breaks that off, and it gets up again later, see the roadmap of the game, 26.15, 26.19 C and 28.5 (E). Both turns
// are chaotic: in 32 fracture patterns the figure breaks through 11 times, and in 4 of them the block of 154 kg stays
// on it, three times half on the ground on an arm or a leg, once on its belly, and the figure waits under it, as 26.15
// asks, on active-ragdoll-26 13 and 6 times, on active-ragdoll-34 14 and 7, standing in the knees on active-ragdoll-35
// 13 and 3, with the arms of 81 cm on active-ragdoll-36 11 and 7, and with the chest of two parts of round 37 13 and 1.
// In 4 of the 32 it comes to rest over the lower edge of the hole it broke, the trunk hanging down on one side and the
// legs on the other, with no ground under its pelvis, and waits there for good, as on active-ragdoll-36 in 4 of 64 more
// patterns, see F69 of the roadmap. Round 40 broke through 15 times and the block stayed on it 3 times. Round 41, whose
// upper body bends on with the tilt of the pelvis in the update before it falls, see nbBendTrunk, breaks through 12
// times, the block stays on it twice and pieces of the wall hold it down 3 more times. With the arms of 78 cm of round 44
// it gets up behind the wall in 13 of the 32 and again after the block in 8, the eleventh no longer breaks through.
// Round 52 gets up behind the wall in 12 and again after the block in 7, and round 53, whose pelvis turns after the
// feet, see nbPelvisTurn, in 10 and 8, once of them standing on a board of the wall, 5 cm over the ground, and the
// second pattern no longer among them. Round 56 gets up behind the wall in 11 and again after the block in 9, and round
// 57, whose soles grip more, see NB_SOLE_FRICTION, in 9 and 8, the second pattern among them again, and in the third it
// lies 1.15 m behind the wall, past the pieces. Round 59, whose neck and head take more damping while the figure
// catches itself, see nbBoneDef::catchDamping, gets up behind the wall in 10 and again after the block in 7, the second
// pattern no longer among them. Round 60, whose spine gives way to hits from ahead and behind, see
// nbBoneDef::hitStiffness, gets up behind the wall in 12 and again after the block in 8, the second pattern among them
// again and the fifth no longer after the block. Until round 40 the test took the second pattern, from round 41 the
// eleventh, from round 44 the second again, from round 53 the third, from round 57 the second again, from round 59 the
// fifth, and since round 60 the second again, where it gets up in both.
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
		bool inside[nb_humanoidBoneCount];
		ENSURE( BonesInWall( figure, wall, inside ) == 0 );
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
	bool inside[nb_humanoidBoneCount];
	ENSURE( BonesInWall( figure, wall, inside ) == 0 );
	DestroyFigureScene( &scene );
	return 0;
}

// The throw through the wall of boards ends in the same bits with one thread and with four, on every platform, in the
// second fracture pattern, which the figure breaks through
#define NB_EXPECTED_WALL_HASH 0xa6cf90dau

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
#define NB_EXPECTED_FIGURE_HASH 0x0403272bu

// See RunFollowScenario
#define NB_EXPECTED_FOLLOW_HASH 0x6f1d5739u

// See RunSteerScenario
#define NB_EXPECTED_STEER_HASH 0x61069695u

static int FigureFollowDeterminismTest( void )
{
	uint32_t one = RunFollowScenario( 1 );
	uint32_t again = RunFollowScenario( 1 );
	uint32_t four = RunFollowScenario( 4 );
	printf( "  follow hash: 0x%08x\n", one );
	ENSURE( one == again );
	ENSURE( one == four );
	ENSURE( one == NB_EXPECTED_FOLLOW_HASH );
	return 0;
}

static int SteerDeterminismTest( void )
{
	uint32_t one = RunSteerScenario( 1 );
	uint32_t again = RunSteerScenario( 1 );
	uint32_t two = RunSteerScenario( 2 );
	uint32_t four = RunSteerScenario( 4 );
	printf( "  steer hash: 0x%08x\n", one );
	ENSURE( one == again );
	ENSURE( one == two );
	ENSURE( one == four );
	ENSURE( one == NB_EXPECTED_STEER_HASH );
	return 0;
}

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
	RUN_TEST( ParallelJointTorqueTest );
	RUN_TEST( SwingLimitTest );
	RUN_TEST( ArmatureTest );
	RUN_TEST( HumanoidClipTest );
	RUN_TEST( FigureStandTest );
	RUN_TEST( FigureFlinchTest );
	RUN_TEST( FigureSwingTest );
	RUN_TEST( FigureCatchTest );
	RUN_TEST( FigureRowTest );
	RUN_TEST( FigureLegHoldTest );
	RUN_TEST( FigureCatchScanTest );
	RUN_TEST( FigureFacingTest );
	RUN_TEST( FigureMuscleTest );
	RUN_TEST( FigureSpreadTest );
	RUN_TEST( FigureTwistTest );
	RUN_TEST( FigureNeckTest );
	RUN_TEST( FigureCatchDampingTest );
	RUN_TEST( FigureSlowRateTest );
	RUN_TEST( FigureSlowStepTest );
	RUN_TEST( FigureSlowFreeStepTest );
	RUN_TEST( FigureSoftSpineTest );
	RUN_TEST( FigureSoftHoldTest );
	RUN_TEST( FigurePushTest );
	RUN_TEST( FigureHoldArmTest );
#if defined( B3_HAS_SWING_LIMITS )
	RUN_TEST( FigureJointRangeTest );
#endif
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
	RUN_TEST( FigureFollowTest );
	RUN_TEST( FigurePunchTest );
	RUN_TEST( FigureFollowHitTest );
	RUN_TEST( FigureFollowBumpTest );
	RUN_TEST( SteerSpeedTest );
	RUN_TEST( SteerWallTest );
	RUN_TEST( SteerGroundTest );
	RUN_TEST( SteerFootTest );
	RUN_TEST( SteerSlopeTest );
	RUN_TEST( SteerStairsTest );
	RUN_TEST( SteerTurnTest );
	RUN_TEST( SteerHitTest );
	RUN_TEST( FigureWallTest );
	RUN_TEST( FigureWallDebrisTest );
	RUN_TEST( ThinBoardTest );
	RUN_TEST( FigureWallDeterminismTest );
	RUN_TEST( FigureFollowDeterminismTest );
	RUN_TEST( SteerDeterminismTest );
	RUN_TEST( FigureDeterminismTest );
	return 0;
}
