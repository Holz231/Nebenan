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

	// The velocity of a bone of a ragdoll when the next step begins: read at the end of every update and after every hit,
	// fall and blast, zero while the ragdoll sleeps, see nbPassFigureThrough
	b3Vec3 stepVelocity;
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

// A figure that catches itself after a hard hit, see figure_balance.c: the legs at rest, what it measures in every update,
// its steps and its upright hold
typedef struct nbBalancer
{
	// The legs at rest: hip pivots in the frame of the pelvis, lengths of thigh and shin, the rotation of the foot in the
	// frame of the pelvis, the corners and the middle of the sole in the frame of the foot, and how far the ankle lies
	// above the sole. Standing: the pelvis origin and the center of mass above the soles, the least gap between the
	// ankles sideways, and the size of the figure against the humanoid of 1.8 m.
	b3Vec3 hip[2];
	float thighLength[2];
	float shinLength[2];
	b3Quat footRest[2];
	b3Vec3 sole[2][4];
	b3Vec3 soleCenter[2];
	float soleHeight[2];
	float standHeight;
	float standCenter;
	float footGap;
	float scale;

	// Measured in every update: up, gravity, the height of the ground under the feet along up and under each foot on the
	// ground, which feet stand, where the figure faces, how far the pelvis tilts, the speed of the bones on average by
	// their mass, the center of mass and its velocity, the capture point, how far it lies outside the support and the
	// middle of the support
	b3Vec3 up;
	float gravity;
	float groundHeight;
	float footGround[2];
	bool footContact[2];
	b3Vec3 forward;
	float tilt;
	float meanSpeed;
	b3Vec3 center;
	b3Vec3 velocity;
	b3Vec3 capture;
	float captureMargin;
	int supportCount;
	b3Vec3 supportMiddle;

	// A step: whether one swings, how long since it began or the figure stood, the foot, the steps in a row and the foot
	// that stepped last, where the ankles are planted, where the swinging one started and where it lands. Updates in a
	// row with the capture point outside, seconds without a foot on the ground and seconds calm.
	bool stepping;
	float stateTime;
	int stepFoot;
	int stepCount;
	int lastFoot;
	b3Vec3 plant[2];
	b3Vec3 swingStart;
	b3Vec3 stepAnkle;
	int dangerUpdates;
	float airborneTime;
	float calmTime;

	// The hold that keeps the pelvis upright, see nbFigureMotors::uprightTorque
	b3JointId upright;
} nbBalancer;

// What balance found in an update, see nbUpdateBalance
typedef enum nbBalanceResult
{
	nb_balanceGoesOn,
	nb_balanceFell,
	nb_balanceSettled,
} nbBalanceResult;

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

	// Getting up: the pose and the root the ragdoll lay in, the clip and how far into it, and why it waits. A figure that
	// moves where it has room to get up, see nbFindRoom, moves its root from where it lay and turns it about up, over a
	// time, and lies where it gets up at the end.
	b3Quat* lyingPose;
	b3WorldTransform lyingRoot;
	int getUpClip;
	float getUpTime;
	bool onBack;
	nbFigureWait wait;
	b3WorldTransform shiftFrom;
	float shiftTurn;
	float shiftTime;
	float shiftLeft;
	int shiftCount;

	// The bones of a ragdoll damage the destructibles they hit, see nbRigDef::enableCollisionDamage
	bool collisionDamage;

	// Catching itself after a hard hit, see figure_balance.c, for the humanoid. How often it caught itself and stood again.
	bool canBalance;
	nbBalancer balancer;
	int catchCount;

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

// The id of the figure at an index
nbFigureId nbFigureIdOf( const nbWorld* world, int index );

// Read the velocities of the bones of the ragdolls as the next step begins with them, after a blast threw them
void nbReadFigureVelocities( nbWorld* world );

// A ragdoll broke through what it hit: every bone gets back up to this share of its speed toward it from before the step,
// along the normal out of what it hit, all together no more than this share of the momentum of the figure. Returns the
// share of its momentum toward it the figure kept.
float nbPassFigureThrough( nbWorld* world, int index, b3Vec3 normal, float keep );

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

// A spring in cycles per second for Box3D from a stiffness in newton meters per radian, against the rotational mass of the
// two bodies about the softest axis of the joint frame of body B, its z axis alone for a hinge
float nbJointSpringHertz( b3BodyId bodyA, b3BodyId bodyB, b3Quat frameB, bool hingeOnly, float stiffness );

// Balance, see figure_balance.c. Can the figure catch itself, the humanoid of the library? The legs at rest, at creation.
bool nbCanBalance( const nbFigure* f );
void nbInitBalancer( nbFigure* f );

// Begin to catch itself, the bones already dynamic: the upright hold and what balance measures. End it, without the hold.
void nbBeginBalance( nbWorld* world, int index, nbFigure* f );
void nbEndBalance( nbFigure* f );

// Measure, decide and give the targets of the joints: the pose of the clip comes in, the legs of balance go out. Fell
// when no step catches the figure, settled when it stands calm.
nbBalanceResult nbUpdateBalance( nbWorld* world, int index, nbFigure* f, float timeStep, b3Quat* pose );

// A figure that catches itself swings a foot, where it faces and the height of the ground under its feet
bool nbBalanceStepping( const nbFigure* f );
b3Vec3 nbBalanceForward( const nbFigure* f );
float nbBalanceGround( const nbFigure* f );

// The most points of the shape of a bone, see nbBonePoints
#define NB_BONE_POINTS 48

// The points that span the shape of a box or a rounded box in the frame of the bone, its hull: 8 corners, or 6 points on
// each rounded corner. Returns how many, none for a capsule.
int nbBonePoints( const nbBoneDef* bone, b3Vec3* points );
