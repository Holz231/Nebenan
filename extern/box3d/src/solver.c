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

		// Added for Nebenan: the transform before the step for the move event
		b3WorldTransform previousTransform = sim->transform;

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
		moveEvents[simIndex].previousTransform = previousTransform;
		moveEvents[simIndex].localCenter = sim->localCenter;
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

// Added for Nebenan: solver blocks of up to blockSize items over [start, start + itemCount), for the ranges of the packs of
// a team, see b3SolveOtherPack. Returns how many.
static int b3InitRangeBlocks( b3SyncBlock* blocks, int start, int itemCount, int blockSize, uint8_t blockType,
							  uint8_t colorIndex )
{
	B3_ASSERT( itemCount == 0 || ( 0 < blockSize && blockSize <= UINT16_MAX ) );

	int blockCount = 0;
	while ( itemCount > 0 )
	{
		int count = b3MinInt( itemCount, blockSize );
		blocks[blockCount].block.startIndex = start;
		blocks[blockCount].block.count = (uint16_t)count;
		blocks[blockCount].block.blockType = blockType;
		blocks[blockCount].block.colorIndex = colorIndex;
		b3AtomicStoreInt( &blocks[blockCount].syncIndex, 0 );
		blockCount += 1;
		start += count;
		itemCount -= count;
	}
	return blockCount;
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

// Added for Nebenan: the knots in several groups in [start, end) take the mean velocity of their copies once all groups
// are through, see b3KnotSplit. A kinematic knot keeps its own.
static void b3MergeKnots( b3StepContext* context, int start, int end )
{
	b3BodyState* states = context->states;
	const b3KnotSplit* splits = context->knotSplits;
	const b3KnotCopy* copies = context->knotCopies;
	for ( int i = start; i < end; ++i )
	{
		const b3KnotSplit* split = splits + i;
		b3BodyState* state = states + split->knot;
		if ( ( state->flags & b3_dynamicFlag ) == 0 )
		{
			continue;
		}

		b3Vec3 v0 = state->linearVelocity;
		b3Vec3 w0 = state->angularVelocity;
		b3Vec3 dv = b3Vec3_zero;
		b3Vec3 dw = b3Vec3_zero;
		for ( int j = 0; j < split->copyCount; ++j )
		{
			const b3KnotCopy* copy = copies + split->firstCopy + j;
			dv = b3Add( dv, b3Sub( copy->linearVelocity, v0 ) );
			dw = b3Add( dw, b3Sub( copy->angularVelocity, w0 ) );
		}

		float share = 1.0f / (float)split->copyCount;
		state->linearVelocity = b3MulAdd( v0, share, dv );
		state->angularVelocity = b3MulAdd( w0, share, dw );
	}
}

// Added for Nebenan: what the blocks of the solver work on, see b3Profile::solverJoints. Every this many steps a step times
// its blocks, the others read no clock for it.
#define B3_SOLVER_SAMPLE 8

typedef enum b3SolverPart
{
	b3_solverJoints,
	b3_solverContacts,
	b3_solverBodies,
} b3SolverPart;

static b3SolverPart b3GetSolverPart( b3SolverStageType stageType, b3SolverBlockType blockType )
{
	if ( stageType == b3_stagePrepareJoints || blockType == b3_graphJointBlock )
	{
		return b3_solverJoints;
	}
	if ( stageType == b3_stageIntegrateVelocities || stageType == b3_stageIntegratePositions )
	{
		return b3_solverBodies;
	}
	return b3_solverContacts;
}

// Added for Nebenan: the clock where a step times its solver, zero in the others
static inline uint64_t b3SampleSolverTicks( const b3StepContext* context )
{
	return context->sampleSolver ? b3GetTicks() : 0;
}

// Added for Nebenan: the ticks since start in a step that times its solver, zero in the others
static inline uint64_t b3SolverTicksSince( const b3StepContext* context, uint64_t start )
{
	return context->sampleSolver ? b3GetTicks() - start : 0;
}

// Added for Nebenan: the time since start to a part of the solver on a thread, in a step that times its solver
static inline void b3AddSolverTicks( b3StepContext* context, int workerIndex, b3SolverPart part, uint64_t start )
{
	if ( context->sampleSolver )
	{
		b3TaskContext* taskContext = context->world->taskContexts.data + workerIndex;
		taskContext->solverTicks[part] += b3GetTicks() - start;
	}
}

static void b3RunBlock( b3SolverStage* stage, b3StepContext* context, b3SolverBlock block, int workerIndex )
{
	b3SolverStageType stageType = stage->type;
	b3SolverBlockType blockType = (b3SolverBlockType)block.blockType;

	// Added for Nebenan: one group of batches of knots, see b3BuildKnots
	if ( blockType == b3_knotBlock )
	{
		const int* groupStarts = context->knotGroupStarts;
		b3ExecuteKnots( stageType, context, groupStarts[block.startIndex], groupStarts[block.startIndex + 1] );
		return;
	}

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

static void b3ExecuteBlock( b3SolverStage* stage, b3StepContext* context, b3SolverBlock block, int workerIndex )
{
	uint64_t start = b3SampleSolverTicks( context );
	b3RunBlock( stage, context, block, workerIndex );
	b3AddSolverTicks( context, workerIndex, b3GetSolverPart( stage->type, (b3SolverBlockType)block.blockType ), start );
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
// Added for Nebenan: returns the blocks it solved, see b3TeamEntry
static int b3ExecuteStage( b3SolverStage* stage, b3StepContext* context, int previousSyncIndex, int syncIndex, int workerIndex )
{
	int completedCount = 0;
	b3SyncBlock* blocks = stage->blocks;
	int blockCount = stage->blockCount;

	int startIndex = GetWorkerStartIndex( workerIndex, blockCount, context->workerCount );
	if ( startIndex == B3_NULL_INDEX )
	{
		return 0;
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
	return completedCount;
}

// Added for Nebenan: a stage a thread took part in, in a step that times its team, see b3TeamEntry
static void b3AddTeamEntry( b3StepContext* context, int workerIndex, uint32_t syncBits, uint64_t startTicks, uint64_t doneTicks,
							uint64_t closeTicks, int blockCount, int flags )
{
	b3TaskContext* taskContext = context->world->taskContexts.data + workerIndex;
	B3_ASSERT( taskContext->teamEntryCount < context->teamEntryCapacity );
	if ( taskContext->teamEntryCount == context->teamEntryCapacity )
	{
		return;
	}

	b3TeamEntry* entry = context->teamEntries + workerIndex * context->teamEntryCapacity + taskContext->teamEntryCount;
	*entry = (b3TeamEntry){ startTicks, doneTicks, closeTicks, syncBits, (uint16_t)blockCount, (uint8_t)flags };
	taskContext->teamEntryCount += 1;
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

	// Added for Nebenan: see b3TeamEntry
	uint64_t openTicks = context->sampleTeam ? b3GetTicks() : 0;

	if ( blockCount == 1 )
	{
		b3ExecuteBlock( stage, context, stage->blocks[0].block, workerIndex );

		if ( context->sampleTeam )
		{
			uint64_t closeTicks = b3GetTicks();
			b3AddTeamEntry( context, workerIndex, syncBits, openTicks, closeTicks, closeTicks, 1, b3_teamEntryAlone );
		}
	}
	else
	{
		b3AtomicStoreU32( &context->atomicSyncBits, syncBits );

		int syncIndex = ( syncBits >> 16 ) & 0xFFFF;
		B3_ASSERT( syncIndex > 0 );
		int previousSyncIndex = syncIndex - 1;

		int completedCount = b3ExecuteStage( stage, context, previousSyncIndex, syncIndex, workerIndex );

		// Added for Nebenan: the wait for the others, see b3Profile::teamStageWait
		uint64_t waitTicks = context->sampleTeam ? b3GetTicks() : 0;

		// Spin waiting for thieves to finish
		while ( b3AtomicLoadInt( &stage->completionCount ) != blockCount )
		{
			b3Pause();
		}

		b3AtomicStoreInt( &stage->completionCount, 0 );

		if ( context->sampleTeam )
		{
			uint64_t closeTicks = b3GetTicks();
			b3TaskContext* taskContext = context->world->taskContexts.data + workerIndex;
			taskContext->teamWait += b3TicksToMilliseconds( closeTicks - waitTicks );
			taskContext->teamStages += 1;
			b3AddTeamEntry( context, workerIndex, syncBits, openTicks, waitTicks, closeTicks, completedCount, 0 );
		}
	}
}

// Added for Nebenan: one stage of the batches of knots, a thread per group, after which the knots in several groups take
// the mean velocity of their copies, see b3BuildKnots. With a team only the groups of its packs, the first ones. With one
// group the calling thread solves it alone, as b3ExecuteMainStage does with any single block.
static void b3ExecuteKnotStage( b3SolverStage* stage, b3StepContext* context, uint32_t syncBits )
{
	b3ExecuteMainStage( stage, context, syncBits );

	uint64_t mergeTicks = context->sampleTeam ? b3GetTicks() : 0;
	b3MergeKnots( context, context->packSplitStarts[0], context->packSplitStarts[context->stagePackCount] );
	if ( context->sampleTeam )
	{
		context->teamMergeTime += b3GetMilliseconds( mergeTicks );
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
		b3RunBlock( &stage, context, block, workerIndex );
		start += count;
	}
}

// Added for Nebenan: one stage of the constraints of a pack, the overflow constraints first, the knots before the others,
// and then the colors in order, as b3SolverTask does
static void b3ExecutePackConstraints( b3SolverStageType type, b3StepContext* context, int pack, int workerIndex )
{
	// The groups of the knots of the pack one after the other, see b3BuildKnots
	const int* knotStarts = context->packKnotStarts;
	b3ExecuteKnots( type, context, knotStarts[pack], knotStarts[pack + 1] );
	b3MergeKnots( context, context->packSplitStarts[pack], context->packSplitStarts[pack + 1] );

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

	// Added for Nebenan: a step that times its solver times a pack whole and its bodies apart, see b3Profile::solverJoints.
	// A pack holds no joints, and a clock read at every one of its many small blocks would cost more than some of them.
	uint64_t packStart = b3SampleSolverTicks( context );
	uint64_t bodyTicks = 0;

	b3ExecutePackRange( b3_stagePrepareContacts, context, overflowStart, overflowEnd, b3_overflowBlock, B3_OVERFLOW_INDEX,
						workerIndex );
	b3ExecutePackRange( b3_stagePrepareWideContacts, context, knotWideStart, knotWideEnd, b3_wideContactBlock, UINT8_MAX,
						workerIndex );
	b3PreparePackContacts_Convex( context, pack );

	for ( int i = 0; i < context->subStepCount; ++i )
	{
		uint64_t bodyClock = b3SampleSolverTicks( context );
		b3ExecutePackRange( b3_stageIntegrateVelocities, context, bodyStart, bodyEnd, b3_packBodyBlock, UINT8_MAX, workerIndex );
		bodyTicks += b3SolverTicksSince( context, bodyClock );
		b3ExecutePackConstraints( b3_stageWarmStart, context, pack, workerIndex );
		b3ExecutePackConstraints( b3_stageSolve, context, pack, workerIndex );
		bodyClock = b3SampleSolverTicks( context );
		b3ExecutePackRange( b3_stageIntegratePositions, context, bodyStart, bodyEnd, b3_packBodyBlock, UINT8_MAX, workerIndex );
		bodyTicks += b3SolverTicksSince( context, bodyClock );
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

	if ( context->sampleSolver )
	{
		b3TaskContext* taskContext = context->world->taskContexts.data + workerIndex;
		uint64_t packTicks = b3GetTicks() - packStart;
		taskContext->solverTicks[b3_solverBodies] += bodyTicks;
		taskContext->solverTicks[b3_solverContacts] += packTicks - bodyTicks;
	}
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

// Added for Nebenan: a team for the largest packs. In packs a thread solves an island alone, so a pack with more than the
// even share of a thread sets how long the solver takes. Then all threads solve that pack in the stages, color by color
// with a barrier after every stage, as the solver does without packs, and with it every further pack above that share,
// see b3BuildPacks. Its constraints only fill the same stages further. Whenever a worker finds no new stage, while the
// calling thread solves the overflow of the team alone or the others finish a stage, it solves one of the other packs
// whole instead, and the calling thread takes those left at the end. The bodies of the packs of the team take their steps
// in the same order as in a pack, and two islands share no moving body, so the results stay the same. Returns false once
// all packs are taken.
static bool b3SolveOtherPack( b3StepContext* context, int workerIndex )
{
	if ( b3AtomicLoadInt( &context->packNext ) >= context->packCount )
	{
		return false;
	}

	int pack = b3AtomicFetchAddInt( &context->packNext, 1 );
	if ( pack >= context->packCount )
	{
		return false;
	}

	b3SolvePack( context, pack, workerIndex );
	(void)b3AtomicFetchAddInt( &context->packDone, 1 );
	return true;
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
		// and store their constraints themselves, so no stage is left, and all their time counts as solving. With a team the
		// stages below hold its packs, the first ones, see b3SolveOtherPack.
		if ( context->packCount > 0 && context->team == false )
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
			profile->packWait += wait;
			b3UpdatePackHold( context->world, wait, total );

			// Signal workers to finish
			b3AtomicStoreU32( &context->atomicSyncBits, UINT_MAX );
			return;
		}

		int bodySyncIndex = 1;
		int stageIndex = 0;

		// Added for Nebenan: a stage of the knots before each sweep over the colors, with a team only those of its packs, see
		// b3ExecuteKnotStage. All of them share the blocks of the groups.
		uint64_t teamTicks = ticks;
		uint32_t knotSyncIndex = 1;
		context->teamStartTicks = teamTicks;

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
		uint64_t partStart = b3SampleSolverTicks( context );
		b3PrepareJoints_Overflow( context );
		b3AddSolverTicks( context, 0, b3_solverJoints, partStart );
		partStart = b3SampleSolverTicks( context );
		b3PrepareContacts_Overflow( context );
		b3AddSolverTicks( context, 0, b3_solverContacts, partStart );
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
			partStart = b3SampleSolverTicks( context );
			b3WarmStartJoints_Overflow( context );
			b3AddSolverTicks( context, 0, b3_solverJoints, partStart );
			syncBits = ( knotSyncIndex << 16 ) | iterationStageIndex;
			B3_ASSERT( stages[iterationStageIndex].type == b3_stageWarmStart );
			b3ExecuteKnotStage( stages + iterationStageIndex, context, syncBits );
			iterationStageIndex += 1;
			knotSyncIndex += 1;
			partStart = b3SampleSolverTicks( context );
			b3WarmStartContacts_Overflow( context );
			b3AddSolverTicks( context, 0, b3_solverContacts, partStart );
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
				partStart = b3SampleSolverTicks( context );
				b3SolveJoints_Overflow( context, useBias );
				b3AddSolverTicks( context, 0, b3_solverJoints, partStart );
				syncBits = ( knotSyncIndex << 16 ) | iterationStageIndex;
				B3_ASSERT( stages[iterationStageIndex].type == b3_stageSolve );
				b3ExecuteKnotStage( stages + iterationStageIndex, context, syncBits );
				iterationStageIndex += 1;
				knotSyncIndex += 1;
				partStart = b3SampleSolverTicks( context );
				b3PushContacts_Overflow( context );
				b3AddSolverTicks( context, 0, b3_solverContacts, partStart );
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
				partStart = b3SampleSolverTicks( context );
				b3SolveJoints_Overflow( context, useBias );
				b3AddSolverTicks( context, 0, b3_solverJoints, partStart );
				syncBits = ( knotSyncIndex << 16 ) | iterationStageIndex;
				B3_ASSERT( stages[iterationStageIndex].type == b3_stageRelax );
				b3ExecuteKnotStage( stages + iterationStageIndex, context, syncBits );
				iterationStageIndex += 1;
				knotSyncIndex += 1;
				partStart = b3SampleSolverTicks( context );
				b3SolveContacts_Overflow( context );
				b3AddSolverTicks( context, 0, b3_solverContacts, partStart );
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
		// Added for Nebenan: a stage of the knots before each sweep over the colors
		stageIndex += 1 + ( 1 + activeColorCount ) + ITERATIONS * ( 1 + activeColorCount ) + 1 +
					  RELAX_ITERATIONS * ( 1 + activeColorCount );

		int restitutionIterations = context->world->restitutionIterations;
		if ( restitutionIterations > 0 && b3AtomicLoadInt( &context->anyRestitution ) != 0 )
		{
			for ( int j = 0; j < restitutionIterations; ++j )
			{
				int iterationStageIndex = stageIndex;
				overflowTicks = b3GetTicks();
				syncBits = ( knotSyncIndex << 16 ) | iterationStageIndex;
				B3_ASSERT( stages[iterationStageIndex].type == b3_stageRestitution );
				b3ExecuteKnotStage( stages + iterationStageIndex, context, syncBits );
				iterationStageIndex += 1;
				knotSyncIndex += 1;
				partStart = b3SampleSolverTicks( context );
				b3ApplyRestitution_Overflow( context );
				b3AddSolverTicks( context, 0, b3_solverContacts, partStart );
				profile->overflow += b3GetMilliseconds( overflowTicks );

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

		stageIndex += 1 + activeColorCount;

		// Store impulses
		overflowTicks = b3GetTicks();
		partStart = b3SampleSolverTicks( context );
		b3StoreImpulses_Overflow( context );
		b3AddSolverTicks( context, 0, b3_solverContacts, partStart );
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

		// Added for Nebenan: the packs a team leaves to the others, this thread takes those left and waits for the rest. A
		// long wait counts as with packs, see b3UpdatePackHold.
		if ( context->team )
		{
			while ( b3SolveOtherPack( context, 0 ) )
			{
			}

			uint64_t waitTicks = b3GetTicks();
			int spinCount = 0;
			while ( b3AtomicLoadInt( &context->packDone ) < context->packCount - context->stagePackCount )
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
			profile->solveImpulses += b3GetMillisecondsAndReset( &ticks );
			profile->packWait += wait;
			b3UpdatePackHold( context->world, wait, b3GetMilliseconds( teamTicks ) );

			// Added for Nebenan: see b3Profile::teamTime and b3TeamStages
			if ( context->sampleTeam )
			{
				context->teamEndTicks = b3GetTicks();
				profile->teamTime = b3TicksToMilliseconds( context->teamEndTicks - teamTicks );
				profile->teamOverflow = profile->overflow;
			}
		}

		// Signal workers to finish
		b3AtomicStoreU32( &context->atomicSyncBits, UINT_MAX );

		B3_ASSERT( stageIndex == context->stageCount );
		return;
	}

	// Worker spins and waits for work
	uint32_t lastSyncBits = 0;
	// uint64_t maxSpinTime = 10;

	// Added for Nebenan: the waits of this worker in a step that times its team, see b3Profile::teamWorkerWait
	b3TaskContext* sampleContext = context->sampleTeam ? context->world->taskContexts.data + workerIndex : NULL;
	while ( true )
	{
		// Spin until main thread bumps changes the sync bits. This can waste significant time overall, but it is necessary for
		// parallel simulation with graph coloring.
		// todo improve this spinner
		uint32_t syncBits;
		int spinCount = 0;
		uint64_t waitTicks = sampleContext != NULL ? b3GetTicks() : 0;

		// Added for Nebenan: what this worker did while it waited, see b3TeamEntry
		int waitFlags = 0;
		while ( ( syncBits = b3AtomicLoadU32( &context->atomicSyncBits ) ) == lastSyncBits )
		{
			// Added for Nebenan: without a new stage of the team, one of the other packs, see b3SolveOtherPack. Its time does
			// not count as a wait.
			if ( context->team && b3AtomicLoadInt( &context->packNext ) < context->packCount )
			{
				uint64_t packTicks = sampleContext != NULL ? b3GetTicks() : 0;
				if ( b3SolveOtherPack( context, workerIndex ) )
				{
					if ( sampleContext != NULL )
					{
						uint64_t doneTicks = b3GetTicks();
						float packTime = b3TicksToMilliseconds( doneTicks - packTicks );
						sampleContext->teamPacks += packTime;
						sampleContext->teamWait -= packTime;
						b3AddTeamEntry( context, workerIndex, 0, packTicks, doneTicks, doneTicks, 0, b3_teamEntryPack );
					}
					spinCount = 0;
					continue;
				}
			}

			if ( spinCount > 5 )
			{
				b3Yield();
				spinCount = 0;
				if ( sampleContext != NULL )
				{
					sampleContext->teamYields += 1;
					waitFlags |= b3_teamEntryAfterYield;
				}
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

		uint64_t enterTicks = 0;
		if ( sampleContext != NULL )
		{
			enterTicks = b3GetTicks();
			sampleContext->teamWait += b3TicksToMilliseconds( enterTicks - waitTicks );
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
		int completedCount = b3ExecuteStage( stage, context, previousSyncIndex, syncIndex, workerIndex );

		// Added for Nebenan: see b3TeamEntry
		if ( sampleContext != NULL )
		{
			uint64_t doneTicks = b3GetTicks();
			b3AddTeamEntry( context, workerIndex, syncBits, enterTicks, doneTicks, doneTicks, completedCount, waitFlags );
		}

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
// Writes the contact ids to ordered in the order of the active colors, as many as the colors have convex contacts.
static void b3OrderColorContacts( const b3World* world, const b3GraphColor* colors, const int* activeColorIndices,
								  int activeColorCount, int awakeBodyCount, int* ordered )
{
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
}

// Added for Nebenan: packs of islands per worker, enough for the workers to even out the work, see b3BuildPacks. A byte
// holds the pack of a contact, so the lookups by contact id stay in the closest caches.
#define B3_PACKS_PER_WORKER 4
#define B3_MAX_PACKS ( B3_PACKS_PER_WORKER * B3_MAX_WORKERS )
_Static_assert( B3_MAX_PACKS <= 256, "a pack needs to fit in a byte" );

// Added for Nebenan: a team takes the largest pack once it holds more than five fourths of the even share of a thread, see
// b3SolveOtherPack. Below that one thread alone is about as fast as the team with its barriers.
#define B3_TEAM_SHARE_NUM 5
#define B3_TEAM_SHARE_DEN 4

// Added for Nebenan: with a team every further pack that holds more than twice the even share of a thread joins it, see
// b3BuildPacks. The team only gets more blocks in the same stages, but a thread in the team solves its share of the work
// about half as fast as a thread alone solves a pack, with the barriers and the waits. Below that one thread alone ends
// such a pack before the team would end it with all of them.
#define B3_TEAM_JOIN_NUM 2
#define B3_TEAM_JOIN_DEN 1

// Added for Nebenan: every this many steps a step with a team times where its constraints go, see b3Profile::teamTime.
// Only that step reads the clock at every stage, the others pay nothing.
#define B3_TEAM_SAMPLE 8

// Added for Nebenan: the row of b3TeamStages of a stage. The stages of the knots have no color, see b3ExecuteKnotStage.
static int b3GetTeamRow( const b3SolverStage* stage )
{
	switch ( stage->type )
	{
		case b3_stagePrepareJoints:
		case b3_stagePrepareWideContacts:
		case b3_stagePrepareContacts:
			return b3_teamPrepare;

		case b3_stageIntegrateVelocities:
		case b3_stageIntegratePositions:
			return b3_teamIntegrate;

		case b3_stageStoreWideImpulses:
		case b3_stageStoreImpulses:
			return b3_teamStore;

		default:
			return stage->colorIndex == UINT8_MAX ? b3_teamKnots : b3_teamColors + stage->colorIndex;
	}
}

// Added for Nebenan: the ticks two spans share
static uint64_t b3GetTicksOverlap( uint64_t start1, uint64_t end1, uint64_t start2, uint64_t end2 )
{
	uint64_t start = start1 > start2 ? start1 : start2;
	uint64_t end = end1 < end2 ? end1 : end2;
	return end > start ? end - start : 0;
}

// Added for Nebenan: how a step that timed its team spent its stages, from the entries of its threads, see
// b3TeamStages. The calling thread ran every stage in turn, and each worker came into a part of the stages with a
// barrier in the same order. Between them it waited or solved other packs, and a stage it never came into, or only
// after it closed, it missed.
static void b3SumTeamStages( b3World* world, const b3StepContext* context )
{
	b3TeamStages* result = &world->teamStages;
	*result = (b3TeamStages){ 0 };

	const b3TeamEntry* callerEntries = context->teamEntries;
	int callerCount = world->taskContexts.data[0].teamEntryCount;
	uint64_t lastTicks = context->teamStartTicks;
	for ( int i = 0; i < callerCount; ++i )
	{
		const b3TeamEntry* entry = callerEntries + i;
		const b3SolverStage* stage = context->stages + ( entry->syncBits & 0xFFFF );
		b3TeamStage* row = result->rows + b3GetTeamRow( stage );

		for ( int j = 0; j < stage->blockCount; ++j )
		{
			row->itemCount += (float)stage->blocks[j].block.count;
		}

		result->serialTime += b3TicksToMilliseconds( entry->startTicks - lastTicks );
		lastTicks = entry->closeTicks;

		float time = b3TicksToMilliseconds( entry->closeTicks - entry->startTicks );
		if ( entry->flags & b3_teamEntryAlone )
		{
			row->aloneCount += 1.0f;
			row->aloneTime += time;
			continue;
		}

		row->barrierCount += 1.0f;
		row->blockCount += (float)stage->blockCount;
		row->callerBlockCount += (float)entry->blockCount;
		row->openTime += time;
		row->workTime += b3TicksToMilliseconds( entry->doneTicks - entry->startTicks );
		row->doneTime += b3TicksToMilliseconds( entry->closeTicks - entry->doneTicks );
	}
	result->tailTime = b3TicksToMilliseconds( context->teamEndTicks - lastTicks );
	result->mergeTime = context->teamMergeTime;

	for ( int workerIndex = 1; workerIndex < context->workerCount; ++workerIndex )
	{
		const b3TeamEntry* entries = context->teamEntries + workerIndex * context->teamEntryCapacity;
		int entryCount = world->taskContexts.data[workerIndex].teamEntryCount;
		int stageIndex = 0;
		int firstIndex = 0;
		for ( int i = 0; i < callerCount; ++i )
		{
			const b3TeamEntry* caller = callerEntries + i;
			if ( caller->flags & b3_teamEntryAlone )
			{
				continue;
			}

			const b3SolverStage* stage = context->stages + ( caller->syncBits & 0xFFFF );
			b3TeamStage* row = result->rows + b3GetTeamRow( stage );
			uint64_t openTicks = caller->startTicks;
			uint64_t closeTicks = caller->closeTicks;

			while ( stageIndex < entryCount && ( entries[stageIndex].flags & b3_teamEntryPack ) )
			{
				stageIndex += 1;
			}

			const b3TeamEntry* entry = NULL;
			if ( stageIndex < entryCount && entries[stageIndex].syncBits == caller->syncBits )
			{
				entry = entries + stageIndex;
				stageIndex += 1;
			}

			// The clocks of the threads may differ a little, so the worker stays within the stage. Without a block it
			// came too late for any.
			uint64_t inTicks = closeTicks;
			uint64_t outTicks = closeTicks;
			if ( entry != NULL && entry->startTicks < closeTicks )
			{
				inTicks = entry->startTicks > openTicks ? entry->startTicks : openTicks;
				outTicks = entry->doneTicks > inTicks ? entry->doneTicks : inTicks;
				outTicks = outTicks < closeTicks ? outTicks : closeTicks;
				inTicks = entry->blockCount > 0 ? inTicks : outTicks;
			}
			else
			{
				entry = NULL;
			}

			// The other packs the worker solved while the stage was open, before it came in or after its blocks
			while ( firstIndex < entryCount && entries[firstIndex].doneTicks <= openTicks )
			{
				firstIndex += 1;
			}

			uint64_t lateTicks = inTicks - openTicks;
			uint64_t doneTicks = closeTicks - outTicks;
			uint64_t packTicks = 0;
			for ( int k = firstIndex; k < entryCount && entries[k].startTicks < closeTicks; ++k )
			{
				const b3TeamEntry* pack = entries + k;
				if ( pack->flags & b3_teamEntryPack )
				{
					uint64_t beforeTicks = b3GetTicksOverlap( pack->startTicks, pack->doneTicks, openTicks, inTicks );
					uint64_t afterTicks = b3GetTicksOverlap( pack->startTicks, pack->doneTicks, outTicks, closeTicks );
					lateTicks -= beforeTicks;
					doneTicks -= afterTicks;
					packTicks += beforeTicks + afterTicks;
				}
			}

			float late = b3TicksToMilliseconds( lateTicks );
			row->workTime += b3TicksToMilliseconds( outTicks - inTicks );
			row->packTime += b3TicksToMilliseconds( packTicks );
			row->lateTime += late;
			row->doneTime += b3TicksToMilliseconds( doneTicks );

			if ( entry == NULL )
			{
				result->missedCount += 1.0f;
				result->missedPackCount += packTicks > 0 ? 1.0f : 0.0f;
				continue;
			}

			result->entryCount += 1.0f;
			result->entryLateTime += late;
			if ( entry->flags & b3_teamEntryAfterYield )
			{
				result->yieldEntryCount += 1.0f;
				result->yieldLateTime += late;
			}
		}

		// Every stage the worker came into is one the calling thread opened
		while ( stageIndex < entryCount && ( entries[stageIndex].flags & b3_teamEntryPack ) )
		{
			stageIndex += 1;
		}
		B3_ASSERT( stageIndex == entryCount );
	}
}

// Added for Nebenan: the work of an island as the packs weigh it, four per contact and one per body
static int b3GetIslandWork( const b3Island* island )
{
	return 4 * island->contacts.count + island->bodies.count;
}

// Added for Nebenan: the work of the awake islands as b3BuildPacks weighs it, for a step of a world with workers that
// solves without packs, see b3Counters::islandWork. A world without workers skips it: its islands could be many.
static void b3MeasureIslandWork( b3World* world, const b3SolverSet* awakeSet )
{
	int total = 0;
	int largest = 0;
	for ( int i = 0; i < awakeSet->islandSims.count; ++i )
	{
		const b3Island* island = b3Array_Get( world->islands, awakeSet->islandSims.data[i].islandId );
		int weight = b3GetIslandWork( island );
		total += weight;
		largest = b3MaxInt( largest, weight );
	}
	world->islandWork = total;
	world->largestIslandWork = largest;
}

// Added for Nebenan: the awake islands in packs of about equal work, the largest pack first, for b3SolvePacks. Writes the
// pack of every awake island to islandPacks by island id, and where the bodies of every pack start to packBodyStarts, for
// b3FillPacks. Returns the pack count, or zero if one island holds more than half of the work and the workers are too few
// for a team, then the colors share it better. If a team solves the first packs, see b3SolveOtherPack, writes how many to
// teamPackCount and their work to teamWork, and zero to both otherwise.
static int b3BuildPacks( b3World* world, const b3SolverSet* awakeSet, int packLimit, int workerCount, int* islandPacks,
						 int* packBodyStarts, int* teamWork, int* teamPackCount )
{
	*teamWork = 0;
	*teamPackCount = 0;

	int islandCount = awakeSet->islandSims.count;
	if ( islandCount < 2 )
	{
		// Added for Nebenan: a single island holds all of the work, see b3Counters::islandWork
		if ( islandCount == 1 )
		{
			const b3Island* island = b3Array_Get( world->islands, awakeSet->islandSims.data[0].islandId );
			world->islandWork = b3GetIslandWork( island );
			world->largestIslandWork = world->islandWork;
		}
		return 0;
	}

	const b3IslandSim* islandSims = awakeSet->islandSims.data;
	int* weights = (int*)b3StackAlloc( &world->stack, islandCount * (int)sizeof( int ), "island weights" );
	int* order = (int*)b3StackAlloc( &world->stack, islandCount * (int)sizeof( int ), "island order" );

	// The work of an island in its contacts, which cost the most, and its bodies. The islands go in buckets by the leading
	// zeros of their work, the largest first, which is order enough for the packs.
	int bucketStarts[33] = { 0 };
	int total = 0;
	int largest = 0;
	for ( int i = 0; i < islandCount; ++i )
	{
		const b3Island* island = b3Array_Get( world->islands, islandSims[i].islandId );
		int weight = b3GetIslandWork( island );
		B3_ASSERT( weight > 0 );
		weights[i] = weight;
		total += weight;
		largest = b3MaxInt( largest, weight );
		bucketStarts[b3CLZ32( (uint32_t)weight )] += 1;
	}

	// Added for Nebenan: see b3Counters::islandWork
	world->islandWork = total;
	world->largestIslandWork = largest;

	// Added for Nebenan: an island with more than half of the work has a pack of its own, the first, see b3SolveOtherPack
	int packCount = 0;
	int64_t shares = (int64_t)B3_TEAM_SHARE_NUM * total;
	if ( 2 * largest <= total || (int64_t)B3_TEAM_SHARE_DEN * workerCount * largest > shares )
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

		// A team for the largest pack, and with it every further one above the share of a thread, see B3_TEAM_JOIN_NUM. The
		// packs go by their work, so those of the team come first.
		if ( (int64_t)B3_TEAM_SHARE_DEN * workerCount * loads[ranked[0]] > shares )
		{
			int64_t joinShares = (int64_t)B3_TEAM_JOIN_NUM * total;
			int teamCount = 1;
			int work = loads[ranked[0]];
			while ( teamCount < packCount && (int64_t)B3_TEAM_JOIN_DEN * workerCount * loads[ranked[teamCount]] > joinShares )
			{
				work += loads[ranked[teamCount]];
				teamCount += 1;
			}
			*teamWork = work;
			*teamPackCount = teamCount;
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
		B3_ASSERT( packBodyStarts[packCount] == awakeSet->bodySims.count );
	}

	b3StackFree( &world->stack, order );
	b3StackFree( &world->stack, weights );
	return packCount;
}

// Added for Nebenan: the awake indices of the bodies of every pack to packBodies from packBodyStarts on, in the order of the
// awake set, and the pack of every contact of the islands to contactPacks by contact id, see b3BuildPacks
static void b3FillPacks( const b3World* world, const b3SolverSet* awakeSet, const int* islandPacks, const int* packBodyStarts,
						 int packCount, int* packBodies, uint8_t* contactPacks )
{
	int cursors[B3_MAX_PACKS];
	for ( int p = 0; p < packCount; ++p )
	{
		cursors[p] = packBodyStarts[p];
	}

	// The bodies of every pack in the order of the awake set
	int bodyCount = awakeSet->bodySims.count;
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
	int islandCount = awakeSet->islandSims.count;
	const b3IslandSim* islandSims = awakeSet->islandSims.data;
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

// Added for Nebenan: the convex contacts of every active color pack by pack for b3SolvePacks, each pack filled up with null
// ids to whole wide constraints, so no wide constraint holds two packs. Writes the entries, at most b3GetPackEntryCapacity,
// for every color where the wide constraints of each pack start, see b3StepContext::packWideStarts, and how many entries
// the color has.
static void b3OrderPackContacts( const b3World* world, const b3GraphColor* colors, const int* activeColorIndices,
								 int activeColorCount, const uint8_t* contactPacks, int packCount, int* entries,
								 int* packWideStarts, int* entryCounts )
{
	// The width of the world, see b3World::simdWidth
	int simdWidth = world->simdWidth;
	int simdShift = simdWidth == 8 ? 3 : 2;

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
}

// Added for Nebenan: the most entries b3OrderPackContacts writes
static int b3GetPackEntryCapacity( const b3World* world, const b3GraphColor* colors, const int* activeColorIndices,
								   int activeColorCount, int packCount )
{
	int capacity = 0;
	for ( int i = 0; i < activeColorCount; ++i )
	{
		capacity += colors[activeColorIndices[i]].convexContacts.count + ( world->simdWidth - 1 ) * packCount;
	}
	return capacity;
}

// Added for Nebenan: what the solver setup gives to a worker while the calling thread builds the knots, see b3BuildKnots.
// With packs the pack of every body and contact, see b3FillPacks, and the convex contacts of the colors pack by pack, see
// b3OrderPackContacts, without packs the convex contacts of the colors in buckets, see b3OrderColorContacts. The knots need
// the packs of the contacts only at the end and wait for the task there, see b3FinishSetupTask, or run it themselves if no
// worker took it yet. Only the calling thread takes from the stack, the task fills what it took before. Every result is
// the same whichever thread computes it.
typedef struct b3SetupTask
{
	const b3World* world;
	const b3SolverSet* awakeSet;
	const b3GraphColor* colors;
	const int* activeColorIndices;
	int activeColorCount;
	int awakeBodyCount;

	// With packs, see b3BuildPacks
	int packCount;
	const int* islandPacks;
	const int* packBodyStarts;
	int* packBodies;
	uint8_t* contactPacks;
	int* packWideStarts;
	int* packEntryCounts;

	// The convex contacts of the colors in order, null without workers or contacts, then there is no task
	int* orderedContacts;

	// The task in the task system, null if the task system ran it right away, whether it started and was waited for, and
	// how long it ran
	void* userTask;
	bool started;
	bool finished;
	float time;
} b3SetupTask;

static void b3SetupTaskCallback( void* context )
{
	b3TracyCZoneNC( setup_task, "Setup Task", b3_colorDarkOrange, true );

	b3SetupTask* task = context;
	uint64_t ticks = b3GetTicks();
	if ( task->packCount > 0 )
	{
		b3FillPacks( task->world, task->awakeSet, task->islandPacks, task->packBodyStarts, task->packCount, task->packBodies,
					 task->contactPacks );
		b3OrderPackContacts( task->world, task->colors, task->activeColorIndices, task->activeColorCount, task->contactPacks,
							 task->packCount, task->orderedContacts, task->packWideStarts, task->packEntryCounts );
	}
	else
	{
		b3OrderColorContacts( task->world, task->colors, task->activeColorIndices, task->activeColorCount, task->awakeBodyCount,
							  task->orderedContacts );
	}
	task->time = b3GetMilliseconds( ticks );

	b3TracyCZoneEnd( setup_task );
}

// Added for Nebenan: hands the setup task to a worker, see b3SetupTask
static void b3StartSetupTask( b3World* world, b3SetupTask* task )
{
	if ( task->orderedContacts == NULL )
	{
		return;
	}

	task->started = true;
	if ( world->activeWorkerCount > 1 && world->taskCount < B3_MAX_TASKS )
	{
		task->userTask = world->enqueueTaskFcn( &b3SetupTaskCallback, task, world->userTaskContext, "setup" );
		world->taskCount += 1;
		world->activeTaskCount += task->userTask == NULL ? 0 : 1;
	}
	else
	{
		b3SetupTaskCallback( task );
	}
}

// Added for Nebenan: waits for the setup task once, or runs it if no worker took it yet, see b3Profile::setupWait
static void b3FinishSetupTask( b3World* world, b3SetupTask* task )
{
	if ( task->started == false || task->finished )
	{
		return;
	}

	uint64_t ticks = b3GetTicks();
	if ( task->userTask != NULL )
	{
		world->finishTaskFcn( task->userTask, world->userTaskContext );
		world->activeTaskCount -= 1;
	}
	task->finished = true;
	world->profile.setupWait = b3GetMilliseconds( ticks );
	world->profile.setupOrder = task->time;
}

// Solve with graph coloring
// Added for Nebenan: a moving body with at least this many contacts in the overflow is a knot, see b3BuildKnots
#define B3_KNOT_MIN_CONTACTS 2

// Added for Nebenan: from this many contacts at knots in the overflow of islands with at least one in
// B3_KNOT_GROUP_SHARE of the work on, the batches go into B3_KNOT_GROUPS groups, and a knot of such an island spreads
// over a group for every B3_KNOT_GROUP_CONTACTS of its contacts, see b3BuildKnots
#define B3_KNOT_GROUP_MIN 256
#define B3_KNOT_GROUP_SHARE 8
#define B3_KNOT_GROUP_CONTACTS 32

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

	// The first batch of every group, B3_KNOT_GROUPS per pack or for the step without packs, and the end, and the most
	// batches in one group
	int* groupStarts;
	int largestGroup;

	// The knots in several groups pack by pack, the first of every pack and the end, and their copies, see b3KnotSplit
	b3KnotSplit* splits;
	int* packSplitStarts;
	int splitCount;
	b3KnotCopy* copies;
	int copyCount;

	// The overflow contacts outside the knots, in their order
	b3ContactSpec* others;
	int otherCount;

	// All of the above in one block but the copies, which follow it, the stack holds few
	void* memory;
} b3Knots;

// Added for Nebenan: the next part of a block in stack memory, on its own cache line
static void* b3TakeKnotMemory( char** cursor, int byteCount )
{
	void* part = *cursor;
	*cursor += ( ( byteCount + 63 ) / 64 ) * 64;
	return part;
}

// Added for Nebenan: the overflow holds the contacts the graph finds no color for, see b3AddContactToGraph, and the
// calling thread solves them one at a time while the workers wait. Beside a few contacts of tight spots these are the
// contacts of knots: large parts of a fracture, dozens to hundreds of pieces in one body, with hundreds of small debris
// resting on and against them. A knot is the awake body with more contacts in the overflow of the two, if it has at
// least B3_KNOT_MIN_CONTACTS. Its contacts go into batches of B3_KNOT_LANES with different partners: a contact goes
// into the oldest batch of its knot that is not full and comes after the last one of its partner. Each batch solves its
// lanes at once against copies of the knot, see b3KnotBatch, which makes the knot's chain of contacts several times
// shorter, and the batches of the knots take turns. With B3_KNOT_GROUP_MIN contacts at knots of large islands the
// batches go into B3_KNOT_GROUPS groups that share no moving partner, which the threads solve at once, and a knot of a
// large island in several groups has a copy in each. A large island holds at least one in B3_KNOT_GROUP_SHARE of the
// work of all awake islands, and a team of many threads solves it, see b3SolveOtherPack, while a thread solves a
// smaller one whole, where copies would only cost. A contact between two knots then stays with the other contacts.
// Everything follows from the order of the overflow and the islands, so the batches and groups are the same with every
// number of workers and in either width. With packs the batches go pack by pack, in the same order within each pack. The
// packs of the contacts come from the setup task, the knots wait for it before they need them.
static void b3BuildKnots( b3World* world, const b3SolverSet* awakeSet, const b3GraphColor* overflow, const uint8_t* contactPacks,
						  int packCount, b3SetupTask* setupTask, b3Knots* knots )
{
	int n = overflow->contacts.count;
	const b3ContactSpec* specs = overflow->contacts.data;
	const b3Contact* contacts = world->contacts.data;
	int awakeBodyCount = awakeSet->bodySims.count;
	int capacity = b3MaxInt( n, 1 );

	// The groups go pack by pack, all in one without packs
	int packs = b3MaxInt( packCount, 1 );
	int groupSlots = B3_KNOT_GROUPS * packs;

	// At most one batch per contact, and a knot in several groups has two contacts at least
	int splitCapacity = capacity / 2 + 1;
	int byteCount = 64 * 9 + capacity * (int)sizeof( b3KnotBatch ) + 3 * B3_KNOT_LANES * capacity * (int)sizeof( int ) +
					capacity * (int)sizeof( b3ContactSpec ) + ( packCount + 1 ) * (int)sizeof( int ) +
					( groupSlots + 1 ) * (int)sizeof( int ) + splitCapacity * (int)sizeof( b3KnotSplit ) +
					( packs + 1 ) * (int)sizeof( int );
	*knots = (b3Knots){ 0 };
	knots->memory = b3StackAlloc( &world->stack, byteCount, "knots" );
	char* cursor = (char*)knots->memory;
	knots->batches = (b3KnotBatch*)b3TakeKnotMemory( &cursor, capacity * (int)sizeof( b3KnotBatch ) );
	knots->contactIds = (int*)b3TakeKnotMemory( &cursor, B3_KNOT_LANES * capacity * (int)sizeof( int ) );
	knots->scaleA = (float*)b3TakeKnotMemory( &cursor, B3_KNOT_LANES * capacity * (int)sizeof( float ) );
	knots->scaleB = (float*)b3TakeKnotMemory( &cursor, B3_KNOT_LANES * capacity * (int)sizeof( float ) );
	knots->others = (b3ContactSpec*)b3TakeKnotMemory( &cursor, capacity * (int)sizeof( b3ContactSpec ) );
	knots->groupStarts = (int*)b3TakeKnotMemory( &cursor, ( groupSlots + 1 ) * (int)sizeof( int ) );
	knots->splits = (b3KnotSplit*)b3TakeKnotMemory( &cursor, splitCapacity * (int)sizeof( b3KnotSplit ) );
	knots->packSplitStarts = (int*)b3TakeKnotMemory( &cursor, ( packs + 1 ) * (int)sizeof( int ) );
	for ( int i = 0; i <= groupSlots; ++i )
	{
		knots->groupStarts[i] = 0;
	}
	for ( int p = 0; p <= packs; ++p )
	{
		knots->packSplitStarts[p] = 0;
	}
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

	// Per awake body its contacts in the overflow and the rank of its knot, later the group and the last batch of a partner
	// and the knot it was counted for. Per contact its awake bodies, null for a mesh contact, and its knot. Per knot its
	// contacts, whether its island is large, its groups as bits, its split, and its batches in the group at hand, the
	// batches of the current knot that are not full, and per batch its contacts, its knot and whether it is the first of
	// its knot in its group. Per contact at a knot its group, and the batches in the order of the groups. There are at
	// most as many knots and batches as contacts.
	int* temporary = (int*)b3StackAlloc(
		&world->stack, ( 2 * awakeBodyCount + ( 17 + B3_KNOT_LANES ) * n + 2 + groupSlots ) * (int)sizeof( int ), "knot work" );
	int* bodyCounts = temporary;
	int* bodyMarks = bodyCounts + awakeBodyCount;
	int* bodiesA = bodyMarks + awakeBodyCount;
	int* bodiesB = bodiesA + n;
	int* contactKnots = bodiesB + n;
	int* knotBodies = contactKnots + n;
	int* knotStarts = knotBodies + n;
	int* knotCursors = knotStarts + n + 1;
	int* knotOrder = knotCursors + n;
	int* knotLarge = knotOrder + n;
	int* knotGroups = knotLarge + n;
	int* knotSplits = knotGroups + n;
	int* knotBatchStarts = knotSplits + n;
	int* openBatches = knotBatchStarts + n + 1;
	int* batchLanes = openBatches + n;
	int* batchSlots = batchLanes + n;
	int* batchKnots = batchSlots + B3_KNOT_LANES * n;
	int* batchFirsts = batchKnots + n;
	int* orderGroups = batchFirsts + n;
	int* slots = orderGroups + n;
	int* groupCursors = slots + n;

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
	int knotContactCount = 0;
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
		knotContactCount += 1;
		if ( bodyMarks[knot] == B3_NULL_INDEX )
		{
			bodyMarks[knot] = knotCount;
			knotBodies[knotCount] = knot;
			knotCount += 1;
		}
	}

	// Groups once the knots of large islands have enough contacts. The work of all awake islands is there with workers,
	// see b3MeasureIslandWork, and the same sum without them.
	int groupCount = 1;
	if ( knotContactCount >= B3_KNOT_GROUP_MIN )
	{
		int totalWork = world->islandWork;
		if ( totalWork == 0 )
		{
			for ( int i = 0; i < awakeSet->islandSims.count; ++i )
			{
				totalWork += b3GetIslandWork( b3Array_Get( world->islands, awakeSet->islandSims.data[i].islandId ) );
			}
		}

		const b3BodySim* sims = awakeSet->bodySims.data;
		const b3Body* bodies = world->bodies.data;
		for ( int r = 0; r < knotCount; ++r )
		{
			const b3Body* body = bodies + sims[knotBodies[r]].bodyId;
			knotLarge[r] = false;
			if ( body->islandId != B3_NULL_INDEX )
			{
				const b3Island* island = b3Array_Get( world->islands, body->islandId );
				knotLarge[r] = (int64_t)B3_KNOT_GROUP_SHARE * b3GetIslandWork( island ) >= totalWork;
			}
		}

		int largeContactCount = 0;
		for ( int k = 0; k < n; ++k )
		{
			largeContactCount += contactKnots[k] != B3_NULL_INDEX && knotLarge[bodyMarks[contactKnots[k]]] ? 1 : 0;
		}
		groupCount = largeContactCount >= B3_KNOT_GROUP_MIN ? B3_KNOT_GROUPS : 1;
	}

	// In groups a contact between two knots stays with the others, the knots change in different groups at once
	if ( groupCount > 1 )
	{
		for ( int k = 0; k < n; ++k )
		{
			int knot = contactKnots[k];
			int partner = bodiesA[k] == knot ? bodiesB[k] : bodiesA[k];
			if ( knot != B3_NULL_INDEX && partner != B3_NULL_INDEX && bodyMarks[partner] != B3_NULL_INDEX )
			{
				contactKnots[k] = B3_NULL_INDEX;
			}
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

	// The group of every contact at a knot. A knot of a large island spreads over a group for every
	// B3_KNOT_GROUP_CONTACTS of its contacts, at least one, any other stays in one, from the group after the last knot
	// on, and deals its contacts out to these groups in turn. A moving partner keeps its group for all its contacts,
	// also with other knots, while a partner that does not move can take any.
	int contactTotal = knotStarts[knotCount];
	for ( int i = 0; i < contactTotal; ++i )
	{
		orderGroups[i] = 0;
	}

	if ( groupCount > 1 )
	{
		// The group of a moving partner, null until it has one
		for ( int i = 0; i < contactTotal; ++i )
		{
			int k = knotOrder[i];
			int partner = bodiesA[k] == contactKnots[k] ? bodiesB[k] : bodiesA[k];
			if ( partner != B3_NULL_INDEX )
			{
				bodyCounts[partner] = B3_NULL_INDEX;
			}
		}

		int offset = 0;
		for ( int r = 0; r < knotCount; ++r )
		{
			int spread = 1;
			if ( knotLarge[r] )
			{
				spread = b3ClampInt( ( knotStarts[r + 1] - knotStarts[r] ) / B3_KNOT_GROUP_CONTACTS, 1, B3_KNOT_GROUPS );
			}
			int first = offset;
			offset = ( offset + spread ) % B3_KNOT_GROUPS;

			int turn = 0;
			for ( int i = knotStarts[r]; i < knotStarts[r + 1]; ++i )
			{
				int k = knotOrder[i];
				int partner = bodiesA[k] == knotBodies[r] ? bodiesB[k] : bodiesA[k];
				if ( partner != B3_NULL_INDEX && bodyCounts[partner] != B3_NULL_INDEX )
				{
					orderGroups[i] = bodyCounts[partner];
					continue;
				}

				orderGroups[i] = ( first + turn % spread ) % B3_KNOT_GROUPS;
				turn += 1;
				if ( partner != B3_NULL_INDEX )
				{
					bodyCounts[partner] = orderGroups[i];
				}
			}

			// The contacts of the knot group by group, in their order within each group
			int groupContacts[B3_KNOT_GROUPS] = { 0 };
			for ( int i = knotStarts[r]; i < knotStarts[r + 1]; ++i )
			{
				groupContacts[orderGroups[i]] += 1;
			}

			int starts[B3_KNOT_GROUPS];
			int start = knotStarts[r];
			for ( int g = 0; g < B3_KNOT_GROUPS; ++g )
			{
				starts[g] = start;
				start += groupContacts[g];
			}

			for ( int i = knotStarts[r]; i < knotStarts[r + 1]; ++i )
			{
				slots[starts[orderGroups[i]]] = knotOrder[i];
				starts[orderGroups[i]] += 1;
			}

			int i = knotStarts[r];
			for ( int g = 0; g < B3_KNOT_GROUPS; ++g )
			{
				for ( int j = 0; j < groupContacts[g]; ++j )
				{
					knotOrder[i] = slots[i];
					orderGroups[i] = g;
					i += 1;
				}
			}
		}
	}

	// The batches group by group, in each group knot after knot. The ranks of the knots are not needed anymore, a partner
	// is now marked with the rank of the knot it was counted for, below B3_NULL_INDEX to tell it from what is left, and
	// remembers the batch it went into last. A contact goes into the oldest batch of its knot in the group that is not full
	// and comes after the last one of its partner, so the lanes of a batch have different partners and few batches stay
	// half full. The knots of a group take turns, batch after batch, so the batches next to each other rarely share a knot.
	for ( int r = 0; r < knotCount; ++r )
	{
		knotCursors[r] = knotStarts[r];
	}

	int groupSlotStarts[B3_KNOT_GROUPS + 1];
	int batchCount = 0;
	int slotCount = 0;
	for ( int g = 0; g < groupCount; ++g )
	{
		groupSlotStarts[g] = slotCount;
		int maxBatches = 0;
		for ( int r = 0; r < knotCount; ++r )
		{
			int mark = -2 - r;
			int openCount = 0;
			knotBatchStarts[r] = batchCount;
			for ( ; knotCursors[r] < knotStarts[r + 1] && orderGroups[knotCursors[r]] == g; knotCursors[r] += 1 )
			{
				int k = knotOrder[knotCursors[r]];
				int partner = bodiesA[k] == knotBodies[r] ? bodiesB[k] : bodiesA[k];
				int last = B3_NULL_INDEX;
				if ( partner != B3_NULL_INDEX && bodyMarks[partner] == mark )
				{
					last = bodyCounts[partner];
				}

				int open = 0;
				while ( open < openCount && openBatches[open] <= last )
				{
					open += 1;
				}

				if ( open == openCount )
				{
					openBatches[openCount] = batchCount;
					openCount += 1;
					batchLanes[batchCount] = 0;
					batchKnots[batchCount] = r;
					batchFirsts[batchCount] = batchCount == knotBatchStarts[r];
					batchCount += 1;
				}

				int batch = openBatches[open];
				batchSlots[B3_KNOT_LANES * batch + batchLanes[batch]] = k;
				batchLanes[batch] += 1;
				if ( partner != B3_NULL_INDEX )
				{
					bodyMarks[partner] = mark;
					bodyCounts[partner] = batch;
				}

				// A full batch takes no more
				if ( batchLanes[batch] == B3_KNOT_LANES )
				{
					openCount -= 1;
					for ( int i = open; i < openCount; ++i )
					{
						openBatches[i] = openBatches[i + 1];
					}
				}
			}
			maxBatches = b3MaxInt( maxBatches, batchCount - knotBatchStarts[r] );
		}
		knotBatchStarts[knotCount] = batchCount;

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
	}
	groupSlotStarts[groupCount] = slotCount;
	B3_ASSERT( slotCount == batchCount );

	// From here on the packs of the contacts
	b3FinishSetupTask( world, setupTask );

	// The groups of every knot as bits, and the knots in more than one, pack by pack in the order of the knots
	int* packSplitStarts = knots->packSplitStarts;
	int splitCount = 0;
	for ( int r = 0; r < knotCount; ++r )
	{
		knotGroups[r] = 0;
		for ( int i = knotStarts[r]; i < knotStarts[r + 1]; ++i )
		{
			knotGroups[r] |= 1 << orderGroups[i];
		}

		knotSplits[r] = B3_NULL_INDEX;
		if ( b3PopCount64( (uint64_t)knotGroups[r] ) > 1 )
		{
			int pack = packCount > 0 ? contactPacks[specs[knotOrder[knotStarts[r]]].contactId] : 0;
			packSplitStarts[pack + 1] += 1;
			splitCount += 1;
		}
	}
	for ( int p = 0; p < packs; ++p )
	{
		packSplitStarts[p + 1] += packSplitStarts[p];
	}

	for ( int p = 0; p < packs; ++p )
	{
		groupCursors[p] = 0;
	}
	for ( int r = 0; r < knotCount; ++r )
	{
		int copyCount = b3PopCount64( (uint64_t)knotGroups[r] );
		if ( copyCount > 1 )
		{
			int pack = packCount > 0 ? contactPacks[specs[knotOrder[knotStarts[r]]].contactId] : 0;
			int split = packSplitStarts[pack] + groupCursors[pack];
			groupCursors[pack] += 1;
			knotSplits[r] = split;
			knots->splits[split] = (b3KnotSplit){ knotBodies[r], 0, copyCount };
		}
	}

	int copyCount = 0;
	for ( int s = 0; s < splitCount; ++s )
	{
		knots->splits[s].firstCopy = copyCount;
		copyCount += knots->splits[s].copyCount;
	}

	// Pack by pack and in each group by group, in the order above
	int* groupStarts = knots->groupStarts;
	for ( int g = 0; g < groupCount; ++g )
	{
		for ( int i = groupSlotStarts[g]; i < groupSlotStarts[g + 1]; ++i )
		{
			int k = batchSlots[B3_KNOT_LANES * slots[i]];
			int pack = packCount > 0 ? contactPacks[specs[k].contactId] : 0;
			groupStarts[B3_KNOT_GROUPS * pack + g + 1] += 1;
		}
	}

	int largestGroup = 0;
	for ( int i = 0; i < groupSlots; ++i )
	{
		largestGroup = b3MaxInt( largestGroup, groupStarts[i + 1] );
		groupStarts[i + 1] += groupStarts[i];
		groupCursors[i] = 0;
	}

	if ( packCount > 0 )
	{
		for ( int p = 0; p <= packCount; ++p )
		{
			knots->packStarts[p] = groupStarts[B3_KNOT_GROUPS * p];
		}
	}

	int contactCount = 0;
	for ( int g = 0; g < groupCount; ++g )
	{
		for ( int i = groupSlotStarts[g]; i < groupSlotStarts[g + 1]; ++i )
		{
			int batch = slots[i];
			int laneCount = batchLanes[batch];
			const int* lanes = batchSlots + B3_KNOT_LANES * batch;
			int r = batchKnots[batch];
			int knot = knotBodies[r];

			int slot = g;
			if ( packCount > 0 )
			{
				slot += B3_KNOT_GROUPS * contactPacks[specs[lanes[0]].contactId];
			}
			int position = groupStarts[slot] + groupCursors[slot];
			groupCursors[slot] += 1;

			// A knot in several groups has a copy in each, in the order of the groups, and its mass splits over all copies
			int copy = B3_NULL_INDEX;
			int copies = 1;
			if ( knotSplits[r] != B3_NULL_INDEX )
			{
				const b3KnotSplit* split = knots->splits + knotSplits[r];
				copy = split->firstCopy + b3PopCount64( (uint64_t)( knotGroups[r] & ( ( 1 << g ) - 1 ) ) );
				copies = split->copyCount;
			}

			knots->batches[position] = (b3KnotBatch){ knot + 1, laneCount, copy, batchFirsts[batch] };
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
					scaleA[lane] = knotIsA ? (float)( laneCount * copies ) : 1.0f;
					scaleB[lane] = knotIsA ? 1.0f : (float)( laneCount * copies );
				}
			}
			contactCount += laneCount;
		}
	}

	knots->batchCount = batchCount;
	knots->contactCount = contactCount;
	knots->largestGroup = largestGroup;
	knots->splitCount = splitCount;
	knots->copyCount = copyCount;
	knots->otherCount = otherCount;
	B3_ASSERT( contactCount + otherCount == n );

	b3StackFree( &world->stack, temporary );

	// After the block of the knots, see b3FreeKnots
	if ( copyCount > 0 )
	{
		knots->copies = (b3KnotCopy*)b3StackAlloc( &world->stack, copyCount * (int)sizeof( b3KnotCopy ), "knot copies" );
	}
}

// Added for Nebenan: gives back the memory of b3BuildKnots
static void b3FreeKnots( b3World* world, b3Knots* knots )
{
	if ( knots->copies != NULL )
	{
		b3StackFree( &world->stack, knots->copies );
	}
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

	// Added for Nebenan: see b3Counters::knotContactCount, b3Counters::teamWork and b3Counters::teamSample
	world->knotContactCount = 0;
	world->knotBatchCount = 0;
	world->knotGroupBatchCount = 0;
	world->knotCopyCount = 0;
	world->teamWork = 0;
	world->teamSample = 0;
	world->solverSample = 0;
	world->teamWorkerCount = 0;
	world->teamStageCount = 0;
	world->teamYieldCount = 0;
	world->teamPackCount = 0;

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
		int teamWork = 0;
		int teamPackCount = 0;

		// Added for Nebenan: the parts of the setup, see b3Profile::setupPacks
		uint64_t partTicks = b3GetTicks();

		// Added for Nebenan: the work of the islands of this step, from b3BuildPacks or b3MeasureIslandWork
		world->islandWork = 0;
		world->largestIslandWork = 0;
		uint8_t* contactPacks = NULL;
		int* packBodyStarts = NULL;
		int* packBodies = NULL;
		int* islandPacks = NULL;
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
			islandPacks = (int*)b3StackAlloc( &world->stack, world->islands.count * (int)sizeof( int ), "island packs" );
			packCount =
				b3BuildPacks( world, awakeSet, packLimit, workerCount, islandPacks, packBodyStarts, &teamWork, &teamPackCount );

			// The setup task fills the bodies and contacts of the packs and the entries of the colors, see b3SetupTask
			if ( packCount > 0 )
			{
				packWideStarts = (int*)b3StackAlloc( &world->stack, activeColorCount * ( packCount + 1 ) * (int)sizeof( int ),
													 "pack wide starts" );
				int entryCapacity = b3GetPackEntryCapacity( world, colors, activeColorIndices, activeColorCount, packCount );
				packContacts =
					(int*)b3StackAlloc( &world->stack, b3MaxInt( entryCapacity, 1 ) * (int)sizeof( int ), "pack contacts" );
			}
			else
			{
				b3StackFree( &world->stack, islandPacks );
				b3StackFree( &world->stack, packBodies );
				b3StackFree( &world->stack, packBodyStarts );
				b3StackFree( &world->stack, contactPacks );
				islandPacks = NULL;
				packBodies = NULL;
				packBodyStarts = NULL;
				contactPacks = NULL;
			}
		}
		else if ( world->workerCount > 1 )
		{
			b3MeasureIslandWork( world, awakeSet );
		}
		world->profile.setupPacks = b3GetMilliseconds( partTicks );

		// Added for Nebenan: without packs but with workers the convex contacts of the colors go in buckets, see
		// b3OrderColorContacts
		int* orderedContacts = packContacts;
		if ( packCount == 0 && workerCount > 1 )
		{
			int convexCount = 0;
			for ( int i = 0; i < activeColorCount; ++i )
			{
				convexCount += colors[activeColorIndices[i]].convexContacts.count;
			}

			if ( convexCount > 0 )
			{
				orderedContacts = (int*)b3StackAlloc( &world->stack, convexCount * (int)sizeof( int ), "ordered contacts" );
			}
		}

		// Added for Nebenan: a worker fills the packs and puts the contacts in order while the calling thread builds the
		// knots, see b3SetupTask
		b3SetupTask setupTask = {
			.world = world,
			.awakeSet = awakeSet,
			.colors = colors,
			.activeColorIndices = activeColorIndices,
			.activeColorCount = activeColorCount,
			.awakeBodyCount = awakeBodyCount,
			.packCount = packCount,
			.islandPacks = islandPacks,
			.packBodyStarts = packBodyStarts,
			.packBodies = packBodies,
			.contactPacks = contactPacks,
			.packWideStarts = packWideStarts,
			.packEntryCounts = packEntryCounts,
			.orderedContacts = orderedContacts,
		};
		b3StartSetupTask( world, &setupTask );

		// Added for Nebenan: see b3Counters::teamWork
		bool team = teamWork > 0;
		world->teamWork = teamWork;
		world->teamPackCount = teamPackCount;

		// Added for Nebenan: the packs whose bodies and constraints the stages hold, the first ones, with a team those it
		// solves, see b3SolveOtherPack. Without packs the knots lie in one, see b3BuildKnots.
		int stagePackCount = team ? teamPackCount : 1;

		// Added for Nebenan: the knots of the overflow in batches, whose wide constraints follow those of the colors, see
		// b3BuildKnots
		b3Knots knots;
		partTicks = b3GetTicks();
		b3BuildKnots( world, awakeSet, overflow, contactPacks, packCount, &setupTask, &knots );
		b3FinishSetupTask( world, &setupTask );
		world->knotContactCount = knots.contactCount;
		world->knotBatchCount = knots.batchCount;
		world->knotGroupBatchCount = knots.largestGroup;
		world->knotCopyCount = knots.copyCount;

		// Added for Nebenan: with packs the wide constraints of the colors as the setup task filled them, see
		// b3OrderPackContacts
		if ( packCount > 0 )
		{
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
		world->profile.setupKnots = b3GetMilliseconds( partTicks ) - world->profile.setupWait;

		// Build the span table for the flat prepare/store parallel-for while I slice the
		// wide constraint buffer across colors. One entry per active color plus a sentinel
		// at wideContactCount.
		b3WidePrepareSpan widePrepareSpans[B3_GRAPH_COLOR_COUNT + 2];
		b3ContactPrepareSpan contactPrepareSpans[B3_GRAPH_COLOR_COUNT + 1];
		b3JointPrepareSpan jointPrepareSpans[B3_GRAPH_COLOR_COUNT + 1];

		// Added for Nebenan: in the order of the setup task, see b3SetupTask
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

		// Added for Nebenan: with a team the stages hold only the bodies and constraints of its packs, the first ones, see
		// b3SolveOtherPack. Their convex constraints lie in one range per color and their knots in one more. A block of the
		// stages that prepare and store them stays in one range, so they need one more block per range at most.
		int knotWide = B3_KNOT_LANES >> simdShift;
		int teamWideSize = 0;
		if ( team )
		{
			bodyDim = b3ComputeBlockCount( packBodyStarts[teamPackCount] - packBodyStarts[0], minBodiesPerBlock, maxBlockCount );

			int teamWideCount = knotWide * ( knots.packStarts[teamPackCount] - knots.packStarts[0] );
			graphBlockCount = 0;
			for ( int i = 0; i < activeColorCount; ++i )
			{
				const int* starts = packWideStarts + i * ( packCount + 1 );
				int count = starts[teamPackCount] - starts[0];
				teamWideCount += count;
				graphWideContactDims[i] = b3ComputeBlockCount( count, minContactsPerBlock, maxBlockCount );
				graphBlockCount += graphWideContactDims[i].count;
			}

			b3BlockDim teamWideDim = b3ComputeBlockCount( teamWideCount, minContactsPerBlock, maxBlockCount );
			teamWideSize = teamWideDim.size;
			convexPrepareDim.count = teamWideDim.count + activeColorCount + 1;
		}

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
		// Added for Nebenan: a stage of the knots before each sweep over the colors, see b3ExecuteKnotStage
		stageCount += 1 + activeColorCount;
		// b3_stageSolve
		stageCount += ITERATIONS * ( 1 + activeColorCount );
		// b3_stageIntegratePositions
		stageCount += 1;
		// b3_stageRelax
		stageCount += RELAX_ITERATIONS * ( 1 + activeColorCount );
		// b3_stageRestitution
		stageCount += 1 + activeColorCount;
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

		// Added for Nebenan: a block per group of knots with batches, with a team those of its packs, see
		// b3ExecuteKnotStage. All stages of the knots share them. The groups go pack by pack, see b3BuildKnots.
		int knotGroupCount = B3_KNOT_GROUPS * stagePackCount;
		b3SyncBlock* knotBlocks =
			(b3SyncBlock*)b3StackAlloc( &world->stack, knotGroupCount * sizeof( b3SyncBlock ), "knot blocks" );
		int knotBlockCount = 0;
		for ( int g = 0; g < knotGroupCount; ++g )
		{
			if ( knots.groupStarts[g] < knots.groupStarts[g + 1] )
			{
				b3SyncBlock* block = knotBlocks + knotBlockCount;
				block->block = (b3SolverBlock){ g, 1, b3_knotBlock, UINT8_MAX };
				b3AtomicStoreInt( &block->syncIndex, 0 );
				knotBlockCount += 1;
			}
		}

		// Added for Nebenan: in a step that times its team an entry per thread for each stage it runs and each pack it
		// solves, see b3TeamEntry. The substeps and the rounds of restitution run their stages again. Taken before the
		// split of an island, which takes from the stack on another thread.
		bool sampleTeam = team && world->stepIndex % B3_TEAM_SAMPLE == 0;
		int teamEntryCapacity = 0;
		b3TeamEntry* teamEntries = NULL;
		if ( sampleTeam )
		{
			int colorStages = 1 + activeColorCount;
			teamEntryCapacity = 5 + stepContext->subStepCount * ( 2 + ( 1 + ITERATIONS + RELAX_ITERATIONS ) * colorStages ) +
								world->restitutionIterations * colorStages + packCount;
			teamEntries = (b3TeamEntry*)b3StackAlloc( &world->stack, workerCount * teamEntryCapacity * sizeof( b3TeamEntry ),
													  "team entries" );
		}

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
		// Added for Nebenan: with a team over the bodies of its packs, see above
		if ( team )
		{
			b3InitRangeBlocks( bodyBlocks, packBodyStarts[0], packBodyStarts[teamPackCount] - packBodyStarts[0], bodyDim.size,
							   b3_packBodyBlock, UINT8_MAX );
		}
		else
		{
			b3InitBlocks( bodyBlocks, bodyDim, awakeBodyCount, b3_bodyBlock, UINT8_MAX );
		}

		// Prepare blocks as a single flat parallel-for over the whole constraint range.
		// The task walks spans to decode flat slot indices back to per-color arrays.
		// Added for Nebenan: with a team over the ranges of its packs, see above
		int convexBlockCount = convexPrepareDim.count;
		if ( team )
		{
			convexBlockCount = 0;
			for ( int i = 0; i < activeColorCount; ++i )
			{
				const int* starts = packWideStarts + i * ( packCount + 1 );
				convexBlockCount +=
					b3InitRangeBlocks( convexBlocks + convexBlockCount, widePrepareSpans[i].start + starts[0],
									   starts[teamPackCount] - starts[0], teamWideSize, b3_wideContactBlock, UINT8_MAX );
			}

			convexBlockCount += b3InitRangeBlocks( convexBlocks + convexBlockCount, knotWideStart + knotWide * knots.packStarts[0],
												   knotWide * ( knots.packStarts[teamPackCount] - knots.packStarts[0] ),
												   teamWideSize, b3_wideContactBlock, UINT8_MAX );
			B3_ASSERT( convexBlockCount <= convexPrepareDim.count );
		}
		else
		{
			b3InitBlocks( convexBlocks, convexPrepareDim, wideContactCount, b3_wideContactBlock, UINT8_MAX );
		}
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

			// Added for Nebenan: with a team the range of its packs, see above
			if ( team )
			{
				const int* starts = packWideStarts + i * ( packCount + 1 );
				b3InitRangeBlocks( baseGraphBlock, starts[0], starts[teamPackCount] - starts[0], graphWideContactDims[i].size,
								   b3_graphWideContactBlock, colorIndex );
			}
			else
			{
				b3InitBlocks( baseGraphBlock, graphWideContactDims[i], colorWideContactCounts[i], b3_graphWideContactBlock,
							  colorIndex );
			}
			baseGraphBlock += graphWideContactDims[i].count;

			b3InitBlocks( baseGraphBlock, graphContactDims[i], colorContactCounts[i], b3_graphContactBlock, colorIndex );
			baseGraphBlock += graphContactDims[i].count;

			graphBlockCounts[i] = graphJointDims[i].count + graphWideContactDims[i].count + graphContactDims[i].count;
		}

		B3_ASSERT( (ptrdiff_t)( baseGraphBlock - graphBlocks ) == graphBlockCount );

		b3SolverStage* stage = stages;
		stage = b3InitStage( stage, b3_stagePrepareJoints, jointBlocks, jointPrepareDim.count, UINT8_MAX );
		stage = b3InitStage( stage, b3_stagePrepareWideContacts, convexBlocks, convexBlockCount, UINT8_MAX );
		stage = b3InitStage( stage, b3_stagePrepareContacts, meshBlocks, meshPrepareDim.count, UINT8_MAX );
		stage = b3InitStage( stage, b3_stageIntegrateVelocities, bodyBlocks, bodyDim.count, UINT8_MAX );

		// Added for Nebenan: the stage of the knots before each sweep over the colors, see b3ExecuteKnotStage
		stage = b3InitStage( stage, b3_stageWarmStart, knotBlocks, knotBlockCount, UINT8_MAX );
		stage = b3InitColorStages( stage, b3_stageWarmStart, 1, activeColorCount, graphColorBlocks, graphBlockCounts,
								   activeColorIndices );
		for ( int j = 0; j < ITERATIONS; ++j )
		{
			stage = b3InitStage( stage, b3_stageSolve, knotBlocks, knotBlockCount, UINT8_MAX );
			stage = b3InitColorStages( stage, b3_stageSolve, 1, activeColorCount, graphColorBlocks, graphBlockCounts,
									   activeColorIndices );
		}
		stage = b3InitStage( stage, b3_stageIntegratePositions, bodyBlocks, bodyDim.count, UINT8_MAX );
		for ( int j = 0; j < RELAX_ITERATIONS; ++j )
		{
			stage = b3InitStage( stage, b3_stageRelax, knotBlocks, knotBlockCount, UINT8_MAX );
			stage = b3InitColorStages( stage, b3_stageRelax, 1, activeColorCount, graphColorBlocks, graphBlockCounts,
									   activeColorIndices );
		}
		stage = b3InitStage( stage, b3_stageRestitution, knotBlocks, knotBlockCount, UINT8_MAX );
		stage = b3InitColorStages( stage, b3_stageRestitution, 1, activeColorCount, graphColorBlocks, graphBlockCounts,
								   activeColorIndices );
		stage = b3InitStage( stage, b3_stageStoreWideImpulses, convexBlocks, convexBlockCount, UINT8_MAX );
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
		stepContext->team = team;
		stepContext->sampleTeam = sampleTeam;
		stepContext->sampleSolver = world->stepIndex % B3_SOLVER_SAMPLE == 0;
		stepContext->teamEntries = teamEntries;
		stepContext->teamEntryCapacity = teamEntryCapacity;
		stepContext->teamMergeTime = 0.0f;
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
		stepContext->knotGroupStarts = knots.groupStarts;
		stepContext->knotCopies = knots.copies;
		stepContext->knotSplits = knots.splits;
		stepContext->packSplitStarts = knots.packSplitStarts;
		// Added for Nebenan: a team solves the overflow of its packs, the first in the order of the packs, and takes the other
		// packs after them, see b3SolveOtherPack
		stepContext->stagePackCount = stagePackCount;
		stepContext->overflowContactCount = team ? packOverflowStarts[teamPackCount] : otherCount;
		b3AtomicStoreInt( &stepContext->packNext, team ? teamPackCount : 0 );
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

			// Added for Nebenan: see b3Profile::teamTime and b3Profile::solverJoints
			taskContext->teamWait = 0.0f;
			taskContext->teamPacks = 0.0f;
			taskContext->teamStages = 0;
			taskContext->teamYields = 0;
			taskContext->teamEntryCount = 0;
			memset( taskContext->solverTicks, 0, sizeof( taskContext->solverTicks ) );
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

		// Added for Nebenan: where the solver worked, summed over the threads, see b3Profile::solverJoints
		if ( stepContext->sampleSolver )
		{
			uint64_t parts[3] = { 0, 0, 0 };
			for ( int i = 0; i < world->workerCount; ++i )
			{
				const b3TaskContext* taskContext = world->taskContexts.data + i;
				for ( int k = 0; k < 3; ++k )
				{
					parts[k] += taskContext->solverTicks[k];
				}
			}
			world->profile.solverJoints = b3TicksToMilliseconds( parts[b3_solverJoints] );
			world->profile.solverContacts = b3TicksToMilliseconds( parts[b3_solverContacts] );
			world->profile.solverBodies = b3TicksToMilliseconds( parts[b3_solverBodies] );
			world->solverSample = 1;
		}

		// Added for Nebenan: the waits of the calling thread and of the workers in a step that timed its team, see
		// b3Profile::teamTime
		if ( stepContext->sampleTeam )
		{
			world->teamSample = 1;
			world->teamWorkerCount = workerCount;
			world->teamStageCount = world->taskContexts.data[0].teamStages;
			world->profile.teamStageWait = world->taskContexts.data[0].teamWait;
			for ( int i = 1; i < workerCount; ++i )
			{
				b3TaskContext* taskContext = world->taskContexts.data + i;
				world->profile.teamWorkerWait += taskContext->teamWait;
				world->profile.teamWorkerPacks += taskContext->teamPacks;
				world->teamYieldCount += taskContext->teamYields;
			}

			b3SumTeamStages( world, stepContext );
		}

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
		if ( teamEntries != NULL )
		{
			b3StackFree( &world->stack, teamEntries );
		}
		b3StackFree( &world->stack, knotBlocks );
		b3StackFree( &world->stack, graphBlocks );
		b3StackFree( &world->stack, jointBlocks );
		b3StackFree( &world->stack, meshBlocks );
		b3StackFree( &world->stack, convexBlocks );
		b3StackFree( &world->stack, bodyBlocks );
		b3StackFree( &world->stack, stages );
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
		if ( orderedContacts != NULL && packCount == 0 )
		{
			b3StackFree( &world->stack, orderedContacts );
		}
		if ( packCount > 0 )
		{
			b3StackFree( &world->stack, packContacts );
			b3StackFree( &world->stack, packWideStarts );
			b3StackFree( &world->stack, islandPacks );
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
