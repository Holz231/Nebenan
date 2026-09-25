// SPDX-License-Identifier: MIT
// Nebenan - polygonal real-time destruction for Box3D

#pragma once

#include "base.h"

#include "box3d/types.h"

/**
 * @defgroup ids Ids
 * Handles to destruction objects. Ids are small value types that carry a generation,
 * so stale ids are detected instead of silently referring to reused memory.
 * @{
 */

/// Destruction world id. References a destruction world attached to one Box3D world.
typedef struct nbWorldId
{
	uint16_t index1;
	uint16_t generation;
} nbWorldId;

/// A destructible structure: one or more convex pieces that share a support graph.
typedef struct nbDestructibleId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbDestructibleId;

/// A chunk is a single convex polyhedron. Every chunk owns exactly one Box3D hull shape.
typedef struct nbChunkId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbChunkId;

static const nbWorldId nb_nullWorldId = NB_ZERO_INIT;
static const nbDestructibleId nb_nullDestructibleId = NB_ZERO_INIT;
static const nbChunkId nb_nullChunkId = NB_ZERO_INIT;

/// Use these to test for null. Example: `if ( NB_IS_NULL( chunkId ) ) { ... }`
#define NB_IS_NULL( id ) ( ( id ).index1 == 0 )

/// Use these to test for non-null.
#define NB_IS_NON_NULL( id ) ( ( id ).index1 != 0 )

/// Compare two ids for equality.
#define NB_ID_EQUALS( id1, id2 ) ( ( id1 ).index1 == ( id2 ).index1 && ( id1 ).generation == ( id2 ).generation )

/** @} */

/**
 * @defgroup material Material
 * @{
 */

/// Physical and fracture properties of a destructible material.
typedef struct nbMaterial
{
	/// Mass density in kg/m^3. Concrete is about 2400, brick 1900, wood 600.
	float density;

	/// Coulomb friction coefficient of the chunk shapes.
	float friction;

	/// Coefficient of restitution of the chunk shapes.
	float restitution;

	/// Damage needed to break one square meter of bond area. Bonds are the glued faces between chunks.
	/// Larger values make the material tougher.
	float strength;

	/// Typical edge length of the fragments created at the center of an impact, in meters.
	float fragmentSize;

	/// Fragments with a smaller volume are turned into dust and removed, in cubic meters.
	float minFragmentVolume;

	/// Maximum number of times a chunk can be refined by runtime fracture. Limits how deep the
	/// fracture hierarchy can go around repeated impacts.
	int maxDepth;

	/// Longest horizontal distance, in meters, a glued chunk may be from its support. Load has to
	/// travel sideways through the structure to reach an anchor, and parts that hang out further
	/// than this break off, like a slab that lost its columns. Zero disables the check.
	float maxSpan;

	/// User material id stored on the Box3D shapes. It is reported by ray casts and contact events.
	uint64_t userMaterialId;
} nbMaterial;

/** @} */

/**
 * @defgroup destructible Destructible
 * @{
 */

/// An anchor plane in the local frame of a destructible. Chunks that touch the half space
/// `dot(normal, x) <= offset` are glued to the world. Chunks without a path of intact bonds to an
/// anchored chunk collapse.
typedef struct nbAnchorPlane
{
	b3Vec3 normal;
	float offset;
} nbAnchorPlane;

/// Maximum number of anchor planes per destructible.
#define NB_MAX_ANCHORS 8

/// A convex piece of a destructible. Pieces are given in the local frame of the destructible.
/// Touching faces of different pieces are bonded together automatically.
typedef struct nbPieceDef
{
	/// Points of the convex piece. The convex hull of the points is used.
	/// Leave null to make a box from halfExtents and transform.
	const b3Vec3* points;

	/// Number of points.
	int pointCount;

	/// Half extents of a box piece. Used when points is null.
	b3Vec3 halfExtents;

	/// Local transform of the piece within the destructible.
	b3Transform transform;

	/// Render material id of the outer surface of this piece.
	uint8_t surfaceMaterial;

	/// Render material id of the fracture surfaces created inside this piece.
	uint8_t interiorMaterial;
} nbPieceDef;

/// Destructible definition. Must be initialized with nbDefaultDestructibleDef.
typedef struct nbDestructibleDef
{
	/// World position of the destructible frame.
	b3Pos position;

	/// World rotation of the destructible frame.
	b3Quat rotation;

	/// Material of all pieces.
	nbMaterial material;

	/// Static destructibles are glued to the world through their anchors. Dynamic destructibles
	/// start as a free rigid body.
	bool isStatic;

	/// Anchor planes in the local frame. If none are given a static destructible is anchored
	/// at its lowest point along local -Y.
	nbAnchorPlane anchors[NB_MAX_ANCHORS];

	/// Number of anchor planes.
	int anchorCount;

	/// Optional pre-fracture of every piece into Voronoi cells of roughly this size, in meters.
	/// Zero starts every piece as a single chunk. Runtime fracture refines chunks around impacts
	/// either way.
	float cellSize;

	/// Seed for the deterministic fracture patterns.
	uint32_t seed;

	/// Collision filter of the chunk shapes.
	b3Filter filter;

	/// Collisions of dynamic chunks with other shapes can damage both sides.
	bool enableCollisionDamage;

	/// User data.
	void* userData;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbDestructibleDef;

/** @} */

/**
 * @defgroup impact Impact
 * @{
 */

/// Describes an impact such as a bullet hit or an explosion.
typedef struct nbImpactDef
{
	/// Impact point in world space.
	b3Pos point;

	/// Direction of the incoming projectile in world space. Fragments behind the surface are ejected
	/// along it. Use zero for an explosion that ejects radially.
	b3Vec3 direction;

	/// Optional surface normal at the impact point in world space. Fragments near the surface spall
	/// back out along it, like the crater of a bullet hole. nbWorld_CastImpact fills it in.
	b3Vec3 normal;

	/// Damage radius in meters. Chunks overlapping this sphere are refined and damaged.
	float radius;

	/// Damage at the impact center. Damage falls off to zero at the radius.
	/// A bond breaks when its accumulated damage exceeds material strength times bond area.
	float damage;

	/// Speed of the fragments ejected at the impact center in meters per second.
	float ejectSpeed;

	/// Number of fragments created by runtime fracture around the impact center.
	/// Zero derives the count from the material fragment size and the damaged volume.
	int fragmentCount;

	/// Limit the impact to one destructible. Null affects all destructibles.
	nbDestructibleId destructibleId;
} nbImpactDef;

/// What an impact did.
typedef struct nbImpactResult
{
	/// Chunks that were split by runtime Voronoi fracture.
	int fracturedChunkCount;

	/// New chunks created by the fracture.
	int createdChunkCount;

	/// Bonds broken by damage.
	int brokenBondCount;

	/// Chunks that changed from glued to free.
	int detachedChunkCount;

	/// New rigid bodies created for debris and collapsing parts.
	int createdBodyCount;

	/// Time spent in the Voronoi fracture in milliseconds.
	float fractureTime;

	/// Total time of the impact in milliseconds.
	float totalTime;
} nbImpactResult;

/** @} */

/**
 * @defgroup world World
 * @{
 */

/// Destruction world definition. Must be initialized with nbDefaultWorldDef.
typedef struct nbWorldDef
{
	/// The Box3D world that simulates the chunks. Required.
	b3WorldId physicsWorld;

	/// Maximum number of free debris bodies. When exceeded, the oldest small debris are removed.
	int maxDebrisBodies;

	/// Small debris are removed after this many seconds. Zero keeps them forever.
	float debrisLifetime;

	/// Debris with a volume below this value count as small debris, in cubic meters.
	float smallDebrisVolume;

	/// Debris that fall below this height along the gravity direction are removed, in meters.
	float killDepth;

	/// Minimum approach speed for collision damage in meters per second.
	float collisionSpeedThreshold;

	/// Scales the collision energy (Joule) into damage.
	float collisionDamageScale;

	/// Damage radius of a collision is this scale times the cube root of the collision energy (Joule).
	float collisionRadiusScale;

	/// Maximum number of collision impacts processed per update. Limits worst case frame time.
	int maxCollisionImpactsPerUpdate;

	/// Upper bound for the fragments one impact creates. Bounds the cost of large explosions.
	int maxFragmentsPerImpact;

	/// Fraction of the approach speed a body keeps when it breaks through a chunk. Box3D resolves the
	/// contact before the fracture happens, so without this a cannonball would bounce off the debris.
	float collisionPassThrough;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbWorldDef;

/// Destruction events collected since the last call to nbWorld_GetEvents. Use them to keep a
/// renderer in sync: build meshes for created chunks, drop destroyed chunks and re-parent moved chunks.
typedef struct nbEvents
{
	/// Chunks created since the last call. They may already be destroyed again, check nbChunk_IsValid.
	const nbChunkId* createdChunks;

	/// Chunks destroyed since the last call.
	const nbChunkId* destroyedChunks;

	/// Chunks that moved to a different body since the last call. Created chunks are not repeated here.
	const nbChunkId* movedChunks;

	int createdCount;
	int destroyedCount;
	int movedCount;
} nbEvents;

/// Counters and timings of a destruction world.
typedef struct nbStats
{
	int destructibleCount;
	int chunkCount;
	int bondCount;
	int staticBodyCount;
	int dynamicBodyCount;
	int debrisCount;

	/// Totals since the world was created.
	int impactCount;
	int fractureCount;
	int createdChunkCount;

	/// Chunks whose physics hull needed the quickhull fallback instead of the direct build.
	int hullFallbackCount;

	/// Timings of the last update and the last impact in milliseconds.
	float updateTime;
	float lastImpactTime;
	float lastFractureTime;

	/// Bytes allocated by Nebenan (all worlds).
	int64_t byteCount;
} nbStats;

/** @} */

/**
 * @defgroup geometry Chunk geometry
 * @{
 */

/// A polygon face of a chunk.
typedef struct nbFace
{
	/// Outward plane in the local frame of the chunk body.
	b3Plane plane;

	/// First index into the index array.
	uint16_t firstIndex;

	/// Number of vertices of this convex polygon, counter clockwise seen from outside.
	uint8_t indexCount;

	/// Render material. The surface material of the piece or the interior material for fracture faces.
	uint8_t material;
} nbFace;

/// Read-only view of the convex polyhedron of a chunk. The data lives as long as the chunk.
typedef struct nbGeometry
{
	const b3Vec3* vertices;
	const nbFace* faces;
	const uint8_t* indices;
	int vertexCount;
	int faceCount;
	int indexCount;
} nbGeometry;

/// A vertex of a triangulated chunk mesh.
typedef struct nbMeshVertex
{
	/// Position in the local frame of the chunk body.
	b3Vec3 position;

	/// Flat face normal in the local frame of the chunk body.
	b3Vec3 normal;

	/// Box projected texture coordinates in meters. Seamless across chunks of the same destructible.
	float u, v;

	/// Render material of the face.
	uint32_t material;
} nbMeshVertex;

/** @} */
