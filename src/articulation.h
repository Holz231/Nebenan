// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"

#include "nebenan/types.h"

typedef struct nbWorld nbWorld;

// The last hits an articulation remembers, so a hit that several events report lands once, see nbHitDef::id
#define NB_HIT_MEMORY 16

// A bone of an articulation: its body, the joint to its parent and the rotation the joint drives to
typedef struct nbBone
{
	b3BodyId bodyId;

	// The joint to the parent, null for the root
	b3JointId jointId;
	int parent;
	nbJointType joint;

	// The pose of the bone in the frame of its parent at rest, and the joint frame in the frame of the bone, see nbBoneDef
	b3Transform rest;
	b3Quat jointFrame;

	// The rotation against the rest pose the joint drives to, in the frame of the bone, within the limits
	b3Quat target;

	// The limits of the joint as Box3D keeps them, as the cosine and sine of half of each angle: the twist about the z
	// axis of the joint frame below and above, and for a ball joint how far that axis swings, see nbLimitJointRotation
	b3CosSin lowerHalf;
	b3CosSin upperHalf;
	b3CosSin coneHalf;

	// Scales the torque limit, the stiffness and the damping ratio of the motor profile for this joint
	float strength;
	float stiffness;
	float damping;

	// The collision shape in the frame of the bone, see nbBoneDef
	nbBoneShape shape;
	b3Vec3 center;
	b3Vec3 halfExtents;
	b3Vec3 start;
	b3Vec3 end;
	float radius;
} nbBone;

struct nbBalancer;

// A figure of bodies and joints, see nbCreateArticulation
typedef struct nbArticulation
{
	nbBone* bones;
	int boneCount;

	// The pose of the program, one rotation per bone: set with nbArticulation_SetTargetPose or sampled from the clip
	b3Quat* pose;

	// How far the figure reaches above and below the origin of its root along up at rest, in meters
	float restTop;
	float restBottom;

	// Balance, null without, see balance.c
	struct nbBalancer* balancer;

	// The axis of the root that points up when the figure stands, see nbRigDef::up
	b3Vec3 up;

	// A static body where the figure was created, and the parallel joint that holds the root upright against it, see
	// nbMotorProfile::uprightTorque
	b3BodyId anchorId;
	b3JointId uprightId;

	nbMotorProfile profile;

	// The clip that plays, copied: the time of every keyframe and the rotations keyframe by keyframe, bone by bone
	float* clipTimes;
	b3Quat* clipRotations;
	int clipKeyframeCount;
	bool clipLoop;
	bool clipPlaying;
	float clipTime;
	float clipSpeed;

	// The clip the figure records itself into while its clip plays, see nbArticulation_RecordPoseClip: the feet and hands
	// whose contacts it keeps, -1 for none, the last frame it recorded, the frame the pass ends at, and the balance it saw
	nbPoseClip* recording;
	int recordBones[4];
	int recordFrame;
	int recordEnd;
	nbBalanceState recordBalance;

	// The ids of the last hits, see NB_HIT_MEMORY, and how many hits landed
	uint32_t hitIds[NB_HIT_MEMORY];
	int hitNext;
	int hitMemoryCount;
	int hitCount;

	uint16_t generation;
	bool isFree;
} nbArticulation;

NB_ARRAY_DECLARE( nbArticulation, nbArticulationArray );

// The articulation and the bone a Box3D body belongs to, by the index of the body, both -1 for a body of no articulation
typedef struct nbBoneRef
{
	int articulation;
	int bone;
} nbBoneRef;

NB_ARRAY_DECLARE( nbBoneRef, nbBoneRefArray );

// Advance the clips of the articulations by a step and set the targets of their joints for the next one, in every update
void nbUpdateArticulations( nbWorld* world, float timeStep );

// The articulation of an id and its world, null for a stale id
nbArticulation* nbGetArticulationFromId( nbArticulationId id, nbWorld** worldOut );

// The most points of the shape of a bone, see nbBonePoints
#define NB_BONE_POINTS 48

// The points that span the shape of a box or a rounded box in the frame of the bone, its hull: 8 corners, or 6 points
// on each rounded corner. Returns how many, none for a capsule.
int nbBonePoints( const nbBoneDef* bone, b3Vec3* points );

// The part of a rotation, from none to all of it: the same axis, that part of the angle, the short way round
b3Quat nbPartOf( b3Quat rotation, float weight );

// The rotation that done count times gives a rotation, the short way round, as exact as floats are: the angles of Box3D
// are approximations, so nbPartOf alone misses by a few thousandths of a radian
b3Quat nbRootOf( b3Quat rotation, int count );

// The index of the articulation whose bone this Box3D body is, NB_NULL_INDEX for a body of no articulation
int nbArticulationOfBody( const nbWorld* world, b3BodyId bodyId );

// Is this Box3D body a bone of the articulation at this index?
bool nbIsArticulationBody( const nbWorld* world, int index, b3BodyId bodyId );

// Set the targets of all joints from a pose, one rotation per bone
void nbSetTargets( nbArticulation* articulation, const b3Quat* rotations );

// Apply a motor profile to the joints and the upright hold
void nbApplyMotorProfile( nbArticulation* articulation, const nbMotorProfile* profile );

// Change the most torque of the upright hold, see nbMotorProfile::uprightTorque
void nbSetUprightTorque( nbArticulation* articulation, float torque );

// The hold of getting up, see balance.c: a motor joint from the static anchor whose angular spring turns the whole root
// toward a rotation in the world, heading and all, with a stiffness in newton meters per radian about its softest axis
// and no torque yet, and a new rotation and most torque for it. The upright hold keeps only the up axis of the root.
b3JointId nbCreateRootHold( nbArticulation* articulation, b3Quat rotation, float stiffness );
void nbSetRootHold( nbArticulation* articulation, b3JointId hold, b3Quat rotation, float torque );

// The rotations of the first count bones against their rest pose, as the bodies are, see nbArticulation_GetPose
void nbGetPose( const nbArticulation* articulation, b3Quat* rotations, int count );

// Record a figure into its clip after a step, see nbArticulation_RecordPoseClip and pose_clip.c
void nbRecordPose( nbWorld* world, int index, nbArticulation* articulation, float timeStep );

// Balance, see balance.c: the update after every step, which gives the pose the joints are to take, the program changed
// the motor profile or put the figure into a pose, the state of the balance, off without, what the balance saw, null
// without, and the balance goes with its figure
const b3Quat* nbUpdateBalance( nbWorld* world, int index, nbArticulation* articulation, float timeStep );
void nbBalanceProfileChanged( nbArticulation* articulation );
void nbBalancePlaced( nbArticulation* articulation );
nbBalanceState nbBalanceStateOf( const nbArticulation* articulation );
const nbBalance* nbBalanceInfoOf( const nbArticulation* articulation );
void nbFreeBalancer( nbArticulation* articulation );

// Destroy every articulation of the world with its bodies and free the arrays, for nbDestroyWorld
void nbDestroyArticulations( nbWorld* world );
