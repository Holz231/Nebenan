// SPDX-License-Identifier: MIT
// Nebenan - polygonal real-time destruction for Box3D
//
// Nebenan breaks convex polyhedra into convex polyhedra. There are no voxels. Every chunk is a
// Box3D hull shape, every free piece of debris is a Box3D rigid body, and structures are held
// together by a support graph of bonds between touching chunk faces.
//
// Typical frame:
//
//   b3World_Step( physicsWorld, 1.0f / 60.0f, 4 );
//   nbWorld_Update( destructionWorld, 1.0f / 60.0f );
//   nbEvents events = nbWorld_GetEvents( destructionWorld );
//   // update render meshes from the events, draw every chunk with the transform of its body

#pragma once

#include "base.h"
#include "types.h"

#include "box3d/box3d.h"

/**
 * @defgroup world World
 * A destruction world lives next to a Box3D world and owns all destructibles in it.
 * @{
 */

/// Default world definition.
NB_API nbWorldDef nbDefaultWorldDef( void );

/// Create a destruction world for an existing Box3D world.
NB_API nbWorldId nbCreateWorld( const nbWorldDef* def );

/// Destroy a destruction world and all bodies and shapes it created in the Box3D world.
NB_API void nbDestroyWorld( nbWorldId worldId );

/// Is this world id valid?
NB_API bool nbWorld_IsValid( nbWorldId worldId );

/// Process collision damage and debris management. Call once after every b3World_Step.
NB_API void nbWorld_Update( nbWorldId worldId, float timeStep );

/// Apply an impact immediately. This refines chunks near the impact with a Voronoi fracture,
/// damages bonds, detaches fragments and lets unsupported parts collapse.
NB_API nbImpactResult nbWorld_ApplyImpact( nbWorldId worldId, const nbImpactDef* def );

/// Cast a ray and apply the impact at the first chunk hit. The impact point and direction of
/// the definition are replaced by the hit point and the ray direction.
/// @return true if a chunk was hit
NB_API bool nbWorld_CastImpact( nbWorldId worldId, b3Pos origin, b3Vec3 translation, const nbImpactDef* def,
								nbImpactResult* result );

/// Get the events collected since the previous call. The arrays stay valid until the next call.
NB_API nbEvents nbWorld_GetEvents( nbWorldId worldId );

/// Get counters and timings.
NB_API nbStats nbWorld_GetStats( nbWorldId worldId );

/// Change the number of fracture workers, see nbWorldDef::workerCount. Restarts the internal threads.
NB_API void nbWorld_SetWorkerCount( nbWorldId worldId, int count );

/// Find the chunk that owns a Box3D shape. Returns null if the shape is not a chunk.
NB_API nbChunkId nbWorld_GetChunkFromShape( nbWorldId worldId, b3ShapeId shapeId );

/// Remove all free debris bodies.
NB_API void nbWorld_ClearDebris( nbWorldId worldId );

/** @} */

/**
 * @defgroup destructible Destructible
 * @{
 */

/// Default material. Plain concrete.
NB_API nbMaterial nbDefaultMaterial( void );

/// Default destructible definition.
NB_API nbDestructibleDef nbDefaultDestructibleDef( void );

/// Default piece definition, a unit cube.
NB_API nbPieceDef nbDefaultPieceDef( void );

/// Create a destructible from convex pieces. Touching faces of different pieces are bonded.
NB_API nbDestructibleId nbCreateDestructible( nbWorldId worldId, const nbDestructibleDef* def, const nbPieceDef* pieces,
											  int pieceCount );

/// Create a destructible box, for example a wall.
NB_API nbDestructibleId nbCreateBox( nbWorldId worldId, const nbDestructibleDef* def, b3Vec3 halfExtents );

/// Destroy a destructible with all of its chunks, bodies and shapes.
NB_API void nbDestroyDestructible( nbDestructibleId destructibleId );

/// Is this destructible id valid?
NB_API bool nbDestructible_IsValid( nbDestructibleId destructibleId );

/// Number of chunks of this destructible, glued or free.
NB_API int nbDestructible_GetChunkCount( nbDestructibleId destructibleId );

/// Get the chunks of this destructible.
/// @return the number of chunks written
NB_API int nbDestructible_GetChunks( nbDestructibleId destructibleId, nbChunkId* chunks, int capacity );

/// Get the user data of a destructible.
NB_API void* nbDestructible_GetUserData( nbDestructibleId destructibleId );

/** @} */

/**
 * @defgroup chunk Chunk
 * @{
 */

/// Is this chunk id valid?
NB_API bool nbChunk_IsValid( nbChunkId chunkId );

/// The body carrying this chunk. The chunk geometry is in the local frame of this body.
NB_API b3BodyId nbChunk_GetBody( nbChunkId chunkId );

/// The Box3D hull shape of this chunk.
NB_API b3ShapeId nbChunk_GetShape( nbChunkId chunkId );

/// The destructible this chunk belongs to.
NB_API nbDestructibleId nbChunk_GetDestructible( nbChunkId chunkId );

/// Volume in cubic meters.
NB_API float nbChunk_GetVolume( nbChunkId chunkId );

/// Centroid in the local frame of the chunk body.
NB_API b3Vec3 nbChunk_GetCentroid( nbChunkId chunkId );

/// Number of fracture generations between this chunk and its original piece.
NB_API int nbChunk_GetDepth( nbChunkId chunkId );

/// Material of the chunk, the one of its original piece. Zero for an invalid chunk.
NB_API nbMaterial nbChunk_GetMaterial( nbChunkId chunkId );

/// Number of intact bonds of this chunk.
NB_API int nbChunk_GetBondCount( nbChunkId chunkId );

/// Is the chunk glued to the world directly through an anchor?
NB_API bool nbChunk_IsAnchored( nbChunkId chunkId );

/// Is the chunk part of free debris or a collapsing part, as opposed to a static structure?
NB_API bool nbChunk_IsDynamic( nbChunkId chunkId );

/// The convex polyhedron of the chunk.
NB_API nbGeometry nbChunk_GetGeometry( nbChunkId chunkId );

/// Number of vertices written by nbChunk_BuildMesh.
NB_API int nbChunk_GetMeshVertexCount( nbChunkId chunkId );

/// Build a flat shaded triangle list of the chunk: three vertices per triangle.
/// @param uvScale texture coordinates per meter
/// @return the number of vertices written
NB_API int nbChunk_BuildMesh( nbChunkId chunkId, nbMeshVertex* vertices, int capacity, float uvScale );

/** @} */
