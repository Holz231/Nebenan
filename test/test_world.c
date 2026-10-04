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

		b3BodyId bodyId = nbChunk_GetBody( chunks[i] );
		b3Pos center = b3Body_GetWorldCenter( bodyId );
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
#define NB_EXPECTED_DETERMINISM_HASH 0x8849cb91u

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

// Static shapes of a batch go into Box3D's tree together. One that goes before the batch ends leaves it, one that moves
// goes in at its new place, queries find all the others, and a box dropped on them comes to rest there.
static int StaticBatchTest( void )
{
#if defined( B3_HAS_STATIC_BATCH )
	int64_t baseBytes = b3GetByteCount();
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BoxHull box = b3MakeBoxHull( 0.4f, 0.4f, 0.4f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();

	b3ShapeId shapes[64];
	b3World_BeginStaticBatch( worldId );
	for ( int i = 0; i < 64; ++i )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = (b3Pos){ (float)( i % 8 ), 0.4f, (float)( i / 8 ) };
		shapes[i] = b3CreateHullShape( b3CreateBody( worldId, &bodyDef ), &shapeDef, &box.base );
	}
	b3DestroyShape( shapes[9], false );
	b3Body_SetTransform( b3Shape_GetBody( shapes[18] ), (b3Pos){ 20.0f, 0.4f, 20.0f }, b3Quat_identity );
	b3World_EndStaticBatch( worldId );

	CountQuery field = { 0 };
	b3AABB fieldBox = { { -1.0f, -1.0f, -1.0f }, { 8.0f, 1.0f, 8.0f } };
	b3World_OverlapAABB( worldId, fieldBox, b3DefaultQueryFilter(), CountCallback, &field );
	ENSURE( field.count == 62 );

	CountQuery moved = { 0 };
	b3AABB movedBox = { { 19.0f, 0.0f, 19.0f }, { 21.0f, 1.0f, 21.0f } };
	b3World_OverlapAABB( worldId, movedBox, b3DefaultQueryFilter(), CountCallback, &moved );
	ENSURE( moved.count == 1 );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = (b3Pos){ 3.0f, 3.0f, 3.0f };
	b3BodyId dropped = b3CreateBody( worldId, &bodyDef );
	b3CreateHullShape( dropped, &shapeDef, &box.base );
	for ( int i = 0; i < 120; ++i )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	ENSURE( (float)b3Body_GetPosition( dropped ).y > 1.0f );

	b3DestroyWorld( worldId );
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
	// to life and comes down instead of hanging in the air.
	{
		TestScene scene = CreateScene();
		CreateLooseBox( &scene, (b3Vec3){ 0.0f, 0.5f, 0.0f }, (b3Vec3){ 1.0f, 0.5f, 1.0f }, 53 );
		nbDestructibleId upper = CreateLooseBox( &scene, (b3Vec3){ 0.0f, 1.5f, 0.0f }, (b3Vec3){ 0.5f, 0.5f, 0.5f }, 54 );
		Step( &scene, 60 );
		ENSURE( nbWorld_GetStats( scene.world ).rubbleCount == 2 );
		b3BodyId body = HeaviestBody( upper );
		float start = (float)b3Body_GetPosition( body ).y;

		nbImpactDef impact = { 0 };
		impact.point = (b3Pos){ 0.0f, 1.0f, 0.0f };
		impact.radius = 0.8f;
		impact.damage = 1.0e7f;
		impact.ejectSpeed = 12.0f;
		nbWorld_ApplyImpact( scene.world, &impact );
		Step( &scene, 90 );
		ENSURE( (float)b3Body_GetPosition( HeaviestBody( upper ) ).y < start - 0.1f );
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
static nbDestructibleId CreateCelledHouse( TestScene* scene, int floors, uint32_t seed )
{
	nbMaterial concrete = nbDefaultMaterial();
	concrete.strength = 1.1e6f;
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;
	float level = story + slab;
	float height = (float)floors * level;
	float inner = depth - 2.0f * t;

	nbOpening openings[4][16];
	int openingCounts[4] = { 0 };
	for ( int floor = 0; floor < floors; ++floor )
	{
		float y = (float)floor * level;
		float front0[] = { 3.9f, 0.0f, 5.1f, 2.2f, 1.0f, 1.0f, 2.6f, 2.2f, 6.4f, 1.0f, 8.0f, 2.2f };
		float front1[] = { 1.0f, 0.9f, 2.6f, 2.2f, 3.7f, 0.9f, 5.3f, 2.2f, 6.4f, 0.9f, 8.0f, 2.2f };
		float back[] = { 1.5f, 0.9f, 3.0f, 2.2f, 6.0f, 0.9f, 7.5f, 2.2f };
		float side[] = { 2.4f, 0.9f, 3.6f, 2.2f };
		AddOpenings( openings[0], openingCounts + 0, floor == 0 ? front0 : front1, 12, width, height, y, t );
		AddOpenings( openings[1], openingCounts + 1, back, 8, width, height, y, t );
		AddOpenings( openings[2], openingCounts + 2, side, 4, inner, height, y, t );
		AddOpenings( openings[3], openingCounts + 3, side, 4, inner, height, y, t );
	}

	// The front and back walls run along x, the end walls between them along z
	nbPieceDef pieces[8];
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
		wall->openings = openings[w];
		wall->openingCount = openingCounts[w];
	}

	for ( int floor = 0; floor < floors; ++floor )
	{
		nbPieceDef* floorSlab = pieces + count++;
		*floorSlab = nbDefaultPieceDef();
		floorSlab->halfExtents = (b3Vec3){ 0.5f * width - t, 0.5f * slab, 0.5f * depth - t };
		floorSlab->transform.p = (b3Vec3){ 0.0f, (float)floor * level + story + 0.5f * slab, 0.0f };
		floorSlab->material = &concrete;
		floorSlab->cellSize = -1.0f;
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.seed = seed;
	def.cellSize = 1.2f;
	def.material.density = 1900.0f;
	def.material.strength = 6.0e5f;
	return nbCreateDestructible( scene->world, &def, pieces, count );
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

// Volume of the pieces of at least a cubic meter that hang in the air: nothing lies right below their lowest chunks
static float FloatingVolume( TestScene* scene )
{
	nbWorld* world = nbGetWorldFromId( scene->world );
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

		bool carried = bottom < 0.05f;
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

		volume += carried ? 0.0f : actor->volume;
	}
	return volume;
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
	nbWorld_ApplyImpact( scene->world, &impact );

	b3ExplosionDef explosion = b3DefaultExplosionDef();
	explosion.position = point;
	explosion.radius = impact.radius;
	explosion.falloff = impact.radius;
	explosion.impulsePerArea = 40.0f * impact.ejectSpeed;
	b3World_Explode( scene->physicsWorld, &explosion );
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

// A building gives way storey by storey: when a storey lost more than half of its walls, on any floor, its last walls
// fly out and everything above comes down in one piece. Nothing else of a building collapses, not the wall above a hole,
// not a floor, not a column under load. Only what is cut off from the ground falls.
static int StoreyTest( void )
{
	// A four story house stands on its walls, and so does a town house of three stories with doors and windows.
	// Buildings never take the load check, not even with bonds that could not carry a thing.
	{
		TestScene scene = CreateScene();
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
		nbWorld_SetSupportScale( scene.world, 1.0f );
		nbDestructibleId house = CreateHouse( &scene, 1, 1, 0.0f );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount == 0 );
		ENSURE( HighestChunk( house ) > 3.0f );
		DestroyScene( &scene );
	}
	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 1.0f );
		nbDestructibleId house = CreateHouse( &scene, 2, 15, 0.15f );
		float start = CenterOfChunks( house ).y;
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).overloadedBondCount == 0 );
		ENSURE( CenterOfChunks( house ).y > start - 0.01f );
		DestroyScene( &scene );
	}

	// A few grenades at a corner of the ground floor of the celled house, as the demo builds it, do not bring it down
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

	// Grenades all around its ground floor bring it down. What is left of the ground floor flies out of the house in one
	// update, and the stories above come down in one piece with their floors, onto the ground. No wall stays standing.
	{
		TestScene scene = CreateScene();
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

		// What came down is still a building. Grenades around its second story bring the story above down onto the rest,
		// the upper story and two floors of 51 cubic meters.
		float top = high;
		BlastPieceStorey( &scene, 1 );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 2 );
		ENSURE( LargestPiece( &scene, &low, &high ) > 40.0f );
		ENSURE( high < top - 0.3f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// The same with the town house of wall boxes for every story. The story above must not stay up on the rubble that
	// froze on it while it was part of the house, or on rubble that froze on that rubble.
	{
		TestScene scene = CreateScene();
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

	// Grenades around the second story bring the story above down onto the ground floor, which stands. With the corners of
	// the second story left whole as well, a rigid shell the old load check never broke. What came down freezes there.
	// Grenades around the ground floor then bring that down too, with what lies on it, and nothing stays up in the air.
	for ( int corners = 0; corners < 2; ++corners )
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 1, 1 + corners, FLT_MAX, corners == 1 );
		Step( &scene, 150 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );
		float standing = HighestStanding( &scene );
		ENSURE( 2.9f < standing && standing < 3.6f );

		// The upper story and two floors are 51 cubic meters
		const nbActor* upper = LargestActor( &scene );
		ENSURE( upper != NULL && upper->isRubble && upper->volume > 40.0f );
		float low = 0.0f, high = 0.0f;
		LargestPiece( &scene, &low, &high );
		ENSURE( low > 2.5f );

		BlastStorey( &scene, 0, 1, FLT_MAX, false );
		Step( &scene, 120 );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 2 );
		ENSURE( HighestStanding( &scene ) < 1.0f );
		ENSURE( LargestPiece( &scene, &low, &high ) > 40.0f );
		ENSURE( low < 2.5f );
		ENSURE( FloatingVolume( &scene ) == 0.0f );
		DestroyScene( &scene );
	}

	// With the walls of the second story left only at +x, what comes down over it turns toward -x
	{
		TestScene scene = CreateScene();
		nbWorld_SetFragmentScale( scene.world, 2.0f );
		CreateCelledHouse( &scene, 3, 3 );
		BlastStorey( &scene, 1, 3, 0.5f, false );
		ENSURE( nbWorld_GetStats( scene.world ).collapsedStoreyCount == 1 );

		nbWorld* world = nbGetWorldFromId( scene.world );
		const nbActor* largest = NULL;
		for ( int i = 0; i < world->debris.count; ++i )
		{
			const nbActor* actor = world->actors.data + world->debris.data[i];
			largest = largest == NULL || actor->volume > largest->volume ? actor : largest;
		}
		ENSURE( largest != NULL && largest->volume > 40.0f );
		b3Vec3 spin = b3Body_GetAngularVelocity( largest->bodyId );
		ENSURE( spin.z > 0.2f && fabsf( spin.x ) < spin.z );
		DestroyScene( &scene );
	}

	// What came down keeps falling when an impact hits it on the way
	{
		TestScene scene = CreateScene();
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

// Structures without floors take the load check when it is on, see nbWorldDef::supportScale
static int SupportTest( void )
{
	// A wall under a concrete block stands while the check is off, as it is by default, and gives way when it comes on
	{
		TestScene scene = CreateScene();
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

// Only the load check reads the second moments of the bonds, see nbBondMoments. Without it nothing computes or keeps
// them. With it on from the start the bonds keep the moments of the faces they were made from, and when it comes on
// later, they get them from the shapes of their chunks, close to the same. Buildings with storeys never need them.
static int BondMomentsTest( void )
{
	{
		TestScene scene = CreateScene();
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

	{
		TestScene scene = CreateScene();
		nbWorld_SetSupportScale( scene.world, 0.03f );
		nbDestructibleId house = CreateCelledHouse( &scene, 2, 3 );
		const nbWorld* world = nbGetWorldFromId( scene.world );
		ENSURE( world->destructibles.data[house.index1 - 1].storeyCount > 0 );
		ENSURE( world->destructibles.data[house.index1 - 1].bondMoments == false );
		int keptAfterLoading = world->bondMoments.count;
		Grenade( &scene, (b3Vec3){ 0.0f, 1.2f, 3.4f } );
		world = nbGetWorldFromId( scene.world );
		ENSURE( world->bondMoments.count == keptAfterLoading );
		DestroyScene( &scene );
	}

	return 0;
}

int WorldTest( void );

int WorldTest( void )
{
	RUN_TEST( CreateTest );
	RUN_TEST( ImpactTest );
	RUN_TEST( CastImpactTest );
	RUN_TEST( CollapseTest );
	RUN_TEST( DeterminismTest );
	RUN_TEST( WorkerTest );
	RUN_TEST( UniqueHullTest );
	RUN_TEST( ExternalHullTest );
	RUN_TEST( StaticBatchTest );
	RUN_TEST( ShapeContactListTest );
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
	RUN_TEST( FragmentScaleTest );
	RUN_TEST( StoreyTest );
	RUN_TEST( SupportTest );
	RUN_TEST( BondMomentsTest );
	RUN_TEST( RestTest );
	RUN_TEST( LandingTest );
	return 0;
}
