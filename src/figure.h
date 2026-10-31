// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"

#include "nebenan/types.h"

typedef struct nbWorld nbWorld;

// The last hits a figure remembers, so a hit that several events report lands once, see nbHitDef::id
#define NB_HIT_MEMORY 16

// A clip as a figure keeps it, copied: the time of every keyframe, the rotations keyframe by keyframe, bone by bone, and
// the root of every keyframe in the frame of the figure
typedef struct nbClip
{
	float* times;
	b3Quat* rotations;
	b3Transform* roots;
	int keyframeCount;
	int boneCount;
	bool loop;
	bool moveRoot;
} nbClip;

// A bone of a figure: its body, the joint to its parent and what the figure needs of its definition
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

	// The limits of the joint as Box3D keeps them, as the cosine and sine of half of each angle: the twist about the z
	// axis of the joint frame below and above, and for a ball joint how far that axis swings, see nbLimitJointRotation
	b3CosSin lowerHalf;
	b3CosSin upperHalf;
	b3CosSin coneHalf;

	// Scales the torque limit, the stiffness and the damping ratio of the motors for this joint
	float strength;
	float stiffness;
	float damping;
	float mass;

	// The spring of the joint at strength 1 in cycles per second, from the inertia of the two bones when the figure fell
	float hertz;

	// The collision shape in the frame of the bone, see nbBoneDef
	nbBoneShape shape;
	b3Vec3 center;
	b3Vec3 halfExtents;
	b3Vec3 start;
	b3Vec3 end;
	float radius;
} nbBone;

// A foot of a humanoid: where its ankle stands while it is down and how the ground there tilts its sole, where a step to
// catch a hit takes it, and while the figure stands how far the ground under its sole lifts it over where the clip
// puts it, how the ground tilts it, and how far the clip holds it down, see nbPlaceFeet. The tilts are rotations in the
// frame of the figure. The ground it found last is a plane in the world, found under its sole where it stood then.
typedef struct nbFoot
{
	b3Pos plant;
	b3Quat tilt;
	b3Pos from;
	b3Quat fromTilt;
	b3Pos target;
	b3Quat targetTilt;
	float lift;
	b3Quat lean;
	float down;
	b3Pos groundPoint;
	b3Vec3 groundNormal;
	b3Pos castAt;
	bool hasGround;
	bool cast;
} nbFoot;

// A figure, see nbCreateFigure
typedef struct nbFigure
{
	nbBone* bones;
	int boneCount;

	// The definition without the rig and the clips, which the figure copied
	nbFigureDef def;

	nbFigureState state;
	float stateTime;

	// Where the figure stands, the point on the ground and its heading, and the heading as an angle
	b3WorldTransform frame;
	float heading;

	// The clips: the ones of the program copied, the library's shared by the figures of a world, see nbClipSet, and the
	// clip of the program that plays instead of idle while the figure stands
	nbClip ownClips[nb_figureClipCount];
	const nbClip* clips[nb_figureClipCount];
	nbClip playClip;
	bool playing;
	float clipTime;
	float clipSpeed;

	// The pose the figure goes for: the rotation of every bone against its rest pose, and the root in the frame. While a
	// new clip blends in, the pose it blends from.
	b3Quat* pose;
	b3Transform root;
	b3Quat* fadePose;
	b3Transform fadeRoot;
	float fadeTime;
	float fadeDuration;

	// The world transforms of the bones in that pose, where an animated figure moves them in the next step
	b3WorldTransform* targets;

	// The flinch of the trunk as a rotation vector and its rate, a damped spring
	b3Vec3 flinch;
	b3Vec3 flinchRate;

	// The feet on the ground and the steps to catch a hit, see nbPush: the feet, the foot in the air or -1 and how far
	// into its step, how far the body moves on the ground in all, how long it takes and how far along it is, and how far
	// the feet stand where they planted instead of where the clip puts them
	nbFoot feet[2];
	int swingFoot;
	float swingTime;
	bool balancing;
	b3Vec3 swayOffset;
	float swayTime;
	float swayDuration;
	float footWeight;
	int stepCount;

	// The feet stood on ground as flat and high as the clip has it in the last update that looked at them
	bool feetFlat;

	// Ragdoll: the strength of the joints, the slack after a hard hit, how far the figure curls up and braces, the
	// average speed of its bones, how long it lies still, the damping of its bones and whether the springs are on
	float strength;
	float stun;
	float protect;
	float brace;
	float speed;
	float restTime;
	float damping;
	bool springs;
	float fallTime;

	// Getting up: the pose and the root the ragdoll lay in, the clip and how far into it, and why it waits
	b3Quat* lyingPose;
	b3WorldTransform lyingRoot;
	int getUpClip;
	float getUpTime;
	bool onBack;
	nbFigureWait wait;

	// The bones of a ragdoll damage the destructibles they hit, see nbRigDef::enableCollisionDamage
	bool collisionDamage;

	// The ids of the last hits, see NB_HIT_MEMORY, and what the figure went through
	uint32_t hitIds[NB_HIT_MEMORY];
	int hitNext;
	int hitMemoryCount;
	int hitCount;
	int knockdownCount;
	int getUpCount;
	int abortCount;

	// The strongest hit of debris on a standing figure in the last step, read from the hit events of Box3D
	float debrisImpulse;
	b3Vec3 debrisDirection;
	b3Pos debrisPoint;
	int debrisBone;

	uint16_t generation;
	bool isFree;
} nbFigure;

NB_ARRAY_DECLARE( nbFigure, nbFigureArray );

// The clips of the library for the humanoid of one height, built once per world for the figures that take them
typedef struct nbClipSet
{
	float height;
	nbClip clips[nb_figureClipCount];
} nbClipSet;

typedef nbClipSet* nbClipSetPointer;
NB_ARRAY_DECLARE( nbClipSetPointer, nbClipSetArray );

// The figure and the bone a Box3D body belongs to, by the index of the body, both -1 for a body of no figure
typedef struct nbBoneRef
{
	int figure;
	int bone;
} nbBoneRef;

NB_ARRAY_DECLARE( nbBoneRef, nbBoneRefArray );

// Move the animated figures, set the joints of the ragdolls and get up those that can, in every update after a step
void nbUpdateFigures( nbWorld* world, float timeStep );

// A blast at a point, before Box3D pushes the dynamic bodies around it, see nbWorld_ApplyImpact: a standing figure that it
// hits hard enough falls first, so the blast throws its bones, a weaker blast makes it flinch
void nbBlastFigures( nbWorld* world, const nbImpactDef* def );

// The index of the figure whose bone this Box3D body is and the bone, NB_NULL_INDEX for a body of no figure
int nbFigureOfBody( const nbWorld* world, b3BodyId bodyId, int* bone );

// The ragdolls whose bones are awake in Box3D, see nbFigureStats::awakeRagdollCount
int nbCountAwakeRagdolls( const nbWorld* world );

// Destroy every figure of the world with its bodies and free the arrays, for nbDestroyWorld
void nbDestroyFigures( nbWorld* world );

// The clips of the library for the humanoid, see figure_clips.c and nbMakeHumanoidClip
int nbBuildHumanoidClip( nbFigureClip which, float height, nbKeyframe* keyframes, b3Quat* rotations, int capacity,
						 nbClipDef* clip );

// The part of a rotation, from none to all of it: the same axis, that part of the angle, the short way round
b3Quat nbPartOf( b3Quat rotation, float weight );

// Two bones from a pivot to a target, the middle joint toward a side, see figure_clips.c: the pivots of the middle and
// the end bone and the axis of the middle joint
b3Vec3 nbSolveTwoBones( b3Vec3 pivot, b3Vec3 target, float upper, float lower, b3Vec3 side, b3Vec3* middle, b3Vec3* end );

// The rotation whose x and y axes are these, made orthonormal around y
b3Quat nbRotationFromAxes( b3Vec3 x, b3Vec3 y );

// The most points of the shape of a bone, see nbBonePoints
#define NB_BONE_POINTS 48

// The points that span the shape of a box or a rounded box in the frame of the bone, its hull: 8 corners, or 6 points on
// each rounded corner. Returns how many, none for a capsule.
int nbBonePoints( const nbBoneDef* bone, b3Vec3* points );
