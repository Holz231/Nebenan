// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#include "constraint_graph.h"

#include "bitset.h"
#include "body.h"
#include "contact.h"
#include "island.h"
#include "joint.h"
#include "physics_world.h"

#include <string.h>

// Solver using graph coloring. Islands are only used for sleep.
// High-Performance Physical Simulations on Next-Generation Architecture with Many Cores
// http://web.eecs.umich.edu/~msmelyan/papers/physsim_onmanycore_itj.pdf

// Kinematic bodies have to be treated like dynamic bodies in graph coloring. Unlike static bodies, we cannot use a dummy solver
// body for kinematic bodies. We cannot access a kinematic body from multiple threads efficiently because the SIMD solver body
// scatter would write to the same kinematic body from multiple threads. Even if these writes don't modify the body, they will
// cause horrible cache stalls. To make this feasible I would need a way to block these writes.

// This is used for debugging by making all constraints be assigned to overflow.
#define B3_FORCE_OVERFLOW 0

static const b3HexColor b3_graphColors[B3_GRAPH_COLOR_COUNT] = {
	b3_colorRed,	   b3_colorOrange,		b3_colorYellow,			b3_colorLimeGreen,		 b3_colorSpringGreen,
	b3_colorAqua,	   b3_colorDodgerBlue,	b3_colorBlueViolet,		b3_colorMagenta,		 b3_colorDeepPink,
	b3_colorCrimson,   b3_colorCoral,		b3_colorGold,			b3_colorGreenYellow,	 b3_colorMediumSeaGreen,
	b3_colorTurquoise, b3_colorDeepSkyBlue, b3_colorCornflowerBlue, b3_colorMediumSlateBlue, b3_colorMediumOrchid,
	b3_colorHotPink,   b3_colorTomato,		b3_colorKhaki,			b3_colorSilver,
};

b3HexColor b3GetGraphColor( int index )
{
	B3_ASSERT( 0 <= index && index < B3_GRAPH_COLOR_COUNT );
	return b3_graphColors[index];
}

void b3CreateGraph( b3ConstraintGraph* graph, int bodyCapacity )
{
	_Static_assert( B3_GRAPH_COLOR_COUNT >= 2, "must have at least two constraint graph colors" );
	_Static_assert( B3_OVERFLOW_INDEX == B3_GRAPH_COLOR_COUNT - 1, "bad over flow index" );

	*graph = (b3ConstraintGraph){ 0 };

	bodyCapacity = b3MaxInt( bodyCapacity, 8 );

	// Initialize graph color bit set.
	// No bitset for overflow color.
	for ( int i = 0; i < B3_OVERFLOW_INDEX; ++i )
	{
		b3GraphColor* color = graph->colors + i;
		color->bodySet = b3CreateBitSet( bodyCapacity );
		b3SetBitCountAndClear( &color->bodySet, bodyCapacity );
	}
}

void b3DestroyGraph( b3ConstraintGraph* graph )
{
	for ( int i = 0; i < B3_GRAPH_COLOR_COUNT; ++i )
	{
		b3GraphColor* color = graph->colors + i;

		// The bit set should never be used on the overflow color
		B3_ASSERT( i != B3_OVERFLOW_INDEX || color->bodySet.bits == NULL );

		b3DestroyBitSet( &color->bodySet );

		b3Array_Destroy( color->convexContacts );
		b3Array_Destroy( color->contacts );
		b3Array_Destroy( color->jointSims );

		// Added for Nebenan
		for ( int d = 0; d < B3_DOMAINS; ++d )
		{
			b3Array_Destroy( color->domainContacts[d] );
		}
	}

	// Added for Nebenan
	for ( int d = 0; d < B3_DOMAINS; ++d )
	{
		b3Array_Destroy( graph->domainOverflow[d] );
	}
}

// Added for Nebenan: counts a contact in or out of a domain at its bodies in an island and at that island, see
// b3Island::domains. A static body is in no island and counts nothing.
static void b3CountDomainContact( b3World* world, const b3Body* bodyA, const b3Body* bodyB, int domain, int delta )
{
	const b3Body* ends[2] = { bodyA, bodyB };
	int islandId = B3_NULL_INDEX;
	for ( int e = 0; e < 2; ++e )
	{
		const b3Body* body = ends[e];
		if ( body->islandId == B3_NULL_INDEX )
		{
			continue;
		}

		islandId = body->islandId;
		if ( body->id >= world->bodyDomainCounts.count )
		{
			int oldCount = world->bodyDomainCounts.count;
			b3Array_Resize( world->bodyDomainCounts, world->bodies.count );
			memset( world->bodyDomainCounts.data + oldCount, 0,
					(size_t)( world->bodyDomainCounts.count - oldCount ) * sizeof( b3DomainCounts ) );
		}

		uint16_t* count = world->bodyDomainCounts.data[body->id].counts + domain;
		B3_ASSERT( delta > 0 ? *count < UINT16_MAX : *count > 0 );
		*count = (uint16_t)( *count + delta );
	}

	B3_ASSERT( islandId != B3_NULL_INDEX );
	b3Island* island = b3Array_Get( world->islands, islandId );
	island->domainContacts[domain] += delta;
}

int b3GetAnchorDomain( const b3Body* bodyA, const b3Body* bodyB )
{
	// A static body has no island. Of two bodies in the island the one with fewer contacts, so the contacts of a knot go
	// to the domains of its partners, the lower id of two with as many.
	const b3Body* anchor = bodyA->islandId != B3_NULL_INDEX ? bodyA : bodyB;
	if ( bodyA->islandId != B3_NULL_INDEX && bodyB->islandId != B3_NULL_INDEX &&
		 ( bodyB->contactCount < bodyA->contactCount || ( bodyB->contactCount == bodyA->contactCount && bodyB->id < bodyA->id ) ) )
	{
		anchor = bodyB;
	}

	B3_ASSERT( 0 <= anchor->domain && anchor->domain < B3_DOMAINS );
	return anchor->domain;
}

// Contacts are always created as non-touching. They get cloned into the constraint
// graph once they are found to be touching.
void b3AddContactToGraph( b3World* world, b3Contact* contact )
{
	B3_ASSERT( contact->manifoldCount > 0 );
	B3_ASSERT( contact->flags & b3_contactTouchingFlag );

	b3ConstraintGraph* graph = &world->constraintGraph;
	int colorIndex = B3_OVERFLOW_INDEX;

	int bodyIdA = contact->edges[0].bodyId;
	int bodyIdB = contact->edges[1].bodyId;
	b3Body* bodyA = b3Array_Get( world->bodies, bodyIdA );
	b3Body* bodyB = b3Array_Get( world->bodies, bodyIdB );

	b3BodyType typeA = bodyA->type;
	b3BodyType typeB = bodyB->type;
	B3_ASSERT( typeA == b3_dynamicBody || typeB == b3_dynamicBody );

#if B3_FORCE_OVERFLOW == 0
	if ( typeA == b3_dynamicBody && typeB == b3_dynamicBody )
	{
		// Dynamic constraint colors cannot encroach on colors reserved for static constraints
		for ( int i = 0; i < B3_DYNAMIC_COLOR_COUNT; ++i )
		{
			b3GraphColor* color = graph->colors + i;
			if ( b3GetBit( &color->bodySet, bodyIdA ) || b3GetBit( &color->bodySet, bodyIdB ) )
			{
				continue;
			}

			b3SetBitGrow( &color->bodySet, bodyIdA );
			b3SetBitGrow( &color->bodySet, bodyIdB );
			colorIndex = i;
			break;
		}
	}
	else if ( typeA == b3_dynamicBody )
	{
		// Static constraint colors build from the end to get higher priority than dyn-dyn constraints
		for ( int i = B3_OVERFLOW_INDEX - 1; i >= 1; --i )
		{
			b3GraphColor* color = graph->colors + i;
			if ( b3GetBit( &color->bodySet, bodyIdA ) )
			{
				continue;
			}

			b3SetBitGrow( &color->bodySet, bodyIdA );
			colorIndex = i;
			break;
		}
	}
	else if ( typeB == b3_dynamicBody )
	{
		// Static constraint colors build from the end to get higher priority than dyn-dyn constraints
		for ( int i = B3_OVERFLOW_INDEX - 1; i >= 1; --i )
		{
			b3GraphColor* color = graph->colors + i;
			if ( b3GetBit( &color->bodySet, bodyIdB ) )
			{
				continue;
			}

			b3SetBitGrow( &color->bodySet, bodyIdB );
			colorIndex = i;
			break;
		}
	}
#endif

	bool isScalar = ( contact->flags & b3_simMeshContact ) || colorIndex == B3_OVERFLOW_INDEX;

	b3GraphColor* color = graph->colors + colorIndex;
	contact->colorIndex = colorIndex;
	contact->localIndex = isScalar ? color->contacts.count : color->convexContacts.count;
	contact->encodedBodySimA = b3EncodeBodySimIndex( bodyA );
	contact->encodedBodySimB = b3EncodeBodySimIndex( bodyB );

	// Added for Nebenan: a convex contact of an island with domains lies in the domain of its body with fewer contacts, in
	// the same color, see b3Island::domains. A mesh contact stays with the others, it keeps the solver from domains.
	B3_ASSERT( b3GetContactDomain( contact ) == -1 );
	if ( ( contact->flags & b3_simMeshContact ) == 0 && contact->islandId != B3_NULL_INDEX &&
		 world->islands.data[contact->islandId].domains )
	{
		int domain = b3GetAnchorDomain( bodyA, bodyB );
		if ( colorIndex == B3_OVERFLOW_INDEX )
		{
			B3_ASSERT( contact->manifoldCount < UINT16_MAX );
			contact->localIndex = graph->domainOverflow[domain].count;
			b3ContactSpec spec = {
				.contactId = contact->contactId,
				.manifoldStart = 0,
				.manifoldCount = (uint16_t)contact->manifoldCount,
			};
			b3Array_Push( graph->domainOverflow[domain], spec );
		}
		else
		{
			contact->localIndex = color->domainContacts[domain].count;
			b3Array_Push( color->domainContacts[domain], contact->contactId );
		}

		contact->flags |= (uint32_t)( domain + 1 ) << B3_CONTACT_DOMAIN_SHIFT;
		b3CountDomainContact( world, bodyA, bodyB, domain, 1 );
		return;
	}

	if ( isScalar )
	{
		B3_ASSERT( contact->manifoldCount < UINT16_MAX );
		b3ContactSpec spec = {
			.contactId = contact->contactId,
			.manifoldStart = 0,
			.manifoldCount = (uint16_t)contact->manifoldCount,
		};
		b3Array_Push( color->contacts, spec );
	}
	else
	{
		b3Array_Push( color->convexContacts, contact->contactId );
	}
}

// Added for Nebenan: takes a contact out of the array of its domain at colorIndex and localIndex, see b3Island::domains
static void b3RemoveDomainContact( b3World* world, b3Contact* contact, int domain, int colorIndex, int localIndex )
{
	b3ConstraintGraph* graph = &world->constraintGraph;
	if ( colorIndex == B3_OVERFLOW_INDEX )
	{
		int movedIndex = b3Array_RemoveSwap( graph->domainOverflow[domain], localIndex );
		if ( movedIndex != B3_NULL_INDEX )
		{
			int movedContactId = graph->domainOverflow[domain].data[localIndex].contactId;
			b3Contact* movedContact = b3Array_Get( world->contacts, movedContactId );
			B3_ASSERT( movedContact->colorIndex == colorIndex && movedContact->localIndex == movedIndex );
			B3_ASSERT( b3GetContactDomain( movedContact ) == domain );
			movedContact->localIndex = localIndex;
		}
	}
	else
	{
		b3Array( int )* contacts = graph->colors[colorIndex].domainContacts + domain;
		int movedIndex = b3Array_RemoveSwap( *contacts, localIndex );
		if ( movedIndex != B3_NULL_INDEX )
		{
			b3Contact* movedContact = b3Array_Get( world->contacts, contacts->data[localIndex] );
			B3_ASSERT( movedContact->colorIndex == colorIndex && movedContact->localIndex == movedIndex );
			B3_ASSERT( b3GetContactDomain( movedContact ) == domain );
			movedContact->localIndex = localIndex;
		}
	}

	const b3Body* bodyA = b3Array_Get( world->bodies, contact->edges[0].bodyId );
	const b3Body* bodyB = b3Array_Get( world->bodies, contact->edges[1].bodyId );
	b3CountDomainContact( world, bodyA, bodyB, domain, -1 );
	contact->flags &= ~(uint32_t)b3_contactDomainBits;
}

void b3SetContactDomain( b3World* world, b3Contact* contact, int domain )
{
	B3_ASSERT( contact->setIndex == b3_awakeSet && contact->colorIndex != B3_NULL_INDEX );
	int current = b3GetContactDomain( contact );
	if ( current == domain )
	{
		return;
	}

	// A mesh contact never lies in a domain
	B3_ASSERT( ( contact->flags & b3_simMeshContact ) == 0 );

	b3ConstraintGraph* graph = &world->constraintGraph;
	int colorIndex = contact->colorIndex;
	b3GraphColor* color = graph->colors + colorIndex;
	const b3Body* bodyA = b3Array_Get( world->bodies, contact->edges[0].bodyId );
	const b3Body* bodyB = b3Array_Get( world->bodies, contact->edges[1].bodyId );

	// Out of its array, the color keeps its bodies
	if ( current >= 0 )
	{
		b3RemoveDomainContact( world, contact, current, colorIndex, contact->localIndex );
	}
	else if ( colorIndex == B3_OVERFLOW_INDEX )
	{
		int localIndex = contact->localIndex;
		int movedIndex = b3Array_RemoveSwap( color->contacts, localIndex );
		if ( movedIndex != B3_NULL_INDEX )
		{
			b3Contact* movedContact = b3Array_Get( world->contacts, color->contacts.data[localIndex].contactId );
			B3_ASSERT( movedContact->localIndex == movedIndex );
			movedContact->localIndex = localIndex;
		}
	}
	else
	{
		int localIndex = contact->localIndex;
		int movedIndex = b3Array_RemoveSwap( color->convexContacts, localIndex );
		if ( movedIndex != B3_NULL_INDEX )
		{
			b3Contact* movedContact = b3Array_Get( world->contacts, color->convexContacts.data[localIndex] );
			B3_ASSERT( movedContact->localIndex == movedIndex );
			movedContact->localIndex = localIndex;
		}
	}

	// Into the array of the new domain, or of the color
	b3ContactSpec spec = {
		.contactId = contact->contactId,
		.manifoldStart = 0,
		.manifoldCount = (uint16_t)contact->manifoldCount,
	};
	if ( domain >= 0 )
	{
		if ( colorIndex == B3_OVERFLOW_INDEX )
		{
			contact->localIndex = graph->domainOverflow[domain].count;
			b3Array_Push( graph->domainOverflow[domain], spec );
		}
		else
		{
			contact->localIndex = color->domainContacts[domain].count;
			b3Array_Push( color->domainContacts[domain], contact->contactId );
		}

		contact->flags |= (uint32_t)( domain + 1 ) << B3_CONTACT_DOMAIN_SHIFT;
		b3CountDomainContact( world, bodyA, bodyB, domain, 1 );
	}
	else if ( colorIndex == B3_OVERFLOW_INDEX )
	{
		contact->localIndex = color->contacts.count;
		b3Array_Push( color->contacts, spec );
	}
	else
	{
		contact->localIndex = color->convexContacts.count;
		b3Array_Push( color->convexContacts, contact->contactId );
	}
}

void b3RemoveContactFromGraph( b3World* world, b3Contact* contact, int bodyIdA, int bodyIdB, int colorIndex, int localIndex,
							   bool meshContact )
{
	b3ConstraintGraph* graph = &world->constraintGraph;

	B3_ASSERT( 0 <= colorIndex && colorIndex < B3_GRAPH_COLOR_COUNT );
	b3GraphColor* color = graph->colors + colorIndex;

	if ( colorIndex != B3_OVERFLOW_INDEX )
	{
		// This might clear a bit for a static body, but this has no effect
		b3ClearBit( &color->bodySet, bodyIdA );
		b3ClearBit( &color->bodySet, bodyIdB );
	}

	// Added for Nebenan: a contact in a domain, see b3Island::domains
	int domain = b3GetContactDomain( contact );
	if ( domain >= 0 )
	{
		b3RemoveDomainContact( world, contact, domain, colorIndex, localIndex );
		return;
	}

	if ( meshContact || colorIndex == B3_OVERFLOW_INDEX )
	{
		int movedIndex = b3Array_RemoveSwap( color->contacts, localIndex );
		if ( movedIndex != B3_NULL_INDEX )
		{
			// Fix index on swapped contact
			int movedContactId = color->contacts.data[localIndex].contactId;
			b3Contact* movedContact = b3Array_Get( world->contacts, movedContactId );
			B3_ASSERT( movedContact->setIndex == b3_awakeSet );
			B3_ASSERT( movedContact->colorIndex == colorIndex );
			B3_ASSERT( movedContact->localIndex == movedIndex );
			movedContact->localIndex = localIndex;
		}
	}
	else
	{
		int movedIndex = b3Array_RemoveSwap( color->convexContacts, localIndex );
		if ( movedIndex != B3_NULL_INDEX )
		{
			// Fix index on swapped contact
			int movedContactId = color->convexContacts.data[localIndex];
			b3Contact* movedContact = b3Array_Get( world->contacts, movedContactId );
			B3_ASSERT( movedContact->setIndex == b3_awakeSet );
			B3_ASSERT( movedContact->colorIndex == colorIndex );
			B3_ASSERT( movedContact->localIndex == movedIndex );
			B3_ASSERT( ( movedContact->flags & b3_simMeshContact ) == 0 );
			movedContact->localIndex = localIndex;
		}
	}
}

static int b3AssignJointColor( b3ConstraintGraph* graph, int bodyIdA, int bodyIdB, b3BodyType typeA, b3BodyType typeB )
{
	B3_ASSERT( typeA == b3_dynamicBody || typeB == b3_dynamicBody );
	B3_UNUSED( graph );
	B3_UNUSED( bodyIdA );
	B3_UNUSED( bodyIdB );
	B3_UNUSED( typeA );
	B3_UNUSED( typeB );

#if B3_FORCE_OVERFLOW == 0
	if ( typeA == b3_dynamicBody && typeB == b3_dynamicBody )
	{
		// Dynamic constraint colors cannot encroach on colors reserved for static constraints
		for ( int i = 0; i < B3_DYNAMIC_COLOR_COUNT; ++i )
		{
			b3GraphColor* color = graph->colors + i;
			if ( b3GetBit( &color->bodySet, bodyIdA ) || b3GetBit( &color->bodySet, bodyIdB ) )
			{
				continue;
			}

			b3SetBitGrow( &color->bodySet, bodyIdA );
			b3SetBitGrow( &color->bodySet, bodyIdB );
			return i;
		}
	}
	else if ( typeA == b3_dynamicBody )
	{
		// Static constraint colors build from the end to get higher priority than dyn-dyn constraints
		for ( int i = B3_OVERFLOW_INDEX - 1; i >= 1; --i )
		{
			b3GraphColor* color = graph->colors + i;
			if ( b3GetBit( &color->bodySet, bodyIdA ) )
			{
				continue;
			}

			b3SetBitGrow( &color->bodySet, bodyIdA );
			return i;
		}
	}
	else if ( typeB == b3_dynamicBody )
	{
		// Static constraint colors build from the end to get higher priority than dyn-dyn constraints
		for ( int i = B3_OVERFLOW_INDEX - 1; i >= 1; --i )
		{
			b3GraphColor* color = graph->colors + i;
			if ( b3GetBit( &color->bodySet, bodyIdB ) )
			{
				continue;
			}

			b3SetBitGrow( &color->bodySet, bodyIdB );
			return i;
		}
	}
#endif

	return B3_OVERFLOW_INDEX;
}

b3JointSim* b3CreateJointInGraph( b3World* world, b3Joint* joint )
{
	b3ConstraintGraph* graph = &world->constraintGraph;

	int bodyIdA = joint->edges[0].bodyId;
	int bodyIdB = joint->edges[1].bodyId;
	b3Body* bodyA = b3Array_Get( world->bodies, bodyIdA );
	b3Body* bodyB = b3Array_Get( world->bodies, bodyIdB );

	int colorIndex = b3AssignJointColor( graph, bodyIdA, bodyIdB, bodyA->type, bodyB->type );

	b3JointSim* jointSim = b3Array_Emplace( graph->colors[colorIndex].jointSims );
	memset( jointSim, 0, sizeof( b3JointSim ) );

	joint->colorIndex = colorIndex;
	joint->localIndex = graph->colors[colorIndex].jointSims.count - 1;
	return jointSim;
}

void b3AddJointToGraph( b3World* world, b3JointSim* jointSim, b3Joint* joint )
{
	b3JointSim* jointDst = b3CreateJointInGraph( world, joint );
	memcpy( jointDst, jointSim, sizeof( b3JointSim ) );
}

void b3RemoveJointFromGraph( b3World* world, int bodyIdA, int bodyIdB, int colorIndex, int localIndex )
{
	b3ConstraintGraph* graph = &world->constraintGraph;

	B3_ASSERT( 0 <= colorIndex && colorIndex < B3_GRAPH_COLOR_COUNT );
	b3GraphColor* color = graph->colors + colorIndex;

	if ( colorIndex != B3_OVERFLOW_INDEX )
	{
		// May clear static bodies, no effect
		b3ClearBit( &color->bodySet, bodyIdA );
		b3ClearBit( &color->bodySet, bodyIdB );
	}

	int movedIndex = b3Array_RemoveSwap( color->jointSims, localIndex );
	if ( movedIndex != B3_NULL_INDEX )
	{
		// Fix moved joint
		b3JointSim* movedJointSim = color->jointSims.data + localIndex;
		int movedId = movedJointSim->jointId;
		b3Joint* movedJoint = b3Array_Get( world->joints, movedId );
		B3_ASSERT( movedJoint->setIndex == b3_awakeSet );
		B3_ASSERT( movedJoint->colorIndex == colorIndex );
		B3_ASSERT( movedJoint->localIndex == movedIndex );
		movedJoint->localIndex = localIndex;
	}
}
