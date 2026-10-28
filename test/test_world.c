// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestScene
{
	b3WorldId physicsWorld;
	nbWorldId world;
	b3BodyId groundId;
} TestScene;

static TestScene CreateSceneWithWorkers( int workerCount, b3EnqueueTaskCallback* enqueueTask, b3FinishTaskCallback* finishTask,
										  void* taskContext )
{
	TestScene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	scene.physicsWorld = b3CreateWorld( &worldDef );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
	scene.groundId = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( scene.groundId, &shapeDef, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.workerCount = workerCount;
	def.enqueueTask = enqueueTask;
	def.finishTask = finishTask;
	def.userTaskContext = taskContext;
	scene.world = nbCreateWorld( &def );
	return scene;
}

static TestScene CreateScene( void )
{
	return CreateSceneWithWorkers( 1, NULL, NULL, NULL );
}

// A scene whose Box3D world and destruction world share one pool of threads, see nbCreateTaskSystem
static TestScene CreateSceneWithPool( nbTaskSystem* pool, int workerCount )
{
	TestScene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	worldDef.enqueueTask = nbEnqueueTask;
	worldDef.finishTask = nbFinishTask;
	worldDef.userTaskContext = pool;
	scene.physicsWorld = b3CreateWorld( &worldDef );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
	scene.groundId = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( scene.groundId, &shapeDef, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.workerCount = workerCount;
	def.taskSystem = pool;
	scene.world = nbCreateWorld( &def );
	return scene;
}

static void DestroyScene( TestScene* scene )
{
	nbDestroyWorld( scene->world );
	b3DestroyWorld( scene->physicsWorld );
}

static nbDestructibleId CreateWall( TestScene* scene, b3Vec3 halfExtents, uint32_t seed )
{
	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.position = (b3Vec3){ 0.0f, halfExtents.y, 0.0f };
	def.seed = seed;
	return nbCreateBox( scene->world, &def, halfExtents );
}

static float TotalChunkVolume( nbDestructibleId wall )
{
	int count = nbDestructible_GetChunkCount( wall );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)( count + 1 ) );
	int written = nbDestructible_GetChunks( wall, chunks, count );
	float volume = 0.0f;
	for ( int i = 0; i < written; ++i )
	{
		volume += nbChunk_GetVolume( chunks[i] );
	}
	free( chunks );
	return volume;
}

// Two stout pillars and a beam across, pre-fractured into cells
static nbDestructibleId CreateGate( TestScene* scene, float x, uint32_t seed )
{
	nbPieceDef pieces[3];
	for ( int i = 0; i < 3; ++i )
	{
		pieces[i] = nbDefaultPieceDef();
	}
	pieces[0].halfExtents = (b3Vec3){ 0.6f, 1.5f, 0.6f };
	pieces[0].transform.p = (b3Vec3){ x - 3.0f, 1.5f, 0.0f };
	pieces[1].halfExtents = (b3Vec3){ 0.6f, 1.5f, 0.6f };
	pieces[1].transform.p = (b3Vec3){ x + 3.0f, 1.5f, 0.0f };
	pieces[2].halfExtents = (b3Vec3){ 4.0f, 0.3f, 0.3f };
	pieces[2].transform.p = (b3Vec3){ x, 3.3f, 0.0f };

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.cellSize = 1.0f;
	def.seed = seed;
	return nbCreateDestructible( scene->world, &def, pieces, 3 );
}

static void Step( TestScene* scene, int count )
{
	for ( int i = 0; i < count; ++i )
	{
		b3World_Step( scene->physicsWorld, 1.0f / 60.0f, 4 );
		nbWorld_Update( scene->world, 1.0f / 60.0f );
	}
}

static int CreateTest( void )
{
	int64_t baseBytes = nbGetByteCount();
	TestScene scene = CreateScene();

	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 3.0f, 1.5f, 0.12f }, 1 );
	ENSURE( nbDestructible_IsValid( wall ) );
	ENSURE( nbDestructible_GetChunkCount( wall ) == 1 );

	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.chunkCount == 1 );
	ENSURE( stats.staticBodyCount == 1 );
	ENSURE( stats.dynamicBodyCount == 0 );

	nbEvents events = nbWorld_GetEvents( scene.world );
	ENSURE( events.createdCount == 1 );
	ENSURE( nbChunk_IsValid( events.createdChunks[0] ) );
	ENSURE( nbChunk_IsAnchored( events.createdChunks[0] ) );
	ENSURE( nbChunk_IsDynamic( events.createdChunks[0] ) == false );

	b3ShapeId shapeId = nbChunk_GetShape( events.createdChunks[0] );
	ENSURE( b3Shape_IsValid( shapeId ) );
	nbChunkId found = nbWorld_GetChunkFromShape( scene.world, shapeId );
	ENSURE( NB_ID_EQUALS( found, events.createdChunks[0] ) );

	nbDestroyDestructible( wall );
	ENSURE( nbDestructible_IsValid( wall ) == false );
	ENSURE( b3Shape_IsValid( shapeId ) == false );

	events = nbWorld_GetEvents( scene.world );
	ENSURE( events.createdCount == 0 );
	ENSURE( events.destroyedCount == 1 );

	DestroyScene( &scene );
	ENSURE( nbGetByteCount() == baseBytes );
	return 0;
}

static int ImpactTest( void )
{
	TestScene scene = CreateScene();
	b3Vec3 halfExtents = { 3.0f, 1.5f, 0.12f };
	nbDestructibleId wall = CreateWall( &scene, halfExtents, 7 );
	float wallVolume = 8.0f * halfExtents.x * halfExtents.y * halfExtents.z;

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.3f, 1.4f, halfExtents.z };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.5f;
	impact.damage = 6.0e4f;
	impact.ejectSpeed = 8.0f;

	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.fracturedChunkCount == 1 );
	ENSURE( result.createdChunkCount > 20 );
	ENSURE( result.brokenBondCount > 10 );
	ENSURE( result.detachedChunkCount > 5 );
	ENSURE( result.createdBodyCount > 5 );

	// Fracture conserves volume up to the dust that was dropped
	float volume = TotalChunkVolume( wall );
	ENSURE( volume <= wallVolume * 1.0001f );
	ENSURE( volume >= wallVolume * 0.995f );

	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.staticBodyCount == 1 );
	ENSURE( stats.dynamicBodyCount == result.createdBodyCount );

	// The fresh debris flies and lands on the ground
	Step( &scene, 240 );

	int count = nbDestructible_GetChunkCount( wall );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( wall, chunks, count );
	int dynamicCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		if ( nbChunk_IsDynamic( chunks[i] ) == false )
		{
			continue;
		}
		dynamicCount += 1;

		// Rubble sits on a static body, whose center Box3D puts at its origin
		b3BodyId bodyId = nbChunk_GetBody( chunks[i] );
		b3Pos center = b3TransformWorldPoint( b3Body_GetTransform( bodyId ), nbChunk_GetCentroid( chunks[i] ) );
		ENSURE( center.y > -0.05f );
		ENSURE( center.y < 3.0f );
	}
	free( chunks );
	ENSURE( dynamicCount > 5 );

	// A second hit in the same spot only damages the small chunks that are already there
	impact.point = (b3Vec3){ 0.3f, 1.4f, 0.0f };
	result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.totalTime >= 0.0f );

	DestroyScene( &scene );
	return 0;
}

// Debris whose center sinks below the kill depth leaves the world. The update passes over bodies whose origin lies far
// enough above the depth. Without ground the debris of a wall falls through it, except what freezes in the crater.
static int KillDepthTest( void )
{
	TestScene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	scene.physicsWorld = b3CreateWorld( &worldDef );
	scene.groundId = b3_nullBodyId;

	float killDepth = -2.0f;
	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.killDepth = killDepth;
	scene.world = nbCreateWorld( &def );

	b3Vec3 halfExtents = { 3.0f, 1.5f, 0.12f };
	nbDestructibleId wall = CreateWall( &scene, halfExtents, 7 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.3f, 1.4f, halfExtents.z };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.5f;
	impact.damage = 6.0e4f;
	impact.ejectSpeed = 8.0f;
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.createdBodyCount > 5 );

	int capacity = nbDestructible_GetChunkCount( wall ) + 1;
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)capacity );
	for ( int frame = 0; frame < 240; ++frame )
	{
		Step( &scene, 1 );

		// Every body that moved in the step is in Box3D's move events, none of them may stay below the depth
		int count = nbDestructible_GetChunks( wall, chunks, capacity );
		for ( int i = 0; i < count; ++i )
		{
			if ( nbChunk_IsDynamic( chunks[i] ) )
			{
				ENSURE( b3Body_GetWorldCenter( nbChunk_GetBody( chunks[i] ) ).y >= killDepth );
			}
		}
	}
	free( chunks );

	// Debris fell through the depth and is gone
	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.debrisCount < result.createdBodyCount );

	DestroyScene( &scene );
	return 0;
}

static int CastImpactTest( void )
{
	TestScene scene = CreateScene();
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 2.0f, 1.5f, 0.12f }, 3 );

	nbImpactDef impact = { 0 };
	impact.radius = 0.4f;
	impact.damage = 5.0e4f;
	impact.ejectSpeed = 10.0f;

	nbImpactResult result;
	bool hit = nbWorld_CastImpact( scene.world, (b3Vec3){ 0.0f, 1.2f, 5.0f }, (b3Vec3){ 0.0f, 0.0f, -10.0f }, &impact, &result );
	ENSURE( hit );
	ENSURE( result.createdChunkCount > 0 );
	ENSURE( result.detachedChunkCount > 0 );

	// A ray that misses
	hit = nbWorld_CastImpact( scene.world, (b3Vec3){ 10.0f, 1.2f, 5.0f }, (b3Vec3){ 0.0f, 0.0f, -10.0f }, &impact, &result );
	ENSURE( hit == false );

	ENSURE( nbDestructible_GetChunkCount( wall ) > 1 );
	DestroyScene( &scene );
	return 0;
}

// Cut a wall near its base. The part above the cut loses its anchor and falls.
static int CollapseTest( void )
{
	TestScene scene = CreateScene();
	b3Vec3 halfExtents = { 1.0f, 1.5f, 0.1f };
	nbDestructibleId wall = CreateWall( &scene, halfExtents, 11 );

	nbImpactDef impact = { 0 };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.3f;
	impact.damage = 1.0e6f;
	impact.ejectSpeed = 4.0f;

	for ( int i = 0; i < 9; ++i )
	{
		impact.point = (b3Vec3){ -1.0f + 0.25f * (float)i, 0.5f, 0.0f };
		nbWorld_ApplyImpact( scene.world, &impact );
	}

	// Find the largest dynamic body
	int count = nbDestructible_GetChunkCount( wall );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( wall, chunks, count );

	b3BodyId heaviest = b3_nullBodyId;
	float heaviestMass = 0.0f;
	for ( int i = 0; i < count; ++i )
	{
		if ( nbChunk_IsDynamic( chunks[i] ) )
		{
			b3BodyId bodyId = nbChunk_GetBody( chunks[i] );
			float mass = b3Body_GetMass( bodyId );
			if ( mass > heaviestMass )
			{
				heaviestMass = mass;
				heaviest = bodyId;
			}
		}
	}
	free( chunks );

	// The upper part of the wall weighs more than a ton
	ENSURE( B3_IS_NON_NULL( heaviest ) );
	ENSURE( heaviestMass > 1000.0f );

	// The upper part now only rests on the jagged cut. It is no longer glued, so a shove topples it.
	b3Pos before = b3Body_GetWorldCenter( heaviest );
	ENSURE( before.y > 1.2f );
	b3Pos top = { before.x, before.y + 1.0f, before.z };
	b3Body_ApplyLinearImpulse( heaviest, (b3Vec3){ 0.0f, 0.0f, -0.8f * heaviestMass }, top, true );
	Step( &scene, 150 );
	b3Pos after = b3Body_GetWorldCenter( heaviest );
	ENSURE( after.y < before.y - 0.4f );

	DestroyScene( &scene );
	return 0;
}

static uint32_t HashDestructible( uint32_t hash, nbDestructibleId wall )
{
	int count = nbDestructible_GetChunkCount( wall );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( wall, chunks, count );
	for ( int i = 0; i < count; ++i )
	{
		b3BodyId bodyId = nbChunk_GetBody( chunks[i] );
		b3WorldTransform transform = b3Body_GetTransform( bodyId );
		float values[8] = { transform.p.x, transform.p.y, transform.p.z, transform.q.v.x,
							transform.q.v.y, transform.q.v.z, transform.q.s, nbChunk_GetVolume( chunks[i] ) };
		const uint8_t* bytes = (const uint8_t*)values;
		for ( size_t k = 0; k < sizeof( values ); ++k )
		{
			hash = ( hash ^ bytes[k] ) * 16777619u;
		}
	}
	free( chunks );
	return ( hash ^ (uint32_t)count ) * 16777619u;
}

// Every bond of the world, its chunks, geometry and state
static uint32_t HashBonds( uint32_t hash, const nbWorld* world )
{
	int count = 0;
	for ( int i = 0; i < world->bonds.count; ++i )
	{
		const nbBond* bond = world->bonds.data + i;
		if ( bond->chunk[0] == NB_NULL_INDEX )
		{
			continue;
		}

		count += 1;
		float values[9] = {
			bond->centroid.x, bond->centroid.y, bond->centroid.z, bond->area,
			bond->normal.x,	  bond->normal.y,	bond->normal.z,	  bond->health,
			(float)( bond->cohesive + 2 * bond->sibling ),
		};
		int chunks[2] = { bond->chunk[0], bond->chunk[1] };
		const uint8_t* bytes = (const uint8_t*)values;
		for ( size_t k = 0; k < sizeof( values ); ++k )
		{
			hash = ( hash ^ bytes[k] ) * 16777619u;
		}
		bytes = (const uint8_t*)chunks;
		for ( size_t k = 0; k < sizeof( chunks ); ++k )
		{
			hash = ( hash ^ bytes[k] ) * 16777619u;
		}
	}
	return ( hash ^ (uint32_t)count ) * 16777619u;
}

static uint32_t RunDeterminismScenarioWith( TestScene scene )
{
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 2.5f, 1.5f, 0.12f }, 5 );

	// A gate whose pillar is blasted, the beam falls with the pieces that lose their support
	nbDestructibleId gate = CreateGate( &scene, 10.0f, 6 );

	nbImpactDef impact = { 0 };
	impact.direction = (b3Vec3){ 0.2f, 0.0f, -1.0f };
	impact.radius = 0.45f;
	impact.damage = 8.0e4f;
	impact.ejectSpeed = 9.0f;

	for ( int frame = 0; frame < 120; ++frame )
	{
		if ( frame % 20 == 0 )
		{
			impact.point = (b3Vec3){ -1.5f + 0.5f * (float)( frame / 20 ), 0.6f + 0.2f * (float)( frame / 20 ), 0.12f };
			nbWorld_ApplyImpact( scene.world, &impact );
		}
		if ( frame == 10 )
		{
			nbImpactDef blast = { 0 };
			blast.point = (b3Vec3){ 13.0f, 2.3f, 0.0f };
			blast.radius = 1.0f;
			blast.damage = 1.0e8f;
			blast.ejectSpeed = 2.0f;
			nbWorld_ApplyImpact( scene.world, &blast );
		}
		Step( &scene, 1 );
	}

	nbStats stats = nbWorld_GetStats( scene.world );
	uint32_t hash = HashDestructible( 2166136261u, wall );
	hash = HashDestructible( hash, gate );
	hash = ( hash ^ (uint32_t)stats.bondCount ) * 16777619u;
	DestroyScene( &scene );
	return hash;
}

static uint32_t RunDeterminismScenario( void )
{
	return RunDeterminismScenarioWith( CreateScene() );
}

// Fracture and physics are bit for bit identical with MSVC, GCC and Clang on x64 and ARM. This is the result
// with the pinned Box3D commit. Update it when the results change on purpose, never to make one platform pass.
#define NB_EXPECTED_DETERMINISM_HASH 0xdd0ef4c0u

static int DeterminismTest( void )
{
	uint32_t hash1 = RunDeterminismScenario();
	uint32_t hash2 = RunDeterminismScenario();
	printf( "determinism hash: 0x%08x\n", hash1 );
	ENSURE( hash1 == hash2 );
	ENSURE( hash1 == NB_EXPECTED_DETERMINISM_HASH );
	return 0;
}

// A shape with a unique hull keeps a copy of its own, other shapes share equal hulls through Box3D's hull database. A
// new hull for a unique shape is shared again, and destroying the shapes frees all copies.
static int UniqueHullTest( void )
{
#if defined( B3_HAS_UNIQUE_HULLS )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 1.0f, 1.0f, 1.0f );

	b3ShapeDef sharedDef = b3DefaultShapeDef();
	b3ShapeId sharedA = b3CreateHullShape( bodyId, &sharedDef, &box.base );
	b3ShapeId sharedB = b3CreateHullShape( bodyId, &sharedDef, &box.base );
	ENSURE( b3Shape_GetHull( sharedA ) == b3Shape_GetHull( sharedB ) );

	b3ShapeDef uniqueDef = b3DefaultShapeDef();
	uniqueDef.uniqueHull = true;
	b3ShapeId uniqueA = b3CreateHullShape( bodyId, &uniqueDef, &box.base );
	b3ShapeId uniqueB = b3CreateHullShape( bodyId, &uniqueDef, b3Shape_GetHull( uniqueA ) );
	ENSURE( b3Shape_GetHull( uniqueA ) != b3Shape_GetHull( sharedA ) );
	ENSURE( b3Shape_GetHull( uniqueB ) != b3Shape_GetHull( uniqueA ) );
	ENSURE( memcmp( b3Shape_GetHull( uniqueB ), &box.base, (size_t)box.base.byteCount ) == 0 );

	b3Shape_SetHull( uniqueB, &box.base );
	ENSURE( b3Shape_GetHull( uniqueB ) == b3Shape_GetHull( sharedA ) );

	b3DestroyShape( uniqueA, false );
	b3DestroyShape( sharedB, false );
	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );
#endif
	return 0;
}

#if defined( B3_HAS_EXTERNAL_HULLS )
// Every chunk gives its Box3D shape the hull built into its own shape, whose points and planes are the vertices and planes
// of the chunk. Counts the chunks on the static body of their destructible and on the bodies of their actors.
static bool CheckChunkHulls( const nbWorld* world, int* staticCount, int* actorCount )
{
	*staticCount = 0;
	*actorCount = 0;
	for ( int i = 0; i < world->chunks.count; ++i )
	{
		const nbChunk* chunk = world->chunks.data + i;
		if ( chunk->shape == NULL || B3_IS_NULL( chunk->shapeId ) )
		{
			continue;
		}

		const nbShape* shape = chunk->shape;
		const b3HullData* hull = b3Shape_GetHull( chunk->shapeId );
		if ( hull != shape->hull )
		{
			return false;
		}

		// Hulls of the direct builder lie in the shape, quickhull gives the rare others an allocation of their own
		const uint8_t* bytes = (const uint8_t*)hull;
		nbShapeLayout layout = nbGetShapeLayout( shape->vertexCount, shape->faceCount, shape->indexCount );
		if ( bytes == (const uint8_t*)shape + layout.hullOffset )
		{
			if ( (const b3Vec3*)( bytes + hull->pointOffset ) != shape->vertices ||
				 (const b3Plane*)( bytes + hull->planeOffset ) != shape->planes )
			{
				return false;
			}
		}

		if ( chunk->flags & nb_chunkStaticBody )
		{
			*staticCount += 1;
		}
		else
		{
			*actorCount += 1;
		}
	}
	return true;
}
#endif

// A shape with an external hull uses it in place and never frees it. A transformed shape gets a copy of its own, a new
// hull through b3Shape_SetHull is shared again. The chunks of a destructible give their Box3D shapes the hulls built into
// their own shapes, on the shared static body, on flying debris and on rubble at rest.
static int ExternalHullTest( void )
{
#if defined( B3_HAS_EXTERNAL_HULLS )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 1.0f, 1.0f, 1.0f );

	b3ShapeDef externalDef = b3DefaultShapeDef();
	externalDef.externalHull = true;
	b3ShapeId shapeA = b3CreateHullShape( bodyId, &externalDef, &box.base );
	b3ShapeId shapeB = b3CreateHullShape( bodyId, &externalDef, &box.base );
	ENSURE( b3Shape_GetHull( shapeA ) == &box.base );
	ENSURE( b3Shape_GetHull( shapeB ) == &box.base );

	b3ShapeId stretched =
		b3CreateTransformedHullShape( bodyId, &externalDef, &box.base, b3Transform_identity, (b3Vec3){ 2.0f, 1.0f, 1.0f } );
	ENSURE( b3Shape_GetHull( stretched ) != &box.base );

	b3DestroyShape( shapeA, false );
	ENSURE( b3Shape_GetHull( shapeB ) == &box.base );
	b3Shape_SetHull( shapeB, &box.base );
	ENSURE( b3Shape_GetHull( shapeB ) != &box.base );
	ENSURE( memcmp( b3Shape_GetHull( shapeB ), &box.base, (size_t)box.base.byteCount ) == 0 );
	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );

	int64_t baseNebenanBytes = nbGetByteCount();
	TestScene scene = CreateScene();
	CreateGate( &scene, 0.0f, 21 );
	const nbWorld* world = nbGetWorldFromId( scene.world );
	int staticCount, actorCount;
	ENSURE( CheckChunkHulls( world, &staticCount, &actorCount ) );
	ENSURE( staticCount > 8 && actorCount == 0 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ -3.0f, 1.5f, 0.6f };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.5f;
	impact.damage = 6.0e4f;
	impact.ejectSpeed = 8.0f;
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.createdBodyCount > 0 );
	ENSURE( CheckChunkHulls( world, &staticCount, &actorCount ) );
	ENSURE( staticCount > 8 && actorCount > 0 );

	Step( &scene, 120 );
	ENSURE( nbWorld_GetStats( scene.world ).rubbleCount > 0 );
	ENSURE( CheckChunkHulls( world, &staticCount, &actorCount ) );
	ENSURE( staticCount > 8 && actorCount > 0 );

	DestroyScene( &scene );
	ENSURE( nbGetByteCount() == baseNebenanBytes );
	ENSURE( b3GetByteCount() == baseBytes );
#endif
	return 0;
}

typedef struct CountQuery
{
	int count;
} CountQuery;

static bool CountCallback( b3ShapeId shapeId, void* context )
{
	(void)shapeId;
	CountQuery* query = context;
	query->count += 1;
	return true;
}

// A pile of boxes over a floor that an explosion goes off in, built with or without a batch. Without, every shape goes
// into its tree with a search of its own, so the trees take another shape.
static b3WorldId CreateBatchPile( bool batch, b3BodyId* bodies, int bodyCount )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
#if defined( B3_HAS_SHAPE_BATCH )
	if ( batch )
	{
		b3World_BeginShapeBatch( worldId );
	}
#else
	(void)batch;
#endif

	b3BodyDef floorDef = b3DefaultBodyDef();
	floorDef.position = (b3Pos){ 0.0f, -0.5f, 0.0f };
	b3BoxHull floor = b3MakeBoxHull( 10.0f, 0.5f, 10.0f );
	b3CreateHullShape( b3CreateBody( worldId, &floorDef ), &shapeDef, &floor.base );

	// Crosses of four boxes, the impulses on a body add up over its shapes
	b3BoxHull arms[4] = {
		b3MakeOffsetBoxHull( 0.15f, 0.15f, 0.15f, (b3Vec3){ 0.3f, 0.0f, 0.0f } ),
		b3MakeOffsetBoxHull( 0.15f, 0.15f, 0.15f, (b3Vec3){ -0.3f, 0.0f, 0.0f } ),
		b3MakeOffsetBoxHull( 0.15f, 0.15f, 0.15f, (b3Vec3){ 0.0f, 0.0f, 0.3f } ),
		b3MakeOffsetBoxHull( 0.15f, 0.15f, 0.15f, (b3Vec3){ 0.0f, 0.0f, -0.3f } ),
	};
	for ( int i = 0; i < bodyCount; ++i )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = (b3Pos){ 1.1f * (float)( i % 4 ) + 0.01f * (float)( i % 3 ), 0.16f + 0.35f * (float)( i / 16 ),
									1.1f * (float)( ( i / 4 ) % 4 ) };
		bodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.05f * (float)( i % 8 ) );
		bodies[i] = b3CreateBody( worldId, &bodyDef );
		for ( int k = 0; k < 4; ++k )
		{
			b3CreateHullShape( bodies[i], &shapeDef, &arms[k].base );
		}
	}

#if defined( B3_HAS_SHAPE_BATCH )
	if ( batch )
	{
		b3World_EndShapeBatch( worldId );
	}
#endif
	return worldId;
}

// Shapes of a batch go into Box3D's trees together, static and moving ones. One that goes before the batch ends leaves
// it, one that moves goes in at its new place, and the shapes of nested batches wait for the outermost one. Queries find
// all the others, and boxes dropped on them come to rest there. A pile simulates the same to the bit whether its shapes
// went in together or one by one, explosion included: the results do not depend on the shape of the trees.
static int ShapeBatchTest( void )
{
#if defined( B3_HAS_SHAPE_BATCH )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BoxHull box = b3MakeBoxHull( 0.4f, 0.4f, 0.4f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();

	b3ShapeId shapes[64];
	b3World_BeginShapeBatch( worldId );
	for ( int i = 0; i < 64; ++i )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = (b3Pos){ (float)( i % 8 ), 0.4f, (float)( i / 8 ) };
		shapes[i] = b3CreateHullShape( b3CreateBody( worldId, &bodyDef ), &shapeDef, &box.base );
	}
	b3DestroyShape( shapes[9], false );
	b3Body_SetTransform( b3Shape_GetBody( shapes[18] ), (b3Pos){ 20.0f, 0.4f, 20.0f }, b3Quat_identity );
	b3World_EndShapeBatch( worldId );

	CountQuery field = { 0 };
	b3AABB fieldBox = { { -1.0f, -1.0f, -1.0f }, { 8.0f, 1.0f, 8.0f } };
	b3World_OverlapAABB( worldId, fieldBox, b3DefaultQueryFilter(), CountCallback, &field );
	ENSURE( field.count == 62 );

	CountQuery moved = { 0 };
	b3AABB movedBox = { { 19.0f, 0.0f, 19.0f }, { 21.0f, 1.0f, 21.0f } };
	b3World_OverlapAABB( worldId, movedBox, b3DefaultQueryFilter(), CountCallback, &moved );
	ENSURE( moved.count == 1 );

	// Boxes in the air in two nested batches, one of the field turns into a box that falls
	b3BodyId dropped[9];
	b3World_BeginShapeBatch( worldId );
	b3World_BeginShapeBatch( worldId );
	for ( int i = 0; i < 9; ++i )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = (b3Pos){ 3.0f * (float)( i % 3 ), 3.0f, 3.0f * (float)( i / 3 ) };
		dropped[i] = b3CreateBody( worldId, &bodyDef );
		b3CreateHullShape( dropped[i], &shapeDef, &box.base );
	}
	b3World_EndShapeBatch( worldId );
	b3DestroyBody( dropped[4] );
	b3Body_SetTransform( dropped[8], (b3Pos){ 5.0f, 5.0f, 5.0f }, b3Quat_identity );
	b3BodyId turned = b3Shape_GetBody( shapes[63] );
	b3Body_SetTransform( turned, (b3Pos){ 4.0f, 2.0f, 4.0f }, b3Quat_identity );
	b3Body_SetType( turned, b3_dynamicBody );
	b3World_EndShapeBatch( worldId );

	CountQuery above = { 0 };
	b3AABB aboveBox = { { -1.0f, 1.5f, -1.0f }, { 8.0f, 6.0f, 8.0f } };
	b3World_OverlapAABB( worldId, aboveBox, b3DefaultQueryFilter(), CountCallback, &above );
	ENSURE( above.count == 9 );

	for ( int i = 0; i < 120; ++i )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	for ( int i = 0; i < 9; ++i )
	{
		if ( i != 4 )
		{
			ENSURE( (float)b3Body_GetPosition( dropped[i] ).y > 1.0f );
		}
	}
	ENSURE( fabsf( (float)b3Body_GetPosition( turned ).y - 1.2f ) < 0.05f );
	b3DestroyWorld( worldId );

	b3BodyId together[48], alone[48];
	b3WorldId batchedId = CreateBatchPile( true, together, 48 );
	b3WorldId singleId = CreateBatchPile( false, alone, 48 );
	b3Pos starts[48];
	for ( int i = 0; i < 48; ++i )
	{
		starts[i] = b3Body_GetPosition( together[i] );
	}
	for ( int i = 0; i < 120; ++i )
	{
		if ( i == 0 )
		{
			b3ExplosionDef explosion = b3DefaultExplosionDef();
			explosion.position = (b3Pos){ 1.65f, 0.1f, 1.65f };
			explosion.radius = 1.5f;
			explosion.falloff = 1.5f;
			explosion.impulsePerArea = 2000.0f;
			b3World_Explode( batchedId, &explosion );
			b3World_Explode( singleId, &explosion );
		}
		b3World_Step( batchedId, 1.0f / 60.0f, 4 );
		b3World_Step( singleId, 1.0f / 60.0f, 4 );
	}
	float thrown = 0.0f;
	for ( int i = 0; i < 48; ++i )
	{
		b3WorldTransform a = b3Body_GetTransform( together[i] );
		b3WorldTransform b = b3Body_GetTransform( alone[i] );
		ENSURE( memcmp( &a, &b, sizeof( b3WorldTransform ) ) == 0 );
		thrown = b3MaxFloat( thrown, b3Length( b3SubPos( a.p, starts[i] ) ) );
	}
	ENSURE( thrown > 1.0f );
	b3DestroyWorld( batchedId );
	b3DestroyWorld( singleId );

	ENSURE( b3GetByteCount() == baseBytes );
#endif
	return 0;
}

// Every shape keeps a list of its own contacts. Destroying one shape of a static body with many takes only the contacts of
// that shape: the box on it falls, the box on another shape of the body stays where it rests.
static int ShapeContactListTest( void )
{
#if defined( B3_HAS_SHAPE_CONTACT_LISTS )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId floorId = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();

	b3ShapeId tiles[8];
	for ( int i = 0; i < 8; ++i )
	{
		b3BoxHull tile = b3MakeOffsetBoxHull( 0.5f, 0.5f, 0.5f, (b3Vec3){ (float)i, 0.0f, 0.0f } );
		tiles[i] = b3CreateHullShape( floorId, &shapeDef, &tile.base );
	}

	b3BoxHull box = b3MakeBoxHull( 0.25f, 0.25f, 0.25f );
	b3BodyId boxes[2];
	float x[2] = { 1.0f, 6.0f };
	for ( int i = 0; i < 2; ++i )
	{
		bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = (b3Pos){ x[i], 0.8f, 0.0f };
		boxes[i] = b3CreateBody( worldId, &bodyDef );
		b3CreateHullShape( boxes[i], &shapeDef, &box.base );
	}

	for ( int i = 0; i < 90; ++i )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	ENSURE( b3Body_GetContactCapacity( floorId ) == 2 );
	float restY = (float)b3Body_GetPosition( boxes[1] ).y;
	ENSURE( restY > 0.7f );

	b3DestroyShape( tiles[1], false );
	ENSURE( b3Body_GetContactCapacity( floorId ) == 1 );

	b3ContactData contacts[4];
	ENSURE( b3Shape_GetContactData( tiles[6], contacts, 4 ) == 1 );
	ENSURE( b3Shape_GetContactData( tiles[5], contacts, 4 ) == 0 );

	for ( int i = 0; i < 60; ++i )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	ENSURE( (float)b3Body_GetPosition( boxes[0] ).y < 0.0f );
	ENSURE( fabsf( (float)b3Body_GetPosition( boxes[1] ).y - restY ) < 0.01f );

	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );
#endif
	return 0;
}

// Box3D reads the contact lists of several bodies side by side and gives each body the touching contacts that
// b3Body_GetContactData gives, in the same order, though most entries of the lists of a pile do not touch
static int ContactDataOfBodiesTest( void )
{
#if defined( B3_HAS_CONTACT_DATA_OF_BODIES )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId bodies[B3_CONTACT_DATA_BODIES];
	bodies[0] = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BoxHull floor = b3MakeBoxHull( 5.0f, 0.5f, 5.0f );
	b3CreateHullShape( bodies[0], &shapeDef, &floor.base );

	// Two layers of boxes 2 cm apart: closer than 3 cm Box3D keeps a contact, from 1 cm it does not touch
	b3BoxHull box = b3MakeBoxHull( 0.25f, 0.25f, 0.25f );
	int bodyCount = 1;
	for ( int layer = 0; layer < 2; ++layer )
	{
		for ( int i = 0; i < 4; ++i )
		{
			for ( int j = 0; j < 4; ++j )
			{
				bodyDef = b3DefaultBodyDef();
				bodyDef.type = b3_dynamicBody;
				bodyDef.position = (b3Pos){ 0.52f * (float)i, 0.75f + 0.5f * (float)layer, 0.52f * (float)j };
				bodies[bodyCount] = b3CreateBody( worldId, &bodyDef );
				b3CreateHullShape( bodies[bodyCount], &shapeDef, &box.base );
				bodyCount += 1;
			}
		}
	}

	for ( int i = 0; i < 60; ++i )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}

	int offsets[B3_CONTACT_DATA_BODIES];
	int counts[B3_CONTACT_DATA_BODIES];
	int room = 0;
	for ( int k = 0; k < bodyCount; ++k )
	{
		offsets[k] = room;
		room += b3Body_GetContactCapacity( bodies[k] );
	}

	b3ContactData* walked = malloc( sizeof( b3ContactData ) * (size_t)room );
	b3ContactData* single = malloc( sizeof( b3ContactData ) * (size_t)room );
	b3Body_GetContactDataOfBodies( bodies, bodyCount, walked, offsets, counts );
	int touching = 0;
	for ( int k = 0; k < bodyCount; ++k )
	{
		int count = b3Body_GetContactData( bodies[k], single, b3Body_GetContactCapacity( bodies[k] ) );
		ENSURE( counts[k] == count );
		for ( int i = 0; i < count; ++i )
		{
			const b3ContactData* contact = walked + offsets[k] + i;
			ENSURE( B3_ID_EQUALS( contact->contactId, single[i].contactId ) );
			ENSURE( B3_ID_EQUALS( contact->shapeIdA, single[i].shapeIdA ) );
			ENSURE( B3_ID_EQUALS( contact->shapeIdB, single[i].shapeIdB ) );
			ENSURE( contact->manifolds == single[i].manifolds && contact->manifoldCount == single[i].manifoldCount );
		}
		touching += count;
	}
	printf( "  touching contacts: %d of %d entries\n", touching, room );
	ENSURE( touching > 0 && 2 * touching < room );

	// Without bodies there is nothing to walk
	b3Body_GetContactDataOfBodies( bodies, 0, walked, offsets, counts );

	free( walked );
	free( single );
	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );
#endif
	return 0;
}

// A body of many shapes with a long contact list, like a storey on a heap of debris: Box3D first runs through the lists of
// its shapes, then walks its own list, and the contacts come out as from b3Body_GetContactData
static int ContactDataOfLargeBodyTest( void )
{
#if defined( B3_HAS_CONTACT_DATA_OF_BODIES )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId floorId = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BoxHull floor = b3MakeBoxHull( 10.0f, 0.5f, 10.0f );
	b3CreateHullShape( floorId, &shapeDef, &floor.base );

	// A slab of 16 x 16 cells on the floor, and a box over every corner where four cells meet
	bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = (b3Pos){ 0.0f, 0.75f, 0.0f };
	b3BodyId slabId = b3CreateBody( worldId, &bodyDef );
	for ( int i = 0; i < 16; ++i )
	{
		for ( int j = 0; j < 16; ++j )
		{
			b3BoxHull cell = b3MakeOffsetBoxHull( 0.25f, 0.25f, 0.25f, (b3Vec3){ 0.5f * (float)i, 0.0f, 0.5f * (float)j } );
			b3CreateHullShape( slabId, &shapeDef, &cell.base );
		}
	}

	b3BoxHull box = b3MakeBoxHull( 0.2f, 0.2f, 0.2f );
	for ( int i = 0; i < 15; ++i )
	{
		for ( int j = 0; j < 15; ++j )
		{
			bodyDef.position = (b3Pos){ 0.5f * (float)i + 0.25f, 1.2f, 0.5f * (float)j + 0.25f };
			b3BodyId boxId = b3CreateBody( worldId, &bodyDef );
			b3CreateHullShape( boxId, &shapeDef, &box.base );
		}
	}

	for ( int i = 0; i < 30; ++i )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}

	b3BodyId bodies[2] = { slabId, floorId };
	int offsets[2] = { 0, b3Body_GetContactCapacity( slabId ) };
	int room = offsets[1] + b3Body_GetContactCapacity( floorId );
	printf( "  slab: %d shapes, %d contacts in its list\n", b3Body_GetShapeCount( slabId ), offsets[1] );
	ENSURE( b3Body_GetShapeCount( slabId ) == 256 && offsets[1] >= 512 );

	b3ContactData* walked = malloc( sizeof( b3ContactData ) * (size_t)room );
	b3ContactData* single = malloc( sizeof( b3ContactData ) * (size_t)room );
	int counts[2];
	b3Body_GetContactDataOfBodies( bodies, 2, walked, offsets, counts );
	for ( int k = 0; k < 2; ++k )
	{
		int count = b3Body_GetContactData( bodies[k], single, b3Body_GetContactCapacity( bodies[k] ) );
		ENSURE( counts[k] == count && count > 0 );
		for ( int i = 0; i < count; ++i )
		{
			const b3ContactData* contact = walked + offsets[k] + i;
			ENSURE( B3_ID_EQUALS( contact->contactId, single[i].contactId ) );
			ENSURE( B3_ID_EQUALS( contact->shapeIdA, single[i].shapeIdA ) );
			ENSURE( B3_ID_EQUALS( contact->shapeIdB, single[i].shapeIdB ) );
			ENSURE( contact->manifolds == single[i].manifolds && contact->manifoldCount == single[i].manifoldCount );
		}
	}

	free( walked );
	free( single );
	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );
#endif
	return 0;
}

// The touching contacts of a body, from the touching contacts of its shapes, which their own lists hold
static int TouchingContactsOfShapes( b3BodyId bodyId, b3ContactId* ids, int capacity )
{
	b3ShapeId shapes[8];
	int shapeCount = b3Body_GetShapes( bodyId, shapes, 8 );
	int count = 0;
	for ( int s = 0; s < shapeCount; ++s )
	{
		int room = b3Shape_GetContactCapacity( shapes[s] );
		b3ContactData* data = malloc( sizeof( b3ContactData ) * (size_t)( room > 0 ? room : 1 ) );
		int found = b3Shape_GetContactData( shapes[s], data, room );
		for ( int i = 0; i < found && count < capacity; ++i )
		{
			ids[count++] = data[i].contactId;
		}
		free( data );
	}
	return count;
}

// The contact data of a body is that of the touching contacts of its shapes, and with less room the start of it. Zero
// if it is, and the count of the touching contacts.
static int CheckTouchingContacts( b3BodyId bodyId, int* touchingCount )
{
	int capacity = b3Body_GetContactCapacity( bodyId );
	b3ContactData* data = malloc( sizeof( b3ContactData ) * (size_t)( capacity > 0 ? capacity : 1 ) );
	b3ContactId* ids = malloc( sizeof( b3ContactId ) * (size_t)( capacity > 0 ? capacity : 1 ) );
	int count = b3Body_GetContactData( bodyId, data, capacity );
	int shapeCount = TouchingContactsOfShapes( bodyId, ids, capacity );
	ENSURE( count == shapeCount );
	for ( int i = 0; i < count; ++i )
	{
		ENSURE( b3Contact_IsValid( data[i].contactId ) );
		bool found = false;
		for ( int j = 0; j < shapeCount && found == false; ++j )
		{
			found = B3_ID_EQUALS( data[i].contactId, ids[j] );
		}
		ENSURE( found );
	}

	int half = count / 2;
	b3ContactData* start = malloc( sizeof( b3ContactData ) * (size_t)( half > 0 ? half : 1 ) );
	ENSURE( b3Body_GetContactData( bodyId, start, half ) == half );
	for ( int i = 0; i < half; ++i )
	{
		ENSURE( B3_ID_EQUALS( start[i].contactId, data[i].contactId ) );
	}

	free( start );
	free( ids );
	free( data );
	*touchingCount = count;
	return 0;
}

// Box3D keeps the touching contacts of every body that is not static in a list of their own, and the contact data comes
// from it. It stays the same as from the shapes of the body while contacts start and stop touching, bodies go and change
// type and shapes go, for a body with a few contacts and for a plate with hundreds, also in batches of bodies. The static
// floor keeps no list, its contact data comes from its contact list.
static int TouchingContactsTest( void )
{
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId floorId = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BoxHull floor = b3MakeBoxHull( 20.0f, 0.5f, 20.0f );
	b3CreateHullShape( floorId, &shapeDef, &floor.base );

	// A plate on the floor that the boxes land on
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = (b3Pos){ 0.0f, 0.6f, 0.0f };
	b3BodyId plateId = b3CreateBody( worldId, &bodyDef );
	b3BoxHull plate = b3MakeBoxHull( 6.0f, 0.1f, 6.0f );
	b3CreateHullShape( plateId, &shapeDef, &plate.base );

	// Boxes of two cells each, dropped from different heights, so they land and bounce at different times
	enum
	{
		boxCount = 120
	};
	b3BodyId boxes[boxCount];
	bool alive[boxCount];
	b3BoxHull left = b3MakeOffsetBoxHull( 0.25f, 0.25f, 0.25f, (b3Vec3){ -0.25f, 0.0f, 0.0f } );
	b3BoxHull right = b3MakeOffsetBoxHull( 0.25f, 0.25f, 0.25f, (b3Vec3){ 0.25f, 0.0f, 0.0f } );
	for ( int i = 0; i < boxCount; ++i )
	{
		bodyDef.position =
			(b3Pos){ 0.6f * (float)( i % 10 ) - 3.0f, 1.0f + 0.7f * (float)( i / 10 ), 0.35f * (float)( i % 7 ) - 1.0f };
		boxes[i] = b3CreateBody( worldId, &bodyDef );
		b3CreateHullShape( boxes[i], &shapeDef, &left.base );
		b3CreateHullShape( boxes[i], &shapeDef, &right.base );
		alive[i] = true;
	}

	int plateMost = 0;
	int floorMost = 0;
	for ( int step = 1; step <= 240; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
		for ( int i = 0; i < boxCount; ++i )
		{
			if ( alive[i] == false )
			{
				continue;
			}

			if ( step == 100 && i % 10 == 3 )
			{
				b3DestroyBody( boxes[i] );
				alive[i] = false;
			}
			else if ( step == 130 && i % 7 == 2 )
			{
				b3Body_SetType( boxes[i], b3_staticBody );
			}
			else if ( step == 160 && i % 7 == 2 )
			{
				b3Body_SetType( boxes[i], b3_dynamicBody );
			}
			else if ( step == 190 && i % 9 == 4 )
			{
				b3ShapeId shapes[2];
				ENSURE( b3Body_GetShapes( boxes[i], shapes, 2 ) == 2 );
				b3DestroyShape( shapes[1], true );
			}
		}

		if ( step % 10 != 0 && step != 101 && step != 131 && step != 161 && step != 191 )
		{
			continue;
		}

		// The plate, the floor and the first boxes in a batch, the boxes static for a while among them
		b3BodyId bodyIds[B3_CONTACT_DATA_BODIES];
		int batchCount = 0;
		bodyIds[batchCount++] = plateId;
		bodyIds[batchCount++] = floorId;
		for ( int i = 0; i < boxCount; ++i )
		{
			if ( alive[i] )
			{
				int count;
				ENSURE( CheckTouchingContacts( boxes[i], &count ) == 0 );
				if ( batchCount < B3_CONTACT_DATA_BODIES )
				{
					bodyIds[batchCount++] = boxes[i];
				}
			}
		}
		int plateCount, floorCount;
		ENSURE( CheckTouchingContacts( plateId, &plateCount ) == 0 );
		ENSURE( CheckTouchingContacts( floorId, &floorCount ) == 0 );
		plateMost = b3MaxInt( plateMost, plateCount );
		floorMost = b3MaxInt( floorMost, floorCount );

		// A batch of bodies gives each the same as alone
		int offsets[B3_CONTACT_DATA_BODIES];
		int counts[B3_CONTACT_DATA_BODIES];
		int room = 0;
		for ( int k = 0; k < batchCount; ++k )
		{
			offsets[k] = room;
			room += b3Body_GetContactCapacity( bodyIds[k] );
		}

		b3ContactData* walked = malloc( sizeof( b3ContactData ) * (size_t)( room > 0 ? room : 1 ) );
		b3ContactData* single = malloc( sizeof( b3ContactData ) * (size_t)( room > 0 ? room : 1 ) );
		b3Body_GetContactDataOfBodies( bodyIds, batchCount, walked, offsets, counts );
		for ( int k = 0; k < batchCount; ++k )
		{
			int count = b3Body_GetContactData( bodyIds[k], single, b3Body_GetContactCapacity( bodyIds[k] ) );
			ENSURE( counts[k] == count );
			for ( int i = 0; i < count; ++i )
			{
				ENSURE( B3_ID_EQUALS( walked[offsets[k] + i].contactId, single[i].contactId ) );
			}
		}
		free( walked );
		free( single );
	}

	// The plate held more touching contacts than a body sorts by insertion, and some boxes touched the floor
	printf( "  plate: at most %d touching contacts, floor %d\n", plateMost, floorMost );
	ENSURE( plateMost > 64 && floorMost > 0 );

	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );
	return 0;
}

// A recording of a world with touching contacts replays to the same states. The snapshot it starts from carries the
// lists of the touching contacts and when each contact was created, see b3ContactTouch, and the replay links and
// unlinks contacts in them from there.
static int TouchingContactsReplayTest( void )
{
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId floorId = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BoxHull floor = b3MakeBoxHull( 10.0f, 0.5f, 10.0f );
	b3CreateHullShape( floorId, &shapeDef, &floor.base );

	enum
	{
		boxCount = 40
	};
	b3BodyId boxes[boxCount];
	bodyDef.type = b3_dynamicBody;
	b3BoxHull box = b3MakeBoxHull( 0.25f, 0.25f, 0.25f );
	for ( int i = 0; i < boxCount; ++i )
	{
		bodyDef.position = (b3Pos){ 0.4f * (float)( i % 5 ) - 1.0f, 1.0f + 0.6f * (float)( i / 5 ), 0.3f * (float)( i % 3 ) };
		boxes[i] = b3CreateBody( worldId, &bodyDef );
		b3CreateHullShape( boxes[i], &shapeDef, &box.base );
	}

	// The pile lands before the recording starts, so its snapshot holds touching contacts
	for ( int step = 0; step < 60; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	int floorCapacity = b3Body_GetContactCapacity( floorId );
	b3ContactData* floorContacts = malloc( sizeof( b3ContactData ) * (size_t)( floorCapacity > 0 ? floorCapacity : 1 ) );
	ENSURE( b3Body_GetContactData( floorId, floorContacts, floorCapacity ) > 0 );
	free( floorContacts );

	b3Recording* recording = b3CreateRecording( 0 );
	b3World_StartRecording( worldId, recording );
	for ( int step = 0; step < 90; ++step )
	{
		if ( step == 30 )
		{
			b3DestroyBody( boxes[3] );
			b3DestroyBody( boxes[17] );
		}
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	b3World_StopRecording( worldId );

	ENSURE( b3ValidateReplay( b3Recording_GetData( recording ), b3Recording_GetSize( recording ), 1 ) );

	b3DestroyRecording( recording );
	b3DestroyWorld( worldId );
	ENSURE( b3GetByteCount() == baseBytes );
	return 0;
}

// A sphere closer to another sphere than the speculative distance gets a contact point before they touch, and so does a
// sphere over a capsule, as in upstream Box3D since #187. A step of zero time runs the collision but not the solver.
static int SpeculativeSphereTest( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;

	b3Sphere sphere = { b3Vec3_zero, 0.5f };
	b3Capsule capsule = { { -0.5f, 0.0f, 0.0f }, { 0.5f, 0.0f, 0.0f }, 0.5f };
	float gap = 0.5f * B3_SPECULATIVE_DISTANCE;

	b3ShapeId upper[2];
	for ( int i = 0; i < 2; ++i )
	{
		bodyDef.position = (b3Pos){ 5.0f * (float)i, 0.0f, 0.0f };
		b3BodyId lowerId = b3CreateBody( worldId, &bodyDef );
		if ( i == 0 )
		{
			b3CreateSphereShape( lowerId, &shapeDef, &sphere );
		}
		else
		{
			b3CreateCapsuleShape( lowerId, &shapeDef, &capsule );
		}

		bodyDef.position = (b3Pos){ 5.0f * (float)i, 1.0f + gap, 0.0f };
		b3BodyId upperId = b3CreateBody( worldId, &bodyDef );
		upper[i] = b3CreateSphereShape( upperId, &shapeDef, &sphere );
	}

	// The first step finds the pairs, the second updates their contacts
	b3World_Step( worldId, 0.0f, 4 );
	b3World_Step( worldId, 0.0f, 4 );

	for ( int i = 0; i < 2; ++i )
	{
		b3ContactData contact;
		ENSURE( b3Shape_GetContactData( upper[i], &contact, 1 ) == 1 );
		ENSURE( contact.manifoldCount == 1 && contact.manifolds[0].pointCount == 1 );
		ENSURE( fabsf( contact.manifolds[0].points[0].separation - gap ) < 1.0e-4f );
	}

	b3DestroyWorld( worldId );
	return 0;
}

// Two hulls a face keeps apart skip their test while a bound shows they stay apart, see b3CollideTask in Box3D. The upper
// hull swings down onto the lower one and back, turning and trembling, in steps of zero time. Whenever GJK finds the two
// closer than the speculative distance, their contact must have points. The first pass holds a rock over the top face of
// a box, the second a slab over an edge of a tilted box, so the face is once on the lower hull and once on the upper.
static int SeparatedHullsTest( void )
{
	b3Vec3 points[] = { { -0.4f, -0.3f, -0.2f }, { 0.45f, -0.25f, -0.3f }, { 0.1f, 0.35f, -0.25f }, { -0.3f, 0.2f, 0.3f },
						{ 0.35f, 0.1f, 0.4f },	 { 0.05f, -0.35f, 0.25f }, { -0.45f, 0.05f, -0.05f } };
	b3HullData* rock = b3CreateHull( points, 7, B3_MAX_HULL_VERTICES );
	ENSURE( rock != NULL );
	b3BoxHull box = b3MakeBoxHull( 0.6f, 0.2f, 0.5f );
	b3BoxHull slab = b3MakeBoxHull( 0.7f, 0.1f, 0.7f );
	b3Vec3 axis = b3Normalize( (b3Vec3){ 1.0f, 0.4f, 0.7f } );

	for ( int pass = 0; pass < 2; ++pass )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		worldDef.gravity = b3Vec3_zero;
		b3WorldId worldId = b3CreateWorld( &worldDef );

		// Recycling would keep a contact without a test as well
		b3World_SetContactRecycleDistance( worldId, 0.0f );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.rotation = pass == 0 ? b3Quat_identity : b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.25f * B3_PI );
		b3BodyId lowerId = b3CreateBody( worldId, &bodyDef );
		b3CreateHullShape( lowerId, &shapeDef, &box.base );

		const b3HullData* upper = pass == 0 ? rock : &slab.base;
		bodyDef.rotation = b3Quat_identity;
		b3BodyId upperId = b3CreateBody( worldId, &bodyDef );
		b3ShapeId upperShapeId = b3CreateHullShape( upperId, &shapeDef, upper );

		b3DistanceInput input = { 0 };
		input.proxyA = (b3ShapeProxy){ b3GetHullPoints( &box.base ), box.base.vertexCount, 0.0f };
		input.proxyB = (b3ShapeProxy){ b3GetHullPoints( upper ), upper->vertexCount, 0.0f };

		// The height at which the upper hull, not turned, would touch the lower one
		b3SimplexCache cache = { 0 };
		input.transform = b3InvMulWorldTransforms( b3Body_GetTransform( lowerId ),
												   (b3WorldTransform){ { 0.0f, 2.0f, 0.0f }, b3Quat_identity } );
		float touchHeight = 2.0f - b3ShapeDistance( &input, &cache, NULL, 0 ).distance;

		int nearCount = 0;
		int apartCount = 0;
		bool hadContact = false;
		int stepCount = 1500;
		for ( int i = 0; i < stepCount; ++i )
		{
			// Three swings from 12 cm above touching to 1 cm into the lower hull, with a slow sway and turn and a tremble
			float t = (float)i / (float)stepCount;
			float swing = 0.5f + 0.5f * cosf( 6.0f * B3_PI * t );
			float height = touchHeight - 0.01f + 0.13f * swing + 0.0004f * sinf( 1.7f * (float)i );
			b3Pos position = { 0.06f * sinf( 10.0f * B3_PI * t ), height, 0.05f * cosf( 8.0f * B3_PI * t ) };
			float angle = 0.15f * sinf( 4.0f * B3_PI * t ) + 0.003f * sinf( 2.3f * (float)i );
			b3Body_SetTransform( upperId, position, b3MakeQuatFromAxisAngle( axis, angle ) );
			b3World_Step( worldId, 0.0f, 1 );

			// Only a contact that was there before the step got the collision in it
			bool hasContact = b3World_GetCounters( worldId ).contactCount == 1;
			bool tested = hadContact && hasContact;
			hadContact = hasContact;

			cache = (b3SimplexCache){ 0 };
			input.transform = b3InvMulWorldTransforms( b3Body_GetTransform( lowerId ), b3Body_GetTransform( upperId ) );
			float distance = b3ShapeDistance( &input, &cache, NULL, 0 ).distance;
			b3ContactData contact;
			bool touching = b3Shape_GetContactData( upperShapeId, &contact, 1 ) == 1;
			if ( tested && distance < B3_SPECULATIVE_DISTANCE - 0.2f * B3_LINEAR_SLOP )
			{
				ENSURE( touching );
				nearCount += 1;
			}
			else if ( tested && touching == false )
			{
				apartCount += 1;
			}
		}

		ENSURE( nearCount > 50 && apartCount > 200 );
		b3DestroyWorld( worldId );
	}

	b3DestroyHull( rock );
	return 0;
}

// The static chunks of a destructible sit on one Box3D body, which goes with the destructible or the world
static int SharedStaticBodyTest( void )
{
#if defined( NB_SHARED_STATIC_BODY )
	TestScene scene = CreateScene();
	int baseBodies = b3World_GetCounters( scene.physicsWorld ).bodyCount;
	nbDestructibleId gate = CreateGate( &scene, 0.0f, 11 );
	ENSURE( nbDestructible_GetChunkCount( gate ) > 8 );
	ENSURE( b3World_GetCounters( scene.physicsWorld ).bodyCount == baseBodies + 1 );

	nbWorld* world = nbGetWorldFromId( scene.world );
	const nbDestructible* destructible = world->destructibles.data + gate.index1 - 1;
	for ( int i = 0; i < world->chunks.count; ++i )
	{
		const nbChunk* chunk = world->chunks.data + i;
		if ( chunk->shape != NULL )
		{
			ENSURE( B3_ID_EQUALS( chunk->bodyId, destructible->staticBody ) );
			ENSURE( B3_ID_EQUALS( b3Shape_GetBody( chunk->shapeId ), destructible->staticBody ) );
		}
	}

	nbDestroyDestructible( gate );
	ENSURE( b3World_GetCounters( scene.physicsWorld ).bodyCount == baseBodies );

	// Taking down the Nebenan world leaves the Box3D world as it found it
	CreateGate( &scene, 0.0f, 12 );
	ENSURE( b3World_GetCounters( scene.physicsWorld ).bodyCount == baseBodies + 1 );
	nbDestroyWorld( scene.world );
	ENSURE( b3World_GetCounters( scene.physicsWorld ).bodyCount == baseBodies );
	b3DestroyWorld( scene.physicsWorld );
#endif
	return 0;
}

// Arrays of a megabyte and more grow in place: they keep their address and their values, and the memory count
// returns to where it was when they are freed
static int LargeBlockTest( void )
{
	int64_t baseBytes = nbGetByteCount();

	// From a small block into a large one, which moves the values once
	nbIntArray values = { 0 };
	for ( int i = 0; i < 300000; ++i )
	{
		nbArray_Push( values, i );
	}
	ENSURE( values.capacity * (int)sizeof( int ) >= (int)( 1 << 20 ) );

	// Then in place, a step at a time and a large step at once
	int* data = values.data;
	for ( int i = 300000; i < 2000000; ++i )
	{
		nbArray_Push( values, i );
	}
	nbArray_Reserve( values, 5000000 );
#if defined( NB_LARGE_BLOCKS )
	ENSURE( values.data == data );
#endif
	NB_UNUSED( data );

	for ( int i = 0; i < values.count; ++i )
	{
		ENSURE( values.data[i] == i );
	}

	// New room can be written up to the end
	values.data[values.capacity - 1] = 7;
	ENSURE( nbGetByteCount() - baseBytes >= (int64_t)values.capacity * (int64_t)sizeof( int ) );

	nbArray_Free( values );
	ENSURE( nbGetByteCount() == baseBytes );
	return 0;
}

// A task system that runs a task as soon as it is enqueued, before the calling thread starts its own share
typedef struct TestTaskSystem
{
	int enqueueCount;
	int finishCount;
} TestTaskSystem;

static void* InlineEnqueue( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName )
{
	(void)taskName;
	TestTaskSystem* system = userContext;
	system->enqueueCount += 1;
	task( taskContext );
	return NULL;
}

static void InlineFinish( void* userTask, void* userContext )
{
	(void)userTask;
	TestTaskSystem* system = userContext;
	system->finishCount += 1;
}

// The fracture must not depend on the number of workers or on how the task system schedules them
static int WorkerTest( void )
{
	for ( int workerCount = 2; workerCount <= 8; workerCount *= 2 )
	{
		uint32_t hash = RunDeterminismScenarioWith( CreateSceneWithWorkers( workerCount, NULL, NULL, NULL ) );
		ENSURE( hash == NB_EXPECTED_DETERMINISM_HASH );
	}

	TestTaskSystem system = { 0 };
	uint32_t hash = RunDeterminismScenarioWith( CreateSceneWithWorkers( 4, InlineEnqueue, InlineFinish, &system ) );
	ENSURE( hash == NB_EXPECTED_DETERMINISM_HASH );
	ENSURE( system.enqueueCount > 0 );
	ENSURE( system.finishCount == 0 );

	// Pre-fracture on the workers gives the same chunks
	uint32_t prefractureHashes[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateSceneWithWorkers( pass == 0 ? 1 : 4, NULL, NULL, NULL );
		nbDestructibleDef def = nbDefaultDestructibleDef();
		def.position = (b3Vec3){ 0.0f, 1.5f, 0.0f };
		def.cellSize = 0.35f;
		def.seed = 3;
		nbDestructibleId wall = nbCreateBox( scene.world, &def, (b3Vec3){ 2.0f, 1.5f, 0.2f } );
		ENSURE( nbDestructible_GetChunkCount( wall ) > 50 );
		prefractureHashes[pass] = HashDestructible( 2166136261u, wall );
		DestroyScene( &scene );
	}
	ENSURE( prefractureHashes[0] == prefractureHashes[1] );

	// The cells of a wall with openings are glued where their faces touch. The workers measure the contact areas, the
	// bonds come out the same as on one thread.
	nbOpening openings[2] = {
		{ { -1.2f, 0.3f, 0.0f }, { 0.6f, 0.5f, 0.3f } },
		{ { 1.4f, -0.6f, 0.0f }, { 0.45f, 1.0f, 0.3f } },
	};
	uint32_t bondHashes[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateSceneWithWorkers( pass == 0 ? 1 : 4, NULL, NULL, NULL );
		nbPieceDef piece = nbDefaultPieceDef();
		piece.halfExtents = (b3Vec3){ 3.0f, 1.5f, 0.15f };
		piece.transform.p = (b3Vec3){ 0.0f, 1.5f, 0.0f };
		piece.openings = openings;
		piece.openingCount = 2;
		nbDestructibleDef def = nbDefaultDestructibleDef();
		def.cellSize = 0.4f;
		def.seed = 9;
		nbDestructibleId wall = nbCreateDestructible( scene.world, &def, &piece, 1 );
		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( nbDestructible_GetChunkCount( wall ) > 80 );
		ENSURE( world->bondCount > 200 );
		bondHashes[pass] = HashBonds( HashDestructible( 2166136261u, wall ), world );
		DestroyScene( &scene );
	}
	ENSURE( bondHashes[0] == bondHashes[1] );
	return 0;
}

// Box3D solves contacts in eight lanes on x86 with AVX2 and in four otherwise, see b3GetSIMDWidth, and with eight it
// collides convex shapes with code compiled for AVX2. Both must give the same bits, on one thread and with workers, which
// solve whole islands. Without AVX2 this runs four lanes twice.
static int SIMDWidthTest( void )
{
	int widths[2] = { 4, 8 };
	for ( int i = 0; i < 2; ++i )
	{
		b3SetSIMDWidth( widths[i] );
		ENSURE( b3GetSIMDWidth() == ( b3IsAVX2Available() ? widths[i] : 4 ) );
		ENSURE( RunDeterminismScenario() == NB_EXPECTED_DETERMINISM_HASH );
		ENSURE( RunDeterminismScenarioWith( CreateSceneWithWorkers( 4, NULL, NULL, NULL ) ) == NB_EXPECTED_DETERMINISM_HASH );
	}

	b3SetSIMDWidth( 0 );
	printf( "contact solver lanes: %d\n", b3GetSIMDWidth() );
	return 0;
}

// A heavy plate on 36 small cubes has more contacts than the constraint graph has colors. The contacts it has left over
// make it a knot, which Box3D solves in batches against copies of the plate, see b3KnotBatch. The plate must come to rest
// on the cubes, as high as they are, without sinking into them or shaking, and the same with every number of workers. The
// light plate is barely heavier than the cubes under a copy.
static int KnotTest( void )
{
	float densities[2] = { 20000.0f, 1000.0f };
	for ( int d = 0; d < 2; ++d )
	{
		b3Pos ends[3];
		for ( int pass = 0; pass < 3; ++pass )
		{
			TestScene scene = CreateSceneWithWorkers( pass == 0 ? 1 : 2 * pass, NULL, NULL, NULL );

			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.type = b3_dynamicBody;
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			b3BoxHull cube = b3MakeBoxHull( 0.05f, 0.05f, 0.05f );
			b3BodyId cubes[36];
			for ( int i = 0; i < 36; ++i )
			{
				bodyDef.position = (b3Vec3){ 0.12f * (float)( i % 6 ) - 0.3f, 0.05f, 0.12f * (float)( i / 6 ) - 0.3f };
				cubes[i] = b3CreateBody( scene.physicsWorld, &bodyDef );
				b3CreateHullShape( cubes[i], &shapeDef, &cube.base );
			}

			bodyDef.position = (b3Vec3){ 0.0f, 0.151f, 0.0f };
			b3BodyId plate = b3CreateBody( scene.physicsWorld, &bodyDef );
			b3BoxHull slab = b3MakeBoxHull( 0.4f, 0.05f, 0.4f );
			shapeDef.density = densities[d];
			b3CreateHullShape( plate, &shapeDef, &slab.base );

			int knotContacts = 0;
			for ( int step = 0; step < 240; ++step )
			{
				nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
				knotContacts = b3MaxInt( knotContacts, b3World_GetCounters( scene.physicsWorld ).knotContactCount );
			}

			b3Pos p = b3Body_GetPosition( plate );
			float speed = b3Length( b3Body_GetLinearVelocity( plate ) );
			float lowest = 1.0f, highest = 0.0f;
			for ( int i = 0; i < 36; ++i )
			{
				float y = (float)b3Body_GetPosition( cubes[i] ).y;
				lowest = b3MinFloat( lowest, y );
				highest = b3MaxFloat( highest, y );
			}
			if ( pass == 0 )
			{
				printf( "  plate of %.0f kg on 36 cubes: %d contacts at the knot, plate at %.4f m moving %.4f m/s, cubes "
						"%.4f to %.4f m\n",
						b3Body_GetMass( plate ), knotContacts, (float)p.y, speed, lowest, highest );
			}

			ENSURE( knotContacts > 0 );
			ENSURE( 0.135f < (float)p.y && (float)p.y < 0.155f );
			ENSURE( speed < 0.01f );
			ENSURE( 0.04f < lowest && highest < 0.06f );
			ends[pass] = p;
			DestroyScene( &scene );
		}

		for ( int pass = 1; pass < 3; ++pass )
		{
			ENSURE( memcmp( ends + pass, ends, sizeof( b3Pos ) ) == 0 );
		}
	}
	return 0;
}

// With enough contacts at knots Box3D deals their batches out to groups that the threads solve at once, and a knot in
// several groups has a copy of itself in each, see b3BuildKnots. Two heavy plates on 400 and 256 small cubes make two
// such knots in two islands. The plates must come to rest on the cubes as in KnotTest, and the same in every bit with
// every number of workers: one solves the groups one after the other, two solve the plates whole in packs, four solve the
// larger plate as a team with a thread per group and the smaller in a pack meanwhile, which holds less than twice the
// share of a thread, see TeamPacksTest. Four lanes must give the same.
static int KnotGroupTest( void )
{
	const int sides[2] = { 20, 16 };
	const int workers[4] = { 1, 2, 4, 4 };
	float densities[2] = { 20000.0f, 1000.0f };
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	for ( int d = 0; d < 2; ++d )
	{
		b3Pos ends[4][2];
		for ( int pass = 0; pass < 4; ++pass )
		{
			b3SetSIMDWidth( pass == 3 ? 4 : 0 );
			TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, workers[pass] );

			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.type = b3_dynamicBody;
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			b3BoxHull cube = b3MakeBoxHull( 0.05f, 0.05f, 0.05f );
			b3BodyId cubes[2][400];
			b3BodyId plates[2];
			for ( int p = 0; p < 2; ++p )
			{
				int side = sides[p];
				float center = p == 0 ? -1.6f : 1.6f;
				float offset = 0.06f * (float)( side - 1 );
				shapeDef.density = 1000.0f;
				for ( int i = 0; i < side * side; ++i )
				{
					bodyDef.position =
						(b3Vec3){ center + 0.12f * (float)( i % side ) - offset, 0.05f, 0.12f * (float)( i / side ) - offset };
					cubes[p][i] = b3CreateBody( scene.physicsWorld, &bodyDef );
					b3CreateHullShape( cubes[p][i], &shapeDef, &cube.base );
				}

				bodyDef.position = (b3Vec3){ center, 0.151f, 0.0f };
				plates[p] = b3CreateBody( scene.physicsWorld, &bodyDef );
				b3BoxHull slab = b3MakeBoxHull( 0.06f * (float)side, 0.05f, 0.06f * (float)side );
				shapeDef.density = densities[d];
				b3CreateHullShape( plates[p], &shapeDef, &slab.base );
			}

			int knotContacts = 0, rowBatches = 0, copies = 0, packSteps = 0, teamSteps = 0, joinSteps = 0;
			for ( int step = 0; step < 240; ++step )
			{
				nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
				b3Counters counters = b3World_GetCounters( scene.physicsWorld );
				knotContacts = b3MaxInt( knotContacts, counters.knotContactCount );
				rowBatches = b3MaxInt( rowBatches, counters.knotGroupBatchCount );
				copies = b3MaxInt( copies, counters.knotCopyCount );
				packSteps += counters.islandWork > 0 && counters.teamWork == 0 ? 1 : 0;
				teamSteps += counters.teamWork > 0 ? 1 : 0;
				joinSteps += counters.teamPackCount > 1 ? 1 : 0;
			}

			for ( int p = 0; p < 2; ++p )
			{
				b3Pos position = b3Body_GetPosition( plates[p] );
				float speed = b3Length( b3Body_GetLinearVelocity( plates[p] ) );
				float lowest = 1.0f, highest = 0.0f;
				for ( int i = 0; i < sides[p] * sides[p]; ++i )
				{
					float y = (float)b3Body_GetPosition( cubes[p][i] ).y;
					lowest = b3MinFloat( lowest, y );
					highest = b3MaxFloat( highest, y );
				}
				if ( pass == 0 )
				{
					printf( "  plate of %.0f kg on %d cubes at %.4f m moving %.4f m/s, cubes %.4f to %.4f m\n",
							b3Body_GetMass( plates[p] ), sides[p] * sides[p], (float)position.y, speed, lowest, highest );
				}

				ENSURE( 0.135f < (float)position.y && (float)position.y < 0.155f );
				ENSURE( speed < 0.01f );
				ENSURE( 0.04f < lowest && highest < 0.06f );
				ends[pass][p] = position;
			}

			if ( pass < 3 )
			{
				printf( "  %d worker%s: %d contacts at knots, %d batches of them in a row, %d copies, packs in %d steps, a team "
						"in %d, with both plates in %d\n",
						workers[pass], workers[pass] == 1 ? "" : "s", knotContacts, rowBatches, copies, packSteps, teamSteps,
						joinSteps );
			}
			ENSURE( copies > 0 && rowBatches < knotContacts / 8 );
			ENSURE( pass != 1 || packSteps > 0 );
			ENSURE( pass != 2 || teamSteps > 0 );
			ENSURE( joinSteps == 0 );
			DestroyScene( &scene );
		}
		b3SetSIMDWidth( 0 );

		for ( int pass = 1; pass < 4; ++pass )
		{
			ENSURE( memcmp( ends + pass, ends, sizeof( ends[0] ) ) == 0 );
		}
	}
	nbDestroyTaskSystem( pool );
	return 0;
}

// Two heavy plates on two equal heaps of cubes make two islands with the same work and a knot each. With 8 workers each
// holds more than twice the share of a thread, so one team solves both in its stages, see b3Counters::teamPackCount, and
// the groups of both knots run in its stages of the knots. With 4 the second holds no more than twice the share, and a
// worker solves it whole while the team solves the first. Every number of workers and either width must end the same in
// every bit.
static int TeamPacksTest( void )
{
	const int workers[4] = { 1, 4, 8, 8 };
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	b3Pos ends[4][2];
	for ( int pass = 0; pass < 4; ++pass )
	{
		b3SetSIMDWidth( pass == 3 ? 4 : 0 );
		TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, workers[pass] );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3BoxHull cube = b3MakeBoxHull( 0.05f, 0.05f, 0.05f );
		const int side = 20;
		b3BodyId plates[2];
		for ( int p = 0; p < 2; ++p )
		{
			float center = p == 0 ? -1.6f : 1.6f;
			float offset = 0.06f * (float)( side - 1 );
			shapeDef.density = 1000.0f;
			for ( int i = 0; i < side * side; ++i )
			{
				bodyDef.position =
					(b3Vec3){ center + 0.12f * (float)( i % side ) - offset, 0.05f, 0.12f * (float)( i / side ) - offset };
				b3BodyId body = b3CreateBody( scene.physicsWorld, &bodyDef );
				b3CreateHullShape( body, &shapeDef, &cube.base );
			}

			bodyDef.position = (b3Vec3){ center, 0.151f, 0.0f };
			plates[p] = b3CreateBody( scene.physicsWorld, &bodyDef );
			b3BoxHull slab = b3MakeBoxHull( 0.06f * (float)side, 0.05f, 0.06f * (float)side );
			shapeDef.density = 20000.0f;
			b3CreateHullShape( plates[p], &shapeDef, &slab.base );
		}

		int teamSteps = 0, joinSteps = 0;
		for ( int step = 0; step < 240; ++step )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			b3Counters counters = b3World_GetCounters( scene.physicsWorld );
			teamSteps += counters.teamWork > 0 ? 1 : 0;
			joinSteps += counters.teamPackCount > 1 ? 1 : 0;
			ENSURE( counters.teamPackCount <= 2 && ( counters.teamWork > 0 ) == ( counters.teamPackCount > 0 ) );
		}

		for ( int p = 0; p < 2; ++p )
		{
			b3Pos position = b3Body_GetPosition( plates[p] );
			ENSURE( 0.135f < (float)position.y && (float)position.y < 0.155f );
			ENSURE( b3Length( b3Body_GetLinearVelocity( plates[p] ) ) < 0.01f );
			ends[pass][p] = position;
		}

		if ( pass < 3 )
		{
			printf( "  %d worker%s: a team in %d steps, with both heaps in %d\n", workers[pass], workers[pass] == 1 ? "" : "s",
					teamSteps, joinSteps );
		}
		ENSURE( pass != 2 || joinSteps > 0 );
		ENSURE( joinSteps <= teamSteps );
		DestroyScene( &scene );
	}
	b3SetSIMDWidth( 0 );
	nbDestroyTaskSystem( pool );

	for ( int pass = 1; pass < 4; ++pass )
	{
		ENSURE( memcmp( ends + pass, ends, sizeof( ends[0] ) ) == 0 );
	}
	return 0;
}

// A heavy plate on two layers of small cubes, 32 by 32 below and 31 by 31 above, each upper cube on the corners of four
// lower ones, makes one island with about 5 800 contacts, a large island that splits into eight domains, see b3Domains.
// Each domain solves its passes alone, and a body with contacts in several domains has a copy in each with its share of
// the mass: the plate and the cubes on the cuts between the domains. The plate must come to rest on the cubes as in
// KnotTest, and every body must end the same in every bit with one, two, four and eight workers and in four lanes.
static int DomainTest( void )
{
	enum
	{
		side = 32,
		cubeCount = side * side + ( side - 1 ) * ( side - 1 ),
	};
	const int workers[5] = { 1, 2, 4, 8, 8 };
	nbTaskSystem* pool = nbCreateTaskSystem( 7 );
	uint32_t hashes[5];
	for ( int pass = 0; pass < 5; ++pass )
	{
		b3SetSIMDWidth( pass == 4 ? 4 : 0 );
		TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, workers[pass] );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3BoxHull cube = b3MakeBoxHull( 0.05f, 0.05f, 0.05f );
		b3BodyId bodies[cubeCount + 1];
		int count = 0;
		for ( int layer = 0; layer < 2; ++layer )
		{
			int cubes = side - layer;
			float offset = 0.06f * (float)( cubes - 1 );
			for ( int i = 0; i < cubes * cubes; ++i )
			{
				bodyDef.position = (b3Vec3){ 0.12f * (float)( i % cubes ) - offset, 0.05f + 0.1f * (float)layer,
											 0.12f * (float)( i / cubes ) - offset };
				bodies[count] = b3CreateBody( scene.physicsWorld, &bodyDef );
				b3CreateHullShape( bodies[count], &shapeDef, &cube.base );
				count += 1;
			}
		}

		bodyDef.position = (b3Vec3){ 0.0f, 0.251f, 0.0f };
		b3BodyId plate = b3CreateBody( scene.physicsWorld, &bodyDef );
		bodies[cubeCount] = plate;
		b3BoxHull slab = b3MakeBoxHull( 0.06f * (float)side, 0.05f, 0.06f * (float)side );
		shapeDef.density = 20000.0f;
		b3CreateHullShape( plate, &shapeDef, &slab.base );

		int domainSteps = 0, domains = 0, copies = 0;
		for ( int step = 0; step < 240; ++step )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			b3Counters counters = b3World_GetCounters( scene.physicsWorld );
			domainSteps += counters.domainCount > 0 ? 1 : 0;
			domains = b3MaxInt( domains, counters.domainCount );
			copies = b3MaxInt( copies, counters.domainCopyCount );
		}

		b3Pos position = b3Body_GetPosition( plate );
		float speed = b3Length( b3Body_GetLinearVelocity( plate ) );
		float lowest[2] = { 1.0f, 1.0f }, highest[2] = { 0.0f, 0.0f };
		uint32_t hash = 2166136261u;
		for ( int i = 0; i <= cubeCount; ++i )
		{
			b3WorldTransform transform = b3Body_GetTransform( bodies[i] );
			const unsigned char* bytes = (const unsigned char*)&transform;
			for ( int k = 0; k < (int)sizeof( transform ); ++k )
			{
				hash = ( hash ^ bytes[k] ) * 16777619u;
			}

			if ( i < cubeCount )
			{
				int layer = i < side * side ? 0 : 1;
				float y = (float)b3Body_GetPosition( bodies[i] ).y;
				lowest[layer] = b3MinFloat( lowest[layer], y );
				highest[layer] = b3MaxFloat( highest[layer], y );
			}
		}
		hashes[pass] = hash;

		if ( pass < 4 )
		{
			printf( "  %d worker%s: domains in %d of 240 steps, %d domains, up to %d copies, plate at %.4f m moving %.4f m/s, "
					"cubes %.4f to %.4f and %.4f to %.4f m\n",
					workers[pass], workers[pass] == 1 ? "" : "s", domainSteps, domains, copies, (float)position.y, speed,
					lowest[0], highest[0], lowest[1], highest[1] );
		}

		ENSURE( domainSteps > 20 && domains == 8 && copies > 8 );
		ENSURE( 0.235f < (float)position.y && (float)position.y < 0.255f );
		ENSURE( speed < 0.01f );
		ENSURE( 0.04f < lowest[0] && highest[0] < 0.06f );
		ENSURE( 0.14f < lowest[1] && highest[1] < 0.16f );
		DestroyScene( &scene );
	}
	b3SetSIMDWidth( 0 );
	nbDestroyTaskSystem( pool );

	for ( int pass = 1; pass < 5; ++pass )
	{
		ENSURE( hashes[pass] == hashes[0] );
	}
	return 0;
}

// The domains of a large island last from step to step and follow what happens to it, see b3Island::domains. A stack that
// falls on the plate of DomainTest joins its domains. A joint or a mesh contact in the world takes them back to the colors,
// and they come back once it is gone. Without the plate and a row of upper cubes the island splits into two halves that
// keep the domains, fall asleep with them and have them again when one wakes. Every body must end the same in every bit
// with one and four workers and in four lanes.
static int DomainLifeTest( void )
{
	enum
	{
		side = 24,
		cubeCount = side * side + ( side - 1 ) * ( side - 1 ),
		stackCount = 3,
		stepCount = 250,
	};
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	uint32_t hashes[3];
	for ( int pass = 0; pass < 3; ++pass )
	{
		b3SetSIMDWidth( pass == 2 ? 4 : 0 );
		TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, 4 );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3BoxHull cube = b3MakeBoxHull( 0.05f, 0.05f, 0.05f );
		b3BodyId bodies[cubeCount + 1 + stackCount];
		bool alive[cubeCount + 1 + stackCount];
		int count = 0;
		for ( int layer = 0; layer < 2; ++layer )
		{
			int cubes = side - layer;
			float offset = 0.06f * (float)( cubes - 1 );
			for ( int i = 0; i < cubes * cubes; ++i )
			{
				bodyDef.position = (b3Vec3){ 0.12f * (float)( i % cubes ) - offset, 0.05f + 0.1f * (float)layer,
											 0.12f * (float)( i / cubes ) - offset };
				bodies[count] = b3CreateBody( scene.physicsWorld, &bodyDef );
				b3CreateHullShape( bodies[count], &shapeDef, &cube.base );
				count += 1;
			}
		}

		bodyDef.position = (b3Vec3){ 0.0f, 0.251f, 0.0f };
		int plate = cubeCount;
		bodies[plate] = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3BoxHull slab = b3MakeBoxHull( 0.06f * (float)side, 0.05f, 0.06f * (float)side );
		b3ShapeDef plateDef = shapeDef;
		plateDef.density = 20000.0f;
		b3CreateHullShape( bodies[plate], &plateDef, &slab.base );
		for ( int i = 0; i < cubeCount + 1 + stackCount; ++i )
		{
			alive[i] = i <= cubeCount;
		}

		b3JointId joint = b3_nullJointId;
		b3BodyId meshBody = b3_nullBodyId;
		b3MeshData* mesh = NULL;
		int domainCounts[stepCount], islandCounts[stepCount];
		for ( int step = 0; step < stepCount; ++step )
		{
			if ( step == 20 )
			{
				// A stack of three cubes falls on the plate
				for ( int k = 0; k < stackCount; ++k )
				{
					bodyDef.position = (b3Vec3){ 0.3f, 0.36f + 0.11f * (float)k, 0.3f };
					bodies[plate + 1 + k] = b3CreateBody( scene.physicsWorld, &bodyDef );
					b3CreateHullShape( bodies[plate + 1 + k], &shapeDef, &cube.base );
					alive[plate + 1 + k] = true;
				}
			}
			else if ( step == 50 )
			{
				// Two upper cubes held apart
				b3DistanceJointDef jointDef = b3DefaultDistanceJointDef();
				jointDef.base.bodyIdA = bodies[side * side];
				jointDef.base.bodyIdB = bodies[side * side + 1];
				jointDef.length = 0.12f;
				joint = b3CreateDistanceJoint( scene.physicsWorld, &jointDef );
			}
			else if ( step == 60 )
			{
				b3DestroyJoint( joint, true );
			}
			else if ( step == 80 )
			{
				// A mesh on the ground under the corner
				mesh = b3CreateGridMesh( 4, 4, 0.1f, 1, false );
				b3BodyDef meshDef = b3DefaultBodyDef();
				meshDef.position = (b3Vec3){ -0.06f * (float)( side - 1 ), 0.0f, -0.06f * (float)( side - 1 ) };
				meshBody = b3CreateBody( scene.physicsWorld, &meshDef );
				b3CreateMeshShape( meshBody, &shapeDef, mesh, (b3Vec3){ 1.0f, 1.0f, 1.0f } );
			}
			else if ( step == 90 )
			{
				b3DestroyBody( meshBody );
				b3DestroyMesh( mesh );
			}
			else if ( step == 110 )
			{
				// Without the plate, the stack and the middle row of upper cubes the island falls into two halves
				for ( int i = 0; i < cubeCount + 1 + stackCount; ++i )
				{
					int row = ( i - side * side ) / ( side - 1 );
					if ( i >= cubeCount || ( i >= side * side && row == side / 2 - 1 ) )
					{
						b3DestroyBody( bodies[i] );
						alive[i] = false;
					}
				}
			}
			else if ( step == 230 )
			{
				ENSURE( b3Body_IsAwake( bodies[0] ) == false && b3Body_IsAwake( bodies[side * side - 1] ) == false );
				b3Body_SetAwake( bodies[0], true );
			}

			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			b3Counters counters = b3World_GetCounters( scene.physicsWorld );
			domainCounts[step] = counters.domainCount;
			islandCounts[step] = counters.islandCount;
		}

		// The half that woke has domains, the other sleeps
		ENSURE( b3Body_IsAwake( bodies[0] ) && b3Body_IsAwake( bodies[side * side - 1] ) == false );

		uint32_t hash = 2166136261u;
		for ( int i = 0; i < cubeCount + 1 + stackCount; ++i )
		{
			if ( alive[i] == false )
			{
				continue;
			}

			b3WorldTransform transform = b3Body_GetTransform( bodies[i] );
			const unsigned char* bytes = (const unsigned char*)&transform;
			for ( int k = 0; k < (int)sizeof( transform ); ++k )
			{
				hash = ( hash ^ bytes[k] ) * 16777619u;
			}

			// Nothing hangs in the air
			float y = (float)b3Body_GetPosition( bodies[i] ).y;
			ENSURE( i < side * side ? ( 0.04f < y && y < 0.06f ) : ( 0.14f < y && y < 0.16f ) );
		}
		hashes[pass] = hash;

		if ( pass == 0 )
		{
			printf( "  domains at steps 19, 49, 59, 79, 89, 109, 140, 229 and 249: %d %d %d %d %d %d %d %d %d, %d islands at 140\n",
					domainCounts[19], domainCounts[49], domainCounts[59], domainCounts[79], domainCounts[89], domainCounts[109],
					domainCounts[140], domainCounts[229], domainCounts[249], islandCounts[140] );
		}

		// With the domains from the start, with the stack, without them while the joint and the mesh are there, again
		// after, in the halves and asleep, and in the half that woke
		ENSURE( domainCounts[19] == 8 && domainCounts[49] == 8 );
		ENSURE( domainCounts[50] == 0 && domainCounts[59] == 0 && domainCounts[61] == 8 && domainCounts[79] == 8 );
		ENSURE( domainCounts[81] == 0 && domainCounts[89] == 0 && domainCounts[91] == 8 && domainCounts[109] == 8 );
		ENSURE( islandCounts[140] == 2 && domainCounts[140] == 8 );
		ENSURE( domainCounts[229] == 0 && domainCounts[230] == 8 && domainCounts[249] == 8 );
		DestroyScene( &scene );
	}
	b3SetSIMDWidth( 0 );
	nbDestroyTaskSystem( pool );

	ENSURE( hashes[1] == hashes[0] && hashes[2] == hashes[0] );
	return 0;
}

// Replaying the events must reproduce the set of live chunks, which is what a renderer relies on.
static int EventTest( void )
{
	TestScene scene = CreateScene();
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 2.0f, 1.5f, 0.12f }, 9 );

	int capacity = 1 << 16;
	int* alive = calloc( (size_t)capacity, sizeof( int ) );

	nbImpactDef impact = { 0 };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.4f;
	impact.damage = 6.0e4f;
	impact.ejectSpeed = 6.0f;

	for ( int round = 0; round < 6; ++round )
	{
		impact.point = (b3Vec3){ -1.0f + 0.4f * (float)round, 1.0f, 0.12f };
		nbWorld_ApplyImpact( scene.world, &impact );
		Step( &scene, 10 );

		nbEvents events = nbWorld_GetEvents( scene.world );
		for ( int i = 0; i < events.destroyedCount; ++i )
		{
			int index = events.destroyedChunks[i].index1;
			ENSURE( index < capacity );
			alive[index] = 0;
		}
		for ( int i = 0; i < events.createdCount; ++i )
		{
			nbChunkId id = events.createdChunks[i];
			if ( nbChunk_IsValid( id ) )
			{
				alive[id.index1] = 1;
			}
		}
		for ( int i = 0; i < events.movedCount; ++i )
		{
			nbChunkId id = events.movedChunks[i];
			ENSURE( nbChunk_IsValid( id ) == false || alive[id.index1] == 1 );
		}
	}

	int liveCount = 0;
	for ( int i = 0; i < capacity; ++i )
	{
		liveCount += alive[i];
	}
	ENSURE( liveCount == nbDestructible_GetChunkCount( wall ) );

	free( alive );
	DestroyScene( &scene );
	return 0;
}

static int DynamicDestructibleTest( void )
{
	TestScene scene = CreateScene();

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.isStatic = false;
	def.position = (b3Vec3){ 0.0f, 0.5f, 0.0f };
	nbDestructibleId crate = nbCreateBox( scene.world, &def, (b3Vec3){ 0.5f, 0.5f, 0.5f } );
	ENSURE( nbDestructible_IsValid( crate ) );

	Step( &scene, 30 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.0f, 0.9f, 0.0f };
	impact.radius = 0.35f;
	impact.damage = 1.0e5f;
	impact.ejectSpeed = 5.0f;
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.fracturedChunkCount == 1 );
	ENSURE( result.createdBodyCount > 0 );

	Step( &scene, 120 );

	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.staticBodyCount == 0 );
	ENSURE( stats.dynamicBodyCount > 1 );

	DestroyScene( &scene );
	return 0;
}

static int MultiPieceTest( void )
{
	TestScene scene = CreateScene();

	// Two pillars and a beam: a small gate
	nbPieceDef pieces[3];
	for ( int i = 0; i < 3; ++i )
	{
		pieces[i] = nbDefaultPieceDef();
	}
	pieces[0].halfExtents = (b3Vec3){ 0.2f, 1.0f, 0.2f };
	pieces[0].transform.p = (b3Vec3){ -1.0f, 1.0f, 0.0f };
	pieces[1].halfExtents = (b3Vec3){ 0.2f, 1.0f, 0.2f };
	pieces[1].transform.p = (b3Vec3){ 1.0f, 1.0f, 0.0f };
	pieces[2].halfExtents = (b3Vec3){ 1.4f, 0.2f, 0.2f };
	pieces[2].transform.p = (b3Vec3){ 0.0f, 2.2f, 0.0f };

	nbDestructibleDef def = nbDefaultDestructibleDef();
	nbDestructibleId gate = nbCreateDestructible( scene.world, &def, pieces, 3 );
	ENSURE( nbDestructible_GetChunkCount( gate ) == 3 );

	nbChunkId chunks[3];
	nbDestructible_GetChunks( gate, chunks, 3 );
	int bondTotal = 0;
	int anchoredCount = 0;
	for ( int i = 0; i < 3; ++i )
	{
		bondTotal += nbChunk_GetBondCount( chunks[i] );
		anchoredCount += nbChunk_IsAnchored( chunks[i] ) ? 1 : 0;
	}

	// Beam glued to both pillars, both pillars on the ground
	ENSURE( bondTotal == 4 );
	ENSURE( anchoredCount == 2 );

	// Destroy the top of one pillar. The beam still hangs on the other one.
	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ -1.0f, 1.9f, 0.0f };
	impact.radius = 0.35f;
	impact.damage = 1.0e7f;
	impact.ejectSpeed = 3.0f;
	nbWorld_ApplyImpact( scene.world, &impact );

	// Now the other one. The beam loses its last support and falls.
	impact.point = (b3Vec3){ 1.0f, 1.9f, 0.0f };
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.createdBodyCount > 0 );

	bool beamFalling = false;
	int count = nbDestructible_GetChunkCount( gate );
	nbChunkId* all = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( gate, all, count );
	for ( int i = 0; i < count; ++i )
	{
		// The impacts also chipped the beam, so look for a heavy dynamic body rather than one chunk
		if ( nbChunk_IsDynamic( all[i] ) && b3Body_GetMass( nbChunk_GetBody( all[i] ) ) > 0.25f * 2400.0f )
		{
			beamFalling = true;
		}
	}
	free( all );
	ENSURE( beamFalling );

	DestroyScene( &scene );
	return 0;
}

static int PreFractureTest( void )
{
	TestScene scene = CreateScene();

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.position = (b3Vec3){ 0.0f, 1.0f, 0.0f };
	def.cellSize = 0.5f;
	nbDestructibleId wall = nbCreateBox( scene.world, &def, (b3Vec3){ 2.0f, 1.0f, 0.1f } );

	int count = nbDestructible_GetChunkCount( wall );
	ENSURE( count > 20 );
	ENSURE_SMALL( TotalChunkVolume( wall ) - 1.6f, 1.0e-3f );

	// All cells are glued, nothing falls
	Step( &scene, 30 );
	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.dynamicBodyCount == 0 );

	DestroyScene( &scene );
	return 0;
}

// Openings are cut out of a piece, out of its cells with a cell size. What is left is the wall around them, glued into
// one and standing.
static int OpeningTest( void )
{
	// A window, and a door that reaches below the wall
	nbOpening openings[2] = {
		{ { -0.8f, 0.2f, 0.0f }, { 0.6f, 0.5f, 0.3f } },
		{ { 1.1f, -0.6f, 0.0f }, { 0.45f, 1.0f, 0.3f } },
	};
	float expectedVolume = 4.0f * 3.0f * 0.3f - 1.2f * 1.0f * 0.3f - 0.9f * 1.9f * 0.3f;

	for ( int k = 0; k < 2; ++k )
	{
		TestScene scene = CreateScene();

		// Turned about the vertical, the openings turn with the piece
		nbPieceDef piece = nbDefaultPieceDef();
		piece.halfExtents = (b3Vec3){ 2.0f, 1.5f, 0.15f };
		piece.transform.p = (b3Vec3){ 1.0f, 1.5f, -2.0f };
		piece.transform.q = b3MakeQuatFromAxisAngle( (b3Vec3){ 0.0f, 1.0f, 0.0f }, 0.5f );
		piece.surfaceMaterial = 3;
		piece.interiorMaterial = 4;
		piece.openings = openings;
		piece.openingCount = 2;

		nbDestructibleDef def = nbDefaultDestructibleDef();
		def.cellSize = k == 0 ? 0.0f : 0.5f;
		nbDestructibleId wall = nbCreateDestructible( scene.world, &def, &piece, 1 );

		int count = nbDestructible_GetChunkCount( wall );
		ENSURE( count >= ( k == 0 ? 4 : 30 ) );
		ENSURE_SMALL( TotalChunkVolume( wall ) - expectedVolume, 1.0e-3f );

		// No chunk reaches into an opening, and the faces of the openings are surface
		nbWorld* world = nbGetWorldFromId( scene.world );
		int openingFaces = 0;
		for ( int c = 0; c < world->chunks.count; ++c )
		{
			const nbShape* shape = world->chunks.data[c].shape;
			if ( shape == NULL )
			{
				continue;
			}

			for ( int v = 0; v < shape->vertexCount; ++v )
			{
				b3Vec3 p = b3InvTransformPoint( piece.transform, shape->vertices[v] );
				for ( int o = 0; o < 2; ++o )
				{
					b3Vec3 d = b3Abs( b3Sub( p, openings[o].center ) );
					b3Vec3 h = openings[o].halfExtents;
					ENSURE( d.x >= h.x - 1.0e-4f || d.y >= h.y - 1.0e-4f || d.z >= h.z - 1.0e-4f );
				}
			}

			for ( int f = 0; f < shape->faceCount; ++f )
			{
				b3Vec3 normal = b3InvRotateVector( piece.transform.q, shape->planes[f].normal );
				uint8_t material = shape->faces[f].material;
				ENSURE( material == 3 || material == 4 );
				openingFaces += b3AbsFloat( normal.z ) < 1.0e-3f && material == 3 && b3AbsFloat( normal.y ) < 0.999f ? 1 : 0;
			}
		}
		ENSURE( openingFaces > 0 );

		// One piece of wall, every chunk reached through the bonds
		int* stack = malloc( sizeof( int ) * (size_t)world->chunks.count );
		world->searchStamp += 1;
		int reached = 0;
		int stackCount = 0;
		for ( int c = 0; c < world->chunks.count && stackCount == 0; ++c )
		{
			if ( world->chunks.data[c].shape != NULL )
			{
				world->chunks.data[c].searchStamp = world->searchStamp;
				stack[stackCount++] = c;
			}
		}
		while ( stackCount > 0 )
		{
			const nbChunk* chunk = world->chunks.data + stack[--stackCount];
			reached += 1;
			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				int side = key & 1;
				key = bond->nextKey[side];
				nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
				if ( other->searchStamp != world->searchStamp )
				{
					other->searchStamp = world->searchStamp;
					stack[stackCount++] = bond->chunk[side ^ 1];
				}
			}
		}
		free( stack );
		ENSURE( reached == count );

		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount == 0 );

		DestroyScene( &scene );
	}
	return 0;
}

// Cells run across the seam between two pieces of the same material and cell size, like around the corner of a house:
// the parts of a cell on both sides are siblings, and the pieces hold together as one block. A piece of another material
// keeps its seam.
static int SeamTest( void )
{
	nbMaterial concrete = nbDefaultMaterial();
	concrete.strength = 1.1e6f;

	for ( int k = 0; k < 2; ++k )
	{
		TestScene scene = CreateScene();

		// An L of two walls, the second stands at the end of the first. The seam is the plane z = 0.15.
		nbPieceDef pieces[2];
		pieces[0] = nbDefaultPieceDef();
		pieces[0].halfExtents = (b3Vec3){ 2.0f, 1.5f, 0.15f };
		pieces[0].transform.p = (b3Vec3){ 0.0f, 1.5f, 0.0f };
		pieces[1] = nbDefaultPieceDef();
		pieces[1].halfExtents = (b3Vec3){ 0.15f, 1.5f, 1.5f };
		pieces[1].transform.p = (b3Vec3){ 1.85f, 1.5f, 1.65f };
		pieces[1].material = k == 0 ? NULL : &concrete;

		nbDestructibleDef def = nbDefaultDestructibleDef();
		def.cellSize = 0.5f;
		nbDestructibleId walls = nbCreateDestructible( scene.world, &def, pieces, 2 );
		ENSURE_SMALL( TotalChunkVolume( walls ) - ( 4.0f * 3.0f * 0.3f + 0.3f * 3.0f * 3.0f ), 1.0e-3f );

		nbWorld* world = nbGetWorldFromId( scene.world );
		int siblingCount = 0;
		int acrossCount = 0;
		int cohesiveAcrossCount = 0;
		for ( int b = 0; b < world->bonds.count; ++b )
		{
			const nbBond* bond = world->bonds.data + b;
			if ( bond->chunk[0] == NB_NULL_INDEX )
			{
				continue;
			}

			float z0 = world->chunks.data[bond->chunk[0]].shape->centroid.z - 0.15f;
			float z1 = world->chunks.data[bond->chunk[1]].shape->centroid.z - 0.15f;
			bool across = z0 * z1 < 0.0f;
			ENSURE( bond->sibling == false || across );
			siblingCount += bond->sibling ? 1 : 0;
			acrossCount += across ? 1 : 0;
			cohesiveAcrossCount += across && bond->cohesive ? 1 : 0;
		}

		ENSURE( acrossCount > 0 );
		if ( k == 0 )
		{
			ENSURE( siblingCount > 0 );
			ENSURE( cohesiveAcrossCount == acrossCount );
		}
		else
		{
			ENSURE( siblingCount == 0 );
			ENSURE( cohesiveAcrossCount == 0 );
		}

		Step( &scene, 30 );
		ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount == 0 );
		DestroyScene( &scene );
	}
	return 0;
}

// After fracture without damage every pair of touching chunks is bonded exactly once and nothing falls.
static int GraphTest( void )
{
	TestScene scene = CreateScene();
	CreateWall( &scene, (b3Vec3){ 3.0f, 1.5f, 0.2f }, 21 );
	nbWorld* world = nbGetWorldFromId( scene.world );

	nbImpactDef impact = { 0 };
	impact.radius = 0.8f;
	impact.damage = 0.0f;

	nbRandom rng = nbMakeRandom( 21, 0 );
	float minBondArea = 0.01f * 0.12f * 0.12f;
	for ( int k = 0; k < 6; ++k )
	{
		float x = nbRandomRange( &rng, -2.5f, 2.5f );
		float y = nbRandomRange( &rng, 0.4f, 2.6f );
		impact.point = (b3Vec3){ x, y, 0.0f };
		nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
		ENSURE( result.detachedChunkCount == 0 );
		ENSURE( result.brokenBondCount == 0 );

		int touching = 0;
		for ( int a = 0; a < world->chunks.count; ++a )
		{
			const nbChunk* chunkA = world->chunks.data + a;
			if ( chunkA->shape == NULL )
			{
				continue;
			}

			for ( int b = a + 1; b < world->chunks.count; ++b )
			{
				const nbChunk* chunkB = world->chunks.data + b;
				if ( chunkB->shape == NULL )
				{
					continue;
				}

				nbBondGeometry geometry;
				float area = nbShape_ContactArea( chunkA->shape, chunkB->shape, 1.0e-4f, false, &geometry );
				if ( area <= minBondArea )
				{
					continue;
				}
				touching += 1;

				int bondCount = 0;
				for ( int key = chunkA->headBondKey; key != NB_NULL_INDEX; )
				{
					const nbBond* bond = world->bonds.data + ( key >> 1 );
					int side = key & 1;
					bondCount += bond->chunk[side ^ 1] == b ? 1 : 0;
					key = bond->nextKey[side];
				}
				ENSURE( bondCount == 1 );
			}
		}
		ENSURE( touching == world->bondCount );
	}

	DestroyScene( &scene );
	return 0;
}

// Debris that comes to rest turns into static rubble. An impact nearby brings it back to life.
static int RubbleTest( void )
{
	TestScene scene = CreateScene();
	CreateWall( &scene, (b3Vec3){ 2.0f, 1.0f, 0.12f }, 21 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.0f, 1.0f, 0.12f };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.6f;
	impact.damage = 1.0e8f;
	impact.ejectSpeed = 2.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( nbWorld_GetStats( scene.world ).debrisCount > 0 );

	Step( &scene, 300 );
	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.rubbleCount > 0 );
	ENSURE( stats.rubbleCount <= stats.debrisCount );

	// Rubble sits on static Box3D bodies
	nbWorld* world = nbGetWorldFromId( scene.world );
	int staticRubble = 0;
	for ( int i = 0; i < world->actors.count; ++i )
	{
		const nbActor* actor = world->actors.data + i;
		if ( actor->isFree == false && actor->isRubble )
		{
			ENSURE( b3Body_GetType( actor->bodyId ) == b3_staticBody );
			staticRubble += 1;
		}
	}
	ENSURE( staticRubble == stats.rubbleCount );

	// A blast on the ground in front of the wall wakes the rubble around it
	impact.point = (b3Vec3){ 0.0f, 0.1f, 0.5f };
	impact.radius = 1.5f;
	impact.damage = 0.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( nbWorld_GetStats( scene.world ).rubbleCount < stats.rubbleCount );

	DestroyScene( &scene );
	return 0;
}

// A dynamic box of the default material resting at a height
static nbDestructibleId CreateLooseBox( TestScene* scene, b3Vec3 position, b3Vec3 halfExtents, uint32_t seed )
{
	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.isStatic = false;
	def.position = position;
	def.seed = seed;
	return nbCreateBox( scene->world, &def, halfExtents );
}

// Mean of the chunk centroids of a destructible in world space, whatever it broke into
static b3Vec3 CenterOfChunks( nbDestructibleId destructible )
{
	int count = nbDestructible_GetChunkCount( destructible );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)( count + 1 ) );
	int written = nbDestructible_GetChunks( destructible, chunks, count );
	b3Vec3 sum = b3Vec3_zero;
	for ( int i = 0; i < written; ++i )
	{
		b3WorldTransform transform = b3Body_GetTransform( nbChunk_GetBody( chunks[i] ) );
		b3Pos centroid = b3TransformWorldPoint( transform, nbChunk_GetCentroid( chunks[i] ) );
		sum = b3Add( sum, b3ToVec3( centroid ) );
	}
	free( chunks );
	return b3MulSV( 1.0f / (float)b3MaxInt( written, 1 ), sum );
}

// The body that carries the most chunks of a destructible
static b3BodyId HeaviestBody( nbDestructibleId destructible )
{
	nbChunkId chunks[1024];
	int count = nbDestructible_GetChunks( destructible, chunks, 1024 );
	b3BodyId best = b3_nullBodyId;
	int bestCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		b3BodyId body = nbChunk_GetBody( chunks[i] );
		int n = 0;
		for ( int j = 0; j < count; ++j )
		{
			n += B3_ID_EQUALS( nbChunk_GetBody( chunks[j] ), body ) ? 1 : 0;
		}
		if ( n > bestCount )
		{
			best = body;
			bestCount = n;
		}
	}
	return best;
}

// Debris freezes into rubble where something fixed carries it, piles from the bottom up, and rubble follows when what it
// rests on moves away
static float FloatingVolume( TestScene* scene );
static int RestTest( void )
{
	// A stack on the ground freezes as a whole
	{
		TestScene scene = CreateScene();
		for ( int i = 0; i < 3; ++i )
		{
			b3Vec3 position = { 0.0f, 0.25f + 0.5f * (float)i, 0.0f };
			b3Vec3 halfExtents = { 0.5f - 0.1f * (float)i, 0.25f, 0.5f };
			CreateLooseBox( &scene, position, halfExtents, 40 + i );
		}
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 3 );
		DestroyScene( &scene );
	}

	// A box on a platform that can move does not freeze, and rides along when the platform moves
	{
		TestScene scene = CreateScene();
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_kinematicBody;
		bodyDef.position = (b3Vec3){ 0.0f, 1.0f, 0.0f };
		b3BodyId platform = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3BoxHull hull = b3MakeBoxHull( 2.0f, 0.1f, 2.0f );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3CreateHullShape( platform, &shapeDef, &hull.base );
		nbDestructibleId box = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 1.35f, 0.0f }, (b3Vec3){ 0.25f, 0.25f, 0.25f }, 44 );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 0 );

		b3Body_SetLinearVelocity( platform, (b3Vec3){ 0.5f, 0.0f, 0.0f } );
		Step( &scene, 60 );
		nbChunkId chunk;
		nbDestructible_GetChunks( box, &chunk, 1 );
		ENSURE( b3Body_GetPosition( nbChunk_GetBody( chunk ) ).x > 0.3f );
		DestroyScene( &scene );
	}

	// A heavy ball knocks the bottom of a frozen stack away, and the box on top comes along instead of hanging in the air
	{
		TestScene scene = CreateScene();
		CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.1f, 0.0f }, (b3Vec3){ 0.5f, 0.1f, 0.5f }, 45 );
		nbDestructibleId top = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, (b3Vec3){ 0.3f, 0.3f, 0.3f }, 46 );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );
		b3Vec3 start = CenterOfChunks( top );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = (b3Vec3){ -3.0f, 0.1f, 0.0f };
		bodyDef.linearVelocity = (b3Vec3){ 12.0f, 0.0f, 0.0f };
		b3BodyId ball = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 7800.0f;
		shapeDef.enableHitEvents = true;
		b3Sphere sphere = { { 0.0f, 0.0f, 0.0f }, 0.25f };
		b3CreateSphereShape( ball, &shapeDef, &sphere );

		Step( &scene, 90 );
		ENSURE( b3Distance( CenterOfChunks( top ), start ) > 0.3f );
		DestroyScene( &scene );
	}

	// An impact on heavy rubble leaves it frozen and only breaks pieces off, unless they came from below its center of
	// mass, see the next case. Light rubble in reach comes back to life and flies.
	{
		TestScene scene = CreateScene();
		nbDestructibleId heavy = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, (b3Vec3){ 1.0f, 0.5f, 1.0f }, 49 );
		CreateLooseBox( &scene, (b3Vec3){ 0.3f, 1.1f, 0.0f }, (b3Vec3){ 0.1f, 0.1f, 0.1f }, 50 );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );

		// The box around the impact reaches a corner of the heavy box, its sphere does not
		nbImpactDef impact = { 0 };
		impact.point = (b3Pos){ 1.4f, 1.4f, 1.4f };
		impact.radius = 0.5f;
		impact.damage = 3.0e5f;
		impact.ejectSpeed = 12.0f;
		nbWorld_ApplyImpact( scene.world, &impact );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );
		ENSURE( nbDestructible_GetChunkCount( heavy ) == 1 );

		impact.point = (b3Pos){ 0.0f, 1.0f, 0.0f };
		nbWorld_ApplyImpact( scene.world, &impact );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 1 );
		ENSURE( nbDestructible_GetChunkCount( heavy ) > 1 );

		nbChunkId chunks[256];
		int count = nbDestructible_GetChunks( heavy, chunks, 256 );
		int frozen = 0;
		for ( int i = 0; i < count; ++i )
		{
			frozen += b3Body_GetType( nbChunk_GetBody( chunks[i] ) ) == b3_staticBody ? 1 : 0;
		}
		ENSURE( frozen > count / 2 );
		DestroyScene( &scene );
	}

	// Heavy rubble that an impact breaks pieces off below its center of mass may have lost what carries it. It comes back
	// to life and comes down instead of hanging in the air. How far depends on what the impact leaves below it: over ten
	// seeds the heaviest piece ends between 6 cm higher and 20 cm lower than it was, carried every time.
	{
		TestScene scene = CreateScene();
		CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, (b3Vec3){ 1.0f, 0.5f, 1.0f }, 53 );
		nbDestructibleId upper = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 1.5f, 0.0f }, (b3Vec3){ 0.5f, 0.5f, 0.5f }, 54 );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );

		nbImpactDef impact = { 0 };
		impact.point = (b3Pos){ 0.0f, 1.0f, 0.0f };
		impact.radius = 0.8f;
		impact.damage = 1.0e7f;
		impact.ejectSpeed = 12.0f;
		nbWorld_ApplyImpact( scene.world, &impact );
		Step( &scene, 1 );
		ENSURE( b3Body_GetType( HeaviestBody( upper ) ) == b3_dynamicBody );
		Step( &scene, 89 );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// Rubble that comes back to life but is still carried freezes again after a few steps, not the whole rest time
	{
		TestScene scene = CreateScene();
		CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.1f, 0.0f }, (b3Vec3){ 0.5f, 0.1f, 0.5f }, 51 );
		nbDestructibleId top = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, (b3Vec3){ 0.3f, 0.3f, 0.3f }, 52 );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );

		nbWorld* world = nbGetWorldFromId( scene.world );
		nbChunkId chunk;
		nbDestructible_GetChunks( top, &chunk, 1 );
		nbThawActor( world, world->chunks.data[chunk.index1 - 1].actorIndex );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 1 );
		Step( &scene, 5 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );
		DestroyScene( &scene );
	}

	// Only the bottom of a frozen stack comes back to life and slides away. The box on top follows once its support moved,
	// and falls to the ground.
	{
		TestScene scene = CreateScene();
		nbDestructibleId bottom = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.1f, 0.0f }, (b3Vec3){ 0.5f, 0.1f, 0.5f }, 47 );
		nbDestructibleId top = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, (b3Vec3){ 0.3f, 0.3f, 0.3f }, 48 );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );
		b3Vec3 start = CenterOfChunks( top );

		nbWorld* world = nbGetWorldFromId( scene.world );
		nbChunkId chunk;
		nbDestructible_GetChunks( bottom, &chunk, 1 );
		int actorIndex = world->chunks.data[chunk.index1 - 1].actorIndex;
		nbThawActor( world, actorIndex );
		b3Body_SetLinearVelocity( world->actors.data[actorIndex].bodyId, (b3Vec3){ 8.0f, 0.0f, 0.0f } );
		ENSURE( world->actors.data[world->chunks.data[chunk.index1 - 1].actorIndex].isRubble == false );

		Step( &scene, 90 );
		ENSURE( CenterOfChunks( top ).y < start.y - 0.1f );
		DestroyScene( &scene );
	}

	return 0;
}

// A strong slab of eight pieces in a row, dropped from a height
static nbDestructibleId CreateFallingSlab( TestScene* scene, float height, float halfDepth )
{
	nbPieceDef pieces[8];
	for ( int i = 0; i < 8; ++i )
	{
		pieces[i] = nbDefaultPieceDef();
		pieces[i].halfExtents = (b3Vec3){ 0.25f, 0.15f, halfDepth };
		pieces[i].transform.p = (b3Vec3){ -1.75f + 0.5f * (float)i, 0.0f, 0.0f };
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.isStatic = false;
	def.position = (b3Vec3){ 0.0f, height, 0.0f };
	def.material.strength = 1.0e9f;
	return nbCreateDestructible( scene->world, &def, pieces, 8 );
}

// Distinct bodies among the chunks of a destructible
static int CountBodies( nbDestructibleId destructible )
{
	nbChunkId chunks[1024];
	int count = nbDestructible_GetChunks( destructible, chunks, 1024 );
	int bodies = 0;
	for ( int i = 0; i < count; ++i )
	{
		b3BodyId body = nbChunk_GetBody( chunks[i] );
		bool seen = false;
		for ( int j = 0; j < i && seen == false; ++j )
		{
			seen = B3_ID_EQUALS( nbChunk_GetBody( chunks[j] ), body );
		}
		bodies += seen ? 0 : 1;
	}
	return bodies;
}

// A large part that lands hard stays whole. What falls keeps its shape, it does not break in halves.
static int LandingTest( void )
{
	TestScene scene = CreateScene();
	nbDestructibleId slab = CreateFallingSlab( &scene, 4.0f, 1.0f );
	Step( &scene, 90 );
	ENSURE( CountBodies( slab ) == 1 );
	DestroyScene( &scene );
	return 0;
}

// A heavy ball breaks through a wall instead of bouncing off it
static int CannonballTest( void )
{
	TestScene scene = CreateScene();
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 3.0f, 1.5f, 0.15f }, 17 );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = (b3Vec3){ 0.0f, 1.4f, 6.0f };
	bodyDef.linearVelocity = (b3Vec3){ 0.0f, 0.0f, -45.0f };
	bodyDef.isBullet = true;
	b3BodyId ball = b3CreateBody( scene.physicsWorld, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = 7800.0f;
	shapeDef.enableHitEvents = true;
	b3Sphere sphere = { { 0.0f, 0.0f, 0.0f }, 0.25f };
	b3CreateSphereShape( ball, &shapeDef, &sphere );

	Step( &scene, 30 );

	ENSURE( nbDestructible_GetChunkCount( wall ) > 20 );
	ENSURE( b3Body_GetPosition( ball ).z < -0.5f );

	DestroyScene( &scene );
	return 0;
}

// Box3D sweeps debris against static shapes only once it could pass through a wall within one step: small pieces later
// than Box3D would by default, which spares most sweeps, large pieces as Box3D does. Shot at a wall 20 cm thick at up to
// 40 m/s, from starting points spread over a step, no piece passes through. Swept at twice their inner radius whatever
// their size, large pieces could pass through walls thinner than their step.
static int SweepTest( void )
{
	float halfWidths[] = { 0.03f, 0.1f, 0.15f, 0.25f };
	float factors[] = { 2.0f, 2.0f, 0.2f / 0.15f, 0.8f };
	float speeds[] = { 9.0f, 15.0f, 18.0f, 22.0f, 27.0f, 40.0f };
	enum
	{
		speedCount = 6,
		phaseCount = 20,
		pieceCount = speedCount * phaseCount,
	};
	for ( int h = 0; h < 4; ++h )
	{
		TestScene scene = CreateScene();
		nbDestructibleDef wallDef = nbDefaultDestructibleDef();
		wallDef.position = (b3Vec3){ 0.0f, 1.5f, 0.0f };
		nbCreateBox( scene.world, &wallDef, (b3Vec3){ 50.0f, 1.5f, 0.1f } );

		float half = halfWidths[h];
		b3BodyId bodies[pieceCount];
		for ( int i = 0; i < pieceCount; ++i )
		{
			nbDestructibleDef def = nbDefaultDestructibleDef();
			def.isStatic = false;
			def.enableCollisionDamage = false;
			def.position = (b3Vec3){ -48.0f + 0.8f * (float)i, 1.5f, 1.0f + 0.015f * (float)( i % phaseCount ) };
			nbDestructibleId piece = nbCreateBox( scene.world, &def, (b3Vec3){ half, half, half } );
			nbChunkId chunk;
			ENSURE( nbDestructible_GetChunks( piece, &chunk, 1 ) == 1 );
			bodies[i] = nbChunk_GetBody( chunk );
		}

		Step( &scene, 1 );
		for ( int i = 0; i < pieceCount; ++i )
		{
			ENSURE( fabsf( b3Body_GetSafetyFactor( bodies[i] ) - factors[h] ) < 1.0e-3f );
			b3Body_SetLinearVelocity( bodies[i], (b3Vec3){ 0.0f, 0.0f, -speeds[i / phaseCount] } );
		}

		Step( &scene, 30 );
		for ( int i = 0; i < pieceCount; ++i )
		{
			ENSURE( b3Body_GetPosition( bodies[i] ).z > 0.0f );
		}
		DestroyScene( &scene );
	}
	return 0;
}

// Debris freezes once it kept still for the rest time, which can change at run time. A box set down on the ground freezes
// after six updates with the default of 0.1 s, and stays awake that long with 0.5 s.
static int RestTimeTest( void )
{
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateScene();
		if ( pass == 1 )
		{
			nbWorld_SetDebrisRestTime( scene.world, 0.5f );
		}
		CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.25f, 0.0f }, (b3Vec3){ 0.25f, 0.25f, 0.25f }, 50 );
		Step( &scene, 10 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == ( pass == 0 ? 1 : 0 ) );
		Step( &scene, 30 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 1 );
		DestroyScene( &scene );
	}

	// At most 1 s
	TestScene scene = CreateScene();
	nbWorld_SetDebrisRestTime( scene.world, 5.0f );
	ENSURE( nbGetWorldFromId( scene.world )->def.debrisRestTime == 1.0f );
	DestroyScene( &scene );
	return 0;
}

// Lowering the debris budget at run time freezes the moving debris over it in the next update, and nothing is removed
static int DebrisBudgetTest( void )
{
	TestScene scene = CreateScene();
	CreateWall( &scene, (b3Vec3){ 2.0f, 1.0f, 0.12f }, 23 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.0f, 1.0f, 0.12f };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.6f;
	impact.damage = 1.0e8f;
	impact.ejectSpeed = 2.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	Step( &scene, 2 );

	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.debrisCount - stats.rubbleCount > 10 );

	// Debris gets a quarter second to fly before the budget may freeze it
	nbWorld_SetDebrisBudget( scene.world, 5 );
	Step( &scene, 1 );
	nbStats young = nbWorld_GetStats( scene.world );
	ENSURE( young.debrisCount == stats.debrisCount );
	Step( &scene, 30 );
	nbStats old = nbWorld_GetStats( scene.world );
	ENSURE( old.debrisCount - old.rubbleCount <= 5 );
	ENSURE( old.debrisCount == stats.debrisCount );

	DestroyScene( &scene );
	return 0;
}

// Larger fragments mean fewer pieces for the same impact
static int FragmentScaleTest( void )
{
	int created[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, pass == 0 ? 1.0f : 2.0f );
		CreateWall( &scene, (b3Vec3){ 2.0f, 1.5f, 0.2f }, 31 );

		nbImpactDef impact = { 0 };
		impact.point = (b3Vec3){ 0.0f, 1.5f, 0.2f };
		impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
		impact.radius = 0.8f;
		impact.damage = 1.0e6f;
		created[pass] = nbWorld_ApplyImpact( scene.world, &impact ).createdChunkCount;
		DestroyScene( &scene );
	}

	ENSURE( created[0] > 20 );
	ENSURE( 2 * created[1] < created[0] );
	return 0;
}

// Visibility of the faces of a chunk, one bit per face
static uint64_t VisibleFaceMask( nbChunkId id )
{
	bool visible[128];
	int count = nbChunk_GetVisibleFaces( id, visible, 128 );
	uint64_t mask = 0;
	for ( int f = 0; f < count && f < 64; ++f )
	{
		mask |= visible[f] ? (uint64_t)1 << f : 0;
	}
	return mask;
}

// Faces between glued chunks are hidden, and a chunk whose faces show up again is reported as exposed
static int VisibleFaceTest( void )
{
	TestScene scene = CreateScene();
	nbDestructibleDef def = nbDefaultDestructibleDef();
	b3Vec3 halfExtents = { 2.0f, 1.5f, 0.2f };
	def.position = (b3Vec3){ 0.0f, halfExtents.y, 0.0f };
	def.cellSize = 0.35f;
	def.seed = 5;
	nbDestructibleId wall = nbCreateBox( scene.world, &def, halfExtents );
	nbWorld_GetEvents( scene.world );

	int capacity = 4096;
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)capacity );
	uint64_t* masks = calloc( (size_t)capacity, sizeof( uint64_t ) );
	uint16_t* generations = calloc( (size_t)capacity, sizeof( uint16_t ) );
	int count = nbDestructible_GetChunks( wall, chunks, capacity );
	ENSURE( count > 50 && count < capacity );

	// A hidden face lies inside the wall, never on its outside
	int hiddenCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		nbGeometry geometry = nbChunk_GetGeometry( chunks[i] );
		bool visible[128];
		ENSURE( nbChunk_GetVisibleFaces( chunks[i], visible, 128 ) == geometry.faceCount );
		for ( int f = 0; f < geometry.faceCount; ++f )
		{
			if ( visible[f] )
			{
				continue;
			}

			hiddenCount += 1;
			const nbFace* face = geometry.faces + f;
			b3Vec3 center = b3Vec3_zero;
			for ( int k = 0; k < face->indexCount; ++k )
			{
				center = b3Add( center, geometry.vertices[geometry.indices[face->firstIndex + k]] );
			}
			center = b3MulSV( 1.0f / (float)face->indexCount, center );
			ENSURE( b3AbsFloat( center.x ) < halfExtents.x - 1.0e-3f );
			ENSURE( b3AbsFloat( center.y ) < halfExtents.y - 1.0e-3f );
			ENSURE( b3AbsFloat( center.z ) < halfExtents.z - 1.0e-3f );
		}

		ENSURE( chunks[i].index1 < capacity );
		masks[chunks[i].index1] = VisibleFaceMask( chunks[i] );
		generations[chunks[i].index1] = chunks[i].generation;
	}
	ENSURE( hiddenCount > 2 * count );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.3f, 1.4f, 0.2f };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.5f;
	impact.damage = 1.0e6f;
	impact.ejectSpeed = 4.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	Step( &scene, 5 );

	// Every surviving chunk whose visible faces changed was reported
	nbEvents events = nbWorld_GetEvents( scene.world );
	ENSURE( events.exposedCount > 0 );
	// A chunk is reported once. Its slot may be reused by a new chunk, which is reported on its own.
	bool* reported = calloc( (size_t)capacity, sizeof( bool ) );
	for ( int i = 0; i < events.exposedCount; ++i )
	{
		nbChunkId id = events.exposedChunks[i];
		ENSURE( id.index1 < capacity );
		for ( int j = 0; j < i; ++j )
		{
			ENSURE( NB_ID_EQUALS( id, events.exposedChunks[j] ) == false );
		}
		reported[id.index1] = reported[id.index1] || id.generation == generations[id.index1];
	}

	int changedCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		if ( nbChunk_IsValid( chunks[i] ) == false )
		{
			continue;
		}

		bool changed = VisibleFaceMask( chunks[i] ) != masks[chunks[i].index1];
		changedCount += changed ? 1 : 0;
		ENSURE( changed == false || reported[chunks[i].index1] );
	}
	ENSURE( changedCount > 0 );

	free( reported );
	free( generations );
	free( masks );
	free( chunks );
	DestroyScene( &scene );
	return 0;
}

// Brick walls and concrete floors of 9 x 6.5 m with stories of 3 m. The wall mask selects the walls: 1 the left end,
// 2 the right end, 4 the front, 8 the back. A column half width above zero puts the ground floor on four columns.
static nbDestructibleId CreateHouse( TestScene* scene, int stories, int walls, float columnHalfWidth )
{
	nbMaterial brick = nbDefaultMaterial();
	brick.density = 1900.0f;
	brick.strength = 6.0e5f;
	nbMaterial concrete = nbDefaultMaterial();
	concrete.density = 2400.0f;
	concrete.strength = 1.1e6f;

	const float width = 9.0f, depth = 6.5f, height = 3.0f, wall = 0.3f, floor = 0.25f;
	nbPieceDef pieces[40];
	int count = 0;
	for ( int story = 0; story < stories; ++story )
	{
		float y = (float)story * ( height + floor );
		if ( story == 0 && columnHalfWidth > 0.0f )
		{
			for ( int c = 0; c < 4; ++c )
			{
				nbPieceDef* piece = pieces + count++;
				*piece = nbDefaultPieceDef();
				piece->halfExtents = (b3Vec3){ columnHalfWidth, 0.5f * height, columnHalfWidth };
				piece->transform.p = (b3Vec3){ ( c & 1 ? 1.0f : -1.0f ) * ( 0.5f * width - columnHalfWidth ), y + 0.5f * height,
											   ( c & 2 ? 1.0f : -1.0f ) * ( 0.5f * depth - columnHalfWidth ) };
			}
		}
		else
		{
			for ( int side = 0; side < 2; ++side )
			{
				float sign = side == 0 ? -1.0f : 1.0f;
				if ( walls & ( 1 << side ) )
				{
					nbPieceDef* end = pieces + count++;
					*end = nbDefaultPieceDef();
					end->halfExtents = (b3Vec3){ 0.5f * wall, 0.5f * height, 0.5f * depth };
					end->transform.p = (b3Vec3){ sign * 0.5f * ( width - wall ), y + 0.5f * height, 0.0f };
				}
				if ( walls & ( 4 << side ) )
				{
					nbPieceDef* front = pieces + count++;
					*front = nbDefaultPieceDef();
					front->halfExtents = (b3Vec3){ 0.5f * width - wall, 0.5f * height, 0.5f * wall };
					front->transform.p = (b3Vec3){ 0.0f, y + 0.5f * height, -sign * 0.5f * ( depth - wall ) };
				}
			}
		}

		nbPieceDef* slab = pieces + count++;
		*slab = nbDefaultPieceDef();
		slab->halfExtents = (b3Vec3){ 0.5f * width, 0.5f * floor, 0.5f * depth };
		slab->transform.p = (b3Vec3){ 0.0f, y + height + 0.5f * floor, 0.0f };
		slab->material = &concrete;
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.material = brick;
	return nbCreateDestructible( scene->world, &def, pieces, count );
}

// A wall of box pieces cut on the grid of the edges of its openings, the way the demo builds its houses
static void AddWallWithOpenings( nbPieceDef* pieces, int* pieceCount, b3Vec3 origin, bool alongX, float width, float height,
								 float thickness, const float* openings, int openingCount )
{
	float xs[16], ys[16];
	int nx = 0, ny = 0;
	xs[nx++] = 0.0f;
	xs[nx++] = width;
	ys[ny++] = 0.0f;
	ys[ny++] = height;
	for ( int i = 0; i < openingCount; ++i )
	{
		xs[nx++] = openings[4 * i + 0];
		xs[nx++] = openings[4 * i + 2];
		ys[ny++] = openings[4 * i + 1];
		ys[ny++] = openings[4 * i + 3];
	}

	for ( int pass = 0; pass < 2; ++pass )
	{
		float* values = pass == 0 ? xs : ys;
		int count = pass == 0 ? nx : ny;
		for ( int i = 1; i < count; ++i )
		{
			float key = values[i];
			int j = i - 1;
			while ( j >= 0 && values[j] > key )
			{
				values[j + 1] = values[j];
				j -= 1;
			}
			values[j + 1] = key;
		}
	}

	for ( int j = 0; j + 1 < ny; ++j )
	{
		for ( int i = 0; i + 1 < nx; ++i )
		{
			float x0 = xs[i], x1 = xs[i + 1], y0 = ys[j], y1 = ys[j + 1];
			if ( x1 - x0 < 1.0e-3f || y1 - y0 < 1.0e-3f )
			{
				continue;
			}

			float cx = 0.5f * ( x0 + x1 ), cy = 0.5f * ( y0 + y1 );
			bool inOpening = false;
			for ( int k = 0; k < openingCount; ++k )
			{
				const float* o = openings + 4 * k;
				inOpening = inOpening || ( o[0] < cx && cx < o[2] && o[1] < cy && cy < o[3] );
			}
			if ( inOpening )
			{
				continue;
			}

			nbPieceDef* piece = pieces + ( *pieceCount )++;
			*piece = nbDefaultPieceDef();
			piece->halfExtents = alongX ? (b3Vec3){ 0.5f * ( x1 - x0 ), 0.5f * ( y1 - y0 ), 0.5f * thickness }
										: (b3Vec3){ 0.5f * thickness, 0.5f * ( y1 - y0 ), 0.5f * ( x1 - x0 ) };
			piece->transform.p = alongX ? (b3Vec3){ origin.x + cx, origin.y + cy, origin.z }
										: (b3Vec3){ origin.x, origin.y + cy, origin.z + cx };
		}
	}
}

// A town house of the demo: brick walls with doors and windows on concrete floors
static nbDestructibleId CreateTownHouse( TestScene* scene, int floors )
{
	nbMaterial concrete = nbDefaultMaterial();
	concrete.strength = 1.1e6f;
	nbPieceDef pieces[256];
	int count = 0;
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;
	for ( int floor = 0; floor < floors; ++floor )
	{
		float y = (float)floor * ( story + slab );
		float front0[] = { 3.9f, 0.0f, 5.1f, 2.2f, 1.0f, 1.0f, 2.6f, 2.2f, 6.4f, 1.0f, 8.0f, 2.2f };
		float front1[] = { 1.0f, 0.9f, 2.6f, 2.2f, 3.7f, 0.9f, 5.3f, 2.2f, 6.4f, 0.9f, 8.0f, 2.2f };
		float back[] = { 1.5f, 0.9f, 3.0f, 2.2f, 6.0f, 0.9f, 7.5f, 2.2f };
		float side[] = { 2.4f, 0.9f, 3.6f, 2.2f };
		float x0 = -0.5f * width, z0 = -0.5f * depth, inner = depth - 2.0f * t;
		const float* front = floor == 0 ? front0 : front1;
		AddWallWithOpenings( pieces, &count, (b3Vec3){ x0, y, -z0 - 0.5f * t }, true, width, story, t, front, 3 );
		AddWallWithOpenings( pieces, &count, (b3Vec3){ x0, y, z0 + 0.5f * t }, true, width, story, t, back, 2 );
		AddWallWithOpenings( pieces, &count, (b3Vec3){ x0 + 0.5f * t, y, z0 + t }, false, inner, story, t, side, 1 );
		AddWallWithOpenings( pieces, &count, (b3Vec3){ -x0 - 0.5f * t, y, z0 + t }, false, inner, story, t, side, 1 );

		nbPieceDef* floorSlab = pieces + count++;
		*floorSlab = nbDefaultPieceDef();
		floorSlab->halfExtents = (b3Vec3){ 0.5f * width, 0.5f * slab, 0.5f * depth };
		floorSlab->transform.p = (b3Vec3){ 0.0f, y + story + 0.5f * slab, 0.0f };
		floorSlab->material = &concrete;
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.material.density = 1900.0f;
	def.material.strength = 6.0e5f;
	return nbCreateDestructible( scene->world, &def, pieces, count );
}

// Cut the doors and windows of one floor out of a wall that runs through all floors. The corners are pairs along the
// wall and up from the floor. A door reaches below the wall, so it opens it to the ground.
static void AddOpenings( nbOpening* openings, int* openingCount, const float* corners, int cornerCount, float length,
						 float height, float floorY, float thickness )
{
	for ( int i = 0; i + 3 < cornerCount; i += 4 )
	{
		float x0 = corners[i], x1 = corners[i + 2];
		float y0 = corners[i + 1] > 0.0f ? floorY + corners[i + 1] : floorY - 0.1f;
		float y1 = floorY + corners[i + 3];
		nbOpening* opening = openings + ( *openingCount )++;
		opening->center = (b3Vec3){ 0.5f * ( x0 + x1 - length ), 0.5f * ( y0 + y1 - height ), 0.0f };
		opening->halfExtents = (b3Vec3){ 0.5f * ( x1 - x0 ), 0.5f * ( y1 - y0 ), thickness };
	}
}

// The town house as the demo builds it: brick walls through all floors with the same doors and windows, pre-fractured
// into cells, and whole concrete floors between them
// The pieces of a celled house with what they point to, so a batch can hold many of them
typedef struct CelledHouse
{
	nbMaterial concrete;
	nbOpening openings[4][16];
	nbPieceDef pieces[8];
	int pieceCount;
	nbDestructibleDef def;
} CelledHouse;

static void BuildCelledHouse( CelledHouse* house, int floors, uint32_t seed, b3Vec3 position )
{
	house->concrete = nbDefaultMaterial();
	house->concrete.strength = 1.1e6f;
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;
	float level = story + slab;
	float height = (float)floors * level;
	float inner = depth - 2.0f * t;

	int openingCounts[4] = { 0 };
	for ( int floor = 0; floor < floors; ++floor )
	{
		float y = (float)floor * level;
		float front0[] = { 3.9f, 0.0f, 5.1f, 2.2f, 1.0f, 1.0f, 2.6f, 2.2f, 6.4f, 1.0f, 8.0f, 2.2f };
		float front1[] = { 1.0f, 0.9f, 2.6f, 2.2f, 3.7f, 0.9f, 5.3f, 2.2f, 6.4f, 0.9f, 8.0f, 2.2f };
		float back[] = { 1.5f, 0.9f, 3.0f, 2.2f, 6.0f, 0.9f, 7.5f, 2.2f };
		float side[] = { 2.4f, 0.9f, 3.6f, 2.2f };
		AddOpenings( house->openings[0], openingCounts + 0, floor == 0 ? front0 : front1, 12, width, height, y, t );
		AddOpenings( house->openings[1], openingCounts + 1, back, 8, width, height, y, t );
		AddOpenings( house->openings[2], openingCounts + 2, side, 4, inner, height, y, t );
		AddOpenings( house->openings[3], openingCounts + 3, side, 4, inner, height, y, t );
	}

	// The front and back walls run along x, the end walls between them along z
	nbPieceDef* pieces = house->pieces;
	int count = 0;
	b3Quat alongZ = b3MakeQuatFromAxisAngle( (b3Vec3){ 0.0f, 1.0f, 0.0f }, -0.5f * B3_PI );
	b3Vec3 centers[4] = {
		{ 0.0f, 0.5f * height, 0.5f * ( depth - t ) },
		{ 0.0f, 0.5f * height, -0.5f * ( depth - t ) },
		{ -0.5f * ( width - t ), 0.5f * height, 0.0f },
		{ 0.5f * ( width - t ), 0.5f * height, 0.0f },
	};
	for ( int w = 0; w < 4; ++w )
	{
		nbPieceDef* wall = pieces + count++;
		*wall = nbDefaultPieceDef();
		wall->halfExtents = (b3Vec3){ 0.5f * ( w < 2 ? width : inner ), 0.5f * height, 0.5f * t };
		wall->transform.p = centers[w];
		wall->transform.q = w < 2 ? b3Quat_identity : alongZ;
		wall->openings = house->openings[w];
		wall->openingCount = openingCounts[w];
	}

	for ( int floor = 0; floor < floors; ++floor )
	{
		nbPieceDef* floorSlab = pieces + count++;
		*floorSlab = nbDefaultPieceDef();
		floorSlab->halfExtents = (b3Vec3){ 0.5f * width - t, 0.5f * slab, 0.5f * depth - t };
		floorSlab->transform.p = (b3Vec3){ 0.0f, (float)floor * level + story + 0.5f * slab, 0.0f };
		floorSlab->material = &house->concrete;
		floorSlab->cellSize = -1.0f;
	}
	house->pieceCount = count;

	house->def = nbDefaultDestructibleDef();
	house->def.position = b3ToPos( position );
	house->def.seed = seed;
	house->def.cellSize = 1.2f;
	house->def.material.density = 1900.0f;
	house->def.material.strength = 6.0e5f;
}

// Every storey of the buildings in the scene gives way at the same damage and keeps the same part of its walls, the top
// one as well
static void SetStoreyCollapse( TestScene* scene, float damage, float residue )
{
	nbStoreyCollapse collapse = { damage, damage, residue, residue };
	nbWorld_SetStoreyCollapse( scene->world, collapse, collapse );
}

static nbDestructibleId CreateCelledHouse( TestScene* scene, int floors, uint32_t seed )
{
	CelledHouse* house = malloc( sizeof( CelledHouse ) );
	BuildCelledHouse( house, floors, seed, b3Vec3_zero );
	nbDestructibleId id = nbCreateDestructible( scene->world, &house->def, house->pieces, house->pieceCount );
	free( house );
	return id;
}

// Highest chunk centroid in world space
static float HighestChunk( nbDestructibleId destructible )
{
	int count = nbDestructible_GetChunkCount( destructible );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)( count + 1 ) );
	int written = nbDestructible_GetChunks( destructible, chunks, count );
	float highest = -FLT_MAX;
	for ( int i = 0; i < written; ++i )
	{
		b3WorldTransform transform = b3Body_GetTransform( nbChunk_GetBody( chunks[i] ) );
		b3Pos centroid = b3TransformWorldPoint( transform, nbChunk_GetCentroid( chunks[i] ) );
		highest = b3MaxFloat( highest, (float)centroid.y );
	}
	free( chunks );
	return highest;
}

// Highest chunk centroid of the structures that still stand
static float HighestStanding( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	float highest = 0.0f;
	for ( int c = 0; c < world->chunks.count; ++c )
	{
		const nbChunk* chunk = world->chunks.data + c;
		const nbActor* actor = world->actors.data + chunk->actorIndex;
		if ( chunk->shape != NULL && actor->isStatic )
		{
			b3WorldTransform transform = nbActor_GetTransform( world, actor );
			highest = b3MaxFloat( highest, (float)b3TransformWorldPoint( transform, chunk->shape->centroid ).y );
		}
	}
	return highest;
}

// The volume of the largest loose piece, and how low and how high it reaches
static float LargestPiece( TestScene* scene, float* low, float* high )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	float largest = 0.0f;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		const nbActor* actor = world->actors.data + world->debris.data[i];
		if ( actor->volume <= largest )
		{
			continue;
		}

		largest = actor->volume;
		*low = FLT_MAX;
		*high = -FLT_MAX;
		for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			b3AABB box = b3Shape_GetAABB( world->chunks.data[c].shapeId );
			*low = b3MinFloat( *low, box.lowerBound.y );
			*high = b3MaxFloat( *high, box.upperBound.y );
		}
	}
	return largest;
}

// A structure gives way where it cannot carry its weight: under too much pressure, and where an overhang bends its bonds
typedef struct FloatQuery
{
	nbWorld* world;
	int actorIndex;
	bool found;
} FloatQuery;

static bool FloatQueryCallback( b3ShapeId shapeId, void* context )
{
	FloatQuery* query = context;
	int chunkIndex = nbFindChunkFromShape( query->world, shapeId );
	if ( chunkIndex == NB_NULL_INDEX || query->world->chunks.data[chunkIndex].actorIndex != query->actorIndex )
	{
		// Box3D goes on with its next tree after the callback returns false, so the answer has to stick. Otherwise a
		// piece on rubble, which Box3D keeps in its static tree, is not carried once the piece itself turns up in the
		// dynamic tree.
		query->found = true;
		return false;
	}
	return true;
}

// Whether a contact pushes the body up from below its center of mass
static bool CarriesFromBelow( const b3ContactData* contact, b3BodyId bodyId, float centerHeight )
{
	bool isA = B3_ID_EQUALS( b3Shape_GetBody( contact->shapeIdA ), bodyId );
	b3Pos centerA = b3Body_GetWorldCenter( b3Shape_GetBody( contact->shapeIdA ) );
	for ( int m = 0; m < contact->manifoldCount; ++m )
	{
		const b3Manifold* manifold = contact->manifolds + m;
		float up = isA ? -manifold->normal.y : manifold->normal.y;
		for ( int k = 0; k < manifold->pointCount; ++k )
		{
			const b3ManifoldPoint* point = manifold->points + k;
			if ( up > 0.3f && point->totalNormalImpulse > 0.0f && (float)centerA.y + point->anchorA.y < centerHeight )
			{
				return true;
			}
		}
	}
	return false;
}

// The actor of a body, NB_NULL_INDEX for a body that is none
static int FindBodyActor( const nbWorld* world, b3BodyId bodyId )
{
	int index = bodyId.index1 - 1;
	int actorIndex = index >= 0 && index < world->bodyToActor.count ? world->bodyToActor.data[index] : NB_NULL_INDEX;
	if ( actorIndex == NB_NULL_INDEX || world->actors.data[actorIndex].isFree ||
		 B3_ID_EQUALS( world->actors.data[actorIndex].bodyId, bodyId ) == false )
	{
		return NB_NULL_INDEX;
	}
	return actorIndex;
}

// The moving pieces that contacts carry from below, through other moving pieces, down to something that does not move
static bool* FindCarriedPieces( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	bool* carried = calloc( (size_t)world->actors.count + 1, sizeof( bool ) );
	for ( bool changed = true; changed; )
	{
		changed = false;
		for ( int i = 0; i < world->debris.count; ++i )
		{
			int actorIndex = world->debris.data[i];
			b3BodyId bodyId = world->actors.data[actorIndex].bodyId;
			int capacity = b3Body_GetContactCapacity( bodyId );
			if ( carried[actorIndex] || b3Body_GetType( bodyId ) != b3_dynamicBody || capacity == 0 )
			{
				continue;
			}

			float centerHeight = (float)b3Body_GetWorldCenter( bodyId ).y;
			b3ContactData* contacts = malloc( sizeof( b3ContactData ) * (size_t)capacity );
			int contactCount = b3Body_GetContactData( bodyId, contacts, capacity );
			for ( int c = 0; c < contactCount && carried[actorIndex] == false; ++c )
			{
				bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), bodyId );
				b3BodyId otherId = b3Shape_GetBody( isA ? contacts[c].shapeIdB : contacts[c].shapeIdA );
				int other = FindBodyActor( world, otherId );
				bool fixed = b3Body_GetType( otherId ) != b3_dynamicBody || ( other != NB_NULL_INDEX && carried[other] );
				carried[actorIndex] = fixed && CarriesFromBelow( contacts + c, bodyId, centerHeight );
			}
			free( contacts );
			changed = changed || carried[actorIndex];
		}
	}
	return carried;
}

// Volume of the pieces of at least a cubic meter that hang in the air: nothing lies right below their lowest chunks, for a
// moving piece no contacts carry it from below, and its center of mass does not lie over the chunks that something lies
// right below. A part that came down tilted onto a heap rests on what lies below its middle, its lowest corner can hang
// free, and so can a cell of a part that came down onto the cells of a storey that stay, which reaches lower between them.
static float FloatingVolume( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	bool* carriedPieces = FindCarriedPieces( scene );
	float volume = 0.0f;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		int actorIndex = world->debris.data[i];
		const nbActor* actor = world->actors.data + actorIndex;
		if ( actor->volume < 1.0f )
		{
			continue;
		}

		float bottom = FLT_MAX;
		for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			bottom = b3MinFloat( bottom, b3Shape_GetAABB( world->chunks.data[c].shapeId ).lowerBound.y );
		}

		bool carried = bottom < 0.05f || carriedPieces[actorIndex];
		for ( int c = actor->headChunk; c != NB_NULL_INDEX && carried == false; c = world->chunks.data[c].nextChunk )
		{
			b3AABB box = b3Shape_GetAABB( world->chunks.data[c].shapeId );
			if ( box.lowerBound.y > bottom + 0.15f )
			{
				continue;
			}

			b3AABB below = { { box.lowerBound.x, box.lowerBound.y - 0.12f, box.lowerBound.z },
							 { box.upperBound.x, box.lowerBound.y + 0.03f, box.upperBound.z } };
			FloatQuery query = { world, actorIndex, false };
			b3World_OverlapAABB( scene->physicsWorld, below, b3DefaultQueryFilter(), FloatQueryCallback, &query );
			carried = query.found;
		}

		b3WorldTransform transform = nbActor_GetTransform( world, actor );
		b3Vec3 center = b3Vec3_zero;
		float weight = 0.0f;
		b3AABB footprint = { { FLT_MAX, 0.0f, FLT_MAX }, { -FLT_MAX, 0.0f, -FLT_MAX } };
		for ( int c = actor->headChunk; c != NB_NULL_INDEX && carried == false; c = world->chunks.data[c].nextChunk )
		{
			const nbChunk* chunk = world->chunks.data + c;
			b3Pos centroid = b3TransformWorldPoint( transform, chunk->shape->centroid );
			center = b3MulAdd( center, chunk->shape->volume, (b3Vec3){ (float)centroid.x, 0.0f, (float)centroid.z } );
			weight += chunk->shape->volume;

			b3AABB box = b3Shape_GetAABB( chunk->shapeId );
			b3AABB below = { { box.lowerBound.x, box.lowerBound.y - 0.12f, box.lowerBound.z },
							 { box.upperBound.x, box.lowerBound.y + 0.03f, box.upperBound.z } };
			FloatQuery query = { world, actorIndex, false };
			b3World_OverlapAABB( scene->physicsWorld, below, b3DefaultQueryFilter(), FloatQueryCallback, &query );
			if ( query.found )
			{
				footprint.lowerBound.x = b3MinFloat( footprint.lowerBound.x, box.lowerBound.x );
				footprint.lowerBound.z = b3MinFloat( footprint.lowerBound.z, box.lowerBound.z );
				footprint.upperBound.x = b3MaxFloat( footprint.upperBound.x, box.upperBound.x );
				footprint.upperBound.z = b3MaxFloat( footprint.upperBound.z, box.upperBound.z );
			}
		}
		if ( carried == false )
		{
			center = b3MulSV( 1.0f / weight, center );
			carried = footprint.lowerBound.x <= center.x && center.x <= footprint.upperBound.x &&
					  footprint.lowerBound.z <= center.z && center.z <= footprint.upperBound.z;
		}

		volume += carried ? 0.0f : actor->volume;
	}
	free( carriedPieces );
	return volume;
}

// Rubble that hangs in the air: nothing lies within 30 cm below any of its chunks
static int RubbleInTheAir( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	int count = 0;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		int actorIndex = world->debris.data[i];
		const nbActor* actor = world->actors.data + actorIndex;
		bool carried = actor->isRubble == false;
		for ( int c = actor->headChunk; c != NB_NULL_INDEX && carried == false; c = world->chunks.data[c].nextChunk )
		{
			b3AABB box = b3Shape_GetAABB( world->chunks.data[c].shapeId );
			b3AABB below = { { box.lowerBound.x, box.lowerBound.y - 0.3f, box.lowerBound.z },
							 { box.upperBound.x, box.lowerBound.y + 0.03f, box.upperBound.z } };
			FloatQuery query = { world, actorIndex, false };
			b3World_OverlapAABB( scene->physicsWorld, below, b3DefaultQueryFilter(), FloatQueryCallback, &query );
			carried = query.found;
		}
		count += carried ? 0 : 1;
	}
	return count;
}

// Rubble that could stay up in the air once what it lies on moves away: a small piece of which none of the chunks it froze
// on is fixed or belongs to a moving piece that holds it, or a large piece, which counts every moving piece it lies on for
// its balance, with one that does not hold it
static int UnheldRubble( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	int count = 0;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		const nbActor* actor = world->actors.data + world->debris.data[i];
		bool held = false, unheld = false;
		for ( int k = 0; actor->isRubble && k < actor->carrierCount; ++k )
		{
			const nbCarrier* carrier = actor->carriers + k;
			const nbChunk* chunk = carrier->chunkIndex != NB_NULL_INDEX ? world->chunks.data + carrier->chunkIndex : NULL;
			if ( chunk != NULL && chunk->generation != carrier->generation )
			{
				continue;
			}

			const nbActor* carrierActor = chunk != NULL ? world->actors.data + chunk->actorIndex : NULL;
			bool holds = carrierActor == NULL || carrierActor->isStatic || carrierActor->isRubble || carrierActor->holdsRubble;
			held = held || holds;
			unheld = unheld || holds == false;
		}
		count += unheld && ( held == false || actor->volume >= 0.1f ) ? 1 : 0;
	}
	return count;
}

// Over the budget nothing freezes in flight, it would hang in the air. The budget waits until the debris lands, then it
// holds again.
static int DebrisBudgetFlightTest( void )
{
	TestScene scene = CreateScene();
	CreateWall( &scene, (b3Vec3){ 2.0f, 1.0f, 0.12f }, 23 );
	nbWorld_SetDebrisBudget( scene.world, 5 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.0f, 1.0f, 0.12f };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.6f;
	impact.damage = 1.0e8f;
	impact.ejectSpeed = 8.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	int flying = 0;
	for ( int i = 0; i < 60; ++i )
	{
		Step( &scene, 1 );
		nbStats stats = nbWorld_GetStats( scene.world );
		flying = b3MaxInt( flying, stats.debrisCount - stats.rubbleCount );
		ENSURE( RubbleInTheAir( &scene ) == 0 );
	}
	ENSURE( flying > 10 );

	Step( &scene, 240 );
	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.debrisCount - stats.rubbleCount <= 5 );
	ENSURE( RubbleInTheAir( &scene ) == 0 );
	DestroyScene( &scene );
	return 0;
}

// A brick wall 3 m wide and 0.3 m thick, pre-fractured into one layer of cells, under a concrete block. Without collision
// damage every fragment comes from a chunk crushed under the load.
static void CreateLoadedWall( TestScene* scene )
{
	nbMaterial concrete = nbDefaultMaterial();
	concrete.density = 2400.0f;
	concrete.strength = 1.1e6f;
	nbPieceDef pieces[2];
	pieces[0] = nbDefaultPieceDef();
	pieces[0].halfExtents = (b3Vec3){ 1.5f, 0.75f, 0.15f };
	pieces[0].transform.p = (b3Vec3){ 0.0f, 0.75f, 0.0f };
	pieces[1] = nbDefaultPieceDef();
	pieces[1].halfExtents = (b3Vec3){ 1.5f, 0.5f, 0.5f };
	pieces[1].transform.p = (b3Vec3){ 0.0f, 2.0f, 0.0f };
	pieces[1].material = &concrete;

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.material.density = 1900.0f;
	def.material.strength = 6.0e5f;
	def.cellSize = 0.35f;
	def.enableCollisionDamage = false;
	nbCreateDestructible( scene->world, &def, pieces, 2 );
}

// A grenade: an impact and the push of its blast
static void Grenade( TestScene* scene, b3Vec3 point )
{
	nbImpactDef impact = { 0 };
	impact.point = point;
	impact.radius = 1.3f;
	impact.damage = 3.0e5f;
	impact.ejectSpeed = 12.0f;
	impact.explosionImpulse = 40.0f * impact.ejectSpeed;
	nbWorld_ApplyImpact( scene->world, &impact );
}

// A point on the walls of a house 9 m wide and 6.5 m deep, d meters along the front, the right side, the back and the
// left side, 31 m around
static b3Vec3 AroundHouse( float d, float y )
{
	return d < 9.0f	   ? (b3Vec3){ -4.5f + d, y, 3.4f }
		   : d < 15.5f ? (b3Vec3){ 4.7f, y, 3.25f - ( d - 9.0f ) }
		   : d < 24.5f ? (b3Vec3){ 4.5f - ( d - 15.5f ), y, -3.4f }
					   : (b3Vec3){ -4.7f, y, -3.25f + ( d - 24.5f ) };
}

// Grenades in rows around a storey of the celled house, 3.25 m high, until it gives way. Where x lies beyond the limit,
// and at the corners when they are kept, the walls stay whole. Returns the number of grenades.
static int BlastStorey( TestScene* scene, int storey, int rows, float limit, bool keepCorners )
{
	int collapsed = nbWorld_GetStats( scene->world ).collapsedStoreyCount;
	int count = 0;
	for ( int row = 0; row < rows; ++row )
	{
		float y = 3.25f * (float)storey + ( rows == 1 ? 1.2f : 0.6f + 1.5f * (float)row / (float)( rows - 1 ) );
		for ( int k = 0; k < 32; ++k )
		{
			b3Vec3 point = AroundHouse( 31.0f * (float)k / 32.0f, y );
			if ( point.x > limit || ( keepCorners && fabsf( point.x ) > 3.2f && fabsf( point.z ) > 1.95f ) )
			{
				continue;
			}

			Grenade( scene, point );
			Step( scene, 10 );
			count += 1;
			if ( nbWorld_GetStats( scene->world ).collapsedStoreyCount > collapsed )
			{
				return count;
			}
		}
	}
	return count;
}

// The largest loose piece, NULL without debris
static const nbActor* LargestActor( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	const nbActor* largest = NULL;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		const nbActor* actor = world->actors.data + world->debris.data[i];
		largest = largest == NULL || actor->volume > largest->volume ? actor : largest;
	}
	return largest;
}

// The local Y axis of the largest loose piece in the world, up for a building that stands upright
static b3Vec3 LargestAxis( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	const nbActor* largest = LargestActor( scene );
	return largest != NULL ? b3RotateVector( nbActor_GetTransform( world, largest ).q, b3Vec3_axisY ) : b3Vec3_axisY;
}

// The volume of what stands of a building between two heights, by the centroids of its chunks, and the smallest x of
// those centroids
static float StandingBetween( TestScene* scene, float low, float high, float* lowestX )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	float volume = 0.0f;
	*lowestX = FLT_MAX;
	for ( int c = 0; c < world->chunks.count; ++c )
	{
		const nbChunk* chunk = world->chunks.data + c;
		const nbActor* actor = world->actors.data + chunk->actorIndex;
		if ( chunk->shape == NULL || actor->isStatic == false )
		{
			continue;
		}

		b3Pos centroid = b3TransformWorldPoint( nbActor_GetTransform( world, actor ), chunk->shape->centroid );
		if ( centroid.y > low && centroid.y < high )
		{
			volume += chunk->shape->volume;
			*lowestX = b3MinFloat( *lowestX, (float)centroid.x );
		}
	}
	return volume;
}

// Whether two cells of a building as it was built, no fragments of them, hold together between two heights, by the
// centroids of their chunks, where they still are where they were built, outside the largest loose piece. Side by side
// anywhere, one on top of the other only above the lowest half meter, where a cell keeps its bond to the one below. The
// parts of a cell are bonded as siblings, see SeamTest.
static bool CellsJoinedBetween( TestScene* scene, float low, float high )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	const nbActor* largest = LargestActor( scene );
	for ( int c = 0; c < world->chunks.count; ++c )
	{
		const nbChunk* chunk = world->chunks.data + c;
		const nbActor* actor = world->actors.data + chunk->actorIndex;
		if ( chunk->shape == NULL || chunk->depth > 0 || actor == largest )
		{
			continue;
		}

		b3WorldTransform home = world->destructibles.data[chunk->destructibleIndex].transform;
		b3Pos centroid = b3TransformWorldPoint( nbActor_GetTransform( world, actor ), chunk->shape->centroid );
		b3Pos origin = b3TransformWorldPoint( home, chunk->shape->centroid );
		if ( centroid.y <= low || centroid.y >= high || b3Length( b3SubPos( centroid, origin ) ) > 0.05f )
		{
			continue;
		}

		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; key = world->bonds.data[key >> 1].nextKey[key & 1] )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			const nbChunk* other = world->chunks.data + bond->chunk[( key & 1 ) ^ 1];
			b3Pos otherCentroid = b3TransformWorldPoint( home, other->shape->centroid );
			bool sideBySide = b3AbsFloat( bond->normal.y ) < 0.7f;
			bool above = centroid.y > low + 0.5f && otherCentroid.y > low + 0.5f;
			if ( bond->sibling == false && ( sideBySide || above ) && other->depth == 0 && otherCentroid.y > low &&
				 otherCentroid.y < high )
			{
				return true;
			}
		}
	}
	return false;
}

// Grenades around a storey of the largest loose piece, in its own frame, until a storey gives way. The second round falls
// between the grenades of the first.
static int BlastPieceStorey( TestScene* scene, int storey )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	int collapsed = nbWorld_GetStats( scene->world ).collapsedStoreyCount;
	int count = 0;
	for ( int k = 0; k < 64; ++k )
	{
		b3WorldTransform transform = nbActor_GetTransform( world, LargestActor( scene ) );
		float d = 31.0f * (float)( k % 32 ) / 32.0f + ( k < 32 ? 0.0f : 0.5f );
		b3Pos point = b3TransformWorldPoint( transform, AroundHouse( d, 3.25f * (float)storey + 1.2f ) );
		Grenade( scene, (b3Vec3){ (float)point.x, (float)point.y, (float)point.z } );
		Step( scene, 5 );
		count += 1;
		if ( nbWorld_GetStats( scene->world ).collapsedStoreyCount > collapsed )
		{
			break;
		}
	}
	return count;
}

// A building gives way storey by storey: when a storey lost 40 % of its walls, on any floor, a tenth of what is left
// stays, the rest flies out, and everything above comes down in one piece and tilts over what stayed. Nothing else of a
// building collapses, not the wall above a hole, not a floor, not a column under load. Only what is cut off from the
// ground falls.
static int StoreyTest( void )
{
	// A four story house stands on its walls, and so does a town house of three stories with doors and windows.
	// Buildings never take the load check, not even with bonds that could not carry a thing.
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetSupportScale( scene.world, 0.01f );
		CreateHouse( &scene, 4, 15, 0.0f );
		Step( &scene, 30 );
		nbStats stats = nbWorld_GetStats( scene.world );
		ENSURE( stats.overloadedBondCount == 0 );
		ENSURE( stats.dynamicBodyCount == 0 );
		DestroyScene( &scene );
	}
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetSupportScale( scene.world, 0.01f );
		CreateTownHouse( &scene, 3 );
		Step( &scene, 30 );
		nbStats stats = nbWorld_GetStats( scene.world );
		ENSURE( stats.overloadedBondCount == 0 );
		ENSURE( stats.dynamicBodyCount == 0 );
		DestroyScene( &scene );
	}

	// A floor on one wall stays up, and a house stands on thin columns
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetSupportScale( scene.world, 1.0f );
		nbDestructibleId house = CreateHouse( &scene, 1, 1, 0.0f );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount == 0 );
		ENSURE( HighestChunk( house ) > 3.0f );
		DestroyScene( &scene );
	}
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetSupportScale( scene.world, 1.0f );
		nbDestructibleId house = CreateHouse( &scene, 2, 15, 0.15f );
		float start = CenterOfChunks( house ).y;
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).overloadedBondCount == 0 );
		ENSURE( CenterOfChunks( house ).y > start - 0.01f );
		DestroyScene( &scene );
	}

	// A few grenades at a corner of the ground floor of the celled house, as the demo builds it, do not bring it down, not
	// even with the default ranges, with which a storey gives way at a fifth of its walls destroyed
	for ( uint32_t seed = 0; seed < 4; ++seed )
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 3, seed );
		for ( int k = 0; k < 4; ++k )
		{
			Grenade( &scene, AroundHouse( k < 2 ? 0.5f + (float)k : 30.5f - (float)( k - 2 ), 1.2f ) );
			Step( &scene, 10 );
		}
		Step( &scene, 30 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 0 );
		ENSURE( HighestStanding( &scene ) > 9.0f );
		DestroyScene( &scene );
	}

	// Grenades all around its ground floor bring it down. Nine tenths of what is left of the ground floor fly out of the
	// house in one update. With the damage all around, the tenth that stays are cells picked at random, which come loose
	// or stay as stumps no higher than a cell. The stories above come down in one piece with their floors, onto the
	// ground, about level. No wall stays standing.
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 3, 3 );
		nbWorld* world = nbGetWorldFromId( scene.world );
		int thrownCount = 0;
		for ( int k = 0; k < 32; ++k )
		{
			Grenade( &scene, AroundHouse( 31.0f * (float)k / 32.0f, 1.2f ) );

			// What a collapse throws out flies fast from the start. Grenade fragments of the house do not come from one.
			for ( int step = 0; step < 10; ++step )
			{
				Step( &scene, 1 );
				int count = 0;
				for ( int i = 0; i < world->debris.count; ++i )
				{
					const nbActor* actor = world->actors.data + world->debris.data[i];
					b3Vec3 velocity = b3Body_GetLinearVelocity( actor->bodyId );
					count += actor->fromCollapse && world->time - actor->freeTime <= 0.02f &&
									 velocity.x * velocity.x + velocity.z * velocity.z > 5.0f * 5.0f
								 ? 1
								 : 0;
				}
				thrownCount = count > thrownCount ? count : thrownCount;
			}
		}
		Step( &scene, 180 );
		ENSURE( thrownCount >= 20 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		// The two upper stories and three floors are 87 cubic meters, the house was 9.75 m tall
		float low = 0.0f, high = 0.0f;
		ENSURE( LargestPiece( &scene, &low, &high ) > 60.0f );
		ENSURE( low < 1.0f );
		ENSURE( high < 9.5f );
		ENSURE( HighestStanding( &scene ) < 2.0f );
		ENSURE( LargestAxis( &scene ).y > 0.96f );

		// What came down is still a building. Grenades around its second story bring the story above down onto the rest,
		// the upper story and two floors of 51 cubic meters.
		float top = high;
		BlastPieceStorey( &scene, 1 );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 2 );
		ENSURE( LargestPiece( &scene, &low, &high ) > 40.0f );
		ENSURE( high < top - 0.3f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );

		// Nothing of a building breaks in two where it lands, only parts of walls do
		ENSURE( nbWorld_GetStats( scene.world ).landingSplitCount == 0 );
		DestroyScene( &scene );
	}

	// The same with the town house of wall boxes for every story. The story above must not stay up on the rubble that
	// froze on it while it was part of the house, or on rubble that froze on that rubble.
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateTownHouse( &scene, 2 );
		for ( int k = 0; k < 32; ++k )
		{
			Grenade( &scene, AroundHouse( 31.0f * (float)k / 32.0f, 1.2f ) );
			Step( &scene, 10 );
		}
		Step( &scene, 180 );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// Grenades around the second story bring the story above down onto the ground floor, which stands, with what stays of
	// the second story. With the corners of the second story left whole as well, a rigid shell the old load check never
	// broke. What came down freezes there. Grenades around the ground floor then bring that down too, with what lies on
	// it, and nothing stays up in the air.
	for ( int corners = 0; corners < 2; ++corners )
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 1, 1 + corners, FLT_MAX, corners == 1 );
		Step( &scene, 300 );

		// It freezes once it rests on the cells under it and they rest too, as long as it sinks it stays awake
		for ( int s = 0; s < 40 && ( LargestActor( &scene ) == NULL || LargestActor( &scene )->isRubble == false ); ++s )
		{
			Step( &scene, 30 );
		}
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );
		float standing = HighestStanding( &scene );
		ENSURE( 2.9f < standing && standing < 6.5f );

		// The upper story and two floors are 51 cubic meters
		const nbActor* upper = LargestActor( &scene );
		ENSURE( upper != NULL && upper->isRubble && upper->volume > 40.0f );
		float low = 0.0f, high = 0.0f;
		LargestPiece( &scene, &low, &high );
		ENSURE( low > 2.5f );

		BlastStorey( &scene, 0, 1, FLT_MAX, false );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 2 );
		ENSURE( HighestStanding( &scene ) < 1.25f );
		ENSURE( LargestPiece( &scene, &low, &high ) > 40.0f );
		ENSURE( low < 2.5f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );

		// Then everything sleeps, some of it on pieces that cannot freeze. An update looks at none of it again.
		Step( &scene, 360 );
		nbWorld* world = nbGetWorldFromId( scene.world );
		int looked = 0;
		for ( int i = 0; i < world->debris.count; ++i )
		{
			const nbActor* actor = world->actors.data + world->debris.data[i];
			bool asleep = actor->isRubble == false && b3Body_IsAwake( actor->bodyId ) == false;
			looked += asleep && actor->settleStamp == world->settleStamp ? 1 : 0;
		}
		ENSURE( looked == 0 );
		DestroyScene( &scene );
	}

	// Grenades at the -x half of the ground floor bring it down with the damage on one side. A tenth of what is left
	// stays as an anchor on the +x side, broken into its cells: those on the ground stay fixed, the others stand on them
	// where they were. The stories above come down onto it and tilt over it toward -x as gravity makes them, nothing
	// turns them. The loose cells give way under them, and they come to rest on what is left of it, leaning less,
	// nothing up in the air. The same with one worker and with four.
	uint32_t hashes[2];
	for ( int w = 0; w < 2; ++w )
	{
		TestScene scene = w == 0 ? CreateScene() : CreateSceneWithWorkers( 4, NULL, NULL, NULL );
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		nbDestructibleId house = CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 0, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		const nbStorey* storey = nbGetWorldFromId( scene.world )->destructibles.data[house.index1 - 1].storeys;
		ENSURE( 0.03f * storey->volume < storey->kept && storey->kept < 0.1f * storey->volume );
		float lowestX = 0.0f;
		float anchor = StandingBetween( &scene, 0.0f, 3.25f, &lowestX );
		ENSURE( 0.0f < anchor && anchor < 0.5f * storey->kept && lowestX > 2.0f );
		ENSURE( HighestStanding( &scene ) < 1.2f );
		ENSURE( CellsJoinedBetween( &scene, 0.0f, 3.25f ) == false );

		Step( &scene, 30 );
		b3Vec3 axis = LargestAxis( &scene );
		ENSURE( axis.x < -0.15f );
		Step( &scene, 300 );
		axis = LargestAxis( &scene );
		ENSURE( axis.y > 0.95f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		hashes[w] = HashDestructible( 2166136261u, house );
		DestroyScene( &scene );
	}
	ENSURE( hashes[0] == hashes[1] );

	// The same at the -x half of the second story: what comes down tilts toward -x over the anchor at +x, whose cells on
	// the ground floor stay fixed to it
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 1, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		float lowestX = 0.0f;
		float anchor = StandingBetween( &scene, 3.25f, 6.5f, &lowestX );
		ENSURE( anchor > 0.0f && lowestX > 2.0f );
		ENSURE( HighestStanding( &scene ) < 3.25f + 1.2f );
		ENSURE( CellsJoinedBetween( &scene, 3.25f, 6.5f ) == false );
		Step( &scene, 40 );
		ENSURE( LargestAxis( &scene ).x < -0.1f );
		Step( &scene, 300 );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// The same at the -x half of the top story, with only the roof above it. The roof hangs at the side of the walls and
	// would come down flat past the anchor, so the cells of the anchor it hangs on come down with it and rest on the rest
	// of the anchor, which breaks into its cells like that of a storey further down. It tilts over that toward -x like
	// the stories over a storey further down and comes to rest on it, in a house of three stories with one worker and
	// with four alike, and in one of two.
	for ( int w = 0; w < 3; ++w )
	{
		TestScene scene = w == 1 ? CreateSceneWithWorkers( 4, NULL, NULL, NULL ) : CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		int floors = w < 2 ? 3 : 2;
		nbDestructibleId house = CreateCelledHouse( &scene, floors, 3 );
		float low = 3.25f * (float)( floors - 1 );
		BlastStorey( &scene, floors - 1, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		float lowestX = 0.0f;
		float anchor = StandingBetween( &scene, low, low + 3.25f, &lowestX );
		ENSURE( anchor > 0.0f && lowestX > 2.0f );
		ENSURE( HighestStanding( &scene ) < low + 1.2f );
		ENSURE( CellsJoinedBetween( &scene, low, low + 3.25f ) == false );
		Step( &scene, 30 );
		ENSURE( LargestAxis( &scene ).x < -0.15f );
		Step( &scene, 300 );
		ENSURE( LargestAxis( &scene ).y > 0.95f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		if ( w < 2 )
		{
			hashes[w] = HashDestructible( 2166136261u, house );
		}
		DestroyScene( &scene );
	}
	ENSURE( hashes[0] == hashes[1] );

	// With the damage all around the top story, the roof comes down about level like the stories over a storey further
	// down
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 2, 3 );
		BlastStorey( &scene, 1, 1, FLT_MAX, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );
		Step( &scene, 300 );
		ENSURE( LargestAxis( &scene ).y > 0.96f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// What came down keeps falling when an impact hits it on the way
	{
		TestScene scene = CreateScene();
		SetStoreyCollapse( &scene, 0.4f, 0.1f );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 2, 3 );
		BlastStorey( &scene, 0, 1, FLT_MAX, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		nbWorld* world = nbGetWorldFromId( scene.world );
		const nbActor* largest = NULL;
		for ( int i = 0; i < world->debris.count; ++i )
		{
			const nbActor* actor = world->actors.data + world->debris.data[i];
			largest = largest == NULL || actor->volume > largest->volume ? actor : largest;
		}
		ENSURE( largest != NULL && largest->volume > 40.0f );

		b3BodyId body = largest->bodyId;
		for ( int step = 0; step < 30 && b3Length( b3Body_GetLinearVelocity( body ) ) < 2.0f; ++step )
		{
			Step( &scene, 1 );
		}
		float speed = b3Length( b3Body_GetLinearVelocity( body ) );
		ENSURE( speed > 2.0f );

		nbImpactDef impact = { 0 };
		impact.point = b3Body_GetWorldCenter( body );
		impact.radius = 0.5f;
		impact.damage = 1.0f;
		nbWorld_ApplyImpact( scene.world, &impact );
		ENSURE( b3Length( b3Body_GetLinearVelocity( body ) ) > 0.9f * speed );
		DestroyScene( &scene );
	}

	return 0;
}

// How much of a storey that gives way stays is up to the game, see nbStoreyCollapse. A storey that gave way does
// not give way again for what stayed of it.
static int StoreyResidueTest( void )
{
	// Nothing stays: all of the walls that are left fly out, and the stories above come down about level
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		SetStoreyCollapse( &scene, 0.4f, 0.0f );
		CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 0, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		float lowestX = 0.0f;
		ENSURE( StandingBetween( &scene, 0.0f, 3.25f, &lowestX ) < 0.05f );
		Step( &scene, 300 );
		ENSURE( LargestAxis( &scene ).y > 0.98f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// Three tenths stay, about 18 % of the walls of the ground floor, broken into their cells, and the stories above tilt
	// over the larger anchor toward -x
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		SetStoreyCollapse( &scene, 0.4f, 0.3f );
		nbDestructibleId house = CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 0, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		const nbStorey* storey = nbGetWorldFromId( scene.world )->destructibles.data[house.index1 - 1].storeys;
		ENSURE( 0.15f * storey->volume < storey->kept && storey->kept < 0.21f * storey->volume );
		float lowestX = 0.0f;
		float anchor = StandingBetween( &scene, 0.0f, 3.25f, &lowestX );
		ENSURE( 0.0f < anchor && anchor < 0.5f * storey->kept && lowestX > 2.0f );
		ENSURE( CellsJoinedBetween( &scene, 0.0f, 3.25f ) == false );
		Step( &scene, 30 );
		ENSURE( LargestAxis( &scene ).x < -0.15f );
		Step( &scene, 300 );
		ENSURE( LargestAxis( &scene ).y > 0.95f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// All of the walls that are left stay, as by default, broken into their cells, of the ground floor and of the top
	// story under the roof with its own ranges. Nothing flies out, and what is above comes down onto the cells or stays
	// up on them, nothing up in the air. Grenades at what stayed bring it down onto what is below, and nothing stays up in
	// the air either.
	for ( int k = 0; k < 4; ++k )
	{
		int floor = k < 2 ? 0 : 2;
		float low = 3.25f * (float)floor;
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		nbDestructibleId house = CreateCelledHouse( &scene, 3, 3 + (uint32_t)( k % 2 ) );
		nbWorld* world = nbGetWorldFromId( scene.world );
		BlastStorey( &scene, floor, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		const nbStorey* storey = world->destructibles.data[house.index1 - 1].storeys + floor;
		ENSURE( storey->kept > 0.6f * storey->volume );
		ENSURE( HighestStanding( &scene ) < low + 1.2f );
		ENSURE( CellsJoinedBetween( &scene, low, low + 3.25f ) == false );
		int fast = 0;
		for ( int i = 0; i < world->debris.count; ++i )
		{
			const nbActor* actor = world->actors.data + world->debris.data[i];
			b3Vec3 velocity = actor->isRubble ? b3Vec3_zero : b3Body_GetLinearVelocity( actor->bodyId );
			fast += actor->fromCollapse && velocity.x * velocity.x + velocity.z * velocity.z > 3.0f * 3.0f ? 1 : 0;
		}
		ENSURE( fast == 0 );

		Step( &scene, 900 );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		ENSURE( UnheldRubble( &scene ) == 0 );
		for ( int g = 0; g < 32; ++g )
		{
			Grenade( &scene, AroundHouse( 31.0f * (float)g / 32.0f + 0.5f, low + 1.0f ) );
			Step( &scene, 10 );
			ENSURE( UnheldRubble( &scene ) == 0 );
		}
		Step( &scene, 600 );
		float bottom = 0.0f, top = 0.0f;
		float largest = LargestPiece( &scene, &bottom, &top );
		ENSURE( largest > ( floor == 0 ? 60.0f : 15.0f ) && bottom < low + 1.25f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// With half of the walls that are left staying, the roof that came down over the top story takes more than a tenth of
	// the walls of the story along, the cells of the anchor it hung on. A grenade on it changes it, and the story is
	// checked again in it, but the cells stay, so it only loses what the grenade breaks off.
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		SetStoreyCollapse( &scene, 0.4f, 0.5f );
		CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 2, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );
		Step( &scene, 300 );

		nbWorld* world = nbGetWorldFromId( scene.world );
		const nbActor* roof = LargestActor( &scene );
		float before = roof->volume;
		ENSURE( before > 15.0f );
		b3Pos point = b3TransformWorldPoint( nbActor_GetTransform( world, roof ), (b3Vec3){ -2.0f, 9.75f, 0.0f } );
		Grenade( &scene, (b3Vec3){ (float)point.x, (float)point.y + 0.3f, (float)point.z } );
		Step( &scene, 300 );
		ENSURE( LargestActor( &scene )->volume > before - 1.0f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// The ranges are clamped to what makes sense, also at creation
	{
		b3WorldDef physicsDef = b3DefaultWorldDef();
		b3WorldId physicsWorld = b3CreateWorld( &physicsDef );
		nbWorldDef def = nbDefaultWorldDef();
		def.physicsWorld = physicsWorld;
		def.storeyCollapse = (nbStoreyCollapse){ -0.5f, 2.0f, 0.3f, 0.1f };
		nbWorldId other = nbCreateWorld( &def );
		nbStoreyCollapse clamped = nbGetWorldFromId( other )->def.storeyCollapse;
		ENSURE( clamped.damageMin == 0.0f && clamped.damageMax == 1.0f && clamped.residueMin == 0.3f &&
				clamped.residueMax == 0.3f );
		nbDestroyWorld( other );
		b3DestroyWorld( physicsWorld );

		TestScene scene = CreateScene();
		nbWorld_SetStoreyCollapse( scene.world, (nbStoreyCollapse){ 0.5f, 0.2f, -1.0f, 2.0f }, (nbStoreyCollapse){ 0 } );
		clamped = nbGetWorldFromId( scene.world )->def.storeyCollapse;
		ENSURE( clamped.damageMin == 0.5f && clamped.damageMax == 0.5f && clamped.residueMin == 0.0f &&
				clamped.residueMax == 1.0f );
		DestroyScene( &scene );

		nbStoreyCollapse storeys = nbDefaultWorldDef().storeyCollapse;
		nbStoreyCollapse top = nbDefaultWorldDef().topStoreyCollapse;
		ENSURE( storeys.damageMin == 0.17f && storeys.damageMax == 0.31f && storeys.residueMin == 1.0f &&
				storeys.residueMax == 1.0f );
		ENSURE( top.damageMin == 0.18f && top.damageMax == 0.25f && top.residueMin == 1.0f && top.residueMax == 1.0f );
	}

	return 0;
}

// Whether rubble lies on something that holds without a piece, following the chunks it froze on: the ground, a structure
// or a moving piece other than that one, see nbIsSupportedWithout
static bool HoldsWithout( nbWorld* world, int rubbleIndex, int actorIndex )
{
	int* stack = malloc( sizeof( int ) * (size_t)world->actors.count );
	bool* seen = calloc( (size_t)world->actors.count, sizeof( bool ) );
	int count = 0;
	stack[count++] = rubbleIndex;
	seen[rubbleIndex] = true;
	bool holds = false;
	while ( count > 0 && holds == false )
	{
		const nbActor* rubble = world->actors.data + stack[--count];
		for ( int k = 0; k < rubble->carrierCount && holds == false; ++k )
		{
			const nbCarrier* carrier = rubble->carriers + k;
			const nbChunk* chunk = carrier->chunkIndex != NB_NULL_INDEX ? world->chunks.data + carrier->chunkIndex : NULL;
			if ( chunk != NULL && ( chunk->generation != carrier->generation || chunk->actorIndex == actorIndex ) )
			{
				continue;
			}

			holds = chunk == NULL || world->actors.data[chunk->actorIndex].isRubble == false;
			if ( holds == false && seen[chunk->actorIndex] == false )
			{
				seen[chunk->actorIndex] = true;
				stack[count++] = chunk->actorIndex;
			}
		}
	}
	free( seen );
	free( stack );
	return holds;
}

// Large moving pieces asleep on rubble that holds only through them, the two hold each other up in the air
static int SelfPropped( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	int count = 0;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		int actorIndex = world->debris.data[i];
		const nbActor* actor = world->actors.data + actorIndex;
		b3BodyId bodyId = actor->bodyId;
		if ( actor->isRubble || actor->volume < 0.1f || b3Body_IsAwake( bodyId ) )
		{
			continue;
		}

		int capacity = b3Body_GetContactCapacity( bodyId );
		b3ContactData* contacts = malloc( sizeof( b3ContactData ) * (size_t)( capacity + 1 ) );
		int contactCount = b3Body_GetContactData( bodyId, contacts, capacity );
		float centerHeight = (float)b3Body_GetWorldCenter( bodyId ).y;
		bool propped = false;
		for ( int c = 0; c < contactCount && propped == false; ++c )
		{
			bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), bodyId );
			int other = FindBodyActor( world, b3Shape_GetBody( isA ? contacts[c].shapeIdB : contacts[c].shapeIdA ) );
			propped = other != NB_NULL_INDEX && world->actors.data[other].isRubble &&
					  CarriesFromBelow( contacts + c, bodyId, centerHeight ) && HoldsWithout( world, other, actorIndex ) == false;
		}
		free( contacts );
		count += propped ? 1 : 0;
	}
	return count;
}

// A shot of the rifle of the demo at a point on the walls of the celled house, d meters around it, from 12 m outside
static void Rifle( TestScene* scene, float d, float y )
{
	b3Vec3 target = AroundHouse( d, y );
	b3Vec3 normal = d < 9.0f	? (b3Vec3){ 0.0f, 0.0f, 1.0f }
				  : d < 15.5f ? (b3Vec3){ 1.0f, 0.0f, 0.0f }
				  : d < 24.5f ? (b3Vec3){ 0.0f, 0.0f, -1.0f }
							  : (b3Vec3){ -1.0f, 0.0f, 0.0f };
	b3Vec3 origin = b3MulAdd( b3MulAdd( target, 12.0f, normal ), 0.3f, b3Vec3_axisY );
	nbImpactDef impact = { 0 };
	impact.radius = 0.78f;
	impact.damage = 2.0e6f;
	impact.ejectSpeed = 30.0f;
	nbImpactResult result;
	nbWorld_CastImpact( scene->world, origin, b3MulSV( 250.0f, b3Normalize( b3Sub( target, origin ) ) ), &impact, &result );
}

// The lowest point of the highest of the parts of buildings that came down
static float HighestPart( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
	float highest = -FLT_MAX;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		const nbActor* actor = world->actors.data + world->debris.data[i];
		if ( actor->isBuildingPart == false )
		{
			continue;
		}

		float bottom = FLT_MAX;
		for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			bottom = b3MinFloat( bottom, b3Shape_GetAABB( world->chunks.data[c].shapeId ).lowerBound.y );
		}
		highest = b3MaxFloat( highest, bottom );
	}
	return highest;
}

// The middle storey of a celled house shot away all around with the rifle, 20 shots a second, then the ground floor, as
// a player would. Returns the large pieces that hang on rubble held only through them once everything came to rest, the
// volume of the large pieces that hang in the air, and the lowest point of the highest part of the house that came down.
static int ShootStoreys( uint32_t seed, int extraShots, float* floating, float* highest )
{
	TestScene scene = CreateScene();
	nbWorld_SetFragmentScale( scene.world, 2.0f );
	nbWorld_SetDebrisBudget( scene.world, 5000 );
	CelledHouse* house = malloc( sizeof( CelledHouse ) );
	BuildCelledHouse( house, 3, 201, b3Vec3_zero );
	house->def.material.fragmentSize = 0.1f;
	house->def.material.friction = 0.8f;
	house->concrete.density = 2400.0f;
	house->concrete.fragmentSize = 0.13f;
	nbCreateDestructible( scene.world, &house->def, house->pieces, house->pieceCount );
	free( house );

	nbRandom rng = nbMakeRandom( seed, 0 );
	for ( int storey = 1; storey >= 0; --storey )
	{
		int collapsed = nbWorld_GetStats( scene.world ).collapsedStoreyCount;
		int after = -1;
		for ( int shot = 0; shot < 600 && after < extraShots; ++shot )
		{
			float d = nbRandomRange( &rng, 0.0f, 31.0f );
			Rifle( &scene, d, 3.25f * (float)storey + nbRandomRange( &rng, 0.2f, 3.1f ) );
			Step( &scene, 3 );
			after = after >= 0 ? after + 1 : nbWorld_GetStats( scene.world ).collapsedStoreyCount > collapsed ? 0 : -1;
		}
		Step( &scene, 120 );
	}
	Step( &scene, 600 );
	int propped = SelfPropped( &scene );
	*floating = FloatingVolume( &scene );
	*highest = HighestPart( &scene );
	DestroyScene( &scene );
	return propped;
}

// The middle storey shot away with the rifle, then the ground floor, as the user did in the demo. The part above came to
// rest on rubble that had frozen on it while it was rubble itself, wedged under its edges and jammed in its openings, and
// on small moving pieces beside it that kept it balanced. It did not freeze, it held that rubble, and the rubble held it
// up in the air. Before, one part stayed up so with this seed, and so did one or two in 7 of 32 runs with 16 seeds and 40
// or 150 shots after each storey gave way. And what came down onto the remains of the middle storey froze there and
// stayed up when the ground floor gave way, frozen it weighed nothing. With this seed the lowest point of the highest part
// stayed at 6.2 m, and above 5 m with 3 of 16 seeds. Now it comes down to 3.1 m, and with none of them above 4.7 m.
static int ShotStoreysTest( void )
{
	float floating = 0.0f, highest = 0.0f;
	ENSURE( ShootStoreys( 3, 40, &floating, &highest ) == 0 );
	ENSURE( floating == 0.0f );
	ENSURE( highest < 4.5f );
	return 0;
}

// Every storey picks where it lies in the ranges of nbStoreyCollapse when its building is created, from the seed of the
// building, and the top storey takes ranges of its own
static int StoreyPickTest( void )
{
	// The picks lie between 0 and 1, differ from storey to storey and from seed to seed, and the same seed picks the same
	float picks[3][6];
	uint32_t seeds[3] = { 3, 3, 4 };
	for ( int h = 0; h < 3; ++h )
	{
		TestScene scene = CreateScene();
		nbDestructibleId house = CreateCelledHouse( &scene, 3, seeds[h] );
		const nbDestructible* building = nbGetWorldFromId( scene.world )->destructibles.data + ( house.index1 - 1 );
		ENSURE( building->storeyCount == 3 );
		for ( int s = 0; s < 3; ++s )
		{
			picks[h][2 * s] = building->storeys[s].damageRoll;
			picks[h][2 * s + 1] = building->storeys[s].residueRoll;
		}
		DestroyScene( &scene );
	}
	for ( int k = 0; k < 6; ++k )
	{
		ENSURE( 0.0f <= picks[0][k] && picks[0][k] < 1.0f );
		ENSURE( picks[1][k] == picks[0][k] && picks[2][k] != picks[0][k] );
		ENSURE( k < 2 || picks[0][k] != picks[0][k - 2] );
	}

	// A storey gives way at the damage its pick sets in the range, and of what is left of it the part its pick sets in
	// the range stays. Here the ground floor of two houses with grenades all around it, one at a time, each of which
	// destroys a little more of it until close to half is gone. A run in which nothing gives way finds what stands after
	// every grenade, and the same run with the range gives way with the grenade that takes the ground floor below its
	// pick.
	nbStoreyCollapse wide = { 0.15f, 0.45f, 0.0f, 0.5f };
	nbStoreyCollapse never = { 1.0f, 1.0f, 0.0f, 0.0f };
	float damages[2];
	for ( int h = 0; h < 2; ++h )
	{
		float standing[31];
		int collapseAt[2] = { -1, -1 };
		float damage = 0.0f, residue = 0.0f, kept = 0.0f;
		for ( int run = 0; run < 2; ++run )
		{
			TestScene scene = CreateScene();
			nbWorld_SetStoreyCollapse( scene.world, run == 0 ? never : wide, run == 0 ? never : wide );
			nbWorld_SetFragmentScale( scene.world, 2.0f );
			nbDestructibleId house = CreateCelledHouse( &scene, 3, 3 + 2 * (uint32_t)h );
			const nbDestructible* building = nbGetWorldFromId( scene.world )->destructibles.data + ( house.index1 - 1 );
			const nbStorey* storey = building->storeys;
			damage = wide.damageMin + storey->damageRoll * ( wide.damageMax - wide.damageMin );
			residue = wide.residueMin + storey->residueRoll * ( wide.residueMax - wide.residueMin );
			standing[0] = 1.0f;
			for ( int k = 0; k < 30 && collapseAt[run] < 0; ++k )
			{
				Grenade( &scene, AroundHouse( 31.0f * (float)k / 32.0f, 1.2f ) );
				Step( &scene, 10 );
				standing[k + 1] = run == 0 ? (float)( building->standing[0] / storey->volume ) : standing[k + 1];
				collapseAt[run] = nbWorld_GetStats( scene.world ).collapsedStoreyCount > 0 ? k : -1;
			}
			kept = storey->kept / storey->volume;
			DestroyScene( &scene );
		}

		int expected = 0;
		while ( expected < 30 && standing[expected + 1] >= 1.0f - damage )
		{
			expected += 1;
		}
		ENSURE( collapseAt[0] < 0 && expected < 30 && collapseAt[1] == expected );
		ENSURE( residue * standing[expected + 1] - 0.02f < kept && kept < residue * standing[expected] + 0.02f );
		damages[h] = damage;
	}
	ENSURE( fabsf( damages[0] - damages[1] ) > 0.05f );

	// The top storey takes the ranges of its own. With them off it does not give way, while the storeys below give way
	// as before, and the other way around.
	for ( int top = 0; top < 2; ++top )
	{
		nbStoreyCollapse on = { 0.4f, 0.4f, 0.1f, 0.1f };
		nbStoreyCollapse off = { 0 };
		TestScene scene = CreateScene();
		nbWorld_SetStoreyCollapse( scene.world, top == 0 ? on : off, top == 0 ? off : on );
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		nbDestructibleId house = CreateCelledHouse( &scene, 3, 3 );
		const nbDestructible* building = nbGetWorldFromId( scene.world )->destructibles.data + ( house.index1 - 1 );
		int held = top == 0 ? 2 : 0;
		for ( int k = 0; k < 30; ++k )
		{
			Grenade( &scene, AroundHouse( 31.0f * (float)k / 32.0f, 3.25f * (float)held + 1.2f ) );
			Step( &scene, 10 );
		}
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 0 );
		ENSURE( building->standing[held] < 0.6 * building->storeys[held].volume );
		BlastStorey( &scene, 2 - held, 1, FLT_MAX, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );
		DestroyScene( &scene );
	}

	return 0;
}

// Structures without floors take the load check when it is on, see nbWorldDef::supportScale
static int SupportTest( void )
{
	// A wall under a concrete block stands while the check is off and gives way when it comes on
	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 0.0f );
		CreateLoadedWall( &scene );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount == 0 );
		nbWorld_SetSupportScale( scene.world, 0.03f );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).overloadedBondCount > 0 );
		ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount > 0 );
		DestroyScene( &scene );
	}

	// A wall too weak for the block on it collapses. The stones it rests on leave it sideways at once, they do not stay
	// wedged under it, and the block comes down.
	for ( int k = 0; k < 3; ++k )
	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 0.03f + 0.05f * (float)k );
		CreateLoadedWall( &scene );
		int thrownCount = 0;
		for ( int step = 0; step < 90; ++step )
		{
			Step( &scene, 1 );
			nbWorld* world = nbGetWorldFromId( scene.world );
			for ( int d = 0; d < world->debris.count; ++d )
			{
				const nbActor* actor = world->actors.data + world->debris.data[d];
				const nbChunk* chunk = world->chunks.data + actor->headChunk;
				b3Vec3 velocity = b3Body_GetLinearVelocity( actor->bodyId );
				if ( actor->isRubble == false && world->time - actor->freeTime <= 0.02f && chunk->materialIndex == 0 &&
					 velocity.x * velocity.x + velocity.z * velocity.z > 4.0f * 4.0f )
				{
					thrownCount += 1;
				}
			}
		}
		ENSURE( thrownCount > 0 );

		// The concrete block started 2 m up
		nbWorld* world = nbGetWorldFromId( scene.world );
		float blockHeight = 0.0f;
		for ( int c = 0; c < world->chunks.count; ++c )
		{
			const nbChunk* chunk = world->chunks.data + c;
			if ( chunk->shape != NULL && chunk->materialIndex == 1 )
			{
				b3WorldTransform transform = nbActor_GetTransform( world, world->actors.data + chunk->actorIndex );
				blockHeight = b3MaxFloat( blockHeight, (float)b3TransformWorldPoint( transform, chunk->shape->centroid ).y );
			}
		}
		ENSURE( blockHeight < 1.5f );
		DestroyScene( &scene );
	}

	// A gate of stout pillars pre-fractured into cells carries its beam
	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 1.0f );
		CreateGate( &scene, 0.0f, 7 );
		Step( &scene, 30 );
		nbStats stats = nbWorld_GetStats( scene.world );
		ENSURE( stats.overloadedBondCount == 0 );
		ENSURE( stats.dynamicBodyCount == 0 );
		DestroyScene( &scene );
	}

	return 0;
}

// A cannon ball of 0.25 m and 7.8 t per cubic meter at 45 m/s from one point at another, as the demo fires it
static void ShootBall( TestScene* scene, b3Vec3 from, b3Vec3 to )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = from;
	bodyDef.linearVelocity = b3MulSV( 45.0f, b3Normalize( b3Sub( to, from ) ) );
	bodyDef.isBullet = true;
	b3BodyId ball = b3CreateBody( scene->physicsWorld, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = 7800.0f;
	shapeDef.enableHitEvents = true;
	b3Sphere sphere = { { 0.0f, 0.0f, 0.0f }, 0.25f };
	b3CreateSphereShape( ball, &shapeDef, &sphere );
}

// Volume of the chunks of a destructible with their centroid above a height
static float VolumeAbove( nbDestructibleId destructible, float height )
{
	int count = nbDestructible_GetChunkCount( destructible );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)( count + 1 ) );
	int written = nbDestructible_GetChunks( destructible, chunks, count );
	float volume = 0.0f;
	for ( int i = 0; i < written; ++i )
	{
		b3WorldTransform transform = b3Body_GetTransform( nbChunk_GetBody( chunks[i] ) );
		b3Pos centroid = b3TransformWorldPoint( transform, nbChunk_GetCentroid( chunks[i] ) );
		volume += (float)centroid.y > height ? nbChunk_GetVolume( chunks[i] ) : 0.0f;
	}
	free( chunks );
	return volume;
}

// The brick wall of the demo's wall scene, 9 m wide and 3.2 m high, with the demo's world: fragments twice the size,
// the damage of collisions, the friction of the ground. Eight cannon balls from where the demo's camera stands knock
// holes along its foot. With the load check on, as by default, it gives way under its weight: the pieces the balls
// knocked into the holes come back to life, the wall comes down to the last of it, and its parts break in two where
// they land hard. Without the check it stays up on what is left of its foot.
static int WallCollapseTest( void )
{
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene;
		b3WorldDef worldDef = b3DefaultWorldDef();
		scene.physicsWorld = b3CreateWorld( &worldDef );
		b3BodyDef groundDef = b3DefaultBodyDef();
		groundDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
		scene.groundId = b3CreateBody( scene.physicsWorld, &groundDef );
		b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
		b3ShapeDef groundShape = b3DefaultShapeDef();
		groundShape.baseMaterial.friction = 0.8f;
		b3CreateHullShape( scene.groundId, &groundShape, &box.base );

		nbWorldDef def = nbDefaultWorldDef();
		ENSURE( def.supportScale == 1.0f );
		def.physicsWorld = scene.physicsWorld;
		def.collisionRadiusScale = 0.02f;
		def.fragmentScale = 2.0f;
		def.supportScale = pass == 0 ? def.supportScale : 0.0f;
		scene.world = nbCreateWorld( &def );

		nbDestructibleDef wallDef = nbDefaultDestructibleDef();
		wallDef.position = b3ToPos( (b3Vec3){ 0.0f, 1.6f, 0.0f } );
		wallDef.seed = 1;
		wallDef.material.density = 1900.0f;
		wallDef.material.strength = 6.0e5f;
		wallDef.material.fragmentSize = 0.1f;
		wallDef.material.friction = 0.8f;
		nbDestructibleId wall = nbCreateBox( scene.world, &wallDef, (b3Vec3){ 4.5f, 1.6f, 0.15f } );
		ENSURE( VolumeAbove( wall, 1.5f ) > 4.7f );

		for ( int k = 0; k < 8; ++k )
		{
			ShootBall( &scene, (b3Vec3){ 0.0f, 1.8f, 9.0f }, (b3Vec3){ -3.5f + (float)k, 0.5f, 0.0f } );
			Step( &scene, 6 );
		}
		Step( &scene, 180 );

		nbStats stats = nbWorld_GetStats( scene.world );
		float high = VolumeAbove( wall, 1.5f );
		printf( "  %s: %.2f cubic meters above 1.5 m, %d bonds overloaded, %d parts broke where they landed\n",
				pass == 0 ? "with the load check" : "without", high, stats.overloadedBondCount, stats.landingSplitCount );
		if ( pass == 0 )
		{
			ENSURE( stats.overloadedBondCount > 0 );
			ENSURE( high < 0.1f );
			ENSURE( stats.landingSplitCount > 0 );
		}
		else
		{
			ENSURE( stats.overloadedBondCount == 0 );
			ENSURE( high > 4.0f );
			ENSURE( stats.landingSplitCount == 0 );
		}
		DestroyScene( &scene );
	}
	return 0;
}

// Only the load check reads the second moments of the bonds, see nbBondMoments. Without it nothing computes or keeps
// them. With it on from the start the bonds keep the moments of the faces they were made from, and when it comes on
// later, they get them from the shapes of their chunks, close to the same. Buildings with storeys and dynamic
// destructibles never need them, only static structures without floors.
static int BondMomentsTest( void )
{
	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 0.0f );
		CreateLoadedWall( &scene );
		Grenade( &scene, (b3Vec3){ 0.0f, 0.75f, 0.15f } );
		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->bondCount > 100 );
		ENSURE( world->bondMoments.count == 0 && world->bondMoments.capacity == 0 );
		DestroyScene( &scene );
	}

	nbBondMoments* kept = NULL;
	int keptCount = 0;
	float worst = 0.0f;
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateScene();
		if ( pass == 0 )
		{
			nbWorld_SetSupportScale( scene.world, 0.03f );
		}
		CreateLoadedWall( &scene );
		if ( pass == 1 )
		{
			nbWorld_SetSupportScale( scene.world, 0.03f );
		}

		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->destructibles.data[0].bondMoments );
		ENSURE( world->bondMoments.count == world->bonds.count );
		if ( pass == 0 )
		{
			keptCount = world->bondMoments.count;
			kept = malloc( sizeof( nbBondMoments ) * (size_t)keptCount );
			memcpy( kept, world->bondMoments.data, sizeof( nbBondMoments ) * (size_t)keptCount );
		}
		else
		{
			ENSURE( world->bondMoments.count == keptCount );
			for ( int i = 0; i < keptCount; ++i )
			{
				const float* a = &kept[i].moments.x;
				const float* b = &world->bondMoments.data[i].moments.x;
				for ( int k = 0; k < 6; ++k )
				{
					float error = fabsf( a[k] - b[k] ) / ( 1.0e-4f + fabsf( kept[i].moments.x + kept[i].moments.y + kept[i].moments.z ) );
					worst = error > worst ? error : worst;
				}
			}

			// New bonds keep them from now on
			Grenade( &scene, (b3Vec3){ 0.0f, 0.75f, 0.15f } );
			world = nbGetWorldFromId( scene.world );
			ENSURE( world->bondMoments.count == world->bonds.count );
		}
		DestroyScene( &scene );
	}
	free( kept );
	printf( "  worst relative difference of recomputed bond moments %g\n", (double)worst );
	ENSURE( worst < 1.0e-3f );

	// A building and a dynamic destructible get none, also when they break
	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 0.03f );
		nbDestructibleId house = CreateCelledHouse( &scene, 2, 3 );
		nbDestructibleDef rockDef = nbDefaultDestructibleDef();
		rockDef.position = b3ToPos( (b3Vec3){ 20.0f, 0.5f, 0.0f } );
		rockDef.isStatic = false;
		rockDef.cellSize = 0.3f;
		nbDestructibleId rock = nbCreateBox( scene.world, &rockDef, (b3Vec3){ 1.0f, 0.5f, 0.5f } );
		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->destructibles.data[house.index1 - 1].storeyCount > 0 );
		ENSURE( world->destructibles.data[house.index1 - 1].bondMoments == false );
		ENSURE( world->destructibles.data[rock.index1 - 1].bondMoments == false );
		ENSURE( world->bondCount > 100 && world->bondMoments.count == 0 );
		Grenade( &scene, (b3Vec3){ 0.0f, 1.2f, 3.4f } );
		Grenade( &scene, (b3Vec3){ 20.0f, 0.5f, 0.6f } );
		world = nbGetWorldFromId( scene.world );
		ENSURE( world->bondMoments.count == 0 );
		DestroyScene( &scene );
	}

	// A pillar with a slab on its side has a floor but no storey, the room under the slab holds no chunk. It takes the
	// load check like a wall, its bonds get their moments once its storeys are found, and the slab breaks off.
	{
		TestScene scene = CreateScene();
		nbPieceDef pieces[2] = { nbDefaultPieceDef(), nbDefaultPieceDef() };
		pieces[0].halfExtents = (b3Vec3){ 0.3f, 3.0f, 0.3f };
		pieces[0].transform.p = (b3Vec3){ 0.0f, 3.0f, 0.0f };
		pieces[1].halfExtents = (b3Vec3){ 2.0f, 0.1f, 2.0f };
		pieces[1].transform.p = (b3Vec3){ 2.3f, 2.0f, 0.0f };
		nbDestructibleDef def = nbDefaultDestructibleDef();
		nbDestructibleId stand = nbCreateDestructible( scene.world, &def, pieces, 2 );
		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->destructibles.data[stand.index1 - 1].storeyCount == 0 );
		ENSURE( world->destructibles.data[stand.index1 - 1].bondMoments );
		ENSURE( world->bondCount == 1 && world->bondMoments.count == world->bonds.count );
		Step( &scene, 30 );
		nbStats stats = nbWorld_GetStats( scene.world );
		ENSURE( stats.overloadedBondCount > 0 && stats.dynamicBodyCount > 0 );
		DestroyScene( &scene );
	}

	return 0;
}

// Every chunk slot of the world: its Box3D body and shape, actor, bonds and flags
static uint32_t HashChunkSlots( uint32_t hash, const nbWorld* world )
{
	for ( int i = 0; i < world->chunks.count; ++i )
	{
		const nbChunk* chunk = world->chunks.data + i;
		if ( chunk->shape == NULL )
		{
			continue;
		}

		int values[13] = {
			i,
			chunk->bodyId.index1,
			chunk->bodyId.generation,
			chunk->shapeId.index1,
			chunk->shapeId.generation,
			chunk->destructibleIndex,
			chunk->actorIndex,
			chunk->headBondKey,
			chunk->bondCount,
			chunk->depth,
			chunk->flags,
			chunk->materialIndex,
			chunk->interiorMaterial,
		};
		const uint8_t* bytes = (const uint8_t*)values;
		for ( size_t k = 0; k < sizeof( values ); ++k )
		{
			hash = ( hash ^ bytes[k] ) * 16777619u;
		}
	}
	return hash;
}

// Many destructibles created at once come out the same as one by one, with one worker or four and with a task system
// that runs every task at once: the same ids, chunks, bonds, bond moments and Box3D bodies and shapes, also after
// grenades. Entries without pieces get null ids.
static int BatchCreateTest( void )
{
	enum
	{
		houseCount = 4,
		entryCount = 9,
	};
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * houseCount );
	for ( int h = 0; h < houseCount; ++h )
	{
		BuildCelledHouse( houses + h, 2 + ( h & 1 ), (uint32_t)( 40 + h ), (b3Vec3){ 14.0f * (float)h, 0.0f, 0.0f } );
	}

	static const nbOpening openings[2] = {
		{ { -1.2f, 0.3f, 0.0f }, { 0.6f, 0.5f, 0.3f } },
		{ { 1.4f, -0.6f, 0.0f }, { 0.45f, 1.0f, 0.3f } },
	};
	nbPieceDef wall = nbDefaultPieceDef();
	wall.halfExtents = (b3Vec3){ 3.0f, 1.5f, 0.15f };
	wall.transform.p = (b3Vec3){ 0.0f, 1.5f, 0.0f };
	wall.openings = openings;
	wall.openingCount = 2;
	nbDestructibleDef wallDef = nbDefaultDestructibleDef();
	wallDef.position = b3ToPos( (b3Vec3){ 0.0f, 0.0f, 12.0f } );
	wallDef.cellSize = 0.4f;
	wallDef.seed = 9;

	static const b3Vec3 rockPoints[8] = {
		{ -0.5f, -0.4f, -0.45f }, { 0.55f, -0.45f, -0.4f }, { -0.45f, 0.5f, -0.5f }, { 0.5f, 0.45f, -0.45f },
		{ -0.5f, -0.5f, 0.5f },	  { 0.45f, -0.4f, 0.55f },	{ -0.4f, 0.45f, 0.45f }, { 0.5f, 0.55f, 0.5f },
	};
	nbPieceDef rock = nbDefaultPieceDef();
	rock.points = rockPoints;
	rock.pointCount = 8;
	nbDestructibleDef rockDef = nbDefaultDestructibleDef();
	rockDef.position = b3ToPos( (b3Vec3){ 20.0f, 3.0f, 12.0f } );
	rockDef.isStatic = false;
	rockDef.cellSize = 0.3f;
	rockDef.seed = 5;

	nbPieceDef box = nbDefaultPieceDef();
	box.halfExtents = (b3Vec3){ 1.0f, 0.5f, 0.5f };
	nbDestructibleDef boxDef = nbDefaultDestructibleDef();
	boxDef.position = b3ToPos( (b3Vec3){ -10.0f, 0.5f, 12.0f } );

	nbDestructibleDef defs[entryCount] = {
		houses[0].def, wallDef, nbDefaultDestructibleDef(), houses[1].def, rockDef, houses[2].def, boxDef, houses[3].def,
		nbDefaultDestructibleDef(),
	};
	const nbPieceDef* lists[entryCount] = {
		houses[0].pieces, &wall, NULL, houses[1].pieces, &rock, houses[2].pieces, &box, houses[3].pieces, &box,
	};
	int counts[entryCount] = {
		houses[0].pieceCount, 1, 0, houses[1].pieceCount, 1, houses[2].pieceCount, 1, houses[3].pieceCount, 0,
	};

	// One by one and at once, with four workers, one and an inline task system, then without and with the load check
	nbDestructibleId firstIds[entryCount];
	uint32_t hashes[6];
	uint32_t momentHashes[6];
	TestTaskSystem system = { 0 };
	for ( int pass = 0; pass < 6; ++pass )
	{
		TestScene scene = pass == 3 ? CreateSceneWithWorkers( 4, InlineEnqueue, InlineFinish, &system )
									: CreateSceneWithWorkers( pass == 2 ? 1 : 4, NULL, NULL, NULL );
		nbWorld_SetSupportScale( scene.world, pass >= 4 ? 0.03f : 0.0f );

		nbDestructibleId ids[entryCount];
		if ( pass == 0 || pass == 4 )
		{
			for ( int i = 0; i < entryCount; ++i )
			{
				ids[i] = nbCreateDestructible( scene.world, defs + i, lists[i], counts[i] );
			}
		}
		else
		{
			nbCreateDestructibles( scene.world, defs, lists, counts, entryCount, ids );
		}

		for ( int i = 0; i < entryCount; ++i )
		{
			ENSURE( ( counts[i] > 0 ) == NB_IS_NON_NULL( ids[i] ) );
			if ( pass == 0 )
			{
				firstIds[i] = ids[i];
			}
			ENSURE( ids[i].index1 == firstIds[i].index1 && ids[i].generation == firstIds[i].generation );
		}

		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->chunkCount > 300 && world->bondCount > 600 );
		uint32_t hash = HashChunkSlots( HashBonds( 2166136261u, world ), world );
		for ( int i = 0; i < entryCount; ++i )
		{
			hash = NB_IS_NON_NULL( ids[i] ) ? HashDestructible( hash, ids[i] ) : hash;
		}

		uint32_t momentHash = 2166136261u;
		const uint8_t* bytes = (const uint8_t*)world->bondMoments.data;
		for ( size_t k = 0; k < sizeof( nbBondMoments ) * (size_t)world->bondMoments.count; ++k )
		{
			momentHash = ( momentHash ^ bytes[k] ) * 16777619u;
		}
		momentHashes[pass] = momentHash;

		Grenade( &scene, (b3Vec3){ 0.0f, 1.2f, 3.4f } );
		Grenade( &scene, (b3Vec3){ 0.0f, 1.0f, 12.15f } );
		Step( &scene, 30 );
		Grenade( &scene, (b3Vec3){ 28.0f, 4.4f, -3.4f } );
		Step( &scene, 30 );
		world = nbGetWorldFromId( scene.world );
		hash = HashChunkSlots( HashBonds( hash, world ), world );
		for ( int i = 0; i < entryCount; ++i )
		{
			hash = NB_IS_NON_NULL( ids[i] ) ? HashDestructible( hash, ids[i] ) : hash;
		}
		hashes[pass] = hash;
		DestroyScene( &scene );
	}
	free( houses );

	ENSURE( hashes[1] == hashes[0] && hashes[2] == hashes[0] && hashes[3] == hashes[0] );
	ENSURE( system.enqueueCount > 0 && system.finishCount == 0 );
	ENSURE( hashes[5] == hashes[4] );
	ENSURE( momentHashes[5] == momentHashes[4] && momentHashes[4] != momentHashes[0] );
	return 0;
}

static uint32_t HashBytes( uint32_t hash, const void* data, size_t size )
{
	const uint8_t* bytes = (const uint8_t*)data;
	for ( size_t k = 0; k < size; ++k )
	{
		hash = ( hash ^ bytes[k] ) * 16777619u;
	}
	return hash;
}

// Everything of a destructible that does not depend on the slots it took: its chunks in order with their geometry and
// material, and their bonds with the place of the other chunk in that order
static uint32_t SignDestructible( nbWorldId worldId, nbDestructibleId id )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	const nbDestructible* destructible = world->destructibles.data + ( id.index1 - 1 );
	uint32_t hash = 2166136261u;
	int place = 0;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		for ( int c = world->actors.data[a].headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			world->chunks.data[c].scratch = place++;
		}
	}

	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		for ( int c = world->actors.data[a].headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			const nbChunk* chunk = world->chunks.data + c;
			nbGeometry geometry = nbChunk_GetGeometry( nbMakeChunkId( world, c ) );
			hash = HashBytes( hash, geometry.vertices, sizeof( b3Vec3 ) * (size_t)geometry.vertexCount );
			hash = HashBytes( hash, geometry.planes, sizeof( b3Plane ) * (size_t)geometry.faceCount );
			hash = HashBytes( hash, geometry.faces, sizeof( nbFace ) * (size_t)geometry.faceCount );
			hash = HashBytes( hash, geometry.indices, (size_t)geometry.indexCount );
			int values[5] = { chunk->bondCount, chunk->depth, chunk->flags & ( nb_chunkAnchored | nb_chunkStaticBody ),
							  chunk->materialIndex, chunk->interiorMaterial };
			hash = HashBytes( hash, values, sizeof( values ) );
			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; key = world->bonds.data[key >> 1].nextKey[key & 1] )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				float floats[8] = { bond->centroid.x, bond->centroid.y, bond->centroid.z, bond->area,
									bond->normal.x,	  bond->normal.y,	bond->normal.z,	  bond->health };
				int ints[4] = { world->chunks.data[bond->chunk[1 - ( key & 1 )]].scratch, key & 1, bond->cohesive,
								bond->sibling };
				hash = HashBytes( hash, floats, sizeof( floats ) );
				hash = HashBytes( hash, ints, sizeof( ints ) );
			}
		}
	}

	b3WorldTransform transform = b3Body_GetTransform( destructible->staticBody );
	return HashBytes( hash, &transform, sizeof( transform ) );
}

// An intact house with nothing near it can go and come back as it was. Damage keeps it, and so does whatever could lie on
// it or hit it: falling debris and the rubble it turns into.
static int StreamTest( void )
{
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * 2 );
	BuildCelledHouse( houses + 0, 2, 61, b3Vec3_zero );
	BuildCelledHouse( houses + 1, 3, 62, (b3Vec3){ 40.0f, 0.0f, 0.0f } );
	nbDestructibleDef defs[2] = { houses[0].def, houses[1].def };
	const nbPieceDef* lists[2] = { houses[0].pieces, houses[1].pieces };
	int counts[2] = { houses[0].pieceCount, houses[1].pieceCount };

	TestScene scene = CreateSceneWithWorkers( 4, NULL, NULL, NULL );
	nbDestructibleId ids[2];
	nbCreateDestructibles( scene.world, defs, lists, counts, 2, ids );
	ENSURE( nbDestructible_IsIntact( ids[0] ) && nbDestructible_IsIntact( ids[1] ) );
	ENSURE( nbDestructible_CanUnload( ids[0], 1.0f ) && nbDestructible_CanUnload( ids[1], 1.0f ) );
	uint32_t signature = SignDestructible( scene.world, ids[0] );
	int chunkCount = nbDestructible_GetChunkCount( ids[0] );

	// The other house takes a grenade on its far side, and a dynamic box is never unloaded
	Grenade( &scene, (b3Vec3){ 44.7f, 1.2f, 0.0f } );
	ENSURE( nbDestructible_IsIntact( ids[1] ) == false && nbDestructible_CanUnload( ids[1], 1.0f ) == false );
	nbDestructibleId box = CreateLooseBox( &scene, (b3Vec3){ -20.0f, 0.3f, 0.0f }, (b3Vec3){ 0.3f, 0.3f, 0.3f }, 7 );
	ENSURE( nbDestructible_IsIntact( box ) && nbDestructible_CanUnload( box, 1.0f ) == false );
	Step( &scene, 30 );

	// Unloading reports every chunk as destroyed and none as exposed
	nbWorld_GetEvents( scene.world );
	nbWorld_GetEvents( scene.world );
	ENSURE( nbDestructible_CanUnload( ids[0], 1.0f ) );
	nbDestroyDestructible( ids[0] );
	ENSURE( nbDestructible_IsValid( ids[0] ) == false && nbDestructible_IsIntact( ids[0] ) == false );
	nbEvents events = nbWorld_GetEvents( scene.world );
	ENSURE( events.destroyedCount == chunkCount && events.exposedCount == 0 );

	// It comes back the same, in other slots
	Step( &scene, 30 );
	ids[0] = nbCreateDestructible( scene.world, defs + 0, lists[0], counts[0] );
	ENSURE( SignDestructible( scene.world, ids[0] ) == signature );
	ENSURE( nbDestructible_IsIntact( ids[0] ) && nbDestructible_CanUnload( ids[0], 1.0f ) );

	// The box far away counts only with a margin that reaches it
	ENSURE( nbDestructible_CanUnload( ids[0], 30.0f ) == false );

	// A box dropped onto the roof keeps the house while it falls and once it froze there
	nbDestructibleId roofBox = CreateLooseBox( &scene, (b3Vec3){ 0.5f, 7.2f, 0.0f }, (b3Vec3){ 0.3f, 0.3f, 0.3f }, 8 );
	ENSURE( nbDestructible_CanUnload( ids[0], 1.0f ) == false );
	int rubble = nbWorld_GetStats( scene.world ).rubbleCount;
	Step( &scene, 300 );
	ENSURE( nbWorld_GetStats( scene.world ).rubbleCount > rubble );
	ENSURE( b3Body_GetType( HeaviestBody( roofBox ) ) == b3_staticBody );
	ENSURE( nbDestructible_IsIntact( ids[0] ) && nbDestructible_CanUnload( ids[0], 1.0f ) == false );

	// Damage that breaks nothing still counts. Fragments larger than the cells keep the impact from refining them.
	nbDestroyDestructible( roofBox );
	ENSURE( nbDestructible_CanUnload( ids[0], 1.0f ) );
	nbWorld_SetFragmentScale( scene.world, 100.0f );
	nbStats before = nbWorld_GetStats( scene.world );
	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ -1.25f, 1.2f, 3.4f };
	impact.radius = 0.5f;
	impact.damage = 1.0f;
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	nbStats after = nbWorld_GetStats( scene.world );
	ENSURE( result.fracturedChunkCount == 0 && after.impactCount == before.impactCount + 1 );
	ENSURE( after.bondCount == before.bondCount && after.chunkCount == before.chunkCount );
	ENSURE( nbDestructible_IsIntact( ids[0] ) == false && nbDestructible_CanUnload( ids[0], 1.0f ) == false );

	DestroyScene( &scene );
	free( houses );
	return 0;
}

// A task system that runs a task only when it is finished, so nothing runs before the caller waits for it
typedef struct DeferredTaskSystem
{
	b3TaskCallback* tasks[64];
	void* contexts[64];
	int count;
	int runCount;
} DeferredTaskSystem;

static void* DeferredEnqueue( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName )
{
	(void)taskName;
	DeferredTaskSystem* system = userContext;
	int slot = system->count % 64;
	system->count += 1;
	system->tasks[slot] = task;
	system->contexts[slot] = taskContext;
	return system->contexts + slot;
}

static void DeferredFinish( void* userTask, void* userContext )
{
	DeferredTaskSystem* system = userContext;
	int slot = (int)( (void**)userTask - system->contexts );
	system->runCount += 1;
	system->tasks[slot]( system->contexts[slot] );
}

// The chunk slots, bonds and destructibles of a world
static uint32_t HashCreated( const TestScene* scene, const nbDestructibleId* ids, int count )
{
	const nbWorld* world = nbGetWorldFromId( scene->world );
	uint32_t hash = HashChunkSlots( HashBonds( 2166136261u, world ), world );
	for ( int i = 0; i < count; ++i )
	{
		hash = NB_IS_NON_NULL( ids[i] ) ? HashDestructible( hash, ids[i] ) : hash;
	}
	return hash;
}

// The second moments every bond keeps
static uint32_t HashMoments( const TestScene* scene )
{
	const nbWorld* world = nbGetWorldFromId( scene->world );
	return HashBytes( 2166136261u, world->bondMoments.data, sizeof( nbBondMoments ) * (size_t)world->bondMoments.count );
}

// Destructibles created in the background come out as if created at once when the creation finishes: the same ids,
// chunks, bonds and Box3D bodies, with the built-in threads, with one thread, with a task system that runs every task
// at once and with one that runs them only when they are finished. Steps in between change nothing, and neither does a
// new number of workers. Should the load check come on in between, the bonds get the moments the check measures for
// the destructibles that were there before, and should it go off, they keep none. A world destroyed with a creation
// in flight drops it.
static int BackgroundCreateTest( void )
{
	enum
	{
		houseCount = 4,
		entryCount = 7,
	};
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * houseCount );
	for ( int h = 0; h < houseCount; ++h )
	{
		BuildCelledHouse( houses + h, 2 + ( h & 1 ), (uint32_t)( 70 + h ), (b3Vec3){ 14.0f * (float)h, 0.0f, 0.0f } );
	}

	static const nbOpening openings[2] = {
		{ { -1.2f, 0.3f, 0.0f }, { 0.6f, 0.5f, 0.3f } },
		{ { 1.4f, -0.6f, 0.0f }, { 0.45f, 1.0f, 0.3f } },
	};
	nbPieceDef wall = nbDefaultPieceDef();
	wall.halfExtents = (b3Vec3){ 3.0f, 1.5f, 0.15f };
	wall.transform.p = (b3Vec3){ 0.0f, 1.5f, 0.0f };
	wall.openings = openings;
	wall.openingCount = 2;
	nbDestructibleDef wallDef = nbDefaultDestructibleDef();
	wallDef.position = b3ToPos( (b3Vec3){ 0.0f, 0.0f, 12.0f } );
	wallDef.cellSize = 0.4f;
	wallDef.seed = 9;

	nbPieceDef box = nbDefaultPieceDef();
	box.halfExtents = (b3Vec3){ 1.0f, 0.5f, 0.5f };
	nbDestructibleDef boxDef = nbDefaultDestructibleDef();
	boxDef.position = b3ToPos( (b3Vec3){ -10.0f, 0.5f, 12.0f } );
	boxDef.isStatic = false;

	nbDestructibleDef defs[entryCount] = {
		houses[0].def, wallDef, nbDefaultDestructibleDef(), houses[1].def, houses[2].def, boxDef, houses[3].def,
	};
	const nbPieceDef* lists[entryCount] = {
		houses[0].pieces, &wall, NULL, houses[1].pieces, houses[2].pieces, &box, houses[3].pieces,
	};
	int counts[entryCount] = {
		houses[0].pieceCount, 1, 0, houses[1].pieceCount, houses[2].pieceCount, 1, houses[3].pieceCount,
	};

	// At once, then in the background with four threads, one, two task systems and a new number of threads in between
	nbDestructibleId firstIds[entryCount];
	uint32_t hashes[6][2];
	TestTaskSystem inlineSystem = { 0 };
	DeferredTaskSystem deferredSystem = { 0 };
	for ( int pass = 0; pass < 6; ++pass )
	{
		TestScene scene = pass == 3	  ? CreateSceneWithWorkers( 4, InlineEnqueue, InlineFinish, &inlineSystem )
						  : pass == 4 ? CreateSceneWithWorkers( 4, DeferredEnqueue, DeferredFinish, &deferredSystem )
									  : CreateSceneWithWorkers( pass == 2 ? 1 : 4, NULL, NULL, NULL );

		nbDestructibleId ids[entryCount];
		if ( pass == 0 )
		{
			Step( &scene, 10 );
			nbCreateDestructibles( scene.world, defs, lists, counts, entryCount, ids );
		}
		else
		{
			nbCreationId creation = nbStartCreating( scene.world, defs, lists, counts, entryCount );
			ENSURE( NB_IS_NON_NULL( creation ) );
			Step( &scene, 10 );
			if ( pass == 5 )
			{
				nbWorld_SetWorkerCount( scene.world, 2 );
			}
			nbFinishCreating( creation, ids );

			// A finished creation is gone
			nbDestructibleId again[entryCount];
			again[0] = nb_nullDestructibleId;
			nbFinishCreating( creation, again );
			ENSURE( NB_IS_NULL( again[0] ) );
		}

		for ( int i = 0; i < entryCount; ++i )
		{
			ENSURE( ( counts[i] > 0 ) == NB_IS_NON_NULL( ids[i] ) );
			if ( pass == 0 )
			{
				firstIds[i] = ids[i];
			}
			ENSURE( ids[i].index1 == firstIds[i].index1 && ids[i].generation == firstIds[i].generation );
		}

		hashes[pass][0] = HashCreated( &scene, ids, entryCount );

		// The workers pack the shapes of a destructible into one block, created at once they keep their own
		const nbWorld* world = nbGetWorldFromId( scene.world );
		for ( int i = 0; i < entryCount; ++i )
		{
			const nbDestructible* destructible = NB_IS_NON_NULL( ids[i] ) ? world->destructibles.data + ids[i].index1 - 1 : NULL;
			ENSURE( destructible == NULL || ( destructible->shapeBlock != NULL ) == ( pass > 0 ) );
			ENSURE( destructible == NULL || pass == 0 ||
					( 0 < destructible->shapeBlockCount && destructible->shapeBlockCount <= destructible->chunkCount ) );
		}
		int blockCount = world->destructibles.data[ids[0].index1 - 1].shapeBlockCount;

		Grenade( &scene, (b3Vec3){ 0.0f, 1.2f, 3.4f } );
		Grenade( &scene, (b3Vec3){ 0.0f, 1.0f, 12.15f } );
		Step( &scene, 30 );
		Grenade( &scene, (b3Vec3){ 28.0f, 4.4f, -3.4f } );
		Step( &scene, 30 );
		hashes[pass][1] = HashCreated( &scene, ids, entryCount );

		// Fractured chunks count off the block, the last chunk of the house takes it along
		const nbDestructible* house = world->destructibles.data + ids[0].index1 - 1;
		ENSURE( pass == 0 || ( house->shapeBlock != NULL && house->shapeBlockCount < blockCount ) );
		nbDestroyDestructible( ids[0] );
		ENSURE( house->shapeBlock == NULL );
		DestroyScene( &scene );
	}

	for ( int pass = 1; pass < 6; ++pass )
	{
		ENSURE( hashes[pass][0] == hashes[0][0] && hashes[pass][1] == hashes[0][1] );
	}
	ENSURE( inlineSystem.enqueueCount > 0 && deferredSystem.runCount > 0 );

	// The load check comes on while the workers prepare: the moments are measured as for those created before it came on
	uint32_t momentHashes[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateSceneWithWorkers( 4, NULL, NULL, NULL );
		nbDestructibleId ids[entryCount];
		if ( pass == 0 )
		{
			nbCreateDestructibles( scene.world, defs, lists, counts, entryCount, ids );
			nbWorld_SetSupportScale( scene.world, 0.03f );
		}
		else
		{
			nbCreationId creation = nbStartCreating( scene.world, defs, lists, counts, entryCount );
			nbWorld_SetSupportScale( scene.world, 0.03f );
			nbFinishCreating( creation, ids );
		}

		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->destructibles.data[ids[1].index1 - 1].bondMoments );
		ENSURE( world->destructibles.data[ids[0].index1 - 1].bondMoments == false );
		ENSURE( HashCreated( &scene, ids, entryCount ) == hashes[0][0] );
		momentHashes[pass] = HashMoments( &scene );
		DestroyScene( &scene );
	}
	ENSURE( momentHashes[1] == momentHashes[0] );

	// It goes off in between: no moments are kept, as if it had been off all along
	{
		TestScene scene = CreateSceneWithWorkers( 4, NULL, NULL, NULL );
		nbWorld_SetSupportScale( scene.world, 0.03f );
		nbCreationId creation = nbStartCreating( scene.world, defs, lists, counts, entryCount );
		nbWorld_SetSupportScale( scene.world, 0.0f );
		nbDestructibleId ids[entryCount];
		nbFinishCreating( creation, ids );
		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->destructibles.data[ids[1].index1 - 1].bondMoments == false );
		ENSURE( HashCreated( &scene, ids, entryCount ) == hashes[0][0] );
		DestroyScene( &scene );
	}

	// More than the threads take in the background: the rest is prepared right away, and an impact in between still gets
	// its tasks run
	{
		enum
		{
			manyCount = 6 * entryCount,
		};
		nbDestructibleDef manyDefs[manyCount];
		const nbPieceDef* manyLists[manyCount];
		int manyCounts[manyCount];
		for ( int i = 0; i < manyCount; ++i )
		{
			manyDefs[i] = defs[i % entryCount];
			manyLists[i] = lists[i % entryCount];
			manyCounts[i] = counts[i % entryCount];
		}

		nbDestructibleDef targetDef = wallDef;
		targetDef.position = b3ToPos( (b3Vec3){ 0.0f, 0.0f, 40.0f } );
		uint32_t manyHashes[2];
		for ( int pass = 0; pass < 2; ++pass )
		{
			TestScene scene = CreateSceneWithWorkers( 4, NULL, NULL, NULL );
			nbDestructibleId target = nbCreateDestructible( scene.world, &targetDef, &wall, 1 );
			nbDestructibleId ids[manyCount];
			if ( pass == 0 )
			{
				Grenade( &scene, (b3Vec3){ 0.0f, 1.0f, 40.15f } );
				nbCreateDestructibles( scene.world, manyDefs, manyLists, manyCounts, manyCount, ids );
			}
			else
			{
				nbCreationId creation = nbStartCreating( scene.world, manyDefs, manyLists, manyCounts, manyCount );
				Grenade( &scene, (b3Vec3){ 0.0f, 1.0f, 40.15f } );
				nbFinishCreating( creation, ids );
			}
			ENSURE( nbDestructible_IsIntact( target ) == false );
			manyHashes[pass] = HashCreated( &scene, ids, manyCount );
			DestroyScene( &scene );
		}
		ENSURE( manyHashes[1] == manyHashes[0] );
	}

	// A world that goes with creations in flight, one never started by its tasks
	{
		DeferredTaskSystem system = { 0 };
		TestScene scene = CreateSceneWithWorkers( 4, NULL, NULL, NULL );
		nbStartCreating( scene.world, defs, lists, counts, entryCount );
		nbStartCreating( scene.world, defs + 3, lists + 3, counts + 3, 2 );
		DestroyScene( &scene );

		scene = CreateSceneWithWorkers( 4, DeferredEnqueue, DeferredFinish, &system );
		nbStartCreating( scene.world, defs, lists, counts, entryCount );
		DestroyScene( &scene );
		ENSURE( system.runCount == entryCount - 1 );
	}

	free( houses );
	return 0;
}

// The rubble actors of a destructible
static int GetRubble( const nbWorld* world, nbDestructibleId id, int* actors, int capacity )
{
	int count = 0;
	const nbDestructible* destructible = world->destructibles.data + id.index1 - 1;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		if ( world->actors.data[a].isRubble && count < capacity )
		{
			actors[count++] = a;
		}
	}
	return count;
}

// Step until destructibles can be saved, at most a minute
static bool StepUntilSaveable( TestScene* scene, const nbDestructibleId* ids, int count )
{
	for ( int steps = 0; steps < 3600; steps += 30 )
	{
		if ( nbCanSaveDestructibles( ids, count, 2.0f ) )
		{
			return true;
		}
		Step( scene, 30 );
	}
	return false;
}

// A save of destructibles, written twice: the size first
static uint8_t* SaveToBuffer( const nbDestructibleId* ids, int count, size_t* size )
{
	*size = nbSaveDestructibles( ids, count, NULL, 0 );
	uint8_t* buffer = malloc( *size + 1 );
	if ( *size == 0 || nbSaveDestructibles( ids, count, buffer, *size ) != *size )
	{
		free( buffer );
		return NULL;
	}
	return buffer;
}

// Damaged houses at rest go out of the world and come back to the bit, see nbSaveDestructibles
static int SaveLoadTest( void )
{
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * 2 );
	BuildCelledHouse( houses + 0, 2, 81, b3Vec3_zero );
	BuildCelledHouse( houses + 1, 2, 82, (b3Vec3){ 30.0f, 0.0f, 0.0f } );

	uint32_t hashes[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateSceneWithWorkers( pass == 0 ? 1 : 4, NULL, NULL, NULL );
		nbDestructibleId ids[2];
		ids[0] = nbCreateDestructible( scene.world, &houses[0].def, houses[0].pieces, houses[0].pieceCount );
		ids[1] = nbCreateDestructible( scene.world, &houses[1].def, houses[1].pieces, houses[1].pieceCount );
		const nbWorld* world = nbGetWorldFromId( scene.world );

		// Debris that moves keeps the house, and it cannot be saved
		Grenade( &scene, (b3Vec3){ 0.0f, 1.2f, 3.4f } );
		Grenade( &scene, (b3Vec3){ 3.0f, 4.4f, -3.4f } );
		ENSURE( nbCanSaveDestructibles( ids, 1, 2.0f ) == false && nbSaveDestructibles( ids, 1, NULL, 0 ) == 0 );
		ENSURE( StepUntilSaveable( &scene, ids, 1 ) );
		int rubble[64];
		ENSURE( GetRubble( world, ids[0], rubble, 64 ) > 0 );

		// A buffer too small stays as it was
		size_t size;
		uint8_t* first = SaveToBuffer( ids, 1, &size );
		ENSURE( first != NULL );
		uint8_t* other = malloc( size );
		memset( other, 0xab, size );
		ENSURE( nbSaveDestructibles( ids, 1, other, size - 1 ) == size && other[0] == 0xab );

		nbWorld_GetEvents( scene.world );
		nbWorld_GetEvents( scene.world );
		nbStats before = nbWorld_GetStats( scene.world );
		int chunkCount = nbDestructible_GetChunkCount( ids[0] );
		nbDestroyDestructible( ids[0] );
		nbEvents events = nbWorld_GetEvents( scene.world );
		ENSURE( events.destroyedCount == chunkCount );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount < before.rubbleCount );
		Step( &scene, 30 );

		// What is not a save of this build is refused, and nothing changes
		nbDestructibleId loaded = nb_nullDestructibleId;
		memcpy( other, first, size );
		other[0] ^= 1;
		ENSURE( nbLoadDestructibles( scene.world, other, size, &loaded ) == false );
		ENSURE( nbLoadDestructibles( scene.world, first, size - 16, &loaded ) == false );
		ENSURE( NB_IS_NULL( loaded ) && nbWorld_GetStats( scene.world ).chunkCount == before.chunkCount - chunkCount );

		// It comes back with every chunk reported as created, and saves to the same bytes
		ENSURE( nbLoadDestructibles( scene.world, first, size, &loaded ) );
		events = nbWorld_GetEvents( scene.world );
		ENSURE( events.createdCount == chunkCount && nbDestructible_IsIntact( loaded ) == false );
		nbStats after = nbWorld_GetStats( scene.world );
		ENSURE( after.chunkCount == before.chunkCount && after.bondCount == before.bondCount );
		ENSURE( after.rubbleCount == before.rubbleCount && after.debrisCount == before.debrisCount );
		ENSURE( after.staticBodyCount == before.staticBodyCount && after.dynamicBodyCount == before.dynamicBodyCount );
		ENSURE( nbCanSaveDestructibles( &loaded, 1, 2.0f ) );
		uint8_t* second = SaveToBuffer( &loaded, 1, &size );
		ENSURE( second != NULL && memcmp( first, second, size ) == 0 );
		free( second );

		// Its rubble comes back to life like any other, the smallest piece where a grenade falls. Large pieces stay.
		int rubbleCount = GetRubble( world, loaded, rubble, 64 );
		ENSURE( rubbleCount > 0 );
		int smallest = rubble[0];
		for ( int k = 1; k < rubbleCount; ++k )
		{
			smallest = world->actors.data[rubble[k]].volume < world->actors.data[smallest].volume ? rubble[k] : smallest;
		}
		b3AABB box = b3Shape_GetAABB( world->chunks.data[world->actors.data[smallest].headChunk].shapeId );
		Grenade( &scene, b3MulSV( 0.5f, b3Add( box.lowerBound, box.upperBound ) ) );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount < after.rubbleCount );
		Step( &scene, 120 );
		hashes[pass] = HashCreated( &scene, &loaded, 1 );

		// Rubble of one house on the other links them: they go together, and the link comes back between the new ones
		ENSURE( StepUntilSaveable( &scene, &loaded, 1 ) );
		rubbleCount = GetRubble( world, loaded, rubble, 64 );
		ENSURE( rubbleCount > 0 );
		nbActor* linked = world->actors.data + rubble[0];
		const nbDestructible* house = world->destructibles.data + ids[1].index1 - 1;
		int carrier = world->actors.data[house->headActor].headChunk;
		linked->carriers[0] = (nbCarrier){ linked->carriers[0].point, carrier, world->chunks.data[carrier].generation };
		linked->carrierCount = linked->carrierCount > 0 ? linked->carrierCount : 1;
		nbDestructibleId pair[2] = { loaded, ids[1] };
		ENSURE( nbCanSaveDestructibles( pair, 1, 2.0f ) == false && nbCanSaveDestructibles( pair + 1, 1, 2.0f ) == false );
		ENSURE( nbSaveDestructibles( pair, 1, NULL, 0 ) == 0 && nbCanSaveDestructibles( pair, 2, 2.0f ) );

		// A hull from quickhull goes along on its own
		nbChunk* chunk = world->chunks.data + carrier;
		b3HullData* hull = b3CreateHull( chunk->shape->vertices, chunk->shape->vertexCount, B3_MAX_HULL_VERTICES );
		ENSURE( hull != NULL );
		chunk->shape->hull = hull;

		free( first );
		first = SaveToBuffer( pair, 2, &size );
		ENSURE( first != NULL );
		nbDestroyDestructible( pair[0] );
		nbDestroyDestructible( pair[1] );
		nbDestructibleId back[2];
		ENSURE( nbLoadDestructibles( scene.world, first, size, back ) );
		const nbActor* restored = NULL;
		for ( int a = world->destructibles.data[back[0].index1 - 1].headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
		{
			const nbActor* actor = world->actors.data + a;
			for ( int k = 0; k < actor->carrierCount; ++k )
			{
				int c = actor->carriers[k].chunkIndex;
				if ( c != NB_NULL_INDEX && world->chunks.data[c].generation == actor->carriers[k].generation &&
					 world->chunks.data[c].destructibleIndex == back[1].index1 - 1 )
				{
					restored = actor;
				}
			}
		}
		ENSURE( restored != NULL );
		second = SaveToBuffer( back, 2, &size );
		ENSURE( second != NULL && memcmp( first, second, size ) == 0 );
		free( first );
		free( second );
		free( other );
		DestroyScene( &scene );
	}

	ENSURE( hashes[0] == hashes[1] );
	free( houses );
	return 0;
}

// Box3D and Nebenan on one pool of threads end as with one thread: houses prepared in the background, grenades whose
// debris keeps the workers of Box3D busy, and a second world on the pool after the first one went. A step without
// many awake contacts hands no task to the pool, see b3GetStepWorkerCount. In some steps the debris of one house holds
// so much of the work that a team of all workers solves its pack, see b3Counters::teamWork.
static int TaskSystemTest( void )
{
	enum
	{
		houseCount = 3
	};
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * houseCount );
	nbDestructibleDef defs[houseCount];
	const nbPieceDef* lists[houseCount];
	int counts[houseCount];
	for ( int h = 0; h < houseCount; ++h )
	{
		BuildCelledHouse( houses + h, 2, (uint32_t)( 90 + h ), (b3Vec3){ 14.0f * (float)h, 0.0f, 0.0f } );
		defs[h] = houses[h].def;
		lists[h] = houses[h].pieces;
		counts[h] = houses[h].pieceCount;
	}

	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	uint32_t hashes[3];
	for ( int pass = 0; pass < 3; ++pass )
	{
		TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, 4 );
		nbDestructibleId ids[houseCount];
		nbCreationId creation = nbStartCreating( scene.world, defs, lists, counts, houseCount );
		Step( &scene, 2 );
		nbFinishCreating( creation, ids );
		Step( &scene, 1 );
		ENSURE( b3World_GetCounters( scene.physicsWorld ).taskCount == 0 );

		for ( int h = 0; h < houseCount; ++h )
		{
			Grenade( &scene, (b3Vec3){ 14.0f * (float)h, 1.2f, 3.4f } );
			Grenade( &scene, (b3Vec3){ 14.0f * (float)h - 2.0f, 4.4f, -3.4f } );
		}

		int maxAwake = 0;
		int maxTasks = 0;
		int teamSteps = 0;
		int joinSteps = 0;
		int timedSteps = 0;
		int sampledSteps = 0;
		int packSampleSteps = 0;
		int heldSteps = 0;
		for ( int i = 0; i < 180; ++i )
		{
			Step( &scene, 1 );
			b3Counters counters = b3World_GetCounters( scene.physicsWorld );

			// Every eighth step samples the solver, also without workers: it measures the work of the islands and what the
			// threads spent solving whole packs, the work of those packs is part of it, and a step that times its team is
			// one of them, see b3Counters::solverSample
			b3Profile stepProfile = b3World_GetProfile( scene.physicsWorld );
			sampledSteps += counters.solverSample;
			ENSURE( counters.teamSample <= counters.solverSample );
			ENSURE( 0 <= counters.packSolveWork && counters.packSolveWork <= counters.islandWork );
			ENSURE( counters.solverSample == 0 ? counters.packSolveWork == 0 && stepProfile.packSolveTime == 0.0f
											   : counters.islandWork > 0 );
			ENSURE( ( counters.packSolveWork > 0 ) == ( stepProfile.packSolveTime > 0.0f ) );
			ENSURE( pass > 0 || counters.packSolveWork == 0 );
			packSampleSteps += counters.packSolveWork > 0 ? 1 : 0;
			heldSteps += counters.packHoldSteps > 0 ? 1 : 0;
			maxAwake = b3MaxInt( maxAwake, counters.awakeContactCount );
			maxTasks = b3MaxInt( maxTasks, counters.taskCount );
			teamSteps += counters.teamWork > 0 ? 1 : 0;
			joinSteps += counters.teamPackCount > 1 ? 1 : 0;
			timedSteps += counters.teamSample;
			ENSURE( ( counters.teamWork > 0 ) == ( counters.teamPackCount > 0 ) );
			ENSURE( counters.teamSample == 0 ||
					( counters.teamWork > 0 && 2 <= counters.teamWorkerCount && counters.teamWorkerCount <= 4 ) );

			// The stages of a timed step and what the calling thread did alone between them and after the last fill the
			// time of its team, and every thread took part in each stage with a barrier while it was open, see
			// b3TeamStages
			b3TeamStages stages = b3World_GetTeamStages( scene.physicsWorld );
			if ( counters.teamSample == 0 )
			{
				ENSURE( stages.rows[b3_teamColors].barrierCount == 0.0f && stages.serialTime == 0.0f );
				continue;
			}

			const float slack = 1.0e-3f;
			float barriers = 0.0f;
			float time = stages.serialTime + stages.tailTime;
			for ( int r = 0; r < b3_teamRowCount; ++r )
			{
				const b3TeamStage* row = stages.rows + r;
				float threadTime = row->workTime + row->packTime + row->lateTime + row->doneTime;
				ENSURE( fabsf( threadTime - row->openTime * (float)counters.teamWorkerCount ) <= slack );
				ENSURE( row->callerBlockCount <= row->blockCount && row->packTime >= 0.0f && row->lateTime >= 0.0f );
				barriers += row->barrierCount;
				time += row->openTime + row->aloneTime;
			}
			ENSURE( barriers > 0.0f && (int)barriers == counters.teamStageCount );
			ENSURE( fabsf( time - b3World_GetProfile( scene.physicsWorld ).teamTime ) <= slack );
			ENSURE( stages.entryCount + stages.missedCount == barriers * (float)( counters.teamWorkerCount - 1 ) );
			ENSURE( stages.yieldEntryCount <= stages.entryCount && stages.missedPackCount <= stages.missedCount );
			ENSURE( stages.mergeTime <= stages.serialTime + slack && stages.entryLateTime >= 0.0f );
		}
		ENSURE( maxAwake >= 1000 && ( pass == 0 || maxTasks > 0 ) );
		ENSURE( pass == 0 ? teamSteps == 0 : teamSteps > 0 );

		// Every eighth step with a team timed where its constraints went, see b3Profile::teamTime. The parts of the calling
		// thread lie within the time of its team.
		nbProfile profile = nbWorld_GetProfile( scene.world );
		ENSURE( profile.teamSampleCount == timedSteps && timedSteps <= teamSteps );
		ENSURE( profile.teamJoinStepCount == joinSteps && joinSteps <= teamSteps );
		ENSURE( profile.solverSampleCount == sampledSteps && 0 < sampledSteps && sampledSteps <= ( 180 + 7 ) / 8 + 1 );
		ENSURE( profile.solverWork > 0.0f && profile.solverConstraints > 0.0f && profile.packSolveWork <= profile.solverWork );

		// Whole packs come in the sampled steps of the first 90, unless the threads wait long for the last packs again and
		// again, as on a busy machine, and the solver keeps the barriers for a while, see b3Counters::packHoldSteps
		ENSURE( ( profile.packSolveWork > 0.0f ) == ( packSampleSteps > 0 ) &&
				( pass == 0 || packSampleSteps > 0 || heldSteps > 0 ) );
		ENSURE( ( profile.teamSampleWork > 0.0f ) == ( timedSteps > 0 ) );
		if ( pass == 1 )
		{
			printf( "  team in %d of 180 steps, %d of them with further packs, %d timed, overflow alone %.3f of %.3f ms\n",
					teamSteps, joinSteps, timedSteps, profile.teamOverflow, profile.teamTime );
		}

		if ( timedSteps > 0 )
		{
			const float slack = 1.0e-3f;
			ENSURE( profile.teamTime > 0.0f && profile.teamStageCount > 0.0f );
			ENSURE( profile.teamOverflow + profile.teamStageWait <= profile.teamTime + slack );
			ENSURE( profile.teamWorkerWait >= -slack && profile.teamWorkerPacks >= 0.0f && profile.teamYieldCount >= 0.0f );
		}
		hashes[pass] = HashCreated( &scene, ids, houseCount );
		DestroyScene( &scene );
	}
	nbDestroyTaskSystem( pool );

	ENSURE( hashes[1] == hashes[0] && hashes[2] == hashes[0] );
	free( houses );
	return 0;
}

// Once enough debris moves, the workers check in blocks which of it comes to rest and what carries it, large pieces
// included, and the calling thread puts their answers together in the order of the debris: the world ends as with one
// thread, with the same rubble, see nbSettleDebris.
static int SettleWorkersTest( void )
{
	enum
	{
		houseCount = 2
	};
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * houseCount );
	nbDestructibleDef defs[houseCount];
	const nbPieceDef* lists[houseCount];
	int counts[houseCount];
	for ( int h = 0; h < houseCount; ++h )
	{
		BuildCelledHouse( houses + h, 2, (uint32_t)( 77 + h ), ( b3Vec3 ){ 14.0f * (float)h, 0.0f, 0.0f } );
		defs[h] = houses[h].def;
		lists[h] = houses[h].pieces;
		counts[h] = houses[h].pieceCount;
	}

	b3Vec3 grenades[] = {
		{ 0.0f, 1.2f, 3.4f }, { -2.0f, 4.4f, -3.4f }, { 2.5f, 1.2f, -3.4f },
		{ 4.7f, 1.2f, 0.0f }, { -4.7f, 4.4f, 0.0f },  { -2.5f, 1.2f, 3.4f },
	};
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	uint32_t hashes[2];
	int rubbleCounts[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, 4 );
		nbDestructibleId ids[houseCount];
		nbCreateDestructibles( scene.world, defs, lists, counts, houseCount, ids );
		for ( int h = 0; h < houseCount; ++h )
		{
			for ( int g = 0; g < NB_ARRAY_COUNT( grenades ); ++g )
			{
				Grenade( &scene, b3Add( grenades[g], ( b3Vec3 ){ 14.0f * (float)h, 0.0f, 0.0f } ) );
			}
		}

		nbWorld_ResetProfile( scene.world );
		int maxMoving = 0;
		for ( int i = 0; i < 240; ++i )
		{
			Step( &scene, 1 );
			nbStats stats = nbWorld_GetStats( scene.world );
			maxMoving = b3MaxInt( maxMoving, stats.debrisCount - stats.rubbleCount );
		}

		// More moving debris than the workers need, and piles that froze
		nbProfile profile = nbWorld_GetProfile( scene.world );
		rubbleCounts[pass] = nbWorld_GetStats( scene.world ).rubbleCount;
		if ( pass == 1 )
		{
			printf( "  up to %d moving, checks on the workers in %d updates, they took %.0f %% and started after %.0f us\n",
					maxMoving, profile.settleSplitCount, 100.0f * profile.settleWorkerShare, profile.settleWakeTime );
		}
		ENSURE( maxMoving > 512 && rubbleCounts[pass] > 0 );
		ENSURE( pass == 0 ? profile.settleSplitCount == 0 : profile.settleSplitCount > 0 );
		ENSURE( 0.0f <= profile.settleWorkerShare && profile.settleWorkerShare <= 1.0f );
		hashes[pass] = HashCreated( &scene, ids, houseCount );
		DestroyScene( &scene );
	}
	nbDestroyTaskSystem( pool );

	ENSURE( hashes[1] == hashes[0] && rubbleCounts[1] == rubbleCounts[0] );
	free( houses );
	return 0;
}

// Over the budget the workers look at the ranked debris before anything freezes, see nbBudgetLook. The calling thread
// freezes the same pieces as alone, also where a look no longer holds because something it read froze.
static int BudgetWorkersTest( void )
{
	enum
	{
		houseCount = 2
	};
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * houseCount );
	nbDestructibleDef defs[houseCount];
	const nbPieceDef* lists[houseCount];
	int counts[houseCount];
	for ( int h = 0; h < houseCount; ++h )
	{
		BuildCelledHouse( houses + h, 2, (uint32_t)( 81 + h ), ( b3Vec3 ){ 14.0f * (float)h, 0.0f, 0.0f } );
		defs[h] = houses[h].def;
		lists[h] = houses[h].pieces;
		counts[h] = houses[h].pieceCount;
	}

	b3Vec3 grenades[] = {
		{ 0.0f, 1.2f, 3.4f },  { -2.0f, 4.4f, -3.4f }, { 2.5f, 1.2f, -3.4f }, { 4.7f, 1.2f, 0.0f },
		{ -4.7f, 4.4f, 0.0f }, { -2.5f, 1.2f, 3.4f },  { 2.0f, 4.4f, 3.4f },  { -4.7f, 1.2f, 1.5f },
	};
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	uint32_t hashes[2];
	int rubbleCounts[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = pass == 0 ? CreateScene() : CreateSceneWithPool( pool, 4 );
		nbWorld_SetDebrisBudget( scene.world, 150 );
		nbDestructibleId ids[houseCount];
		nbCreateDestructibles( scene.world, defs, lists, counts, houseCount, ids );
		for ( int h = 0; h < houseCount; ++h )
		{
			for ( int g = 0; g < NB_ARRAY_COUNT( grenades ); ++g )
			{
				Grenade( &scene, b3Add( grenades[g], ( b3Vec3 ){ 14.0f * (float)h, 0.0f, 0.0f } ) );
			}
		}

		int maxMoving = 0;
		for ( int i = 0; i < 240; ++i )
		{
			Step( &scene, 1 );
			nbStats stats = nbWorld_GetStats( scene.world );
			maxMoving = b3MaxInt( maxMoving, stats.debrisCount - stats.rubbleCount );
		}

		// Far over the budget, and only with workers did they look, see nbWorld::lookEstimate
		const nbWorld* world = nbGetWorldFromId( scene.world );
		rubbleCounts[pass] = nbWorld_GetStats( scene.world ).rubbleCount;
		ENSURE( maxMoving > 512 && rubbleCounts[pass] > 0 );
		ENSURE( pass == 0 ? world->lookEstimate == 0.0f : world->lookEstimate > 0.0f );
		hashes[pass] = HashCreated( &scene, ids, houseCount );
		DestroyScene( &scene );
	}
	nbDestroyTaskSystem( pool );

	ENSURE( hashes[1] == hashes[0] && rubbleCounts[1] == rubbleCounts[0] );
	free( houses );
	return 0;
}

// What a function after the steps sees of the world, and whether it ran on another thread than the one that stepped
typedef struct AfterStepProbe
{
	b3WorldId physicsWorld;
	int callerToken;
	uint64_t probe;
	int calls;
	int otherThreadCalls;
} AfterStepProbe;

static void ProbeAfterStep( void* context )
{
	AfterStepProbe* probe = context;
	probe->probe = probe->probe * 31u + (uint64_t)b3World_GetAwakeBodyCount( probe->physicsWorld );
	probe->calls += 1;
	probe->otherThreadCalls += nbGetThreadToken() != probe->callerToken ? 1 : 0;
}

// nbWorld_Step on the step thread, on the calling thread and the old way with b3World_Step and nbWorld_Update end in the
// same worlds to the bit, with one thread and with a pool. The thread that steps waits for the step in every lookup of
// Nebenan and Box3D, also with two steps queued, and nbWorld_GetBodyEvents has the latest move of every body that moved
// and is still there. A function after a step sees the world as it is after that step. The worlds can go with steps
// queued, the Box3D world first or the destruction world.
static int AsyncStepTest( void )
{
	enum
	{
		houseCount = 2,
		passCount = 5,
		frameCount = 150
	};
	CelledHouse* houses = malloc( sizeof( CelledHouse ) * houseCount );
	nbDestructibleDef defs[houseCount];
	const nbPieceDef* lists[houseCount];
	int counts[houseCount];
	for ( int h = 0; h < houseCount; ++h )
	{
		BuildCelledHouse( houses + h, 2, (uint32_t)( 70 + h ), (b3Vec3){ 14.0f * (float)h, 0.0f, 0.0f } );
		defs[h] = houses[h].def;
		lists[h] = houses[h].pieces;
		counts[h] = houses[h].pieceCount;
	}

	int64_t baseBytes = nbGetByteCount();
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	uint32_t hashes[passCount];
	uint64_t probes[passCount];
	int moveCounts[passCount];
	float waitTimes[passCount];
	AfterStepProbe afterProbes[passCount];
	for ( int pass = 0; pass < passCount; ++pass )
	{
		// The old way, nbWorld_Step on the calling thread, then on the step thread, last with a pool for both worlds
		TestScene scene = pass == passCount - 1 ? CreateSceneWithPool( pool, 4 ) : CreateScene();
		nbWorld_SetAsyncStep( scene.world, pass >= 2 );
		nbDestructibleId ids[houseCount];
		nbCreateDestructibles( scene.world, defs, lists, counts, houseCount, ids );

		uint64_t probe = 0;
		int moveCount = 0;
		float waitTime = 0.0f;
		AfterStepProbe afterProbe = { scene.physicsWorld, nbGetThreadToken(), 0, 0, 0 };
		for ( int frame = 0; frame < frameCount; ++frame )
		{
			b3BodyEvents moves = nbWorld_GetBodyEvents( scene.world );
			for ( int i = 0; i < moves.moveCount; ++i )
			{
				const b3BodyMoveEvent* move = moves.moveEvents + i;
				ENSURE( b3Body_IsValid( move->bodyId ) );
				b3WorldTransform transform = b3Body_GetTransform( move->bodyId );
				ENSURE( memcmp( &transform, &move->transform, sizeof( transform ) ) == 0 );
			}
			moveCount += moves.moveCount;

			if ( frame == 10 )
			{
				for ( int h = 0; h < houseCount; ++h )
				{
					Grenade( &scene, (b3Vec3){ 14.0f * (float)h, 1.2f, 3.4f } );
					Grenade( &scene, (b3Vec3){ 14.0f * (float)h - 2.0f, 4.4f, -3.4f } );
				}
			}

			// Two steps in every third frame, the second follows the first on the step thread
			int stepCount = frame % 3 == 0 ? 2 : 1;
			for ( int s = 0; s < stepCount; ++s )
			{
				if ( pass == 0 )
				{
					b3World_Step( scene.physicsWorld, 1.0f / 60.0f, 4 );
					nbWorld_Update( scene.world, 1.0f / 60.0f );
					ProbeAfterStep( &afterProbe );
				}
				else
				{
					nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
					nbWorld_RunAfterStep( scene.world, ProbeAfterStep, &afterProbe );
				}
			}

			// Right after the steps, Box3D and Nebenan wait for them by turns
			if ( frame % 2 == 0 )
			{
				probe = probe * 31u + (uint64_t)b3World_GetCounters( scene.physicsWorld ).awakeContactCount;
			}
			else
			{
				probe = probe * 31u + (uint64_t)nbWorld_GetStats( scene.world ).debrisCount;
			}
			waitTime += nbWorld_GetStats( scene.world ).waitTime;
		}

		nbWorld_GetBodyEvents( scene.world );
		afterProbes[pass] = afterProbe;
		hashes[pass] = HashCreated( &scene, ids, houseCount );
		probes[pass] = probe;
		moveCounts[pass] = moveCount;
		waitTimes[pass] = waitTime;

		// The worlds go with a step queued
		nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
		if ( pass == 3 )
		{
			b3DestroyWorld( scene.physicsWorld );
			nbDestroyWorld( scene.world );
		}
		else
		{
			DestroyScene( &scene );
		}
	}
	nbDestroyTaskSystem( pool );
	ENSURE( nbGetByteCount() == baseBytes );

	// Only the step thread has steps to wait for, and only it runs the functions after the steps
	ENSURE( moveCounts[0] == 0 && moveCounts[1] > 1000 );
	ENSURE( waitTimes[0] == 0.0f && waitTimes[1] == 0.0f && waitTimes[2] > 0.0f && waitTimes[passCount - 1] > 0.0f );
	ENSURE( afterProbes[0].calls == frameCount + frameCount / 3 && afterProbes[1].otherThreadCalls == 0 );
	ENSURE( afterProbes[2].otherThreadCalls == afterProbes[2].calls );
	for ( int pass = 1; pass < passCount; ++pass )
	{
		ENSURE( hashes[pass] == hashes[0] && probes[pass] == probes[0] );
		ENSURE( moveCounts[pass] == moveCounts[1] );
		ENSURE( afterProbes[pass].calls == afterProbes[0].calls && afterProbes[pass].probe == afterProbes[0].probe );
	}
	free( houses );
	return 0;
}

// nbWorld_PlanSteps keeps the time a frame leaves over for the next one, takes at most two steps per frame and counts a
// frame that hung as 0.1 s. Before the first step the steps count as cheap. With the step on a thread of its own it
// waits for the steps before it reads how long the last one took.
static int PlanStepsTest( void )
{
	const float step = 1.0f / 64.0f;
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateScene();
		nbWorld_SetAsyncStep( scene.world, pass == 1 );

		ENSURE( nbWorld_PlanSteps( scene.world, 0.5f * step, step ) == 0 );
		ENSURE( nbWorld_PlanSteps( scene.world, 0.5f * step, step ) == 1 );
		ENSURE( nbWorld_PlanSteps( scene.world, 2.5f * step, step ) == 2 );
		ENSURE( nbWorld_PlanSteps( scene.world, 0.5f * step, step ) == 1 );

		// What two steps cannot take is dropped, up to one step that waits for the next frame
		ENSURE( nbWorld_PlanSteps( scene.world, 1.0f, step ) == 2 );
		ENSURE( nbWorld_PlanSteps( scene.world, 0.0f, step ) == 1 );
		ENSURE( nbWorld_PlanSteps( scene.world, 0.0f, step ) == 0 );
		ENSURE( nbWorld_PlanSteps( scene.world, -1.0f, step ) == 0 );
		ENSURE( nbWorld_PlanSteps( scene.world, step, 0.0f ) == 0 );

		// A frame of one step takes it, however long the last one took
		for ( int frame = 0; frame < 30; ++frame )
		{
			ENSURE( nbWorld_PlanSteps( scene.world, step, step ) == 1 );
			nbWorld_Step( scene.world, step, 4 );
		}
		DestroyScene( &scene );
	}
	return 0;
}

// The determinism scenario in frames of 1/60 s through nbWorld_Advance at stepRate steps per second, with the impacts at
// the same times, then settle frames until all of it rests. Counts the steps and the frames it settled.
static uint32_t RunScenarioAtRate( TestScene scene, float stepRate, int settleFrames, int* stepCount, int* awakeCount )
{
	nbWorld_SetStepRate( scene.world, stepRate );
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 2.5f, 1.5f, 0.12f }, 5 );
	nbDestructibleId gate = CreateGate( &scene, 10.0f, 6 );

	nbImpactDef impact = { 0 };
	impact.direction = (b3Vec3){ 0.2f, 0.0f, -1.0f };
	impact.radius = 0.45f;
	impact.damage = 8.0e4f;
	impact.ejectSpeed = 9.0f;

	*stepCount = 0;
	for ( int frame = 0; frame < 120 + settleFrames; ++frame )
	{
		if ( frame < 120 && frame % 20 == 0 )
		{
			impact.point = (b3Vec3){ -1.5f + 0.5f * (float)( frame / 20 ), 0.6f + 0.2f * (float)( frame / 20 ), 0.12f };
			nbWorld_ApplyImpact( scene.world, &impact );
		}
		if ( frame == 10 )
		{
			nbImpactDef blast = { 0 };
			blast.point = (b3Vec3){ 13.0f, 2.3f, 0.0f };
			blast.radius = 1.0f;
			blast.damage = 1.0e8f;
			blast.ejectSpeed = 2.0f;
			nbWorld_ApplyImpact( scene.world, &blast );
		}
		*stepCount += nbWorld_Advance( scene.world, 1.0f / 60.0f );
	}

	nbStats stats = nbWorld_GetStats( scene.world );
	*awakeCount = b3World_GetAwakeBodyCount( scene.physicsWorld );
	uint32_t hash = HashDestructible( 2166136261u, wall );
	hash = HashDestructible( hash, gate );
	hash = ( hash ^ (uint32_t)stats.bondCount ) * 16777619u;
	DestroyScene( &scene );
	return hash;
}

// The results of the determinism scenario at 30 and 120 Hz with ten seconds to settle, the same on every platform like
// NB_EXPECTED_DETERMINISM_HASH
#define NB_EXPECTED_HASH_30HZ 0xca0268d0u
#define NB_EXPECTED_HASH_120HZ 0xc3d1c91au

// The steps per second of nbWorld_Advance. At 60 Hz it takes the steps of the determinism scenario to the bit. At 30 and
// 120 Hz every frame of 1/60 s takes half a step and two, whatever the steps cost, the world ends the same with one and
// four threads and on every platform, and everything comes to rest. Each step gets the substeps that make 240 per second.
static int StepRateTest( void )
{
	ENSURE( nbGetSubStepCount( 30.0f ) == 8 );
	ENSURE( nbGetSubStepCount( 60.0f ) == 4 );
	ENSURE( nbGetSubStepCount( 120.0f ) == 2 );
	ENSURE( nbGetSubStepCount( 144.0f ) == 2 );
	ENSURE( nbGetSubStepCount( 240.0f ) == 1 );
	ENSURE( nbGetSubStepCount( 1000.0f ) == 1 );
	ENSURE( nbGetSubStepCount( 0.0f ) == 4 );

	{
		TestScene scene = CreateScene();
		ENSURE( nbWorld_GetStepRate( scene.world ) == 60.0f );
		nbWorld_SetStepRate( scene.world, 1.0f );
		ENSURE( nbWorld_GetStepRate( scene.world ) == 10.0f );
		nbWorld_SetStepRate( scene.world, 1000.0f );
		ENSURE( nbWorld_GetStepRate( scene.world ) == 240.0f );

		// Two steps of 1/120 s fit into a frame of 1/60 s, and four when the steps are cheap
		ENSURE( nbWorld_PlanSteps( scene.world, 1.0f / 60.0f, 1.0f / 120.0f ) == 2 );
		ENSURE( nbWorld_PlanSteps( scene.world, 1.0f, 1.0f / 120.0f ) == 4 );
		DestroyScene( &scene );
	}

	int steps = 0, awake = 0;
	ENSURE( RunScenarioAtRate( CreateScene(), 60.0f, 0, &steps, &awake ) == NB_EXPECTED_DETERMINISM_HASH );
	ENSURE( steps == 120 );

	const float rates[2] = { 30.0f, 120.0f };
	const uint32_t expected[2] = { NB_EXPECTED_HASH_30HZ, NB_EXPECTED_HASH_120HZ };
	for ( int r = 0; r < 2; ++r )
	{
		int steps1 = 0, steps4 = 0, awake1 = 0, awake4 = 0;
		uint32_t hash1 = RunScenarioAtRate( CreateScene(), rates[r], 600, &steps1, &awake1 );
		uint32_t hash4 = RunScenarioAtRate( CreateSceneWithWorkers( 4, NULL, NULL, NULL ), rates[r], 600, &steps4, &awake4 );
		printf( "  %.0f Hz: %d steps, hash 0x%08x, %d bodies awake after 10 s\n", rates[r], steps1, hash1, awake1 );
		ENSURE( hash1 == hash4 );
		ENSURE( hash1 == expected[r] );
		ENSURE( steps1 == (int)( 720.0f * rates[r] / 60.0f ) && steps4 == steps1 );
		ENSURE( awake1 == 0 && awake4 == 0 );
	}
	return 0;
}

// The pose that nbWorld_GetDrawPoses handed out for a body, null if it has none
static const nbDrawPose* FindDrawPose( nbDrawPoses poses, b3BodyId bodyId )
{
	for ( int i = 0; i < poses.count; ++i )
	{
		if ( B3_ID_EQUALS( poses.poses[i].bodyId, bodyId ) )
		{
			return poses.poses + i;
		}
	}
	return NULL;
}

// nbWorld_GetDrawPoses draws a body between its poses before and after the last step. At the ends it draws them exactly,
// in between its center of mass lies on the straight line and it turns the short way, though its origin lies 3 m away.
// With several steps between two calls the poses are those around the last step. A body that stops is drawn once more at
// its last pose, one that goes is not drawn, and a call without a step draws the same bodies again. With the step thread
// the poses are the same to the bit. nbWorld_GetStepFraction tells how far the time nbWorld_PlanSteps left over reaches.
static int DrawPoseTest( void )
{
	const float step = 1.0f / 64.0f;
	b3WorldTransform halfways[2];
	for ( int pass = 0; pass < 2; ++pass )
	{
		TestScene scene = CreateScene();
		nbWorld_SetAsyncStep( scene.world, pass == 1 );
		ENSURE( nbWorld_GetStepFraction( scene.world ) == 1.0f );

		// A box 3 m beside its origin that spins as it flies, and two boxes that stop and go
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.enableSleep = false;
		bodyDef.position = (b3Vec3){ 0.0f, 10.0f, 0.0f };
		bodyDef.linearVelocity = (b3Vec3){ 2.0f, 0.0f, 0.0f };
		bodyDef.angularVelocity = (b3Vec3){ 0.0f, 0.0f, 8.0f };
		b3BodyId spinner = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3BoxHull offsetBox = b3MakeOffsetBoxHull( 0.5f, 0.5f, 0.5f, (b3Vec3){ 3.0f, 0.0f, 0.0f } );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3CreateHullShape( spinner, &shapeDef, &offsetBox.base );

		bodyDef.linearVelocity = b3Vec3_zero;
		bodyDef.angularVelocity = b3Vec3_zero;
		b3BoxHull box = b3MakeCubeHull( 0.5f );
		bodyDef.position = (b3Vec3){ 10.0f, 10.0f, 0.0f };
		b3BodyId stopper = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3CreateHullShape( stopper, &shapeDef, &box.base );
		bodyDef.position = (b3Vec3){ -10.0f, 10.0f, 0.0f };
		b3BodyId goner = b3CreateBody( scene.physicsWorld, &bodyDef );
		b3CreateHullShape( goner, &shapeDef, &box.base );

		// The first call starts collecting
		ENSURE( nbWorld_GetDrawPoses( scene.world, 1.0f ).count == 0 );

		b3WorldTransform start = b3Body_GetTransform( spinner );
		nbWorld_Step( scene.world, step, 4 );
		nbDrawPoses poses = nbWorld_GetDrawPoses( scene.world, 1.0f );
		ENSURE( poses.count == 3 );
		b3BodyId bodies[3] = { spinner, stopper, goner };
		for ( int i = 0; i < 3; ++i )
		{
			const nbDrawPose* pose = FindDrawPose( poses, bodies[i] );
			b3WorldTransform transform = b3Body_GetTransform( bodies[i] );
			ENSURE( pose != NULL && memcmp( &pose->transform, &transform, sizeof( transform ) ) == 0 );
		}

		// Without a step the same bodies, now at the start of the step
		poses = nbWorld_GetDrawPoses( scene.world, 0.0f );
		ENSURE( poses.count == 3 );
		ENSURE( memcmp( &FindDrawPose( poses, spinner )->transform, &start, sizeof( start ) ) == 0 );

		// Halfway the center of mass lies halfway, not the origin
		b3WorldTransform end = b3Body_GetTransform( spinner );
		b3Vec3 localCenter = b3Body_GetLocalCenter( spinner );
		poses = nbWorld_GetDrawPoses( scene.world, 0.5f );
		b3WorldTransform halfway = FindDrawPose( poses, spinner )->transform;
		b3Pos fromCenter = b3TransformWorldPoint( start, localCenter );
		b3Pos toCenter = b3TransformWorldPoint( end, localCenter );
		b3Vec3 offCenter = b3SubPos( b3TransformWorldPoint( halfway, localCenter ), b3LerpPosition( fromCenter, toCenter, 0.5f ) );
		ENSURE_SMALL( b3Length( offCenter ), 1.0e-5f );
		b3Quat turn = b3NLerp( start.q, end.q, 0.5f );
		ENSURE_SMALL( 1.0f - b3AbsFloat( b3DotQuat( halfway.q, turn ) ), 1.0e-6f );
		ENSURE( b3Length( b3SubPos( halfway.p, b3LerpPosition( start.p, end.p, 0.5f ) ) ) > 1.0e-3f );
		halfways[pass] = halfway;

		// Two steps between two calls, the poses are those around the second
		nbWorld_Step( scene.world, step, 4 );
		b3WorldTransform beforeLast = b3Body_GetTransform( spinner );
		nbWorld_Step( scene.world, step, 4 );
		poses = nbWorld_GetDrawPoses( scene.world, 0.0f );
		ENSURE( poses.count == 3 );
		ENSURE( memcmp( &FindDrawPose( poses, spinner )->transform, &beforeLast, sizeof( beforeLast ) ) == 0 );

		// One stops and one goes. The one that stopped is drawn once more at its last pose.
		b3Body_SetType( stopper, b3_staticBody );
		b3WorldTransform stopped = b3Body_GetTransform( stopper );
		b3DestroyBody( goner );
		nbWorld_Step( scene.world, step, 4 );
		poses = nbWorld_GetDrawPoses( scene.world, 0.5f );
		ENSURE( poses.count == 2 && FindDrawPose( poses, goner ) == NULL && FindDrawPose( poses, spinner ) != NULL );
		const nbDrawPose* last = FindDrawPose( poses, stopper );
		ENSURE( last != NULL && memcmp( &last->transform, &stopped, sizeof( stopped ) ) == 0 );
		poses = nbWorld_GetDrawPoses( scene.world, 0.5f );
		ENSURE( poses.count == 1 && FindDrawPose( poses, spinner ) != NULL );

		// The fraction of what nbWorld_PlanSteps leaves over
		ENSURE( nbWorld_PlanSteps( scene.world, 0.5f * step, step ) == 0 );
		ENSURE( nbWorld_GetStepFraction( scene.world ) == 0.5f );
		ENSURE( nbWorld_PlanSteps( scene.world, 0.75f * step, step ) == 1 );
		ENSURE( nbWorld_GetStepFraction( scene.world ) == 0.25f );
		DestroyScene( &scene );
	}

	ENSURE( memcmp( halfways + 0, halfways + 1, sizeof( halfways[0] ) ) == 0 );
	return 0;
}

// The profile counts the steps and impacts since its reset, and every stage holds its parts
static int ProfileTest( void )
{
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	TestScene scene = CreateSceneWithPool( pool, 4 );

	// Three walls far apart, so their debris lies in islands of their own
	b3Vec3 halfExtents = { 3.0f, 1.5f, 0.12f };
	nbDestructibleDef def = nbDefaultDestructibleDef();
	for ( int i = 0; i < 3; ++i )
	{
		def.position = (b3Vec3){ 15.0f * (float)( i - 1 ), halfExtents.y, 0.0f };
		def.seed = 7 + (uint32_t)i;
		nbCreateBox( scene.world, &def, halfExtents );
	}

	nbWorld_ResetProfile( scene.world );
	nbProfile empty = nbWorld_GetProfile( scene.world );
	ENSURE( empty.stepCount == 0 && empty.impactCount == 0 && empty.stages[nb_profileTotal] == 0.0f );

	// Enough cells for the workers
	nbImpactDef impact = { 0 };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.8f;
	impact.damage = 6.0e4f;
	impact.ejectSpeed = 8.0f;
	impact.fragmentCount = 64;
	for ( int i = 0; i < 3; ++i )
	{
		impact.point = (b3Vec3){ 15.0f * (float)( i - 1 ) + 0.3f, 1.4f, halfExtents.z };
		nbWorld_ApplyImpact( scene.world, &impact );
	}

	const int stepCount = 90;
	for ( int i = 0; i < stepCount; ++i )
	{
		nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
	}

	nbProfile profile = nbWorld_GetProfile( scene.world );
	const float* t = profile.stages;
	printf( "  step %.3f ms, update %.3f ms, impacts %.3f ms, cells %.3f ms, largest island %.2f, team in %d steps with %.2f, "
			"%d workers started after %.0f us\n",
			t[nb_profileStep], t[nb_profileUpdate], t[nb_profileImpacts], t[nb_profileCells], profile.largestIsland,
			profile.teamStepCount, profile.teamShare, profile.wakeCount, profile.wakeTime );
	ENSURE( profile.workerCount == 4 );
	ENSURE( profile.stepCount == stepCount && profile.impactCount == 3 );
	ENSURE( t[nb_profileStep] > 0.0f && t[nb_profileUpdate] > 0.0f && t[nb_profileImpacts] > 0.0f );
	ENSURE( fabsf( t[nb_profileTotal] - t[nb_profileStep] - t[nb_profileUpdate] - t[nb_profileImpacts] ) < 1.0e-4f );

	// The timers of the parts run inside the timers of the stages that hold them
	const float slack = 1.0e-3f;
	ENSURE( t[nb_profilePairs] + t[nb_profileCollide] + t[nb_profileSolve] <= t[nb_profileStep] + slack );
	ENSURE( t[nb_profileCreateContacts] <= t[nb_profilePairs] + slack );
	ENSURE( t[nb_profileGatherContacts] > 0.0f && t[nb_profileUpdateContacts] > 0.0f && t[nb_profileMergeContacts] > 0.0f );
	float collideParts = t[nb_profileGatherContacts] + t[nb_profileUpdateContacts] + t[nb_profileMergeContacts];
	ENSURE( collideParts + t[nb_profileContactStates] <= t[nb_profileCollide] + slack );
	ENSURE( t[nb_profileSolverSetup] + t[nb_profileConstraints] <= t[nb_profileSolve] + slack );
	ENSURE( t[nb_profileSetupPacks] > 0.0f && t[nb_profileSetupTasks] > 0.0f && t[nb_profileSetupKnots] > 0.0f );
	ENSURE( t[nb_profileSetupWait] >= 0.0f );
	ENSURE( t[nb_profileSetupPacks] + t[nb_profileSetupKnots] + t[nb_profileSetupWait] <= t[nb_profileSolverSetup] + slack );
	ENSURE( t[nb_profileOverflow] <= t[nb_profileConstraints] + slack );
	ENSURE( t[nb_profileCollisionDamage] + t[nb_profileSettle] + t[nb_profileBudget] + t[nb_profileRelease] <=
			t[nb_profileUpdate] + slack );
	ENSURE( t[nb_profileCells] > 0.0f && t[nb_profileCellsAlone] <= t[nb_profileCells] + slack );
	ENSURE( t[nb_profileCells] <= t[nb_profileUpdate] + t[nb_profileImpacts] + slack );
	ENSURE( profile.freezeCount >= 0.0f && profile.thawCount >= 0.0f );
	ENSURE( 0.0f <= profile.knotContactCount && profile.knotContactCount <= profile.overflowCount );
	ENSURE( profile.knotBatchCount <= profile.knotContactCount );

	// Every eighth step times its contact loop: the checks and the narrowphase run in the blocks, and the loop has at most
	// as many tasks as workers and at least as many blocks as tasks
	ENSURE( 0 < profile.collideSampleCount && profile.collideSampleCount <= ( stepCount + 7 ) / 8 );
	ENSURE( profile.collideWork > 0.0f && profile.collideChecks > 0.0f && profile.collideNarrowphase > 0.0f );
	ENSURE( profile.collideChecks + profile.collideNarrowphase <= profile.collideWork + slack );
	ENSURE( profile.collideLongestBlock <= profile.collideWork + slack );
	ENSURE( profile.collideLate >= 0.0f && profile.collideClaim >= 0.0f && profile.collideDone >= 0.0f );
	ENSURE( 1.0f <= profile.collideTaskCount && profile.collideTaskCount <= 4.0f );
	ENSURE( profile.collideTaskCount <= profile.collideBlockCount );
	ENSURE( 0.0f <= profile.collideMissedCount && profile.collideMissedCount < profile.collideTaskCount );
	ENSURE( profile.collideContactCount > 0.0f && profile.collideNarrowphaseCount > 0.0f );

	// The parts of the checks of the update run inside them, and every eighth update times how the threads spent the
	// checks: the support of the quiet pieces is part of the blocks, and the checks have at most as many tasks as workers
	float settleParts =
		t[nb_profileGatherMoving] + t[nb_profileCheckMoving] + t[nb_profileSearchSupport] + t[nb_profileFreezeRubble];
	ENSURE( t[nb_profileCheckMoving] > 0.0f && settleParts <= t[nb_profileSettle] + slack );
	ENSURE( 0 < profile.settleSampleCount && profile.settleSampleCount <= ( stepCount + 7 ) / 8 );
	ENSURE( profile.settleWork > 0.0f && profile.settleSupportWork <= profile.settleWork + slack );
	ENSURE( profile.settleLongestBlock <= profile.settleWork + slack );
	ENSURE( profile.settleLate >= 0.0f && profile.settleClaim > -slack && profile.settleDone >= 0.0f );
	ENSURE( 1.0f <= profile.settleTaskCount && profile.settleTaskCount <= 4.0f );
	ENSURE( profile.settleTaskCount <= profile.settleBlockCount );
	ENSURE( 0.0f <= profile.settleMissedCount && profile.settleMissedCount < profile.settleTaskCount );
	ENSURE( profile.settleMovingCount > 0.0f && profile.settleQuietCount <= profile.settleMovingCount );
	ENSURE( profile.settleCallerCount <= profile.settleMovingCount );

	// A world with workers knows its islands. None of the three holds more than half of the work in the steps with packs,
	// and the bound of the packs is at least the even share.
	ENSURE( profile.islandStepCount > 0 && profile.colorStepCount < profile.islandStepCount );
	ENSURE( 0.0f < profile.largestIsland && profile.largestIsland <= 1.0f );
	for ( int i = 0; i < 4; ++i )
	{
		ENSURE( profile.islandFloor[i] >= 1.0f / (float)( 2 << i ) - 1.0e-6f );
	}

	// A team only takes a pack with more than five fourths of the even share of the workers of a step, at most four here.
	// With so little debris most steps have one worker, see TaskSystemTest for steps with teams.
	ENSURE( profile.teamStepCount <= profile.islandStepCount );
	ENSURE( profile.teamStepCount == 0 || ( 1.25f / 4.0f < profile.teamShare && profile.teamShare <= 1.0f ) );

	// The cells of the impact went to the workers. From how many items they help next time depends on how fast they
	// started, between the bounds of nbFractureWorkerItems and nbSettleWorkerPieces.
	ENSURE( profile.wakeCount > 0 && profile.wakeTime >= 0.0f && profile.longestWake >= profile.wakeTime );
	ENSURE( 8 <= profile.fractureWorkerItems && profile.fractureWorkerItems <= 32 );
	ENSURE( 128 <= profile.settleWorkerPieces && profile.settleWorkerPieces <= 512 );

	nbWorld_ResetProfile( scene.world );
	ENSURE( nbWorld_GetProfile( scene.world ).stepCount == 0 );

	DestroyScene( &scene );
	nbDestroyTaskSystem( pool );
	return 0;
}

// The fit of profiles with different numbers of workers, its projection and the report
static int ScalingFitTest( void )
{
	// Every stage takes 2 ms on one thread plus 8 ms spread over the workers
	nbProfile profiles[4] = { 0 };
	int workers[4] = { 1, 2, 4, 4 };
	for ( int i = 0; i < 4; ++i )
	{
		profiles[i].workerCount = workers[i];
		profiles[i].stepCount = 100;
		for ( int k = 0; k < nb_profileStageCount; ++k )
		{
			profiles[i].stages[k] = 2.0f + 8.0f / (float)workers[i];
		}
	}

	nbScaling scaling = nbFitScaling( profiles, 4 );
	ENSURE( scaling.profileCount == 4 );
	for ( int k = 0; k < nb_profileStageCount; ++k )
	{
		ENSURE( fabsf( scaling.serial[k] - 2.0f ) < 1.0e-4f && fabsf( scaling.parallel[k] - 8.0f ) < 1.0e-4f );
	}
	ENSURE( fabsf( nbScaling_Project( &scaling, nb_profileStep, 8 ) - 3.0f ) < 1.0e-4f );

	// Slower with more workers: nothing counts as parallel. One number of workers only: all is serial.
	for ( int i = 0; i < 4; ++i )
	{
		profiles[i].stages[nb_profileUpdate] = 1.0f + 0.1f * (float)workers[i];
	}
	scaling = nbFitScaling( profiles, 4 );
	ENSURE( scaling.parallel[nb_profileUpdate] == 0.0f );
	ENSURE( fabsf( scaling.serial[nb_profileUpdate] - ( 1.0f + 0.1f * 11.0f / 4.0f ) ) < 1.0e-4f );
	scaling = nbFitScaling( profiles + 2, 2 );
	ENSURE( scaling.parallel[nb_profileStep] == 0.0f && fabsf( scaling.serial[nb_profileStep] - 4.0f ) < 1.0e-4f );

	// With timed steps of a team the report shows how it spent its stages, with the most workers, see b3TeamStages
	for ( int i = 2; i < 4; ++i )
	{
		profiles[i].teamSampleCount = 10;
		profiles[i].teamTime = 1.0f;
		b3TeamStage* row = profiles[i].teamStages.rows + b3_teamColors;
		*row = (b3TeamStage){ .barrierCount = 13.0f,
							  .blockCount = 104.0f,
							  .callerBlockCount = 26.0f,
							  .itemCount = 832.0f,
							  .openTime = 0.2f,
							  .workTime = 0.45f,
							  .packTime = 0.15f,
							  .lateTime = 0.15f,
							  .doneTime = 0.05f };
		profiles[i].teamStages.entryCount = 39.0f;

		// And how the threads spent the contact loop, see b3Profile::collideWork
		profiles[i].collideSampleCount = 10;
		profiles[i].collideLoop = 0.5f;
		profiles[i].collideWork = 1.6f;
		profiles[i].collideChecks = 0.6f;
		profiles[i].collideNarrowphase = 0.9f;
		profiles[i].collideLate = 0.2f;
		profiles[i].collideClaim = 0.04f;
		profiles[i].collideDone = 0.16f;
		profiles[i].collideLongestBlock = 0.05f;
		profiles[i].collideTaskCount = 4.0f;
		profiles[i].collideBlockCount = 128.0f;
		profiles[i].collideContactCount = 8000.0f;
		profiles[i].collideNarrowphaseCount = 2000.0f;

		// And how the threads spent the checks of the update, see nbProfile::settleWork
		profiles[i].settleSampleCount = 10;
		profiles[i].settleCall = 0.1f;
		profiles[i].settleWork = 0.3f;
		profiles[i].settleSupportWork = 0.1f;
		profiles[i].settleLate = 0.05f;
		profiles[i].settleClaim = 0.01f;
		profiles[i].settleDone = 0.04f;
		profiles[i].settleLongestBlock = 0.02f;
		profiles[i].settleTaskCount = 4.0f;
		profiles[i].settleBlockCount = 32.0f;
		profiles[i].settleMovingCount = 2000.0f;
		profiles[i].settleQuietCount = 100.0f;
		profiles[i].settleCallerCount = 500.0f;
	}

	// And what a unit of the solver's work costs, alone, in packs and in the team's blocks, see b3Counters::solverSample
	for ( int i = 0; i < 4; ++i )
	{
		profiles[i].solverSampleCount = 10;
		profiles[i].solverWork = 100000.0f;
		profiles[i].solverConstraints = 3.0f / (float)workers[i];
		profiles[i].packSolveTime = i == 0 ? 0.0f : 0.6f;
		profiles[i].packSolveWork = i == 0 ? 0.0f : 20000.0f;
		profiles[i].teamSampleWork = i < 2 ? 0.0f : 50000.0f;
	}

	// The report counts its whole length like snprintf, also into a buffer too small
	int length = nbFormatScalingReport( profiles, 4, NULL, 0 );
	ENSURE( length > 200 );
	char* text = malloc( (size_t)length + 1 );
	ENSURE( nbFormatScalingReport( profiles, 4, text, length + 1 ) == length );
	ENSURE( (int)strlen( text ) == length && strstr( text, "createContacts" ) != NULL );
	const char* colorRow = "color 0       13.0    0.0    8.0    64.0    0.200    0.000     56      19     19      6       25";
	ENSURE( strstr( text, "The stages of the team with 4 workers" ) != NULL && strstr( text, colorRow ) != NULL );
	const char* contactRow = "ns in blocks per contact         200.0";
	const char* shareRow = "in blocks of the tasks %          80.0";
	ENSURE( strstr( text, "The contact loop in every eighth step" ) != NULL );
	ENSURE( strstr( text, contactRow ) != NULL && strstr( text, shareRow ) != NULL );
	const char* restRow = "ns per check for rest            100.0";
	const char* supportRow = "us per support of a piece         1.00";
	const char* settleShareRow = "in blocks of the tasks %          75.0";
	const char* callerRow = "  on the calling thread %         25.0";
	ENSURE( strstr( text, "The checks of the update in every eighth update" ) != NULL );
	ENSURE( strstr( text, restRow ) != NULL && strstr( text, supportRow ) != NULL );
	ENSURE( strstr( text, settleShareRow ) != NULL && strstr( text, callerRow ) != NULL );
	const char* solverRow = "constraints on calling thread     30.0    15.0     7.5";
	const char* packRow = "in packs, over all threads           -    30.0    30.0";
	const char* packShareRow = "  work in packs %                  0.0    20.0    20.0";
	const char* teamBlockRow = "in team blocks, all threads          -       -     9.0";
	const char* teamTimeRow = "team on the calling thread           -       -    20.0";
	ENSURE( strstr( text, "The solver in every eighth step" ) != NULL && strstr( text, solverRow ) != NULL );
	ENSURE( strstr( text, packRow ) != NULL && strstr( text, packShareRow ) != NULL );
	ENSURE( strstr( text, teamBlockRow ) != NULL && strstr( text, teamTimeRow ) != NULL );
	char small[16];
	ENSURE( nbFormatScalingReport( profiles, 4, small, sizeof( small ) ) == length );
	ENSURE( strlen( small ) == sizeof( small ) - 1 );
	free( text );

	// The worker counts to measure with: 1, 2, 4 and so on, then the recommended count
	int counts[8];
	int countCount = nbGetScalingWorkerCounts( counts, 8 );
	ENSURE( countCount >= 1 && counts[0] == 1 && counts[countCount - 1] == nbGetRecommendedWorkerCount( false ) );
	for ( int i = 1; i < countCount; ++i )
	{
		ENSURE( counts[i] > counts[i - 1] );
	}
	ENSURE( strcmp( nbGetProfileStageName( nb_profileContactStates ), "contactStates" ) == 0 );
	ENSURE( strcmp( nbGetProfileStageName( nb_profileSetupKnots ), "setupKnots" ) == 0 );
	ENSURE( strcmp( nbGetProfileStageName( nb_profileUpdateContacts ), "updateContacts" ) == 0 );
	ENSURE( strcmp( nbGetProfileStageName( nb_profileCheckMoving ), "checkMoving" ) == 0 );
	ENSURE( strcmp( nbGetProfileStageName( nb_profileTypeChanges ), "typeChanges" ) == 0 );
	return 0;
}

// The threads a game should take: one per performance core, at most 8, with one hardware thread left over for the thread
// that draws while the step runs on a thread of its own
static int WorkerCountTest( void )
{
	int hardware = nbGetHardwareThreadCount();
	int performance = nbGetPerformanceCoreCount();
	int workers = nbGetRecommendedWorkerCount( false );
	int asyncWorkers = nbGetRecommendedWorkerCount( true );
	printf( "  hardware threads %d, performance cores %d, workers %d, with the step on a thread of its own %d\n", hardware,
			performance, workers, asyncWorkers );

	ENSURE( hardware >= 1 && 0 <= performance && performance <= hardware );
	ENSURE( 1 <= workers && workers <= 8 && workers <= hardware );
	ENSURE( 1 <= asyncWorkers && asyncWorkers <= workers );
	ENSURE( hardware == 1 || asyncWorkers <= hardware - 1 );
	ENSURE( performance == 0 || workers == b3MinInt( performance, 8 ) );
	return 0;
}

// The sites of the stream test: walls along a road, 20 m apart, built from the pieces the stream asks for
typedef struct StreamTestSites
{
	nbPieceDef pieces[32];
	int prepares;
	int releases;
} StreamTestSites;

static void PrepareTestSite( int site, nbSiteDef* siteDef, void* context )
{
	StreamTestSites* sites = context;
	nbPieceDef* piece = sites->pieces + site;
	*piece = nbDefaultPieceDef();
	piece->halfExtents = (b3Vec3){ 2.0f, 1.5f, 0.15f };
	siteDef->def = nbDefaultDestructibleDef();
	siteDef->def.position = b3ToPos( (b3Vec3){ 20.0f * (float)site, 1.5f, 0.0f } );
	siteDef->def.seed = (uint32_t)( 50 + site );
	siteDef->pieces = piece;
	siteDef->pieceCount = 1;
	sites->prepares += 1;
}

static void ReleaseTestSite( int site, void* context )
{
	NB_UNUSED( site );
	StreamTestSites* sites = context;
	sites->releases += 1;
}

// A stream loads the walls along a road around a viewer that drives down it and back, a wall far away as soon as a falling
// ball comes near it, and lets the walls go behind the viewer, a damaged one with its rubble, which comes back with the
// same chunks. With the step on a thread of its own the stream runs after the steps and ends the same to the bit.
static int StreamSiteTest( void )
{
	enum
	{
		siteCount = 21,
		passCount = 2
	};
	int64_t baseBytes = nbGetByteCount();
	nbStreamStats results[passCount];
	uint32_t hashes[passCount];
	int damagedChunks[passCount];
	for ( int pass = 0; pass < passCount; ++pass )
	{
		TestScene scene = CreateScene();
		nbWorld_SetAsyncStep( scene.world, pass == 1 );

		StreamTestSites sites = { 0 };
		nbStreamDef def = nbDefaultStreamDef();
		def.loadRadius = 30.0f;
		def.unloadMargin = 10.0f;
		def.activeLoadRadius = 5.0f;
		def.activeHoldRadius = 8.0f;
		def.holdTime = 0.5f;
		def.retryTime = 0.1f;
		def.margin = 1.0f;
		def.prepareFcn = PrepareTestSite;
		def.releaseFcn = ReleaseTestSite;
		def.userContext = &sites;
		nbStreamId stream = nbCreateStream( scene.world, &def );
		ENSURE( NB_IS_NON_NULL( stream ) );
		for ( int i = 0; i < siteCount; ++i )
		{
			float x = 20.0f * (float)i;
			ENSURE( nbStream_AddSite( stream, (b3AABB){ { x - 2.0f, 0.0f, -0.15f }, { x + 2.0f, 3.0f, 0.15f } } ) == i );
		}

		// The walls at 0 and 20 m right away
		b3Vec3 viewer = { 0.0f, 1.5f, 0.0f };
		nbStream_LoadAround( stream, &viewer, 1 );
		ENSURE( nbStream_GetStats( stream ).loadedCount == 2 );

		// A grenade on the wall at 20 m, and a ball that falls next to the wall at 100 m, far from the viewer
		Grenade( &scene, (b3Vec3){ 20.0f, 1.5f, 0.3f } );
		b3BodyDef ballDef = b3DefaultBodyDef();
		ballDef.type = b3_dynamicBody;
		ballDef.position = b3ToPos( (b3Vec3){ 100.0f, 6.0f, 2.0f } );
		b3BodyId ball = b3CreateBody( scene.physicsWorld, &ballDef );
		b3Sphere sphere = { b3Vec3_zero, 0.3f };
		b3ShapeDef ballShape = b3DefaultShapeDef();
		b3CreateSphereShape( ball, &ballShape, &sphere );

		bool ballLoaded = false;
		for ( int frame = 0; frame < 180; ++frame )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			nbStream_Update( stream, &viewer, 1 );
			ballLoaded = ballLoaded || NB_IS_NON_NULL( nbStream_GetDestructible( stream, 5 ) );
		}
		ENSURE( ballLoaded );
		nbDestructibleId damaged = nbStream_GetDestructible( stream, 1 );
		ENSURE( nbDestructible_IsIntact( damaged ) == false );
		damagedChunks[pass] = nbDestructible_GetChunkCount( damaged );

		// Down the road and back at 1 m per frame. A wall within 25 m of the viewer is always there.
		for ( int frame = 0; frame < 800; ++frame )
		{
			viewer.x += frame < 400 ? 1.0f : -1.0f;
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			nbStream_Update( stream, &viewer, 1 );
			for ( int i = 0; i < siteCount; ++i )
			{
				float distance = b3AbsFloat( 20.0f * (float)i - viewer.x );
				ENSURE( distance > 25.0f || NB_IS_NON_NULL( nbStream_GetDestructible( stream, i ) ) );
			}
			if ( frame == 399 )
			{
				ENSURE( nbStream_GetStats( stream ).loadedCount <= 6 );
			}
		}

		// The damaged wall went with its rubble and came back with the same chunks
		nbStreamStats stats = nbStream_GetStats( stream );
		ENSURE( stats.saves >= 1 && stats.restores >= 1 && stats.urgentLoads >= 1 );
		ENSURE( stats.loads >= siteCount && stats.unloads >= siteCount - 4 );
		damaged = nbStream_GetDestructible( stream, 1 );
		ENSURE( nbDestructible_GetChunkCount( damaged ) == damagedChunks[pass] );

		nbDestructibleId ids[siteCount];
		for ( int i = 0; i < siteCount; ++i )
		{
			ids[i] = nbStream_GetDestructible( stream, i );
		}
		hashes[pass] = HashCreated( &scene, ids, siteCount );
		results[pass] = stats;

		// The world can take the stream with it, or the stream goes first and leaves its walls
		if ( pass == 1 )
		{
			nbDestroyStream( stream );
			ENSURE( nbWorld_GetStats( scene.world ).destructibleCount == stats.loadedCount );
		}
		DestroyScene( &scene );
		ENSURE( sites.prepares == sites.releases && sites.prepares >= stats.loads );
	}
	ENSURE( nbGetByteCount() == baseBytes );

	printf( "  %d loads, %d of them for a moving body, %d unloads, %d saves, %d brought back, damaged wall of %d chunks\n",
			results[0].loads, results[0].urgentLoads, results[0].unloads, results[0].saves, results[0].restores, damagedChunks[0] );
	ENSURE( hashes[1] == hashes[0] && damagedChunks[1] == damagedChunks[0] );
	ENSURE( results[1].loads == results[0].loads && results[1].unloads == results[0].unloads );
	ENSURE( results[1].saves == results[0].saves && results[1].restores == results[0].restores );
	ENSURE( results[1].urgentLoads == results[0].urgentLoads );
	return 0;
}

int WorldTest( void );

int WorldTest( void )
{
	RUN_TEST( CreateTest );
	RUN_TEST( ImpactTest );
	RUN_TEST( KillDepthTest );
	RUN_TEST( CastImpactTest );
	RUN_TEST( CollapseTest );
	RUN_TEST( DeterminismTest );
	RUN_TEST( WorkerTest );
	RUN_TEST( SIMDWidthTest );
	RUN_TEST( KnotTest );
	RUN_TEST( KnotGroupTest );
	RUN_TEST( TeamPacksTest );
	RUN_TEST( DomainTest );
	RUN_TEST( DomainLifeTest );
	RUN_TEST( UniqueHullTest );
	RUN_TEST( ExternalHullTest );
	RUN_TEST( ShapeBatchTest );
	RUN_TEST( ShapeContactListTest );
	RUN_TEST( ContactDataOfBodiesTest );
	RUN_TEST( ContactDataOfLargeBodyTest );
	RUN_TEST( TouchingContactsTest );
	RUN_TEST( TouchingContactsReplayTest );
	RUN_TEST( SpeculativeSphereTest );
	RUN_TEST( SeparatedHullsTest );
	RUN_TEST( SharedStaticBodyTest );
	RUN_TEST( LargeBlockTest );
	RUN_TEST( EventTest );
	RUN_TEST( DynamicDestructibleTest );
	RUN_TEST( MultiPieceTest );
	RUN_TEST( PreFractureTest );
	RUN_TEST( OpeningTest );
	RUN_TEST( SeamTest );
	RUN_TEST( GraphTest );
	RUN_TEST( RubbleTest );
	RUN_TEST( CannonballTest );
	RUN_TEST( SweepTest );
	RUN_TEST( VisibleFaceTest );
	RUN_TEST( DebrisBudgetTest );
	RUN_TEST( DebrisBudgetFlightTest );
	RUN_TEST( RestTimeTest );
	RUN_TEST( FragmentScaleTest );
	RUN_TEST( StoreyTest );
	RUN_TEST( StoreyResidueTest );
	RUN_TEST( ShotStoreysTest );
	RUN_TEST( StoreyPickTest );
	RUN_TEST( SupportTest );
	RUN_TEST( WallCollapseTest );
	RUN_TEST( BondMomentsTest );
	RUN_TEST( BatchCreateTest );
	RUN_TEST( StreamTest );
	RUN_TEST( BackgroundCreateTest );
	RUN_TEST( SaveLoadTest );
	RUN_TEST( TaskSystemTest );
	RUN_TEST( SettleWorkersTest );
	RUN_TEST( BudgetWorkersTest );
	RUN_TEST( AsyncStepTest );
	RUN_TEST( PlanStepsTest );
	RUN_TEST( StepRateTest );
	RUN_TEST( DrawPoseTest );
	RUN_TEST( ProfileTest );
	RUN_TEST( ScalingFitTest );
	RUN_TEST( WorkerCountTest );
	RUN_TEST( StreamSiteTest );
	RUN_TEST( RestTest );
	RUN_TEST( LandingTest );
	return 0;
}
