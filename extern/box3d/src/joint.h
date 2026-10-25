// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "math_internal.h"
#include "solver.h"

#include "box3d/types.h"

typedef struct b3DebugDraw b3DebugDraw;
typedef struct b3StepContext b3StepContext;
typedef struct b3World b3World;

/// A joint edge is used to connect bodies and joints together
/// in a joint graph where each body is a node and each joint
/// is an edge. A joint edge belongs to a doubly linked list
/// maintained in each attached body. Each joint has two joint
/// nodes, one for each attached body.
typedef struct b3JointEdge
{
	int bodyId;
	int prevKey;
	int nextKey;
} b3JointEdge;

// Map from b3JointId to b3Joint in the solver sets
typedef struct b3Joint
{
	void* userData;

	// index of simulation set stored in b3World
	// B3_NULL_INDEX when slot is free
	int setIndex;

	// index into the constraint graph color array, may be B3_NULL_INDEX for sleeping/disabled joints
	// B3_NULL_INDEX when slot is free
	int colorIndex;

	// joint index within set or graph color
	// B3_NULL_INDEX when slot is free
	int localIndex;

	b3JointEdge edges[2];

	int jointId;
	int islandId;

	// Index into the island's joints array for O(1) swap-removal.
	// B3_NULL_INDEX when not in an island.
	int islandIndex;

	float drawScale;

	b3JointType type;

	// This is monotonically advanced when a body is allocated in this slot
	// Used to check for invalid b3JointId
	uint16_t generation;

	bool collideConnected;
} b3Joint;

// Added for Nebenan: the point constraint of a joint at the positions its bodies have in a substep: the anchors relative to
// the centers of mass and the inverse of the effective mass as b3Solve3 applies it, see b3ComputePointPose and
// b3JointSim::hasPose
typedef struct b3PointPose
{
	b3Vec3 rA;
	b3Vec3 rB;
	b3Matrix3 cofactors;
	float invDet;
	bool solvable;
} b3PointPose;

// Added for Nebenan: the two angular constraints across the axis of a revolute or parallel joint at the positions its
// bodies have in a substep: their axes and the inverse of their effective mass as b3Solve2 applies it, see
// b3ComputeAxesPose
typedef struct b3AxesPose
{
	b3Vec3 perpAxisX;
	b3Vec3 perpAxisY;
	float xx, xy, yx, yy;
	bool solvable;
} b3AxesPose;

// Added for Nebenan: what the solver of a parallel joint needs of the positions of its bodies, see b3JointSim::hasPose
typedef struct b3ParallelPose
{
	b3AxesPose axes;
	b3Vec2 bias;
} b3ParallelPose;

// Added for Nebenan: what the solver of a revolute joint needs of the positions of its bodies, see b3JointSim::hasPose
typedef struct b3RevolutePose
{
	b3PointPose point;
	b3AxesPose axes;
	b3Vec2 collinearity;
	float angle;
} b3RevolutePose;

// Added for Nebenan: what the solver of a spherical joint needs of the positions of its bodies, see b3JointSim::hasPose
typedef struct b3SphericalPose
{
	b3PointPose point;
	b3Vec3 springBias;
	float twistAngle;
	float swingAngle;
} b3SphericalPose;

typedef struct b3DistanceJoint
{
	float length;
	float hertz;
	float dampingRatio;
	float lowerSpringForce;
	float upperSpringForce;
	float minLength;
	float maxLength;

	float maxMotorForce;
	float motorSpeed;

	float impulse;
	float lowerImpulse;
	float upperImpulse;
	float motorImpulse;

	int indexA;
	int indexB;
	b3Vec3 anchorA;
	b3Vec3 anchorB;
	b3Vec3 deltaCenter;
	b3Softness distanceSoftness;
	float axialMass;

	bool enableSpring;
	bool enableLimit;
	bool enableMotor;
} b3DistanceJoint;

typedef struct b3MotorJoint
{
	b3Vec3 linearVelocity;
	b3Vec3 angularVelocity;
	float maxVelocityForce;
	float maxVelocityTorque;
	float linearHertz;
	float linearDampingRatio;
	float maxSpringForce;
	float angularHertz;
	float angularDampingRatio;
	float maxSpringTorque;

	b3Vec3 linearVelocityImpulse;
	b3Vec3 angularVelocityImpulse;
	b3Vec3 linearSpringImpulse;
	b3Vec3 angularSpringImpulse;

	b3Softness linearSpring;
	b3Softness angularSpring;

	int indexA;
	int indexB;
	b3Transform frameA;
	b3Transform frameB;
	b3Vec3 deltaCenter;
	b3Matrix3 angularMass;
} b3MotorJoint;

typedef struct b3ParallelJoint
{
	float hertz;
	float dampingRatio;
	float maxTorque;

	b3Vec2 perpImpulse;
	b3Vec3 perpAxisX;
	b3Vec3 perpAxisY;

	b3Quat quatA;
	b3Quat quatB;
	int indexA;
	int indexB;
	b3Softness softness;

	// Added for Nebenan: see b3JointSim::hasPose
	b3ParallelPose pose;
} b3ParallelJoint;

typedef struct b3PrismaticJoint
{
	b3Vec2 perpImpulse;
	b3Vec3 angularImpulse;
	float springImpulse;
	float motorImpulse;
	float lowerImpulse;
	float upperImpulse;
	float hertz;
	float dampingRatio;
	float maxMotorForce;
	float motorSpeed;
	float targetTranslation;
	float lowerTranslation;
	float upperTranslation;

	int indexA;
	int indexB;
	b3Transform frameA;
	b3Transform frameB;
	b3Vec3 jointAxis;
	b3Vec3 perpAxisY;
	b3Vec3 perpAxisZ;
	b3Vec3 deltaCenter;
	float deltaAngle;
	b3Matrix3 rotationMass;
	b3Softness springSoftness;

	bool enableSpring;
	bool enableLimit;
	bool enableMotor;
} b3PrismaticJoint;

typedef struct b3RevoluteJoint
{
	b3Vec3 linearImpulse;
	b3Vec2 perpImpulse;
	float springImpulse;
	float motorImpulse;
	float lowerImpulse;
	float upperImpulse;
	float hertz;
	float dampingRatio;

	// Added for Nebenan: the most torque of the spring, see b3RevoluteJointDef::maxSpringTorque
	float maxSpringTorque;

	float maxMotorTorque;
	float motorSpeed;
	float targetAngle;
	float lowerAngle;
	float upperAngle;

	int indexA;
	int indexB;
	b3Transform frameA;
	b3Transform frameB;
	b3Vec3 rotationAxisZ;
	b3Vec3 perpAxisX;
	b3Vec3 perpAxisY;
	b3Vec3 deltaCenter;
	float deltaAngle;
	float axialMass;
	b3Softness springSoftness;

	bool enableSpring;
	bool enableMotor;
	bool enableLimit;

	// Added for Nebenan: see b3JointSim::hasPose
	b3RevolutePose pose;
} b3RevoluteJoint;

typedef struct b3SphericalJoint
{
	b3Vec3 linearImpulse;
	b3Vec3 springImpulse;
	b3Vec3 motorImpulse;
	float lowerTwistImpulse;
	float upperTwistImpulse;
	float swingImpulse;
	float hertz;
	float dampingRatio;

	// Added for Nebenan: the most torque of the spring, see b3SphericalJointDef::maxSpringTorque
	float maxSpringTorque;

	float maxMotorTorque;
	b3Vec3 motorVelocity;
	float lowerTwistAngle;
	float upperTwistAngle;
	float coneAngle;
	b3Quat targetRotation;

	int indexA;
	int indexB;
	b3Transform frameA;
	b3Transform frameB;
	b3Vec3 deltaCenter;
	b3Vec3 swingAxis;
	b3Vec3 twistJacobian;

	b3Matrix3 rotationMass;
	float swingMass;
	float twistMass;
	b3Softness springSoftness;

	bool enableSpring;
	bool enableMotor;
	bool enableConeLimit;
	bool enableTwistLimit;

	// Added for Nebenan: see b3JointSim::hasPose
	b3SphericalPose pose;
} b3SphericalJoint;

typedef struct b3WeldJoint
{
	float linearHertz;
	float linearDampingRatio;
	float angularHertz;
	float angularDampingRatio;

	b3Softness linearSpring;
	b3Softness angularSpring;
	b3Vec3 linearImpulse;
	b3Vec3 angularImpulse;

	int indexA;
	int indexB;
	b3Transform frameA;
	b3Transform frameB;
	b3Vec3 deltaCenter;

	b3Matrix3 angularMass;
} b3WeldJoint;

typedef struct b3WheelJoint
{
	b3Vec2 linearImpulse;
	b3Vec2 angularImpulse;
	float spinImpulse;
	float maxSpinTorque;
	float spinSpeed;
	float suspensionSpringImpulse;
	float lowerSuspensionImpulse;
	float upperSuspensionImpulse;
	float lowerSuspensionLimit;
	float upperSuspensionLimit;
	float suspensionHertz;
	float suspensionDampingRatio;
	float steeringSpringImpulse;
	float lowerSteeringImpulse;
	float upperSteeringImpulse;
	float lowerSteeringLimit;
	float upperSteeringLimit;
	float targetSteeringAngle;
	float maxSteeringTorque;
	float steeringHertz;
	float steeringDampingRatio;

	int indexA;
	int indexB;
	b3Transform frameA;
	b3Transform frameB;
	b3Vec3 deltaCenter;
	float spinMass;
	float suspensionMass;
	float steeringMass;
	b3Softness suspensionSoftness;
	b3Softness steeringSoftness;

	bool enableSpinMotor;
	bool enableSuspensionSpring;
	bool enableSuspensionLimit;
	bool enableSteering;
	bool enableSteeringLimit;
	bool enableSteeringMotor;
} b3WheelJoint;

/// The base joint class. Joints are used to constraint two bodies together in
/// various fashions. Some joints also feature limits and motors.
typedef struct b3JointSim
{
	int jointId;

	int bodyIdA;
	int bodyIdB;

	b3JointType type;

	// Joint frames local to body origin
	b3Transform localFrameA;
	b3Transform localFrameB;

	float invMassA, invMassB;
	b3Matrix3 invIA, invIB;

	float constraintHertz;
	float constraintDampingRatio;

	b3Softness constraintSoftness;

	float forceThreshold;
	float torqueThreshold;

	bool fixedRotation;

	// Added for Nebenan: the bodies move only where the solver integrates their positions, so the relax stage of a substep
	// sees them where the warm start and the solve of the next substep see them. A spherical, revolute or parallel joint
	// keeps what it needs of their positions from relax, see b3SphericalPose, and those two stages take it from there
	// instead of computing it again, to the bit. The first substep of a step computes its own.
	bool hasPose;

	union
	{
		b3DistanceJoint distanceJoint;
		b3MotorJoint motorJoint;
		b3ParallelJoint parallelJoint;
		b3RevoluteJoint revoluteJoint;
		b3SphericalJoint sphericalJoint;
		b3PrismaticJoint prismaticJoint;
		b3WeldJoint weldJoint;
		b3WheelJoint wheelJoint;
	};
} b3JointSim;

// Added for Nebenan: the point constraint of a joint at the positions of its bodies, its anchors in body frames relative to
// the centers of mass turned by the delta rotations, with the operations of b3Solve3 up to the right hand side, see
// b3SolvePointPose
static inline void b3ComputePointPose( b3PointPose* pose, const b3JointSim* base, b3Quat deltaRotationA, b3Vec3 anchorA,
									   b3Quat deltaRotationB, b3Vec3 anchorB )
{
	float mA = base->invMassA;
	float mB = base->invMassB;
	b3Vec3 rA = b3RotateVector( deltaRotationA, anchorA );
	b3Vec3 rB = b3RotateVector( deltaRotationB, anchorB );

	//// K = [(1/m1 + 1/m2) * eye(2) - skew(r1) * invI1 * skew(r1) - skew(r2) * invI2 * skew(r2)]
	b3Matrix3 sA = b3Skew( rA );
	b3Matrix3 sB = b3Skew( rB );
	b3Matrix3 kA = b3MulMM( sA, b3MulMM( base->invIA, sA ) );
	b3Matrix3 kB = b3MulMM( sB, b3MulMM( base->invIB, sB ) );
	b3Matrix3 k = b3NegateMat3( b3AddMM( kA, kB ) );
	k.cx.x += mA + mB;
	k.cy.y += mA + mB;
	k.cz.z += mA + mB;

	float det = b3Det( k );
	pose->solvable = b3AbsFloat( det ) > 1000.0f * FLT_MIN;
	pose->invDet = pose->solvable ? 1.0f / det : 0.0f;
	pose->cofactors.cx = b3Cross( k.cy, k.cz );
	pose->cofactors.cy = b3Cross( k.cz, k.cx );
	pose->cofactors.cz = b3Cross( k.cx, k.cy );
	pose->rA = rA;
	pose->rB = rB;
}

// Added for Nebenan: the rest of b3Solve3 for the effective mass of a point pose
static inline b3Vec3 b3SolvePointPose( const b3PointPose* pose, b3Vec3 a )
{
	if ( pose->solvable )
	{
		float invDet = pose->invDet;
		return B3_LITERAL( b3Vec3 ){
			invDet * b3Dot( pose->cofactors.cx, a ),
			invDet * b3Dot( pose->cofactors.cy, a ),
			invDet * b3Dot( pose->cofactors.cz, a ),
		};
	}

	return b3Vec3_zero;
}

// Added for Nebenan: the collinearity constraint of a revolute or parallel joint at the positions of its bodies, from the
// joint frame of body A and the relative rotation, with the operations of b3Solve2 up to the right hand side, see
// b3SolveAxesPose
static inline void b3ComputeAxesPose( b3AxesPose* pose, const b3JointSim* base, b3Quat quatA, b3Quat relQ )
{
	b3Vec3 perpAxisX =
		b3MulSV( 0.5f, b3RotateVector( quatA, b3Add( b3MulSV( relQ.s, b3Vec3_axisX ), b3Cross( relQ.v, b3Vec3_axisX ) ) ) );
	b3Vec3 perpAxisY =
		b3MulSV( 0.5f, b3RotateVector( quatA, b3Add( b3MulSV( relQ.s, b3Vec3_axisY ), b3Cross( relQ.v, b3Vec3_axisY ) ) ) );

	b3Matrix3 invInertiaSum = b3AddMM( base->invIA, base->invIB );
	float kxx = b3Dot( perpAxisX, b3MulMV( invInertiaSum, perpAxisX ) );
	float kyy = b3Dot( perpAxisY, b3MulMV( invInertiaSum, perpAxisY ) );
	float kxy = b3Dot( perpAxisX, b3MulMV( invInertiaSum, perpAxisY ) );

	b3Matrix2 k = { { kxx, kxy }, { kxy, kyy } };
	float det = b3Det2( k );
	pose->solvable = det > 1000.0f * FLT_MIN;
	float invDet = pose->solvable ? 1.0f / det : 0.0f;
	pose->xx = invDet * k.cy.y;
	pose->xy = invDet * k.cy.x;
	pose->yx = -invDet * k.cx.y;
	pose->yy = invDet * k.cx.x;
	pose->perpAxisX = perpAxisX;
	pose->perpAxisY = perpAxisY;
}

// Added for Nebenan: the rest of b3Solve2 for the effective mass of an axes pose
static inline b3Vec2 b3SolveAxesPose( const b3AxesPose* pose, b3Vec2 b )
{
	if ( pose->solvable )
	{
		return B3_LITERAL( b3Vec2 ){
			pose->xx * b.x - pose->xy * b.y,
			pose->yx * b.x + pose->yy * b.y,
		};
	}

	return B3_LITERAL( b3Vec2 ){ 0.0f, 0.0f };
}

void b3DestroyJointInternal( b3World* world, b3Joint* joint, bool wakeBodies );

b3Joint* b3GetJointFullId( b3World* world, b3JointId jointId );
b3JointSim* b3GetJointSim( b3World* world, b3Joint* joint );
b3JointSim* b3GetJointSimCheckType( b3JointId jointId, b3JointType type );

void b3PrepareJoint( b3JointSim* joint, b3StepContext* context );
void b3WarmStartJoint( b3JointSim* joint, b3StepContext* context );
void b3SolveJoint( b3JointSim* joint, b3StepContext* context, bool useBias );

void b3PrepareJoints_Overflow( b3StepContext* context );
void b3WarmStartJoints_Overflow( b3StepContext* context );
void b3SolveJoints_Overflow( b3StepContext* context, bool useBias );

void b3GetJointReaction( b3World* world, b3JointSim* sim, float invTimeStep, float* force, float* torque );

void b3DrawJoint( b3DebugDraw* draw, b3World* world, b3Joint* joint );

b3Vec3 b3GetDistanceJointForce( b3World* world, b3JointSim* base );
b3Vec3 b3GetMotorJointForce( b3World* world, b3JointSim* base );
b3Vec3 b3GetPrismaticJointForce( b3World* world, b3JointSim* base );
b3Vec3 b3GetRevoluteJointForce( b3World* world, b3JointSim* base );
b3Vec3 b3GetSphericalJointForce( b3World* world, b3JointSim* base );
b3Vec3 b3GetWeldJointForce( b3World* world, b3JointSim* base );
b3Vec3 b3GetWheelJointForce( b3World* world, b3JointSim* base );

b3Vec3 b3GetMotorJointTorque( b3World* world, b3JointSim* base );
b3Vec3 b3GetParallelJointTorque( b3World* world, b3JointSim* base );
b3Vec3 b3GetPrismaticJointTorque( b3World* world, b3JointSim* base );
b3Vec3 b3GetRevoluteJointTorque( b3World* world, b3JointSim* base );
b3Vec3 b3GetSphericalJointTorque( b3World* world, b3JointSim* base );
b3Vec3 b3GetWeldJointTorque( b3World* world, b3JointSim* base );
b3Vec3 b3GetWheelJointTorque( b3World* world, b3JointSim* base );

void b3PrepareDistanceJoint( b3JointSim* base, b3StepContext* context );
void b3PrepareMotorJoint( b3JointSim* base, b3StepContext* context );
void b3PrepareParallelJoint( b3JointSim* base, b3StepContext* context );
void b3PreparePrismaticJoint( b3JointSim* base, b3StepContext* context );
void b3PrepareRevoluteJoint( b3JointSim* base, b3StepContext* context );
void b3PrepareSphericalJoint( b3JointSim* base, b3StepContext* context );
void b3PrepareWeldJoint( b3JointSim* base, b3StepContext* context );
void b3PrepareWheelJoint( b3JointSim* base, b3StepContext* context );

void b3WarmStartDistanceJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartMotorJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartParallelJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartPrismaticJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartRevoluteJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartSphericalJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartWeldJoint( b3JointSim* base, b3StepContext* context );
void b3WarmStartWheelJoint( b3JointSim* base, b3StepContext* context );

void b3SolveDistanceJoint( b3JointSim* base, b3StepContext* context, bool useBias );
void b3SolveMotorJoint( b3JointSim* base, b3StepContext* context );
void b3SolveParallelJoint( b3JointSim* base, b3StepContext* context, bool useBias );
void b3SolvePrismaticJoint( b3JointSim* base, b3StepContext* context, bool useBias );
void b3SolveRevoluteJoint( b3JointSim* base, b3StepContext* context, bool useBias );
void b3SolveSphericalJoint( b3JointSim* base, b3StepContext* context, bool useBias );
void b3SolveWeldJoint( b3JointSim* base, b3StepContext* context, bool useBias );
void b3SolveWheelJoint( b3JointSim* base, b3StepContext* context, bool useBias );

void b3DrawDistanceJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB );
void b3DrawParallelJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB, float scale );
void b3DrawPrismaticJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB, float scale );
void b3DrawRevoluteJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB, float scale );
void b3DrawSphericalJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB, float scale );
void b3DrawWeldJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB, float scale );
void b3DrawWheelJoint( b3DebugDraw* draw, b3JointSim* base, b3WorldTransform transformA, b3WorldTransform transformB, float scale );
