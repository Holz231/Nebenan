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

/// Start a pool of threadCount threads besides the calling one, for Nebenan and Box3D together. Hand it to Box3D with
/// b3WorldDef::enqueueTask = nbEnqueueTask, b3WorldDef::finishTask = nbFinishTask and b3WorldDef::userTaskContext set
/// to the pool, and to Nebenan with nbWorldDef::taskSystem. Then both share the cores instead of crowding them with
/// threads of their own, and the threads take the tasks of a step or an impact before destructibles that are prepared in
/// the background.
NB_API nbTaskSystem* nbCreateTaskSystem( int threadCount );

/// Stop the threads of a pool. The worlds that use it must be gone.
NB_API void nbDestroyTaskSystem( nbTaskSystem* taskSystem );

/// b3EnqueueTaskCallback of a pool from nbCreateTaskSystem, the user context is the pool
NB_API void* nbEnqueueTask( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName );

/// b3FinishTaskCallback of a pool from nbCreateTaskSystem, the user context is the pool
NB_API void nbFinishTask( void* userTask, void* userContext );

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

/// Change the moving debris budget, see nbWorldDef::maxDebrisBodies. It bounds the work of the Box3D step. The next
/// update freezes what is over budget into rubble.
NB_API void nbWorld_SetDebrisBudget( nbWorldId worldId, int maxDebrisBodies );

/// Change how much load the bonds carry, see nbWorldDef::supportScale. The next update checks every structure again.
/// Turning the check on measures the faces of the bonds of every structure without floors once, only the check needs
/// their shape.
NB_API void nbWorld_SetSupportScale( nbWorldId worldId, float scale );

/// Change the fraction of its walls a storey needs to stand, see nbWorldDef::storeySupport. The next update checks every
/// structure again.
NB_API void nbWorld_SetStoreySupport( nbWorldId worldId, float fraction );

/// Change the size of the fragments of all materials, see nbWorldDef::fragmentScale. Applies to the next impacts.
NB_API void nbWorld_SetFragmentScale( nbWorldId worldId, float scale );

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

/// Create many destructibles at once, defs[i] from the pieceCounts[i] pieces at pieceLists[i], and write their ids to ids.
/// The same as calling nbCreateDestructible for each of them in order, with the same ids and the same chunks and bonds to
/// the bit, but faster with workers: while the calling thread builds one destructible into the world, the workers
/// already compute the cells of the next. One without pieces gets a null id.
NB_API void nbCreateDestructibles( nbWorldId worldId, const nbDestructibleDef* defs, const nbPieceDef* const* pieceLists,
								   const int* pieceCounts, int count, nbDestructibleId* ids );

/// Start creating many destructibles like nbCreateDestructibles, but in the background: the workers draw the sites and
/// compute the cells, the shapes of the whole pieces and the contact areas, while the calling thread goes on, for
/// example with the physics step. Nothing goes into the world before nbFinishCreating builds them in. The definitions,
/// the pieces and all they point to must stay alive until then. Without workers, all of it happens right here, and so
/// it does for every destructible beyond the 32 that the built-in threads can have in the background at a time. A world
/// that is destroyed with creations in flight waits for its workers and drops what they prepared.
NB_API nbCreationId nbStartCreating( nbWorldId worldId, const nbDestructibleDef* defs, const nbPieceDef* const* pieceLists,
									 const int* pieceCounts, int count );

/// Build the destructibles of nbStartCreating into the world and write their ids, in order. They get the ids, chunks and
/// bonds to the bit that nbCreateDestructibles would give them at this point, waiting for the workers if they are not
/// done. The time of this call decides the result, never the speed of the workers. One without pieces gets a null id.
NB_API void nbFinishCreating( nbCreationId creationId, nbDestructibleId* ids );

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

/// Is the destructible as it was created? Nothing damaged or broke a bond of it, and no chunk broke off or went away.
NB_API bool nbDestructible_IsIntact( nbDestructibleId destructibleId );

/// Can the destructible go now and come back later without anything noticing? That holds for a static destructible that
/// is intact, see nbDestructible_IsIntact, with nothing within margin meters of its bounds that could lie on it or hit it:
/// no debris, no rubble and no body that moves or can be moved. Static bodies, sensors, the standing parts of other
/// destructibles and shapes that do not collide with its chunks do not count. Created again from the same definition, it
/// has the same chunks and bonds to the bit, only with new ids.
/// This streams a large city: destroy the intact houses far from everything that happens with nbDestroyDestructible,
/// and create them again with nbCreateDestructibles before anything can come near them.
NB_API bool nbDestructible_CanUnload( nbDestructibleId destructibleId, float margin );

/// Whether destructibles can be saved with nbSaveDestructibles now, to go out of the world and come back later as they
/// were: each of them rests, every part of it stands or is rubble, its load check has run, nothing that moves or can be
/// moved lies within margin meters of its parts, and rubble lies on chunks of the set only, as no other rubble lies on
/// them. Destructibles whose rubble lies on each other go together.
NB_API bool nbCanSaveDestructibles( const nbDestructibleId* ids, int count, float margin );

/// Save destructibles at rest into a buffer: their chunks with their shapes, bonds, actors and rubble with what it lies on,
/// and the Box3D bodies of the rubble. The world does not change, so nbDestroyDestructible takes them out afterwards.
/// Returns the bytes it takes and writes them if they fit, so a call with a null buffer gives the size. Zero if one of
/// them does not rest or rubble links it to a destructible outside the set, see nbCanSaveDestructibles. The buffer loads
/// into the build that wrote it.
NB_API size_t nbSaveDestructibles( const nbDestructibleId* ids, int count, void* buffer, size_t capacity );

/// Bring destructibles saved with nbSaveDestructibles back into a world, with their chunks, bonds, actors and rubble to the
/// bit, under new ids that it writes in the order of the save. Their chunks are reported as created. Returns false and
/// changes nothing for a buffer of another build or one that does not hold what was saved.
NB_API bool nbLoadDestructibles( nbWorldId worldId, const void* buffer, size_t size, nbDestructibleId* ids );

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

/// Which faces of the chunk can be seen. A face that the interfaces of the chunk's bonds cover completely lies
/// inside the structure and is false. When the chunk loses a bond it shows up in nbEvents::exposedChunks.
/// @param visible one flag per face of nbChunk_GetGeometry
/// @return the number of flags written
NB_API int nbChunk_GetVisibleFaces( nbChunkId chunkId, bool* visible, int capacity );

/** @} */
