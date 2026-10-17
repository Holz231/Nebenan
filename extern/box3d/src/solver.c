// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#include "solver.h"

#include "arena_allocator.h"
#include "bitset.h"
#include "body.h"
#include "compound.h"
#include "contact.h"
#include "contact_solver.h"
#include "core.h"
#include "ctz.h"
#include "island.h"
#include "joint.h"
#include "parallel_for.h"
#include "physics_world.h"
#include "platform.h"
#include "sensor.h"
#include "shape.h"
#include "solver_set.h"

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

// these are useful for solver testing
#define ITERATIONS 1
#define RELAX_ITERATIONS 1

#if ( defined( __GNUC__ ) || defined( __clang__ ) ) && ( defined( __i386__ ) || defined( __x86_64__ ) )
static void b3Pause( void )
{
	__asm__ __volatile__( "pause\n" );
}
#elif ( defined( __arm__ ) && defined( __ARM_ARCH ) && __ARM_ARCH >= 7 ) || defined( __aarch64__ )
static void b3Pause( void )
{
	__asm__ __volatile__( "yield" ::: "memory" );
}
#elif defined( _MSC_VER ) && ( defined( _M_IX86 ) || defined( _M_X64 ) )
static void b3Pause( void )
{
	_mm_pause();
}
#elif defined( _MSC_VER ) && ( defined( _M_ARM ) || defined( _M_ARM64 ) )
static void b3Pause( void )
{
	__yield();
}
#else
static void b3Pause( void )
{
}
#endif

typedef struct b3WorkerContext
{
	b3StepContext* context;
	int workerIndex;
	void* userTask;
} b3WorkerContext;

// Integrate velocities, apply damping, and gyroscopic torque
static void b3IntegrateVelocitiesTask( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneNC( integrate_velocity, "IntVel", b3_colorDeepPink, true );

	B3_VALIDATE( block.startIndex + block.count <= context->world->solverSets.data[b3_awakeSet].bodyStates.count );

	b3BodyState* states = context->states;
	b3BodySim* sims = context->sims;

	b3Vec3 gravity = context->world->gravity;
	float h = context->h;

	// Added for Nebenan: the bodies of a pack of islands come as a list of awake indices, see b3SolvePacks
	const int* bodyIndices = block.blockType == b3_packBodyBlock ? context->packBodies : NULL;

	for ( int k = block.startIndex; k < block.startIndex + block.count; ++k )
	{
		int i = bodyIndices != NULL ? bodyIndices[k] : k;
		b3BodySim* sim = sims + i;
		b3BodyState* state = states + i;

		b3Vec3 v = state->linearVelocity;
		b3Vec3 w = state->angularVelocity;

		// Damping math
		// Differential equation: dv/dt + c * v = 0
		// Solution: v(t) = v0 * exp(-c * t)
		// Time step: v(t + dt) = v0 * exp(-c * (t + dt)) = v0 * exp(-c * t) * exp(-c * dt) = v(t) * exp(-c * dt)
		// v2 = exp(-c * dt) * v1
		// Pade approximation:
		// v2 = v1 * 1 / (1 + c * dt)
		float linearDamping = 1.0f / ( 1.0f + h * sim->linearDamping );
		float angularDamping = 1.0f / ( 1.0f + h * sim->angularDamping );

		// Gravity scale will be zero for kinematic bodies
		float gravityScale = sim->invMass > 0.0f ? sim->gravityScale : 0.0f;

		b3Vec3 linearVelocityDelta = b3Blend2( h * sim->invMass, sim->force, h * gravityScale, gravity );
		v = b3MulAdd( linearVelocityDelta, linearDamping, v );

		b3Vec3 angularVelocityDelta = b3MulSV( h, b3MulMV( sim->invInertiaWorld, sim->torque ) );
		w = b3MulAdd( angularVelocityDelta, angularDamping, w );

		// Gyroscopic torque by solving this nonlinear equation using Newton-Raphson.
		// I * (w2 - w1) + h * cross(w2, I * w2) = 0
		// This is all done in local coordinates where the Jacobian is easier to compute.
		// This improves the simulation of long skinny bodies.
		{
			// Get current rotation.
			b3Quat q0 = sim->transform.q;
			b3Quat q = b3MulQuat( state->deltaRotation, q0 );

			// todo wasteful computation
			b3Matrix3 inertiaLocal = b3InvertMatrix( sim->invInertiaLocal );

			// Compute local angular velocity
			b3Vec3 omega1 = b3InvRotateVector( q, w );
			b3Vec3 omega2 = omega1;

			// Symmetric inertia tensor: 6 unique entries (column-major)
			float i00 = inertiaLocal.cx.x;
			float i01 = inertiaLocal.cy.x;
			float i02 = inertiaLocal.cz.x;
			float i11 = inertiaLocal.cy.y;
			float i12 = inertiaLocal.cz.y;
			float i22 = inertiaLocal.cz.z;

			for ( int gyroIteration = 0; gyroIteration < B3_GYROSCOPIC_ITERATIONS; ++gyroIteration )
			{
				float w1 = omega2.x;
				float w2 = omega2.y;
				float w3 = omega2.z;

				// Iw = I * omega2 (shared between residual and Jacobian)
				float Iw1 = i00 * w1 + i01 * w2 + i02 * w3;
				float Iw2 = i01 * w1 + i11 * w2 + i12 * w3;
				float Iw3 = i02 * w1 + i12 * w2 + i22 * w3;

				// Residual: b = I * (omega2 - omega1) + h * cross(omega2, I * omega2)
				b3Vec3 dw = b3Sub( omega2, omega1 );
				b3Vec3 b = {
					i00 * dw.x + i01 * dw.y + i02 * dw.z + h * ( w2 * Iw3 - w3 * Iw2 ),
					i01 * dw.x + i11 * dw.y + i12 * dw.z + h * ( w3 * Iw1 - w1 * Iw3 ),
					i02 * dw.x + i12 * dw.y + i22 * dw.z + h * ( w1 * Iw2 - w2 * Iw1 ),
				};

				// Jacobian J = I + h * (skew(omega2) * I - skew(I * omega2))
				// Jacobian derived by Erin Catto, Ph.D. Do not attempt to do this without a Ph.D.
				// Doubled inertia terms above fold into Iw, e.g. row 2 col 1: i00*w3 - i02*w1 - Iw3.
				b3Matrix3 J = {
					{ i00 + h * ( w2 * i02 - w3 * i01 ), i01 + h * ( w3 * i00 - w1 * i02 - Iw3 ),
					  i02 + h * ( w1 * i01 - w2 * i00 + Iw2 ) },
					{ i01 + h * ( w2 * i12 - w3 * i11 + Iw3 ), i11 + h * ( w3 * i01 - w1 * i12 ),
					  i12 + h * ( w1 * i11 - w2 * i01 - Iw1 ) },
					{ i02 + h * ( w2 * i22 - w3 * i12 - Iw2 ), i12 + h * ( w3 * i02 - w1 * i22 + Iw1 ),
					  i22 + h * ( w1 * i12 - w2 * i02 ) },
				};

				omega2 = b3Sub( omega2, b3Solve3( J, b ) );
			}

			w = b3RotateVector( q, omega2 );
		}

		state->linearVelocity = v;
		state->angularVelocity = w;
	}

	b3TracyCZoneEnd( integrate_velocity );
}

static void b3IntegratePositionsTask( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneNC( integrate_positions, "IntPos", b3_colorDarkSeaGreen, true );

	B3_VALIDATE( block.startIndex + block.count <= context->world->solverSets.data[b3_awakeSet].bodyStates.count );

	b3BodyState* states = context->states;
	float h = context->h;
	float maxLinearSpeed = context->maxLinearVelocity;
	float maxAngularSpeed = B3_MAX_ROTATION * context->inv_dt;
	float maxLinearSpeedSquared = maxLinearSpeed * maxLinearSpeed;
	float maxAngularSpeedSquared = maxAngularSpeed * maxAngularSpeed;

	// Added for Nebenan: the bodies of a pack of islands come as a list of awake indices, see b3SolvePacks
	const int* bodyIndices = block.blockType == b3_packBodyBlock ? context->packBodies : NULL;

	for ( int k = block.startIndex; k < block.startIndex + block.count; ++k )
	{
		int i = bodyIndices != NULL ? bodyIndices[k] : k;
		b3BodyState* state = states + i;

		b3Vec3 v = state->linearVelocity;
		b3Vec3 w = state->angularVelocity;

		// Motion locks - these can be viewed as a constraint that come last
		v.x = ( state->flags & b3_lockLinearX ) ? 0.0f : v.x;
		v.y = ( state->flags & b3_lockLinearY ) ? 0.0f : v.y;
		v.z = ( state->flags & b3_lockLinearZ ) ? 0.0f : v.z;
		w.x = ( state->flags & b3_lockAngularX ) ? 0.0f : w.x;
		w.y = ( state->flags & b3_lockAngularY ) ? 0.0f : w.y;
		w.z = ( state->flags & b3_lockAngularZ ) ? 0.0f : w.z;

		// Clamp to max linear speed
		if ( b3Dot( v, v ) > maxLinearSpeedSquared )
		{
			float ratio = maxLinearSpeed / b3Length( v );
			v = b3MulSV( ratio, v );
			state->flags |= b3_isSpeedCapped;
		}

		// Clamp to max angular speed
		if ( b3Dot( w, w ) > maxAngularSpeedSquared && ( state->flags & b3_allowFastRotation ) == 0 )
		{
			float ratio = maxAngularSpeed / b3Length( w );
			w = b3MulSV( ratio, w );
			state->flags |= b3_isSpeedCapped;
		}

		state->linearVelocity = v;
		state->angularVelocity = w;
		state->deltaPosition = b3MulAdd( state->deltaPosition, h, v );
		state->deltaRotation = b3IntegrateRotation( state->deltaRotation, b3MulSV( h, w ) );
	}

	b3TracyCZoneEnd( integrate_positions );
}

static void b3PrepareJointsTask( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneNC( prepare_joints, "PrepJoints", b3_colorOldLace, true );

	b3JointPrepareSpan* spans = context->jointPrepareSpans;

	int index = block.startIndex;
	int endIndex = block.startIndex + block.count;

	// Find color for start index. Linear search but fast.
	int colorIndex = 0;
	while ( spans[colorIndex + 1].start <= index )
	{
		colorIndex += 1;
	}

	// Loop over block
	while ( index < endIndex )
	{
		int colorStart = spans[colorIndex].start;
		int colorEndIndex = b3MinInt( spans[colorIndex + 1].start, endIndex );
		b3JointSim* joints = spans[colorIndex].joints;

		// Loop over color
		for ( ; index < colorEndIndex; ++index )
		{
			B3_ASSERT( 0 <= index - colorStart && index - colorStart < spans[colorIndex].count );
			b3JointSim* joint = joints + ( index - colorStart );
			b3PrepareJoint( joint, context );
		}

		// Advance to next color
		colorIndex += 1;
	}

	b3TracyCZoneEnd( prepare_joints );
}

static void b3WarmStartJointsTask( b3SolverBlock block, b3StepContext* context )
{
	b3TracyCZoneNC( warm_joints, "WarmJoints", b3_colorGold, true );

	b3GraphColor* color = context->graph->colors + block.colorIndex;
	b3JointSim* joints = color->jointSims.data;

	for ( int i = block.startIndex; i < block.startIndex + block.count; ++i )
	{
		b3JointSim* joint = joints + i;
		b3WarmStartJoint( joint, context );
	}

	b3TracyCZoneEnd( warm_joints );
}

static void b3SolveJointsTask( b3SolverBlock block, b3StepContext* context, bool useBias, int workerIndex )
{
	b3TracyCZoneNC( solve_joints, "SolveJoints", b3_colorLemonChiffon, true );

	b3GraphColor* color = context->graph->colors + block.colorIndex;
	b3JointSim* joints = color->jointSims.data;

	B3_ASSERT( 0 <= block.startIndex && block.startIndex + block.count <= color->jointSims.count );

	b3BitSet* jointStateBitSet = &context->world->taskContexts.data[workerIndex].jointStateBitSet;

	for ( int i = block.startIndex; i < block.startIndex + block.count; ++i )
	{
		b3JointSim* joint = joints + i;
		b3SolveJoint( joint, context, useBias );

		if ( useBias && ( joint->forceThreshold < FLT_MAX || joint->torqueThreshold < FLT_MAX ) &&
			 b3GetBit( jointStateBitSet, joint->jointId ) == false )
		{
			float force, torque;
			b3GetJointReaction( context->world, joint, context->inv_h, &force, &torque );

			// Check thresholds. A zero threshold means all awake joints get reported.
			if ( force >= joint->forceThreshold || torque >= joint->torqueThreshold )
			{
				// Flag this joint for processing.
				b3SetBit( jointStateBitSet, joint->jointId );
			}
		}
	}

	b3TracyCZoneEnd( solve_joints );
}

#define B2_MAX_CONTINUOUS_SENSOR_HITS 8

typedef struct b3ContinuousContext
{
	b3World* world;
	b3BodySim* fastBodySim;
	b3Shape* fastShape;
	b3Vec3 centroid1, centroid2;
	b3Sweep sweep;
	// World base for re-centering sweeps. Keeps TOI in float precision far from the origin.
	b3Pos base;
	float fraction;
	b3SensorHit sensorHits[B2_MAX_CONTINUOUS_SENSOR_HITS];
	float sensorFractions[B2_MAX_CONTINUOUS_SENSOR_HITS];
	int sensorCount;

	// Added for Nebenan: how far the fast shape reaches from the center of mass of its body, see b3IsBeyondHullFace
	float fastReach;

	int distanceIterations;
	int pushBackIterations;
	int rootIterations;
} b3ContinuousContext;

// Added for Nebenan: the radius of the sphere around the center of mass of a body that holds one of its shapes in every
// pose, or FLT_MAX for a shape that has no such sphere here
static float b3GetShapeReach( const b3Shape* shape, b3Vec3 localCenter )
{
	switch ( shape->type )
	{
		case b3_sphereShape:
			return b3Distance( shape->sphere.center, localCenter ) + shape->sphere.radius;

		case b3_capsuleShape:
			return b3MaxFloat( b3Distance( shape->capsule.center1, localCenter ),
							   b3Distance( shape->capsule.center2, localCenter ) ) +
				   shape->capsule.radius;

		case b3_hullShape:
		{
			const b3HullData* hull = shape->hull;
			const b3Vec3* points = b3GetHullPoints( hull );
			float reach = 0.0f;
			for ( int i = 0; i < hull->vertexCount; ++i )
			{
				reach = b3MaxFloat( reach, b3Distance( points[i], localCenter ) );
			}
			return reach;
		}

		default:
			return FLT_MAX;
	}
}

// Added for Nebenan: whether the fast shape stays beyond one face plane of a static hull for the whole step. The sweep moves
// the center of mass along a line, and the shape stays within its reach around it. If that sphere lies beyond the same plane
// at both ends of the step, it lies beyond it in between, and the time of impact cannot find the shapes any closer than
// its target distance: It would return no hit. The margin covers the target distance and its tolerance several times over.
static bool b3IsBeyondHullFace( const b3ContinuousContext* context, const b3HullData* hull, b3WorldTransform transform )
{
	float reach = context->fastReach + 4.0f * B3_LINEAR_SLOP;
	b3Vec3 center1 = b3InvTransformWorldPoint( transform, b3OffsetPos( context->base, context->sweep.c1 ) );
	b3Vec3 center2 = b3InvTransformWorldPoint( transform, b3OffsetPos( context->base, context->sweep.c2 ) );
	const b3Plane* planes = b3GetHullPlanes( hull );
	for ( int i = 0; i < hull->faceCount; ++i )
	{
		if ( b3PlaneSeparation( planes[i], center1 ) > reach && b3PlaneSeparation( planes[i], center2 ) > reach )
		{
			return true;
		}
	}

	return false;
}

// This is called from b3DynamicTree_Query for continuous collision
static bool b3ContinuousQueryCallback( int proxyId, uint64_t userData, void* context )
{
	B3_UNUSED( proxyId );

	int shapeId = (int)userData;
	b3ContinuousContext* continuousContext = context;

	b3Shape* fastShape = continuousContext->fastShape;
	b3BodySim* fastBodySim = continuousContext->fastBodySim;

	B3_ASSERT( fastShape->sensorIndex == B3_NULL_INDEX );

	// Skip same shape
	if ( shapeId == fastShape->id )
	{
		return true;
	}

	b3World* world = continuousContext->world;
	b3Shape* shape = b3Array_Get( world->shapes, shapeId );

	// Skip same body
	if ( shape->bodyId == fastShape->bodyId )
	{
		return true;
	}

	// Skip sensors unless both shapes want sensor events
	bool isSensor = shape->sensorIndex != B3_NULL_INDEX;
	if ( isSensor && ( ( shape->flags & b3_enableSensorEvents ) == 0 || ( fastShape->flags & b3_enableSensorEvents ) == 0 ) )
	{
		return true;
	}

	// Skip filtered shapes
	bool canCollide = b3ShouldShapesCollide( fastShape->filter, shape->filter );
	if ( canCollide == false )
	{
		return true;
	}

	b3Body* body = b3Array_Get( world->bodies, shape->bodyId );

	b3BodySim* bodySim = b3GetBodySim( world, body );
	B3_ASSERT( body->type == b3_staticBody || ( fastBodySim->flags & b3_isBullet ) );

	// Skip bullets
	if ( body->flags & b3_isBullet )
	{
		return true;
	}

	// Skip filtered bodies
	b3Body* fastBody = b3Array_Get( world->bodies, fastBodySim->bodyId );
	canCollide = b3ShouldBodiesCollide( world, fastBody, body );
	if ( canCollide == false )
	{
		return true;
	}

	// Custom user filtering
	if ( ( shape->flags & b3_enableCustomFiltering ) != 0 || ( fastShape->flags & b3_enableCustomFiltering ) != 0 )
	{
		b3CustomFilterFcn* customFilterFcn = world->customFilterFcn;
		if ( customFilterFcn != NULL )
		{
			b3ShapeId idA = { shape->id + 1, world->worldId, shape->generation };
			b3ShapeId idB = { fastShape->id + 1, world->worldId, fastShape->generation };
			canCollide = customFilterFcn( idA, idB, world->customFilterContext );
			if ( canCollide == false )
			{
				return true;
			}
		}
	}

	// Added for Nebenan: a static hull the fast shape cannot reach needs no time of impact. At scale 1 under fire this skips
	// about half of the tests, and the results stay bit-identical.
	if ( isSensor == false && body->type == b3_staticBody && shape->type == b3_hullShape &&
		 b3IsBeyondHullFace( continuousContext, shape->hull, bodySim->transform ) )
	{
		return true;
	}

	// todo does having a sweep on shapeA help with bullets?
	b3Sweep sweepA = b3MakeRelativeSweep( bodySim, continuousContext->base );

	// Time of impact versus shape. Supports all shape types
	// Added for Nebenan: over the whole step, not only up to the earliest impact so far. The query hands out the shapes
	// in the order of its tree, and a shorter interval moves the root the iteration settles on, so the result would
	// depend on how the shapes went into the tree. Costs nothing measurable.
	b3TOIOutput output = b3ShapeTimeOfImpact( shape, fastShape, &sweepA, &continuousContext->sweep, 1.0f );
	if ( isSensor )
	{
		// Only accept a sensor hit that is sooner than the current solid hit.
		if ( output.fraction <= continuousContext->fraction && continuousContext->sensorCount < B2_MAX_CONTINUOUS_SENSOR_HITS )
		{
			int index = continuousContext->sensorCount;

			// The hit shape is a sensor
			b3SensorHit sensorHit = {
				.sensorId = shape->id,
				.visitorId = fastShape->id,
			};

			continuousContext->sensorHits[index] = sensorHit;
			continuousContext->sensorFractions[index] = output.fraction;
			continuousContext->sensorCount += 1;
		}
	}
	else if ( 0.0f < output.fraction && output.fraction < continuousContext->fraction )
	{
		bool didHit = true;

		if ( didHit && ( ( shape->flags & b3_enablePreSolveEvents ) || ( fastShape->flags & b3_enablePreSolveEvents ) ) )
		{
			b3ShapeId shapeIdA = { shape->id + 1, world->worldId, shape->generation };
			b3ShapeId shapeIdB = { fastShape->id + 1, world->worldId, fastShape->generation };
			b3Pos point = b3OffsetPos( continuousContext->base, output.point );
			didHit = world->preSolveFcn( shapeIdA, shapeIdB, point, output.normal, world->preSolveContext );
		}

		if ( didHit )
		{
			continuousContext->fraction = output.fraction;
			continuousContext->distanceIterations = b3MaxInt( continuousContext->distanceIterations, output.distanceIterations );
			continuousContext->pushBackIterations = b3MaxInt( continuousContext->pushBackIterations, output.pushBackIterations );
			continuousContext->rootIterations = b3MaxInt( continuousContext->rootIterations, output.rootIterations );
		}
	}

	// Added for Nebenan: no stall warning here and in b3SolveContinuous, as upstream since #179. Reading the clock twice per
	// test cost more than the warning was worth.

	// Continue query
	return true;
}

// Continuous collision of dynamic versus static
static void b3SolveContinuous( b3World* world, int bodySimIndex, b3TaskContext* taskContext, float dt )
{
	b3TracyCZoneNC( ccd, "CCD", b3_colorDarkGoldenRod, true );

	b3SolverSet* awakeSet = b3Array_Get( world->solverSets, b3_awakeSet );
	b3BodySim* fastBodySim = b3Array_Get( awakeSet->bodySims, bodySimIndex );
	B3_VALIDATE( fastBodySim->flags & b3_isFast );

	// Re-center the sweep on the fast body so the TOI and the swept query stay in float precision
	b3Pos base = fastBodySim->center0;

	b3Sweep sweep = b3MakeRelativeSweep( fastBodySim, base );

	b3Transform xf1;
	xf1.q = sweep.q1;
	xf1.p = b3Sub( sweep.c1, b3RotateVector( sweep.q1, sweep.localCenter ) );

	b3Transform xf2;
	xf2.q = sweep.q2;
	xf2.p = b3Sub( sweep.c2, b3RotateVector( sweep.q2, sweep.localCenter ) );

	b3DynamicTree* staticTree = world->broadPhase.trees + b3_staticBody;
	b3DynamicTree* kinematicTree = world->broadPhase.trees + b3_kinematicBody;
	b3DynamicTree* dynamicTree = world->broadPhase.trees + b3_dynamicBody;
	b3Body* fastBody = b3Array_Get( world->bodies, fastBodySim->bodyId );

	b3ContinuousContext context = { 0 };
	context.world = world;
	context.sweep = sweep;
	context.base = base;
	context.fastBodySim = fastBodySim;
	context.fraction = 1.0f;

	bool isBullet = ( fastBodySim->flags & b3_isBullet ) != 0;

	int shapeId = fastBody->headShapeId;
	while ( shapeId != B3_NULL_INDEX )
	{
		b3Shape* fastShape = b3Array_Get( world->shapes, shapeId );
		shapeId = fastShape->nextShapeId;

		context.fastShape = fastShape;
		context.centroid1 = b3TransformPoint( xf1, fastShape->localCentroid );
		context.centroid2 = b3TransformPoint( xf2, fastShape->localCentroid );

		b3AABB box1 = fastShape->aabb;
		// xf2 is relative to the base, so translate the box back to world space, rounding outward
		b3AABB box2 = b3OffsetAABB( b3ComputeShapeAABB( fastShape, xf2 ), base );

		// Store this to avoid double computation in the case there is no impact event
		fastShape->aabb = box2;

		// No continuous collision for meshes
		if ( fastShape->type == b3_meshShape || fastShape->type == b3_heightShape )
		{
			continue;
		}

		// No continuous collision for sensors
		if ( fastShape->sensorIndex != B3_NULL_INDEX )
		{
			continue;
		}

		b3AABB sweptBox = b3AABB_Union( box1, box2 );
		context.fastReach = b3GetShapeReach( fastShape, sweep.localCenter ); // Added for Nebenan
		b3DynamicTree_Query( staticTree, sweptBox, B3_DEFAULT_MASK_BITS, false, b3ContinuousQueryCallback, &context );

		if ( isBullet )
		{
			b3DynamicTree_Query( kinematicTree, sweptBox, B3_DEFAULT_MASK_BITS, false, b3ContinuousQueryCallback, &context );
			b3DynamicTree_Query( dynamicTree, sweptBox, B3_DEFAULT_MASK_BITS, false, b3ContinuousQueryCallback, &context );
		}
	}

	const float speculativeScalar = B3_SPECULATIVE_DISTANCE;

	if ( context.fraction < 1.0f )
	{
		// Handle time of impact event. The sweep is relative to the base, so re-add the base
		// to return the advanced pose to world space.
		b3Quat q = b3NLerp( sweep.q1, sweep.q2, context.fraction );
		b3Vec3 c = b3Lerp( sweep.c1, sweep.c2, context.fraction );
		b3Vec3 origin = b3Sub( c, b3RotateVector( q, sweep.localCenter ) );

		// Advance body
		b3WorldTransform transform = { b3OffsetPos( base, origin ), q };
		b3Pos center = b3OffsetPos( base, c );
		fastBodySim->transform = transform;
		fastBodySim->center = center;
		fastBodySim->rotation0 = q;
		fastBodySim->center0 = center;
		fastBodySim->flags |= b3_hadTimeOfImpact;

		// Timeloss means there is a lost gravity contribution. Other forces and torques are ignored for now.
		b3BodyState* fastBodyState = b3Array_Get( awakeSet->bodyStates, bodySimIndex );
		b3Vec3 v = fastBodyState->linearVelocity;
		float timeLoss = ( 1.0f - context.fraction ) * dt;
		b3Vec3 dv = b3MulSV( -timeLoss * fastBodySim->gravityScale, world->gravity );
		dv.x = ( fastBodyState->flags & b3_lockLinearX ) ? 0.0f : dv.x;
		dv.y = ( fastBodyState->flags & b3_lockLinearY ) ? 0.0f : dv.y;
		dv.z = ( fastBodyState->flags & b3_lockLinearZ ) ? 0.0f : dv.z;
		fastBodyState->linearVelocity = b3Add( v, dv );

		// The move event was written before CCD, so correct it with the impact pose
		b3BodyMoveEvent* event = b3Array_Get( world->bodyMoveEvents, bodySimIndex );
		event->transform = fastBodySim->transform;

		// Prepare AABBs for broad-phase.
		// Even though a body is fast, it may not move much. So the AABB may not need enlargement.

		shapeId = fastBody->headShapeId;
		while ( shapeId != B3_NULL_INDEX )
		{
			b3Shape* shape = b3Array_Get( world->shapes, shapeId );

			// Must recompute aabb at the interpolated transform
			b3AABB aabb = b3ComputeFatShapeAABB( shape, transform, speculativeScalar );
			shape->aabb = aabb;

			b3AABB* shapeFatAABB = world->fatAABBs.data + shapeId;
			if ( b3AABB_Contains( *shapeFatAABB, aabb ) == false )
			{
				*shapeFatAABB = b3AABB_Inflate( aabb, shape->aabbMargin );

				// Regular bodies mark the hierarchy as moved using atomic operations.
				// Bullets are handled separately at a later stage.
				if ( isBullet == false )
				{
					b3BroadPhase_MarkProxyMoved( &world->broadPhase, shape->proxyKey, *shapeFatAABB );
				}
				else
				{
					fastBodySim->flags |= b3_enlargeBulletBounds;
				}
			}

			shapeId = shape->nextShapeId;
		}
	}
	else
	{
		// No time of impact event

		// Advance body
		fastBodySim->rotation0 = fastBodySim->transform.q;
		fastBodySim->center0 = fastBodySim->center;

		// Prepare AABBs for broad-phase
		shapeId = fastBody->headShapeId;
		while ( shapeId != B3_NULL_INDEX )
		{
			b3Shape* shape = b3Array_Get( world->shapes, shapeId );

			// shape->aabb is still valid from above

			b3AABB* shapeFatAABB = world->fatAABBs.data + shapeId;
			if ( b3AABB_Contains( *shapeFatAABB, shape->aabb ) == false )
			{
				*shapeFatAABB = b3AABB_Inflate( shape->aabb, shape->aabbMargin );

				if ( isBullet == false )
				{
					b3BroadPhase_MarkProxyMoved( &world->broadPhase, shape->proxyKey, *shapeFatAABB );
				}
				else
				{
					fastBodySim->flags |= b3_enlargeBulletBounds;
				}
			}

			shapeId = shape->nextShapeId;
		}
	}

	// Push sensor hits on the the task context for serial processing.
	for ( int i = 0; i < context.sensorCount; ++i )
	{
		// Skip any sensor hits that occurred after a solid hit
		if ( context.sensorFractions[i] < context.fraction )
		{
			b3Array_Push( taskContext->sensorHits, context.sensorHits[i] );
		}
	}

	taskContext->distanceIterations = b3MaxInt( taskContext->distanceIterations, context.distanceIterations );
	taskContext->pushBackIterations = b3MaxInt( taskContext->pushBackIterations, context.pushBackIterations );
	taskContext->rootIterations = b3MaxInt( taskContext->rootIterations, context.rootIterations );

	b3TracyCZoneEnd( ccd );
}

// Added for Nebenan: b3FinalizeBodiesTask reads every awake body's record, its island and its shapes with their boxes, all
// from scattered memory, and it waited for them most. These prefetch the record, and, with the record in the cache, the
// island and the fields of the first shape the loop reads. Forced inline, GCC 13 drops the prefetches of a helper that
// only reads.
B3_FORCE_INLINE void b3PrefetchFinalizeBody( const b3Body* body )
{
	const char* p = (const char*)body;
	b3Prefetch( p );
	b3Prefetch( p + 64 );
	b3Prefetch( p + sizeof( b3Body ) - 1 );
}

B3_FORCE_INLINE void b3PrefetchFinalizeData( const b3World* world, const b3Body* body )
{
	int shapeId = body->headShapeId;
	if ( shapeId != B3_NULL_INDEX )
	{
		const b3Shape* shape = world->shapes.data + shapeId;
		b3Prefetch( &shape->nextShapeId );
		b3Prefetch( (const char*)( &shape->aabb + 1 ) - 1 );
		b3Prefetch( &shape->hullBounds );
		b3Prefetch( (const char*)( &shape->hullBounds + 1 ) - 1 );
		const b3AABB* fatAABB = world->fatAABBs.data + shapeId;
		b3Prefetch( fatAABB );
		b3Prefetch( (const char*)( fatAABB + 1 ) - 1 );
	}

	if ( body->islandId != B3_NULL_INDEX )
	{
		b3Prefetch( world->islands.data + body->islandId );
	}
}

// Implements b3ParallelForCallback
static void b3FinalizeBodiesTask( int startIndex, int endIndex, int workerIndex, void* context )
{
	b3TracyCZoneNC( finalize_bodies, "Finalize", b3_colorMediumSeaGreen, true );

	b3StepContext* stepContext = (b3StepContext*)context;
	b3World* world = stepContext->world;
	b3Body* bodies = world->bodies.data;
	b3BodySim* sims = stepContext->sims;
	b3BodyState* states = stepContext->states;

	B3_ASSERT( endIndex <= world->bodyMoveEvents.count );

	bool enableSleep = world->enableSleep;
	bool enableContinuous = world->enableContinuous;
	float timeStep = stepContext->dt;
	float invTimeStep = stepContext->inv_dt;
	uint16_t worldId = world->worldId;

	// The body move event array has should already have the correct size
	b3BodyMoveEvent* moveEvents = world->bodyMoveEvents.data;

	b3TaskContext* taskContext = world->taskContexts.data + workerIndex;
	b3BitSet* awakeIslandBitSet = &taskContext->awakeIslandBitSet;

	const float speculativeScalar = B3_SPECULATIVE_DISTANCE;

	for ( int simIndex = startIndex; simIndex < endIndex; ++simIndex )
	{
		// Added for Nebenan: the record eight bodies ahead, the island and first shape four ahead
		if ( simIndex + 8 < endIndex )
		{
			b3PrefetchFinalizeBody( bodies + sims[simIndex + 8].bodyId );
		}

		if ( simIndex + 4 < endIndex )
		{
			b3PrefetchFinalizeData( world, bodies + sims[simIndex + 4].bodyId );
		}

		b3BodyState* state = states + simIndex;
		b3BodySim* sim = sims + simIndex;

		b3Vec3 v = state->linearVelocity;
		b3Vec3 w = state->angularVelocity;
		b3Vec3 localOmega = b3InvRotateVector( sim->transform.q, w );
		b3Vec3 localDeltaRotation = b3InvRotateVector( sim->transform.q, state->deltaRotation.v );

		if ( b3IsValidVec3( v ) == false || b3IsValidVec3( w ) == false )
		{
			const char* name = b3FindNameWithDefault( &world->names, bodies[sim->bodyId].nameId, "NULL" );
			b3Log( "unstable: %s", name );
		}

		B3_ASSERT( b3IsValidVec3( v ) );
		B3_ASSERT( b3IsValidVec3( w ) );

		sim->center = b3OffsetPos( sim->center, state->deltaPosition );
		sim->transform.q = b3NormalizeQuat( b3MulQuat( state->deltaRotation, sim->transform.q ) );

		// Use the velocity of the farthest point on the body to account for rotation.
		b3Vec3 velocityArc = b3ModifiedCross( b3Abs( localOmega ), sim->maxExtent );
		float maxVelocity = b3Length( v ) + b3Length( velocityArc );

		// Sleep needs to observe position correction as well as true velocity.
		// q = [sin(theta/2) * v, cos(theta/2)]
		// for small angles abs(theta) ~= 2 * length(sin(theta/2) * v)
		b3Vec3 rotationArc = b3ModifiedCross( b3Abs( localDeltaRotation ), sim->maxExtent );
		float maxDeltaPosition = b3Length( state->deltaPosition ) + 2.0f * b3Length( rotationArc );

		// Position correction is not as important for sleep as true velocity.
		float positionSleepFactor = 0.5f;
		float sleepVelocity = b3MaxFloat( maxVelocity, positionSleepFactor * invTimeStep * maxDeltaPosition );

		// reset state deltas
		state->deltaPosition = b3Vec3_zero;
		state->deltaRotation = b3Quat_identity;

		sim->transform.p = b3OffsetPos( sim->center, b3Neg( b3RotateVector( sim->transform.q, sim->localCenter ) ) );

		// cache miss here, however I need the shape list below
		b3Body* body = bodies + sim->bodyId;
		body->bodyMoveIndex = simIndex;
		body->sleepVelocity = sleepVelocity;

		moveEvents[simIndex].userData = body->userData;
		moveEvents[simIndex].transform = sim->transform;
		moveEvents[simIndex].bodyId = (b3BodyId){ sim->bodyId + 1, worldId, body->generation };
		moveEvents[simIndex].fellAsleep = false;

		// reset applied force and torque
		sim->force = b3Vec3_zero;
		sim->torque = b3Vec3_zero;

		// If you hit this then it means you deferred mass computation but never called b3Body_ApplyMassFromShapes
		// or b3Body_SetMassData.
		B3_ASSERT( ( body->flags & b3_dirtyMass ) == 0 );

		// Clear the transient flags (fast, speed capped, had TOI). These flags are conditionally set
		// as part of the code below.
		body->flags &= ~b3_bodyTransientFlags;
		sim->flags &= ~( b3_isFast | b3_bodyTransientFlags );

		// The body state flag knows about speed capping (used for debug draw).
		body->flags |= ( state->flags & b3_isSpeedCapped );
		state->flags &= ~b3_bodyTransientFlags;

		if ( enableSleep == false || ( body->flags & b3_enableSleep ) == 0 || sleepVelocity > body->sleepThreshold )
		{
			// Body is not sleepy
			body->sleepTime = 0.0f;

			float safetyFactor = body->safetyFactor;
			float maxMotion = b3MaxFloat( maxDeltaPosition, maxVelocity * timeStep );
			if ( body->type == b3_dynamicBody && enableContinuous && maxMotion > safetyFactor * sim->minExtent )
			{
				// This flag is used for debug draw and contact recycling.
				sim->flags |= b3_isFast;

				// Store in fast array for the continuous collision stage
				// This is deterministic because the order of TOI sweeps doesn't matter
				if ( sim->flags & b3_isBullet )
				{
					int bulletIndex = b3AtomicFetchAddInt( &stepContext->bulletBodyCount, 1 );
					stepContext->bulletBodies[bulletIndex] = simIndex;
				}
				else
				{
					b3SolveContinuous( world, simIndex, taskContext, timeStep );
				}
			}
			else
			{
				// Body is safe to advance
				sim->center0 = sim->center;
				sim->rotation0 = sim->transform.q;
			}
		}
		else
		{
			// Body is safe to advance and is falling asleep
			sim->center0 = sim->center;
			sim->rotation0 = sim->transform.q;
			body->sleepTime += timeStep;
		}

		// Update world space inverse inertia tensor.
		b3Matrix3 rotationMatrix = b3MakeMatrixFromQuat( sim->transform.q );
		sim->invInertiaWorld = b3MulMM( b3MulMM( rotationMatrix, sim->invInertiaLocal ), b3Transpose( rotationMatrix ) );

		// Any single body in an island can keep it awake
		b3Island* island = b3Array_Get( world->islands, body->islandId );
		if ( body->sleepTime < B3_TIME_TO_SLEEP )
		{
			// keep island awake
			int islandIndex = island->localIndex;
			b3SetBit( awakeIslandBitSet, islandIndex );
		}
		else if ( island->constraintRemoveCount > 0 )
		{
			// Body wants to sleep but its island needs splitting first. Track the sleepiest candidate.
			// Break sleep time ties using the island id to ensure determinism. The cross worker reduction
			// breaks ties the same way.
			if ( body->sleepTime > taskContext->splitSleepTime ||
				 ( body->sleepTime == taskContext->splitSleepTime && body->islandId > taskContext->splitIslandId ) )
			{
				// pick the sleepiest candidate
				taskContext->splitIslandId = body->islandId;
				taskContext->splitSleepTime = body->sleepTime;
			}
		}

		// Update shapes AABBs
		// For fast non-bullet bodies the AABB has already been updated in b3SolveContinuous
		// For fast bullet bodies the AABB will be updated at a later stage
		if ( ( sim->flags & b3_isFast ) == 0 )
		{
			b3WorldTransform transform = sim->transform;
			int shapeId = body->headShapeId;
			while ( shapeId != B3_NULL_INDEX )
			{
				b3Shape* shape = b3Array_Get( world->shapes, shapeId );

				b3AABB aabb = b3ComputeFatShapeAABB( shape, transform, speculativeScalar );
				shape->aabb = aabb;

				b3AABB* shapeFatAABB = world->fatAABBs.data + shapeId;
				if ( b3AABB_Contains( *shapeFatAABB, aabb ) == false )
				{
					*shapeFatAABB = b3AABB_Inflate( aabb, shape->aabbMargin );

					// Mark the hierarchy as moved using atomic operations.
					b3BroadPhase_MarkProxyMoved( &world->broadPhase, shape->proxyKey, *shapeFatAABB );
				}

				shapeId = shape->nextShapeId;
			}
		}
	}

	b3TracyCZoneEnd( finalize_bodies );
}

typedef struct b3BlockDim
{
	// number of items per block (except last block)
	int size;

	// total number of blocks
	int count;
} b3BlockDim;

// A block is a range of tasks, a start index and count as a sub-array. Each worker receives at
// most M blocks of work. The workers may receive less blocks if there is not sufficient work.
// Each block of work has a minimum number of elements (block size). This in turn may limit the
// number of blocks. If there are many elements then the block size is increased so there are
// still at most M blocks of work per worker. M is a tunable number that has two goals:
// 1. keep M small to reduce overhead
// 2. keep M large enough for other workers to be able to steal work
// The block size is a power of two to make math efficient.
static inline b3BlockDim b3ComputeBlockCount( int itemCount, int minSize, int maxBlockCount )
{
	b3BlockDim dim = { 0 };
	if ( itemCount == 0 )
	{
		return dim;
	}

	if ( itemCount <= minSize * maxBlockCount )
	{
		dim.size = minSize;
	}
	else
	{
		dim.size = ( itemCount + maxBlockCount - 1 ) / maxBlockCount;
	}

	dim.count = ( itemCount + dim.size - 1 ) / dim.size;

	B3_ASSERT( dim.count >= 1 );
	B3_ASSERT( dim.size * dim.count >= itemCount );

	return dim;
}

// Initialize solver blocks for a contiguous range of items. Computes block size internally
// from the same parameters used by b3ComputeBlockCount. The atomic claim counter is zeroed
// so workers can CAS (0, 1) on the first stage that owns these blocks.
static void b3InitBlocks( b3SyncBlock* blocks, b3BlockDim dim, int itemCount, uint8_t blockType, uint8_t colorIndex )
{
	if ( dim.count == 0 )
	{
		return;
	}

	B3_ASSERT( itemCount >= dim.count );

	// Compute the number of elements per block
	int blockSize = dim.size;

	// Simulation too big
	B3_ASSERT( blockSize <= UINT16_MAX );

	for ( int i = 0; i < dim.count; ++i )
	{
		blocks[i].block.startIndex = i * blockSize;
		blocks[i].block.count = (uint16_t)blockSize;
		blocks[i].block.blockType = blockType;
		blocks[i].block.colorIndex = colorIndex;
		b3AtomicStoreInt( &blocks[i].syncIndex, 0 );
	}

	// The last block may not be full
	blocks[dim.count - 1].block.count = (uint16_t)( itemCount - ( dim.count - 1 ) * blockSize );

	B3_VALIDATE( blocks[dim.count - 1].block.count <= blockSize );
	B3_VALIDATE( ( dim.count - 1 ) * dim.size + blocks[dim.count - 1].block.count == itemCount );
}

static inline b3SolverStage* b3InitStage( b3SolverStage* stage, b3SolverStageType type, b3SyncBlock* blocks, int blockCount,
										  uint8_t colorIndex )
{
	stage->type = type;
	stage->blocks = blocks;
	stage->blockCount = blockCount;
	stage->colorIndex = colorIndex;
	b3AtomicStoreInt( &stage->completionCount, 0 );
	return stage + 1;
}

// Initialize one stage per color for each iteration. Used for warm start, solve, relax, and restitution.
// All iterations of a given color share the same b3SyncBlock array so the per-block syncIndex
// grows monotonically across stages within that color.
static b3SolverStage* b3InitColorStages( b3SolverStage* stage, b3SolverStageType type, int iterations, int activeColorCount,
										 b3SyncBlock** colorBlocks, int* colorBlockCounts, int* activeColorIndices )
{
	for ( int j = 0; j < iterations; ++j )
	{
		for ( int i = 0; i < activeColorCount; ++i )
		{
			stage = b3InitStage( stage, type, colorBlocks[i], colorBlockCounts[i], (uint8_t)activeColorIndices[i] );
		}
	}
	return stage;
}

static void b3ExecuteBlock( b3SolverStage* stage, b3StepContext* context, b3SolverBlock block, int workerIndex )
{
	b3SolverStageType stageType = stage->type;
	b3SolverBlockType blockType = (b3SolverBlockType)block.blockType;

	switch ( stageType )
	{
		case b3_stagePrepareJoints:
			b3PrepareJointsTask( block, context );
			break;

		case b3_stagePrepareWideContacts:
			b3PrepareContacts_Convex( block, context );
			break;

		case b3_stagePrepareContacts:
			b3PrepareContacts_Mesh( block, context );
			break;

		case b3_stageIntegrateVelocities:
			b3IntegrateVelocitiesTask( block, context );
			break;

		case b3_stageWarmStart:
			if ( blockType == b3_graphJointBlock )
			{
				b3WarmStartJointsTask( block, context );
			}
			else if ( blockType == b3_graphWideContactBlock )
			{
				b3WarmStartContacts_Convex( block, context );
			}
			else
			{
				b3WarmStartContacts_Mesh( block, context );
			}
			break;

		case b3_stageSolve:
			if ( blockType == b3_graphJointBlock )
			{
				bool useBias = true;
				b3SolveJointsTask( block, context, useBias, workerIndex );
			}
			else if ( blockType == b3_graphWideContactBlock )
			{
				b3PushContacts_Convex( block, context );
			}
			else
			{
				b3PushContacts_Mesh( block, context );
			}
			break;

		case b3_stageIntegratePositions:
			b3IntegratePositionsTask( block, context );
			break;

		case b3_stageRelax:
			if ( blockType == b3_graphJointBlock )
			{
				bool useBias = false;
				b3SolveJointsTask( block, context, useBias, workerIndex );
			}
			else if ( blockType == b3_graphWideContactBlock )
			{
				b3SolveContacts_Convex( block, context );
			}
			else
			{
				b3SolveContacts_Mesh( block, context );
			}
			break;

		case b3_stageRestitution:
			if ( blockType == b3_graphWideContactBlock )
			{
				b3ApplyRestitution_Convex( block, context );
			}
			else if ( blockType == b3_graphContactBlock || blockType == b3_overflowBlock )
			{
				// Added for Nebenan: overflow blocks of a pack of islands, see b3SolvePacks
				b3ApplyRestitution_Mesh( block, context );
			}
			break;

		case b3_stageStoreWideImpulses:
			b3StoreImpulses_Convex( block, context, workerIndex );
			break;

		case b3_stageStoreImpulses:
			b3StoreImpulses_Mesh( block, context, workerIndex );
			break;
	}
}

// This staggers the worker start indices so they avoid touching the same solver blocks
static inline int GetWorkerStartIndex( int workerIndex, int blockCount, int workerCount )
{
	if ( blockCount <= workerCount )
	{
		return workerIndex < blockCount ? workerIndex : B3_NULL_INDEX;
	}

	int blocksPerWorker = blockCount / workerCount;
	int remainder = blockCount - blocksPerWorker * workerCount;
	return blocksPerWorker * workerIndex + b3MinInt( remainder, workerIndex );
}

// Execute a stage, which is an array of solver blocks, each controlled with an atomic sync index.
// Each worker starts at its home index and sweeps the ring, CAS-claiming any unclaimed blocks.
static void b3ExecuteStage( b3SolverStage* stage, b3StepContext* context, int previousSyncIndex, int syncIndex, int workerIndex )
{
	int completedCount = 0;
	b3SyncBlock* blocks = stage->blocks;
	int blockCount = stage->blockCount;

	int startIndex = GetWorkerStartIndex( workerIndex, blockCount, context->workerCount );
	if ( startIndex == B3_NULL_INDEX )
	{
		return;
	}

	B3_ASSERT( 0 <= startIndex && startIndex < blockCount );

	int blockIndex = startIndex;
	for ( int i = 0; i < blockCount; ++i )
	{
		if ( b3AtomicCompareExchangeInt( &blocks[blockIndex].syncIndex, previousSyncIndex, syncIndex ) )
		{
			B3_ASSERT( completedCount < blockCount );

			// Pass the descriptor by value -- the wrapping b3SyncBlock holds the atomic
			// syncIndex but we only copy .block, so the struct copy never aliases the CAS target.
			b3ExecuteBlock( stage, context, blocks[blockIndex].block, workerIndex );
			completedCount += 1;
		}

		blockIndex += 1;
		if ( blockIndex >= blockCount )
		{
			blockIndex = 0;
		}
	}

	(void)b3AtomicFetchAddInt( &stage->completionCount, completedCount );
}

// Execute a stage on worker 0 (main thread).
static void b3ExecuteMainStage( b3SolverStage* stage, b3StepContext* context, uint32_t syncBits )
{
	int blockCount = stage->blockCount;
	if ( blockCount == 0 )
	{
		return;
	}

	const int workerIndex = 0;

	if ( blockCount == 1 )
	{
		b3ExecuteBlock( stage, context, stage->blocks[0].block, workerIndex );
	}
	else
	{
		b3AtomicStoreU32( &context->atomicSyncBits, syncBits );

		int syncIndex = ( syncBits >> 16 ) & 0xFFFF;
		B3_ASSERT( syncIndex > 0 );
		int previousSyncIndex = syncIndex - 1;

		b3ExecuteStage( stage, context, previousSyncIndex, syncIndex, workerIndex );

		// Spin waiting for thieves to finish
		while ( b3AtomicLoadInt( &stage->completionCount ) != blockCount )
		{
			b3Pause();
		}

		b3AtomicStoreInt( &stage->completionCount, 0 );
	}
}

// Added for Nebenan: one stage of a pack of islands over [start, end), in blocks that b3SolverBlock::count can hold
static void b3ExecutePackRange( b3SolverStageType type, b3StepContext* context, int start, int end, uint8_t blockType,
								uint8_t colorIndex, int workerIndex )
{
	b3SolverStage stage = { .type = type };
	while ( start < end )
	{
		int count = b3MinInt( end - start, UINT16_MAX );
		b3SolverBlock block = { start, (uint16_t)count, blockType, colorIndex };
		b3ExecuteBlock( &stage, context, block, workerIndex );
		start += count;
	}
}

// Added for Nebenan: one stage of the batches of knots in [start, end), see b3KnotBatch
static void b3ExecuteKnots( b3SolverStageType type, b3StepContext* context, int start, int end )
{
	if ( start == end )
	{
		return;
	}

	switch ( type )
	{
		case b3_stageWarmStart:
			b3WarmStartKnots( context, start, end );
			break;

		case b3_stageSolve:
			b3PushKnots( context, start, end );
			break;

		case b3_stageRelax:
			b3SolveKnots( context, start, end );
			break;

		case b3_stageRestitution:
			b3ApplyKnotRestitution( context, start, end );
			break;

		default:
			B3_ASSERT( false );
			break;
	}
}

// Added for Nebenan: one stage of the constraints of a pack, the overflow constraints first, the knots before the others,
// and then the colors in order, as b3SolverTask does
static void b3ExecutePackConstraints( b3SolverStageType type, b3StepContext* context, int pack, int workerIndex )
{
	const int* knotStarts = context->packKnotStarts;
	b3ExecuteKnots( type, context, knotStarts[pack], knotStarts[pack + 1] );

	const int* overflowStarts = context->packOverflowStarts;
	b3ExecutePackRange( type, context, overflowStarts[pack], overflowStarts[pack + 1], b3_overflowBlock, B3_OVERFLOW_INDEX,
						workerIndex );

	for ( int i = 0; i < context->activeColorCount; ++i )
	{
		const int* wideStarts = context->packWideStarts + i * ( context->packCount + 1 );
		b3ExecutePackRange( type, context, wideStarts[pack], wideStarts[pack + 1], b3_graphWideContactBlock,
							(uint8_t)context->activeColorIndices[i], workerIndex );
	}
}

// Added for Nebenan: one stage of the convex contact constraints of a pack by their index across all colors, as the
// stage that stores their impulses numbers them
static void b3ExecutePackWideConstraints( b3SolverStageType type, b3StepContext* context, int pack, int workerIndex )
{
	for ( int i = 0; i < context->activeColorCount; ++i )
	{
		const int* wideStarts = context->packWideStarts + i * ( context->packCount + 1 );
		int base = context->widePrepareSpans[i].start;
		b3ExecutePackRange( type, context, base + wideStarts[pack], base + wideStarts[pack + 1], b3_wideContactBlock, UINT8_MAX,
							workerIndex );
	}
}

// Added for Nebenan: one pack of islands on one thread, from preparing its constraints through all substeps and
// restitution to storing their impulses
static void b3SolvePack( b3StepContext* context, int pack, int workerIndex )
{
	int bodyStart = context->packBodyStarts[pack];
	int bodyEnd = context->packBodyStarts[pack + 1];
	int overflowStart = context->packOverflowStarts[pack];
	int overflowEnd = context->packOverflowStarts[pack + 1];

	// The wide constraints of the knots of the pack, see b3KnotBatch
	int knotWide = B3_KNOT_LANES / context->world->simdWidth;
	int knotWideStart = context->knotWideStart + knotWide * context->packKnotStarts[pack];
	int knotWideEnd = context->knotWideStart + knotWide * context->packKnotStarts[pack + 1];

	b3ExecutePackRange( b3_stagePrepareContacts, context, overflowStart, overflowEnd, b3_overflowBlock, B3_OVERFLOW_INDEX,
						workerIndex );
	b3ExecutePackRange( b3_stagePrepareWideContacts, context, knotWideStart, knotWideEnd, b3_wideContactBlock, UINT8_MAX,
						workerIndex );
	b3PreparePackContacts_Convex( context, pack );

	for ( int i = 0; i < context->subStepCount; ++i )
	{
		b3ExecutePackRange( b3_stageIntegrateVelocities, context, bodyStart, bodyEnd, b3_packBodyBlock, UINT8_MAX, workerIndex );
		b3ExecutePackConstraints( b3_stageWarmStart, context, pack, workerIndex );
		b3ExecutePackConstraints( b3_stageSolve, context, pack, workerIndex );
		b3ExecutePackRange( b3_stageIntegratePositions, context, bodyStart, bodyEnd, b3_packBodyBlock, UINT8_MAX, workerIndex );
		b3ExecutePackConstraints( b3_stageRelax, context, pack, workerIndex );
	}

	// Preparing a constraint with restitution sets the flag, so a pack with one always sees it. A pack that sees it set by
	// another pack runs restitution for nothing: without propagation, which packs require, it skips constraints without
	// restitution, see b3ApplyRestitution_Convex.
	if ( b3AtomicLoadInt( &context->anyRestitution ) != 0 )
	{
		for ( int j = 0; j < context->world->restitutionIterations; ++j )
		{
			b3ExecutePackConstraints( b3_stageRestitution, context, pack, workerIndex );
		}
	}

	b3ExecutePackWideConstraints( b3_stageStoreWideImpulses, context, pack, workerIndex );
	b3ExecutePackRange( b3_stageStoreWideImpulses, context, knotWideStart, knotWideEnd, b3_wideContactBlock, UINT8_MAX,
						workerIndex );
	b3ExecutePackRange( b3_stageStoreImpulses, context, overflowStart, overflowEnd, b3_overflowBlock, B3_OVERFLOW_INDEX,
						workerIndex );
}

// Added for Nebenan: the constraint solver without a barrier per stage. Two islands never share a moving body, so a thread
// can take an island from preparing its constraints through all substeps to storing their impulses alone, and its bodies
// and constraints stay in its cache. Each body takes its steps in the same order as with a barrier per stage, so nothing
// changes in the results. The threads take whole packs of islands, the largest first, until none is left. One that loses
// its core holds up only the pack it is on.
static void b3SolvePacks( b3StepContext* context, int workerIndex )
{
	int solved = 0;
	while ( true )
	{
		int pack = b3AtomicFetchAddInt( &context->packNext, 1 );
		if ( pack >= context->packCount )
		{
			break;
		}

		b3SolvePack( context, pack, workerIndex );
		solved += 1;
	}

	if ( solved > 0 )
	{
		(void)b3AtomicFetchAddInt( &context->packDone, solved );
	}
}

// Added for Nebenan: a wait for the last packs is long from a millisecond on and from half the time of the packs on,
// from nine on the solver keeps the barriers for 120 steps, and a stretch that comes back within twice the last one
// lasts twice as long, up to 960 steps
#define B3_PACK_WAIT_LONG 1.0f
#define B3_PACK_HOLD_SCORE 9
#define B3_PACK_HOLD_STEPS 120
#define B3_PACK_HOLD_MAX 960

// Added for Nebenan: a thread that loses its core to another program holds up the pack it is on, while with a barrier
// per stage the others take over what it has not started yet. Once the threads wait long for the last packs again and
// again, the solver solves by colors for a while. Each such step counts three and each step without one takes one off,
// so a single long wait, as it comes now and then without load, does not count. The results are the same either way,
// see b3SolvePacks.
static void b3UpdatePackHold( b3World* world, float wait, float total )
{
	bool longWait = wait >= B3_PACK_WAIT_LONG && 2.0f * wait >= total;
	world->packWaitScore = longWait ? world->packWaitScore + 3 : b3MaxInt( world->packWaitScore - 1, 0 );
	if ( world->packWaitScore < B3_PACK_HOLD_SCORE )
	{
		return;
	}

	bool soon = world->packHoldLength > 0 && world->packHoldGap < 2 * world->packHoldLength;
	world->packHoldLength = soon ? b3MinInt( 2 * world->packHoldLength, B3_PACK_HOLD_MAX ) : B3_PACK_HOLD_STEPS;
	world->packHoldSteps = world->packHoldLength;
	world->packWaitScore = 0;
}

// Parallel solver task
static void b3SolverTask( void* taskContext )
{
	b3WorkerContext* workerContext = (b3WorkerContext*)taskContext;
	int workerIndex = workerContext->workerIndex;
	b3StepContext* context = workerContext->context;
	int activeColorCount = context->activeColorCount;
	b3SolverStage* stages = context->stages;
	b3Profile* profile = &context->world->profile;

	if ( workerIndex == 0 )
	{
		// The orchestrator slot is a race. The calling thread of b3World_Step also enters here
		// as worker 0, so progress is guaranteed even if the user's task system schedules tasks
		// out of order, has fewer threads than workerCount, or runs the task synchronously
		// inside enqueueTaskFcn. Whoever wins the CAS becomes the orchestrator; the loser
		// returns and lets the spinner-only path handle workers >0.
		if ( b3AtomicCompareExchangeInt( &context->mainClaimed, 0, 1 ) == false )
		{
			return;
		}

		// Main thread synchronizes the workers and does work itself.
		//
		// This needs to be a task for the main thread because the user's task system may execute
		// the tasks serially and this is the first task. This single task is able to fully
		// complete all work even if all other workers are blocked.

		// Stages are re-used by loops so that I don't need more stages for large substep counts.
		// The sync indices grow monotonically for the body/graph/constraint groupings because they share solver blocks.
		// The stage index and sync indices are combined in to sync bits for atomic synchronization.
		// The workers need to compute the previous sync index for a given stage so that CAS works correctly. This
		// setup makes this easy to do.

		/*
		b3_stagePrepareJoints,
		b3_stagePrepareContacts,
		b3_stageIntegrateVelocities,
		b3_stageWarmStart,
		b3_stageSolve,
		b3_stageIntegratePositions,
		b3_stageRelax,
		b3_stageStoreImpulses
		*/

		uint64_t ticks = b3GetTicks();

		// Added for Nebenan: whole packs of islands without a barrier if the step has them, see b3SolvePacks. They prepare
		// and store their constraints themselves, so no stage is left, and all their time counts as solving.
		if ( context->packCount > 0 )
		{
			// The workers join in with the stage index 0xFFFF, see below
			b3AtomicStoreU32( &context->atomicSyncBits, ( 1u << 16 ) | 0xFFFF );
			b3SolvePacks( context, 0 );

			// How long this thread waits for the others, see b3UpdatePackHold
			uint64_t waitTicks = b3GetTicks();
			int spinCount = 0;
			while ( b3AtomicLoadInt( &context->packDone ) < context->packCount )
			{
				if ( spinCount > 5 )
				{
					b3Yield();
					spinCount = 0;
				}
				else
				{
					b3Pause();
					b3Pause();
					spinCount += 1;
				}
			}

			float wait = b3GetMilliseconds( waitTicks );
			float total = b3GetMillisecondsAndReset( &ticks );
			profile->solveImpulses += total;
			b3UpdatePackHold( context->world, wait, total );

			// Signal workers to finish
			b3AtomicStoreU32( &context->atomicSyncBits, UINT_MAX );
			return;
		}

		int bodySyncIndex = 1;
		int stageIndex = 0;

		// Prepare joint constraints
		uint32_t jointSyncIndex = 1;
		uint32_t syncBits = ( jointSyncIndex << 16 ) | stageIndex;
		B3_ASSERT( stages[stageIndex].type == b3_stagePrepareJoints );
		b3ExecuteMainStage( stages + stageIndex, context, syncBits );
		stageIndex += 1;
		jointSyncIndex += 1;

		// Prepare convex contact constraints
		uint32_t convexSyncIndex = 1;
		syncBits = ( convexSyncIndex << 16 ) | stageIndex;
		B3_ASSERT( stages[stageIndex].type == b3_stagePrepareWideContacts );
		b3ExecuteMainStage( stages + stageIndex, context, syncBits );
		stageIndex += 1;
		convexSyncIndex += 1;

		// Prepare mesh contact constraints
		uint32_t meshSyncIndex = 1;
		syncBits = ( meshSyncIndex << 16 ) | stageIndex;
		B3_ASSERT( stages[stageIndex].type == b3_stagePrepareContacts );
		b3ExecuteMainStage( stages + stageIndex, context, syncBits );
		stageIndex += 1;
		meshSyncIndex += 1;

		// Single-threaded overflow work. These constraints don't fit in the graph coloring.
		uint64_t overflowTicks = b3GetTicks();
		b3PrepareJoints_Overflow( context );
		b3PrepareContacts_Overflow( context );
		profile->overflow += b3GetMilliseconds( overflowTicks );

		profile->prepareConstraints += b3GetMillisecondsAndReset( &ticks );

		int graphSyncIndex = 1;
		int subStepCount = context->subStepCount;
		for ( int subStepIndex = 0; subStepIndex < subStepCount; ++subStepIndex )
		{
			// stageIndex restarted each iteration
			// syncBits still increases monotonically because the upper bits increase each iteration
			int iterationStageIndex = stageIndex;

			// Integrate velocities
			syncBits = ( bodySyncIndex << 16 ) | iterationStageIndex;
			B3_ASSERT( stages[iterationStageIndex].type == b3_stageIntegrateVelocities );
			b3ExecuteMainStage( stages + iterationStageIndex, context, syncBits );
			iterationStageIndex += 1;
			bodySyncIndex += 1;

			profile->integrateVelocities += b3GetMillisecondsAndReset( &ticks );

			// Warm start constraints
			overflowTicks = b3GetTicks();
			b3WarmStartJoints_Overflow( context );
			b3WarmStartKnots( context, 0, context->knotBatchCount );
			b3WarmStartContacts_Overflow( context );
			profile->overflow += b3GetMilliseconds( overflowTicks );

			for ( int colorIndex = 0; colorIndex < activeColorCount; ++colorIndex )
			{
				syncBits = ( graphSyncIndex << 16 ) | iterationStageIndex;
				B3_ASSERT( stages[iterationStageIndex].type == b3_stageWarmStart );
				b3ExecuteMainStage( stages + iterationStageIndex, context, syncBits );
				iterationStageIndex += 1;
			}
			graphSyncIndex += 1;

			profile->warmStart += b3GetMillisecondsAndReset( &ticks );

			// Solve constraints
			bool useBias = true;
			for ( int j = 0; j < ITERATIONS; ++j )
			{
				// Overflow constraints have lower priority. Typically these are dynamic-vs-dynamic.
				overflowTicks = b3GetTicks();
				b3SolveJoints_Overflow( context, useBias );
				b3PushKnots( context, 0, context->knotBatchCount );
				b3PushContacts_Overflow( context );
				profile->overflow += b3GetMilliseconds( overflowTicks );

				for ( int colorIndex = 0; colorIndex < activeColorCount; ++colorIndex )
				{
					syncBits = ( graphSyncIndex << 16 ) | iterationStageIndex;
					B3_ASSERT( stages[iterationStageIndex].type == b3_stageSolve );
					b3ExecuteMainStage( stages + iterationStageIndex, context, syncBits );
					iterationStageIndex += 1;
				}
				graphSyncIndex += 1;
			}

			profile->solveImpulses += b3GetMillisecondsAndReset( &ticks );

			// Integrate positions
			B3_ASSERT( stages[iterationStageIndex].type == b3_stageIntegratePositions );
			syncBits = ( bodySyncIndex << 16 ) | iterationStageIndex;
			b3ExecuteMainStage( stages + iterationStageIndex, context, syncBits );
			iterationStageIndex += 1;
			bodySyncIndex += 1;

			profile->integratePositions += b3GetMillisecondsAndReset( &ticks );

			// Relax constraints
			useBias = false;
			for ( int j = 0; j < RELAX_ITERATIONS; ++j )
			{
				overflowTicks = b3GetTicks();
				b3SolveJoints_Overflow( context, useBias );
				b3SolveKnots( context, 0, context->knotBatchCount );
				b3SolveContacts_Overflow( context );
				profile->overflow += b3GetMilliseconds( overflowTicks );

				for ( int colorIndex = 0; colorIndex < activeColorCount; ++colorIndex )
				{
					syncBits = ( graphSyncIndex << 16 ) | iterationStageIndex;
					B3_ASSERT( stages[iterationStageIndex].type == b3_stageRelax );
					b3ExecuteMainStage( stages + iterationStageIndex, context, syncBits );
					iterationStageIndex += 1;
				}
				graphSyncIndex += 1;
			}

			profile->relaxImpulses += b3GetMillisecondsAndReset( &ticks );
		}

		// Advance the stage according to the sub-stepping tasks just completed
		// integrate velocities / warm start / solve / integrate positions / relax
		stageIndex += 1 + activeColorCount + ITERATIONS * activeColorCount + 1 + RELAX_ITERATIONS * activeColorCount;

		int restitutionIterations = context->world->restitutionIterations;
		if ( restitutionIterations > 0 && b3AtomicLoadInt( &context->anyRestitution ) != 0 )
		{
			for ( int j = 0; j < restitutionIterations; ++j )
			{
				overflowTicks = b3GetTicks();
				b3ApplyKnotRestitution( context, 0, context->knotBatchCount );
				b3ApplyRestitution_Overflow( context );
				profile->overflow += b3GetMilliseconds( overflowTicks );

				int iterationStageIndex = stageIndex;
				for ( int colorIndex = 0; colorIndex < activeColorCount; ++colorIndex )
				{
					syncBits = ( graphSyncIndex << 16 ) | iterationStageIndex;
					B3_ASSERT( stages[iterationStageIndex].type == b3_stageRestitution );
					b3ExecuteMainStage( stages + iterationStageIndex, context, syncBits );
					iterationStageIndex += 1;
				}
				graphSyncIndex += 1;
			}

			profile->restitution += b3GetMillisecondsAndReset( &ticks );
		}

		stageIndex += activeColorCount;

		// Store impulses
		overflowTicks = b3GetTicks();
		b3StoreImpulses_Overflow( context );
		profile->overflow += b3GetMilliseconds( overflowTicks );

		syncBits = ( convexSyncIndex << 16 ) | stageIndex;
		B3_ASSERT( stages[stageIndex].type == b3_stageStoreWideImpulses );
		b3ExecuteMainStage( stages + stageIndex, context, syncBits );
		stageIndex += 1;

		syncBits = ( meshSyncIndex << 16 ) | stageIndex;
		B3_ASSERT( stages[stageIndex].type == b3_stageStoreImpulses );
		b3ExecuteMainStage( stages + stageIndex, context, syncBits );
		stageIndex += 1;

		profile->storeImpulses += b3GetMillisecondsAndReset( &ticks );

		// Signal workers to finish
		b3AtomicStoreU32( &context->atomicSyncBits, UINT_MAX );

		B3_ASSERT( stageIndex == context->stageCount );
		return;
	}

	// Worker spins and waits for work
	uint32_t lastSyncBits = 0;
	// uint64_t maxSpinTime = 10;
	while ( true )
	{
		// Spin until main thread bumps changes the sync bits. This can waste significant time overall, but it is necessary for
		// parallel simulation with graph coloring.
		// todo improve this spinner
		uint32_t syncBits;
		int spinCount = 0;
		while ( ( syncBits = b3AtomicLoadU32( &context->atomicSyncBits ) ) == lastSyncBits )
		{
			if ( spinCount > 5 )
			{
				b3Yield();
				spinCount = 0;
			}
			else
			{
				// Using the cycle counter helps to account for variation in mm_pause timing across different
				// CPUs. However, this is X64 only.
				// uint64_t prev = __rdtsc();
				// do
				//{
				//	b3Pause();
				//}
				// while ((__rdtsc() - prev) < maxSpinTime);
				// maxSpinTime += 10;
				b3Pause();
				b3Pause();
				spinCount += 1;
			}
		}

		if ( syncBits == UINT_MAX )
		{
			// sentinel hit
			break;
		}

		int stageIndex = syncBits & 0xFFFF;

		// Added for Nebenan: the packs of islands, see above
		if ( stageIndex == 0xFFFF )
		{
			b3SolvePacks( context, workerIndex );
			lastSyncBits = syncBits;
			continue;
		}

		B3_ASSERT( stageIndex < context->stageCount );

		int syncIndex = ( syncBits >> 16 ) & 0xFFFF;
		B3_ASSERT( syncIndex > 0 );

		int previousSyncIndex = syncIndex - 1;

		b3SolverStage* stage = stages + stageIndex;
		b3ExecuteStage( stage, context, previousSyncIndex, syncIndex, workerIndex );

		lastSyncBits = syncBits;
	}
}

static void b3BulletBodyTask( int startIndex, int endIndex, int workerIndex, void* context )
{
	b3TracyCZoneNC( bullet_body_task, "Bullet Body Task", b3_colorLightSkyBlue, true );

	b3StepContext* stepContext = (b3StepContext*)context;
	b3TaskContext* taskContext = b3Array_Get( stepContext->world->taskContexts, workerIndex );

	B3_ASSERT( startIndex <= endIndex );

	for ( int i = startIndex; i < endIndex; ++i )
	{
		int simIndex = stepContext->bulletBodies[i];
		b3SolveContinuous( stepContext->world, simIndex, taskContext, stepContext->dt );
	}

	b3TracyCZoneEnd( bullet_body_task );
}

// Added for Nebenan: buckets for the order of the contacts in a color, see b3OrderColorContacts
#define B3_CONTACT_ORDER_BUCKETS 64

// Added for Nebenan: the convex contacts of every active color, in buckets of the smaller awake index of their bodies. A
// color holds no two contacts of the same moving body, so the order within it changes no result. In this order the
// worker that integrates a range of bodies gets most of their contacts in every color, and the states of those bodies stay
// in its cache instead of moving between cores from color to color. Only with workers. The collide task wrote the keys,
// see b3World::contactSortKeys, and the counting sort reads them from that dense array instead of the scattered contacts.
// Returns the contact ids in stack memory in the order of the active colors, or NULL.
static int* b3OrderColorContacts( b3World* world, const b3GraphColor* colors, const int* activeColorIndices, int activeColorCount,
								  int awakeBodyCount, int workerCount )
{
	if ( workerCount < 2 )
	{
		return NULL;
	}

	int total = 0;
	for ( int i = 0; i < activeColorCount; ++i )
	{
		total += colors[activeColorIndices[i]].convexContacts.count;
	}

	if ( total == 0 )
	{
		return NULL;
	}

	int* ordered = (int*)b3StackAlloc( &world->stack, total * (int)sizeof( int ), "ordered contacts" );
	const int* keys = world->contactSortKeys;
	int offset = 0;
	for ( int i = 0; i < activeColorCount; ++i )
	{
		const b3GraphColor* color = colors + activeColorIndices[i];
		const int* ids = color->convexContacts.data;
		int count = color->convexContacts.count;

		// Keys from an earlier step can exceed the bodies awake now, they go in the last bucket
		int starts[B3_CONTACT_ORDER_BUCKETS + 1] = { 0 };
		for ( int k = 0; k < count; ++k )
		{
			int bucket = (int)( (int64_t)keys[ids[k]] * B3_CONTACT_ORDER_BUCKETS / awakeBodyCount );
			starts[b3MinInt( bucket, B3_CONTACT_ORDER_BUCKETS - 1 ) + 1] += 1;
		}

		for ( int b = 0; b < B3_CONTACT_ORDER_BUCKETS; ++b )
		{
			starts[b + 1] += starts[b];
		}

		for ( int k = 0; k < count; ++k )
		{
			int bucket = (int)( (int64_t)keys[ids[k]] * B3_CONTACT_ORDER_BUCKETS / awakeBodyCount );
			ordered[offset + starts[b3MinInt( bucket, B3_CONTACT_ORDER_BUCKETS - 1 )]++] = ids[k];
		}

		offset += count;
	}

	return ordered;
}

// Added for Nebenan: packs of islands per worker, enough for the workers to even out the work, see b3BuildPacks. A byte
// holds the pack of a contact, so the lookups by contact id stay in the closest caches.
#define B3_PACKS_PER_WORKER 4
#define B3_MAX_PACKS ( B3_PACKS_PER_WORKER * B3_MAX_WORKERS )
_Static_assert( B3_MAX_PACKS <= 256, "a pack needs to fit in a byte" );

// Added for Nebenan: the work of the awake islands as b3BuildPacks weighs it, for a step of a world with workers that
// solves without packs, see b3Counters::islandWork. A world without workers skips it: its islands could be many.
static void b3MeasureIslandWork( b3World* world, const b3SolverSet* awakeSet )
{
	int total = 0;
	int largest = 0;
	for ( int i = 0; i < awakeSet->islandSims.count; ++i )
	{
		const b3Island* island = b3Array_Get( world->islands, awakeSet->islandSims.data[i].islandId );
		int weight = 4 * island->contacts.count + island->bodies.count;
		total += weight;
		largest = b3MaxInt( largest, weight );
	}
	world->islandWork = total;
	world->largestIslandWork = largest;
}

// Added for Nebenan: the awake islands in packs of about equal work, the largest pack first, for b3SolvePacks. Writes the
// pack of every contact of the islands to contactPacks by contact id, and the awake indices of the bodies of every pack to
// packBodies from packBodyStarts on. Returns the pack count, or zero if one island holds more than half of the work, which
// the colors share better among the workers.
static int b3BuildPacks( b3World* world, const b3SolverSet* awakeSet, int packLimit, uint8_t* contactPacks,
						 int* packBodyStarts, int* packBodies )
{
	int islandCount = awakeSet->islandSims.count;
	if ( islandCount < 2 )
	{
		// Added for Nebenan: a single island holds all of the work, see b3Counters::islandWork
		if ( islandCount == 1 )
		{
			const b3Island* island = b3Array_Get( world->islands, awakeSet->islandSims.data[0].islandId );
			world->islandWork = 4 * island->contacts.count + island->bodies.count;
			world->largestIslandWork = world->islandWork;
		}
		return 0;
	}

	const b3IslandSim* islandSims = awakeSet->islandSims.data;
	int* weights = (int*)b3StackAlloc( &world->stack, islandCount * (int)sizeof( int ), "island weights" );
	int* order = (int*)b3StackAlloc( &world->stack, islandCount * (int)sizeof( int ), "island order" );
	int* islandPacks = (int*)b3StackAlloc( &world->stack, world->islands.count * (int)sizeof( int ), "island packs" );

	// The work of an island in its contacts, which cost the most, and its bodies. The islands go in buckets by the leading
	// zeros of their work, the largest first, which is order enough for the packs.
	int bucketStarts[33] = { 0 };
	int total = 0;
	int largest = 0;
	for ( int i = 0; i < islandCount; ++i )
	{
		const b3Island* island = b3Array_Get( world->islands, islandSims[i].islandId );
		int weight = 4 * island->contacts.count + island->bodies.count;
		B3_ASSERT( weight > 0 );
		weights[i] = weight;
		total += weight;
		largest = b3MaxInt( largest, weight );
		bucketStarts[b3CLZ32( (uint32_t)weight )] += 1;
	}

	// Added for Nebenan: see b3Counters::islandWork
	world->islandWork = total;
	world->largestIslandWork = largest;

	int packCount = 0;
	if ( 2 * largest <= total )
	{
		int start = 0;
		for ( int b = 0; b < 33; ++b )
		{
			int count = bucketStarts[b];
			bucketStarts[b] = start;
			start += count;
		}

		for ( int i = 0; i < islandCount; ++i )
		{
			order[bucketStarts[b3CLZ32( (uint32_t)weights[i] )]++] = i;
		}

		// Each island to the pack with the least work so far
		packCount = b3MinInt( packLimit, islandCount );
		int loads[B3_MAX_PACKS] = { 0 };
		for ( int k = 0; k < islandCount; ++k )
		{
			int lightest = 0;
			for ( int p = 1; p < packCount; ++p )
			{
				lightest = loads[p] < loads[lightest] ? p : lightest;
			}

			int i = order[k];
			loads[lightest] += weights[i];
			islandPacks[islandSims[i].islandId] = lightest;
		}

		// The packs numbered by their work, the largest first, so the workers take those first
		int ranked[B3_MAX_PACKS];
		for ( int p = 0; p < packCount; ++p )
		{
			int k = p;
			while ( k > 0 && loads[ranked[k - 1]] < loads[p] )
			{
				ranked[k] = ranked[k - 1];
				k -= 1;
			}
			ranked[k] = p;
		}

		int packIds[B3_MAX_PACKS];
		for ( int k = 0; k < packCount; ++k )
		{
			packIds[ranked[k]] = k;
		}

		int cursors[B3_MAX_PACKS + 1] = { 0 };
		for ( int i = 0; i < islandCount; ++i )
		{
			int islandId = islandSims[i].islandId;
			int pack = packIds[islandPacks[islandId]];
			islandPacks[islandId] = pack;
			cursors[pack + 1] += b3Array_Get( world->islands, islandId )->bodies.count;
		}

		for ( int p = 0; p < packCount; ++p )
		{
			cursors[p + 1] += cursors[p];
		}

		for ( int p = 0; p <= packCount; ++p )
		{
			packBodyStarts[p] = cursors[p];
		}

		// The bodies of every pack in the order of the awake set
		int bodyCount = awakeSet->bodySims.count;
		B3_ASSERT( cursors[packCount] == bodyCount );
		const b3BodySim* sims = awakeSet->bodySims.data;
		const b3Body* bodies = world->bodies.data;
		for ( int i = 0; i < bodyCount; ++i )
		{
			if ( i + 8 < bodyCount )
			{
				b3Prefetch( &bodies[sims[i + 8].bodyId].islandId );
			}

			const b3Body* body = bodies + sims[i].bodyId;
			B3_ASSERT( body->setIndex == b3_awakeSet && body->localIndex == i );
			packBodies[cursors[islandPacks[body->islandId]]++] = i;
		}

		// Every contact of an island belongs to it, the overflow contacts as well
		for ( int i = 0; i < islandCount; ++i )
		{
			if ( i + 2 < islandCount )
			{
				b3Prefetch( b3Array_Get( world->islands, islandSims[i + 2].islandId )->contacts.data );
			}

			const b3Island* island = b3Array_Get( world->islands, islandSims[i].islandId );
			int pack = islandPacks[island->islandId];
			const b3ContactLink* links = island->contacts.data;
			for ( int k = 0; k < island->contacts.count; ++k )
			{
				contactPacks[links[k].contactId] = (uint8_t)pack;
			}
		}
	}

	b3StackFree( &world->stack, islandPacks );
	b3StackFree( &world->stack, order );
	b3StackFree( &world->stack, weights );
	return packCount;
}

// Added for Nebenan: the convex contacts of every active color pack by pack for b3SolvePacks, each pack filled up with null
// ids to whole wide constraints, so no wide constraint holds two packs. Writes for every color where the wide constraints of
// each pack start, see b3StepContext::packWideStarts, and how many entries the color has. Returns the entries in stack memory.
static int* b3OrderPackContacts( b3World* world, const b3GraphColor* colors, const int* activeColorIndices, int activeColorCount,
								 const uint8_t* contactPacks, int packCount, int* packWideStarts, int* entryCounts )
{
	// The width of the world, see b3World::simdWidth
	int simdWidth = world->simdWidth;
	int simdShift = simdWidth == 8 ? 3 : 2;

	int capacity = 0;
	for ( int i = 0; i < activeColorCount; ++i )
	{
		capacity += colors[activeColorIndices[i]].convexContacts.count + ( simdWidth - 1 ) * packCount;
	}

	int* entries = (int*)b3StackAlloc( &world->stack, b3MaxInt( capacity, 1 ) * (int)sizeof( int ), "pack contacts" );
	int offset = 0;
	for ( int i = 0; i < activeColorCount; ++i )
	{
		const b3GraphColor* color = colors + activeColorIndices[i];
		const int* ids = color->convexContacts.data;
		int count = color->convexContacts.count;
		int* starts = packWideStarts + i * ( packCount + 1 );

		int cursors[B3_MAX_PACKS] = { 0 };
		for ( int k = 0; k < count; ++k )
		{
			cursors[contactPacks[ids[k]]] += 1;
		}

		int entryCount = 0;
		for ( int p = 0; p < packCount; ++p )
		{
			int packContactCount = cursors[p];
			starts[p] = entryCount >> simdShift;
			cursors[p] = entryCount;
			entryCount += ( packContactCount + simdWidth - 1 ) & ~( simdWidth - 1 );
		}
		starts[packCount] = entryCount >> simdShift;

		for ( int k = 0; k < count; ++k )
		{
			entries[offset + cursors[contactPacks[ids[k]]]++] = ids[k];
		}

		for ( int p = 0; p < packCount; ++p )
		{
			int end = starts[p + 1] << simdShift;
			for ( int e = cursors[p]; e < end; ++e )
			{
				entries[offset + e] = B3_NULL_INDEX;
			}
		}

		entryCounts[i] = entryCount;
		offset += entryCount;
	}

	return entries;
}

// Solve with graph coloring
// Added for Nebenan: a moving body with at least this many contacts in the overflow is a knot, see b3BuildKnots
#define B3_KNOT_MIN_CONTACTS 2

// Added for Nebenan: the overflow of a step in batches of knots and the contacts left over, in stack memory, see
// b3BuildKnots
typedef struct b3Knots
{
	// The batches, with B3_KNOT_LANES contact ids and scales of the inverse mass of body A and B per batch
	b3KnotBatch* batches;
	int* contactIds;
	float* scaleA;
	float* scaleB;
	int batchCount;
	int contactCount;

	// With packs, the first batch of every pack and the end
	int* packStarts;

	// The overflow contacts outside the knots, in their order
	b3ContactSpec* others;
	int otherCount;

	// All of the above in one block, the stack holds few
	void* memory;
} b3Knots;

// Added for Nebenan: the next part of a block in stack memory, on its own cache line
static void* b3TakeKnotMemory( char** cursor, int byteCount )
{
	void* part = *cursor;
	*cursor += ( ( byteCount + 63 ) / 64 ) * 64;
	return part;
}

// Added for Nebenan: the overflow holds the contacts the graph finds no color for, see b3AddContactToGraph, and the calling
// thread solves them one at a time while the workers wait. Beside a few contacts of tight spots these are the contacts of
// knots: large parts of a fracture, dozens to hundreds of pieces in one body, with hundreds of small debris resting on and
// against them. A knot is the awake body with more contacts in the overflow of the two, if it has at least
// B3_KNOT_MIN_CONTACTS. Its contacts go into batches of B3_KNOT_LANES with different partners: the contact with a partner
// the knot already met k times in the overflow goes into a batch of round k. Each batch solves its lanes at once against
// copies of the knot, see b3KnotBatch, which makes the knot's chain of contacts several times shorter, and the batches of
// the knots take turns. Everything follows from the order of the overflow, so the batches are the same with every number
// of workers and in either width. With packs the batches go pack by pack, in the same order within each pack.
static void b3BuildKnots( b3World* world, const b3SolverSet* awakeSet, const b3GraphColor* overflow, const uint8_t* contactPacks,
						  int packCount, b3Knots* knots )
{
	int n = overflow->contacts.count;
	const b3ContactSpec* specs = overflow->contacts.data;
	const b3Contact* contacts = world->contacts.data;
	int awakeBodyCount = awakeSet->bodySims.count;
	int capacity = b3MaxInt( n, 1 );

	// At most one batch per contact
	int byteCount = 64 * 6 + capacity * (int)sizeof( b3KnotBatch ) + 3 * B3_KNOT_LANES * capacity * (int)sizeof( int ) +
					capacity * (int)sizeof( b3ContactSpec ) + ( packCount + 1 ) * (int)sizeof( int );
	*knots = (b3Knots){ 0 };
	knots->memory = b3StackAlloc( &world->stack, byteCount, "knots" );
	char* cursor = (char*)knots->memory;
	knots->batches = (b3KnotBatch*)b3TakeKnotMemory( &cursor, capacity * (int)sizeof( b3KnotBatch ) );
	knots->contactIds = (int*)b3TakeKnotMemory( &cursor, B3_KNOT_LANES * capacity * (int)sizeof( int ) );
	knots->scaleA = (float*)b3TakeKnotMemory( &cursor, B3_KNOT_LANES * capacity * (int)sizeof( float ) );
	knots->scaleB = (float*)b3TakeKnotMemory( &cursor, B3_KNOT_LANES * capacity * (int)sizeof( float ) );
	knots->others = (b3ContactSpec*)b3TakeKnotMemory( &cursor, capacity * (int)sizeof( b3ContactSpec ) );
	if ( packCount > 0 )
	{
		knots->packStarts = (int*)b3TakeKnotMemory( &cursor, ( packCount + 1 ) * (int)sizeof( int ) );
		for ( int p = 0; p <= packCount; ++p )
		{
			knots->packStarts[p] = 0;
		}
	}
	B3_ASSERT( cursor <= (char*)knots->memory + byteCount );

	if ( n == 0 )
	{
		return;
	}

	// Per awake body its contacts in the overflow and the rank of its knot, later the round of a partner and the knot it was
	// counted for. Per contact its awake bodies, null for a mesh contact, and its knot. Per knot its contacts and batches,
	// per round of the current knot its open batch, and per batch its contacts. There are at most as many knots, rounds and
	// batches as contacts.
	int* temporary = (int*)b3StackAlloc(
		&world->stack, ( 2 * awakeBodyCount + ( 12 + B3_KNOT_LANES ) * n + 2 ) * (int)sizeof( int ), "knot work" );
	int* bodyCounts = temporary;
	int* bodyMarks = bodyCounts + awakeBodyCount;
	int* bodiesA = bodyMarks + awakeBodyCount;
	int* bodiesB = bodiesA + n;
	int* contactKnots = bodiesB + n;
	int* knotBodies = contactKnots + n;
	int* knotStarts = knotBodies + n;
	int* knotCursors = knotStarts + n + 1;
	int* knotOrder = knotCursors + n;
	int* knotBatchStarts = knotOrder + n;
	int* roundBatches = knotBatchStarts + n + 1;
	int* roundKnots = roundBatches + n;
	int* batchLanes = roundKnots + n;
	int* batchSlots = batchLanes + n;

	// The bodies of the contacts, which lie scattered in memory, in one pass
	for ( int k = 0; k < n; ++k )
	{
		if ( k + 8 < n )
		{
			b3Prefetch( &contacts[specs[k + 8].contactId].encodedBodySimA );
		}

		const b3Contact* contact = contacts + specs[k].contactId;
		bool mesh = ( contact->flags & b3_simMeshContact ) != 0;
		int a = mesh ? B3_NULL_INDEX : b3DecodeAwakeIndex( contact->encodedBodySimA );
		int b = mesh ? B3_NULL_INDEX : b3DecodeAwakeIndex( contact->encodedBodySimB );
		bodiesA[k] = a;
		bodiesB[k] = b;
		if ( a != B3_NULL_INDEX )
		{
			bodyCounts[a] = 0;
			bodyMarks[a] = B3_NULL_INDEX;
		}
		if ( b != B3_NULL_INDEX )
		{
			bodyCounts[b] = 0;
			bodyMarks[b] = B3_NULL_INDEX;
		}
	}

	for ( int k = 0; k < n; ++k )
	{
		if ( bodiesA[k] != B3_NULL_INDEX )
		{
			bodyCounts[bodiesA[k]] += 1;
		}
		if ( bodiesB[k] != B3_NULL_INDEX )
		{
			bodyCounts[bodiesB[k]] += 1;
		}
	}

	// The knot of every contact, and the knots ranked by their first contact
	int knotCount = 0;
	for ( int k = 0; k < n; ++k )
	{
		int a = bodiesA[k];
		int b = bodiesB[k];
		int knot = a;
		if ( b != B3_NULL_INDEX && ( a == B3_NULL_INDEX || bodyCounts[b] > bodyCounts[a] ) )
		{
			knot = b;
		}

		contactKnots[k] = B3_NULL_INDEX;
		if ( knot == B3_NULL_INDEX || bodyCounts[knot] < B3_KNOT_MIN_CONTACTS )
		{
			continue;
		}

		contactKnots[k] = knot;
		if ( bodyMarks[knot] == B3_NULL_INDEX )
		{
			bodyMarks[knot] = knotCount;
			knotBodies[knotCount] = knot;
			knotCount += 1;
		}
	}

	// The contacts of every knot in their order, and the others apart
	for ( int r = 0; r <= knotCount; ++r )
	{
		knotStarts[r] = 0;
	}
	for ( int k = 0; k < n; ++k )
	{
		if ( contactKnots[k] != B3_NULL_INDEX )
		{
			knotStarts[bodyMarks[contactKnots[k]] + 1] += 1;
		}
	}
	for ( int r = 0; r < knotCount; ++r )
	{
		knotStarts[r + 1] += knotStarts[r];
		knotCursors[r] = knotStarts[r];
	}

	int otherCount = 0;
	for ( int k = 0; k < n; ++k )
	{
		int knot = contactKnots[k];
		if ( knot == B3_NULL_INDEX )
		{
			knots->others[otherCount] = specs[k];
			otherCount += 1;
			continue;
		}

		int rank = bodyMarks[knot];
		knotOrder[knotCursors[rank]] = k;
		knotCursors[rank] += 1;
	}

	// The batches, knot after knot. The ranks of the knots are not needed anymore, a partner is now marked with the rank of
	// the knot it was counted for, below B3_NULL_INDEX to tell it from what is left. A batch is open for its round until it
	// is full.
	for ( int i = 0; i < n; ++i )
	{
		roundKnots[i] = B3_NULL_INDEX;
	}

	int batchCount = 0;
	int maxBatches = 0;
	for ( int r = 0; r < knotCount; ++r )
	{
		int mark = -2 - r;
		knotBatchStarts[r] = batchCount;
		for ( int i = knotStarts[r]; i < knotStarts[r + 1]; ++i )
		{
			int k = knotOrder[i];
			int partner = bodiesA[k] == knotBodies[r] ? bodiesB[k] : bodiesA[k];
			int round = 0;
			if ( partner != B3_NULL_INDEX )
			{
				if ( bodyMarks[partner] != mark )
				{
					bodyMarks[partner] = mark;
					bodyCounts[partner] = 0;
				}

				round = bodyCounts[partner];
				bodyCounts[partner] += 1;
			}

			if ( roundKnots[round] != r || batchLanes[roundBatches[round]] == B3_KNOT_LANES )
			{
				roundKnots[round] = r;
				roundBatches[round] = batchCount;
				batchLanes[batchCount] = 0;
				batchCount += 1;
			}

			int batch = roundBatches[round];
			batchSlots[B3_KNOT_LANES * batch + batchLanes[batch]] = k;
			batchLanes[batch] += 1;
		}
		maxBatches = b3MaxInt( maxBatches, batchCount - knotBatchStarts[r] );
	}
	knotBatchStarts[knotCount] = batchCount;

	// The knots take turns, batch after batch, so the batches next to each other rarely share a knot. With packs pack by
	// pack in that order.
	int* slots = knotOrder;
	int slotCount = 0;
	for ( int i = 0; i < maxBatches; ++i )
	{
		for ( int r = 0; r < knotCount; ++r )
		{
			if ( knotBatchStarts[r] + i < knotBatchStarts[r + 1] )
			{
				slots[slotCount] = knotBatchStarts[r] + i;
				slotCount += 1;
			}
		}
	}
	B3_ASSERT( slotCount == batchCount );

	if ( packCount > 0 )
	{
		int* packStarts = knots->packStarts;
		for ( int i = 0; i < batchCount; ++i )
		{
			int k = batchSlots[B3_KNOT_LANES * slots[i]];
			packStarts[contactPacks[specs[k].contactId] + 1] += 1;
		}
		for ( int p = 0; p < packCount; ++p )
		{
			packStarts[p + 1] += packStarts[p];
		}
	}

	int packCursors[B3_MAX_PACKS] = { 0 };
	int contactCount = 0;
	for ( int i = 0; i < batchCount; ++i )
	{
		int batch = slots[i];
		int laneCount = batchLanes[batch];
		const int* lanes = batchSlots + B3_KNOT_LANES * batch;
		int knot = contactKnots[lanes[0]];

		int position = i;
		if ( packCount > 0 )
		{
			int pack = contactPacks[specs[lanes[0]].contactId];
			position = knots->packStarts[pack] + packCursors[pack];
			packCursors[pack] += 1;
		}

		knots->batches[position] = (b3KnotBatch){ knot + 1, laneCount };
		int* ids = knots->contactIds + B3_KNOT_LANES * position;
		float* scaleA = knots->scaleA + B3_KNOT_LANES * position;
		float* scaleB = knots->scaleB + B3_KNOT_LANES * position;
		for ( int lane = 0; lane < B3_KNOT_LANES; ++lane )
		{
			ids[lane] = B3_NULL_INDEX;
			scaleA[lane] = 1.0f;
			scaleB[lane] = 1.0f;
			if ( lane < laneCount )
			{
				int k = lanes[lane];
				bool knotIsA = bodiesA[k] == knot;
				ids[lane] = specs[k].contactId;
				scaleA[lane] = knotIsA ? (float)laneCount : 1.0f;
				scaleB[lane] = knotIsA ? 1.0f : (float)laneCount;
			}
		}
		contactCount += laneCount;
	}

	knots->batchCount = batchCount;
	knots->contactCount = contactCount;
	knots->otherCount = otherCount;
	B3_ASSERT( contactCount + otherCount == n );

	b3StackFree( &world->stack, temporary );
}

// Added for Nebenan: gives back the memory of b3BuildKnots
static void b3FreeKnots( b3World* world, b3Knots* knots )
{
	b3StackFree( &world->stack, knots->memory );
}

void b3Solve( b3World* world, b3StepContext* stepContext )
{
	// Added for Nebenan: wide constraints in the width of the world, see b3World::simdWidth
	int simdShift = world->simdWidth == 8 ? 3 : 2;

	// Only count steps that advance the simulation
	world->stepIndex += 1;

	b3SolverSet* awakeSet = b3Array_Get( world->solverSets, b3_awakeSet );
	int awakeBodyCount = awakeSet->bodySims.count;

	// Added for Nebenan: see b3Counters::knotContactCount
	world->knotContactCount = 0;
	world->knotBatchCount = 0;

	if ( awakeBodyCount == 0 )
	{
		return;
	}

	// Solve constraints using graph coloring
	{
		b3TracyCZoneNC( solver_setup, "Solver Setup", b3_colorDarkOrange, true );
		uint64_t setupTicks = b3GetTicks();

		// Prepare buffers for continuous collision (fast bodies)
		b3AtomicStoreInt( &stepContext->bulletBodyCount, 0 );
		stepContext->bulletBodies = (int*)b3StackAlloc( &world->stack, awakeBodyCount * sizeof( int ), "bullet bodies" );

		b3ConstraintGraph* graph = &world->constraintGraph;
		b3GraphColor* colors = graph->colors;

		stepContext->sims = awakeSet->bodySims.data;
		stepContext->states = awakeSet->bodyStates.data;

		// count contacts, joints, and colors
		int activeColorCount = 0;
		for ( int i = 0; i < B3_GRAPH_COLOR_COUNT - 1; ++i )
		{
			int perColorContactCount = colors[i].convexContacts.count + colors[i].contacts.count;
			int perColorJointCount = colors[i].jointSims.count;
			int occupancyCount = perColorContactCount + perColorJointCount;
			activeColorCount += occupancyCount > 0 ? 1 : 0;
		}

		// prepare for move events
		b3Array_Resize( world->bodyMoveEvents, awakeBodyCount );

		// Added for Nebenan: the workers the step wakes, see b3GetStepWorkerCount
		int workerCount = world->activeWorkerCount;

		// Target 4 blocks per worker to allow work stealing
		const int maxBlockCount = 4 * workerCount;

		// Body blocks are for parallel iteration over bodies directly (integration, update transforms)
		int minBodiesPerBlock = 32;
		b3BlockDim bodyDim = b3ComputeBlockCount( awakeBodyCount, minBodiesPerBlock, maxBlockCount );

		const int minContactsPerBlock = 4;
		const int minJointsPerBlock = 4;

		// Configure blocks for tasks parallel-for each active graph color
		// The blocks are a mix of convex contact, mesh contact, and joint blocks
		int activeColorIndices[B3_GRAPH_COLOR_COUNT];
		int colorWideContactCounts[B3_GRAPH_COLOR_COUNT];
		int colorContactCounts[B3_GRAPH_COLOR_COUNT];
		// int colorManifoldCounts[B3_GRAPH_COLOR_COUNT];
		int colorJointCounts[B3_GRAPH_COLOR_COUNT];
		b3BlockDim graphWideContactDims[B3_GRAPH_COLOR_COUNT];
		b3BlockDim graphContactDims[B3_GRAPH_COLOR_COUNT];
		b3BlockDim graphJointDims[B3_GRAPH_COLOR_COUNT];
		int graphBlockCount = 0;

		// c is the active color index
		int wideContactCount = 0;
		int contactCount = 0;
		int manifoldCount = 0;
		int jointCount = 0;
		int c = 0;
		for ( int i = 0; i < B3_GRAPH_COLOR_COUNT - 1; ++i )
		{
			b3GraphColor* color = colors + i;
			int colorConvexContactCount = color->convexContacts.count;
			int colorContactCount = color->contacts.count;
			int colorJointCount = color->jointSims.count;

			if ( colorConvexContactCount + colorContactCount + colorJointCount == 0 )
			{
				continue;
			}

			activeColorIndices[c] = i;

			// Ceiling for wide constraint count
			int colorWideConstraintCount =
				colorConvexContactCount > 0 ? ( ( colorConvexContactCount - 1 ) >> simdShift ) + 1 : 0;
			wideContactCount += colorWideConstraintCount;
			colorWideContactCounts[c] = colorWideConstraintCount;

			colorContactCounts[c] = colorContactCount;
			contactCount += colorContactCount;

			// Compute manifold starts and accumulate manifold count
			for ( int j = 0; j < colorContactCount; ++j )
			{
				color->contacts.data[j].manifoldStart = manifoldCount;
				manifoldCount += color->contacts.data[j].manifoldCount;
			}

			colorJointCounts[c] = colorJointCount;
			jointCount += colorJointCount;

			// Solver block dimensions
			graphWideContactDims[c] = b3ComputeBlockCount( colorWideConstraintCount, minContactsPerBlock, maxBlockCount );
			graphContactDims[c] = b3ComputeBlockCount( colorContactCount, minContactsPerBlock, maxBlockCount );
			graphJointDims[c] = b3ComputeBlockCount( colorJointCount, minJointsPerBlock, maxBlockCount );
			graphBlockCount += graphWideContactDims[c].count + graphContactDims[c].count + graphJointDims[c].count;

			c += 1;
		}
		activeColorCount = c;

		b3GraphColor* overflow = colors + B3_OVERFLOW_INDEX;
		int overflowCount = overflow->contacts.count;

		// Added for Nebenan: whole islands per thread if the step allows, see b3SolvePacks. Joints, mesh contacts and the
		// propagation of restitution, which needs to know whether any constraint of the step has restitution before the
		// first pack starts, keep the barrier per stage, and so does a step while other programs take the cores, see
		// b3UpdatePackHold. With packs the convex contacts of every color go pack by pack, which can take more wide
		// constraints.
		bool held = world->packHoldSteps > 0;
		if ( held )
		{
			world->packHoldSteps -= 1;
			world->packHoldGap = 0;
		}
		else if ( world->packHoldGap < 2 * B3_PACK_HOLD_MAX )
		{
			world->packHoldGap += 1;
		}

		int packCount = 0;

		// Added for Nebenan: the work of the islands of this step, from b3BuildPacks or b3MeasureIslandWork
		world->islandWork = 0;
		world->largestIslandWork = 0;
		uint8_t* contactPacks = NULL;
		int* packBodyStarts = NULL;
		int* packBodies = NULL;
		int* packWideStarts = NULL;
		int* packContacts = NULL;
		int packEntryCounts[B3_GRAPH_COLOR_COUNT];
		if ( held == false && workerCount > 1 && contactCount == 0 && jointCount == 0 && overflow->jointSims.count == 0 &&
			 overflowCount <= UINT16_MAX && world->enableRestitutionPropagation == false )
		{
			int packLimit = B3_PACKS_PER_WORKER * workerCount;
			int contactIdCapacity = b3GetIdCapacity( &world->contactIdPool );
			contactPacks = (uint8_t*)b3StackAlloc( &world->stack, contactIdCapacity, "contact packs" );
			packBodyStarts = (int*)b3StackAlloc( &world->stack, ( packLimit + 1 ) * (int)sizeof( int ), "pack body starts" );
			packBodies = (int*)b3StackAlloc( &world->stack, awakeBodyCount * (int)sizeof( int ), "pack bodies" );
			packCount = b3BuildPacks( world, awakeSet, packLimit, contactPacks, packBodyStarts, packBodies );

			if ( packCount > 0 )
			{
				packWideStarts = (int*)b3StackAlloc( &world->stack, activeColorCount * ( packCount + 1 ) * (int)sizeof( int ),
													 "pack wide starts" );
				packContacts = b3OrderPackContacts( world, colors, activeColorIndices, activeColorCount, contactPacks, packCount,
													packWideStarts, packEntryCounts );

				for ( int i = 0; i < activeColorCount; ++i )
				{
					int colorWideConstraintCount = packEntryCounts[i] >> simdShift;
					wideContactCount += colorWideConstraintCount - colorWideContactCounts[i];
					colorWideContactCounts[i] = colorWideConstraintCount;
					graphBlockCount -= graphWideContactDims[i].count;
					graphWideContactDims[i] = b3ComputeBlockCount( colorWideConstraintCount, minContactsPerBlock, maxBlockCount );
					graphBlockCount += graphWideContactDims[i].count;
				}
			}
			else
			{
				b3StackFree( &world->stack, packBodies );
				b3StackFree( &world->stack, packBodyStarts );
				b3StackFree( &world->stack, contactPacks );
				packBodies = NULL;
				packBodyStarts = NULL;
				contactPacks = NULL;
			}
		}
		else if ( world->workerCount > 1 )
		{
			b3MeasureIslandWork( world, awakeSet );
		}

		// Added for Nebenan: the knots of the overflow in batches, whose wide constraints follow those of the colors, see
		// b3BuildKnots
		b3Knots knots;
		b3BuildKnots( world, awakeSet, overflow, contactPacks, packCount, &knots );
		world->knotContactCount = knots.contactCount;
		world->knotBatchCount = knots.batchCount;
		int knotWideStart = wideContactCount;
		wideContactCount += knots.batchCount * ( B3_KNOT_LANES >> simdShift );

		// Prepare and store run as one flat parallel-for over the entire wide constraint range,
		// partitioned into uniformly sized blocks. Color info is consulted inside the task via
		// a small span array, so blocks do not need to honor color boundaries here.
		b3BlockDim convexPrepareDim = b3ComputeBlockCount( wideContactCount, minContactsPerBlock, maxBlockCount );
		b3BlockDim meshPrepareDim = b3ComputeBlockCount( contactCount, minContactsPerBlock, maxBlockCount );
		b3BlockDim jointPrepareDim = b3ComputeBlockCount( jointCount, minJointsPerBlock, maxBlockCount );

		int wideContactByteCount = b3GetWideContactConstraintByteCount( world->simdWidth );
		void* wideConstraints = b3StackAlloc( &world->stack, wideContactCount * wideContactByteCount, "wide contacts" );
		b3ContactConstraint* contactConstraints =
			(b3ContactConstraint*)b3StackAlloc( &world->stack, contactCount * sizeof( b3ContactConstraint ), "contacts" );
		b3ManifoldConstraint* manifoldConstraints = (b3ManifoldConstraint*)b3StackAlloc(
			&world->stack, manifoldCount * sizeof( b3ManifoldConstraint ), "manifold constraints" );

		// Added for Nebenan: the constraints of the overflow outside the knots, see b3BuildKnots
		int otherCount = knots.otherCount;
		int overflowManifoldCount = 0;
		for ( int i = 0; i < otherCount; ++i )
		{
			knots.others[i].manifoldStart = overflowManifoldCount;
			overflowManifoldCount += knots.others[i].manifoldCount;
		}

		overflow->contactConstraints =
			(b3ContactConstraint*)b3StackAlloc( &world->stack, otherCount * sizeof( b3ContactConstraint ), "overflow contacts" );
		overflow->manifoldConstraints = (b3ManifoldConstraint*)b3StackAlloc(
			&world->stack, overflowManifoldCount * sizeof( b3ManifoldConstraint ), "overflow manifolds" );

		// Added for Nebenan: with packs the overflow contacts go pack by pack, see b3SolvePacks. A body has all its overflow
		// contacts in its pack, in the same order.
		int* packOverflowStarts = NULL;
		b3ContactSpec* packOverflowContacts = NULL;
		if ( packCount > 0 )
		{
			packOverflowStarts =
				(int*)b3StackAlloc( &world->stack, ( packCount + 1 ) * (int)sizeof( int ), "pack overflow starts" );
			packOverflowContacts = (b3ContactSpec*)b3StackAlloc(
				&world->stack, b3MaxInt( otherCount, 1 ) * (int)sizeof( b3ContactSpec ), "pack overflow contacts" );

			int cursors[B3_MAX_PACKS + 1] = { 0 };
			for ( int k = 0; k < otherCount; ++k )
			{
				cursors[contactPacks[knots.others[k].contactId] + 1] += 1;
			}

			for ( int p = 0; p < packCount; ++p )
			{
				cursors[p + 1] += cursors[p];
			}

			for ( int p = 0; p <= packCount; ++p )
			{
				packOverflowStarts[p] = cursors[p];
			}

			for ( int k = 0; k < otherCount; ++k )
			{
				const b3ContactSpec* spec = knots.others + k;
				packOverflowContacts[cursors[contactPacks[spec->contactId]]++] = *spec;
			}
		}

		// Build the span table for the flat prepare/store parallel-for while I slice the
		// wide constraint buffer across colors. One entry per active color plus a sentinel
		// at wideContactCount.
		b3WidePrepareSpan widePrepareSpans[B3_GRAPH_COLOR_COUNT + 2];
		b3ContactPrepareSpan contactPrepareSpans[B3_GRAPH_COLOR_COUNT + 1];
		b3JointPrepareSpan jointPrepareSpans[B3_GRAPH_COLOR_COUNT + 1];

		// Added for Nebenan, with packs see above
		int* orderedContacts = packCount > 0 ? packContacts
											 : b3OrderColorContacts( world, colors, activeColorIndices, activeColorCount,
																	 awakeBodyCount, workerCount );
		int orderedOffset = 0;

		// Distribute transient constraints to each graph color and prepare spans
		// todo it might be simpler for solver blocks to index into the global arrays
		{
			int wideBase = 0;
			int contactBase = 0;
			int jointBase = 0;
			for ( int i = 0; i < activeColorCount; ++i )
			{
				int j = activeColorIndices[i];
				b3GraphColor* color = colors + j;

				// Added for Nebenan: with packs the entries include the empty lanes, see b3OrderPackContacts
				int colorConvexContactCount = packCount > 0 ? packEntryCounts[i] : color->convexContacts.count;
				widePrepareSpans[i].start = wideBase;
				widePrepareSpans[i].count = colorConvexContactCount;
				widePrepareSpans[i].contacts = color->convexContacts.data;
				widePrepareSpans[i].scaleA = NULL;
				widePrepareSpans[i].scaleB = NULL;

				// Added for Nebenan: the prepare and store steps take the contacts in the order of b3OrderColorContacts, or of
				// b3OrderPackContacts
				if ( orderedContacts != NULL )
				{
					widePrepareSpans[i].contacts = orderedContacts + orderedOffset;
					orderedOffset += colorConvexContactCount;
				}

				if ( colorConvexContactCount == 0 )
				{
					color->wideConstraints = NULL;
					color->wideConstraintCount = 0;
				}
				else
				{
					color->wideConstraints =
						( (uint8_t*)wideConstraints + wideBase * wideContactByteCount );

					int colorContactCountW = ( ( colorConvexContactCount - 1 ) >> simdShift ) + 1;
					color->wideConstraintCount = colorContactCountW;
					wideBase += colorContactCountW;
				}

				int colorContactCount = color->contacts.count;
				contactPrepareSpans[i].start = contactBase;
				contactPrepareSpans[i].count = colorContactCount;
				contactPrepareSpans[i].contacts = color->contacts.data;

				if ( colorContactCount == 0 )
				{
					color->contactConstraints = NULL;
					color->contactConstraintCount = 0;
				}
				else
				{
					color->contactConstraints = contactConstraints + contactBase;
					color->contactConstraintCount = colorContactCount;
					contactBase += colorContactCount;
				}

				jointPrepareSpans[i].start = jointBase;
				jointPrepareSpans[i].count = color->jointSims.count;
				jointPrepareSpans[i].joints = color->jointSims.data;
				jointBase += color->jointSims.count;
			}

			// Added for Nebenan: the knots after the colors, see b3BuildKnots
			B3_ASSERT( wideBase == knotWideStart );
			widePrepareSpans[activeColorCount].start = knotWideStart;
			widePrepareSpans[activeColorCount].count = B3_KNOT_LANES * knots.batchCount;
			widePrepareSpans[activeColorCount].contacts = knots.contactIds;
			widePrepareSpans[activeColorCount].scaleA = knots.scaleA;
			widePrepareSpans[activeColorCount].scaleB = knots.scaleB;

			// Sentinels
			widePrepareSpans[activeColorCount + 1].start = wideContactCount;
			widePrepareSpans[activeColorCount + 1].count = 0;
			widePrepareSpans[activeColorCount + 1].contacts = NULL;
			widePrepareSpans[activeColorCount + 1].scaleA = NULL;
			widePrepareSpans[activeColorCount + 1].scaleB = NULL;

			contactPrepareSpans[activeColorCount].start = contactCount;
			contactPrepareSpans[activeColorCount].count = 0;
			contactPrepareSpans[activeColorCount].contacts = NULL;
			B3_ASSERT( contactBase == contactCount );

			jointPrepareSpans[activeColorCount].start = jointCount;
			jointPrepareSpans[activeColorCount].count = 0;
			jointPrepareSpans[activeColorCount].joints = NULL;
			B3_ASSERT( jointBase == jointCount );
		}

		//// Special span for overflow to allow for function re-use
		// Added for Nebenan: without the contacts of knots
		b3ContactPrepareSpan overflowSpans[2] = { 0 };
		overflowSpans[0].start = 0;
		overflowSpans[0].count = otherCount;
		overflowSpans[0].contacts = packOverflowContacts != NULL ? packOverflowContacts : knots.others;
		overflowSpans[1].start = otherCount;
		overflowSpans[1].count = 0;
		overflowSpans[1].contacts = NULL;

		int stageCount = 0;

		// b3_stagePrepareJoints
		stageCount += 1;
		// b3_stagePrepareWideContacts
		stageCount += 1;
		// b3_stagePrepareContacts
		stageCount += 1;
		// b3_stageIntegrateVelocities
		stageCount += 1;
		// b3_stageWarmStart
		stageCount += activeColorCount;
		// b3_stageSolve
		stageCount += ITERATIONS * activeColorCount;
		// b3_stageIntegratePositions
		stageCount += 1;
		// b3_stageRelax
		stageCount += RELAX_ITERATIONS * activeColorCount;
		// b3_stageRestitution
		stageCount += activeColorCount;
		// b3_stageStoreWideImpulses
		stageCount += 1;
		// b3_stageStoreImpulses
		stageCount += 1;

		b3SolverStage* stages = (b3SolverStage*)b3StackAlloc( &world->stack, stageCount * sizeof( b3SolverStage ), "stages" );
		b3SyncBlock* bodyBlocks =
			(b3SyncBlock*)b3StackAlloc( &world->stack, bodyDim.count * sizeof( b3SyncBlock ), "body blocks" );
		b3SyncBlock* convexBlocks =
			(b3SyncBlock*)b3StackAlloc( &world->stack, convexPrepareDim.count * sizeof( b3SyncBlock ), "convex blocks" );
		b3SyncBlock* meshBlocks =
			(b3SyncBlock*)b3StackAlloc( &world->stack, meshPrepareDim.count * sizeof( b3SyncBlock ), "mesh blocks" );
		b3SyncBlock* jointBlocks =
			(b3SyncBlock*)b3StackAlloc( &world->stack, jointPrepareDim.count * sizeof( b3SyncBlock ), "joint blocks" );
		b3SyncBlock* graphBlocks =
			(b3SyncBlock*)b3StackAlloc( &world->stack, graphBlockCount * sizeof( b3SyncBlock ), "graph blocks" );

		// Split an awake island. This modifies:
		// - stack allocator
		// - world island array and solver set
		// - island indices on bodies, contacts, and joints
		// I'm squeezing this task in here because it may be expensive and this is a safe place to put it.
		// Note: cannot split islands in parallel with FinalizeBodies
		void* splitIslandTask = NULL;
		if ( world->splitIslandId != B3_NULL_INDEX )
		{
			// Added for Nebenan: a step on the calling thread alone splits the island right here
			if ( world->taskCount < B3_MAX_TASKS && world->activeWorkerCount > 1 )
			{
				splitIslandTask = world->enqueueTaskFcn( &b3SplitIslandTask, world, world->userTaskContext, "split" );
				world->taskCount += 1;
				world->activeTaskCount += splitIslandTask == NULL ? 0 : 1;
			}
			else
			{
				b3SplitIslandTask( world );
			}
		}

		// Prepare body blocks
		b3InitBlocks( bodyBlocks, bodyDim, awakeBodyCount, b3_bodyBlock, UINT8_MAX );

		// Prepare blocks as a single flat parallel-for over the whole constraint range.
		// The task walks spans to decode flat slot indices back to per-color arrays.
		b3InitBlocks( convexBlocks, convexPrepareDim, wideContactCount, b3_wideContactBlock, UINT8_MAX );
		b3InitBlocks( meshBlocks, meshPrepareDim, contactCount, b3_contactBlock, UINT8_MAX );
		b3InitBlocks( jointBlocks, jointPrepareDim, jointCount, b3_jointBlock, UINT8_MAX );

		// Prepare graph work blocks. Each color gets joint blocks followed by contact blocks.
		b3SyncBlock* graphColorBlocks[B3_GRAPH_COLOR_COUNT] = { 0 };
		b3SyncBlock* baseGraphBlock = graphBlocks;
		int graphBlockCounts[B3_GRAPH_COLOR_COUNT] = { 0 };
		for ( int i = 0; i < activeColorCount; ++i )
		{
			graphColorBlocks[i] = baseGraphBlock;

			uint8_t colorIndex = (uint8_t)activeColorIndices[i];
			b3InitBlocks( baseGraphBlock, graphJointDims[i], colorJointCounts[i], b3_graphJointBlock, colorIndex );
			baseGraphBlock += graphJointDims[i].count;

			b3InitBlocks( baseGraphBlock, graphWideContactDims[i], colorWideContactCounts[i], b3_graphWideContactBlock,
						  colorIndex );
			baseGraphBlock += graphWideContactDims[i].count;

			b3InitBlocks( baseGraphBlock, graphContactDims[i], colorContactCounts[i], b3_graphContactBlock, colorIndex );
			baseGraphBlock += graphContactDims[i].count;

			graphBlockCounts[i] = graphJointDims[i].count + graphWideContactDims[i].count + graphContactDims[i].count;
		}

		B3_ASSERT( (ptrdiff_t)( baseGraphBlock - graphBlocks ) == graphBlockCount );

		b3SolverStage* stage = stages;
		stage = b3InitStage( stage, b3_stagePrepareJoints, jointBlocks, jointPrepareDim.count, UINT8_MAX );
		stage = b3InitStage( stage, b3_stagePrepareWideContacts, convexBlocks, convexPrepareDim.count, UINT8_MAX );
		stage = b3InitStage( stage, b3_stagePrepareContacts, meshBlocks, meshPrepareDim.count, UINT8_MAX );
		stage = b3InitStage( stage, b3_stageIntegrateVelocities, bodyBlocks, bodyDim.count, UINT8_MAX );
		stage = b3InitColorStages( stage, b3_stageWarmStart, 1, activeColorCount, graphColorBlocks, graphBlockCounts,
								   activeColorIndices );
		stage = b3InitColorStages( stage, b3_stageSolve, ITERATIONS, activeColorCount, graphColorBlocks, graphBlockCounts,
								   activeColorIndices );
		stage = b3InitStage( stage, b3_stageIntegratePositions, bodyBlocks, bodyDim.count, UINT8_MAX );
		stage = b3InitColorStages( stage, b3_stageRelax, RELAX_ITERATIONS, activeColorCount, graphColorBlocks, graphBlockCounts,
								   activeColorIndices );
		stage = b3InitColorStages( stage, b3_stageRestitution, 1, activeColorCount, graphColorBlocks, graphBlockCounts,
								   activeColorIndices );
		stage = b3InitStage( stage, b3_stageStoreWideImpulses, convexBlocks, convexPrepareDim.count, UINT8_MAX );
		stage = b3InitStage( stage, b3_stageStoreImpulses, meshBlocks, meshPrepareDim.count, UINT8_MAX );

		B3_ASSERT( (int)( stage - stages ) == stageCount );

		B3_ASSERT( workerCount <= B3_MAX_WORKERS );
		b3WorkerContext workerContext[B3_MAX_WORKERS];

		stepContext->graph = graph;
		stepContext->activeColorCount = activeColorCount;
		stepContext->workerCount = workerCount;
		stepContext->stageCount = stageCount;
		stepContext->stages = stages;
		stepContext->wideConstraints = wideConstraints;
		stepContext->widePrepareSpans = widePrepareSpans;
		stepContext->wideContactCount = wideContactCount;
		stepContext->manifoldConstraints = manifoldConstraints;
		stepContext->contactConstraints = contactConstraints;
		stepContext->contactPrepareSpans = contactPrepareSpans;
		stepContext->overflowSpans = overflowSpans;
		stepContext->jointPrepareSpans = jointPrepareSpans;

		// Added for Nebenan
		stepContext->packCount = packCount;
		stepContext->packBodyStarts = packBodyStarts;
		stepContext->packBodies = packBodies;
		stepContext->packOverflowStarts = packOverflowStarts;
		stepContext->packWideStarts = packWideStarts;
		stepContext->activeColorIndices = activeColorIndices;
		stepContext->knotBatches = knots.batches;
		stepContext->knotBatchCount = knots.batchCount;
		stepContext->knotConstraints = (uint8_t*)wideConstraints + knotWideStart * wideContactByteCount;
		stepContext->knotWideStart = knotWideStart;
		stepContext->packKnotStarts = knots.packStarts;
		stepContext->overflowContactCount = otherCount;
		b3AtomicStoreInt( &stepContext->packNext, 0 );
		b3AtomicStoreInt( &stepContext->packDone, 0 );

		b3AtomicStoreU32( &stepContext->atomicSyncBits, 0 );
		b3AtomicStoreInt( &stepContext->mainClaimed, 0 );

		world->profile.solverSetup = b3GetMillisecondsAndReset( &setupTicks );
		b3TracyCZoneEnd( solver_setup );

		b3TracyCZoneNC( solve_constraints, "Solve Constraints", b3_colorIndigo, true );
		uint64_t constraintTicks = b3GetTicks();

		// Added for Nebenan: the events of every worker are gathered below, also of those the step does not wake
		int jointIdCapacity = b3GetIdCapacity( &world->jointIdPool );
		int contactIdCapacity = b3GetIdCapacity( &world->contactIdPool );
		for ( int i = 0; i < world->workerCount; ++i )
		{
			b3TaskContext* taskContext = b3Array_Get( world->taskContexts, i );
			b3SetBitCountAndClear( &taskContext->jointStateBitSet, jointIdCapacity );
			b3SetBitCountAndClear( &taskContext->hitEventBitSet, contactIdCapacity );
			taskContext->hasHitEvents = false;
		}

		for ( int i = 0; i < workerCount; ++i )
		{
			workerContext[i].context = stepContext;
			workerContext[i].workerIndex = i;

			// Added for Nebenan: alone the calling thread solves below as worker 0
			if ( workerCount == 1 )
			{
				workerContext[i].userTask = NULL;
			}
			// Added for Nebenan: the built-in scheduler never runs a task inside the enqueue, so the calling thread can keep
			// worker 0 for itself. It just prepared the stages and has them in its cache. A waiting worker would otherwise
			// take the task first and orchestrate the stages with a cold cache.
			else if ( i == 0 && world->scheduler != NULL )
			{
				workerContext[i].userTask = NULL;
			}
			else if ( world->taskCount < B3_MAX_TASKS )
			{
				char buffer[16];
				snprintf( buffer, sizeof( buffer ), "solve[%d]", i );
				workerContext[i].userTask =
					world->enqueueTaskFcn( &b3SolverTask, workerContext + i, world->userTaskContext, buffer );
				world->taskCount += 1;
				world->activeTaskCount += workerContext[i].userTask == NULL ? 0 : 1;
			}
			else
			{
				workerContext[i].userTask = NULL;
				b3SolverTask( workerContext + i );
			}
		}

		// The calling thread of b3World_Step also enters b3SolverTask as worker 0 and races for the
		// orchestrator slot via the CAS inside. This guarantees progress even when the user's task
		// system can't run the queued worker 0 promptly: it might schedule out of order, have fewer
		// threads than workerCount, or invert priority by parking the calling thread in finishTaskFcn.
		// Whoever wins the CAS becomes the orchestrator; the loser returns and lets the spinner-only
		// path handle workers >0.
		b3WorkerContext callerContext = { stepContext, 0, NULL };
		b3SolverTask( &callerContext );

		// Finish constraint solve
		for ( int i = 0; i < workerCount; ++i )
		{
			if ( workerContext[i].userTask != NULL )
			{
				world->finishTaskFcn( workerContext[i].userTask, world->userTaskContext );
				world->activeTaskCount -= 1;
			}
		}

		// Finish island split
		if ( splitIslandTask != NULL )
		{
			world->finishTaskFcn( splitIslandTask, world->userTaskContext );
			world->activeTaskCount -= 1;
		}
		world->splitIslandId = B3_NULL_INDEX;

		world->profile.constraints = b3GetMillisecondsAndReset( &constraintTicks );
		b3TracyCZoneEnd( solve_constraints );

		b3TracyCZoneNC( update_transforms, "Update Transforms", b3_colorMediumSeaGreen, true );
		uint64_t transformTicks = b3GetTicks();

		// Prepare the island bit set used in body finalization.
		int awakeIslandCount = awakeSet->islandSims.count;
		for ( int i = 0; i < world->workerCount; ++i )
		{
			b3TaskContext* taskContext = world->taskContexts.data + i;
			b3Array_Clear( taskContext->sensorHits );
			b3SetBitCountAndClear( &taskContext->awakeIslandBitSet, awakeIslandCount );
			taskContext->splitIslandId = B3_NULL_INDEX;
			taskContext->splitSleepTime = 0.0f;
		}

		// Finish the user tree task that was queued earlier in the time step. This must be complete before touching the
		// broad-phase.
		if ( world->userTreeTask != NULL )
		{
			world->finishTaskFcn( world->userTreeTask, world->userTaskContext );
			world->userTreeTask = NULL;
			world->activeTaskCount -= 1;
		}

		b3ValidateNoMoved( &world->broadPhase );

		// Finalize bodies. Must happen after the constraint solver and after island splitting.
		b3ParallelFor( world, &b3FinalizeBodiesTask, awakeBodyCount, 16, stepContext, "ccd" );

		// Free in reverse order
		b3StackFree( &world->stack, graphBlocks );
		b3StackFree( &world->stack, jointBlocks );
		b3StackFree( &world->stack, meshBlocks );
		b3StackFree( &world->stack, convexBlocks );
		b3StackFree( &world->stack, bodyBlocks );
		b3StackFree( &world->stack, stages );
		if ( orderedContacts != NULL && packCount == 0 )
		{
			b3StackFree( &world->stack, orderedContacts );
		}
		if ( packCount > 0 )
		{
			b3StackFree( &world->stack, packOverflowContacts );
			b3StackFree( &world->stack, packOverflowStarts );
		}
		b3StackFree( &world->stack, overflow->manifoldConstraints );
		b3StackFree( &world->stack, overflow->contactConstraints );
		b3StackFree( &world->stack, manifoldConstraints );
		b3StackFree( &world->stack, contactConstraints );
		b3StackFree( &world->stack, wideConstraints );
		b3FreeKnots( world, &knots );
		if ( packCount > 0 )
		{
			b3StackFree( &world->stack, packContacts );
			b3StackFree( &world->stack, packWideStarts );
			b3StackFree( &world->stack, packBodies );
			b3StackFree( &world->stack, packBodyStarts );
			b3StackFree( &world->stack, contactPacks );
		}

		world->profile.transforms = b3GetMilliseconds( transformTicks );
		b3TracyCZoneEnd( update_transforms );
	}

	// Report joint events
	{
		b3TracyCZoneNC( joint_events, "Joint Events", b3_colorPeru, true );
		uint64_t jointEventTicks = b3GetTicks();

		// Gather bits for all joints that have force/torque events
		b3BitSet* jointStateBitSet = &world->taskContexts.data[0].jointStateBitSet;
		for ( int i = 1; i < world->workerCount; ++i )
		{
			b3InPlaceUnion( jointStateBitSet, &world->taskContexts.data[i].jointStateBitSet );
		}

		{
			uint32_t wordCount = jointStateBitSet->blockCount;
			uint64_t* bits = jointStateBitSet->bits;

			b3Joint* jointArray = world->joints.data;
			uint16_t worldIndex0 = world->worldId;

			for ( uint32_t k = 0; k < wordCount; ++k )
			{
				uint64_t word = bits[k];
				while ( word != 0 )
				{
					uint32_t ctz = b3CTZ64( word );
					int jointId = (int)( 64 * k + ctz );

					B3_ASSERT( jointId < world->joints.capacity );

					b3Joint* joint = jointArray + jointId;

					B3_ASSERT( joint->setIndex == b3_awakeSet );

					b3JointEvent event = {
						.jointId =
							{
								.index1 = jointId + 1,
								.world0 = worldIndex0,
								.generation = joint->generation,
							},
						.userData = joint->userData,
					};

					b3Array_Push( world->jointEvents, event );

					// Clear the smallest set bit
					word = word & ( word - 1 );
				}
			}
		}

		world->profile.jointEvents = b3GetMilliseconds( jointEventTicks );
		b3TracyCZoneEnd( joint_events );
	}

	// Report hit events
	{
		b3TracyCZoneNC( hit_events, "Hit Events", b3_colorRosyBrown, true );
		uint64_t hitTicks = b3GetTicks();

		B3_ASSERT( world->contactHitEvents.count == 0 );

		// Fast path: if no worker flagged any hit-event candidates during b2StoreImpulsesTask, skip entirely.
		bool anyHitEvents = false;
		for ( int i = 0; i < world->workerCount; ++i )
		{
			if ( world->taskContexts.data[i].hasHitEvents )
			{
				anyHitEvents = true;
				break;
			}
		}

		if ( anyHitEvents )
		{
			// Union per-worker bits into worker 0's bit set.
			b3BitSet* hitEventBitSet = &world->taskContexts.data[0].hitEventBitSet;
			for ( int i = 1; i < world->workerCount; ++i )
			{
				if ( world->taskContexts.data[i].hasHitEvents )
				{
					b3InPlaceUnion( hitEventBitSet, &world->taskContexts.data[i].hitEventBitSet );
				}
			}

			float threshold = world->hitEventThreshold;
			b3Contact* contactArray = world->contacts.data;
			uint16_t worldId = world->worldId;

			uint32_t wordCount = hitEventBitSet->blockCount;
			uint64_t* bits = hitEventBitSet->bits;
			for ( uint32_t k = 0; k < wordCount; ++k )
			{
				uint64_t word = bits[k];
				while ( word != 0 )
				{
					uint32_t ctz = b3CTZ64( word );
					int contactId = (int)( 64 * k + ctz );

					b3Contact* contact = contactArray + contactId;
					B3_ASSERT( contact->setIndex == b3_awakeSet && contact->colorIndex != B3_NULL_INDEX );

					b3Shape* shapeA = b3Array_Get( world->shapes, contact->shapeIdA );
					b3Shape* shapeB = b3Array_Get( world->shapes, contact->shapeIdB );
					b3Body* bodyA = b3Array_Get( world->bodies, shapeA->bodyId );
					b3Body* bodyB = b3Array_Get( world->bodies, shapeB->bodyId );
					b3BodySim* simA = b3GetBodySim( world, bodyA );
					b3BodySim* simB = b3GetBodySim( world, bodyB );
					b3Pos midCenter = b3LerpPosition( simA->center, simB->center, 0.5f );

					b3ContactHitEvent event = { 0 };
					event.approachSpeed = threshold;

					bool found = false;
					int triangleIndex = 0;
					int manifoldCount = contact->manifoldCount;
					for ( int i = 0; i < manifoldCount; ++i )
					{
						b3Manifold* manifold = contact->manifolds + i;
						int pointCount = manifold->pointCount;
						for ( int p = 0; p < pointCount; ++p )
						{
							b3ManifoldPoint* mp = manifold->points + p;
							float approachSpeed = -mp->normalVelocity;

							// Need to check total impulse because the point may be speculative and not colliding
							if ( approachSpeed > event.approachSpeed && mp->totalNormalImpulse > 0.0f )
							{
								event.approachSpeed = approachSpeed;
								event.point = b3OffsetPos( midCenter, b3Lerp( mp->anchorA, mp->anchorB, 0.5f ) );
								event.normal = manifold->normal;
								triangleIndex = mp->triangleIndex;
								found = true;
							}
						}
					}

					if ( found == true )
					{
						event.shapeIdA = (b3ShapeId){ shapeA->id + 1, worldId, shapeA->generation };
						event.shapeIdB = (b3ShapeId){ shapeB->id + 1, worldId, shapeB->generation };

						event.contactId = (b3ContactId){
							.index1 = contact->contactId + 1,
							.world0 = worldId,
							.padding = 0,
							.generation = contact->generation,
						};

						// shapeB is never a compound today (asserted in b3CreateContact), so the
						// childIndex argument is irrelevant for it. shapeA carries the compound.
						event.userMaterialIdA = b3GetShapeUserMaterialId( shapeA, contact->childIndex, triangleIndex );
						event.userMaterialIdB = b3GetShapeUserMaterialId( shapeB, 0, triangleIndex );

						b3Array_Push( world->contactHitEvents, event );
					}

					// Clear the smallest set bit
					word = word & ( word - 1 );
				}
			}
		}

		world->profile.hitEvents = b3GetMilliseconds( hitTicks );
		b3TracyCZoneEnd( hit_events );
	}

	{
		b3TracyCZoneNC( refit_bvh, "Refit BVH", b3_colorFireBrick, true );
		uint64_t refitTicks = b3GetTicks();

		b3DynamicTree_Refit( world->broadPhase.trees + b3_kinematicBody );
		b3DynamicTree_Refit( world->broadPhase.trees + b3_dynamicBody );

		b3ValidateBroadPhase( &world->broadPhase );

		world->profile.refit = b3GetMilliseconds( refitTicks );
		b3TracyCZoneEnd( refit_bvh );
	}

	int bulletBodyCount = b3AtomicLoadInt( &stepContext->bulletBodyCount );
	if ( bulletBodyCount > 0 )
	{
		b3TracyCZoneNC( bullets, "Bullets", b3_colorDarkGoldenRod, true );
		uint64_t bulletTicks = b3GetTicks();

		// Fast bullet bodies
		// Note: a bullet body may be moving slow
		int minRange = 8;
		b3ParallelFor( world, &b3BulletBodyTask, bulletBodyCount, minRange, stepContext, "bullets" );

		// Serially enlarge broad-phase proxies for bullet shapes
		b3BroadPhase* broadPhase = &world->broadPhase;
		b3DynamicTree* dynamicTree = broadPhase->trees + b3_dynamicBody;

		// Fast array access is important here
		b3Body* bodyArray = world->bodies.data;
		b3BodySim* bodySimArray = awakeSet->bodySims.data;
		b3Shape* shapeArray = world->shapes.data;

		// Serially enlarge broad-phase proxies for bullet shapes
		int* bulletBodySimIndices = stepContext->bulletBodies;

		// This loop has non-deterministic order but it shouldn't affect the result
		for ( int i = 0; i < bulletBodyCount; ++i )
		{
			b3BodySim* bulletBodySim = bodySimArray + bulletBodySimIndices[i];
			if ( ( bulletBodySim->flags & b3_enlargeBulletBounds ) == 0 )
			{
				continue;
			}

			// Clear flag
			bulletBodySim->flags &= ~b3_enlargeBulletBounds;

			int bodyId = bulletBodySim->bodyId;
			B3_ASSERT( 0 <= bodyId && bodyId < world->bodies.count );
			b3Body* bulletBody = bodyArray + bodyId;

			int shapeId = bulletBody->headShapeId;
			while ( shapeId != B3_NULL_INDEX )
			{
				b3Shape* shape = shapeArray + shapeId;

				int proxyKey = shape->proxyKey;
				int proxyId = B3_PROXY_ID( proxyKey );
				B3_VALIDATE( B3_PROXY_TYPE( proxyKey ) == b3_dynamicBody );

				b3AABB treeAABB = b3DynamicTree_GetAABB( dynamicTree, proxyId );
				b3AABB shapeFatAABB = world->fatAABBs.data[shapeId];

				if ( b3AABB_Contains( treeAABB, shapeFatAABB ) == false )
				{
					b3DynamicTree_EnlargeProxy( dynamicTree, proxyId, shapeFatAABB );
				}

				shapeId = shape->nextShapeId;
			}
		}

		world->profile.bullets = b3GetMilliseconds( bulletTicks );
		b3TracyCZoneEnd( bullets );
	}

	b3StackFree( &world->stack, stepContext->bulletBodies );
	stepContext->bulletBodies = NULL;
	b3AtomicStoreInt( &stepContext->bulletBodyCount, 0 );

	// Report sensor hits. This may include bullets sensor hits.
	{
		b3TracyCZoneNC( sensor_hits, "Sensor Hits", b3_colorPowderBlue, true );
		uint64_t sensorHitTicks = b3GetTicks();

		int workerCount = world->workerCount;
		B3_ASSERT( workerCount == world->taskContexts.count );

		for ( int i = 0; i < workerCount; ++i )
		{
			b3TaskContext* taskContext = world->taskContexts.data + i;
			int hitCount = taskContext->sensorHits.count;
			b3SensorHit* hits = taskContext->sensorHits.data;

			for ( int j = 0; j < hitCount; ++j )
			{
				b3SensorHit hit = hits[j];
				b3Shape* sensorShape = b3Array_Get( world->shapes, hit.sensorId );
				b3Shape* visitor = b3Array_Get( world->shapes, hit.visitorId );

				b3Sensor* sensor = b3Array_Get( world->sensors, sensorShape->sensorIndex );
				b3Visitor shapeRef = {
					.shapeId = hit.visitorId,
					.generation = visitor->generation,
				};
				b3Array_Push( sensor->hits, shapeRef );
			}
		}

		world->profile.sensorHits = b3GetMilliseconds( sensorHitTicks );
		b3TracyCZoneEnd( sensor_hits );
	}

	// Island sleeping
	// This must be done last because putting islands to sleep invalidates the enlarged body bits.
	// todo_erin figure out how to do this in parallel with tree refit
	if ( world->enableSleep == true )
	{
		b3TracyCZoneNC( sleep_islands, "Island Sleep", b3_colorLightSlateGray, true );
		uint64_t sleepTicks = b3GetTicks();

		// Collect split island candidate for the next time step. No need to split if sleeping is disabled.
		B3_ASSERT( world->splitIslandId == B3_NULL_INDEX );
		float splitSleepTimer = 0.0f;
		for ( int i = 0; i < world->workerCount; ++i )
		{
			b3TaskContext* taskContext = world->taskContexts.data + i;
			if ( taskContext->splitIslandId != B3_NULL_INDEX && taskContext->splitSleepTime >= splitSleepTimer )
			{
				B3_ASSERT( taskContext->splitSleepTime > 0.0f );

				// Tie breaking for determinism. Largest island id wins. Needed due to work stealing.
				if ( taskContext->splitSleepTime == splitSleepTimer && taskContext->splitIslandId < world->splitIslandId )
				{
					continue;
				}

				world->splitIslandId = taskContext->splitIslandId;
				splitSleepTimer = taskContext->splitSleepTime;
			}
		}

		b3BitSet* awakeIslandBitSet = &world->taskContexts.data[0].awakeIslandBitSet;
		for ( int i = 1; i < world->workerCount; ++i )
		{
			b3InPlaceUnion( awakeIslandBitSet, &world->taskContexts.data[i].awakeIslandBitSet );
		}

		// Need to process in reverse because this moves islands to sleeping solver sets.
		b3IslandSim* islands = awakeSet->islandSims.data;
		int count = awakeSet->islandSims.count;
		for ( int islandIndex = count - 1; islandIndex >= 0; islandIndex -= 1 )
		{
			if ( b3GetBit( awakeIslandBitSet, islandIndex ) == true )
			{
				// this island is still awake
				continue;
			}

			b3IslandSim* island = islands + islandIndex;
			int islandId = island->islandId;

			b3TrySleepIsland( world, islandId );
		}

		b3ValidateSolverSets( world );

		world->profile.sleepIslands = b3GetMilliseconds( sleepTicks );
		b3TracyCZoneEnd( sleep_islands );
	}
}
