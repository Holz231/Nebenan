// SPDX-License-Identifier: MIT

// The clips of the library for the humanoid of nbMakeHumanoidBones: standing, the pose to fall with and the two layers of
// Overgrowth's active ragdoll, curled up and braced, and getting up from the front and from the back. Getting up plays
// kinematic, so its poses must fit the ground: the root is placed in the frame of the figure, and the limbs reach for
// points on the ground by inverse kinematics. Between two key poses the angles alone would lift a hand off its point or
// push it into the ground, so the clip is baked into keyframes 15 times a second: the root and the angles interpolated,
// and every hand, knee or foot that carries the figure in both key poses on the same point solved for it again.

#include "figure.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <string.h>

// How a limb carries the figure: free, a leg on its foot, a leg on its knee with the shin back along the ground, or an arm
// on its hand
typedef enum nbPlantKind
{
	nb_plantFree,
	nb_plantFoot,
	nb_plantKnee,
	nb_plantHand,
} nbPlantKind;

// A limb on a point: the point, the side its middle joint bends to or the way its shin lies, the rotation of its end in the
// frame, and how far a kneeling foot points
typedef struct nbPlant
{
	nbPlantKind kind;
	b3Vec3 target;
	b3Vec3 side;
	b3Quat end;
	float pointed;
} nbPlant;

// The limbs in the order of the plants: arms, then legs, left before right
#define NB_LIMBS 4

// A key pose of a clip: its time, the root, the rotations and the limbs that carry the figure
typedef struct nbKeyPose
{
	float time;
	b3Transform root;
	b3Quat q[nb_humanoidBoneCount];
	nbPlant plants[NB_LIMBS];
} nbKeyPose;

#define NB_KEY_POSES 24

// Keyframes per second of a baked clip
#define NB_BAKE_RATE 15.0f

// A pose being built and the clip it goes into
typedef struct nbPoseMaker
{
	nbBoneDef bones[nb_humanoidBoneCount];
	float s;

	// The rotation of every bone against its rest pose, the root in the frame of the figure, the bones in the frame, and
	// the limbs that carry the figure
	b3Quat q[nb_humanoidBoneCount];
	b3Transform root;
	b3Transform world[nb_humanoidBoneCount];
	nbPlant plants[NB_LIMBS];

	nbKeyPose keys[NB_KEY_POSES];
	int keyCount;

	nbKeyframe* keyframes;
	b3Quat* rotations;
	int capacity;
	int count;
} nbPoseMaker;

static const b3Vec3 nb_x = { 1.0f, 0.0f, 0.0f };
static const b3Vec3 nb_y = { 0.0f, 1.0f, 0.0f };
static const b3Vec3 nb_z = { 0.0f, 0.0f, 1.0f };

static void nbForward( nbPoseMaker* m )
{
	m->world[0] = m->root;
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		const nbBoneDef* bone = m->bones + i;
		b3Transform local = { bone->restTransform.p, b3NormalizeQuat( b3MulQuat( bone->restTransform.q, m->q[i] ) ) };
		m->world[i] = b3MulTransforms( m->world[bone->parent], local );
	}
}

// Every bone at rest, every limb free and the root standing upright at the origin of the frame
static void nbResetPose( nbPoseMaker* m, float rootHeight )
{
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		m->q[i] = b3Quat_identity;
	}
	memset( m->plants, 0, sizeof( m->plants ) );
	m->root = (b3Transform){ { 0.0f, rootHeight, 0.0f }, b3Quat_identity };
	nbForward( m );
}

// Turn a bone about an axis of its own, after the turns it has
static void nbTurn( nbPoseMaker* m, int bone, b3Vec3 axis, float angle )
{
	m->q[bone] = b3NormalizeQuat( b3MulQuat( m->q[bone], b3MakeQuatFromAxisAngle( axis, angle ) ) );
}

// Turn a bone of each side, the left one by the angle and the right one mirrored: the same about x, the opposite about y
// and z
static void nbTurnPair( nbPoseMaker* m, int left, int right, b3Vec3 axis, float angle )
{
	nbTurn( m, left, axis, angle );
	float mirror = axis.x != 0.0f ? 1.0f : -1.0f;
	nbTurn( m, right, axis, mirror * angle );
}

// Turn the three bones of the trunk by a share each
static void nbTurnSpine( nbPoseMaker* m, b3Vec3 axis, float angle )
{
	nbTurn( m, nb_humanoidBelly, axis, angle / 3.0f );
	nbTurn( m, nb_humanoidRibs, axis, angle / 3.0f );
	nbTurn( m, nb_humanoidChest, axis, angle / 3.0f );
}

// Give a bone a rotation in the frame of the figure, for its parent as it is
static void nbSetFrameRotation( nbPoseMaker* m, int bone, b3Quat rotation )
{
	nbForward( m );
	const nbBoneDef* def = m->bones + bone;
	b3Quat parent = b3MulQuat( m->world[def->parent].q, def->restTransform.q );
	m->q[bone] = b3NormalizeQuat( b3InvMulQuat( parent, rotation ) );
	nbForward( m );
}

// The rotation whose x and y axes are these, made orthonormal around y
static b3Quat nbRotationFromAxes( b3Vec3 x, b3Vec3 y )
{
	y = b3Normalize( y );
	x = b3Normalize( b3MulSub( x, b3Dot( x, y ), y ) );
	b3Matrix3 matrix = { x, y, b3Cross( x, y ) };
	return b3NormalizeQuat( b3MakeQuatFromMatrix( &matrix ) );
}

// The part of a vector across a unit direction, of unit length, or the fallback
static b3Vec3 nbAcross( b3Vec3 v, b3Vec3 direction, b3Vec3 fallback )
{
	b3Vec3 across = b3MulSub( v, b3Dot( v, direction ), direction );
	float length = b3Length( across );
	return length > 1.0e-5f ? b3MulSV( 1.0f / length, across ) : fallback;
}

// Two bones from a pivot to a target, the middle joint toward a side: the pivot of the middle bone and of the end bone,
// the target pulled into reach, and the normal of the plane of the two bones, which is the axis of the middle joint. It
// holds when a bone points along the side, where the side alone would leave the turn about the bone open.
static b3Vec3 nbSolveTwoBones( b3Vec3 pivot, b3Vec3 target, float upper, float lower, b3Vec3 side, b3Vec3* middle,
							   b3Vec3* end )
{
	b3Vec3 offset = b3Sub( target, pivot );
	float distance = b3Length( offset );
	b3Vec3 axis = distance > 1.0e-6f ? b3MulSV( 1.0f / distance, offset ) : b3Neg( nb_y );
	distance = b3ClampFloat( distance, fabsf( upper - lower ) + 1.0e-3f, upper + lower - 1.0e-4f );
	float along = ( upper * upper - lower * lower + distance * distance ) / ( 2.0f * distance );
	float out = sqrtf( b3MaxFloat( upper * upper - along * along, 0.0f ) );
	b3Vec3 bend = nbAcross( side, axis, nbAcross( nb_y, axis, nb_x ) );
	*middle = b3Add( pivot, b3Add( b3MulSV( along, axis ), b3MulSV( out, bend ) ) );
	*end = b3MulAdd( pivot, distance, axis );
	return b3Normalize( b3Cross( axis, bend ) );
}

// A leg from its hip to an ankle point, the knee ahead toward a side, the foot at a rotation in the frame
static void nbReachLeg( nbPoseMaker* m, int side, b3Vec3 ankle, b3Vec3 kneeSide, b3Quat footRotation )
{
	int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
	int shin = thigh + 1;
	int foot = thigh + 2;
	nbForward( m );
	b3Vec3 hip = m->world[thigh].p;
	float upper = b3Length( m->bones[shin].restTransform.p );
	float lower = b3Length( m->bones[foot].restTransform.p );
	b3Vec3 knee, end;
	b3Vec3 thighX = nbSolveTwoBones( hip, ankle, upper, lower, kneeSide, &knee, &end );

	// The thigh along the bone with the knee axis across the plane of the leg, the knee ahead, and the shin about the
	// same axis
	b3Vec3 thighY = b3Normalize( b3Sub( knee, hip ) );
	nbSetFrameRotation( m, thigh, nbRotationFromAxes( thighX, thighY ) );
	nbSetFrameRotation( m, shin, nbRotationFromAxes( thighX, b3Sub( end, knee ) ) );
	nbSetFrameRotation( m, foot, footRotation );
	m->plants[2 + side] = (nbPlant){ nb_plantFoot, ankle, kneeSide, footRotation, 0.0f };
}

// A leg kneeling: the thigh from the hip toward a knee point, the knee at the height of the point as far as the thigh
// reaches, the shin back along the ground, the foot pointed at an angle
static void nbKneel( nbPoseMaker* m, int side, b3Vec3 knee, b3Vec3 back, float pointed )
{
	int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
	int shin = thigh + 1;
	int foot = thigh + 2;
	nbForward( m );
	b3Vec3 hip = m->world[thigh].p;
	float length = b3Length( m->bones[shin].restTransform.p );
	float drop = hip.y - knee.y;
	b3Vec3 flat = { knee.x - hip.x, 0.0f, knee.z - hip.z };
	float flatLength = b3Length( flat );
	if ( drop >= length )
	{
		knee = b3MulAdd( hip, length, b3Neg( nb_y ) );
	}
	else
	{
		float reach = sqrtf( length * length - drop * drop );
		b3Vec3 toward = flatLength > 1.0e-4f ? b3MulSV( 1.0f / flatLength, flat ) : back;
		knee = (b3Vec3){ hip.x + reach * toward.x, knee.y, hip.z + reach * toward.z };
	}
	b3Vec3 thighY = b3Normalize( b3Sub( knee, hip ) );
	b3Vec3 thighX = b3Cross( back, thighY );
	thighX = b3LengthSquared( thighX ) > 1.0e-6f ? b3Normalize( thighX ) : b3Neg( nb_x );
	nbSetFrameRotation( m, thigh, nbRotationFromAxes( thighX, thighY ) );
	nbSetFrameRotation( m, shin, nbRotationFromAxes( thighX, back ) );
	m->q[foot] = b3MakeQuatFromAxisAngle( nb_x, -pointed );
	nbForward( m );
	m->plants[2 + side] = (nbPlant){ nb_plantKnee, knee, back, b3Quat_identity, pointed };
}

// An arm from its shoulder to a wrist point, the elbow toward a side, the hand at a rotation in the frame
static void nbReachArm( nbPoseMaker* m, int side, b3Vec3 wrist, b3Vec3 elbowSide, b3Quat handRotation )
{
	int upperArm = side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight;
	int forearm = upperArm + 1;
	int hand = upperArm + 2;
	nbForward( m );
	b3Vec3 shoulder = m->world[upperArm].p;
	float upper = b3Length( m->bones[forearm].restTransform.p );
	float lower = b3Length( m->bones[hand].restTransform.p );
	b3Vec3 elbow, end;
	b3Vec3 normal = nbSolveTwoBones( shoulder, wrist, upper, lower, elbowSide, &elbow, &end );

	// The forearm bends toward the front of the upper arm, so the front looks away from the elbow and the elbow axis is
	// the other way across the plane of the arm
	b3Vec3 upperY = b3Normalize( b3Sub( elbow, shoulder ) );
	b3Vec3 upperX = b3Neg( normal );
	nbSetFrameRotation( m, upperArm, nbRotationFromAxes( upperX, upperY ) );
	nbSetFrameRotation( m, forearm, nbRotationFromAxes( upperX, b3Sub( end, elbow ) ) );
	nbSetFrameRotation( m, hand, handRotation );
	m->plants[side] = (nbPlant){ nb_plantHand, wrist, elbowSide, handRotation, 0.0f };
}

// The lowest point of the pose in the frame
static float nbLowest( nbPoseMaker* m )
{
	nbForward( m );
	float lowest = FLT_MAX;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		const nbBoneDef* bone = m->bones + i;
		b3Vec3 points[NB_BONE_POINTS];
		int count = nbBonePoints( bone, points );
		float radius = 0.0f;
		if ( count == 0 )
		{
			points[0] = bone->start;
			points[1] = bone->end;
			count = 2;
			radius = bone->radius;
		}
		for ( int k = 0; k < count; ++k )
		{
			lowest = b3MinFloat( lowest, b3TransformPoint( m->world[i], points[k] ).y - radius );
		}
	}
	return lowest;
}

// Set the root down so the lowest point of the pose touches the ground
static void nbSetDown( nbPoseMaker* m )
{
	m->root.p.y -= nbLowest( m );
	nbForward( m );
}

// Keep the pose as a key pose of the clip
static void nbAddPose( nbPoseMaker* m, float time )
{
	if ( m->keyCount < NB_KEY_POSES )
	{
		nbKeyPose* key = m->keys + m->keyCount;
		key->time = time;
		key->root = m->root;
		memcpy( key->q, m->q, sizeof( m->q ) );
		memcpy( key->plants, m->plants, sizeof( m->plants ) );
	}
	m->keyCount += 1;
}

// Write the pose as a keyframe of the clip
static void nbEmitPose( nbPoseMaker* m, float time )
{
	int k = m->count++;
	if ( k >= m->capacity )
	{
		return;
	}

	b3Quat* rotations = m->rotations + k * nb_humanoidBoneCount;
	memcpy( rotations, m->q, sizeof( m->q ) );
	rotations[0] = b3Quat_identity;
	m->keyframes[k] = (nbKeyframe){ time, rotations, m->root };
}

// Does a limb carry the figure in two key poses the same way? A knee or a hand that is on another point in the second slides
// along the ground to it, a foot stays on its point.
static bool nbSamePlant( const nbPlant* a, const nbPlant* b )
{
	if ( a->kind == nb_plantFree || a->kind != b->kind )
	{
		return false;
	}
	return a->kind != nb_plantFoot || b3LengthSquared( b3Sub( a->target, b->target ) ) < 1.0e-8f;
}

// A plant on the way from one key pose to the next
static nbPlant nbBlendPlants( const nbPlant* a, const nbPlant* b, float alpha )
{
	nbPlant plant = *a;
	plant.target = b3Lerp( a->target, b->target, alpha );
	plant.side = b3Normalize( b3Lerp( a->side, b->side, alpha ) );
	plant.end = b3NLerp( a->end, b->end, alpha );
	plant.pointed = a->pointed + alpha * ( b->pointed - a->pointed );
	return plant;
}

// Put a limb on its point again, for the pose as it is
static void nbReplant( nbPoseMaker* m, int limb, const nbPlant* plant )
{
	switch ( plant->kind )
	{
		case nb_plantHand:
			nbReachArm( m, limb, plant->target, plant->side, plant->end );
			break;
		case nb_plantFoot:
			nbReachLeg( m, limb - 2, plant->target, plant->side, plant->end );
			break;
		case nb_plantKnee:
			nbKneel( m, limb - 2, plant->target, plant->side, plant->pointed );
			break;
		default:
			break;
	}
}

// The lowest point of the bones of a limb in the frame, from its root bone to its end
static b3Vec3 nbLowestOfLimb( nbPoseMaker* m, int first )
{
	nbForward( m );
	b3Vec3 lowest = { 0.0f, FLT_MAX, 0.0f };
	for ( int i = first; i < first + 3; ++i )
	{
		const nbBoneDef* bone = m->bones + i;
		b3Vec3 points[NB_BONE_POINTS];
		int count = nbBonePoints( bone, points );
		float radius = 0.0f;
		if ( count == 0 )
		{
			points[0] = bone->start;
			points[1] = bone->end;
			count = 2;
			radius = bone->radius;
		}
		for ( int k = 0; k < count; ++k )
		{
			b3Vec3 p = b3TransformPoint( m->world[i], points[k] );
			p.y -= radius;
			if ( p.y < lowest.y )
			{
				lowest = p;
			}
		}
	}
	return lowest;
}

// The arms that carry nothing stay above the ground: an arm whose lowest point sinks in turns about its shoulder, the way
// that lifts that point, as far as it sank. The hand hangs far from the shoulder, so a small turn lifts it. A leg that
// swings hangs its foot under the hip, where a turn at the hip lifts it hardly at all and far too fast from one keyframe
// to the next, its key poses keep it clear instead.
static void nbLiftLimbs( nbPoseMaker* m )
{
	const int firsts[2] = { nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight };
	for ( int limb = 0; limb < 2; ++limb )
	{
		if ( m->plants[limb].kind != nb_plantFree )
		{
			continue;
		}

		int first = firsts[limb];
		for ( int iteration = 0; iteration < 4; ++iteration )
		{
			b3Vec3 lowest = nbLowestOfLimb( m, first );
			if ( lowest.y >= 0.0f )
			{
				break;
			}

			// Turning about the axis across the arm of the point and up raises the point at the rate of its distance
			// across up
			b3Vec3 arm = b3Sub( lowest, m->world[first].p );
			b3Vec3 axis = b3Cross( arm, nb_y );
			float rate = b3Length( axis );
			if ( rate < 1.0e-4f )
			{
				break;
			}
			float angle = b3MinFloat( 1.2f * ( -lowest.y + 0.005f * m->s ) / rate, 0.5f );
			b3Quat turn = b3MakeQuatFromAxisAngle( b3MulSV( 1.0f / rate, axis ), angle );
			nbSetFrameRotation( m, first, b3NormalizeQuat( b3MulQuat( turn, m->world[first].q ) ) );
		}
	}
}

// Bake the key poses into keyframes: at the rate between two key poses, or only the key poses for a rate of zero. Baked
// keyframes keep the free limbs above the ground.
static void nbBake( nbPoseMaker* m, float rate )
{
	int keyCount = m->keyCount < NB_KEY_POSES ? m->keyCount : NB_KEY_POSES;
	for ( int k = 0; k < keyCount; ++k )
	{
		const nbKeyPose* a = m->keys + k;
		m->root = a->root;
		memcpy( m->q, a->q, sizeof( m->q ) );
		memcpy( m->plants, a->plants, sizeof( m->plants ) );
		if ( rate > 0.0f )
		{
			nbLiftLimbs( m );
		}
		nbEmitPose( m, a->time );
		if ( k + 1 == keyCount || rate <= 0.0f )
		{
			continue;
		}

		const nbKeyPose* b = m->keys + k + 1;
		int samples = (int)ceilf( ( b->time - a->time ) * rate - 1.0e-3f );
		for ( int j = 1; j < samples; ++j )
		{
			float alpha = (float)j / (float)samples;
			m->root.p = b3Lerp( a->root.p, b->root.p, alpha );
			m->root.q = b3NLerp( a->root.q, b->root.q, alpha );
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				m->q[i] = b3NLerp( a->q[i], b->q[i], alpha );
			}
			for ( int limb = 0; limb < NB_LIMBS; ++limb )
			{
				if ( nbSamePlant( a->plants + limb, b->plants + limb ) )
				{
					nbPlant plant = nbBlendPlants( a->plants + limb, b->plants + limb, alpha );
					nbReplant( m, limb, &plant );
				}
				else
				{
					m->plants[limb].kind = nb_plantFree;
				}
			}
			nbLiftLimbs( m );
			nbEmitPose( m, a->time + alpha * ( b->time - a->time ) );
		}
	}
}

// The rotation of a foot that stands flat, facing ahead, and of a hand that lies flat on the ground pointing ahead
static b3Quat nbFlatFoot( void )
{
	return b3MulQuat( b3MakeQuatFromAxisAngle( nb_z, B3_PI ), b3MakeQuatFromAxisAngle( nb_x, 0.5f * B3_PI ) );
}

static b3Quat nbFlatHand( float side )
{
	// The hand along +z, its palm down: its y axis ahead, its x axis across as the arm turns it
	return nbRotationFromAxes( (b3Vec3){ -side, 0.0f, 0.0f }, nb_z );
}

//----------------------------------------------------------------------------------------------------------------------
// Standing and falling

static void nbMakeIdle( nbPoseMaker* m, float standHeight )
{
	// Soft knees, the arms hanging a little bent, and a breath that lifts the chest and the arms
	for ( int k = 0; k < 3; ++k )
	{
		nbResetPose( m, standHeight );
		float breath = k == 1 ? 1.0f : 0.0f;
		nbTurnPair( m, nb_humanoidThighLeft, nb_humanoidThighRight, nb_x, 0.06f );
		nbTurnPair( m, nb_humanoidShinLeft, nb_humanoidShinRight, nb_x, -0.1f );
		nbTurnPair( m, nb_humanoidFootLeft, nb_humanoidFootRight, nb_x, 0.04f );
		nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.06f );
		nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_z, 0.02f * breath );
		nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.15f + 0.04f * breath );
		nbTurn( m, nb_humanoidBelly, nb_x, 0.02f );
		nbTurn( m, nb_humanoidChest, nb_x, -0.01f - 0.03f * breath );
		nbTurn( m, nb_humanoidHead, nb_x, 0.03f + 0.02f * breath );
		nbAddPose( m, 2.0f * (float)k );
	}
}

static void nbMakeFall( nbPoseMaker* m, float standHeight )
{
	// The arms up and out, flailing in turn, the knees bent, the trunk a little rounded, the head up
	for ( int k = 0; k < 3; ++k )
	{
		nbResetPose( m, standHeight );
		float swing = k == 1 ? 1.0f : -1.0f;
		nbTurn( m, nb_humanoidUpperArmLeft, nb_x, 0.9f + 0.3f * swing );
		nbTurn( m, nb_humanoidUpperArmLeft, nb_z, 0.7f - 0.2f * swing );
		nbTurn( m, nb_humanoidUpperArmRight, nb_x, 0.9f - 0.3f * swing );
		nbTurn( m, nb_humanoidUpperArmRight, nb_z, -0.7f - 0.2f * swing );
		nbTurn( m, nb_humanoidForearmLeft, nb_x, 0.8f + 0.2f * swing );
		nbTurn( m, nb_humanoidForearmRight, nb_x, 0.8f - 0.2f * swing );
		nbTurn( m, nb_humanoidThighLeft, nb_x, 0.5f + 0.2f * swing );
		nbTurn( m, nb_humanoidThighRight, nb_x, 0.5f - 0.2f * swing );
		nbTurnPair( m, nb_humanoidShinLeft, nb_humanoidShinRight, nb_x, -0.8f );
		nbTurnSpine( m, nb_x, 0.25f );
		nbTurn( m, nb_humanoidHead, nb_x, -0.15f );
		nbAddPose( m, 0.4f * (float)k );
	}
}

static void nbMakeCurl( nbPoseMaker* m, float standHeight )
{
	// Knees to the chest, the back round, the chin down and the forearms in front of the face
	nbResetPose( m, standHeight );
	nbTurnPair( m, nb_humanoidThighLeft, nb_humanoidThighRight, nb_x, 1.7f );
	nbTurnPair( m, nb_humanoidShinLeft, nb_humanoidShinRight, nb_x, -2.1f );
	nbTurnPair( m, nb_humanoidFootLeft, nb_humanoidFootRight, nb_x, -0.3f );
	nbTurnSpine( m, nb_x, 1.2f );
	nbTurn( m, nb_humanoidNeck, nb_x, 0.2f );
	nbTurn( m, nb_humanoidHead, nb_x, 0.25f );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 1.3f );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_z, -0.35f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 2.2f );
	nbAddPose( m, 0.0f );
}

static void nbMakeBrace( nbPoseMaker* m, float standHeight )
{
	// The arms out ahead and a little apart, the elbows soft, the hands bent back to take the fall, the head up, the back
	// and the legs straight. A hollow back would lift the legs over the head once the chest lies on the ground.
	nbResetPose( m, standHeight );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 1.35f );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_z, 0.2f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.25f );
	nbTurnPair( m, nb_humanoidHandLeft, nb_humanoidHandRight, nb_x, -0.4f );
	nbTurn( m, nb_humanoidHead, nb_x, -0.2f );
	nbTurnPair( m, nb_humanoidThighLeft, nb_humanoidThighRight, nb_x, 0.1f );
	nbTurnPair( m, nb_humanoidShinLeft, nb_humanoidShinRight, nb_x, -0.15f );
	nbAddPose( m, 0.0f );
}

//----------------------------------------------------------------------------------------------------------------------
// Getting up

// The pelvis turned from upright toward the front by a tilt, a quarter turn lying on the front with the head ahead, and
// rolled about the length of the body by a roll, half a turn lying on the back
static b3Quat nbLying( float tilt, float roll )
{
	return b3NormalizeQuat( b3MulQuat( b3MakeQuatFromAxisAngle( nb_z, roll ), b3MakeQuatFromAxisAngle( nb_x, tilt ) ) );
}

// The pelvis placed so its two hips meet a point, for a pelvis at this rotation
static void nbPlaceHips( nbPoseMaker* m, b3Vec3 hips, b3Quat rotation )
{
	b3Vec3 offset = b3MulSV( 0.5f, b3Add( m->bones[nb_humanoidThighLeft].restTransform.p,
										  m->bones[nb_humanoidThighRight].restTransform.p ) );
	m->root = (b3Transform){ b3Sub( hips, b3RotateVector( rotation, offset ) ), rotation };
	nbForward( m );
}

// From lying on the front with the head ahead, along +z: the hands come beside the shoulders, the arms push the chest up
// while the knees come under the hips, onto hands and knees, kneeling up, the left foot forward in the air and down ahead,
// up over it with the right foot behind, and the right foot beside the left. Every point that carries the figure stays
// where it is while it carries: the hands from placing to kneeling up, the knees from hands and knees to rising, the left
// foot from setting it down to the end. The lying start has the arms along the body and the legs straight, the figure
// blends into it from the pose it lies in. Shifted by an offset on the ground and in time, for the clip from the back.
static void nbMakeGetUpFront( nbPoseMaker* m, b3Vec3 offset, float start, float standHeight )
{
	float s = m->s;
	float pi = B3_PI;
	float kneeRadius = m->bones[nb_humanoidShinLeft].radius;
	float thigh = b3Length( m->bones[nb_humanoidShinLeft].restTransform.p );
	b3Vec3 ahead = nb_z;
	b3Vec3 back = b3Normalize( (b3Vec3){ 0.0f, 0.3f, -1.0f } );

	// Lying flat, the knees a little bent and the feet pointed, so the toes stay off the ground
	nbResetPose( m, 0.0f );
	m->root.p = offset;
	m->root.q = nbLying( 0.5f * pi, 0.0f );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_z, 0.25f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.3f );
	nbTurnPair( m, nb_humanoidShinLeft, nb_humanoidShinRight, nb_x, -0.5f );
	nbTurnPair( m, nb_humanoidFootLeft, nb_humanoidFootRight, nb_x, -0.7f );
	nbSetDown( m );
	float lyingHeight = m->root.p.y;
	nbAddPose( m, start );

	// The hands where they push from: beside the shoulders, a little ahead, wrists low
	b3Vec3 hands[2];
	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		b3Vec3 shoulder = m->world[side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight].p;
		hands[side] = (b3Vec3){ shoulder.x + 0.1f * s * x, 0.05f * s, shoulder.z + 0.08f * s };
	}

	// The hands placed, the head up, the knees a little bent
	nbResetPose( m, 0.0f );
	m->root = (b3Transform){ { offset.x, lyingHeight, offset.z }, nbLying( 0.5f * pi, 0.0f ) };
	nbTurn( m, nb_humanoidChest, nb_x, -0.1f );
	nbTurn( m, nb_humanoidHead, nb_x, -0.3f );
	nbTurnPair( m, nb_humanoidShinLeft, nb_humanoidShinRight, nb_x, -0.6f );
	nbTurnPair( m, nb_humanoidFootLeft, nb_humanoidFootRight, nb_x, -0.7f );
	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		nbReachArm( m, side, hands[side], (b3Vec3){ x, 1.0f, -0.3f }, nbFlatHand( x ) );
	}
	nbAddPose( m, start + 0.35f );

	// The chest pushed up, the hips rising, the knees drawn up behind them
	b3Vec3 hips = { offset.x, 0.25f * s, offset.z - 0.05f * s };
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.42f * pi, 0.0f ) );
	nbTurnSpine( m, nb_x, -0.2f );
	nbTurn( m, nb_humanoidHead, nb_x, -0.3f );
	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		b3Vec3 hip = m->world[side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight].p;
		float drop = hip.y - kneeRadius;
		float reach = sqrtf( b3MaxFloat( thigh * thigh - drop * drop, 0.0f ) );
		nbKneel( m, side, (b3Vec3){ hip.x, kneeRadius, hip.z - reach }, back, 0.75f );
		nbReachArm( m, side, hands[side], (b3Vec3){ x, 0.5f, -1.0f }, nbFlatHand( x ) );
	}
	nbAddPose( m, start + 0.75f );

	// On hands and knees: the hips over the knees, the back nearly level
	float kneeZ = hands[0].z - 0.5f * s;
	hips = (b3Vec3){ offset.x, kneeRadius + thigh, kneeZ };
	b3Vec3 knees[2];
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.46f * pi, 0.0f ) );
	nbTurn( m, nb_humanoidHead, nb_x, -0.35f );
	for ( int side = 0; side < 2; ++side )
	{
		float x = side == 0 ? 1.0f : -1.0f;
		b3Vec3 hip = m->world[side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight].p;
		knees[side] = (b3Vec3){ hip.x, kneeRadius, hip.z };
		nbKneel( m, side, knees[side], back, 0.75f );
		nbReachArm( m, side, hands[side], (b3Vec3){ x, 0.3f, -1.0f }, nbFlatHand( x ) );
	}
	nbAddPose( m, start + 1.1f );

	// Kneeling up, the knees where they are, the arms hanging
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.08f, 0.0f ) );
	for ( int side = 0; side < 2; ++side )
	{
		nbKneel( m, side, knees[side], back, 0.75f );
	}
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.3f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.5f );
	nbAddPose( m, start + 1.55f );

	// The weight over the right knee, the left knee lifts off as it kneels, the shin and foot as they lay
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.06f, 0.0f ) );
	nbTurnSpine( m, nb_z, 0.12f );
	nbKneel( m, 1, knees[1], back, 0.75f );
	nbKneel( m, 0, b3Add( knees[0], (b3Vec3){ 0.0f, 0.12f * s, 0.06f * s } ), back, 0.75f );
	m->plants[2].kind = nb_plantFree;
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.35f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.55f );
	nbAddPose( m, start + 1.75f );

	// The left knee comes up ahead, the shin folded back under it and rising, the foot high behind
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.05f, 0.0f ) );
	nbTurnSpine( m, nb_z, 0.12f );
	nbKneel( m, 1, knees[1], back, 0.75f );
	nbForward( m );
	b3Vec3 raised = b3Add( m->world[nb_humanoidThighLeft].p, (b3Vec3){ 0.0f, -0.12f * s, 0.43f * s } );
	nbKneel( m, 0, raised, b3Normalize( (b3Vec3){ 0.0f, 0.15f, -1.0f } ), 0.8f );
	m->plants[2].kind = nb_plantFree;
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.4f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.6f );
	nbAddPose( m, start + 2.0f );

	// The left foot over the point ahead where it goes down, flat
	nbForward( m );
	b3Vec3 leftHip = m->world[nb_humanoidThighLeft].p;
	float footHeight = 0.06f * s;
	b3Vec3 leftFoot = { leftHip.x, footHeight, leftHip.z + 0.42f * s };
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.05f, 0.0f ) );
	nbTurnSpine( m, nb_z, 0.06f );
	nbKneel( m, 1, knees[1], back, 0.75f );
	nbReachLeg( m, 0, b3Add( leftFoot, (b3Vec3){ 0.0f, 0.15f * s, -0.06f * s } ), ahead, nbFlatFoot() );
	m->plants[2].kind = nb_plantFree;
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.4f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.6f );
	nbAddPose( m, start + 2.2f );

	// The left foot down ahead, flat, the shin upright
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.05f, 0.0f ) );
	nbKneel( m, 1, knees[1], back, 0.75f );
	nbReachLeg( m, 0, leftFoot, ahead, nbFlatFoot() );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.4f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.6f );
	nbAddPose( m, start + 2.35f );

	// Leaning over the left foot, the right knee lifts off as it kneels
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, (b3Vec3){ hips.x, hips.y + 0.08f * s, hips.z + 0.08f * s }, nbLying( 0.2f, 0.0f ) );
	nbReachLeg( m, 0, leftFoot, ahead, nbFlatFoot() );
	nbForward( m );
	nbKneel( m, 1, b3Add( knees[1], (b3Vec3){ 0.0f, 0.16f * s, 0.06f * s } ), back, 0.75f );
	m->plants[3].kind = nb_plantFree;
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.55f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.65f );
	nbAddPose( m, start + 2.6f );

	// Up over the left foot, leaning into it, the right knee off the ground, its toes behind
	b3Vec3 rightToes = { knees[1].x, 0.16f * s, knees[1].z - 0.38f * s };
	hips = (b3Vec3){ offset.x, 0.72f * s, kneeZ + 0.2f * s };
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.35f, 0.0f ) );
	nbReachLeg( m, 0, leftFoot, ahead, nbFlatFoot() );
	nbReachLeg( m, 1, rightToes, ahead, b3MulQuat( nbFlatFoot(), b3MakeQuatFromAxisAngle( nb_x, -0.5f ) ) );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.7f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.7f );
	nbAddPose( m, start + 2.95f );

	// Standing on the left leg, the right foot swinging forward in the air
	hips = (b3Vec3){ offset.x, 0.9f * s, leftFoot.z - 0.02f * s };
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.12f, 0.0f ) );
	nbReachLeg( m, 0, leftFoot, ahead, nbFlatFoot() );
	nbTurn( m, nb_humanoidThighRight, nb_x, 0.6f );
	nbTurn( m, nb_humanoidShinRight, nb_x, -1.7f );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.3f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.4f );
	nbAddPose( m, start + 3.4f );

	// The right foot comes down beside the left, its heel first
	b3Vec3 rightFoot = { offset.x - ( leftFoot.x - offset.x ), footHeight, leftFoot.z };
	hips = (b3Vec3){ offset.x, 0.93f * s, leftFoot.z - 0.01f * s };
	nbResetPose( m, 0.0f );
	nbPlaceHips( m, hips, nbLying( 0.06f, 0.0f ) );
	nbReachLeg( m, 0, leftFoot, ahead, nbFlatFoot() );
	nbReachLeg( m, 1, b3Add( rightFoot, (b3Vec3){ 0.0f, 0.04f * s, -0.04f * s } ), ahead,
				b3MulQuat( nbFlatFoot(), b3MakeQuatFromAxisAngle( nb_x, 0.15f ) ) );
	m->plants[3].kind = nb_plantFree;
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.15f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.25f );
	nbAddPose( m, start + 3.65f );

	// Standing: the right foot beside the left, the root at the height of the idle clip
	nbResetPose( m, standHeight );
	m->root.p.x = offset.x;
	m->root.p.z = leftFoot.z;
	nbForward( m );
	nbReachLeg( m, 0, leftFoot, ahead, nbFlatFoot() );
	nbReachLeg( m, 1, rightFoot, ahead, nbFlatFoot() );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_x, 0.06f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.15f );
	nbAddPose( m, start + 3.95f );
}

// From lying on the back with the head ahead: the right knee and arm come across, the figure rolls over its left side
// onto its front, and gets up from there
static void nbMakeGetUpBack( nbPoseMaker* m, float standHeight )
{
	float s = m->s;
	float pi = B3_PI;

	// On the back
	nbResetPose( m, 0.0f );
	m->root.q = nbLying( 0.5f * pi, pi );
	nbTurnPair( m, nb_humanoidUpperArmLeft, nb_humanoidUpperArmRight, nb_z, 0.25f );
	nbTurnPair( m, nb_humanoidForearmLeft, nb_humanoidForearmRight, nb_x, 0.3f );
	nbSetDown( m );
	nbAddPose( m, 0.0f );

	// The right knee up and the right arm across, rolling toward the left over the left arm along the body
	nbResetPose( m, 0.0f );
	m->root.p.x = -0.04f * s;
	m->root.q = nbLying( 0.5f * pi, 1.2f * pi );
	nbTurn( m, nb_humanoidThighRight, nb_x, 0.9f );
	nbTurn( m, nb_humanoidThighRight, nb_z, 0.2f );
	nbTurn( m, nb_humanoidShinRight, nb_x, -1.4f );
	nbTurn( m, nb_humanoidUpperArmRight, nb_x, 1.3f );
	nbTurn( m, nb_humanoidUpperArmRight, nb_z, 0.3f );
	nbTurn( m, nb_humanoidUpperArmLeft, nb_z, 0.05f );
	nbSetDown( m );
	nbAddPose( m, 0.35f );

	// On the left side
	nbResetPose( m, 0.0f );
	m->root.p.x = -0.12f * s;
	m->root.q = nbLying( 0.5f * pi, 1.5f * pi );
	nbTurn( m, nb_humanoidThighRight, nb_x, 0.6f );
	nbTurn( m, nb_humanoidShinRight, nb_x, -0.8f );
	nbTurn( m, nb_humanoidUpperArmRight, nb_x, 1.0f );
	nbTurn( m, nb_humanoidUpperArmLeft, nb_z, 0.05f );
	nbSetDown( m );
	nbAddPose( m, 0.7f );

	// On the front, a body width to the left of where it lay, and up from there
	nbMakeGetUpFront( m, (b3Vec3){ -0.24f * s, 0.0f, 0.0f }, 1.05f, standHeight );
}

int nbBuildHumanoidClip( nbFigureClip which, float height, nbKeyframe* keyframes, b3Quat* rotations, int capacity,
						 nbClipDef* clip )
{
	nbPoseMaker* m = nbAlloc( sizeof( nbPoseMaker ) );
	memset( m, 0, sizeof( nbPoseMaker ) );
	float standHeight = nbMakeHumanoidBones( height, 74.0f, m->bones, nb_humanoidBoneCount );
	m->s = height / 1.8f;
	m->keyframes = keyframes;
	m->rotations = rotations;
	m->capacity = capacity;

	bool loop = false;
	bool moveRoot = false;
	float rate = 0.0f;
	switch ( which )
	{
		case nb_clipIdle:
			nbMakeIdle( m, standHeight );
			loop = true;
			break;
		case nb_clipFall:
			nbMakeFall( m, standHeight );
			loop = true;
			break;
		case nb_clipCurl:
			nbMakeCurl( m, standHeight );
			break;
		case nb_clipBrace:
			nbMakeBrace( m, standHeight );
			break;
		case nb_clipGetUpFront:
			nbMakeGetUpFront( m, b3Vec3_zero, 0.0f, standHeight );
			moveRoot = true;
			rate = NB_BAKE_RATE;
			break;
		case nb_clipGetUpBack:
			nbMakeGetUpBack( m, standHeight );
			moveRoot = true;
			rate = NB_BAKE_RATE;
			break;
		default:
			break;
	}

	nbBake( m, rate );
	int count = m->count <= capacity ? m->count : 0;
	nbFree( m, sizeof( nbPoseMaker ) );
	*clip = (nbClipDef){ keyframes, count, nb_humanoidBoneCount, loop, moveRoot };
	return count;
}
