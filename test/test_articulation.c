// SPDX-License-Identifier: MIT

// The stages of the roadmap for figures on Box3D: a hinge pendulum, a three-link arm with motors, a passive ragdoll, a
// figure that holds a pose, a clip of three keyframes, one hit that lands once, a figure that breaks a wall, and the same
// results with any number of threads.

#include "test_macros.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STEP_TIME ( 1.0f / 60.0f )
#define SUB_STEPS 4

typedef struct FigureScene
{
	b3WorldId physicsWorld;
	nbWorldId world;
	b3BodyId groundId;
} FigureScene;

static FigureScene CreateFigureScene( int workerCount )
{
	FigureScene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	scene.physicsWorld = b3CreateWorld( &worldDef );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = b3ToPos( (b3Vec3){ 0.0f, -1.0f, 0.0f } );
	scene.groundId = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.8f;
	b3CreateHullShape( scene.groundId, &shapeDef, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.workerCount = workerCount;
	def.asyncStep = false;
	def.collisionRadiusScale = 0.02f;
	scene.world = nbCreateWorld( &def );
	return scene;
}

static void DestroyFigureScene( FigureScene* scene )
{
	nbDestroyWorld( scene->world );
	b3DestroyWorld( scene->physicsWorld );
}

static void StepFigures( FigureScene* scene, int steps )
{
	for ( int i = 0; i < steps; ++i )
	{
		nbWorld_Step( scene->world, STEP_TIME, SUB_STEPS );
	}
}

static bool FiniteVec3( b3Vec3 v )
{
	return isfinite( v.x ) && isfinite( v.y ) && isfinite( v.z );
}

// A box of this mass with its origin at one end, so a joint at the origin hangs it like a rod
static b3BodyId CreateRod( b3WorldId worldId, b3Vec3 position, float length, float mass )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = b3ToPos( position );
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );

	float half = 0.5f * length;
	float thick = 0.04f;
	b3BoxHull box = b3MakeOffsetBoxHull( thick, half, thick, (b3Vec3){ 0.0f, -half, 0.0f } );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = mass / ( 8.0f * thick * half * thick );
	b3CreateHullShape( bodyId, &shapeDef, &box.base );
	return bodyId;
}

// A hinge about the x axis of the world at the origin of body B, on body A at this point
static b3RevoluteJointDef HingeDef( b3BodyId bodyA, b3BodyId bodyB, b3Vec3 pivotA )
{
	b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
	def.base.bodyIdA = bodyA;
	def.base.bodyIdB = bodyB;
	b3Quat zToX = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
	def.base.localFrameA = (b3Transform){ pivotA, zToX };
	def.base.localFrameB = (b3Transform){ b3Vec3_zero, zToX };
	return def;
}

// The humanoid of the library standing on the ground
typedef struct Figure
{
	nbArticulationId id;
	nbBoneDef bones[nb_humanoidBoneCount];
	float rootHeight;
} Figure;

static Figure CreateHumanoid( FigureScene* scene, b3Vec3 offset, b3Quat rotation, const nbMotorProfile* profile )
{
	Figure figure;
	figure.rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, figure.bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = figure.bones;
	rig.boneCount = nb_humanoidBoneCount;
	b3Vec3 position = { offset.x, figure.rootHeight + offset.y, offset.z };
	figure.id = nbCreateArticulation( scene->world, &rig, b3ToPos( position ), rotation, profile );
	return figure;
}

static float PelvisHeight( const Figure* figure )
{
	return b3ToVec3( b3Body_GetPosition( nbArticulation_GetBody( figure->id, nb_humanoidPelvis ) ) ).y;
}

// Every bone is finite and no joint came apart
static bool CheckFigure( const Figure* figure, float maxSeparation )
{
	int count = nbArticulation_GetBoneCount( figure->id );
	for ( int i = 0; i < count; ++i )
	{
		b3BodyId body = nbArticulation_GetBody( figure->id, i );
		if ( FiniteVec3( b3ToVec3( b3Body_GetPosition( body ) ) ) == false ||
			 FiniteVec3( b3Body_GetLinearVelocity( body ) ) == false )
		{
			return false;
		}
		if ( i > 0 && b3Joint_GetLinearSeparation( nbArticulation_GetJoint( figure->id, i ) ) > maxSeparation )
		{
			return false;
		}
	}
	return true;
}

static float MaxJointError( const Figure* figure )
{
	float error = 0.0f;
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		error = b3MaxFloat( error, nbArticulation_GetJointError( figure->id, i ) );
	}
	return error;
}

static float MaxBoneSpeed( const Figure* figure )
{
	float speed = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		speed = b3MaxFloat( speed, b3Length( b3Body_GetLinearVelocity( nbArticulation_GetBody( figure->id, i ) ) ) );
	}
	return speed;
}

//----------------------------------------------------------------------------------------------------------------------

// A spring with a torque limit pulls no harder than the limit, so it cannot lift a rod that gravity holds down. Without
// the limit the same spring lifts it. The same for a ball joint.
static int SpringTorqueLimitTest( void )
{
#if defined( B3_HAS_SPRING_TORQUE_LIMITS )
	FigureScene scene = CreateFigureScene( 1 );

	b3BodyDef anchorDef = b3DefaultBodyDef();
	anchorDef.position = b3ToPos( (b3Vec3){ 0.0f, 3.0f, 0.0f } );
	b3BodyId anchor = b3CreateBody( scene.physicsWorld, &anchorDef );

	// The default is no limit
	b3RevoluteJointDef defaults = b3DefaultRevoluteJointDef();
	ENSURE( defaults.maxSpringTorque == FLT_MAX );
	b3SphericalJointDef sphericalDefaults = b3DefaultSphericalJointDef();
	ENSURE( sphericalDefaults.maxSpringTorque == FLT_MAX );

	// A rod of 5 kg and 1 m hanging down, its spring wants it horizontal: that takes about 25 N m against gravity
	b3BodyId rod = CreateRod( scene.physicsWorld, (b3Vec3){ 0.0f, 3.0f, 0.0f }, 1.0f, 5.0f );
	b3RevoluteJointDef hinge = HingeDef( anchor, rod, b3Vec3_zero );
	hinge.enableSpring = true;
	hinge.hertz = 8.0f;
	hinge.dampingRatio = 1.0f;
	hinge.targetAngle = 0.5f * B3_PI;
	hinge.maxSpringTorque = 4.0f;
	b3JointId joint = b3CreateRevoluteJoint( scene.physicsWorld, &hinge );
	ENSURE( b3RevoluteJoint_GetMaxSpringTorque( joint ) == 4.0f );

	float maxTorque = 0.0f;
	for ( int i = 0; i < 120; ++i )
	{
		StepFigures( &scene, 1 );
		maxTorque = b3MaxFloat( maxTorque, fabsf( b3RevoluteJoint_GetSpringTorque( joint ) ) );
	}
	float angle = b3RevoluteJoint_GetAngle( joint );
	printf( "hinge spring with 4 N m: torque %.2f N m, angle %.2f of %.2f rad\n", maxTorque, angle, hinge.targetAngle );
	ENSURE( maxTorque <= 4.0f + 1.0e-3f );
	ENSURE( fabsf( angle ) < 0.5f );

	// Without the limit the rod comes up, a spring sags a little under the load
	b3RevoluteJoint_SetMaxSpringTorque( joint, FLT_MAX );
	StepFigures( &scene, 180 );
	angle = b3RevoluteJoint_GetAngle( joint );
	printf( "hinge spring without limit: angle %.2f rad\n", angle );
	ENSURE( fabsf( angle - hinge.targetAngle ) < 0.1f );
	ENSURE( fabsf( b3RevoluteJoint_GetSpringTorque( joint ) ) > 10.0f );

	// A ball joint: the same rod, the spring wants it turned to the side by a quarter turn
	b3BodyId ballRod = CreateRod( scene.physicsWorld, (b3Vec3){ 3.0f, 3.0f, 0.0f }, 1.0f, 5.0f );
	b3SphericalJointDef ball = b3DefaultSphericalJointDef();
	ball.base.bodyIdA = anchor;
	ball.base.bodyIdB = ballRod;
	ball.base.localFrameA = (b3Transform){ { 3.0f, 0.0f, 0.0f }, b3Quat_identity };
	ball.base.localFrameB = b3Transform_identity;
	ball.enableSpring = true;
	ball.hertz = 8.0f;
	ball.dampingRatio = 1.0f;
	ball.targetRotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.5f * B3_PI );
	ball.maxSpringTorque = 4.0f;
	b3JointId ballJoint = b3CreateSphericalJoint( scene.physicsWorld, &ball );

	maxTorque = 0.0f;
	for ( int i = 0; i < 120; ++i )
	{
		StepFigures( &scene, 1 );
		maxTorque = b3MaxFloat( maxTorque, b3Length( b3SphericalJoint_GetSpringTorque( ballJoint ) ) );
	}
	b3Vec3 tip = b3ToVec3( b3Body_GetWorldPoint( ballRod, (b3Vec3){ 0.0f, -1.0f, 0.0f } ) );
	printf( "ball spring with 4 N m: torque %.2f N m, tip at %.2f m below the pivot\n", maxTorque, 3.0f - tip.y );
	ENSURE( maxTorque <= 4.0f + 1.0e-3f );
	ENSURE( 3.0f - tip.y > 0.8f );

	b3SphericalJoint_SetMaxSpringTorque( ballJoint, FLT_MAX );
	StepFigures( &scene, 180 );
	tip = b3ToVec3( b3Body_GetWorldPoint( ballRod, (b3Vec3){ 0.0f, -1.0f, 0.0f } ) );
	printf( "ball spring without limit: tip at %.2f m below the pivot\n", 3.0f - tip.y );
	ENSURE( 3.0f - tip.y < 0.2f );

	DestroyFigureScene( &scene );
#endif
	return 0;
}

// Stage 1 of the roadmap: two bodies and a hinge. The pendulum stays within its limits, its anchor holds, nothing blows up
// and it comes to rest.
static int HingePendulumTest( void )
{
	FigureScene scene = CreateFigureScene( 1 );

	b3BodyDef anchorDef = b3DefaultBodyDef();
	anchorDef.position = b3ToPos( (b3Vec3){ 0.0f, 3.0f, 0.0f } );
	b3BodyId anchor = b3CreateBody( scene.physicsWorld, &anchorDef );

	// Damping on the spin alone drains a pendulum slowly: the joint turns the motion of its center back into spin every
	// step, so both velocities are damped
	b3BodyId rod = CreateRod( scene.physicsWorld, (b3Vec3){ 0.0f, 3.0f, 0.0f }, 1.0f, 2.0f );
	b3Body_SetAngularVelocity( rod, (b3Vec3){ 6.0f, 0.0f, 0.0f } );
	b3Body_SetAngularDamping( rod, 0.5f );
	b3Body_SetLinearDamping( rod, 0.5f );

	b3RevoluteJointDef hinge = HingeDef( anchor, rod, b3Vec3_zero );
	hinge.enableLimit = true;
	hinge.lowerAngle = -1.0f;
	hinge.upperAngle = 1.0f;
	b3JointId joint = b3CreateRevoluteJoint( scene.physicsWorld, &hinge );

	float maxSeparation = 0.0f;
	float maxOvershoot = 0.0f;
	for ( int i = 0; i < 600; ++i )
	{
		StepFigures( &scene, 1 );
		float angle = b3RevoluteJoint_GetAngle( joint );
		ENSURE( isfinite( angle ) );
		maxOvershoot = b3MaxFloat( maxOvershoot, b3MaxFloat( hinge.lowerAngle - angle, angle - hinge.upperAngle ) );
		maxSeparation = b3MaxFloat( maxSeparation, b3Joint_GetLinearSeparation( joint ) );
	}

	float speed = b3Length( b3Body_GetAngularVelocity( rod ) );
	printf( "pendulum: separation %.2f mm, over the limit %.3f rad, speed after 10 s %.3f rad/s\n", 1000.0f * maxSeparation,
			maxOvershoot, speed );
	ENSURE( maxSeparation < 0.01f );
	ENSURE( maxOvershoot < 0.05f );
	ENSURE( speed < 0.2f );

	DestroyFigureScene( &scene );
	return 0;
}

// Stage 2: three links with motors reach their angles against gravity, without jumps, with their anchors tight
static int ThreeLinkArmTest( void )
{
	FigureScene scene = CreateFigureScene( 1 );

	b3BodyDef anchorDef = b3DefaultBodyDef();
	anchorDef.position = b3ToPos( (b3Vec3){ 0.0f, 3.0f, 0.0f } );
	b3BodyId anchor = b3CreateBody( scene.physicsWorld, &anchorDef );

	float targets[3] = { 0.6f, -0.9f, 0.4f };
	b3BodyId links[3];
	b3JointId joints[3];
	b3BodyId parent = anchor;
	b3Vec3 pivot = b3Vec3_zero;
	for ( int i = 0; i < 3; ++i )
	{
		links[i] = CreateRod( scene.physicsWorld, (b3Vec3){ 0.0f, 3.0f - 0.5f * (float)i, 0.0f }, 0.5f, 1.0f );
		b3RevoluteJointDef hinge = HingeDef( parent, links[i], pivot );
		hinge.enableLimit = true;
		hinge.lowerAngle = -2.5f;
		hinge.upperAngle = 2.5f;
		// Stiff enough that the links sag little under the ones below them
		hinge.enableSpring = true;
		hinge.hertz = 15.0f;
		hinge.dampingRatio = 1.0f;
		hinge.targetAngle = targets[i];
		hinge.maxSpringTorque = 200.0f;
		joints[i] = b3CreateRevoluteJoint( scene.physicsWorld, &hinge );
		parent = links[i];
		pivot = (b3Vec3){ 0.0f, -0.5f, 0.0f };
	}

	float maxStep = 0.0f;
	float maxSeparation = 0.0f;
	float angles[3] = { 0.0f, 0.0f, 0.0f };
	for ( int step = 0; step < 180; ++step )
	{
		StepFigures( &scene, 1 );
		for ( int i = 0; i < 3; ++i )
		{
			float angle = b3RevoluteJoint_GetAngle( joints[i] );
			ENSURE( isfinite( angle ) );
			maxStep = b3MaxFloat( maxStep, fabsf( angle - angles[i] ) );
			angles[i] = angle;
			maxSeparation = b3MaxFloat( maxSeparation, b3Joint_GetLinearSeparation( joints[i] ) );
		}
	}

	printf( "arm: angles %.3f %.3f %.3f, largest change per step %.3f rad, separation %.2f mm\n", angles[0], angles[1], angles[2],
			maxStep, 1000.0f * maxSeparation );
	for ( int i = 0; i < 3; ++i )
	{
		ENSURE( fabsf( angles[i] - targets[i] ) < 0.08f );
	}
	ENSURE( maxStep < 0.5f );
	ENSURE( maxSeparation < 0.01f );

	DestroyFigureScene( &scene );
	return 0;
}

// The ids, bodies and joints of a figure, and everything goes with it
static int ArticulationApiTest( void )
{
	int64_t baseBytes = nbGetByteCount();
	FigureScene scene = CreateFigureScene( 1 );
	int64_t sceneBytes = nbGetByteCount();

	nbMotorProfile passive = nbPassiveMotorProfile();
	Figure figure = CreateHumanoid( &scene, b3Vec3_zero, b3Quat_identity, &passive );
	ENSURE( nbArticulation_IsValid( figure.id ) );
	ENSURE( nbArticulation_GetBoneCount( figure.id ) == nb_humanoidBoneCount );
	ENSURE( figure.rootHeight > 0.9f && figure.rootHeight < 1.1f );

	// Every bone has a body, every bone but the root a joint to its parent, and the body leads back to the bone
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		b3BodyId body = nbArticulation_GetBody( figure.id, i );
		ENSURE( b3Body_IsValid( body ) );
		ENSURE( b3Body_GetType( body ) == b3_dynamicBody );
		b3JointId joint = nbArticulation_GetJoint( figure.id, i );
		ENSURE( b3Joint_IsValid( joint ) == ( i > 0 ) );

		int bone = -1;
		nbArticulationId found = nbWorld_GetArticulationFromBody( scene.world, body, &bone );
		ENSURE( NB_ID_EQUALS( found, figure.id ) );
		ENSURE( bone == i );
	}
	ENSURE( b3Joint_GetType( nbArticulation_GetJoint( figure.id, nb_humanoidShinLeft ) ) == b3_revoluteJoint );
	ENSURE( b3Joint_GetType( nbArticulation_GetJoint( figure.id, nb_humanoidThighLeft ) ) == b3_sphericalJoint );

	// The feet stand on the ground, the head is on top
	b3WorldTransform transforms[nb_humanoidBoneCount];
	ENSURE( nbArticulation_GetBoneTransforms( figure.id, transforms, nb_humanoidBoneCount ) == nb_humanoidBoneCount );
	float head = b3ToVec3( transforms[nb_humanoidHead].p ).y;
	float foot = b3ToVec3( transforms[nb_humanoidFootLeft].p ).y;
	ENSURE( head > 1.5f && head < 1.7f );
	ENSURE( foot > 0.0f && foot < 0.12f );

	// The mass adds up
	float mass = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		mass += b3Body_GetMass( nbArticulation_GetBody( figure.id, i ) );
	}
	printf( "humanoid: %d bones, %.1f kg, pelvis at %.2f m\n", nb_humanoidBoneCount, mass, figure.rootHeight );
	ENSURE( fabsf( mass - 75.0f ) < 0.5f );

	// A hit lands once per number, one without a number every time
	nbHitDef tap = { 0 };
	tap.bone = nb_humanoidHead;
	tap.impulse = (b3Vec3){ 0.0f, 0.0f, -0.01f };
	tap.point = b3Body_GetWorldCenter( nbArticulation_GetBody( figure.id, nb_humanoidHead ) );
	ENSURE( nbArticulation_ApplyHit( figure.id, &tap ) );
	ENSURE( nbArticulation_ApplyHit( figure.id, &tap ) );
	tap.id = 5;
	ENSURE( nbArticulation_ApplyHit( figure.id, &tap ) );
	ENSURE( nbArticulation_ApplyHit( figure.id, &tap ) == false );
	tap.bone = nb_humanoidBoneCount;
	tap.id = 6;
	ENSURE( nbArticulation_ApplyHit( figure.id, &tap ) == false );
	ENSURE( nbArticulation_GetHitCount( figure.id ) == 3 );

	// A body that is no bone
	int bone = 0;
	ENSURE( NB_IS_NULL( nbWorld_GetArticulationFromBody( scene.world, scene.groundId, &bone ) ) );
	ENSURE( bone == -1 );

	b3BodyId pelvis = nbArticulation_GetBody( figure.id, nb_humanoidPelvis );
	nbDestroyArticulation( figure.id );
	ENSURE( nbArticulation_IsValid( figure.id ) == false );
	ENSURE( b3Body_IsValid( pelvis ) == false );
	ENSURE( NB_IS_NULL( nbWorld_GetArticulationFromBody( scene.world, pelvis, NULL ) ) );
	ENSURE( b3World_GetAwakeBodyCount( scene.physicsWorld ) == 0 );

	// A second figure takes the slot with a new generation
	Figure second = CreateHumanoid( &scene, b3Vec3_zero, b3Quat_identity, &passive );
	ENSURE( nbArticulation_IsValid( second.id ) );
	ENSURE( second.id.index1 == figure.id.index1 );
	ENSURE( second.id.generation != figure.id.generation );
	ENSURE( nbArticulation_IsValid( figure.id ) == false );
	ENSURE( nbGetByteCount() > sceneBytes );

	DestroyFigureScene( &scene );
	ENSURE( nbGetByteCount() == baseBytes );
	return 0;
}

// Stage 3: a passive humanoid dropped from half a meter lands, falls over and comes to rest lying down. Every joint holds
// to within the stretch of the solver, nothing sinks into the ground or flies off.
static int PassiveRagdollTest( void )
{
	FigureScene scene = CreateFigureScene( 1 );
	nbMotorProfile passive = nbPassiveMotorProfile();
	b3Quat tilt = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.3f );
	Figure figure = CreateHumanoid( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, tilt, &passive );

	float maxSpeed = 0.0f;
	float lowest = FLT_MAX;
	float maxSeparation = 0.0f;
	float maxOvershoot = 0.0f;
	for ( int step = 0; step < 360; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( CheckFigure( &figure, 0.05f ) );
		maxSpeed = b3MaxFloat( maxSpeed, MaxBoneSpeed( &figure ) );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			b3BodyId body = nbArticulation_GetBody( figure.id, i );
			lowest = b3MinFloat( lowest, b3ToVec3( b3Body_GetWorldCenter( body ) ).y );
			if ( i > 0 )
			{
				maxSeparation =
					b3MaxFloat( maxSeparation, b3Joint_GetLinearSeparation( nbArticulation_GetJoint( figure.id, i ) ) );
			}
		}

		// The knees and elbows stay within their limits
		int hinges[4] = { nb_humanoidShinLeft, nb_humanoidShinRight, nb_humanoidForearmLeft, nb_humanoidForearmRight };
		for ( int k = 0; k < 4; ++k )
		{
			b3JointId joint = nbArticulation_GetJoint( figure.id, hinges[k] );
			float angle = b3RevoluteJoint_GetAngle( joint );
			float lower = b3RevoluteJoint_GetLowerLimit( joint );
			float upper = b3RevoluteJoint_GetUpperLimit( joint );
			maxOvershoot = b3MaxFloat( maxOvershoot, b3MaxFloat( lower - angle, angle - upper ) );
		}
	}

	float pelvis = PelvisHeight( &figure );
	float restSpeed = MaxBoneSpeed( &figure );
	printf( "passive ragdoll: pelvis %.2f m, lowest center %.3f m, fastest %.1f m/s, at rest %.3f m/s, separation %.1f mm, "
			"over the limit %.3f rad\n",
			pelvis, lowest, maxSpeed, restSpeed, 1000.0f * maxSeparation, maxOvershoot );
	// The joints stretch for a few steps when a bone hits the ground hard, measured 4 to 6 cm with a drop of half a meter,
	// and the limits give by a tenth of a radian under the weight of the body
	ENSURE( pelvis < 0.5f );
	ENSURE( lowest > -0.03f );
	ENSURE( maxSpeed < 15.0f );
	ENSURE( restSpeed < 0.3f );
	ENSURE( maxSeparation < 0.07f );
	ENSURE( maxOvershoot < 0.15f );

	DestroyFigureScene( &scene );
	return 0;
}

// Stage 4 and step 2 of chapter 33: a figure with motors holds its standing pose for five seconds, its joints near their
// targets and its motors below their limits
static int StandTest( void )
{
	FigureScene scene = CreateFigureScene( 1 );
	nbMotorProfile stand = nbDefaultMotorProfile();
	Figure figure = CreateHumanoid( &scene, b3Vec3_zero, b3Quat_identity, &stand );

	float lowestPelvis = FLT_MAX;
	float maxError = 0.0f;
	for ( int step = 0; step < 300; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( CheckFigure( &figure, 0.02f ) );
		lowestPelvis = b3MinFloat( lowestPelvis, PelvisHeight( &figure ) );
		if ( step > 60 )
		{
			maxError = b3MaxFloat( maxError, MaxJointError( &figure ) );
		}
	}

	float load = 0.0f;
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		load = b3MaxFloat( load, nbArticulation_GetMotorLoad( figure.id, i ) );
	}
	b3Vec3 head = b3ToVec3( b3Body_GetPosition( nbArticulation_GetBody( figure.id, nb_humanoidHead ) ) );
	printf( "stand: pelvis down to %.3f of %.3f m, joint error %.3f rad, motor load %.2f, head at %.2f m\n", lowestPelvis,
			figure.rootHeight, maxError, load, head.y );
	ENSURE( lowestPelvis > 0.9f * figure.rootHeight );
	ENSURE( maxError < 0.15f );
	ENSURE( load < 1.0f );
	ENSURE( head.y > 1.5f );
	ENSURE( MaxBoneSpeed( &figure ) < 0.2f );

	DestroyFigureScene( &scene );
	return 0;
}

// A clip of three keyframes: the left arm comes up, the elbow bends, the arm goes down
typedef struct ArmClip
{
	b3Quat poses[3][nb_humanoidBoneCount];
	nbKeyframe keyframes[3];
	nbClipDef clip;
} ArmClip;

static void MakeArmClip( ArmClip* clip, bool loop )
{
	for ( int k = 0; k < 3; ++k )
	{
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			clip->poses[k][i] = b3Quat_identity;
		}
	}

	// About the local x axis of a limb that hangs down a positive turn brings it forward
	clip->poses[1][nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.3f );
	clip->poses[1][nb_humanoidForearmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.5f );
	clip->poses[2][nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.2f );

	float times[3] = { 0.0f, 0.6f, 1.2f };
	for ( int k = 0; k < 3; ++k )
	{
		clip->keyframes[k].time = times[k];
		clip->keyframes[k].rotations = clip->poses[k];
	}
	clip->clip.keyframes = clip->keyframes;
	clip->clip.keyframeCount = 3;
	clip->clip.boneCount = nb_humanoidBoneCount;
	clip->clip.loop = loop;
}

// Step 3 of chapter 33: the clip plays without a jump in the pose, the motors follow it, the keyframes come out as they
// went in, and the clip ends on its last pose
static int ClipTest( void )
{
	ArmClip arm;
	MakeArmClip( &arm, false );

	// Sampling gives the keyframes back at their times, between them a blend
	b3Quat sampled[nb_humanoidBoneCount];
	for ( int k = 0; k < 3; ++k )
	{
		nbSampleClip( &arm.clip, arm.keyframes[k].time, sampled, nb_humanoidBoneCount );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			ENSURE( fabsf( b3DotQuat( sampled[i], arm.poses[k][i] ) ) > 0.99999f );
		}
	}
	nbSampleClip( &arm.clip, 0.3f, sampled, nb_humanoidBoneCount );
	float half = b3GetQuatAngle( sampled[nb_humanoidUpperArmLeft] );
	ENSURE( half > 0.6f && half < 0.7f );
	nbSampleClip( &arm.clip, 5.0f, sampled, nb_humanoidBoneCount );
	ENSURE( fabsf( b3DotQuat( sampled[nb_humanoidUpperArmLeft], arm.poses[2][nb_humanoidUpperArmLeft] ) ) > 0.99999f );

	// A clip that loops comes around
	ArmClip loop;
	MakeArmClip( &loop, true );
	nbSampleClip( &loop.clip, 1.2f + 0.6f, sampled, nb_humanoidBoneCount );
	ENSURE( fabsf( b3DotQuat( sampled[nb_humanoidForearmLeft], loop.poses[1][nb_humanoidForearmLeft] ) ) > 0.99999f );

	// A long clip: the elbow bends by a hundredth more in every one of 200 keyframes
	enum
	{
		longCount = 200
	};
	b3Quat* longPoses = malloc( sizeof( b3Quat ) * longCount * nb_humanoidBoneCount );
	nbKeyframe longKeys[longCount];
	for ( int k = 0; k < longCount; ++k )
	{
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			longPoses[k * nb_humanoidBoneCount + i] = b3Quat_identity;
		}
		longPoses[k * nb_humanoidBoneCount + nb_humanoidForearmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.01f * (float)k );
		longKeys[k].time = 0.1f * (float)k;
		longKeys[k].rotations = longPoses + k * nb_humanoidBoneCount;
	}
	nbClipDef longClip = { longKeys, longCount, nb_humanoidBoneCount, false };
	nbSampleClip( &longClip, 0.1f * 150.5f, sampled, nb_humanoidBoneCount );
	float bend = b3GetQuatAngle( sampled[nb_humanoidForearmLeft] );
	free( longPoses );
	printf( "clip of %d keyframes: %.4f rad at keyframe 150.5\n", longCount, bend );
	ENSURE( fabsf( bend - 1.505f ) < 1.0e-3f );

	FigureScene scene = CreateFigureScene( 1 );
	nbMotorProfile stand = nbDefaultMotorProfile();
	Figure figure = CreateHumanoid( &scene, b3Vec3_zero, b3Quat_identity, &stand );
	StepFigures( &scene, 60 );

	nbArticulation_PlayClip( figure.id, &arm.clip, 1.0f );
	ENSURE( nbArticulation_IsClipPlaying( figure.id ) );

	b3JointId shoulder = nbArticulation_GetJoint( figure.id, nb_humanoidUpperArmLeft );
	b3JointId elbow = nbArticulation_GetJoint( figure.id, nb_humanoidForearmLeft );
	float lastElbow = b3RevoluteJoint_GetAngle( elbow );
	b3Quat lastShoulder = b3Body_GetRotation( nbArticulation_GetBody( figure.id, nb_humanoidUpperArmLeft ) );
	float maxStep = 0.0f;
	float maxElbow = 0.0f;
	float maxError = 0.0f;
	float lowestPelvis = FLT_MAX;
	for ( int step = 0; step < 90; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( CheckFigure( &figure, 0.02f ) );
		float elbowAngle = b3RevoluteJoint_GetAngle( elbow );
		maxElbow = b3MaxFloat( maxElbow, elbowAngle );
		b3Quat shoulderRotation = b3Body_GetRotation( nbArticulation_GetBody( figure.id, nb_humanoidUpperArmLeft ) );
		float turn = b3GetQuatAngle( b3InvMulQuat( lastShoulder, shoulderRotation ) );
		turn = turn > B3_PI ? 2.0f * B3_PI - turn : turn;
		maxStep = b3MaxFloat( maxStep, b3MaxFloat( fabsf( elbowAngle - lastElbow ), turn ) );
		lastElbow = elbowAngle;
		lastShoulder = shoulderRotation;
		maxError = b3MaxFloat( maxError, nbArticulation_GetJointError( figure.id, nb_humanoidUpperArmLeft ) );
		lowestPelvis = b3MinFloat( lowestPelvis, PelvisHeight( &figure ) );
	}

	float coneAngle = b3SphericalJoint_GetConeAngle( shoulder );
	printf( "clip: elbow up to %.2f rad, shoulder error up to %.2f rad, largest turn per step %.3f rad, time %.2f s, pelvis "
			"down to %.3f m, shoulder cone now %.2f rad\n",
			maxElbow, maxError, maxStep, nbArticulation_GetClipTime( figure.id ), lowestPelvis, coneAngle );
	ENSURE( maxElbow > 1.0f );
	ENSURE( maxError < 0.5f );
	ENSURE( maxStep < 0.25f );
	ENSURE( nbArticulation_IsClipPlaying( figure.id ) == false );
	ENSURE( fabsf( nbArticulation_GetClipTime( figure.id ) - 1.2f ) < 1.0e-5f );
	ENSURE( lowestPelvis > 0.9f * figure.rootHeight );

	// The arm settles on the last pose
	StepFigures( &scene, 60 );
	ENSURE( nbArticulation_GetJointError( figure.id, nb_humanoidUpperArmLeft ) < 0.1f );
	ENSURE( nbArticulation_GetJointError( figure.id, nb_humanoidForearmLeft ) < 0.1f );

	// A target pose stops the clip and the arm follows it. A target is no guaranteed end position: the spring of the
	// elbow sags under the forearm and the hand, the harder the farther from hanging down.
	b3Quat pose[nb_humanoidBoneCount];
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		pose[i] = b3Quat_identity;
	}
	pose[nb_humanoidForearmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.0f );
	nbArticulation_SetTargetPose( figure.id, pose, nb_humanoidBoneCount );
	ENSURE( nbArticulation_IsClipPlaying( figure.id ) == false );
	StepFigures( &scene, 90 );
	float elbowAngle = b3RevoluteJoint_GetAngle( elbow );
	printf( "clip: elbow at %.2f rad for a target of 1.00 rad, sag %.2f rad\n", elbowAngle, 1.0f - elbowAngle );
	ENSURE( elbowAngle > 0.6f && elbowAngle < 1.1f );

	DestroyFigureScene( &scene );
	return 0;
}

// Step 4 of chapter 33: a hit lands once, whoever reports it. A moderate hit on the chest leaves the figure standing, a
// heavy one knocks it down, and nothing is teleported either way.
static int HitTest( void )
{
	FigureScene scene = CreateFigureScene( 1 );
	nbMotorProfile stand = nbDefaultMotorProfile();
	Figure figure = CreateHumanoid( &scene, b3Vec3_zero, b3Quat_identity, &stand );
	StepFigures( &scene, 60 );

	b3BodyId torso = nbArticulation_GetBody( figure.id, nb_humanoidTorso );
	b3Vec3 before = b3Body_GetLinearVelocity( torso );

	// A push of 20 N s moves 75 kg at 0.27 m/s, about what a person absorbs without a step
	nbHitDef hit = { 0 };
	hit.id = 7;
	hit.bone = nb_humanoidTorso;
	hit.impulse = (b3Vec3){ 0.0f, 0.0f, -20.0f };
	hit.point = b3Body_GetWorldPoint( torso, (b3Vec3){ 0.0f, 0.3f, 0.1f } );
	ENSURE( nbArticulation_ApplyHit( figure.id, &hit ) );
	ENSURE( nbArticulation_ApplyHit( figure.id, &hit ) == false );
	ENSURE( nbArticulation_GetHitCount( figure.id ) == 1 );

	// The impulse went in once: the torso moves back by the impulse over its mass, before the joints share it out
	b3Vec3 after = b3Body_GetLinearVelocity( torso );
	float kick = before.z - after.z;
	float expected = 20.0f / b3Body_GetMass( torso );
	printf( "hit: torso %.2f m/s back, %.2f for the impulse alone\n", kick, expected );
	ENSURE( kick > 0.9f * expected && kick < 1.1f * expected );

	float lowestPelvis = FLT_MAX;
	float maxSpeed = 0.0f;
	float maxSeparation = 0.0f;
	for ( int step = 0; step < 180; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( CheckFigure( &figure, 0.07f ) );
		lowestPelvis = b3MinFloat( lowestPelvis, PelvisHeight( &figure ) );
		maxSpeed = b3MaxFloat( maxSpeed, MaxBoneSpeed( &figure ) );
		for ( int i = 1; i < nb_humanoidBoneCount; ++i )
		{
			maxSeparation = b3MaxFloat( maxSeparation, b3Joint_GetLinearSeparation( nbArticulation_GetJoint( figure.id, i ) ) );
		}
	}
	printf( "hit of 20 N s: pelvis down to %.3f of %.3f m, fastest bone %.1f m/s, joints stretched %.1f mm, error now %.3f rad\n",
			lowestPelvis, figure.rootHeight, maxSpeed, 1000.0f * maxSeparation, MaxJointError( &figure ) );
	ENSURE( lowestPelvis > 0.85f * figure.rootHeight );
	ENSURE( maxSpeed < 10.0f );
	ENSURE( MaxJointError( &figure ) < 0.2f );

	// A heavy hit bends the torso back against its motors and knocks the figure down
	hit.id = 8;
	hit.impulse = (b3Vec3){ 0.0f, 0.0f, -120.0f };
	hit.point = b3Body_GetWorldPoint( torso, (b3Vec3){ 0.0f, 0.3f, 0.1f } );
	ENSURE( nbArticulation_ApplyHit( figure.id, &hit ) );
	maxSpeed = 0.0f;
	maxSeparation = 0.0f;
	float torsoYield = 0.0f;
	for ( int step = 0; step < 240; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( CheckFigure( &figure, 0.1f ) );
		maxSpeed = b3MaxFloat( maxSpeed, MaxBoneSpeed( &figure ) );
		torsoYield = b3MaxFloat( torsoYield, nbArticulation_GetJointError( figure.id, nb_humanoidTorso ) );
		for ( int i = 1; i < nb_humanoidBoneCount; ++i )
		{
			maxSeparation = b3MaxFloat( maxSeparation, b3Joint_GetLinearSeparation( nbArticulation_GetJoint( figure.id, i ) ) );
		}
	}
	float pelvis = PelvisHeight( &figure );
	printf( "hit of 120 N s: pelvis at %.3f m, fastest bone %.1f m/s, joints stretched %.1f mm, torso gave way %.2f rad\n",
			pelvis, maxSpeed, 1000.0f * maxSeparation, torsoYield );
	ENSURE( pelvis < 0.6f );
	ENSURE( maxSpeed < 40.0f );
	ENSURE( torsoYield > 0.1f );
	ENSURE( nbArticulation_GetHitCount( figure.id ) == 2 );

	DestroyFigureScene( &scene );
	return 0;
}

// Chapter 26.16: a figure thrown at a wall breaks it through the collision damage of the world, like a cannonball, and
// the wall stops it
static int WallHitTest( void )
{
	FigureScene scene = CreateFigureScene( 1 );

	nbDestructibleDef wallDef = nbDefaultDestructibleDef();
	wallDef.position = b3ToPos( (b3Vec3){ 0.0f, 1.5f, -3.0f } );
	wallDef.seed = 11;
	nbDestructibleId wall = nbCreateBox( scene.world, &wallDef, (b3Vec3){ 2.5f, 1.5f, 0.1f } );
	ENSURE( nbDestructible_IsIntact( wall ) );

	nbMotorProfile passive = nbPassiveMotorProfile();
	Figure figure = CreateHumanoid( &scene, (b3Vec3){ 0.0f, 0.3f, 0.0f }, b3Quat_identity, &passive );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		b3Body_SetLinearVelocity( nbArticulation_GetBody( figure.id, i ), (b3Vec3){ 0.0f, 2.0f, -20.0f } );
	}

	for ( int step = 0; step < 120; ++step )
	{
		StepFigures( &scene, 1 );
		ENSURE( CheckFigure( &figure, 0.1f ) );
	}

	b3Vec3 pelvis = b3ToVec3( b3Body_GetPosition( nbArticulation_GetBody( figure.id, nb_humanoidPelvis ) ) );
	nbStats stats = nbWorld_GetStats( scene.world );
	printf( "wall: intact %s, %d chunks, pelvis at z %.2f m\n", nbDestructible_IsIntact( wall ) ? "yes" : "no", stats.chunkCount,
			pelvis.z );
	ENSURE( nbDestructible_IsIntact( wall ) == false );
	ENSURE( pelvis.z > -8.0f );

	DestroyFigureScene( &scene );
	return 0;
}

static uint32_t HashFigure( uint32_t hash, const Figure* figure )
{
	b3WorldTransform transforms[nb_humanoidBoneCount];
	int count = nbArticulation_GetBoneTransforms( figure->id, transforms, nb_humanoidBoneCount );
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
	return hash;
}

// A figure that stands, plays its clip, takes a hit and falls, with this many threads
static uint32_t RunFigureScenario( int workerCount )
{
	FigureScene scene = CreateFigureScene( workerCount );
	nbMotorProfile stand = nbDefaultMotorProfile();
	Figure figure = CreateHumanoid( &scene, b3Vec3_zero, b3Quat_identity, &stand );
	nbMotorProfile passive = nbPassiveMotorProfile();
	Figure other =
		CreateHumanoid( &scene, (b3Vec3){ 1.0f, 0.4f, 0.0f }, b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.4f ), &passive );

	ArmClip arm;
	MakeArmClip( &arm, true );
	nbArticulation_PlayClip( figure.id, &arm.clip, 1.0f );

	uint32_t hash = 2166136261u;
	for ( int step = 0; step < 240; ++step )
	{
		if ( step == 60 )
		{
			nbHitDef hit = { 0 };
			hit.id = 1;
			hit.bone = nb_humanoidTorso;
			hit.impulse = (b3Vec3){ 30.0f, 0.0f, -300.0f };
			hit.point =
				b3Body_GetWorldPoint( nbArticulation_GetBody( figure.id, nb_humanoidTorso ), (b3Vec3){ 0.0f, 0.3f, 0.1f } );
			nbArticulation_ApplyHit( figure.id, &hit );
		}
		StepFigures( &scene, 1 );
		if ( step % 30 == 29 )
		{
			hash = HashFigure( hash, &figure );
			hash = HashFigure( hash, &other );
		}
	}

	DestroyFigureScene( &scene );
	return hash;
}

// The figures end up the same to the bit with one thread and with four
static int FigureDeterminismTest( void )
{
	uint32_t hash1 = RunFigureScenario( 1 );
	uint32_t hash2 = RunFigureScenario( 1 );
	uint32_t hash4 = RunFigureScenario( 4 );
	printf( "figure determinism hash: 0x%08x\n", hash1 );
	ENSURE( hash1 == hash2 );
	ENSURE( hash1 == hash4 );
	return 0;
}

int ArticulationTest( void );

int ArticulationTest( void )
{
	RUN_TEST( SpringTorqueLimitTest );
	RUN_TEST( HingePendulumTest );
	RUN_TEST( ThreeLinkArmTest );
	RUN_TEST( ArticulationApiTest );
	RUN_TEST( PassiveRagdollTest );
	RUN_TEST( StandTest );
	RUN_TEST( ClipTest );
	RUN_TEST( HitTest );
	RUN_TEST( WallHitTest );
	RUN_TEST( FigureDeterminismTest );
	return 0;
}
