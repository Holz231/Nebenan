// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "container.h"

#include "box3d/math_functions.h"

#include <stdint.h>

typedef struct b3Contact b3Contact;
typedef struct b3Joint b3Joint;
typedef struct b3World b3World;

// Added for Nebenan: the domains a large island solves in, see b3Domains in src/solver.c
#define B3_DOMAINS 8

// Added for Nebenan: the cells per side of the grid in which an island is cut into its domains, see b3CutIslandDomains
#define B3_DOMAIN_GRID 64

// Added for Nebenan: a node of a cut that does not cut, all its cells go to its first domain, see b3DomainCut
#define B3_DOMAIN_UNCUT 255

// Added for Nebenan: where the last cut of an island put its domains, so that the bodies of an island that joins take the
// domain where they lie, see b3PlaceInDomain. The corner of the grid of the cut and the inverse size of its cells along its
// two axes, then per node of the cut, the whole island first and then the halves of every node, the axis of the grid it
// cuts across and the cell its second half starts at, or B3_DOMAIN_UNCUT.
typedef struct b3DomainCut
{
	float lower[2];
	float invSize[2];
	uint8_t axes[2];
	uint8_t sides[B3_DOMAINS - 1];
	uint8_t starts[B3_DOMAINS - 1];
} b3DomainCut;

// Snapshots write a cut as its bytes, see world_snapshot.c, a cell fits in a byte, and the nodes that cut fill the first
// B3_DOMAINS - 1 places of the heap only when every node halves its domains evenly
_Static_assert( sizeof( b3DomainCut ) == 4 * sizeof( float ) + 2 + 2 * ( B3_DOMAINS - 1 ), "a cut has no padding" );
_Static_assert( B3_DOMAIN_GRID < B3_DOMAIN_UNCUT, "a cell fits in a byte" );
_Static_assert( ( B3_DOMAINS & ( B3_DOMAINS - 1 ) ) == 0, "the domains are a power of two" );

// Cached contact data stored in the island for fast contiguous iteration.
// Avoids touching b3Contact during union-find in b3SplitIsland.
typedef struct b3ContactLink
{
	int contactId;
	int bodyIdA;
	int bodyIdB;
} b3ContactLink;

b3DeclareArray( b3ContactLink );

// Cached joint data stored in the island for fast contiguous iteration.
typedef struct b3JointLink
{
	int jointId;
	int bodyIdA;
	int bodyIdB;
} b3JointLink;

b3DeclareArray( b3JointLink );

// Deterministic solver
//
// Collide all awake contacts
// Use bit array to emit start/stop touching events in defined order, per thread. Try using contact index, assuming contacts are
// created in a deterministic order. bit-wise OR together bit arrays and issue changes:
// - start touching: merge islands - temporary linked list - mark root island dirty - wake all - largest island is root
// - stop touching: increment constraintRemoveCount

// Persistent island for awake bodies, joints, and contacts.
// Contacts are touching.
// Contacts and joints may connect to static bodies, but static bodies are not in the island.
// https://en.wikipedia.org/wiki/Component_(graph_theory)
// https://en.wikipedia.org/wiki/Dynamic_connectivity
typedef struct b3Island
{
	// index of solver set stored in b3World
	// may be B3_NULL_INDEX
	int setIndex;

	// island index within set
	// may be B3_NULL_INDEX
	int localIndex;

	int islandId;

	// Keeps track of how many contacts have been removed from this island.
	// This is used to determine if an island is a candidate for splitting.
	int constraintRemoveCount;

	// I tried using a stack array for this but the data pointer goes out of
	// sync when the world island array grows.
	b3Array( int ) bodies;

	// Contacts and joints that belong to this island. May connect to static
	// bodies not in the island.
	// Each link has the two body ids so that b3SplitIsland's union-find pass
	// never needs to touch b3Contact/b3Joint.
	b3Array( b3ContactLink ) contacts;
	b3Array( b3JointLink ) joints;

	// Added for Nebenan: the island solves in domains, each of its bodies has one and its touching contacts lie in the
	// domains of the graph, see b3Domains in src/solver.c. How many contacts lie in each domain while the island is awake,
	// and the step the domains were cut last.
	bool domains;
	int domainContacts[B3_DOMAINS];
	int domainCutStep;
	b3DomainCut domainCut;
} b3Island;

// This is used to move islands across solver sets
typedef struct b3IslandSim
{
	int islandId;
} b3IslandSim;

b3Island* b3CreateIsland( b3World* world, int setIndex );

// Added for Nebenan: the domain where a cut puts a body with this center of mass, see b3DomainCut
int b3PlaceInDomain( const b3DomainCut* cut, b3Pos center );
void b3DestroyIsland( b3World* world, int islandId );

// Link contacts into the island graph when it starts having contact points
void b3LinkContact( b3World* world, b3Contact* contact );

// Unlink contact from the island graph when it stops having contact points
void b3UnlinkContact( b3World* world, b3Contact* contact );

// Link a joint into the island graph when it is created
void b3LinkJoint( b3World* world, b3Joint* joint );

// Unlink a joint from the island graph when it is destroyed
void b3UnlinkJoint( b3World* world, b3Joint* joint );

void b3SplitIsland( b3World* world, int baseId );
void b3SplitIslandTask( void* context );

// Added for Nebenan: the contacts of an island with domains go back to the colors and its bodies lose their domains, see
// b3Island::domains
void b3RemoveIslandDomains( b3World* world, b3Island* island );

void b3ValidateIsland( b3World* world, int islandId );
