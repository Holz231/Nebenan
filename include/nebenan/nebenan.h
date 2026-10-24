// SPDX-License-Identifier: MIT
// Nebenan - polygonal real-time destruction for Box3D
//
// Nebenan breaks convex polyhedra into convex polyhedra. There are no voxels. Every chunk is a
// Box3D hull shape, every free piece of debris is a Box3D rigid body, and structures are held
// together by a support graph of bonds between touching chunk faces.
//
// Typical frame:
//
//   nbDrawPoses poses = nbWorld_GetDrawPoses( destructionWorld, nbWorld_GetStepFraction( destructionWorld ) );
//   nbEvents events = nbWorld_GetEvents( destructionWorld );
//   // move the render meshes of the bodies to their poses, update them from the events, apply impacts
//   nbWorld_Advance( destructionWorld, frameTime );
//   // draw every chunk at the pose of its body while the steps run on a thread of their own

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

/// The hardware threads of this machine, at least one.
NB_API int nbGetHardwareThreadCount( void );

/// The cores of the fastest kind on this machine, without their second hardware threads and without efficiency cores.
/// Zero if the system does not tell.
NB_API int nbGetPerformanceCoreCount( void );

/// How many threads Box3D and Nebenan run best with on this machine, for b3WorldDef::workerCount and
/// nbWorldDef::workerCount, and one less for nbCreateTaskSystem: one per performance core, at most 8. Second hardware
/// threads and efficiency cores add little to the solver or slow it down, and so do more threads than cores. With the
/// step on a thread of its own, see nbWorldDef::asyncStep, one hardware thread stays free for the thread that draws,
/// since the workers of a step wait for their next task without sleeping.
NB_API int nbGetRecommendedWorkerCount( bool asyncStep );

/// Default world definition.
NB_API nbWorldDef nbDefaultWorldDef( void );

/// Create a destruction world for an existing Box3D world.
NB_API nbWorldId nbCreateWorld( const nbWorldDef* def );

/// Destroy a destruction world and all bodies and shapes it created in the Box3D world.
NB_API void nbDestroyWorld( nbWorldId worldId );

/// Is this world id valid?
NB_API bool nbWorld_IsValid( nbWorldId worldId );

/// Process collision damage and debris management. Call once after every b3World_Step, or let nbWorld_Step do both.
NB_API void nbWorld_Update( nbWorldId worldId, float timeStep );

/// Step the Box3D world with b3World_Step and update the destruction with nbWorld_Update. With nbWorldDef::asyncStep both
/// run on a thread of Nebenan and this returns right away, a step called meanwhile follows the one before. The calling
/// thread waits for them as soon as it calls a function of Nebenan or Box3D for the two worlds, so the program does all
/// of that before the step and draws while it runs. Other threads must leave the worlds alone until then.
NB_API void nbWorld_Step( nbWorldId worldId, float timeStep, int subStepCount );

/// How many steps of timeStep seconds a frame of frameTime seconds takes with nbWorld_Step, the time left over waits for
/// the next frame. When the steps cannot keep up, the simulation slows down instead of taking more and more steps per
/// frame, which would slow down every frame further: A frame counts at most 0.1 s and takes at most as many steps as
/// make 1/30 s, rounded, from one to eight, and as make 1/60 s unless the last step with its update took less than 4 ms
/// per 1/60 s. These are two steps or one at 60 Hz, four or two at 120 Hz and one at 30 Hz. Waits for the steps.
NB_API int nbWorld_PlanSteps( nbWorldId worldId, float frameTime, float timeStep );

/// The latest move of every body that moved in the steps of nbWorld_Step since the previous call, without the bodies that
/// went meanwhile. Waits for the steps, from any thread. The array stays valid until the next call.
NB_API b3BodyEvents nbWorld_GetBodyEvents( nbWorldId worldId );

/// Where to draw the bodies that move: between their poses before and after the last step of nbWorld_Step, fraction of
/// the way, so the picture moves smoothly when frames come more often than steps. With nbWorld_GetStepFraction the
/// picture lies one step behind the physics. The center of mass moves on a straight line and the body turns the short
/// way. Holds every body whose pose to draw changed since the previous call: the ones that moved in the last step, and
/// once more the ones that stopped since, at their last pose. Collects from the first call on, which has none. Waits for
/// the steps, from any thread. The array stays valid until the next call.
NB_API nbDrawPoses nbWorld_GetDrawPoses( nbWorldId worldId, float fraction );

/// How far the time that nbWorld_PlanSteps left over for the next step reaches into it, from 0 to 1, the fraction for
/// nbWorld_GetDrawPoses. 1 until the first call of nbWorld_PlanSteps.
NB_API float nbWorld_GetStepFraction( nbWorldId worldId );

/// Take the steps that a frame of frameTime seconds needs at the step rate of the world, see nbWorldDef::stepRate: plans
/// them with nbWorld_PlanSteps and starts each with nbWorld_Step and nbGetSubStepCount substeps. Returns how many it
/// started.
NB_API int nbWorld_Advance( nbWorldId worldId, float frameTime );

/// Change the steps per second of nbWorld_Advance from the next call on, see nbWorldDef::stepRate. Call it on the thread
/// that plans the steps. It does not wait for the steps.
NB_API void nbWorld_SetStepRate( nbWorldId worldId, float stepRate );

/// The steps per second of nbWorld_Advance, see nbWorldDef::stepRate. Does not wait for the steps.
NB_API float nbWorld_GetStepRate( nbWorldId worldId );

/// The substeps of a step at stepRate steps per second that make 240 substeps per second: 8 at 30 Hz, 4 at 60 Hz and 2
/// at 120 Hz, at least one. For a program that steps with nbWorld_Step or b3World_Step on its own.
NB_API int nbGetSubStepCount( float stepRate );

/// Run fcn( context ) after the steps that the calling thread started with nbWorld_Step, on the thread that takes them and
/// before the steps that follow, or right here if none runs. Like a step, fcn may use the two worlds while the calling
/// thread goes on, so that thread must leave alone what fcn touches until it waits for the steps. fcn must not call the
/// functions that wait for the steps from every thread: nbWorld_Step, nbWorld_RunAfterStep, nbWorld_GetBodyEvents,
/// nbWorld_SetAsyncStep, nbWorld_SetWorkerCount and nbDestroyWorld.
NB_API void nbWorld_RunAfterStep( nbWorldId worldId, nbAfterStepFcn* fcn, void* context );

/// Turn the thread of nbWorld_Step on or off, see nbWorldDef::asyncStep. Waits for the steps.
NB_API void nbWorld_SetAsyncStep( nbWorldId worldId, bool enabled );

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

/// Where the time of the steps went since the world was created or since nbWorld_ResetProfile, per step: Box3D's stages
/// and the parts of them that run on the calling thread alone, the update and its parts, the impacts, the share of the
/// largest island and how long the workers take to start. Waits for the steps.
NB_API nbProfile nbWorld_GetProfile( nbWorldId worldId );

/// Start the profile anew, for example once a scene is loaded. Waits for the steps.
NB_API void nbWorld_ResetProfile( nbWorldId worldId );

/// The worker counts to measure the scaling with on this machine: 1, 2, 4 and so on below the recommended count of
/// nbGetRecommendedWorkerCount without nbWorldDef::asyncStep, and that count itself. Returns how many, at most capacity.
NB_API int nbGetScalingWorkerCounts( int* counts, int capacity );

/// Fit T(n) = serial + parallel / n to every stage of profiles of the same work with different numbers of workers, by
/// least squares with both parts at least zero. With a single number of workers all time counts as serial.
NB_API nbScaling nbFitScaling( const nbProfile* profiles, int count );

/// Milliseconds per step of a stage with this many workers, from the fit.
NB_API float nbScaling_Project( const nbScaling* scaling, nbProfileStage stage, int workerCount );

/// The name of a stage of nbProfile, in English, for logs and tables.
NB_API const char* nbGetProfileStageName( nbProfileStage stage );

/// Write the profiles of the same work with different numbers of workers as a table into text: every stage per number
/// of workers, its fit and its projection to 8 and 16 workers, then the parts on the calling thread alone, the largest
/// island and the start of the workers. Writes at most capacity bytes with the terminating zero and returns the length
/// of the whole table, like snprintf.
NB_API int nbFormatScalingReport( const nbProfile* profiles, int count, char* text, int capacity );

/// Change the number of fracture workers, see nbWorldDef::workerCount. Restarts the internal threads.
NB_API void nbWorld_SetWorkerCount( nbWorldId worldId, int count );

/// Change the moving debris budget, see nbWorldDef::maxDebrisBodies. It bounds the work of the Box3D step. The next
/// update freezes what is over budget into rubble.
NB_API void nbWorld_SetDebrisBudget( nbWorldId worldId, int maxDebrisBodies );

/// Change how long debris must keep still before it counts as at rest, see nbWorldDef::debrisRestTime. Applies to the
/// next update.
NB_API void nbWorld_SetDebrisRestTime( nbWorldId worldId, float seconds );

/// Change how much load the bonds carry, see nbWorldDef::supportScale. The next update checks every structure again.
/// Turning the check on measures the faces of the bonds of every structure without floors once, only the check needs
/// their shape.
NB_API void nbWorld_SetSupportScale( nbWorldId worldId, float scale );

/// Change how the storeys of a building give way and the top storey, see nbWorldDef::storeyCollapse and
/// nbWorldDef::topStoreyCollapse. Every storey keeps where it lies in the ranges. The next update checks every structure
/// again.
NB_API void nbWorld_SetStoreyCollapse( nbWorldId worldId, nbStoreyCollapse storeys, nbStoreyCollapse topStorey );

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

/**
 * @defgroup stream Stream
 * A world too large to hold at once, like a city of tens of thousands of houses, keeps only the destructibles near its
 * viewers and near anything that moves. The stream decides which ones and when, the program says what stands where.
 * @{
 */

/// Default stream definition: sites within 100 m of a viewer load, two per update, and go beyond 120 m once no body moved
/// within 20 m of them for five seconds, four per update, of those one damaged one with its rubble. A moving body loads the
/// sites within 10 m of it.
NB_API nbStreamDef nbDefaultStreamDef( void );

/// Create a stream for a world. Its sites start out of the world, see nbStream_AddSite.
NB_API nbStreamId nbCreateStream( nbWorldId worldId, const nbStreamDef* def );

/// Destroy a stream. Its sites in the world stay as destructibles of their own, a load in the background is built in
/// first, and the damaged sites it kept go. The world destroys its streams with it.
NB_API void nbDestroyStream( nbStreamId streamId );

/// Add a site: a place where the destructible that nbStreamDef::prepareFcn defines for it stands, with bounds that hold
/// it as it is built. Returns its number, counting from zero. It loads once a viewer or a moving body comes near.
NB_API int nbStream_AddSite( nbStreamId streamId, b3AABB bounds );

/// Load every site within the load radius of the viewers at once, with nbCreateDestructibles, for the start of a level.
NB_API void nbStream_LoadAround( nbStreamId streamId, const b3Vec3* viewers, int viewerCount );

/// Stream around the viewers, once per frame: the sites that can go go, the ones the workers prepared come into the
/// world, and the next ones start, the ones a moving body asked for first, then the nearest. Every step marks the sites
/// near the bodies that moved. With nbWorldDef::asyncStep after nbWorld_Step this runs on the thread of the steps, after
/// them, see nbWorld_RunAfterStep, and the viewers are copied.
NB_API void nbStream_Update( nbStreamId streamId, const b3Vec3* viewers, int viewerCount );

/// Change the load radius, see nbStreamDef::loadRadius. Applies to the next update.
NB_API void nbStream_SetLoadRadius( nbStreamId streamId, float radius );

/// The destructible of a site, null while it is not in the world.
NB_API nbDestructibleId nbStream_GetDestructible( nbStreamId streamId, int site );

/// Get counters and timings.
NB_API nbStreamStats nbStream_GetStats( nbStreamId streamId );

/** @} */

/**
 * @defgroup articulation Articulation
 * Figures of Box3D bodies and joints that follow a pose, see the types. The same Box3D world simulates them with the
 * debris, their bones hit walls like cannonballs do, and every step the update samples the clip of every figure and sets
 * the targets of its joints for the next step.
 * @{
 */

/// Default bone: a capsule of 1 kg along y with a ball joint to its parent.
NB_API nbBoneDef nbDefaultBoneDef( void );

/// Default rig definition.
NB_API nbRigDef nbDefaultRigDef( void );

/// A profile that holds a standing pose: joints of 800 N m per radian with 150 N m times the strength of the bone, the
/// root held upright with 1500 N m per radian up to 250 N m.
NB_API nbMotorProfile nbDefaultMotorProfile( void );

/// A passive ragdoll: no motors, no upright hold.
NB_API nbMotorProfile nbPassiveMotorProfile( void );

/// Write the bones of a humanoid of this height in meters and mass in kilograms, nb_humanoidBoneCount of them in the
/// order of nbHumanoidBone: a pelvis as the root, a torso and a head on ball joints, arms and legs that hang down with
/// ball joints at the shoulders, hips, wrists and ankles and hinges at the elbows and knees, feet as boxes. It stands
/// with its y axis up and faces +z. Returns the height of the pelvis origin over the soles, so an articulation created
/// at that height stands on the ground at y = 0. The names of the bones are static strings.
NB_API float nbMakeHumanoidBones( float height, float mass, nbBoneDef* bones, int capacity );

/// Create a figure from a rig at the rest pose, the root at this position with this rotation, with these motors or
/// passive for null. The bones are dynamic Box3D bodies in the world of the destruction world, the joints are Box3D
/// joints, and the rig can go afterwards.
NB_API nbArticulationId nbCreateArticulation( nbWorldId worldId, const nbRigDef* rig, b3Pos position, b3Quat rotation,
											  const nbMotorProfile* profile );

/// Destroy a figure with its bodies and joints.
NB_API void nbDestroyArticulation( nbArticulationId articulationId );

/// Is this articulation id valid?
NB_API bool nbArticulation_IsValid( nbArticulationId articulationId );

/// The bones of the figure.
NB_API int nbArticulation_GetBoneCount( nbArticulationId articulationId );

/// The Box3D body of a bone.
NB_API b3BodyId nbArticulation_GetBody( nbArticulationId articulationId, int bone );

/// The Box3D joint of a bone to its parent, null for the root.
NB_API b3JointId nbArticulation_GetJoint( nbArticulationId articulationId, int bone );

/// Change the motors, for example from standing to a soft figure that takes a hit or to a passive ragdoll. Wakes the
/// figure.
NB_API void nbArticulation_SetMotorProfile( nbArticulationId articulationId, const nbMotorProfile* profile );

/// The motors of the figure.
NB_API nbMotorProfile nbArticulation_GetMotorProfile( nbArticulationId articulationId );

/// Set the pose the joints drive to: the rotation of every bone against its rest pose, in the frame of the bone, one per
/// bone. The root is not driven. Stops a clip that plays. The motors reach the pose as far as their torque, the limits
/// and the world let them, nothing is teleported.
NB_API void nbArticulation_SetTargetPose( nbArticulationId articulationId, const b3Quat* rotations, int count );

/// Play a clip from its start, at this speed, 1 for real time. The clip is copied. Every update samples it and sets the
/// targets of the joints, see nbWorld_Update. Null stops the clip, the joints keep their last targets.
NB_API void nbArticulation_PlayClip( nbArticulationId articulationId, const nbClipDef* clip, float speed );

/// Seconds into the clip that plays or played last.
NB_API float nbArticulation_GetClipTime( nbArticulationId articulationId );

/// Is a clip playing? A clip that does not loop stops at its end.
NB_API bool nbArticulation_IsClipPlaying( nbArticulationId articulationId );

/// Apply a hit: one impulse on a bone, once per hit id, see nbHitDef. The physics takes it from there, the motors give
/// way as far as their torque limits make them. Wakes the figure.
/// @return true if the hit landed, false if a hit with this id landed before or the bone does not exist
NB_API bool nbArticulation_ApplyHit( nbArticulationId articulationId, const nbHitDef* hit );

/// How many hits landed.
NB_API int nbArticulation_GetHitCount( nbArticulationId articulationId );

/// The pose of the figure as the physics has it: the world transform of every bone, up to capacity.
/// @return the number of transforms written
NB_API int nbArticulation_GetBoneTransforms( nbArticulationId articulationId, b3WorldTransform* transforms, int capacity );

/// The pose the joints drive to, placed on the root as the physics has it: the world transform of every bone at its
/// target rotation, up to capacity. Drawn over the bones it shows how far the figure is from its pose.
/// @return the number of transforms written
NB_API int nbArticulation_GetTargetTransforms( nbArticulationId articulationId, b3WorldTransform* transforms, int capacity );

/// How far the joint of a bone is from its target, in radians. Zero for the root and fixed bones.
NB_API float nbArticulation_GetJointError( nbArticulationId articulationId, int bone );

/// How hard the motor of a bone works: the torque of its spring over its limit, 1 when it is at the limit and cannot
/// pull the bone any closer to its target. Zero without motors.
NB_API float nbArticulation_GetMotorLoad( nbArticulationId articulationId, int bone );

/// Sample a clip at a time: the rotations of its bones, interpolated between the two keyframes around the time, up to
/// capacity. Past the end the last pose, or around again for a clip that loops.
NB_API void nbSampleClip( const nbClipDef* clip, float time, b3Quat* rotations, int capacity );

/// The figure a Box3D body belongs to and which bone it is, for example the body a ray hit. Null for a body of no
/// figure.
NB_API nbArticulationId nbWorld_GetArticulationFromBody( nbWorldId worldId, b3BodyId bodyId, int* bone );

/** @} */
