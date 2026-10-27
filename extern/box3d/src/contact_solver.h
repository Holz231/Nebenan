// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "math_internal.h"
#include "solver.h"

typedef struct b3ManifoldConstraintPoint
{
	b3Vec3 rA, rB;
	float baseSeparation;
	float relativeVelocity;
	float normalImpulse;
	float totalNormalImpulse;
	float restitutionImpulse;
	float normalMass;
	float leverArm;
} b3ManifoldConstraintPoint;

typedef struct b3ManifoldConstraint
{
	b3ManifoldConstraintPoint points[B3_MAX_MANIFOLD_POINTS];
	int pointCount;
	b3Vec3 normal;
	b3Vec3 tangent1;
	b3Vec3 tangent2;
	// Friction centers
	b3Vec3 centerA, centerB;
	float twistMass;
	float twistImpulse;
	b3Matrix2 tangentMass;
	b3Vec2 frictionImpulse;
	b3Vec3 rollingImpulse;
	float tangentVelocity1;
	float tangentVelocity2;
} b3ManifoldConstraint;

typedef struct b3ContactConstraint
{
	b3ManifoldConstraint* constraints;
	struct b3Contact* contact;
	int indexA;
	int indexB;
	float invMassA, invMassB;
	b3Matrix3 invIA, invIB;
	b3Softness softness;
	b3Matrix3 rollingMass;
	float friction;
	float restitution;
	float rollingResistance;
	int manifoldCount;
} b3ContactConstraint;

// Added for Nebenan: the size of a wide convex contact constraint in the given width, see b3World::simdWidth
int b3GetWideContactConstraintByteCount( int simdWidth );

// Overflow contacts don't fit into the constraint graph coloring
void b3PrepareContacts_Overflow( b3StepContext* context );
void b3WarmStartContacts_Overflow( b3StepContext* context );
void b3PushContacts_Overflow( b3StepContext* context );
void b3SolveContacts_Overflow( b3StepContext* context );
void b3ApplyRestitution_Overflow( b3StepContext* context );
void b3StoreImpulses_Overflow( b3StepContext* context );

void b3PrepareContacts_Mesh( b3SolverBlock block, b3StepContext* context );
void b3WarmStartContacts_Mesh( b3SolverBlock block, b3StepContext* context );
void b3PushContacts_Mesh( b3SolverBlock block, b3StepContext* context );
void b3SolveContacts_Mesh( b3SolverBlock block, b3StepContext* context );
void b3ApplyRestitution_Mesh( b3SolverBlock block, b3StepContext* context );
void b3StoreImpulses_Mesh( b3SolverBlock block, b3StepContext* context, int workerIndex );

void b3PrepareContacts_Convex( b3SolverBlock block, b3StepContext* context );
void b3PreparePackContacts_Convex( b3StepContext* context, int pack );
void b3WarmStartContacts_Convex( b3SolverBlock block, b3StepContext* context );
void b3PushContacts_Convex( b3SolverBlock block, b3StepContext* context );
void b3SolveContacts_Convex( b3SolverBlock block, b3StepContext* context );
void b3ApplyRestitution_Convex( b3SolverBlock block, b3StepContext* context );
void b3StoreImpulses_Convex( b3SolverBlock block, b3StepContext* context, int workerIndex );

// Added for Nebenan: the batches of knots in [startBatch, endBatch), see b3KnotBatch. They are prepared and stored with
// the convex contacts.
void b3WarmStartKnots( b3StepContext* context, int startBatch, int endBatch );
void b3PushKnots( b3StepContext* context, int startBatch, int endBatch );
void b3SolveKnots( b3StepContext* context, int startBatch, int endBatch );
void b3ApplyKnotRestitution( b3StepContext* context, int startBatch, int endBatch );

// Added for Nebenan: one pass of the wide constraints of a domain, [start, end) of all wide constraints, see b3Domains
void b3SolveDomainContacts( b3SolverStageType type, b3StepContext* context, int start, int end );

// Added for Nebenan: the functions above pick these by the width of the world, see contact_solver_wide.inl
int b3GetWideContactConstraintByteCountW4( void );
void b3PrepareContacts_ConvexW4( b3SolverBlock block, b3StepContext* context );
void b3PreparePackContacts_ConvexW4( b3StepContext* context, int pack );
void b3WarmStartContacts_ConvexW4( b3SolverBlock block, b3StepContext* context );
void b3PushContacts_ConvexW4( b3SolverBlock block, b3StepContext* context );
void b3SolveContacts_ConvexW4( b3SolverBlock block, b3StepContext* context );
void b3ApplyRestitution_ConvexW4( b3SolverBlock block, b3StepContext* context );
void b3StoreImpulses_ConvexW4( b3SolverBlock block, b3StepContext* context, int workerIndex );
void b3WarmStartKnotsW4( b3StepContext* context, int startBatch, int endBatch );
void b3PushKnotsW4( b3StepContext* context, int startBatch, int endBatch );
void b3SolveKnotsW4( b3StepContext* context, int startBatch, int endBatch );
void b3ApplyKnotRestitutionW4( b3StepContext* context, int startBatch, int endBatch );
void b3SolveDomainContactsW4( b3SolverStageType type, b3StepContext* context, int start, int end );
#if defined( B3_SIMD_HAS_WIDTH_8 )
int b3GetWideContactConstraintByteCountW8( void );
void b3PrepareContacts_ConvexW8( b3SolverBlock block, b3StepContext* context );
void b3PreparePackContacts_ConvexW8( b3StepContext* context, int pack );
void b3WarmStartContacts_ConvexW8( b3SolverBlock block, b3StepContext* context );
void b3PushContacts_ConvexW8( b3SolverBlock block, b3StepContext* context );
void b3SolveContacts_ConvexW8( b3SolverBlock block, b3StepContext* context );
void b3ApplyRestitution_ConvexW8( b3SolverBlock block, b3StepContext* context );
void b3StoreImpulses_ConvexW8( b3SolverBlock block, b3StepContext* context, int workerIndex );
void b3WarmStartKnotsW8( b3StepContext* context, int startBatch, int endBatch );
void b3PushKnotsW8( b3StepContext* context, int startBatch, int endBatch );
void b3SolveKnotsW8( b3StepContext* context, int startBatch, int endBatch );
void b3ApplyKnotRestitutionW8( b3StepContext* context, int startBatch, int endBatch );
void b3SolveDomainContactsW8( b3SolverStageType type, b3StepContext* context, int start, int end );
#endif

// Added for Nebenan: the copies of a knot in a batch, its velocity before the batch and the sum of the changes of its
// copies, see b3KnotBatch
typedef struct b3KnotCopies
{
	b3Vec3 v0, w0;
	b3Vec3 dv, dw;
} b3KnotCopies;
