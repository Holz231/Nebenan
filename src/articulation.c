// SPDX-License-Identifier: MIT

// Articulations: figures of Box3D bodies and joints that follow a pose with bounded joint springs. A rig describes the
// bones at rest, an articulation is one figure made from it. Its joints are springs to a target rotation with a torque
// limit, so the figure holds a pose but gives way to anything stronger, and a parallel joint against a static anchor can
// hold the root upright until the figure learns to step. The program sets a target pose, or a clip of keyframes plays
// and the update samples it once per step.

#include "articulation.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

// Damping ratio of the joint constraints of a figure, see nbCreateArticulation
#define NB_JOINT_DAMPING_RATIO 5.0f

// The torque limit of the joint springs is an addition to Box3D, see extern/README.md. Without it the motors hold their
// pose against anything and nbArticulation_GetMotorLoad knows nothing.
#if defined( B3_HAS_SPRING_TORQUE_LIMITS )
#define NB_SPRING_TORQUE_LIMITS 1
#else
#define NB_SPRING_TORQUE_LIMITS 0
#endif

// A motor at this part of its torque limit works at it, see nbFigureStats::saturatedJointCount
#define NB_SATURATED_LOAD 0.99f

//----------------------------------------------------------------------------------------------------------------------
// Definitions

nbBoneDef nbDefaultBoneDef( void )
{
	nbBoneDef def = { 0 };
	def.parent = -1;
	def.restTransform = b3Transform_identity;
	def.shape = nb_capsuleBone;
	def.start = (b3Vec3){ 0.0f, 0.05f, 0.0f };
	def.end = (b3Vec3){ 0.0f, 0.25f, 0.0f };
	def.radius = 0.05f;
	def.halfExtents = (b3Vec3){ 0.05f, 0.05f, 0.05f };
	def.mass = 1.0f;
	def.joint = nb_ballJoint;
	def.jointFrame = b3Quat_identity;
	def.lowerAngle = -0.5f;
	def.upperAngle = 0.5f;
	def.coneAngle = 0.5f;
	def.strength = 1.0f;
	return def;
}

nbRigDef nbDefaultRigDef( void )
{
	nbRigDef def = { 0 };
	def.friction = 0.6f;
	def.up = b3Vec3_axisY;
	def.selfCollision = true;
	def.enableCollisionDamage = true;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

nbMotorProfile nbDefaultMotorProfile( void )
{
	nbMotorProfile profile = { 0 };
	profile.stiffness = 800.0f;
	profile.dampingRatio = 1.0f;
	profile.maxTorque = 150.0f;
	profile.uprightTorque = 250.0f;
	profile.uprightStiffness = 1500.0f;
	profile.uprightDampingRatio = 1.0f;
	return profile;
}

nbMotorProfile nbPassiveMotorProfile( void )
{
	nbMotorProfile profile = { 0 };
	return profile;
}

//----------------------------------------------------------------------------------------------------------------------
// Humanoid

// A bone along its local y axis from the pivot to the next pivot, as a capsule that fits between them
static nbBoneDef nbLimb( const char* name, int parent, b3Vec3 pivot, b3Quat rest, float length, float radius, float mass,
						 nbJointType joint, b3Quat jointFrame, float lower, float upper, float cone, float strength )
{
	nbBoneDef def = nbDefaultBoneDef();
	def.name = name;
	def.parent = parent;
	def.restTransform.p = pivot;
	def.restTransform.q = rest;
	float inset = b3MinFloat( radius, 0.5f * length );
	def.start = (b3Vec3){ 0.0f, inset, 0.0f };
	def.end = (b3Vec3){ 0.0f, length - inset, 0.0f };
	def.radius = radius;
	def.mass = mass;
	def.joint = joint;
	def.jointFrame = jointFrame;
	def.lowerAngle = lower;
	def.upperAngle = upper;
	def.coneAngle = cone;
	def.strength = strength;
	return def;
}

// The masses below add up to this, for a figure of 1.8 m. Hands and feet are heavier than on a person: a foot of 1 kg
// that has to stop the 75 kg landing on it stretches its ankle joint by centimeters for a few steps, a foot of 2.5 kg by
// less than half of that. The solver likes mass ratios below 30 to 1 between joined bodies.
#define NB_HUMANOID_MASS 73.0f
#define NB_HUMANOID_HEIGHT 1.8f

float nbMakeHumanoidBones( float height, float mass, nbBoneDef* bones, int capacity )
{
	if ( bones == NULL || capacity < nb_humanoidBoneCount || height <= 0.0f || mass <= 0.0f )
	{
		return 0.0f;
	}

	float s = height / NB_HUMANOID_HEIGHT;
	float m = mass / NB_HUMANOID_MASS;

	// A limb that hangs down has its local y along world -y. The cone of a ball joint sits on the bone, the hinge axis
	// runs across it along local x. The foot points forward, along +z.
	b3Quat down = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, B3_PI );
	b3Quat zToY = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -0.5f * B3_PI );
	b3Quat zToX = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
	b3Quat footRest = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.5f * B3_PI );
	b3Quat identity = b3Quat_identity;

	// Pelvis: a short capsule across the hips, the root
	nbBoneDef pelvis = nbDefaultBoneDef();
	pelvis.name = "pelvis";
	pelvis.parent = -1;
	pelvis.start = (b3Vec3){ -0.06f * s, 0.0f, 0.0f };
	pelvis.end = (b3Vec3){ 0.06f * s, 0.0f, 0.0f };
	pelvis.radius = 0.11f * s;
	pelvis.mass = 11.0f * m;
	pelvis.joint = nb_fixedJoint;
	bones[nb_humanoidPelvis] = pelvis;

	bones[nb_humanoidTorso] = nbLimb( "torso", nb_humanoidPelvis, (b3Vec3){ 0.0f, 0.10f * s, 0.0f }, identity, 0.48f * s,
									  0.13f * s, 22.0f * m, nb_ballJoint, zToY, -0.4f, 0.4f, 0.35f, 1.6f );
	bones[nb_humanoidHead] = nbLimb( "head", nb_humanoidTorso, (b3Vec3){ 0.0f, 0.48f * s, 0.0f }, identity, 0.25f * s, 0.10f * s,
									 5.0f * m, nb_ballJoint, zToY, -0.8f, 0.8f, 0.6f, 0.3f );

	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		int upperArm = side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight;
		int forearm = side == 0 ? nb_humanoidForearmLeft : nb_humanoidForearmRight;
		int hand = side == 0 ? nb_humanoidHandLeft : nb_humanoidHandRight;
		int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
		int shin = side == 0 ? nb_humanoidShinLeft : nb_humanoidShinRight;
		int foot = side == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight;

		bones[upperArm] = nbLimb( side == 0 ? "upper arm left" : "upper arm right", nb_humanoidTorso,
								  (b3Vec3){ 0.23f * s * x, 0.42f * s, 0.0f }, down, 0.32f * s, 0.045f * s, 2.0f * m, nb_ballJoint,
								  zToY, -0.8f, 0.8f, 1.5f, 0.6f );
		bones[forearm] = nbLimb( side == 0 ? "forearm left" : "forearm right", upperArm, (b3Vec3){ 0.0f, 0.32f * s, 0.0f },
								 identity, 0.28f * s, 0.04f * s, 1.2f * m, nb_hingeJoint, zToX, -0.05f, 2.4f, 0.0f, 0.4f );
		bones[hand] = nbLimb( side == 0 ? "hand left" : "hand right", forearm, (b3Vec3){ 0.0f, 0.28f * s, 0.0f }, identity,
							  0.18f * s, 0.035f * s, 0.8f * m, nb_ballJoint, zToY, -0.3f, 0.3f, 0.6f, 0.15f );

		bones[thigh] =
			nbLimb( side == 0 ? "thigh left" : "thigh right", nb_humanoidPelvis, (b3Vec3){ 0.10f * s * x, -0.03f * s, 0.0f },
					down, 0.45f * s, 0.075f * s, 7.5f * m, nb_ballJoint, zToY, -0.5f, 0.5f, 1.2f, 2.0f );
		bones[shin] = nbLimb( side == 0 ? "shin left" : "shin right", thigh, (b3Vec3){ 0.0f, 0.45f * s, 0.0f }, identity,
							  0.44f * s, 0.055f * s, 3.5f * m, nb_hingeJoint, zToX, -2.4f, 0.05f, 0.0f, 1.5f );

		// A box with a flat sole, 6 cm under the ankle, reaching forward
		nbBoneDef footDef = nbDefaultBoneDef();
		footDef.name = side == 0 ? "foot left" : "foot right";
		footDef.parent = shin;
		footDef.restTransform.p = (b3Vec3){ 0.0f, 0.44f * s, 0.0f };
		footDef.restTransform.q = footRest;
		footDef.shape = nb_boxBone;
		footDef.center = (b3Vec3){ 0.0f, 0.09f * s, -0.03f * s };
		footDef.halfExtents = (b3Vec3){ 0.045f * s, 0.13f * s, 0.03f * s };
		footDef.mass = 2.5f * m;
		footDef.joint = nb_ballJoint;
		footDef.jointFrame = zToY;
		footDef.lowerAngle = -0.35f;
		footDef.upperAngle = 0.35f;
		footDef.coneAngle = 0.5f;
		footDef.strength = 0.8f;
		bones[foot] = footDef;
	}

	// Hip 3 cm under the pelvis origin, thigh, shin and the sole 6 cm under the ankle
	return ( 0.03f + 0.45f + 0.44f + 0.06f ) * s;
}

//----------------------------------------------------------------------------------------------------------------------
// Lookup

nbArticulation* nbGetArticulationFromId( nbArticulationId id, nbWorld** worldOut )
{
	if ( id.index1 < 1 || id.world0 >= NB_MAX_WORLDS )
	{
		return NULL;
	}

	nbWorld* world = nbGetWorld( id.world0 );
	if ( world->inUse == false || id.index1 > world->articulations.count )
	{
		return NULL;
	}

	nbArticulation* articulation = world->articulations.data + ( id.index1 - 1 );
	if ( articulation->isFree || articulation->generation != id.generation )
	{
		return NULL;
	}

	if ( worldOut != NULL )
	{
		*worldOut = world;
	}
	return articulation;
}

static nbArticulationId nbMakeArticulationId( const nbWorld* world, int index )
{
	const nbArticulation* articulation = world->articulations.data + index;
	return (nbArticulationId){ index + 1, world->worldIndex, articulation->generation };
}

// Remember the bone of a body, by the index of the body in Box3D
static void nbMapBone( nbWorld* world, b3BodyId bodyId, int articulation, int bone )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( bodyIndex >= world->bodyToBone.count )
	{
		nbArray_Reserve( world->bodyToBone, bodyIndex + 1 );
		for ( int k = world->bodyToBone.count; k <= bodyIndex; ++k )
		{
			world->bodyToBone.data[k] = (nbBoneRef){ NB_NULL_INDEX, NB_NULL_INDEX };
		}
		world->bodyToBone.count = bodyIndex + 1;
	}
	world->bodyToBone.data[bodyIndex] = (nbBoneRef){ articulation, bone };
}

bool nbIsArticulationBody( const nbWorld* world, int index, b3BodyId bodyId )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( bodyIndex < 0 || bodyIndex >= world->bodyToBone.count )
	{
		return false;
	}

	nbBoneRef ref = world->bodyToBone.data[bodyIndex];
	if ( ref.articulation != index )
	{
		return false;
	}

	// The body at the index may be another one by now
	b3BodyId stored = world->articulations.data[index].bones[ref.bone].bodyId;
	return stored.index1 == bodyId.index1 && stored.generation == bodyId.generation;
}

static void nbUnmapBone( nbWorld* world, b3BodyId bodyId )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( 0 <= bodyIndex && bodyIndex < world->bodyToBone.count )
	{
		world->bodyToBone.data[bodyIndex] = (nbBoneRef){ NB_NULL_INDEX, NB_NULL_INDEX };
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Joints and motors

// Rotate the z axis onto a direction
static b3Quat nbQuatZTo( b3Vec3 direction )
{
	float length = b3Length( direction );
	if ( length < FLT_EPSILON )
	{
		return b3Quat_identity;
	}
	return b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3MulSV( 1.0f / length, direction ) );
}

// Box3D sets a joint spring in cycles per second against the rotational mass of its two bodies about the axis of the
// spring: the same cycles make a weak spring on a light foot and a strong one on a heavy thigh. A stiffness in newton
// meters per radian is the same on every joint, this turns it into the cycles per second of one joint. The rotational
// mass is taken about the softest of the axes, so a ball joint is at least this stiff about every axis.
static float nbSpringHertz( b3BodyId bodyA, b3BodyId bodyB, b3Quat frameB, bool hingeOnly, float stiffness )
{
	if ( stiffness <= 0.0f )
	{
		return 0.0f;
	}

	b3Matrix3 invI =
		b3AddMM( b3Body_GetWorldInverseRotationalInertia( bodyA ), b3Body_GetWorldInverseRotationalInertia( bodyB ) );
	b3Vec3 axes[3] = { b3RotateVector( frameB, b3Vec3_axisX ), b3RotateVector( frameB, b3Vec3_axisY ),
					   b3RotateVector( frameB, b3Vec3_axisZ ) };
	float invMass = 0.0f;
	for ( int k = hingeOnly ? 2 : 0; k < 3; ++k )
	{
		invMass = b3MaxFloat( invMass, b3Dot( axes[k], b3MulMV( invI, axes[k] ) ) );
	}
	if ( invMass <= 0.0f )
	{
		return 0.0f;
	}

	return sqrtf( stiffness * invMass ) / ( 2.0f * B3_PI );
}

// The cycles per second of the spring of a bone, see nbSpringHertz
static float nbBoneHertz( const nbArticulation* articulation, const nbBone* bone, float stiffness )
{
	b3Quat frameB = b3MulQuat( b3Body_GetRotation( bone->bodyId ), bone->jointFrame );
	return nbSpringHertz( articulation->bones[bone->parent].bodyId, bone->bodyId, frameB, bone->joint == nb_hingeJoint,
						  stiffness );
}

// The target of a joint as Box3D sees it: the rotation of joint frame B against joint frame A. A bone rotated by q
// against its rest pose puts frame B at conj(f) q f against frame A, with f the joint frame in the frame of the bone.
static b3Quat nbJointRotation( const nbBone* bone, b3Quat rotation )
{
	b3Quat relative = b3MulQuat( b3MulQuat( b3Conjugate( bone->jointFrame ), rotation ), bone->jointFrame );
	return b3NormalizeQuat( relative );
}

static void nbDriveJoint( const nbBone* bone )
{
	b3Quat relative = nbJointRotation( bone, bone->target );
	switch ( bone->joint )
	{
		case nb_hingeJoint:
			b3RevoluteJoint_SetTargetAngle( bone->jointId, b3GetTwistAngle( relative ) );
			break;
		case nb_ballJoint:
			b3SphericalJoint_SetTargetRotation( bone->jointId, relative );
			break;
		default:
			break;
	}
}

// Set the targets of all joints. Wakes the figure when a target moved, so a sleeping figure follows its clip.
void nbSetTargets( nbArticulation* articulation, const b3Quat* rotations )
{
	bool changed = false;
	for ( int i = 1; i < articulation->boneCount; ++i )
	{
		nbBone* bone = articulation->bones + i;
		b3Quat target = b3NormalizeQuat( rotations[i] );

		// About a tenth of a degree
		if ( fabsf( b3DotQuat( target, bone->target ) ) < 0.9999995f )
		{
			changed = true;
		}
		bone->target = target;
		if ( bone->joint != nb_fixedJoint )
		{
			nbDriveJoint( bone );
		}
	}

	if ( changed && articulation->profile.stiffness > 0.0f )
	{
		b3Body_SetAwake( articulation->bones[0].bodyId, true );
	}
}

// Apply a motor profile to the joints and the upright hold
void nbApplyMotorProfile( nbArticulation* articulation, const nbMotorProfile* profile )
{
	float stiffness = b3MaxFloat( 0.0f, profile->stiffness );
	float dampingRatio = b3MaxFloat( 0.0f, profile->dampingRatio );
	bool enable = stiffness > 0.0f && profile->maxTorque > 0.0f;

	for ( int i = 1; i < articulation->boneCount; ++i )
	{
		const nbBone* bone = articulation->bones + i;
		float maxTorque = b3MaxFloat( 0.0f, profile->maxTorque * bone->strength );
		float hertz = bone->joint == nb_fixedJoint ? 0.0f : nbBoneHertz( articulation, bone, stiffness );
		switch ( bone->joint )
		{
			case nb_hingeJoint:
				b3RevoluteJoint_EnableSpring( bone->jointId, enable );
				b3RevoluteJoint_SetSpringHertz( bone->jointId, hertz );
				b3RevoluteJoint_SetSpringDampingRatio( bone->jointId, dampingRatio );
#if NB_SPRING_TORQUE_LIMITS
				b3RevoluteJoint_SetMaxSpringTorque( bone->jointId, maxTorque );
#endif
				break;
			case nb_ballJoint:
				b3SphericalJoint_EnableSpring( bone->jointId, enable );
				b3SphericalJoint_SetSpringHertz( bone->jointId, hertz );
				b3SphericalJoint_SetSpringDampingRatio( bone->jointId, dampingRatio );
#if NB_SPRING_TORQUE_LIMITS
				b3SphericalJoint_SetMaxSpringTorque( bone->jointId, maxTorque );
#endif
				break;
			default:
				break;
		}
		NB_UNUSED( maxTorque );
	}

	if ( b3Joint_IsValid( articulation->uprightId ) )
	{
		b3Quat frameB = b3MulQuat( b3Body_GetRotation( articulation->bones[0].bodyId ), nbQuatZTo( articulation->up ) );
		float hertz = nbSpringHertz( articulation->anchorId, articulation->bones[0].bodyId, frameB, false,
									 b3MaxFloat( 0.0f, profile->uprightStiffness ) );
		b3ParallelJoint_SetSpringHertz( articulation->uprightId, hertz );
		b3ParallelJoint_SetSpringDampingRatio( articulation->uprightId, b3MaxFloat( 0.0f, profile->uprightDampingRatio ) );
		b3ParallelJoint_SetMaxTorque( articulation->uprightId, b3MaxFloat( 0.0f, profile->uprightTorque ) );
	}
}

void nbSetUprightTorque( nbArticulation* articulation, float torque )
{
	if ( b3Joint_IsValid( articulation->uprightId ) )
	{
		b3ParallelJoint_SetMaxTorque( articulation->uprightId, b3MaxFloat( 0.0f, torque ) );
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Clips

// The time of keyframe k, with the times stride bytes apart: a clip of the program keeps them in its keyframes, an
// articulation in an array of its own
static inline float nbKeyTime( const float* times, size_t stride, int k )
{
	return *(const float*)( (const char*)times + (size_t)k * stride );
}

// Which two keyframes a time falls between, and how far along, from 0 to 1
static int nbClipSegment( const float* times, size_t stride, int count, bool loop, float time, float* alpha )
{
	if ( count < 2 )
	{
		*alpha = 0.0f;
		return 0;
	}

	float start = nbKeyTime( times, stride, 0 );
	float duration = nbKeyTime( times, stride, count - 1 );
	if ( loop && duration > start )
	{
		float length = duration - start;
		time = start + fmodf( time - start, length );
		if ( time < start )
		{
			time += length;
		}
	}
	time = b3ClampFloat( time, start, duration );

	int segment = 0;
	while ( segment + 2 < count && time >= nbKeyTime( times, stride, segment + 1 ) )
	{
		segment += 1;
	}

	float from = nbKeyTime( times, stride, segment );
	float span = nbKeyTime( times, stride, segment + 1 ) - from;
	*alpha = span > 0.0f ? b3ClampFloat( ( time - from ) / span, 0.0f, 1.0f ) : 1.0f;
	return segment;
}

static void nbBlendPoses( const b3Quat* from, const b3Quat* to, float alpha, int boneCount, b3Quat* rotations )
{
	for ( int i = 0; i < boneCount; ++i )
	{
		rotations[i] = b3NLerp( b3NormalizeQuat( from[i] ), b3NormalizeQuat( to[i] ), alpha );
	}
}

void nbSampleClip( const nbClipDef* clip, float time, b3Quat* rotations, int capacity )
{
	if ( clip == NULL || rotations == NULL || capacity < clip->boneCount || clip->keyframeCount <= 0 )
	{
		return;
	}

	float alpha;
	int count = clip->keyframeCount;
	int segment = nbClipSegment( &clip->keyframes[0].time, sizeof( nbKeyframe ), count, clip->loop, time, &alpha );
	const b3Quat* from = clip->keyframes[segment].rotations;
	const b3Quat* to = clip->keyframes[count > 1 ? segment + 1 : segment].rotations;
	nbBlendPoses( from, to, alpha, clip->boneCount, rotations );
}

static void nbSampleArticulationClip( nbArticulation* articulation, b3Quat* rotations )
{
	float alpha;
	int segment = nbClipSegment( articulation->clipTimes, sizeof( float ), articulation->clipKeyframeCount,
								 articulation->clipLoop, articulation->clipTime, &alpha );
	int next = articulation->clipKeyframeCount > 1 ? segment + 1 : segment;
	const b3Quat* from = articulation->clipRotations + segment * articulation->boneCount;
	const b3Quat* to = articulation->clipRotations + next * articulation->boneCount;
	nbBlendPoses( from, to, alpha, articulation->boneCount, rotations );
}

static void nbFreeClip( nbArticulation* articulation )
{
	if ( articulation->clipKeyframeCount > 0 )
	{
		nbFree( articulation->clipTimes, sizeof( float ) * (size_t)articulation->clipKeyframeCount );
		nbFree( articulation->clipRotations,
				sizeof( b3Quat ) * (size_t)articulation->clipKeyframeCount * (size_t)articulation->boneCount );
	}
	articulation->clipTimes = NULL;
	articulation->clipRotations = NULL;
	articulation->clipKeyframeCount = 0;
	articulation->clipPlaying = false;
	articulation->clipTime = 0.0f;

	// A recording ends with its clip
	articulation->recording = NULL;
}

void nbUpdateArticulations( nbWorld* world, float timeStep )
{
	for ( int i = 0; i < world->articulations.count; ++i )
	{
		nbArticulation* articulation = world->articulations.data + i;
		bool clip = articulation->recording != NULL || articulation->clipPlaying;
		if ( articulation->isFree || ( clip == false && articulation->balancer == NULL ) )
		{
			continue;
		}

		// Each part timed for the figures that do it, see nb_profileFigures
		uint64_t lap = b3GetTicks();
		const b3Quat* targets = NULL;
		if ( clip )
		{
			// A recording takes the pose the step left, which the targets of the time of the clip drove to
			if ( articulation->recording != NULL )
			{
				nbRecordPose( world, i, articulation, timeStep );
			}

			if ( articulation->clipPlaying )
			{
				articulation->clipTime += articulation->clipSpeed * timeStep;
				float duration = articulation->clipTimes[articulation->clipKeyframeCount - 1];
				if ( articulation->clipLoop == false && articulation->clipTime >= duration )
				{
					// The clip ends on its last pose
					articulation->clipTime = duration;
					articulation->clipPlaying = false;
				}

				nbSampleArticulationClip( articulation, articulation->pose );
				targets = articulation->pose;
			}
			nbProfileLap( world, nb_profileFigureClips, &lap );
		}

		// Balance takes the pose of the program and gives the targets itself, with the legs and what else it overrides
		if ( articulation->balancer != NULL )
		{
			targets = nbUpdateBalance( world, i, articulation, timeStep );
			nbProfileLap( world, nb_profileFigureBalance, &lap );
		}

		if ( targets != NULL )
		{
			nbSetTargets( articulation, targets );
			nbProfileLap( world, nb_profileFigureMotors, &lap );
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Create and destroy

static float nbBoneVolume( const nbBoneDef* def )
{
	if ( def->shape == nb_boxBone )
	{
		return 8.0f * def->halfExtents.x * def->halfExtents.y * def->halfExtents.z;
	}

	float length = b3Length( b3Sub( def->end, def->start ) );
	float r = def->radius;
	return B3_PI * r * r * length + ( 4.0f / 3.0f ) * B3_PI * r * r * r;
}

nbArticulationId nbCreateArticulation( nbWorldId worldId, const nbRigDef* rig, b3Pos position, b3Quat rotation,
									   const nbMotorProfile* profile )
{
	NB_ASSERT( rig->internalValue == NB_SECRET_COOKIE );
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL || rig->bones == NULL || rig->boneCount <= 0 )
	{
		return nb_nullArticulationId;
	}

	// The root first, every other bone after its parent
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		int parent = rig->bones[i].parent;
		if ( i == 0 ? parent != -1 : ( parent < 0 || parent >= i ) )
		{
			return nb_nullArticulationId;
		}
	}

	int index;
	if ( world->freeArticulations.count > 0 )
	{
		index = world->freeArticulations.data[--world->freeArticulations.count];
	}
	else
	{
		nbArticulation blank = { 0 };
		nbArray_Push( world->articulations, blank );
		index = world->articulations.count - 1;
	}

	nbArticulation* articulation = world->articulations.data + index;
	uint16_t generation = articulation->generation;
	memset( articulation, 0, sizeof( nbArticulation ) );
	articulation->generation = generation;
	articulation->isFree = false;
	articulation->boneCount = rig->boneCount;
	articulation->bones = nbAlloc( sizeof( nbBone ) * (size_t)rig->boneCount );
	articulation->pose = nbAlloc( sizeof( b3Quat ) * (size_t)rig->boneCount );
	articulation->up = b3LengthSquared( rig->up ) > 0.0f ? b3Normalize( rig->up ) : b3Vec3_axisY;
	articulation->profile = profile != NULL ? *profile : nbPassiveMotorProfile();
	articulation->anchorId = b3_nullBodyId;
	articulation->uprightId = b3_nullJointId;

	// Bodies at the rest pose, every bone in the frame of its parent, which comes first. Without self collision every
	// bone of the figure is in a collision group of its own, which Box3D keeps apart, negative and unique per figure.
	b3WorldTransform* transforms = nbAlloc( sizeof( b3WorldTransform ) * (size_t)rig->boneCount );
	int groupIndex = rig->selfCollision ? 0 : -( index + 1 );
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		const nbBoneDef* def = rig->bones + i;
		b3WorldTransform transform;
		if ( i == 0 )
		{
			transform.p = position;
			transform.q = b3NormalizeQuat( rotation );
		}
		else
		{
			transform = b3MulWorldTransforms( transforms[def->parent], def->restTransform );
		}
		transforms[i] = transform;

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = transform.p;
		bodyDef.rotation = transform.q;
		bodyDef.name = def->name;
		b3BodyId bodyId = b3CreateBody( world->physicsWorld, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.name = def->name;
		shapeDef.baseMaterial.friction = rig->friction;
		shapeDef.enableHitEvents = rig->enableCollisionDamage;
		shapeDef.filter.groupIndex = groupIndex;
		float volume = b3MaxFloat( nbBoneVolume( def ), 1.0e-6f );
		shapeDef.density = b3MaxFloat( def->mass, 1.0e-3f ) / volume;
		if ( def->shape == nb_boxBone )
		{
			b3BoxHull box = b3MakeOffsetBoxHull( def->halfExtents.x, def->halfExtents.y, def->halfExtents.z, def->center );
			b3CreateHullShape( bodyId, &shapeDef, &box.base );
		}
		else
		{
			b3Capsule capsule = { def->start, def->end, def->radius };
			b3CreateCapsuleShape( bodyId, &shapeDef, &capsule );
		}

		nbBone* bone = articulation->bones + i;
		bone->bodyId = bodyId;
		bone->jointId = b3_nullJointId;
		bone->parent = def->parent;
		bone->joint = i == 0 ? nb_fixedJoint : def->joint;
		bone->rest = def->restTransform;
		bone->jointFrame = b3NormalizeQuat( def->jointFrame );
		bone->target = b3Quat_identity;
		bone->strength = def->strength;
		bone->shape = def->shape;
		bone->center = def->center;
		bone->halfExtents = def->halfExtents;
		bone->start = def->start;
		bone->end = def->end;
		bone->radius = def->radius;
		articulation->pose[i] = b3Quat_identity;
		nbMapBone( world, bodyId, index, i );
	}

	// How far the figure reaches above and below its root along up, from the shapes at the rest pose
	articulation->restTop = -FLT_MAX;
	articulation->restBottom = FLT_MAX;
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		const nbBoneDef* def = rig->bones + i;
		b3Transform local = b3InvMulWorldTransforms( transforms[0], transforms[i] );
		b3Vec3 points[8];
		int pointCount;
		float radius;
		if ( def->shape == nb_boxBone )
		{
			for ( int k = 0; k < 8; ++k )
			{
				b3Vec3 corner = { k & 1 ? def->halfExtents.x : -def->halfExtents.x,
								  k & 2 ? def->halfExtents.y : -def->halfExtents.y,
								  k & 4 ? def->halfExtents.z : -def->halfExtents.z };
				points[k] = b3Add( def->center, corner );
			}
			pointCount = 8;
			radius = 0.0f;
		}
		else
		{
			points[0] = def->start;
			points[1] = def->end;
			pointCount = 2;
			radius = def->radius;
		}

		for ( int k = 0; k < pointCount; ++k )
		{
			float height = b3Dot( b3TransformPoint( local, points[k] ), articulation->up );
			articulation->restTop = b3MaxFloat( articulation->restTop, height + radius );
			articulation->restBottom = b3MinFloat( articulation->restBottom, height - radius );
		}
	}
	nbFree( transforms, sizeof( b3WorldTransform ) * (size_t)rig->boneCount );

	// Joints: frame A in the parent at the pivot, turned like the bone at rest, frame B at the origin of the bone. Both
	// coincide at rest, so the rest pose is the zero of every angle and the identity of every target. The constraints are
	// damped with 5 instead of Box3D's 2: measured on the passive humanoid falling over, the knees and elbows then
	// overshoot their limits by 0.1 rad at most under the weight of the body, with 2 by up to 0.37 rad. The joints still
	// stretch by 2 to 6 cm for a few steps when a bone hits the ground hard, the solver takes a step to pull them back.
	const nbMotorProfile* motors = &articulation->profile;
	bool enableSpring = motors->stiffness > 0.0f && motors->maxTorque > 0.0f;
	for ( int i = 1; i < rig->boneCount; ++i )
	{
		const nbBoneDef* def = rig->bones + i;
		nbBone* bone = articulation->bones + i;
		b3Transform frameA = { def->restTransform.p, b3MulQuat( def->restTransform.q, bone->jointFrame ) };
		b3Transform frameB = { b3Vec3_zero, bone->jointFrame };
		b3BodyId parentId = articulation->bones[def->parent].bodyId;
		float maxTorque = b3MaxFloat( 0.0f, motors->maxTorque * def->strength );
		float hertz =
			def->joint == nb_fixedJoint ? 0.0f : nbBoneHertz( articulation, bone, b3MaxFloat( 0.0f, motors->stiffness ) );
		NB_UNUSED( maxTorque );

		switch ( def->joint )
		{
			case nb_hingeJoint:
			{
				b3RevoluteJointDef jointDef = b3DefaultRevoluteJointDef();
				jointDef.base.bodyIdA = parentId;
				jointDef.base.bodyIdB = bone->bodyId;
				jointDef.base.localFrameA = frameA;
				jointDef.base.localFrameB = frameB;
				jointDef.base.collideConnected = false;
				jointDef.base.constraintDampingRatio = NB_JOINT_DAMPING_RATIO;
				jointDef.enableLimit = true;
				jointDef.lowerAngle = def->lowerAngle;
				jointDef.upperAngle = def->upperAngle;
				jointDef.enableSpring = enableSpring;
				jointDef.hertz = hertz;
				jointDef.dampingRatio = b3MaxFloat( 0.0f, motors->dampingRatio );
#if NB_SPRING_TORQUE_LIMITS
				jointDef.maxSpringTorque = maxTorque;
#endif
				bone->jointId = b3CreateRevoluteJoint( world->physicsWorld, &jointDef );
				break;
			}
			case nb_ballJoint:
			{
				b3SphericalJointDef jointDef = b3DefaultSphericalJointDef();
				jointDef.base.bodyIdA = parentId;
				jointDef.base.bodyIdB = bone->bodyId;
				jointDef.base.localFrameA = frameA;
				jointDef.base.localFrameB = frameB;
				jointDef.base.collideConnected = false;
				jointDef.base.constraintDampingRatio = NB_JOINT_DAMPING_RATIO;
				jointDef.enableConeLimit = true;
				jointDef.coneAngle = b3ClampFloat( def->coneAngle, 0.0f, 0.5f * B3_PI );
				jointDef.enableTwistLimit = true;
				jointDef.lowerTwistAngle = def->lowerAngle;
				jointDef.upperTwistAngle = def->upperAngle;
				jointDef.enableSpring = enableSpring;
				jointDef.hertz = hertz;
				jointDef.dampingRatio = b3MaxFloat( 0.0f, motors->dampingRatio );
#if NB_SPRING_TORQUE_LIMITS
				jointDef.maxSpringTorque = maxTorque;
#endif
				bone->jointId = b3CreateSphericalJoint( world->physicsWorld, &jointDef );
				break;
			}
			default:
			{
				b3WeldJointDef jointDef = b3DefaultWeldJointDef();
				jointDef.base.bodyIdA = parentId;
				jointDef.base.bodyIdB = bone->bodyId;
				jointDef.base.localFrameA = frameA;
				jointDef.base.localFrameB = frameB;
				jointDef.base.collideConnected = false;
				jointDef.base.constraintDampingRatio = NB_JOINT_DAMPING_RATIO;
				bone->jointId = b3CreateWeldJoint( world->physicsWorld, &jointDef );
				break;
			}
		}
	}

	// The upright hold: a parallel joint keeps the up axis of the root along the up of the world, against a static
	// anchor, with the torque of the profile. Nothing without torque, but there for the next profile.
	{
		b3BodyDef anchorDef = b3DefaultBodyDef();
		anchorDef.type = b3_staticBody;
		anchorDef.position = position;
		articulation->anchorId = b3CreateBody( world->physicsWorld, &anchorDef );

		b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
		b3Vec3 worldUp = b3LengthSquared( gravity ) > 0.0f ? b3Neg( b3Normalize( gravity ) ) : b3Vec3_axisY;

		b3ParallelJointDef jointDef = b3DefaultParallelJointDef();
		jointDef.base.bodyIdA = articulation->anchorId;
		jointDef.base.bodyIdB = articulation->bones[0].bodyId;
		jointDef.base.localFrameA = (b3Transform){ b3Vec3_zero, nbQuatZTo( worldUp ) };
		jointDef.base.localFrameB = (b3Transform){ b3Vec3_zero, nbQuatZTo( articulation->up ) };
		jointDef.base.collideConnected = false;
		b3Quat frameB = b3MulQuat( b3Body_GetRotation( articulation->bones[0].bodyId ), jointDef.base.localFrameB.q );
		jointDef.hertz = nbSpringHertz( articulation->anchorId, articulation->bones[0].bodyId, frameB, false,
										b3MaxFloat( 0.0f, motors->uprightStiffness ) );
		jointDef.dampingRatio = b3MaxFloat( 0.0f, motors->uprightDampingRatio );
		jointDef.maxTorque = b3MaxFloat( 0.0f, motors->uprightTorque );
		articulation->uprightId = b3CreateParallelJoint( world->physicsWorld, &jointDef );
	}

	return nbMakeArticulationId( world, index );
}

static void nbDestroyArticulationAt( nbWorld* world, int index )
{
	nbArticulation* articulation = world->articulations.data + index;
	bool physicsAlive = b3World_IsValid( world->physicsWorld );

	// A body takes its shapes and joints with it
	for ( int i = 0; i < articulation->boneCount; ++i )
	{
		nbUnmapBone( world, articulation->bones[i].bodyId );
		if ( physicsAlive )
		{
			b3DestroyBody( articulation->bones[i].bodyId );
		}
	}
	if ( physicsAlive && b3Body_IsValid( articulation->anchorId ) )
	{
		b3DestroyBody( articulation->anchorId );
	}

	nbFreeClip( articulation );
	nbFreeBalancer( articulation );
	nbFree( articulation->bones, sizeof( nbBone ) * (size_t)articulation->boneCount );
	nbFree( articulation->pose, sizeof( b3Quat ) * (size_t)articulation->boneCount );
	articulation->bones = NULL;
	articulation->pose = NULL;
	articulation->boneCount = 0;
	articulation->isFree = true;
	articulation->generation += 1;
	nbArray_Push( world->freeArticulations, index );
}

void nbDestroyArticulation( nbArticulationId id )
{
	nbWorld* world;
	nbArticulation* articulation = nbGetArticulationFromId( id, &world );
	if ( articulation == NULL )
	{
		return;
	}
	nbDestroyArticulationAt( world, (int)( articulation - world->articulations.data ) );
}

void nbDestroyArticulations( nbWorld* world )
{
	for ( int i = 0; i < world->articulations.count; ++i )
	{
		if ( world->articulations.data[i].isFree == false )
		{
			nbDestroyArticulationAt( world, i );
		}
	}
	nbArray_Free( world->articulations );
	nbArray_Free( world->freeArticulations );
	nbArray_Free( world->bodyToBone );
}

//----------------------------------------------------------------------------------------------------------------------
// Articulation API

bool nbArticulation_IsValid( nbArticulationId id )
{
	return nbGetArticulationFromId( id, NULL ) != NULL;
}

int nbArticulation_GetBoneCount( nbArticulationId id )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	return articulation != NULL ? articulation->boneCount : 0;
}

b3BodyId nbArticulation_GetBody( nbArticulationId id, int bone )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || bone < 0 || bone >= articulation->boneCount )
	{
		return b3_nullBodyId;
	}
	return articulation->bones[bone].bodyId;
}

b3JointId nbArticulation_GetJoint( nbArticulationId id, int bone )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || bone < 0 || bone >= articulation->boneCount )
	{
		return b3_nullJointId;
	}
	return articulation->bones[bone].jointId;
}

void nbArticulation_SetMotorProfile( nbArticulationId id, const nbMotorProfile* profile )
{
	nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || profile == NULL )
	{
		return;
	}

	articulation->profile = *profile;
	if ( articulation->balancer != NULL )
	{
		// Balance decides when the profile of the program applies, a falling figure keeps its own
		nbBalanceProfileChanged( articulation );
	}
	else
	{
		nbApplyMotorProfile( articulation, &articulation->profile );
	}
	b3Body_SetAwake( articulation->bones[0].bodyId, true );
}

nbMotorProfile nbArticulation_GetMotorProfile( nbArticulationId id )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	return articulation != NULL ? articulation->profile : nbPassiveMotorProfile();
}

void nbArticulation_SetTargetPose( nbArticulationId id, const b3Quat* rotations, int count )
{
	nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || rotations == NULL || count < articulation->boneCount )
	{
		return;
	}

	nbFreeClip( articulation );
	memcpy( articulation->pose, rotations, sizeof( b3Quat ) * (size_t)articulation->boneCount );

	// With balance the next update sets the targets, with what balance overrides
	if ( articulation->balancer == NULL )
	{
		nbSetTargets( articulation, articulation->pose );
	}
}

void nbGetPose( const nbArticulation* articulation, b3Quat* rotations, int count )
{
	// Every bone against its parent turned by its rest pose, as the target is
	rotations[0] = b3Quat_identity;
	for ( int i = 1; i < count; ++i )
	{
		const nbBone* bone = articulation->bones + i;
		b3Quat parent = b3Body_GetRotation( articulation->bones[bone->parent].bodyId );
		b3Quat rotation = b3Body_GetRotation( bone->bodyId );
		rotations[i] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( parent, bone->rest.q ), rotation ) );
	}
}

int nbArticulation_GetPose( nbArticulationId id, b3Quat* rotations, int capacity )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || rotations == NULL || capacity < 1 )
	{
		return 0;
	}

	int count = b3MinInt( capacity, articulation->boneCount );
	nbGetPose( articulation, rotations, count );
	return count;
}

void nbArticulation_PlacePose( nbArticulationId id, b3WorldTransform root, const b3Quat* rotations )
{
	nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || rotations == NULL )
	{
		return;
	}

	// The bodies where the pose puts them, at rest, and the pose as the target
	nbFreeClip( articulation );
	int count = articulation->boneCount;
	b3WorldTransform* transforms = nbAlloc( sizeof( b3WorldTransform ) * (size_t)count );
	transforms[0] = (b3WorldTransform){ root.p, b3NormalizeQuat( root.q ) };
	articulation->pose[0] = b3Quat_identity;
	for ( int i = 0; i < count; ++i )
	{
		const nbBone* bone = articulation->bones + i;
		if ( i > 0 )
		{
			articulation->pose[i] = b3NormalizeQuat( rotations[i] );
			b3Transform local = { bone->rest.p, b3MulQuat( bone->rest.q, articulation->pose[i] ) };
			transforms[i] = b3MulWorldTransforms( transforms[bone->parent], local );
		}
		b3Body_SetTransform( bone->bodyId, transforms[i].p, transforms[i].q );
		b3Body_SetLinearVelocity( bone->bodyId, b3Vec3_zero );
		b3Body_SetAngularVelocity( bone->bodyId, b3Vec3_zero );
	}
	nbFree( transforms, sizeof( b3WorldTransform ) * (size_t)count );

	if ( articulation->balancer != NULL )
	{
		nbBalancePlaced( articulation );
	}
	nbSetTargets( articulation, articulation->pose );
	b3Body_SetAwake( articulation->bones[0].bodyId, true );
}

void nbArticulation_PlayClip( nbArticulationId id, const nbClipDef* clip, float speed )
{
	nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL )
	{
		return;
	}

	nbFreeClip( articulation );
	if ( clip == NULL || clip->keyframes == NULL || clip->keyframeCount <= 0 || clip->boneCount != articulation->boneCount )
	{
		return;
	}

	int count = clip->keyframeCount;
	int boneCount = articulation->boneCount;
	articulation->clipTimes = nbAlloc( sizeof( float ) * (size_t)count );
	articulation->clipRotations = nbAlloc( sizeof( b3Quat ) * (size_t)count * (size_t)boneCount );
	for ( int k = 0; k < count; ++k )
	{
		articulation->clipTimes[k] = clip->keyframes[k].time;
		memcpy( articulation->clipRotations + k * boneCount, clip->keyframes[k].rotations, sizeof( b3Quat ) * (size_t)boneCount );
	}
	articulation->clipKeyframeCount = count;
	articulation->clipLoop = clip->loop;
	articulation->clipPlaying = true;
	articulation->clipTime = 0.0f;
	articulation->clipSpeed = speed;

	// The first pose right away, the update takes it from there. With balance the update sets the targets.
	nbSampleArticulationClip( articulation, articulation->pose );
	if ( articulation->balancer == NULL )
	{
		nbSetTargets( articulation, articulation->pose );
	}
}

float nbArticulation_GetClipTime( nbArticulationId id )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	return articulation != NULL ? articulation->clipTime : 0.0f;
}

bool nbArticulation_IsClipPlaying( nbArticulationId id )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	return articulation != NULL && articulation->clipPlaying;
}

bool nbArticulation_ApplyHit( nbArticulationId id, const nbHitDef* hit )
{
	nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || hit == NULL || hit->bone < 0 || hit->bone >= articulation->boneCount )
	{
		return false;
	}

	// Once per hit, whoever reports it. A hit without a number always lands.
	if ( hit->id != 0 )
	{
		for ( int i = 0; i < articulation->hitMemoryCount; ++i )
		{
			if ( articulation->hitIds[i] == hit->id )
			{
				return false;
			}
		}
		articulation->hitIds[articulation->hitNext] = hit->id;
		articulation->hitNext = ( articulation->hitNext + 1 ) % NB_HIT_MEMORY;
		articulation->hitMemoryCount = b3MinInt( articulation->hitMemoryCount + 1, NB_HIT_MEMORY );
	}
	articulation->hitCount += 1;

	b3Body_ApplyLinearImpulse( articulation->bones[hit->bone].bodyId, hit->impulse, hit->point, true );
	return true;
}

int nbArticulation_GetHitCount( nbArticulationId id )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	return articulation != NULL ? articulation->hitCount : 0;
}

int nbArticulation_GetBoneTransforms( nbArticulationId id, b3WorldTransform* transforms, int capacity )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || transforms == NULL )
	{
		return 0;
	}

	int count = b3MinInt( capacity, articulation->boneCount );
	for ( int i = 0; i < count; ++i )
	{
		transforms[i] = b3Body_GetTransform( articulation->bones[i].bodyId );
	}
	return count;
}

int nbArticulation_GetTargetTransforms( nbArticulationId id, b3WorldTransform* transforms, int capacity )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || transforms == NULL || capacity < 1 )
	{
		return 0;
	}

	// From the root as it is, bone by bone in the frame of its parent, turned to its target
	int count = b3MinInt( capacity, articulation->boneCount );
	transforms[0] = b3Body_GetTransform( articulation->bones[0].bodyId );
	for ( int i = 1; i < count; ++i )
	{
		const nbBone* bone = articulation->bones + i;
		b3Transform local = { bone->rest.p, b3MulQuat( bone->rest.q, bone->target ) };
		transforms[i] = b3MulWorldTransforms( transforms[bone->parent], local );
	}
	return count;
}

// The angle of a rotation, the short way round
static float nbRotationAngle( b3Quat q )
{
	float angle = b3GetQuatAngle( q );
	return angle > B3_PI ? 2.0f * B3_PI - angle : angle;
}

// How far the joint of a bone is from its target, see nbArticulation_GetJointError
static float nbJointError( const nbArticulation* articulation, int bone )
{
	const nbBone* b = articulation->bones + bone;
	if ( b->joint == nb_fixedJoint )
	{
		return 0.0f;
	}

	// The joint frames as Box3D sees them
	b3Quat parentRotation = b3Body_GetRotation( articulation->bones[b->parent].bodyId );
	b3Quat frameA = b3MulQuat( parentRotation, b3MulQuat( b->rest.q, b->jointFrame ) );
	b3Quat frameB = b3MulQuat( b3Body_GetRotation( b->bodyId ), b->jointFrame );
	b3Quat relative = b3InvMulQuat( frameA, frameB );
	b3Quat target = nbJointRotation( b, b->target );

	if ( b->joint == nb_hingeJoint )
	{
		float error = b3GetTwistAngle( relative ) - b3GetTwistAngle( target );
		if ( error > B3_PI )
		{
			error -= 2.0f * B3_PI;
		}
		else if ( error < -B3_PI )
		{
			error += 2.0f * B3_PI;
		}
		return fabsf( error );
	}

	return nbRotationAngle( b3InvMulQuat( target, relative ) );
}

float nbArticulation_GetJointError( nbArticulationId id, int bone )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || bone < 1 || bone >= articulation->boneCount )
	{
		return 0.0f;
	}
	return nbJointError( articulation, bone );
}

// How hard the motor of a bone works, see nbArticulation_GetMotorLoad
static float nbMotorLoad( const nbArticulation* articulation, int bone )
{
	const nbBone* b = articulation->bones + bone;
	float maxTorque = articulation->profile.maxTorque * b->strength;
	if ( maxTorque <= 0.0f || articulation->profile.stiffness <= 0.0f )
	{
		return 0.0f;
	}

	float torque = 0.0f;
#if NB_SPRING_TORQUE_LIMITS
	if ( b->joint == nb_hingeJoint )
	{
		torque = fabsf( b3RevoluteJoint_GetSpringTorque( b->jointId ) );
	}
	else if ( b->joint == nb_ballJoint )
	{
		torque = b3Length( b3SphericalJoint_GetSpringTorque( b->jointId ) );
	}
#endif
	return torque / maxTorque;
}

float nbArticulation_GetMotorLoad( nbArticulationId id, int bone )
{
	const nbArticulation* articulation = nbGetArticulationFromId( id, NULL );
	if ( articulation == NULL || bone < 1 || bone >= articulation->boneCount )
	{
		return 0.0f;
	}
	return nbMotorLoad( articulation, bone );
}

nbFigureStats nbWorld_GetFigureStats( nbWorldId worldId )
{
	nbFigureStats stats = { 0 };
	const nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return stats;
	}

	for ( int i = 0; i < world->articulations.count; ++i )
	{
		const nbArticulation* articulation = world->articulations.data + i;
		if ( articulation->isFree )
		{
			continue;
		}

		stats.figureCount += 1;
		stats.boneCount += articulation->boneCount;
		stats.jointCount += articulation->boneCount - 1 + ( B3_IS_NON_NULL( articulation->uprightId ) ? 1 : 0 );
		stats.playingCount += articulation->clipPlaying ? 1 : 0;

		const nbBalance* balance = nbBalanceInfoOf( articulation );
		stats.stateCounts[balance != NULL ? balance->state : nb_balanceOff] += 1;
		if ( balance != NULL )
		{
			stats.fallCount += balance->fallCount;
			stats.getUpCount += balance->getUpCount;
			stats.failedGetUpCount += balance->failedGetUpCount;
		}

		for ( int bone = 0; bone < articulation->boneCount; ++bone )
		{
			stats.awakeBoneCount += b3Body_IsAwake( articulation->bones[bone].bodyId ) ? 1 : 0;
			if ( bone > 0 && articulation->bones[bone].joint != nb_fixedJoint )
			{
				stats.saturatedJointCount += nbMotorLoad( articulation, bone ) >= NB_SATURATED_LOAD ? 1 : 0;
				stats.largestJointError = b3MaxFloat( stats.largestJointError, nbJointError( articulation, bone ) );
			}
		}
	}
	return stats;
}

nbArticulationId nbWorld_GetArticulationFromBody( nbWorldId worldId, b3BodyId bodyId, int* bone )
{
	if ( bone != NULL )
	{
		*bone = NB_NULL_INDEX;
	}

	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return nb_nullArticulationId;
	}

	int bodyIndex = bodyId.index1 - 1;
	if ( bodyIndex < 0 || bodyIndex >= world->bodyToBone.count )
	{
		return nb_nullArticulationId;
	}

	nbBoneRef ref = world->bodyToBone.data[bodyIndex];
	if ( ref.articulation == NB_NULL_INDEX )
	{
		return nb_nullArticulationId;
	}

	const nbArticulation* articulation = world->articulations.data + ref.articulation;
	if ( articulation->isFree || ref.bone >= articulation->boneCount )
	{
		return nb_nullArticulationId;
	}

	// The body at the index may be another one by now
	b3BodyId stored = articulation->bones[ref.bone].bodyId;
	if ( stored.index1 != bodyId.index1 || stored.generation != bodyId.generation )
	{
		return nb_nullArticulationId;
	}

	if ( bone != NULL )
	{
		*bone = ref.bone;
	}
	return nbMakeArticulationId( world, ref.articulation );
}
