// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "arena_allocator.h"
#include "container.h"

#include "box3d/collision.h"
#include "box3d/types.h"

#define B3_FORCE_GHOST_COLLISIONS 0

typedef struct b3Shape b3Shape;
typedef struct b3World b3World;

typedef union b3ContactCache
{
	b3SATCache satCache;
	b3SimplexCache simplexCache;
} b3ContactCache;

typedef struct b3TriangleCache
{
	int triangleIndex;
	b3ContactCache cache;
} b3TriangleCache;

b3DeclareArray( b3TriangleCache );

enum b3ContactFlags
{
	// Set when the solid shapes are touching.
	b3_contactTouchingFlag = 0x00000001,

	// Contact has a hit event
	b3_contactHitEventFlag = 0x00000002,

	// This contact wants contact events
	b3_contactEnableContactEvents = 0x00000004,

	// This is contact is between a dynamic and static body
	b3_contactStaticFlag = 0x00000008,

	b3_contactRecycleFlag = 0x00000010,

	// Set when the shapes are touching
	b3_simTouchingFlag = 0x00010000,

	// This contact no longer has overlapping AABBs
	b3_simDisjoint = 0x00020000,

	// This contact started touching
	b3_simStartedTouching = 0x00040000,

	// This contact stopped touching
	b3_simStoppedTouching = 0x00080000,

	// This contact has a hit event
	b3_simEnableHitEvent = 0x00100000,

	// This contact wants pre-solve events
	b3_simEnablePreSolveEvents = 0x00200000,

	// This is a mesh contact
	b3_simMeshContact = 0x00400000,

	// Relative transform is cached for contact recycling
	b3_relativeTransformValid = 0x00800000,

	// Enable speculative contact points
	b3_enableSpeculativePoints = 0x01000000,

	// Added for Nebenan: two hulls a face of one kept apart at the last test, see b3ConvexContact::gapPlane
	b3_simSeparatedHulls = 0x02000000,
};

// A contact edge is used to connect bodies and contacts together
// in a contact graph where each body is a node and each contact
// is an edge. A contact edge belongs to a doubly linked list
// maintained in each attached body. Each contact has two contact
// edges, one for each attached body.
typedef struct b3ContactEdge
{
	int bodyId;
	int prevKey;
	int nextKey;
} b3ContactEdge;

// Added for Nebenan: a contact in the lists of the touching contacts of its bodies, see b3Body::headTouchKey, kept in
// b3World::contactTouches by contact id, apart from the contact, so the collide loop reads no more than before. The keys
// are [31 : contactId | 1 : edgeIndex] like those of the contact lists of the bodies. Creation counts the contacts the
// world created before this one. A body puts a new contact at the front of its contact list, so that list runs from the
// highest creation to the lowest. Padded to 32 bytes, so a record never straddles two cache lines.
typedef struct b3ContactTouch
{
	int prevKey[2];
	int nextKey[2];
	uint64_t creation;
	uint64_t padding;
} b3ContactTouch;

typedef struct b3MeshContact
{
	b3Array( b3TriangleCache ) triangleCache;
	b3AABB queryBounds;
} b3MeshContact;

typedef struct b3ConvexContact
{
	b3ContactCache cache;

	// Added for Nebenan: for two hulls a face kept apart, the plane of that face in the frame of its body, and the center
	// and the half extents of the bounds of the other hull in the frame of its body. With b3SATCache::gap they tell how
	// far the bodies may move until the face could let the hulls come within the speculative distance, see
	// b3CollideTask. Larger than b3MeshContact, so a contact takes 248 bytes instead of 232.
	b3Plane gapPlane;
	b3Vec3 gapCenter;
	b3Vec3 gapExtents;
} b3ConvexContact;

// Represents the persistent interaction between two shapes
typedef struct b3Contact
{
	// index of simulation set stored in b3World
	// B3_NULL_INDEX when slot is free
	int setIndex;

	// index into the constraint graph color array
	// B3_NULL_INDEX for non-touching or sleeping contacts
	// B3_NULL_INDEX when slot is free
	int colorIndex;

	// contact index within set or graph color
	// B3_NULL_INDEX when slot is free
	int localIndex;

	b3ContactEdge edges[2];
	int shapeIdA;
	int shapeIdB;
	int childIndex;

	// Added for Nebenan: links in the contact lists of shape A and shape B, see b3Shape::headContactKey
	int shapePrevKey[2];
	int shapeNextKey[2];

	// A contact only belongs to an island if touching, otherwise B3_NULL_INDEX.
	int islandId;

	// Index into the island's contacts array for O(1) swap-removal.
	// B3_NULL_INDEX when not in an island.
	int islandIndex;

	// Back index into b3World::contacts
	int contactId;

	// Encoded body sim indices to avoid a cache miss accessing the body in the narrow phase.
	int encodedBodySimA;
	int encodedBodySimB;

	// b3ContactFlags
	uint32_t flags;

	// todo embed single manifold with ability to allocate overfloat, use union.
	b3Manifold* manifolds;
	int manifoldCount;

	// Cache for contact recycling.
	b3Quat cachedRotationA;
	b3Quat cachedRotationB;
	b3Transform cachedRelativePose;

	// Mixed friction and restitution
	float friction;

	// Added for Nebenan: the key last written to b3World::contactSortKeys for this contact, B3_NULL_INDEX before the
	// first. The collide task compares with it here, in the contact it has in the cache anyway, instead of reading the
	// array. Fills the padding before the union, so a contact stays the same size.
	int sortKey;

	// Usage determined by b3_simMeshContact in simFlags
	union
	{
		b3ConvexContact convexContact;
		b3MeshContact meshContact;
	};

	float restitution;
	float rollingResistance;
	b3Vec3 tangentVelocity;

	// This is monotonically advanced when a contact is allocated in this slot
	// Used to check for invalid b3ContactId
	uint32_t generation;
} b3Contact;

typedef struct b3ContactSpec
{
	int contactId;

	// Start of the global manifold constraint array
	int manifoldStart;
	uint16_t manifoldCount;
} b3ContactSpec;

b3DeclareArray( b3ContactSpec );

void b3InitializeContactRegisters( void );

void b3CreateContact( b3World* world, b3Shape* shapeA, b3Shape* shapeB, int childIndex );
void b3DestroyContact( b3World* world, b3Contact* contact, bool wakeBodies );

// Added for Nebenan: prefetch what b3DestroyContact touches besides the contact itself, for a contact that a loop destroys
// soon
void b3PrefetchContactDestroy( const b3World* world, const b3Contact* contact );

// Added for Nebenan: put a contact that starts touching at the front of the lists of the touching contacts of its bodies,
// and take one that stops touching or is destroyed out of them, see b3ContactTouch
void b3LinkTouchingContact( b3World* world, const b3Contact* contact );
void b3UnlinkTouchingContact( b3World* world, const b3Contact* contact );

// Added for Nebenan: prefetch what linking a contact that is not touching yet, or unlinking a touching one, writes in the
// records of other contacts: the first records of the lists of its bodies, or its neighbours there. It reads the bodies
// and the record of the contact, which a loop prefetches a step before.
void b3PrefetchTouchLinks( const b3World* world, const b3Contact* contact );

bool b3UpdateContact( b3World* world, int workerIndex, b3Contact* contact, b3Shape* shapeA, b3Vec3 localCenterA, b3WorldTransform xfA,
					  b3Shape* shapeB, b3Vec3 localCenterB, b3WorldTransform xfB, bool isFast, b3Arena arena );

bool b3ComputeMeshManifolds( b3World* world, int workerIndex, b3Contact* contact, const b3Shape* shapeA, const int* materialMap,
							 b3WorldTransform xfA, const b3Shape* shapeB, b3WorldTransform xfB, bool isFast, b3Arena arena );
