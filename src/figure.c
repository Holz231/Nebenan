// SPDX-License-Identifier: MIT

// Figures after the hybrid of Overgrowth, see the roadmap of the game, chapters 26 to 28 and 31 to 33, and
// Source/Objects/riggedobject.cpp, Source/Graphics/skeleton.cpp and Data/Scripts/aschar.as in Overgrowth. A figure
// stands animated: its bones are kinematic bodies that the update moves with velocities toward the next pose of its
// clip, so debris bounces off them and Box3D solves none of its joints. A hard hit, a blast, a heavy body pressing on
// it or ground that falls away turn it into an active ragdoll: the bones become dynamic with the velocities they had,
// the hit acts once, spread over the bones near it, and the springs of the joints, bounded in torque, pull toward a
// pose to fall with. Like the ragdoll strength of Overgrowth the springs are as strong as the figure is fast and slack
// after a hard hit. A fast figure flails, a slow one relaxes, and one that flies at something curls up or reaches ahead
// when that is in front of it. Lying still it slows down, falls asleep, and gets up once it finds ground, room and
// nothing on it: kinematic again, with a clip from its front or its back that starts from the pose it lies in, blended
// in over the first part of the clip, as Overgrowth blends the relative bone transforms from the ragdoll to the
// animation. Everything runs in the update after a step, in the order of the figures, with operations that give the
// same bits everywhere.

#include "figure.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

// Damping ratio of the joint constraints of a figure. Measured on the ragdoll of ragdoll-1: the knees and elbows then
// overshoot their limits by 0.1 rad at most under the weight of the body, with Box3D's 2 by up to 0.37 rad.
#define NB_JOINT_DAMPING_RATIO 5.0f

// The torque limit of the joint springs is an addition to Box3D, see extern/README.md. Without it the springs of a
// ragdoll hold its pose against anything.
#if defined( B3_HAS_SPRING_TORQUE_LIMITS )
#define NB_SPRING_TORQUE_LIMITS 1
#else
#define NB_SPRING_TORQUE_LIMITS 0
#endif

// Below this strength the springs of a ragdoll are off
#define NB_MIN_STRENGTH 0.01f

// The damping of the bones of a ragdoll that lies still, in 1/s, reached after two seconds, like the damping of
// Overgrowth that rises by half per second of rest: Box3D lets it fall asleep sooner, and it lies without shivering
#define NB_REST_DAMPING 4.0f
#define NB_REST_DAMPING_RATE 0.5f

// A ragdoll lies still long enough to get up after this many seconds of rest
#define NB_REST_TO_GET_UP 0.25f

// How far ahead a flying ragdoll looks for what it flies at, in seconds of its speed, from what speed, with a sphere of
// this radius, and how fast its curl and brace follow, per second
#define NB_DANGER_TIME 0.35f
#define NB_DANGER_SPEED 1.5f
#define NB_DANGER_RADIUS 0.3f
#define NB_PROTECT_RATE 6.0f

// Seconds the get-up clip blends in from the pose the ragdoll lay in, and the stand clip after it
#define NB_GET_UP_BLEND 0.6f
#define NB_STAND_FADE 0.25f
#define NB_CLIP_FADE 0.2f

// A standing figure looks for its ground this far above and below its feet, follows it up and down at most this fast
// in meters per second, and falls if it is gone or moves faster than this
#define NB_GROUND_ABOVE 0.4f
#define NB_GROUND_BELOW 0.35f
#define NB_GROUND_FOLLOW 2.0f
#define NB_GROUND_SPEED 1.5f

// Every fourth update a standing figure checks what presses on it, figure by figure in turn
#define NB_CRUSH_PERIOD 4

// A figure that gets up falls again from debris that hits it with this part of the knockdown impulse
#define NB_ABORT_PART 0.25f

// Room to get up: a capsule of this radius from this high over the ground up to the height of the figure, and one this far
// ahead of it, where the figure kneels
#define NB_ROOM_RADIUS 0.22f
#define NB_ROOM_FLOOR 0.2f
#define NB_ROOM_AHEAD 0.45f

// The most contacts of a bone a check reads
#define NB_CONTACT_CAPACITY 16

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
	def.stiffness = 1.0f;
	def.damping = 1.0f;
	return def;
}

nbRigDef nbDefaultRigDef( void )
{
	nbRigDef def = { 0 };
	def.friction = 0.6f;
	def.selfCollision = true;
	def.enableCollisionDamage = true;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

nbFigureMotors nbDefaultFigureMotors( void )
{
	nbFigureMotors motors = { 0 };
	motors.stiffness = 600.0f;
	motors.dampingRatio = 1.0f;
	motors.maxTorque = 120.0f;
	motors.strengthPerSpeed = 0.1f;
	motors.fallStrength = 0.8f;
	motors.stunTime = 0.4f;
	return motors;
}

nbFigureDef nbDefaultFigureDef( void )
{
	nbFigureDef def = { 0 };
	def.position = b3ToPos( b3Vec3_zero );
	def.standHeight = 0.98f;
	def.motors = nbDefaultFigureMotors();
	def.knockdownImpulse = 50.0f;
	def.flinchScale = 0.008f;
	def.flinchLimit = 0.5f;
	def.flinchHertz = 2.0f;
	def.crushForce = 600.0f;
	def.restSpeed = 0.4f;
	def.getUpDelay = 1.0f;
	def.getUpSpeed = 1.4f;
	def.canGetUp = true;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
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

// A segment of the trunk: a rounded block around a center, on a ball joint at its pivot that bends about the axis across
// it with its twist, 26 degrees forward and 9 back, and leans and turns 11 degrees within its cone. Three of them round
// the back by 77 degrees and arch it by 26, a strong joint each, three times as stiff as the others, so the trunk holds
// like one joint of the motors.
static nbBoneDef nbSegment( const char* name, int parent, b3Vec3 pivot, b3Vec3 center, b3Vec3 halfExtents, float rounding,
							float mass )
{
	nbBoneDef def = nbDefaultBoneDef();
	def.name = name;
	def.parent = parent;
	def.restTransform.p = pivot;
	def.restTransform.q = b3Quat_identity;
	def.shape = nb_roundedBoxBone;
	def.center = center;
	def.halfExtents = halfExtents;
	def.radius = rounding;
	def.mass = mass;
	def.joint = nb_ballJoint;
	def.jointFrame = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
	def.lowerAngle = -0.15f;
	def.upperAngle = 0.45f;
	def.coneAngle = 0.2f;
	def.strength = 1.6f;
	def.stiffness = 3.0f;
	return def;
}

// The masses below add up to this, for a figure of 1.8 m. Hands and feet are heavier than on a person: a foot of 1 kg
// that has to stop the 75 kg landing on it stretches its ankle joint by centimeters for a few steps, a foot of 2.5 kg by
// less than half of that. The solver likes mass ratios below 30 to 1 between joined bodies.
#define NB_HUMANOID_MASS 74.0f
#define NB_HUMANOID_HEIGHT 1.8f

// The height of the pelvis origin over the soles of the humanoid of 1.8 m, see nbMakeHumanoidBones
#define NB_HUMANOID_STAND 0.98f

float nbMakeHumanoidBones( float height, float mass, nbBoneDef* bones, int capacity )
{
	if ( bones == NULL || capacity < nb_humanoidBoneCount || height <= 0.0f || mass <= 0.0f )
	{
		return 0.0f;
	}

	float s = height / NB_HUMANOID_HEIGHT;
	float m = mass / NB_HUMANOID_MASS;

	// Built after a mannequin, measured in its picture from behind: the pelvis origin stands 98 cm above the soles, the
	// shoulders 1.50 m, the head reaches from 1.60 to 1.80 m on a short neck, and the hands hang to 82 cm, a little out
	// from the hips. A limb that hangs down has its local y along world -y. The cone of a ball joint sits on the bone, the
	// hinge axis runs across it along local x. The foot points forward, along +z. The hip bends about the axis across it
	// like a hinge, with the twist of its ball joint: 132 degrees forward, so the figure can kneel, crawl and get up, 23
	// back, and its cone keeps the leg within 40 degrees of that axis when it swings out or turns. A cone along the bone
	// would allow as much back and out as forward.
	b3Quat down = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, B3_PI );
	b3Quat zToY = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -0.5f * B3_PI );
	b3Quat zToX = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
	b3Quat footRest = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.5f * B3_PI );
	b3Quat identity = b3Quat_identity;

	// The trunk in four rounded blocks like the mannequin: a round pelvis from 83 cm to 1.05 m, belly and ribs as two
	// flatter rolls above it, and a broad chest up to the shoulders at 1.56 m. Neighbours overlap, the next but one lies
	// more than 10 cm away.
	nbBoneDef pelvis = nbDefaultBoneDef();
	pelvis.name = "pelvis";
	pelvis.parent = -1;
	pelvis.shape = nb_roundedBoxBone;
	pelvis.center = (b3Vec3){ 0.0f, -0.04f * s, 0.0f };
	pelvis.halfExtents = (b3Vec3){ 0.178f * s, 0.11f * s, 0.11f * s };
	pelvis.radius = 0.08f * s;
	pelvis.mass = 11.0f * m;
	pelvis.joint = nb_fixedJoint;
	bones[nb_humanoidPelvis] = pelvis;

	bones[nb_humanoidBelly] =
		nbSegment( "belly", nb_humanoidPelvis, (b3Vec3){ 0.0f, 0.07f * s, 0.0f }, (b3Vec3){ 0.0f, 0.06f * s, 0.0f },
				   (b3Vec3){ 0.16f * s, 0.06f * s, 0.10f * s }, 0.055f * s, 5.0f * m );
	bones[nb_humanoidRibs] =
		nbSegment( "ribs", nb_humanoidBelly, (b3Vec3){ 0.0f, 0.12f * s, 0.0f }, (b3Vec3){ 0.0f, 0.06f * s, 0.0f },
				   (b3Vec3){ 0.165f * s, 0.055f * s, 0.105f * s }, 0.05f * s, 5.0f * m );
	bones[nb_humanoidChest] =
		nbSegment( "chest", nb_humanoidRibs, (b3Vec3){ 0.0f, 0.11f * s, 0.0f }, (b3Vec3){ 0.0f, 0.14f * s, 0.0f },
				   (b3Vec3){ 0.19f * s, 0.135f * s, 0.12f * s }, 0.07f * s, 12.0f * m );

	// The neck from the top of the chest, and the head on it like an egg, each bending and turning half as far as a neck
	nbBoneDef neck = nbLimb( "neck", nb_humanoidChest, (b3Vec3){ 0.0f, 0.26f * s, 0.0f }, identity, 0.08f * s, 0.05f * s,
							 1.0f * m, nb_ballJoint, zToY, -0.4f, 0.4f, 0.3f, 0.3f );
	neck.start = (b3Vec3){ 0.0f, 0.0f, 0.0f };
	neck.end = (b3Vec3){ 0.0f, 0.07f * s, 0.0f };
	nbBoneDef head = nbLimb( "head", nb_humanoidNeck, (b3Vec3){ 0.0f, 0.08f * s, 0.0f }, identity, 0.2f * s, 0.095f * s, 5.0f * m,
							 nb_ballJoint, zToY, -0.4f, 0.4f, 0.3f, 0.3f );
	head.start = (b3Vec3){ 0.0f, 0.075f * s, 0.0f };
	head.end = (b3Vec3){ 0.0f, 0.085f * s, 0.0f };

	// The neck weighs a kilogram between the chest and the head. At the stiffness of the motors its two springs turn it
	// faster than half the substeps per second and it shakes the head. At 0.3 they still hold the head.
	neck.stiffness = 0.3f;
	head.stiffness = 0.3f;
	bones[nb_humanoidNeck] = neck;
	bones[nb_humanoidHead] = head;

	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		int upperArm = side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight;
		int forearm = side == 0 ? nb_humanoidForearmLeft : nb_humanoidForearmRight;
		int hand = side == 0 ? nb_humanoidHandLeft : nb_humanoidHandRight;
		int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
		int shin = side == 0 ? nb_humanoidShinLeft : nb_humanoidShinRight;
		int foot = side == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight;

		// The arms hang 3 degrees out from the body, past the trunk by 3 cm and more, the fingertips 30 cm from the middle
		b3Quat armRest = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, B3_PI + 0.05f * x );
		bones[upperArm] = nbLimb( side == 0 ? "upper arm left" : "upper arm right", nb_humanoidChest,
								  (b3Vec3){ 0.23f * s * x, 0.22f * s, 0.0f }, armRest, 0.28f * s, 0.05f * s, 2.0f * m,
								  nb_ballJoint, zToY, -0.8f, 0.8f, 1.5f, 0.6f );
		bones[forearm] = nbLimb( side == 0 ? "forearm left" : "forearm right", upperArm, (b3Vec3){ 0.0f, 0.28f * s, 0.0f },
								 identity, 0.24f * s, 0.045f * s, 1.2f * m, nb_hingeJoint, zToX, -0.05f, 2.4f, 0.0f, 0.4f );
		bones[hand] = nbLimb( side == 0 ? "hand left" : "hand right", forearm, (b3Vec3){ 0.0f, 0.24f * s, 0.0f }, identity,
							  0.16f * s, 0.035f * s, 0.8f * m, nb_ballJoint, zToY, -0.3f, 0.3f, 0.6f, 0.15f );

		// The legs carry the body: their joints see far more than their two bones and take three times the damping
		bones[thigh] =
			nbLimb( side == 0 ? "thigh left" : "thigh right", nb_humanoidPelvis, (b3Vec3){ 0.10f * s * x, -0.03f * s, 0.0f },
					down, 0.45f * s, 0.07f * s, 7.5f * m, nb_ballJoint, zToX, -0.4f, 2.3f, 0.7f, 2.0f );
		bones[thigh].damping = 3.0f;
		bones[shin] = nbLimb( side == 0 ? "shin left" : "shin right", thigh, (b3Vec3){ 0.0f, 0.45f * s, 0.0f }, identity,
							  0.44f * s, 0.06f * s, 3.5f * m, nb_hingeJoint, zToX, -2.4f, 0.05f, 0.0f, 1.5f );
		bones[shin].damping = 3.0f;

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
		footDef.coneAngle = 0.8f;
		footDef.strength = 0.8f;
		footDef.damping = 3.0f;
		bones[foot] = footDef;
	}

	// Hip 3 cm under the pelvis origin, thigh, shin and the sole 6 cm under the ankle
	return NB_HUMANOID_STAND * s;
}

int nbBonePoints( const nbBoneDef* bone, b3Vec3* points )
{
	b3Vec3 c = bone->center;
	b3Vec3 h = bone->halfExtents;
	if ( bone->shape == nb_boxBone )
	{
		for ( int k = 0; k < 8; ++k )
		{
			points[k] = (b3Vec3){ c.x + ( k & 1 ? h.x : -h.x ), c.y + ( k & 2 ? h.y : -h.y ), c.z + ( k & 4 ? h.z : -h.z ) };
		}
		return 8;
	}
	if ( bone->shape != nb_roundedBoxBone )
	{
		return 0;
	}

	// Every corner rounded by points on its three axes and halfway between them, so each edge has two faces
	float r = b3ClampFloat( bone->radius, 0.0f, 0.95f * b3MinFloat( h.x, b3MinFloat( h.y, h.z ) ) );
	float d = 0.70710678f * r;
	int count = 0;
	for ( int k = 0; k < 8; ++k )
	{
		float sx = k & 1 ? 1.0f : -1.0f;
		float sy = k & 2 ? 1.0f : -1.0f;
		float sz = k & 4 ? 1.0f : -1.0f;
		b3Vec3 corner = { c.x + sx * ( h.x - r ), c.y + sy * ( h.y - r ), c.z + sz * ( h.z - r ) };
		const b3Vec3 offsets[6] = { { sx * r, 0.0f, 0.0f },	  { 0.0f, sy * r, 0.0f },	{ 0.0f, 0.0f, sz * r },
									{ sx * d, sy * d, 0.0f }, { sx * d, 0.0f, sz * d }, { 0.0f, sy * d, sz * d } };
		for ( int j = 0; j < 6; ++j )
		{
			points[count++] = b3Add( corner, offsets[j] );
		}
	}
	return count;
}

b3HullData* nbCreateBoneHull( const nbBoneDef* bone )
{
	b3Vec3 points[NB_BONE_POINTS];
	int count = bone != NULL ? nbBonePoints( bone, points ) : 0;
	return count > 0 ? b3CreateHull( points, count, B3_MAX_HULL_VERTICES ) : NULL;
}

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

//----------------------------------------------------------------------------------------------------------------------
// Rotations

b3Quat nbPartOf( b3Quat rotation, float weight )
{
	b3Quat q = b3NormalizeQuat( rotation );
	if ( q.s < 0.0f )
	{
		q = b3NegateQuat( q );
	}
	float angle;
	b3Vec3 axis = b3GetAxisAngle( &angle, q );
	if ( b3LengthSquared( axis ) == 0.0f )
	{
		return b3Quat_identity;
	}
	return b3MakeQuatFromAxisAngle( axis, b3ClampFloat( weight, 0.0f, 1.0f ) * angle );
}

// e to the minus x for x from 0 to a few, from its series at a quarter of x squared twice, the same bits everywhere
static float nbDecay( float x )
{
	float a = 0.25f * b3ClampFloat( x, 0.0f, 20.0f );
	float grow = 1.0f + a * ( 1.0f + a * ( 0.5f + a * ( 1.0f / 6.0f + a * ( 1.0f / 24.0f + a * ( 1.0f / 120.0f ) ) ) ) );
	grow = grow * grow;
	grow = grow * grow;
	return 1.0f / grow;
}

// A rotation from a rotation vector, its direction the axis and its length the angle
static b3Quat nbQuatFromVector( b3Vec3 vector )
{
	float angle = b3Length( vector );
	if ( angle < 1.0e-6f )
	{
		return b3Quat_identity;
	}
	return b3MakeQuatFromAxisAngle( b3MulSV( 1.0f / angle, vector ), angle );
}

// The rotation about up by an angle, up being y in the frame of the figure
static b3Quat nbHeadingQuat( float heading )
{
	return b3MakeQuatFromAxisAngle( b3Vec3_axisY, heading );
}

// The rotation of a joint as Box3D sees it: joint frame B against joint frame A. A bone rotated by q against its rest
// pose puts frame B at conj(f) q f against frame A, with f the joint frame in the frame of the bone.
static b3Quat nbJointRotation( const nbBone* bone, b3Quat rotation )
{
	return b3NormalizeQuat( b3MulQuat( b3MulQuat( b3Conjugate( bone->jointFrame ), rotation ), bone->jointFrame ) );
}

// Bring a rotation of a joint as Box3D sees it back into the limits of the joint, as Box3D measures them: the twist about
// the z axis of the joint frame, see b3GetTwistAngle, and the swing of that axis, see b3GetSwingAngle. The limits are soft,
// a spring that pulls beyond one pushes the joint through it. The halves of the angles are compared by their cosines and
// sines, without an arc tangent, from ragdoll-11. Returns false for a rotation within the limits, which stays as it is.
static bool nbLimitJointRotation( const nbBone* bone, b3Quat* relative )
{
	// The rotation as swing times twist. The twist is the direction (s, z) on the half circle at half its angle, beyond a
	// limit when it lies past the direction of half the limit. The cosine of half the swing is the length of (s, z).
	b3Quat q = relative->s < 0.0f ? b3NegateQuat( *relative ) : *relative;
	bool aboveUpper = bone->upperHalf.cosine * q.v.z - bone->upperHalf.sine * q.s > 0.0f;
	bool belowLower = bone->lowerHalf.cosine * q.v.z - bone->lowerHalf.sine * q.s < 0.0f;
	float twistSquared = q.s * q.s + q.v.z * q.v.z;
	bool beyondCone = bone->joint == nb_ballJoint && twistSquared < bone->coneHalf.cosine * bone->coneHalf.cosine;
	if ( aboveUpper == false && belowLower == false && beyondCone == false )
	{
		return false;
	}

	// A hinge turns about its axis alone, its drive ignores the swing
	float twistLength = sqrtf( twistSquared );
	b3Quat twist = twistLength > 0.0f ? (b3Quat){ { 0.0f, 0.0f, q.v.z / twistLength }, q.s / twistLength } : b3Quat_identity;
	b3Quat swing = b3MulQuat( q, b3Conjugate( twist ) );
	if ( aboveUpper || belowLower )
	{
		b3CosSin limit = aboveUpper ? bone->upperHalf : bone->lowerHalf;
		twist = (b3Quat){ { 0.0f, 0.0f, limit.sine }, limit.cosine };
	}
	float swingLength = sqrtf( swing.v.x * swing.v.x + swing.v.y * swing.v.y );
	if ( beyondCone && swingLength > 0.0f )
	{
		float scale = bone->coneHalf.sine / swingLength;
		swing = (b3Quat){ { scale * swing.v.x, scale * swing.v.y, 0.0f }, bone->coneHalf.cosine };
	}
	*relative = b3NormalizeQuat( b3MulQuat( swing, twist ) );
	return true;
}

// The rotation of a bone against its rest pose within the limits of its joint
static b3Quat nbLimitBoneRotation( const nbBone* bone, b3Quat rotation )
{
	if ( bone->joint == nb_fixedJoint )
	{
		return b3Quat_identity;
	}

	b3Quat relative = nbJointRotation( bone, rotation );
	if ( nbLimitJointRotation( bone, &relative ) == false )
	{
		return rotation;
	}
	return b3NormalizeQuat( b3MulQuat( b3MulQuat( bone->jointFrame, relative ), b3Conjugate( bone->jointFrame ) ) );
}

// Box3D sets a joint spring in cycles per second against the rotational mass of its two bodies about the axis of the
// spring: the same cycles make a weak spring on a light foot and a strong one on a heavy thigh. A stiffness in newton
// meters per radian is the same on every joint, this turns it into the cycles per second of one joint, from ragdoll-1. The
// rotational mass is taken about the softest of the axes, so a ball joint is at least this stiff about every axis.
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

//----------------------------------------------------------------------------------------------------------------------
// Clips

static void nbFreeClip( nbClip* clip )
{
	if ( clip->keyframeCount > 0 )
	{
		nbFree( clip->times, sizeof( float ) * (size_t)clip->keyframeCount );
		nbFree( clip->rotations, sizeof( b3Quat ) * (size_t)clip->keyframeCount * (size_t)clip->boneCount );
		nbFree( clip->roots, sizeof( b3Transform ) * (size_t)clip->keyframeCount );
	}
	memset( clip, 0, sizeof( nbClip ) );
}

// Copy a clip of the program for the bones of a figure. A clip that does not fit stays empty.
static void nbCopyClip( nbClip* clip, const nbClipDef* def, int boneCount )
{
	nbFreeClip( clip );
	if ( def == NULL || def->keyframes == NULL || def->keyframeCount <= 0 || def->boneCount != boneCount )
	{
		return;
	}

	int count = def->keyframeCount;
	clip->times = nbAlloc( sizeof( float ) * (size_t)count );
	clip->rotations = nbAlloc( sizeof( b3Quat ) * (size_t)count * (size_t)boneCount );
	clip->roots = nbAlloc( sizeof( b3Transform ) * (size_t)count );
	clip->keyframeCount = count;
	clip->boneCount = boneCount;
	clip->loop = def->loop;
	clip->moveRoot = def->moveRoot;
	for ( int k = 0; k < count; ++k )
	{
		const nbKeyframe* key = def->keyframes + k;
		clip->times[k] = k > 0 ? b3MaxFloat( key->time, clip->times[k - 1] ) : key->time;
		b3Transform root = key->root;
		root.q = b3NormalizeQuat( root.q );
		clip->roots[k] = root;
		for ( int i = 0; i < boneCount; ++i )
		{
			clip->rotations[k * boneCount + i] = key->rotations != NULL ? b3NormalizeQuat( key->rotations[i] ) : b3Quat_identity;
		}
	}
}

static float nbClipDuration( const nbClip* clip )
{
	return clip->keyframeCount > 0 ? clip->times[clip->keyframeCount - 1] : 0.0f;
}

// Which two keyframes a time falls between, and how far along, from 0 to 1
static int nbClipSegment( const float* times, size_t stride, int count, bool loop, float time, float* alpha )
{
#define NB_KEY_TIME( k ) ( *(const float*)( (const char*)times + (size_t)( k ) * stride ) )
	if ( count < 2 )
	{
		*alpha = 0.0f;
		return 0;
	}

	float start = NB_KEY_TIME( 0 );
	float end = NB_KEY_TIME( count - 1 );
	if ( loop && end > start )
	{
		float length = end - start;
		time = start + fmodf( time - start, length );
		if ( time < start )
		{
			time += length;
		}
	}
	time = b3ClampFloat( time, start, end );

	int segment = 0;
	while ( segment + 2 < count && time >= NB_KEY_TIME( segment + 1 ) )
	{
		segment += 1;
	}

	float from = NB_KEY_TIME( segment );
	float span = NB_KEY_TIME( segment + 1 ) - from;
	*alpha = span > 0.0f ? b3ClampFloat( ( time - from ) / span, 0.0f, 1.0f ) : 1.0f;
	return segment;
#undef NB_KEY_TIME
}

// A root between two roots
static b3Transform nbBlendRoots( b3Transform a, b3Transform b, float alpha )
{
	return (b3Transform){ b3Lerp( a.p, b.p, alpha ), b3NLerp( a.q, b.q, alpha ) };
}

// The root of a figure while a clip without root motion plays: upright at the standing height
static b3Transform nbStandRoot( const nbFigure* f )
{
	return (b3Transform){ { 0.0f, f->def.standHeight, 0.0f }, b3Quat_identity };
}

// Sample a clip of a figure into a pose and a root. A clip without keyframes is the rest pose.
static void nbSampleFigureClip( const nbFigure* f, const nbClip* clip, float time, b3Quat* pose, b3Transform* root )
{
	if ( clip->keyframeCount == 0 )
	{
		for ( int i = 0; i < f->boneCount; ++i )
		{
			pose[i] = b3Quat_identity;
		}
		*root = nbStandRoot( f );
		return;
	}

	float alpha;
	int segment = nbClipSegment( clip->times, sizeof( float ), clip->keyframeCount, clip->loop, time, &alpha );
	int next = clip->keyframeCount > 1 ? segment + 1 : segment;
	const b3Quat* from = clip->rotations + segment * clip->boneCount;
	const b3Quat* to = clip->rotations + next * clip->boneCount;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		pose[i] = b3NLerp( from[i], to[i], alpha );
	}
	*root = clip->moveRoot ? nbBlendRoots( clip->roots[segment], clip->roots[next], alpha ) : nbStandRoot( f );
}

void nbSampleClip( const nbClipDef* clip, float time, b3Quat* rotations, b3Transform* root, int capacity )
{
	if ( clip == NULL || rotations == NULL || capacity < clip->boneCount || clip->keyframeCount <= 0 )
	{
		return;
	}

	float alpha;
	int count = clip->keyframeCount;
	int segment = nbClipSegment( &clip->keyframes[0].time, sizeof( nbKeyframe ), count, clip->loop, time, &alpha );
	const nbKeyframe* from = clip->keyframes + segment;
	const nbKeyframe* to = clip->keyframes + ( count > 1 ? segment + 1 : segment );
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		b3Quat a = from->rotations != NULL ? b3NormalizeQuat( from->rotations[i] ) : b3Quat_identity;
		b3Quat b = to->rotations != NULL ? b3NormalizeQuat( to->rotations[i] ) : b3Quat_identity;
		rotations[i] = b3NLerp( a, b, alpha );
	}
	if ( root != NULL )
	{
		b3Transform a = { from->root.p, b3NormalizeQuat( from->root.q ) };
		b3Transform b = { to->root.p, b3NormalizeQuat( to->root.q ) };
		*root = nbBlendRoots( a, b, alpha );
	}
}

int nbMakeHumanoidClip( nbFigureClip which, float height, nbKeyframe* keyframes, b3Quat* rotations, int capacity,
						nbClipDef* clip )
{
	if ( keyframes == NULL || rotations == NULL || clip == NULL || (int)which < 0 || which >= nb_figureClipCount ||
		 height <= 0.0f )
	{
		return 0;
	}
	return nbBuildHumanoidClip( which, height, keyframes, rotations, capacity, clip );
}

//----------------------------------------------------------------------------------------------------------------------
// Lookup

static nbFigure* nbGetFigureFromId( nbFigureId id, nbWorld** worldOut )
{
	if ( id.index1 < 1 || id.world0 >= NB_MAX_WORLDS )
	{
		return NULL;
	}

	// The lookup that waits for steps on a thread of their own
	nbWorld* world = nbGetWorld( id.world0 );
	if ( world->inUse == false || id.index1 > world->figures.count )
	{
		return NULL;
	}

	nbFigure* f = world->figures.data + ( id.index1 - 1 );
	if ( f->isFree || f->generation != id.generation )
	{
		return NULL;
	}

	if ( worldOut != NULL )
	{
		*worldOut = world;
	}
	return f;
}

static nbFigureId nbMakeFigureId( const nbWorld* world, int index )
{
	return (nbFigureId){ index + 1, (uint16_t)world->worldIndex, world->figures.data[index].generation };
}

// Remember the bone of a body, by the index of the body in Box3D
static void nbMapBone( nbWorld* world, b3BodyId bodyId, int figure, int bone )
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
	world->bodyToBone.data[bodyIndex] = (nbBoneRef){ figure, bone };
}

static void nbUnmapBone( nbWorld* world, b3BodyId bodyId )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( 0 <= bodyIndex && bodyIndex < world->bodyToBone.count )
	{
		world->bodyToBone.data[bodyIndex] = (nbBoneRef){ NB_NULL_INDEX, NB_NULL_INDEX };
	}
}

int nbFigureOfBody( const nbWorld* world, b3BodyId bodyId, int* bone )
{
	int bodyIndex = bodyId.index1 - 1;
	if ( bodyIndex < 0 || bodyIndex >= world->bodyToBone.count )
	{
		return NB_NULL_INDEX;
	}

	nbBoneRef ref = world->bodyToBone.data[bodyIndex];
	if ( ref.figure == NB_NULL_INDEX )
	{
		return NB_NULL_INDEX;
	}

	// The body at the index may be another one by now
	b3BodyId stored = world->figures.data[ref.figure].bones[ref.bone].bodyId;
	if ( stored.index1 != bodyId.index1 || stored.generation != bodyId.generation )
	{
		return NB_NULL_INDEX;
	}

	if ( bone != NULL )
	{
		*bone = ref.bone;
	}
	return ref.figure;
}

//----------------------------------------------------------------------------------------------------------------------
// Poses

// The world transforms of the bones of a pose with the root at a world transform
static void nbComputePose( const nbFigure* f, b3WorldTransform root, const b3Quat* pose, b3WorldTransform* transforms )
{
	transforms[0] = root;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Transform local = { bone->rest.p, b3NormalizeQuat( b3MulQuat( bone->rest.q, pose[i] ) ) };
		transforms[i] = b3MulWorldTransforms( transforms[bone->parent], local );
	}
}

// The pose the bodies have: the rotation of every bone against its rest pose, and the root as it is
static void nbReadPose( const nbFigure* f, b3Quat* pose, b3WorldTransform* root )
{
	*root = b3Body_GetTransform( f->bones[0].bodyId );
	pose[0] = b3Quat_identity;
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		b3Quat parent = b3Body_GetRotation( f->bones[bone->parent].bodyId );
		b3Quat rotation = b3Body_GetRotation( bone->bodyId );
		pose[i] = b3NormalizeQuat( b3InvMulQuat( b3MulQuat( parent, bone->rest.q ), rotation ) );
	}
}

// Move the bodies of a kinematic figure toward its targets in the next step
static void nbDriveBodies( nbFigure* f, float timeStep )
{
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetTargetTransform( f->bones[i].bodyId, f->targets[i], timeStep, true );
	}
}

// Mass weighted average velocity of the bones
static b3Vec3 nbAverageVelocity( const nbFigure* f )
{
	b3Vec3 sum = b3Vec3_zero;
	float mass = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		sum = b3MulAdd( sum, f->bones[i].mass, b3Body_GetLinearVelocity( f->bones[i].bodyId ) );
		mass += f->bones[i].mass;
	}
	return mass > 0.0f ? b3MulSV( 1.0f / mass, sum ) : b3Vec3_zero;
}

// The up of the world
static b3Vec3 nbUpOf( const nbWorld* world )
{
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	return b3LengthSquared( gravity ) > 0.0f ? b3Neg( b3Normalize( gravity ) ) : b3Vec3_axisY;
}

// The frame of a figure: the point on the ground and the heading about up, where up is the y axis of the frame. Up is
// taken along the gravity of the world, turned from y the shortest way.
static b3WorldTransform nbMakeFrame( const nbWorld* world, b3Pos position, float heading )
{
	b3Vec3 up = nbUpOf( world );
	b3Quat tilt = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, up );
	return (b3WorldTransform){ position, b3NormalizeQuat( b3MulQuat( tilt, nbHeadingQuat( heading ) ) ) };
}

//----------------------------------------------------------------------------------------------------------------------
// Queries

// What a query of a figure leaves out: its own bones
typedef struct nbFigureQuery
{
	const nbWorld* world;
	int figure;
	bool skipFigures;
	bool hit;
	float fraction;
	b3Vec3 normal;
	b3Pos point;
	b3ShapeId shapeId;
} nbFigureQuery;

static bool nbSkipShape( const nbFigureQuery* query, b3ShapeId shapeId )
{
	int other = nbFigureOfBody( query->world, b3Shape_GetBody( shapeId ), NULL );
	return other != NB_NULL_INDEX && ( other == query->figure || query->skipFigures );
}

static float nbClosestHitFcn( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
							  int triangleIndex, int childIndex, void* context )
{
	NB_UNUSED( userMaterialId );
	NB_UNUSED( triangleIndex );
	NB_UNUSED( childIndex );
	nbFigureQuery* query = context;
	if ( b3Shape_IsSensor( shapeId ) || nbSkipShape( query, shapeId ) )
	{
		return -1.0f;
	}

	// The callbacks come in any order: the closest wins, and of two at the same distance the shape of the lower index
	if ( query->hit == false || fraction < query->fraction ||
		 ( fraction == query->fraction && shapeId.index1 < query->shapeId.index1 ) )
	{
		query->hit = true;
		query->fraction = fraction;
		query->normal = normal;
		query->point = point;
		query->shapeId = shapeId;
	}
	return fraction;
}

static bool nbAnyOverlapFcn( b3ShapeId shapeId, void* context )
{
	nbFigureQuery* query = context;
	if ( b3Shape_IsSensor( shapeId ) || nbSkipShape( query, shapeId ) )
	{
		return true;
	}
	query->hit = true;
	return false;
}

// The ground under a point: cast down from above it, leaving out the figure and, if asked, every other figure
static nbFigureQuery nbFindGround( const nbWorld* world, int index, b3Pos point, b3Vec3 up, float above, float below )
{
	nbFigureQuery query = { world, index, true, false, 1.0f, b3Vec3_zero, point, b3_nullShapeId };
	b3Pos origin = b3OffsetPos( point, b3MulSV( above, up ) );
	b3World_CastRay( world->physicsWorld, origin, b3MulSV( -( above + below ), up ), b3DefaultQueryFilter(), nbClosestHitFcn,
					 &query );
	return query;
}

// Does anything but the figure overlap a capsule?
static bool nbCapsuleBlocked( const nbWorld* world, int index, b3Pos origin, b3Vec3 a, b3Vec3 b, float radius )
{
	b3Vec3 points[2] = { a, b };
	b3ShapeProxy proxy = { points, 2, radius };
	nbFigureQuery query = { world, index, false, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId };
	b3World_OverlapShape( world->physicsWorld, origin, &proxy, b3DefaultQueryFilter(), nbAnyOverlapFcn, &query );
	return query.hit;
}

//----------------------------------------------------------------------------------------------------------------------
// States

// Set the springs of a ragdoll to a strength
static void nbSetStrength( nbFigure* f, float strength )
{
	bool enable = strength >= NB_MIN_STRENGTH && f->def.motors.stiffness > 0.0f && f->def.motors.maxTorque > 0.0f;
	if ( enable == false && f->springs == false )
	{
		return;
	}

	float scale = sqrtf( b3MaxFloat( strength, 0.0f ) );
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		float hertz = scale * bone->hertz;
		float dampingRatio = b3MaxFloat( 0.0f, f->def.motors.dampingRatio ) * bone->damping;
		float maxTorque = b3MaxFloat( 0.0f, strength * f->def.motors.maxTorque * bone->strength );
		NB_UNUSED( maxTorque );
		switch ( bone->joint )
		{
			case nb_hingeJoint:
				b3RevoluteJoint_EnableSpring( bone->jointId, enable );
				if ( enable )
				{
					b3RevoluteJoint_SetSpringHertz( bone->jointId, hertz );
					b3RevoluteJoint_SetSpringDampingRatio( bone->jointId, dampingRatio );
#if NB_SPRING_TORQUE_LIMITS
					b3RevoluteJoint_SetMaxSpringTorque( bone->jointId, maxTorque );
#endif
				}
				break;
			case nb_ballJoint:
				b3SphericalJoint_EnableSpring( bone->jointId, enable );
				if ( enable )
				{
					b3SphericalJoint_SetSpringHertz( bone->jointId, hertz );
					b3SphericalJoint_SetSpringDampingRatio( bone->jointId, dampingRatio );
#if NB_SPRING_TORQUE_LIMITS
					b3SphericalJoint_SetMaxSpringTorque( bone->jointId, maxTorque );
#endif
				}
				break;
			default:
				break;
		}
	}
	f->springs = enable;
}

// Drive the joints of a ragdoll to a pose, within their limits
static void nbSetJointTargets( nbFigure* f, const b3Quat* pose )
{
	for ( int i = 1; i < f->boneCount; ++i )
	{
		const nbBone* bone = f->bones + i;
		if ( bone->joint == nb_fixedJoint )
		{
			continue;
		}

		b3Quat relative = nbJointRotation( bone, pose[i] );
		nbLimitJointRotation( bone, &relative );
		if ( bone->joint == nb_hingeJoint )
		{
			b3RevoluteJoint_SetTargetAngle( bone->jointId, b3GetTwistAngle( relative ) );
		}
		else
		{
			b3SphericalJoint_SetTargetRotation( bone->jointId, relative );
		}
	}
}

// The damping of the bones of a ragdoll
static void nbSetDamping( nbFigure* f, float damping )
{
	if ( damping == f->damping )
	{
		return;
	}
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetLinearDamping( f->bones[i].bodyId, damping );
		b3Body_SetAngularDamping( f->bones[i].bodyId, damping );
	}
	f->damping = damping;
}

// The figure falls as an active ragdoll: its bones become dynamic with the velocities they have, slack for the stun
// time if a hit knocked it down
static void nbFall( nbWorld* world, nbFigure* f, bool stunned )
{
	NB_UNUSED( world );
	bool wasRagdoll = f->state == nb_figureRagdoll;
	if ( wasRagdoll == false )
	{
		for ( int i = 0; i < f->boneCount; ++i )
		{
			b3Body_SetType( f->bones[i].bodyId, b3_dynamicBody );
		}

		// The springs at strength 1, from the inertia the bones have now that they are dynamic
		for ( int i = 1; i < f->boneCount; ++i )
		{
			nbBone* bone = f->bones + i;
			float stiffness = b3MaxFloat( 0.0f, f->def.motors.stiffness ) * bone->stiffness;
			b3Quat frameB = b3MulQuat( b3Body_GetRotation( bone->bodyId ), bone->jointFrame );
			bone->hertz = bone->joint == nb_fixedJoint ? 0.0f
													   : nbSpringHertz( f->bones[bone->parent].bodyId, bone->bodyId, frameB,
																		bone->joint == nb_hingeJoint, stiffness );
		}

		if ( f->state == nb_figureGettingUp )
		{
			f->abortCount += 1;
		}
		f->knockdownCount += 1;
		f->state = nb_figureRagdoll;
		f->stateTime = 0.0f;
		f->restTime = 0.0f;
		f->fallTime = 0.0f;
		f->protect = 0.0f;
		f->brace = 0.0f;
		f->strength = 0.0f;
		f->wait = nb_waitMoving;
		f->flinch = b3Vec3_zero;
		f->flinchRate = b3Vec3_zero;
		f->springs = true;
		nbSetStrength( f, 0.0f );
		f->damping = -1.0f;
		nbSetDamping( f, 0.0f );
	}

	if ( stunned )
	{
		f->stun = b3MaxFloat( f->stun, f->def.motors.stunTime );
	}
	b3Body_SetAwake( f->bones[0].bodyId, true );
}

// A lying figure starts to get up: the pose it lies in, the frame it gets up in and the clip, and its bones kinematic
static void nbStartGetUp( nbWorld* world, int index, nbFigure* f, b3Pos ground, b3Vec3 up )
{
	NB_UNUSED( index );
	nbReadPose( f, f->lyingPose, &f->lyingRoot );

	// The heading along the spine, from the pelvis toward the head, as the clips lie with the head ahead
	b3Vec3 spine = b3RotateVector( f->lyingRoot.q, b3Vec3_axisY );
	b3Vec3 front = b3RotateVector( f->lyingRoot.q, b3Vec3_axisZ );
	b3Vec3 flat = b3MulSub( spine, b3Dot( spine, up ), up );
	if ( b3LengthSquared( flat ) < 1.0e-4f )
	{
		// Standing on its head or sitting: the front decides
		flat = b3MulSub( front, b3Dot( front, up ), up );
	}

	b3WorldTransform frame = nbMakeFrame( world, ground, 0.0f );
	b3Vec3 local = b3InvRotateVector( frame.q, flat );
	f->heading = b3Atan2( local.x, local.z );
	f->frame = nbMakeFrame( world, ground, f->heading );
	f->onBack = b3Dot( front, up ) > 0.0f;
	f->getUpClip = f->onBack ? nb_clipGetUpBack : nb_clipGetUpFront;
	f->getUpTime = 0.0f;

	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3Body_SetType( f->bones[i].bodyId, b3_kinematicBody );
	}
	nbSetDamping( f, 0.0f );
	f->springs = false;
	f->state = nb_figureGettingUp;
	f->stateTime = 0.0f;
	f->wait = nb_waitNone;
}

// Blend a pose in from another one over a time, see nbFigure::fadePose
static void nbStartFade( nbFigure* f, float duration )
{
	memcpy( f->fadePose, f->pose, sizeof( b3Quat ) * (size_t)f->boneCount );
	f->fadeRoot = f->root;
	f->fadeTime = 0.0f;
	f->fadeDuration = duration;
}

// A figure that got up stands: the frame moves to where the clip left the root, the stand clip blends in
static void nbStand( nbWorld* world, nbFigure* f )
{
	// Under the root as the get-up clip left it, turned as far as it turned about up
	b3Transform last = f->root;
	b3Vec3 ahead = b3RotateVector( last.q, b3Vec3_axisZ );
	float turn = b3Atan2( ahead.x, ahead.z );
	b3Pos ground = b3TransformWorldPoint( f->frame, (b3Vec3){ last.p.x, 0.0f, last.p.z } );
	f->heading += turn;
	f->frame = nbMakeFrame( world, ground, f->heading );
	f->root = (b3Transform){ { 0.0f, last.p.y, 0.0f }, b3NormalizeQuat( b3MulQuat( nbHeadingQuat( -turn ), last.q ) ) };

	nbStartFade( f, NB_STAND_FADE );
	f->state = nb_figureAnimated;
	f->stateTime = 0.0f;
	f->playing = false;
	f->clipTime = 0.0f;
	f->clipSpeed = 1.0f;
	f->getUpCount += 1;
}

//----------------------------------------------------------------------------------------------------------------------
// Hits

// Remember a hit id, false if the figure took it before
static bool nbTakeHitId( nbFigure* f, uint32_t id )
{
	if ( id == 0 )
	{
		return true;
	}
	for ( int k = 0; k < f->hitMemoryCount; ++k )
	{
		if ( f->hitIds[k] == id )
		{
			return false;
		}
	}
	f->hitIds[f->hitNext] = id;
	f->hitNext = ( f->hitNext + 1 ) % NB_HIT_MEMORY;
	f->hitMemoryCount = f->hitMemoryCount < NB_HIT_MEMORY ? f->hitMemoryCount + 1 : NB_HIT_MEMORY;
	return true;
}

// The trunk of a standing figure flinches from an impulse at a point: it turns about its root away from the hit
static void nbFlinch( nbFigure* f, b3Vec3 impulse, b3Pos point )
{
	b3Vec3 arm = b3SubPos( point, f->targets[0].p );
	b3Vec3 turn = b3Cross( arm, impulse );
	float length = b3Length( turn );
	if ( length < 1.0e-6f )
	{
		return;
	}

	// A critically damped spring kicked with a rate swings out the farthest after one over its angular frequency, by the
	// rate over e times that frequency: the kick that turns the trunk by the flinch scale times the impulse
	float angle = b3MinFloat( f->def.flinchScale * b3Length( impulse ), f->def.flinchLimit );
	float omega = 2.0f * B3_PI * b3MaxFloat( f->def.flinchHertz, 0.1f );
	b3Vec3 axis = b3InvRotateVector( f->frame.q, b3MulSV( 1.0f / length, turn ) );
	f->flinchRate = b3MulAdd( f->flinchRate, 2.7182818f * omega * angle, axis );
}

// A hit spread over the bones of a ragdoll, as Overgrowth's RiggedObject::ApplyForceToRagdoll spreads it: every bone takes a
// part as large as its mass over the square of its distance to the point plus 0.01 square meters, at its center of mass.
// A bone that took all of it would whip about its joints, a hard hit on the chest folds the trunk at the hips.
static void nbSpreadHit( nbFigure* f, b3Vec3 impulse, b3Pos point )
{
	float total = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3BodyId bodyId = f->bones[i].bodyId;
		float distanceSquared = b3LengthSquared( b3SubPos( point, b3Body_GetWorldCenter( bodyId ) ) );
		total += b3Body_GetMass( bodyId ) / ( distanceSquared + 0.01f );
	}
	if ( total <= 0.0f )
	{
		return;
	}

	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3BodyId bodyId = f->bones[i].bodyId;
		float distanceSquared = b3LengthSquared( b3SubPos( point, b3Body_GetWorldCenter( bodyId ) ) );
		float part = b3Body_GetMass( bodyId ) / ( distanceSquared + 0.01f ) / total;
		b3Body_ApplyLinearImpulseToCenter( bodyId, b3MulSV( part, impulse ), true );
	}
}

// A hit on a figure, from the program, a blast or debris
static void nbTakeHit( nbWorld* world, nbFigure* f, int bone, b3Vec3 impulse, b3Pos point, bool fromDebris )
{
	f->hitCount += 1;
	float magnitude = b3Length( impulse );

	// A figure that gets up breaks off from much less, see the roadmap 26.15
	float threshold = f->def.knockdownImpulse;
	if ( f->state == nb_figureGettingUp )
	{
		threshold *= NB_ABORT_PART;
	}

	if ( f->state != nb_figureRagdoll && magnitude < threshold )
	{
		if ( f->state == nb_figureAnimated )
		{
			nbFlinch( f, impulse, point );
		}
		return;
	}

	// Debris that hits a ragdoll pushed it in the step already
	if ( f->state == nb_figureRagdoll && fromDebris )
	{
		return;
	}

	NB_UNUSED( bone );
	nbFall( world, f, true );
	nbSpreadHit( f, impulse, point );
}

// The impulse a blast gives a bone, as b3World_Explode gives it: the impulse per area times the area the bone turns to the
// blast, falling off from the radius to twice the radius. The area of a capsule seen from the side.
static b3Vec3 nbBlastImpulse( const nbFigure* f, int index, const nbImpactDef* def )
{
	const nbBone* bone = f->bones + index;
	b3WorldTransform transform = b3Body_GetTransform( bone->bodyId );
	b3Vec3 center = bone->shape == nb_capsuleBone ? b3MulSV( 0.5f, b3Add( bone->start, bone->end ) ) : bone->center;
	b3Vec3 offset = b3SubPos( b3TransformWorldPoint( transform, center ), def->point );
	float distance = b3Length( offset );
	float size;
	float area;
	if ( bone->shape == nb_capsuleBone )
	{
		float length = b3Length( b3Sub( bone->end, bone->start ) );
		size = bone->radius;
		area = 2.0f * bone->radius * length + B3_PI * bone->radius * bone->radius;
	}
	else
	{
		b3Vec3 h = bone->halfExtents;
		size = b3MinFloat( h.x, b3MinFloat( h.y, h.z ) );
		area = 4.0f * b3MaxFloat( h.x * h.y, b3MaxFloat( h.y * h.z, h.x * h.z ) );
	}

	float radius = def->radius;
	float gap = b3MaxFloat( distance - size, 0.0f );
	if ( gap > 2.0f * radius || distance < 1.0e-6f )
	{
		return b3Vec3_zero;
	}
	float scale = gap > radius ? b3ClampFloat( ( 2.0f * radius - gap ) / radius, 0.0f, 1.0f ) : 1.0f;
	return b3MulSV( def->explosionImpulse * area * scale / distance, offset );
}

void nbBlastFigures( nbWorld* world, const nbImpactDef* def )
{
	if ( def->explosionImpulse <= 0.0f || world->figures.count == 0 )
	{
		return;
	}

	for ( int index = 0; index < world->figures.count; ++index )
	{
		nbFigure* f = world->figures.data + index;
		if ( f->isFree || f->state == nb_figureRagdoll )
		{
			continue;
		}

		// The blast on the whole figure and its strongest bone
		b3Vec3 total = b3Vec3_zero;
		int strongest = 0;
		float strongestImpulse = 0.0f;
		for ( int i = 0; i < f->boneCount; ++i )
		{
			b3Vec3 impulse = nbBlastImpulse( f, i, def );
			total = b3Add( total, impulse );
			float magnitude = b3Length( impulse );
			if ( magnitude > strongestImpulse )
			{
				strongestImpulse = magnitude;
				strongest = i;
			}
		}

		float magnitude = b3Length( total );
		if ( magnitude == 0.0f )
		{
			continue;
		}

		f->hitCount += 1;
		if ( magnitude < f->def.knockdownImpulse )
		{
			// The blast does not reach kinematic bones, the figure flinches from it at its strongest bone
			b3Pos point = b3Body_GetWorldCenter( f->bones[strongest].bodyId );
			nbFlinch( f, total, point );
			continue;
		}

		// Dynamic before the blast, so it throws the bones itself, once
		nbFall( world, f, true );
	}
}

// The strongest hit of debris on every standing figure and every figure that gets up in the last step, from the hit
// events of Box3D: a kinematic bone is as heavy as the world, so the debris bounced off it with about its momentum
static void nbCollectDebrisHits( nbWorld* world )
{
	for ( int index = 0; index < world->figures.count; ++index )
	{
		world->figures.data[index].debrisImpulse = 0.0f;
	}

	b3ContactEvents events = b3World_GetContactEvents( world->physicsWorld );
	for ( int i = 0; i < events.hitCount; ++i )
	{
		const b3ContactHitEvent* event = events.hitEvents + i;
		if ( b3Shape_IsValid( event->shapeIdA ) == false || b3Shape_IsValid( event->shapeIdB ) == false )
		{
			continue;
		}

		b3BodyId bodyA = b3Shape_GetBody( event->shapeIdA );
		b3BodyId bodyB = b3Shape_GetBody( event->shapeIdB );
		for ( int side = 0; side < 2; ++side )
		{
			int bone;
			int index = nbFigureOfBody( world, side == 0 ? bodyA : bodyB, &bone );
			if ( index == NB_NULL_INDEX )
			{
				continue;
			}

			nbFigure* f = world->figures.data + index;
			b3BodyId other = side == 0 ? bodyB : bodyA;
			if ( f->state == nb_figureRagdoll || b3Body_GetType( other ) != b3_dynamicBody ||
				 nbFigureOfBody( world, other, NULL ) == index )
			{
				continue;
			}

			// The normal points from A to B, the other body pushes the bone away from it
			float impulse = b3Body_GetMass( other ) * event->approachSpeed;
			if ( impulse > f->debrisImpulse )
			{
				f->debrisImpulse = impulse;
				f->debrisDirection = side == 0 ? b3Neg( event->normal ) : event->normal;
				f->debrisPoint = event->point;
				f->debrisBone = bone;
			}
		}
	}
}

// How hard dynamic bodies of others press on the bones of a figure, in newtons: the normal impulses of their contacts in
// the last step over its time. Only what pushes down on it, if asked, for debris that lies on a lying figure.
static float nbPressingForce( const nbWorld* world, int index, const nbFigure* f, float timeStep, b3Vec3 up, bool fromAbove )
{
	b3ContactData contacts[NB_CONTACT_CAPACITY];
	float force = 0.0f;
	for ( int i = 0; i < f->boneCount; ++i )
	{
		b3BodyId bodyId = f->bones[i].bodyId;
		int count = b3Body_GetContactData( bodyId, contacts, NB_CONTACT_CAPACITY );
		for ( int c = 0; c < count; ++c )
		{
			const b3ContactData* contact = contacts + c;
			b3BodyId bodyA = b3Shape_GetBody( contact->shapeIdA );
			bool boneIsA = bodyA.index1 == bodyId.index1;
			b3BodyId other = boneIsA ? b3Shape_GetBody( contact->shapeIdB ) : bodyA;
			if ( b3Body_GetType( other ) != b3_dynamicBody || nbFigureOfBody( world, other, NULL ) == index )
			{
				continue;
			}

			for ( int m = 0; m < contact->manifoldCount; ++m )
			{
				const b3Manifold* manifold = contact->manifolds + m;

				// The normal points from A to B: the push on the bone
				b3Vec3 push = boneIsA ? b3Neg( manifold->normal ) : manifold->normal;
				if ( fromAbove && b3Dot( push, up ) > -0.3f )
				{
					continue;
				}
				for ( int p = 0; p < manifold->pointCount; ++p )
				{
					force += manifold->points[p].totalNormalImpulse / timeStep;
				}
			}
		}
	}
	return force;
}

//----------------------------------------------------------------------------------------------------------------------
// Update

static void nbUpdateGettingUpPose( nbFigure* f, float advance, float timeStep );

// The pose of a standing figure from its clip, the flinch of its trunk and what it blends from
static void nbAnimate( nbWorld* world, int index, nbFigure* f, float timeStep )
{
	NB_UNUSED( world );
	NB_UNUSED( index );

	// The clip
	const nbClip* clip = f->playing ? &f->playClip : f->clips[nb_clipIdle];
	f->clipTime += f->clipSpeed * timeStep;
	if ( clip->loop == false )
	{
		f->clipTime = b3MinFloat( f->clipTime, nbClipDuration( clip ) );
	}
	nbSampleFigureClip( f, clip, f->clipTime, f->pose, &f->root );

	// The fade from the pose before
	if ( f->fadeTime < f->fadeDuration )
	{
		f->fadeTime += timeStep;
		float alpha = b3ClampFloat( f->fadeTime / f->fadeDuration, 0.0f, 1.0f );
		alpha = alpha * alpha * ( 3.0f - 2.0f * alpha );
		for ( int i = 0; i < f->boneCount; ++i )
		{
			f->pose[i] = b3NLerp( f->fadePose[i], f->pose[i], alpha );
		}
		f->root = nbBlendRoots( f->fadeRoot, f->root, alpha );
	}

	// The flinch: a critically damped spring that straightens the trunk, stepped by its exact solution, so the swing of a
	// hit is what nbFlinch gives it at any step rate. The decay over a step from a series of the same bits everywhere.
	float omega = 2.0f * B3_PI * b3MaxFloat( f->def.flinchHertz, 0.1f );
	float decay = nbDecay( omega * timeStep );
	b3Vec3 sum = b3MulAdd( f->flinchRate, omega, f->flinch );
	f->flinch = b3MulSV( decay, b3MulAdd( f->flinch, timeStep, sum ) );
	f->flinchRate = b3MulSV( decay, b3MulSub( f->flinchRate, omega * timeStep, sum ) );
	float angle = b3Length( f->flinch );
	if ( angle > f->def.flinchLimit && angle > 0.0f )
	{
		f->flinch = b3MulSV( f->def.flinchLimit / angle, f->flinch );
	}

	// The flinch turns the trunk about the root: shared by the bones of the spine of the humanoid, the root alone for
	// other rigs. The rotation is in the frame of the figure, the spine bones turn by their share in their own frames.
	if ( b3LengthSquared( f->flinch ) > 1.0e-8f )
	{
		b3Quat turn = nbQuatFromVector( f->flinch );
		if ( f->boneCount == nb_humanoidBoneCount )
		{
			b3Quat share = nbPartOf( turn, 1.0f / 3.0f );
			b3Quat frameQ = f->root.q;
			const int spine[3] = { nb_humanoidBelly, nb_humanoidRibs, nb_humanoidChest };
			for ( int k3 = 0; k3 < 3; ++k3 )
			{
				const nbBone* bone = f->bones + spine[k3];
				frameQ = b3MulQuat( frameQ, bone->rest.q );
				b3Quat local = b3NormalizeQuat( b3InvMulQuat( frameQ, b3MulQuat( share, frameQ ) ) );
				f->pose[spine[k3]] = b3NormalizeQuat( b3MulQuat( local, f->pose[spine[k3]] ) );
				frameQ = b3MulQuat( frameQ, f->pose[spine[k3]] );
			}
		}
		else
		{
			f->root.q = b3NormalizeQuat( b3MulQuat( turn, f->root.q ) );
		}
	}
}

// A standing figure: its ground, what presses on it, its pose and its bones
static void nbUpdateAnimated( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	// Debris that hit it in the step
	if ( f->debrisImpulse > 0.0f )
	{
		nbTakeHit( world, f, f->debrisBone, b3MulSV( f->debrisImpulse, f->debrisDirection ), f->debrisPoint, true );
		if ( f->state == nb_figureRagdoll )
		{
			return;
		}
	}

	// What presses on it, in turn with the other figures
	if ( ( world->figureUpdates + index ) % NB_CRUSH_PERIOD == 0 &&
		 nbPressingForce( world, index, f, timeStep, up, false ) >= f->def.crushForce )
	{
		nbFall( world, f, false );
		return;
	}

	// The ground: a figure follows it up and down a little, and falls where it is gone or gives way
	nbFigureQuery ground = nbFindGround( world, index, f->frame.p, up, NB_GROUND_ABOVE, NB_GROUND_BELOW );
	bool falls = ground.hit == false;
	if ( ground.hit )
	{
		b3BodyId body = b3Shape_GetBody( ground.shapeId );
		if ( b3Body_GetType( body ) != b3_staticBody )
		{
			b3Vec3 velocity = b3Body_GetLinearVelocity( body );
			falls = b3Length( velocity ) > NB_GROUND_SPEED;
		}
	}
	if ( falls )
	{
		nbFall( world, f, false );
		return;
	}

	float height = b3Dot( b3SubPos( ground.point, f->frame.p ), up );
	float step = NB_GROUND_FOLLOW * timeStep;
	f->frame.p = b3OffsetPos( f->frame.p, b3MulSV( b3ClampFloat( height, -step, step ), up ) );

	nbAnimate( world, index, f, timeStep );
	nbComputePose( f, b3MulWorldTransforms( f->frame, f->root ), f->pose, f->targets );
	nbDriveBodies( f, timeStep );
}

// How far a flying ragdoll protects itself and how much of that it braces: a sphere cast along its velocity finds what it
// flies at, and the nearer that is the more it protects itself, like the danger of Overgrowth's SetActiveRagdollFallPose.
// What is ahead of its front it braces against with its arms, from the rest it curls up. A figure that curled up falling
// onto its front would roll over its head.
static void nbSenseDanger( nbWorld* world, int index, nbFigure* f, b3Vec3 velocity, float timeStep )
{
	float protect = 0.0f;
	float brace = 0.0f;
	if ( f->speed > NB_DANGER_SPEED )
	{
		b3Pos origin = b3Body_GetWorldCenter( f->bones[0].bodyId );
		b3Vec3 point = b3Vec3_zero;
		b3ShapeProxy proxy = { &point, 1, NB_DANGER_RADIUS };
		nbFigureQuery query = { world, index, false, false, 1.0f, b3Vec3_zero, origin, b3_nullShapeId };
		b3World_CastShape( world->physicsWorld, origin, &proxy, b3MulSV( NB_DANGER_TIME, velocity ), b3DefaultQueryFilter(),
						   nbClosestHitFcn, &query );
		if ( query.hit )
		{
			// Into its surface, or along the velocity where the cast starts in it
			b3Vec3 hazard = b3LengthSquared( query.normal ) > 0.25f ? b3Neg( query.normal ) : b3MulSV( 1.0f / f->speed, velocity );
			b3Vec3 front = b3RotateVector( b3Body_GetRotation( f->bones[0].bodyId ), b3Vec3_axisZ );
			protect = 1.0f - query.fraction;
			brace = protect * b3MaxFloat( 0.0f, b3Dot( front, hazard ) );
		}
	}

	float rate = NB_PROTECT_RATE * timeStep;
	f->protect += b3ClampFloat( protect - f->protect, -rate, rate );
	f->brace += b3ClampFloat( brace - f->brace, -rate, rate );
}

// Mix a layer into a pose by a weight
static void nbMixPose( b3Quat* pose, const b3Quat* layer, float weight, int count )
{
	if ( weight <= 0.0f )
	{
		return;
	}
	for ( int i = 1; i < count; ++i )
	{
		pose[i] = b3NLerp( pose[i], layer[i], b3MinFloat( weight, 1.0f ) );
	}
}

// Can a lying figure get up where it lies? Ground under its pelvis, room to rise and kneel, nothing on it.
static nbFigureWait nbCheckGetUp( const nbWorld* world, int index, const nbFigure* f, b3Vec3 up, float timeStep, b3Pos* ground )
{
	b3Pos pelvis = b3Body_GetWorldCenter( f->bones[0].bodyId );
	nbFigureQuery floor = nbFindGround( world, index, pelvis, up, 0.1f, 0.6f );
	if ( floor.hit == false || b3Dot( floor.normal, up ) < 0.7f )
	{
		return nb_waitGround;
	}
	*ground = floor.point;

	if ( nbPressingForce( world, index, f, timeStep, up, true ) > 0.0f )
	{
		return nb_waitDebris;
	}

	// Standing up over the pelvis, and kneeling a little ahead along the spine
	b3Vec3 spine = b3RotateVector( b3Body_GetRotation( f->bones[0].bodyId ), b3Vec3_axisY );
	b3Vec3 flat = b3MulSub( spine, b3Dot( spine, up ), up );
	float flatLength = b3Length( flat );
	flat = flatLength > 1.0e-3f ? b3MulSV( 1.0f / flatLength, flat ) : b3Vec3_zero;
	float height = 1.75f * f->def.standHeight;
	b3Vec3 low = b3MulSV( NB_ROOM_FLOOR + NB_ROOM_RADIUS, up );
	b3Vec3 high = b3MulSV( height - NB_ROOM_RADIUS, up );
	if ( nbCapsuleBlocked( world, index, floor.point, low, high, NB_ROOM_RADIUS ) )
	{
		return nb_waitRoom;
	}
	b3Vec3 ahead = b3MulSV( NB_ROOM_AHEAD, flat );
	if ( nbCapsuleBlocked( world, index, floor.point, b3Add( low, ahead ), b3Add( b3MulSV( 0.6f * height, up ), ahead ),
						   NB_ROOM_RADIUS ) )
	{
		return nb_waitRoom;
	}
	return nb_waitNone;
}

// A ragdoll: its strength, its curl and brace, the targets of its joints, its rest and whether it gets up
static void nbUpdateRagdoll( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	f->stun = b3MaxFloat( 0.0f, f->stun - timeStep );
	bool awake = b3Body_IsAwake( f->bones[0].bodyId );
	b3Vec3 velocity = awake ? nbAverageVelocity( f ) : b3Vec3_zero;
	f->speed = b3Length( velocity );

	if ( awake )
	{
		// The strength of Overgrowth: as fast as it flies, slack after a hard hit
		float strength = b3ClampFloat( f->speed * f->def.motors.strengthPerSpeed, 0.0f, f->def.motors.fallStrength );
		f->strength = b3MaxFloat( 0.0f, strength - f->stun );

		nbSenseDanger( world, index, f, velocity, timeStep );

		// The pose to fall with, as far as the figure is strong, relaxed toward its rest pose as far as it is not: a slow one
		// mostly lies, and one that flailed on the ground would crawl over it with its own strength. Then curled up and
		// braced as far as it is.
		f->fallTime += timeStep;
		b3Quat* pose = f->pose;
		b3Quat* layer = f->fadePose;
		b3Transform root;
		nbSampleFigureClip( f, f->clips[nb_clipFall], f->fallTime, pose, &root );
		float fallStrength = f->def.motors.fallStrength;
		float flail = fallStrength > 0.0f ? b3ClampFloat( f->strength / fallStrength, 0.0f, 1.0f ) : 0.0f;
		for ( int i = 1; i < f->boneCount; ++i )
		{
			pose[i] = nbPartOf( pose[i], flail );
		}
		float curl = f->protect - f->brace;
		if ( curl > 0.0f )
		{
			nbSampleFigureClip( f, f->clips[nb_clipCurl], f->fallTime, layer, &root );
			nbMixPose( pose, layer, curl, f->boneCount );
		}
		if ( f->brace > 0.0f )
		{
			nbSampleFigureClip( f, f->clips[nb_clipBrace], f->fallTime, layer, &root );
			nbMixPose( pose, layer, f->brace, f->boneCount );
		}
		for ( int i = 1; i < f->boneCount; ++i )
		{
			pose[i] = nbLimitBoneRotation( f->bones + i, pose[i] );
		}
		nbSetJointTargets( f, pose );
		nbSetStrength( f, f->strength );

		// Lying still it slows down and falls asleep
		f->restTime = f->speed < f->def.restSpeed ? f->restTime + timeStep : 0.0f;
		nbSetDamping( f, b3MinFloat( 1.0f, NB_REST_DAMPING_RATE * f->restTime ) * NB_REST_DAMPING );
	}
	else
	{
		f->restTime += timeStep;
		f->strength = 0.0f;
	}

	// Getting up, checked every eighth update in turn with the other figures
	if ( f->def.canGetUp == false )
	{
		f->wait = nb_waitStaysDown;
		return;
	}
	if ( f->stateTime < f->def.getUpDelay || f->restTime < NB_REST_TO_GET_UP )
	{
		f->wait = nb_waitMoving;
		return;
	}
	if ( ( world->figureUpdates + index ) % 8 != 0 && f->wait != nb_waitMoving )
	{
		return;
	}

	b3Pos ground;
	f->wait = nbCheckGetUp( world, index, f, up, timeStep, &ground );
	if ( f->wait == nb_waitNone )
	{
		nbStartGetUp( world, index, f, ground, up );
		nbUpdateGettingUpPose( f, 0.0f, timeStep );
	}
}

// The pose of a figure that gets up: its get-up clip, blended in from the pose the ragdoll lay in over the first part of
// the clip, the root as well as the bones in the frames of their parents, as Overgrowth blends from the ragdoll
static void nbUpdateGettingUpPose( nbFigure* f, float advance, float timeStep )
{
	const nbClip* clip = f->clips[f->getUpClip];
	f->getUpTime += b3MaxFloat( f->def.getUpSpeed, 0.01f ) * advance;
	nbSampleFigureClip( f, clip, f->getUpTime, f->pose, &f->root );

	float blend = b3ClampFloat( f->getUpTime / NB_GET_UP_BLEND, 0.0f, 1.0f );
	blend = blend * blend * ( 3.0f - 2.0f * blend );
	b3WorldTransform clipRoot = b3MulWorldTransforms( f->frame, f->root );
	b3WorldTransform root = { b3LerpPosition( f->lyingRoot.p, clipRoot.p, blend ), b3NLerp( f->lyingRoot.q, clipRoot.q, blend ) };
	for ( int i = 1; i < f->boneCount; ++i )
	{
		f->pose[i] = b3NLerp( f->lyingPose[i], f->pose[i], blend );
	}
	nbComputePose( f, root, f->pose, f->targets );
	nbDriveBodies( f, timeStep );
}

// A figure that gets up: debris or a body on it breaks it off, else the clip plays to its end and the figure stands
static void nbUpdateGettingUp( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up )
{
	if ( f->debrisImpulse > 0.0f )
	{
		nbTakeHit( world, f, f->debrisBone, b3MulSV( f->debrisImpulse, f->debrisDirection ), f->debrisPoint, true );
		if ( f->state == nb_figureRagdoll )
		{
			return;
		}
	}

	if ( ( world->figureUpdates + index ) % NB_CRUSH_PERIOD == 0 &&
		 nbPressingForce( world, index, f, timeStep, up, false ) >= NB_ABORT_PART * f->def.crushForce )
	{
		nbFall( world, f, false );
		return;
	}

	nbUpdateGettingUpPose( f, timeStep, timeStep );
	if ( f->getUpTime >= nbClipDuration( f->clips[f->getUpClip] ) )
	{
		nbStand( world, f );
	}
}

void nbUpdateFigures( nbWorld* world, float timeStep )
{
	if ( world->figures.count == 0 || timeStep <= 0.0f )
	{
		return;
	}

	world->figureUpdates += 1;
	b3Vec3 up = nbUpOf( world );
	nbCollectDebrisHits( world );
	for ( int index = 0; index < world->figures.count; ++index )
	{
		nbFigure* f = world->figures.data + index;
		if ( f->isFree )
		{
			continue;
		}

		f->stateTime += timeStep;
		switch ( f->state )
		{
			case nb_figureAnimated:
				nbUpdateAnimated( world, index, f, timeStep, up );
				break;
			case nb_figureRagdoll:
				nbUpdateRagdoll( world, index, f, timeStep, up );
				break;
			default:
				nbUpdateGettingUp( world, index, f, timeStep, up );
				break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Create and destroy

// The clips of the library for a humanoid that stands this high, built the first time a figure asks for them
static const nbClipSet* nbGetHumanoidClips( nbWorld* world, float standHeight )
{
	for ( int i = 0; i < world->humanoidClips.count; ++i )
	{
		if ( world->humanoidClips.data[i]->height == standHeight )
		{
			return world->humanoidClips.data[i];
		}
	}

	nbClipSet* set = nbAlloc( sizeof( nbClipSet ) );
	memset( set, 0, sizeof( nbClipSet ) );
	set->height = standHeight;
	nbKeyframe* keyframes = nbAlloc( sizeof( nbKeyframe ) * NB_HUMANOID_CLIP_KEYFRAMES );
	size_t rotationSize = sizeof( b3Quat ) * NB_HUMANOID_CLIP_KEYFRAMES * nb_humanoidBoneCount;
	b3Quat* rotations = nbAlloc( rotationSize );
	float height = standHeight * NB_HUMANOID_HEIGHT / NB_HUMANOID_STAND;
	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		nbClipDef clip;
		if ( nbBuildHumanoidClip( (nbFigureClip)k, height, keyframes, rotations, NB_HUMANOID_CLIP_KEYFRAMES, &clip ) > 0 )
		{
			nbCopyClip( set->clips + k, &clip, nb_humanoidBoneCount );
		}
	}
	nbFree( rotations, rotationSize );
	nbFree( keyframes, sizeof( nbKeyframe ) * NB_HUMANOID_CLIP_KEYFRAMES );
	nbArray_Push( world->humanoidClips, set );
	return set;
}

nbFigureId nbCreateFigure( nbWorldId worldId, const nbFigureDef* def )
{
	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	nbWorld* world = nbGetWorldFromId( worldId );
	const nbRigDef* rig = def->rig;
	if ( world == NULL || rig == NULL || rig->bones == NULL || rig->boneCount <= 0 )
	{
		return nb_nullFigureId;
	}
	NB_ASSERT( rig->internalValue == NB_SECRET_COOKIE );

	// The root first, every other bone after its parent
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		int parent = rig->bones[i].parent;
		if ( i == 0 ? parent != -1 : ( parent < 0 || parent >= i ) )
		{
			return nb_nullFigureId;
		}
	}

	int index;
	if ( world->freeFigures.count > 0 )
	{
		index = world->freeFigures.data[--world->freeFigures.count];
	}
	else
	{
		nbFigure blank = { 0 };
		nbArray_Push( world->figures, blank );
		index = world->figures.count - 1;
	}

	int boneCount = rig->boneCount;
	nbFigure* f = world->figures.data + index;
	uint16_t generation = f->generation;
	memset( f, 0, sizeof( nbFigure ) );
	f->generation = generation;
	f->boneCount = boneCount;
	f->def = *def;
	f->def.rig = NULL;
	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		f->def.clips[k] = NULL;
	}
	f->bones = nbAlloc( sizeof( nbBone ) * (size_t)boneCount );
	f->pose = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	f->fadePose = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	f->lyingPose = nbAlloc( sizeof( b3Quat ) * (size_t)boneCount );
	f->targets = nbAlloc( sizeof( b3WorldTransform ) * (size_t)boneCount );
	f->collisionDamage = rig->enableCollisionDamage;

	// The clips of the program copied, the library's for the humanoid shared, a rig of other bones without a clip at rest
	const nbClipSet* library = NULL;
	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		f->clips[k] = f->ownClips + k;
		if ( def->clips[k] != NULL )
		{
			nbCopyClip( f->ownClips + k, def->clips[k], boneCount );
			continue;
		}
		if ( boneCount == nb_humanoidBoneCount )
		{
			library = library != NULL ? library : nbGetHumanoidClips( world, def->standHeight );
			f->clips[k] = library->clips + k;
		}
	}

	for ( int i = 0; i < boneCount; ++i )
	{
		const nbBoneDef* boneDef = rig->bones + i;
		nbBone* bone = f->bones + i;
		bone->bodyId = b3_nullBodyId;
		bone->jointId = b3_nullJointId;
		bone->parent = boneDef->parent;
		bone->joint = i == 0 ? nb_fixedJoint : boneDef->joint;
		bone->rest = boneDef->restTransform;
		bone->rest.q = b3NormalizeQuat( bone->rest.q );
		bone->jointFrame = b3NormalizeQuat( boneDef->jointFrame );
		bone->lowerHalf = (b3CosSin){ 1.0f, 0.0f };
		bone->upperHalf = (b3CosSin){ 1.0f, 0.0f };
		bone->coneHalf = (b3CosSin){ 1.0f, 0.0f };
		bone->strength = boneDef->strength;
		bone->stiffness = b3MaxFloat( 0.0f, boneDef->stiffness );
		bone->damping = b3MaxFloat( 0.0f, boneDef->damping );
		bone->mass = b3MaxFloat( boneDef->mass, 1.0e-3f );
		bone->shape = boneDef->shape;
		bone->center = boneDef->center;
		bone->halfExtents = boneDef->halfExtents;
		bone->start = boneDef->start;
		bone->end = boneDef->end;
		bone->radius = boneDef->radius;
	}

	// The first pose of the idle clip where the figure stands
	f->heading = def->heading;
	f->frame = nbMakeFrame( world, def->position, def->heading );
	f->clipSpeed = 1.0f;
	f->state = nb_figureAnimated;
	nbSampleFigureClip( f, f->clips[nb_clipIdle], 0.0f, f->pose, &f->root );
	nbComputePose( f, b3MulWorldTransforms( f->frame, f->root ), f->pose, f->targets );

	// Kinematic bodies in that pose. Without self collision every bone of the figure is in a collision group of its own,
	// which Box3D keeps apart, negative and unique per figure. All shapes report hits, so debris that hits a standing
	// figure shows in the events, see nbCollectDebrisHits.
	int groupIndex = rig->selfCollision ? 0 : -( index + 1 );
	for ( int i = 0; i < boneCount; ++i )
	{
		const nbBoneDef* boneDef = rig->bones + i;
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_kinematicBody;
		bodyDef.position = f->targets[i].p;
		bodyDef.rotation = f->targets[i].q;
		bodyDef.name = boneDef->name;
		bodyDef.userData = def->userData;
		b3BodyId bodyId = b3CreateBody( world->physicsWorld, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.name = boneDef->name;
		shapeDef.baseMaterial.friction = rig->friction;
		shapeDef.enableHitEvents = true;
		shapeDef.filter.groupIndex = groupIndex;
		float volume = b3MaxFloat( nbBoneVolume( boneDef ), 1.0e-6f );
		shapeDef.density = f->bones[i].mass / volume;
		if ( boneDef->shape == nb_boxBone )
		{
			b3BoxHull box = b3MakeOffsetBoxHull( boneDef->halfExtents.x, boneDef->halfExtents.y, boneDef->halfExtents.z,
												 boneDef->center );
			b3CreateHullShape( bodyId, &shapeDef, &box.base );
		}
		else if ( boneDef->shape == nb_roundedBoxBone )
		{
			b3HullData* hull = nbCreateBoneHull( boneDef );
			shapeDef.density = f->bones[i].mass / b3MaxFloat( hull->volume, 1.0e-6f );
			b3CreateHullShape( bodyId, &shapeDef, hull );
			b3DestroyHull( hull );
		}
		else
		{
			b3Capsule capsule = { boneDef->start, boneDef->end, boneDef->radius };
			b3CreateCapsuleShape( bodyId, &shapeDef, &capsule );
		}

		f->bones[i].bodyId = bodyId;
		nbMapBone( world, bodyId, index, i );
	}

	// Joints: frame A in the parent at the pivot, turned like the bone at rest, frame B at the origin of the bone. Both
	// coincide at rest, so the rest pose is the zero of every angle. Between kinematic bones Box3D keeps them aside and
	// solves nothing, until the figure falls.
	for ( int i = 1; i < boneCount; ++i )
	{
		const nbBoneDef* boneDef = rig->bones + i;
		nbBone* bone = f->bones + i;
		b3Transform frameA = { bone->rest.p, b3MulQuat( bone->rest.q, bone->jointFrame ) };
		b3Transform frameB = { b3Vec3_zero, bone->jointFrame };
		b3BodyId parentId = f->bones[bone->parent].bodyId;
		switch ( bone->joint )
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
				jointDef.lowerAngle = boneDef->lowerAngle;
				jointDef.upperAngle = boneDef->upperAngle;
				bone->jointId = b3CreateRevoluteJoint( world->physicsWorld, &jointDef );
				bone->lowerHalf = b3ComputeCosSin( 0.5f * b3RevoluteJoint_GetLowerLimit( bone->jointId ) );
				bone->upperHalf = b3ComputeCosSin( 0.5f * b3RevoluteJoint_GetUpperLimit( bone->jointId ) );
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
				jointDef.coneAngle = b3ClampFloat( boneDef->coneAngle, 0.0f, 0.5f * B3_PI );
				jointDef.enableTwistLimit = true;
				jointDef.lowerTwistAngle = boneDef->lowerAngle;
				jointDef.upperTwistAngle = boneDef->upperAngle;
				bone->jointId = b3CreateSphericalJoint( world->physicsWorld, &jointDef );
				bone->lowerHalf = b3ComputeCosSin( 0.5f * b3SphericalJoint_GetLowerTwistLimit( bone->jointId ) );
				bone->upperHalf = b3ComputeCosSin( 0.5f * b3SphericalJoint_GetUpperTwistLimit( bone->jointId ) );
				bone->coneHalf = b3ComputeCosSin( 0.5f * b3SphericalJoint_GetConeLimit( bone->jointId ) );
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

	if ( def->startAsRagdoll )
	{
		nbFall( world, f, false );
		f->knockdownCount = 0;
	}

	return nbMakeFigureId( world, index );
}

static void nbDestroyFigureAt( nbWorld* world, int index )
{
	nbFigure* f = world->figures.data + index;
	bool physicsAlive = b3World_IsValid( world->physicsWorld );

	// A body takes its shapes and joints with it
	for ( int i = 0; i < f->boneCount; ++i )
	{
		nbUnmapBone( world, f->bones[i].bodyId );
		if ( physicsAlive && b3Body_IsValid( f->bones[i].bodyId ) )
		{
			b3DestroyBody( f->bones[i].bodyId );
		}
	}

	for ( int k = 0; k < nb_figureClipCount; ++k )
	{
		nbFreeClip( f->ownClips + k );
	}
	nbFreeClip( &f->playClip );
	size_t boneCount = (size_t)f->boneCount;
	nbFree( f->bones, sizeof( nbBone ) * boneCount );
	nbFree( f->pose, sizeof( b3Quat ) * boneCount );
	nbFree( f->fadePose, sizeof( b3Quat ) * boneCount );
	nbFree( f->lyingPose, sizeof( b3Quat ) * boneCount );
	nbFree( f->targets, sizeof( b3WorldTransform ) * boneCount );
	uint16_t generation = f->generation;
	memset( f, 0, sizeof( nbFigure ) );
	f->generation = (uint16_t)( generation + 1 );
	f->isFree = true;
	nbArray_Push( world->freeFigures, index );
}

void nbDestroyFigure( nbFigureId figureId )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f == NULL )
	{
		return;
	}
	nbDestroyFigureAt( world, (int)( f - world->figures.data ) );
}

void nbDestroyFigures( nbWorld* world )
{
	for ( int i = 0; i < world->figures.count; ++i )
	{
		if ( world->figures.data[i].isFree == false )
		{
			nbDestroyFigureAt( world, i );
		}
	}
	nbArray_Free( world->figures );
	nbArray_Free( world->freeFigures );
	nbArray_Free( world->bodyToBone );

	for ( int i = 0; i < world->humanoidClips.count; ++i )
	{
		nbClipSet* set = world->humanoidClips.data[i];
		for ( int k = 0; k < nb_figureClipCount; ++k )
		{
			nbFreeClip( set->clips + k );
		}
		nbFree( set, sizeof( nbClipSet ) );
	}
	nbArray_Free( world->humanoidClips );
}

//----------------------------------------------------------------------------------------------------------------------
// Figures

bool nbFigure_IsValid( nbFigureId figureId )
{
	return nbGetFigureFromId( figureId, NULL ) != NULL;
}

int nbFigure_GetBoneCount( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->boneCount : 0;
}

b3BodyId nbFigure_GetBody( nbFigureId figureId, int bone )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || bone < 0 || bone >= f->boneCount )
	{
		return b3_nullBodyId;
	}
	return f->bones[bone].bodyId;
}

b3JointId nbFigure_GetJoint( nbFigureId figureId, int bone )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || bone < 1 || bone >= f->boneCount )
	{
		return b3_nullJointId;
	}
	return f->bones[bone].jointId;
}

nbFigureState nbFigure_GetState( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->state : nb_figureAnimated;
}

nbFigureInfo nbFigure_GetInfo( nbFigureId figureId )
{
	nbFigureInfo info = { 0 };
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL )
	{
		return info;
	}

	info.state = f->state;
	info.stateTime = f->stateTime;
	info.strength = f->state == nb_figureRagdoll ? f->strength : 0.0f;
	info.protect = f->state == nb_figureRagdoll ? f->protect : 0.0f;
	info.brace = f->state == nb_figureRagdoll ? f->brace : 0.0f;
	info.speed = f->speed;
	info.restTime = f->state == nb_figureRagdoll ? f->restTime : 0.0f;
	info.onBack = f->onBack;
	info.wait = f->state == nb_figureRagdoll ? f->wait : nb_waitNone;
	info.getUpTime = f->state == nb_figureGettingUp ? f->getUpTime : 0.0f;
	info.getUpBlend = f->state == nb_figureGettingUp ? b3ClampFloat( f->getUpTime / NB_GET_UP_BLEND, 0.0f, 1.0f ) : 0.0f;
	info.flinch = b3RotateVector( f->frame.q, f->flinch );
	info.hitCount = f->hitCount;
	info.knockdownCount = f->knockdownCount;
	info.getUpCount = f->getUpCount;
	info.abortCount = f->abortCount;
	return info;
}

void* nbFigure_GetUserData( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->def.userData : NULL;
}

b3WorldTransform nbFigure_GetFrame( nbFigureId figureId )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	return f != NULL ? f->frame : (b3WorldTransform){ b3ToPos( b3Vec3_zero ), b3Quat_identity };
}

void nbFigure_Move( nbFigureId figureId, b3Pos position, float heading )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f == NULL || f->state != nb_figureAnimated )
	{
		return;
	}

	bool far = b3Length( b3SubPos( position, f->frame.p ) ) > 1.0f;
	f->heading = heading;
	f->frame = nbMakeFrame( world, position, heading );
	if ( far )
	{
		// A jump: the bones go there at once, without the velocity of a jump
		nbComputePose( f, b3MulWorldTransforms( f->frame, f->root ), f->pose, f->targets );
		for ( int i = 0; i < f->boneCount; ++i )
		{
			b3Body_SetTransform( f->bones[i].bodyId, f->targets[i].p, f->targets[i].q );
			b3Body_SetLinearVelocity( f->bones[i].bodyId, b3Vec3_zero );
			b3Body_SetAngularVelocity( f->bones[i].bodyId, b3Vec3_zero );
		}
	}
}

void nbFigure_PlayClip( nbFigureId figureId, const nbClipDef* clip, float speed )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL )
	{
		return;
	}

	nbCopyClip( &f->playClip, clip, f->boneCount );
	f->playing = f->playClip.keyframeCount > 0;
	f->clipTime = 0.0f;
	f->clipSpeed = speed;
	if ( f->state == nb_figureAnimated )
	{
		nbStartFade( f, NB_CLIP_FADE );
	}
}

bool nbFigure_ApplyHit( nbFigureId figureId, const nbHitDef* hit )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f == NULL || hit == NULL || hit->bone < 0 || hit->bone >= f->boneCount )
	{
		return false;
	}
	if ( nbTakeHitId( f, hit->id ) == false )
	{
		return false;
	}

	if ( f->state == nb_figureRagdoll )
	{
		// A hit that meets a ragdoll acts as it is and leaves it slack a moment, as Overgrowth's limp stun
		f->hitCount += 1;
		f->stun = b3MaxFloat( f->stun, f->def.motors.stunTime );
		nbSpreadHit( f, hit->impulse, hit->point );
		return true;
	}

	nbTakeHit( world, f, hit->bone, hit->impulse, hit->point, false );
	return true;
}

void nbFigure_Knockdown( nbFigureId figureId )
{
	nbWorld* world;
	nbFigure* f = nbGetFigureFromId( figureId, &world );
	if ( f != NULL && f->state != nb_figureRagdoll )
	{
		nbFall( world, f, false );
	}
}

int nbFigure_GetBoneTransforms( nbFigureId figureId, b3WorldTransform* transforms, int capacity )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || transforms == NULL )
	{
		return 0;
	}

	int count = b3MinInt( capacity, f->boneCount );
	for ( int i = 0; i < count; ++i )
	{
		transforms[i] = b3Body_GetTransform( f->bones[i].bodyId );
	}
	return count;
}

int nbFigure_GetTargetTransforms( nbFigureId figureId, b3WorldTransform* transforms, int capacity )
{
	nbFigure* f = nbGetFigureFromId( figureId, NULL );
	if ( f == NULL || transforms == NULL )
	{
		return 0;
	}

	int count = b3MinInt( capacity, f->boneCount );
	if ( f->state == nb_figureRagdoll )
	{
		// The pose the joints pull toward, on the root as it is
		b3WorldTransform* pose = nbAlloc( sizeof( b3WorldTransform ) * (size_t)f->boneCount );
		b3Quat* limited = nbAlloc( sizeof( b3Quat ) * (size_t)f->boneCount );
		for ( int i = 0; i < f->boneCount; ++i )
		{
			limited[i] = nbLimitBoneRotation( f->bones + i, f->pose[i] );
		}
		nbComputePose( f, b3Body_GetTransform( f->bones[0].bodyId ), limited, pose );
		memcpy( transforms, pose, sizeof( b3WorldTransform ) * (size_t)count );
		nbFree( limited, sizeof( b3Quat ) * (size_t)f->boneCount );
		nbFree( pose, sizeof( b3WorldTransform ) * (size_t)f->boneCount );
		return count;
	}

	memcpy( transforms, f->targets, sizeof( b3WorldTransform ) * (size_t)count );
	return count;
}

nbFigureId nbWorld_GetFigureFromBody( nbWorldId worldId, b3BodyId bodyId, int* bone )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return nb_nullFigureId;
	}

	int index = nbFigureOfBody( world, bodyId, bone );
	return index != NB_NULL_INDEX ? nbMakeFigureId( world, index ) : nb_nullFigureId;
}

nbFigureStats nbWorld_GetFigureStats( nbWorldId worldId )
{
	nbFigureStats stats = { 0 };
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return stats;
	}

	for ( int i = 0; i < world->figures.count; ++i )
	{
		const nbFigure* f = world->figures.data + i;
		if ( f->isFree )
		{
			continue;
		}

		stats.figureCount += 1;
		stats.animatedCount += f->state == nb_figureAnimated ? 1 : 0;
		stats.ragdollCount += f->state == nb_figureRagdoll ? 1 : 0;
		stats.gettingUpCount += f->state == nb_figureGettingUp ? 1 : 0;
		stats.awakeRagdollCount += f->state == nb_figureRagdoll && b3Body_IsAwake( f->bones[0].bodyId ) ? 1 : 0;
		stats.hitCount += f->hitCount;
		stats.knockdownCount += f->knockdownCount;
		stats.getUpCount += f->getUpCount;
		stats.abortCount += f->abortCount;
	}
	return stats;
}
