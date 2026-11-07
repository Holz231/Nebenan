// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"

#include "nebenan/types.h"

typedef struct nbWorld nbWorld;

// The last hits a figure remembers, so a hit that several events report lands once, see nbHitDef::id
#define NB_HIT_MEMORY 16

// The height of the pelvis origin over the soles of the humanoid of 1.8 m as it stands, a little in the knees, see
// nbMakeHumanoidBones. With straight legs it would stand 2 cm higher.
#define NB_HUMANOID_STAND 0.96f

// A standing figure falls from ground that moves faster than this, in meters per second
#define NB_GROUND_SPEED 1.5f

// A steered figure faces where it moves only while it moves faster than this, in meters per second
#define NB_STEER_FACE_SPEED 0.1f

// The spine of the humanoid: its bones from the belly up to the chest, one after the other in nbHumanoidBone, which
// share the flinch of the trunk
#define NB_SPINE_COUNT ( nb_humanoidChest - nb_humanoidBelly + 1 )

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

	// A ball joint with a limit in each of four directions in place of the cone, see nbBoneDef::lowerSwingX: one over the
	// tangent of a quarter of the limits about +x, -x, +y and -y of the joint frame as Box3D keeps them, and the square of
	// the sine of half the least of them, within which every swing lies inside
	bool swingLimits;
	float swingInverseTans[4];
	float swingInside;

	// Scales the torque limit, the stiffness and the damping ratio of the motors for this joint, the damping ratio once
	// more while the figure catches itself, see nbBoneDef::catchDamping, the share of the stiffness the joint keeps after
	// a hit, see nbBoneDef::hitStiffness, and the rotational inertia added to the turn of its bone against its parent, see
	// nbBoneDef::armature
	float strength;
	float stiffness;
	float damping;
	float catchDamping;
	float hitStiffness;
	float armature;
	float mass;

	// Scales the torque limit of the joint on top of its strength while the figure braces it, one otherwise, see
	// NB_KNEE_BRACE
	float brace;

	// The spring of the joint at strength 1 in cycles per second, from the inertia of the two bones when the figure fell
	float hertz;

	// The muscles of the joint, see nbUpdateMuscles: how fast they can turn it, the strength the figure asks of them, how
	// far they are activated, whether the spring of the joint is on, the stiffness, damping ratio and torque limit it was
	// last given, none at first, and the angle of a hinge in the update before
	float maxSpeed;
	float drive;
	float activation;
	bool springOn;
	float springHertz;
	float springDamping;
	float springTorque;
	float hingeAngle;

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

	// The spring that swings the bone from a hit while the figure stands, see nbBoneDef::flinch: how fast a hit turns it
	// against how fast it would turn what the bone carries, the angular frequency of the spring, zero for none, and its
	// damping ratio, and whether what it carries hangs beside the body at rest, see nbLagCarried. Its swing about its joint
	// as a rotation vector in the frame of the figure and the rate of that, how fast the bone turns and its joint moves in
	// all from hits, and how fast its parent turned and its joint moved when the update before ended, see nbSwingBones.
	float flinch;
	float flinchOmega;
	float flinchDamping;
	bool hangs;
	b3Vec3 swing;
	b3Vec3 swingRate;
	b3Vec3 turnRate;
	b3Vec3 pivotVelocity;
	b3Vec3 carried;
	b3Vec3 carriedVelocity;

	// The rotation of the bone in the frame of the figure while the update turns the bones, see nbTurnBones
	b3Quat turned;
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

// Where an upper arm is in its swing against a fall, see nbRowArms
typedef enum nbArmSwing
{
	nb_armHangs,
	nb_armSwingsOut,
	nb_armSwingsBack,
} nbArmSwing;

// A figure that catches itself after a hard hit, see figure_balance.c: the legs at rest, what it measures in every update,
// its steps and its upright hold
typedef struct nbBalancer
{
	// The legs at rest: hip pivots in the frame of the pelvis, lengths of thigh and shin, the rotation of the foot in the
	// frame of the pelvis, the corners and the middle of the sole in the frame of the foot, and how far the ankle lies
	// above the sole. Standing: the pelvis origin above the soles with straight legs, the center of mass above them in
	// the first pose of the idle clip, the least gap between the ankles sideways, and the size of the figure against the
	// humanoid of 1.8 m.
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

	// Measured in every update: up, gravity, the height of the ground under the feet along up and under each foot on
	// the ground, which feet stand, where each foot faces and where the figure faces after its feet, how far the pelvis
	// tilts, the speed of the bones on average by their mass, the center of mass, its velocity and the angular momentum
	// of the figure about it, the capture point, how far it lies outside the support and the middle of the support
	b3Vec3 up;
	float gravity;
	float groundHeight;
	float footGround[2];
	bool footContact[2];
	b3Vec3 footFacing[2];
	b3Vec3 forward;
	float tilt;
	float meanSpeed;
	b3Vec3 center;
	b3Vec3 velocity;
	b3Vec3 spin;
	b3Vec3 capture;
	float captureMargin;
	int supportCount;
	b3Vec3 supportMiddle;

	// A step: whether one swings, how long since it began or the figure stood, the foot, the steps in a row and the foot
	// that stepped last, where the ankles are planted, where the swinging one started, how fast it moved over the ground
	// then and where it lands, and how far the capture point at its landing lies outside the support of both soles,
	// negative inside. Updates in a row with the capture point outside, seconds without a foot on the ground, seconds
	// with one foot alone on the ground, seconds calm and seconds without rest and without a push.
	bool stepping;
	float stateTime;
	int stepFoot;
	int stepCount;
	int lastFoot;
	b3Vec3 plant[2];
	b3Vec3 swingStart;
	b3Vec3 swingVelocity;
	b3Vec3 stepAnkle;
	float stepResidual;
	int dangerUpdates;
	float airborneTime;
	float oneFootTime;
	float calmTime;
	float restlessTime;

	// How much of its turn after the feet the pelvis takes and the upper body against it, see nbPelvisTurn: all of it
	// while a foot swings, falling off while the figure stands
	float turnScale;

	// How much of that turn the pelvis itself takes, see nbStepShare: going over each step from the share it had when
	// the step began to the share of the step, kept while the figure stands
	float pelvisShare;
	float pelvisShareFrom;

	// The arms row against a fall, see nbRowArms: where each upper arm is in its swing, the seconds into the swing out or
	// the way back, the seconds since it came out, the rotation against its rest pose it swings from, out from where it
	// started or back from where it came out, the axis of the swing in the frame of the shoulder and its angle
	nbArmSwing armSwing[2];
	float armTime[2];
	float armHold[2];
	b3Quat armFrom[2];
	b3Vec3 armAxis[2];
	float armAngle[2];

	// An arm that something holds or pushes goes slack, see nbHoldArms: the seconds it stays held after the last check
	// that found the push, and how slack it is, from 0 to 1
	float armHeld[2];
	float armSlack[2];

	// The hold that keeps the pelvis upright, see nbFigureMotors::uprightTorque: against the world, or between the pelvis
	// and each thigh, shared by the legs that carry the figure. Without the hold against the world the figure catches
	// itself with its own strength, see NB_OWN_MAX_STEPS.
	b3JointId upright;
	b3JointId uprightLegs[2];
	bool ownStrength;
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

	// The collision group of its bones, of this figure alone: negative without self collision, positive with it. The
	// queries of the figure leave it out, so Box3D does not compute its own bones, see nbFigureQueryFilter.
	int groupIndex;

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

	// The legs blend in from the fade pose too, after the feet set them: a ragdoll that wakes up standing has its knees
	// where they lay, see nbStandWhereItIs
	bool fadeLegs;

	// The world transforms of the bones in that pose, where an animated figure moves them in the next step
	b3WorldTransform* targets;

	// The flinch of the trunk as a rotation vector and its rate, a damped spring. Whether a bone has a spring of its own,
	// see nbBone::flinchOmega, and whether one swings or the trunk flinches.
	b3Vec3 flinch;
	b3Vec3 flinchRate;
	bool hasSwings;

	// The upper body of the humanoid turns about the axis up its spine more easily than it tilts: a flinch turns it
	// twistGain times as far about that axis and as much faster back, up to twistLimit, the twist its spine allows, and
	// the joints of its spine share the twist after their twist ranges, spineTwist, and the tilt forward and back and to
	// the side after their swing ranges, spineBend and spineSide, see nbComputeTwist
	float twistGain;
	float twistLimit;
	float spineTwist[NB_SPINE_COUNT];
	float spineBend[NB_SPINE_COUNT];
	float spineSide[NB_SPINE_COUNT];
	bool swinging;

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
	int pushCount;

	// The joints that give way after a hit, see nbSoftenJoints: the share of the hit that softens them, how soft they still
	// are, from 1 right after it to 0, and how many seconds until they hold as stiff as before
	float softShare;
	float softness;
	float softTime;

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

	// Whether bodies of others pushed a bone of the figure sideways at the last check, see nbFigureDef::pushForce, and
	// the strongest push on a bone of each arm of a humanoid then, in newtons
	bool pushed;
	float armPush[2];

	// Following its clip with dynamic bones near another figure, see nbFigureDef::engageRadius and nbBeginFollowing:
	// whether it follows, a motor joint per bone from the static anchor of the world that drives it, where the clip put
	// the bones for the end of the last step, how fast it moved them in it and the rotations of their joints, see
	// nbFollowClip and nbFollowJoints, whether another
	// figure was near at the start of the update, how long none was, how long a bone was stuck far from the clip, see
	// NB_FOLLOW_STUCK, whether other figures overpowered it at the last check, see NB_FOLLOW_OVERPOWER, and how often it
	// was pushed off its clip
	bool following;
	b3JointId* followJoints;
	b3WorldTransform* followTargets;
	b3Vec3* followVelocities;
	b3Quat* followRelative;
	bool engaged;
	float apartTime;
	float stuckTime;
	bool overpowered;
	int letGoCount;

	// Steered by a program, see nbFigure_Steer and figure_steer.c: what it wants, the capsule it moves as over the point
	// of its frame, its radius and the heights of its lower and upper center, the velocity it moves with, whether the
	// probe under the capsule finds ground, the rate of the spring that holds it over the ground, how long it is in the
	// air, the slope of its way and how fast its heading turns. The gait: whether the feet step after the capsule, how far
	// into its cycle of two steps, how far the cycle is behind the feet and how fast it catches up, see NB_GAIT_KEEP, the
	// speed it steps for, the velocity its feet aim with and how fast that changes, whether each foot swings, may lift
	// again, how far into its swing it is and from where it swung, how high it lifts and how close under the body it
	// lands, the heading each foot has, lifted with and lands with, how far each rolls over its toes, or with its toes up
	// over its heel, and lifted rolled, how far it landed rolled over its heel and how long it stands since, how far it
	// rolls over its toes at least before it lifts and how fast it rolled over them in the last update, how long from the
	// last update on its leg reaches it as the capsule goes on and whether it was behind the hip, see nbLowerSteered, how
	// many times as fast as its gait it swings, and where a swinging foot is on its way without its lift, how fast it
	// moves there and how fast it turns, see nbSwingPath. Then how far the legs stretch in parts of their length, how far
	// the pelvis turns and sways over the feet, how far the gait lowers the root, and how far the root went down in all in
	// the last update, how fast that changed and how far of it the gait lowered it.
	bool steered;
	nbSteer steer;
	float moverRadius;
	float moverLow;
	float moverHigh;
	b3Vec3 moveVelocity;
	bool onGround;
	float probeRate;
	float airTime;
	float slope;
	float headingRate;
	bool gaitOn;
	float gaitPhase;
	float gaitLag;
	float gaitLagRate;
	float gaitSpeed;
	b3Vec3 gaitVelocity;
	b3Vec3 gaitAcceleration;
	bool footSwings[2];
	bool canLift[2];
	float swingAlong[2];
	float swingStart[2];
	float swingHeight[2];
	float swingNarrow[2];
	float footHeading[2];
	float liftHeading[2];
	float landHeading[2];
	float footRoll[2];
	float swingRoll[2];
	float landRoll[2];
	float stanceTime[2];
	float footPush[2];
	float footRollRate[2];
	float footOut[2];
	bool outBehind[2];
	float swingHurry[2];
	float swingCalm[2];
	b3Pos swingBase[2];
	b3Vec3 swingVelocity[2];
	float swingTurnRate[2];
	float legReach;
	float gaitTurn;
	float gaitSway;
	float gaitCrouch;
	float gaitBend;
	float gaitBendRate;
	float rootDrop;
	float rootDropRate;
	float rootCrouch;

	// On stairs, see nbFindFoothold: how sure the figure is that it walks stairs, from 0 to 1, how deep a tread is along its
	// way and how much higher the next one lies, how many treads a step goes, where each foot stands or lands and where a
	// swinging foot lifted, the ground under the middle of its sole, whether a swinging foot chose where it lands, and how
	// much faster than its cadence the gait goes so that the foot lands there as far into the stance as on level ground
	float stairWeight;
	float stairDepth;
	float stairRise;
	int stairStep;
	b3Pos foothold[2];
	b3Pos liftHold[2];
	bool footholdChosen[2];
	float gaitRate;

	// How far the root goes on from where it was as the gait starts or ends on stairs, see nbCarryRoot, the ground the body
	// followed in the last update, see nbStairGround, and the frame, the height of the root over it and whether the figure
	// walked in the last update
	float stairCarry;
	b3Pos stairBase;
	b3Pos carryFrame;
	float carryRoot;
	bool carryGait;

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

// e to the minus x for x from 0 to a few, the same bits everywhere, see figure.c
float nbDecay( float x );

// The frame of a figure: the point on the ground and the heading about up, see figure.c
b3WorldTransform nbMakeFrame( const nbWorld* world, b3Pos position, float heading );

// Steering, see figure_steer.c: the capsule of a steered figure from its body when it is created, and its move in an
// update while it stands, false where it falls
void nbInitSteering( nbFigure* f, const b3WorldTransform* transforms );
bool nbMoveSteered( nbWorld* world, int index, nbFigure* f, float timeStep, b3Vec3 up );

// A steered figure stands again or is placed: still, on the ground, its feet where they stand
void nbResetSteering( nbFigure* f );

// An angle from minus pi to pi
float nbWrapAngle( float angle );

// A blast at a point, before Box3D pushes the dynamic bodies around it, see nbWorld_ApplyImpact: a standing figure that it
// hits hard enough falls first, so the blast throws its bones, a weaker blast makes it flinch
void nbBlastFigures( nbWorld* world, const nbImpactDef* def );

// The index of the figure whose bone this Box3D body is and the bone, NB_NULL_INDEX for a body of no figure
int nbFigureOfBody( const nbWorld* world, b3BodyId bodyId, int* bone );

// The filter of the queries of a figure: Box3D leaves out the shapes of its bones before it computes them. Their
// callbacks still leave out the figure, which takes the same shapes away, so what the queries find stays the same.
b3QueryFilter nbFigureQueryFilter( const nbWorld* world, int index );

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

// The static body that the upright holds of figures that catch themselves and the drives of figures that follow their
// clips hold against, one for the world, made the first time a figure needs it
b3BodyId nbFigureAnchor( nbWorld* world );

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
float nbJointSpringHertz( b3BodyId bodyA, b3BodyId bodyB, b3Quat frameB, bool hingeOnly, float stiffness, float armature );

// How far the ball joint of a bone lets it swing out of its rest direction toward one side, in radians: the swing about an
// axis in the xy plane of the joint frame, (axisX, axisY) of length 1, within the limit in each of four directions, or the
// cone in every direction, see nbBoneDef::lowerSwingX
float nbSwingLimit( const nbBone* bone, float axisX, float axisY );

// Ask the muscles of the joint of a bone for a strength of the motors, see nbFigureMotors, or let them relax. They follow
// in the update, see nbUpdateMuscles.
void nbSetBoneSpring( nbFigure* f, int index, float strength, bool enable );

// Balance, see figure_balance.c. Can the figure catch itself, the humanoid of the library? The legs at rest and the
// height of the center of mass standing, at creation.
bool nbCanBalance( const nbFigure* f );
void nbInitBalancer( nbFigure* f, float standCenter );

// Begin to catch itself, the bones already dynamic: the upright hold and what balance measures. End it, without the hold.
void nbBeginBalance( nbWorld* world, int index, nbFigure* f );
void nbEndBalance( nbFigure* f );

// Measure, decide and give the targets of the joints: the pose of the clip comes in, the legs and the arms of balance go
// out. Fell when the steps do not catch the figure, settled when it stands calm.
nbBalanceResult nbUpdateBalance( nbWorld* world, int index, nbFigure* f, float timeStep, b3Quat* pose );

// A figure that catches itself swings a foot, where it faces and the height of the ground under its feet
bool nbBalanceStepping( const nbFigure* f );
b3Vec3 nbBalanceForward( const nbFigure* f );
float nbBalanceGround( const nbFigure* f );

// Does a foot of the humanoid, 0 left or 1 right, touch ground it could stand on, as balance measures it: contacts of the
// foot with faces no steeper than walkable ground that carry it, and their mean height
bool nbFootOnGround( const nbWorld* world, int index, const nbFigure* f, int foot, b3Vec3 up, float* height );

// The most points of the shape of a bone, see nbBonePoints
#define NB_BONE_POINTS 48

// The points that span the shape of a box or a rounded box in the frame of the bone, its hull: 8 corners, or 6 points on
// each rounded corner. Returns how many, none for a capsule.
int nbBonePoints( const nbBoneDef* bone, b3Vec3* points );
