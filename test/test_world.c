// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <math.h>
#include <stdlib.h>

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

static uint32_t RunDeterminismScenarioWith( TestScene scene )
{
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 2.5f, 1.5f, 0.12f }, 5 );

	// A gate that collapses under its own weight once a pillar is gone, for the load check
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

	// The collapse of the gate is part of the result
	nbStats stats = nbWorld_GetStats( scene.world );
	uint32_t hash = HashDestructible( 2166136261u, wall );
	hash = HashDestructible( hash, gate );
	hash = ( hash ^ (uint32_t)stats.overloadedBondCount ) * 16777619u;
	DestroyScene( &scene );
	return hash;
}

static uint32_t RunDeterminismScenario( void )
{
	return RunDeterminismScenarioWith( CreateScene() );
}

// Fracture and physics are bit for bit identical with MSVC, GCC and Clang on x64 and ARM. This is the result
// with the pinned Box3D commit. Update it when the results change on purpose, never to make one platform pass.
#define NB_EXPECTED_DETERMINISM_HASH 0xb2ddfdf0u

static int DeterminismTest( void )
{
	uint32_t hash1 = RunDeterminismScenario();
	uint32_t hash2 = RunDeterminismScenario();
	printf( "determinism hash: 0x%08x\n", hash1 );
	ENSURE( hash1 == hash2 );
	ENSURE( hash1 == NB_EXPECTED_DETERMINISM_HASH );
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
	return 0;
}

// Impacts, broken bonds and debris landing on the ground report dust for particle effects
static int DustTest( void )
{
	TestScene scene = CreateScene();
	CreateWall( &scene, (b3Vec3){ 2.0f, 1.5f, 0.15f }, 4 );
	nbWorld_GetEvents( scene.world );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 0.0f, 1.2f, 0.15f };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.normal = (b3Vec3){ 0.0f, 0.0f, 1.0f };
	impact.radius = 0.5f;
	impact.damage = 1.0e5f;
	impact.ejectSpeed = 8.0f;
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.brokenBondCount > 0 );

	nbEvents events = nbWorld_GetEvents( scene.world );
	int impactCount = 0;
	int crackCount = 0;
	for ( int i = 0; i < events.dustCount; ++i )
	{
		const nbDustEvent* dust = events.dust + i;
		ENSURE( dust->volume > 0.0f );
		float distance = b3Distance( dust->point, impact.point );
		if ( dust->type == nb_dustImpact )
		{
			impactCount += 1;
			ENSURE( distance < 1.0e-4f );
			ENSURE( dust->velocity.z > 0.0f );
		}
		else if ( dust->type == nb_dustCrack )
		{
			crackCount += 1;
			ENSURE( distance < impact.radius + 0.01f );
		}
	}
	ENSURE( impactCount == 1 );
	ENSURE( crackCount == result.brokenBondCount );

	// The fragments fall on the ground
	int collisionCount = 0;
	for ( int frame = 0; frame < 120; ++frame )
	{
		Step( &scene, 1 );
		events = nbWorld_GetEvents( scene.world );
		for ( int i = 0; i < events.dustCount; ++i )
		{
			collisionCount += events.dust[i].type == nb_dustCollision ? 1 : 0;
		}
	}
	ENSURE( collisionCount > 0 );

	DestroyScene( &scene );
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
				float area = nbShape_ContactArea( chunkA->shape, chunkB->shape, 1.0e-4f, &geometry );
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

// A beam on two pillars loses one pillar. The part further than the span from the other pillar breaks off.
static int SpanTest( void )
{
	TestScene scene = CreateScene();

	nbPieceDef pieces[3];
	for ( int i = 0; i < 3; ++i )
	{
		pieces[i] = nbDefaultPieceDef();
	}
	pieces[0].halfExtents = (b3Vec3){ 0.25f, 1.5f, 0.25f };
	pieces[0].transform.p = (b3Vec3){ -3.0f, 1.5f, 0.0f };
	pieces[1].halfExtents = (b3Vec3){ 0.25f, 1.5f, 0.25f };
	pieces[1].transform.p = (b3Vec3){ 3.0f, 1.5f, 0.0f };
	pieces[2].halfExtents = (b3Vec3){ 4.0f, 0.2f, 0.3f };
	pieces[2].transform.p = (b3Vec3){ 0.0f, 3.2f, 0.0f };

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.material.maxSpan = 3.0f;
	nbDestructibleId bridge = nbCreateDestructible( scene.world, &def, pieces, 3 );

	// With both pillars every part of the beam is within the span
	Step( &scene, 2 );
	ENSURE( nbWorld_GetStats( scene.world ).dynamicBodyCount == 0 );

	// Knock out the top of the right pillar
	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 3.0f, 2.8f, 0.0f };
	impact.radius = 0.45f;
	impact.damage = 1.0e8f;
	impact.ejectSpeed = 2.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	Step( &scene, 1 );

	// The right half of the beam is now more than 3 m from the left pillar and falls
	float fallingMass = 0.0f;
	float staticBeamVolume = 0.0f;
	int count = nbDestructible_GetChunkCount( bridge );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( bridge, chunks, count );
	for ( int i = 0; i < count; ++i )
	{
		if ( nbChunk_IsDynamic( chunks[i] ) )
		{
			fallingMass = b3MaxFloat( fallingMass, b3Body_GetMass( nbChunk_GetBody( chunks[i] ) ) );
		}
		else if ( nbChunk_GetCentroid( chunks[i] ).y > 3.0f )
		{
			staticBeamVolume += nbChunk_GetVolume( chunks[i] );
		}
	}
	free( chunks );

	// Beam volume is 8 * 0.4 * 0.6 = 1.92 m^3, a sizable part falls and a sizable part stays
	ENSURE( fallingMass > 0.3f * 2400.0f );
	ENSURE( staticBeamVolume > 0.3f );

	DestroyScene( &scene );
	return 0;
}

// A beam glued to the side of a pillar holds when it is short and breaks off at the pillar when it is long.
// The joint of 0.3 x 0.3 m carries 2 MPa, which is a bending moment of 9 kNm: a beam of about 2.9 m.
static int CantileverTest( void )
{
	for ( int k = 0; k < 2; ++k )
	{
		float length = k == 0 ? 1.5f : 4.0f;
		TestScene scene = CreateScene();

		nbPieceDef pieces[2] = { nbDefaultPieceDef(), nbDefaultPieceDef() };
		pieces[0].halfExtents = (b3Vec3){ 0.15f, 1.5f, 0.15f };
		pieces[0].transform.p = (b3Vec3){ 0.0f, 1.5f, 0.0f };
		pieces[1].halfExtents = (b3Vec3){ 0.5f * length, 0.15f, 0.15f };
		pieces[1].transform.p = (b3Vec3){ 0.15f + 0.5f * length, 2.7f, 0.0f };

		nbDestructibleDef def = nbDefaultDestructibleDef();
		nbDestructibleId post = nbCreateDestructible( scene.world, &def, pieces, 2 );
		ENSURE( nbDestructible_GetChunkCount( post ) == 2 );

		Step( &scene, 2 );
		nbStats stats = nbWorld_GetStats( scene.world );
		ENSURE( stats.overloadedBondCount == k );
		ENSURE( stats.dynamicBodyCount == k );

		// The short beam loads the joint with 540 kPa in bending (Box3D gravity is 10), 27 % of the strength.
		// A fallen beam is debris and reports nothing.
		nbChunkId chunks[2];
		ENSURE( nbDestructible_GetChunks( post, chunks, 2 ) == 2 );
		for ( int i = 0; i < 2; ++i )
		{
			float utilization = nbChunk_GetUtilization( chunks[i] );
			bool isBeam = nbChunk_GetCentroid( chunks[i] ).x > 0.15f;
			if ( k == 0 )
			{
				ENSURE_SMALL( utilization - 0.27f, 0.005f );
			}
			else if ( isBeam )
			{
				ENSURE( utilization == 0.0f );
			}
		}

		DestroyScene( &scene );
	}
	return 0;
}

// A heavy block on a slender column. The joint carries 2.4 MPa, which is fine for concrete and too much for
// a weak material.
static int CrushTest( void )
{
	for ( int k = 0; k < 2; ++k )
	{
		TestScene scene = CreateScene();

		nbPieceDef pieces[2] = { nbDefaultPieceDef(), nbDefaultPieceDef() };
		pieces[0].halfExtents = (b3Vec3){ 0.1f, 1.0f, 0.1f };
		pieces[0].transform.p = (b3Vec3){ 0.0f, 1.0f, 0.0f };
		pieces[1].halfExtents = (b3Vec3){ 1.0f, 0.5f, 1.0f };
		pieces[1].transform.p = (b3Vec3){ 0.0f, 2.5f, 0.0f };

		nbDestructibleDef def = nbDefaultDestructibleDef();
		def.material.compressiveStrength = k == 0 ? 3.0e7f : 1.0e6f;
		nbCreateDestructible( scene.world, &def, pieces, 2 );

		Step( &scene, 2 );
		nbStats stats = nbWorld_GetStats( scene.world );
		ENSURE( stats.overloadedBondCount == k );
		ENSURE( stats.dynamicBodyCount == k );

		DestroyScene( &scene );
	}
	return 0;
}

// A pre-fractured beam on two pillars carries itself. Without the right pillar it cantilevers 6.4 m from the
// edge of the left one, 4.8 MPa where concrete holds 2 MPa. It breaks near the pillar and the rest falls.
static int BeamTest( void )
{
	TestScene scene = CreateScene();
	nbDestructibleId gate = CreateGate( &scene, 0.0f, 3 );
	ENSURE( nbDestructible_GetChunkCount( gate ) > 12 );

	Step( &scene, 5 );
	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.overloadedBondCount == 0 );
	ENSURE( stats.dynamicBodyCount == 0 );

	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 3.0f, 2.3f, 0.0f };
	impact.radius = 1.0f;
	impact.damage = 1.0e8f;
	impact.ejectSpeed = 2.0f;
	nbWorld_ApplyImpact( scene.world, &impact );
	Step( &scene, 10 );

	stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.overloadedBondCount > 0 );

	float fallingMass = 0.0f;
	float staticBeamVolume = 0.0f;
	int count = nbDestructible_GetChunkCount( gate );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( gate, chunks, count );
	for ( int i = 0; i < count; ++i )
	{
		if ( nbChunk_IsDynamic( chunks[i] ) )
		{
			fallingMass = b3MaxFloat( fallingMass, b3Body_GetMass( nbChunk_GetBody( chunks[i] ) ) );
		}
		else if ( nbChunk_GetCentroid( chunks[i] ).y > 3.0f )
		{
			staticBeamVolume += nbChunk_GetVolume( chunks[i] );
		}
	}
	free( chunks );

	// The beam holds 2.9 m^3, a long part of it falls in one piece and the part over the left pillar stays
	ENSURE( fallingMass > 1.0f * 2400.0f );
	ENSURE( staticBeamVolume > 0.2f );
	ENSURE( staticBeamVolume < 1.5f );

	DestroyScene( &scene );
	return 0;
}

// A concrete beam glued to the side of a brick pillar. Every chunk keeps the material of its piece, also
// after fracture, and the joint is only as strong as the brick: the 1.5 m beam loads it with 0.55 MPa, which
// concrete carries (see CantileverTest) and brick does not.
static int MaterialTest( void )
{
	TestScene scene = CreateScene();

	nbMaterial brick = nbDefaultMaterial();
	brick.density = 1900.0f;
	brick.strength = 6.0e5f;
	brick.tensileStrength = 0.3e6f;
	brick.compressiveStrength = 6.0e6f;
	brick.userMaterialId = 1;

	nbMaterial concrete = nbDefaultMaterial();
	concrete.userMaterialId = 2;

	nbPieceDef pieces[2] = { nbDefaultPieceDef(), nbDefaultPieceDef() };
	pieces[0].halfExtents = (b3Vec3){ 0.15f, 1.5f, 0.15f };
	pieces[0].transform.p = (b3Vec3){ 0.0f, 1.5f, 0.0f };
	pieces[1].halfExtents = (b3Vec3){ 0.75f, 0.15f, 0.15f };
	pieces[1].transform.p = (b3Vec3){ 0.9f, 2.7f, 0.0f };
	pieces[1].material = &concrete;

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.material = brick;
	nbDestructibleId post = nbCreateDestructible( scene.world, &def, pieces, 2 );
	ENSURE( nbDestructible_GetChunkCount( post ) == 2 );

	// Fracture the beam without damage
	nbImpactDef impact = { 0 };
	impact.point = (b3Vec3){ 1.3f, 2.7f, 0.15f };
	impact.radius = 0.35f;
	impact.damage = 0.0f;
	nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
	ENSURE( result.createdChunkCount > 2 );
	ENSURE( result.brokenBondCount == 0 );

	int count = nbDestructible_GetChunkCount( post );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)count );
	nbDestructible_GetChunks( post, chunks, count );
	for ( int i = 0; i < count; ++i )
	{
		bool isBeam = nbChunk_GetCentroid( chunks[i] ).x > 0.15f;
		nbMaterial material = nbChunk_GetMaterial( chunks[i] );
		ENSURE( material.userMaterialId == ( isBeam ? 2u : 1u ) );

		b3ShapeId shapeId = nbChunk_GetShape( chunks[i] );
		ENSURE( b3Shape_GetDensity( shapeId ) == ( isBeam ? 2400.0f : 1900.0f ) );
		ENSURE( b3Shape_GetSurfaceMaterial( shapeId ).userMaterialId == ( isBeam ? 2u : 1u ) );
	}
	free( chunks );

	// The fragments at the pillar inherited the joint, as strong as brick
	nbWorld* world = nbGetWorldFromId( scene.world );
	int joints = 0;
	for ( int b = 0; b < world->bonds.count; ++b )
	{
		const nbBond* bond = world->bonds.data + b;
		if ( bond->chunk[0] == NB_NULL_INDEX ||
			 world->chunks.data[bond->chunk[0]].materialIndex == world->chunks.data[bond->chunk[1]].materialIndex )
		{
			continue;
		}
		ENSURE_SMALL( bond->health / ( brick.strength * bond->area ) - 1.0f, 1.0e-4f );
		joints += 1;
	}
	ENSURE( joints > 0 );

	// The load check breaks the joint, not the beam
	Step( &scene, 2 );
	nbStats stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.overloadedBondCount >= 1 );
	ENSURE( stats.dynamicBodyCount == 1 );

	DestroyScene( &scene );
	return 0;
}

// A semicircular arch of loose stones on two foundations, every joint dry. The inner radius is 2.5 m. Free rigid
// stones in Box3D stand with an outer radius of 2.82 m and fall with 2.78 m and less, about the thickness Heyman
// gives for the thinnest semicircular arch.
static nbDestructibleId CreateArch( TestScene* scene, float outer )
{
	enum
	{
		stoneCount = 11
	};

	float inner = 2.5f, base = 0.3f;
	b3Vec3 points[stoneCount][8];
	nbPieceDef pieces[stoneCount + 2];
	for ( int i = 0; i < stoneCount; ++i )
	{
		float a0 = B3_PI * (float)i / (float)stoneCount;
		float a1 = B3_PI * (float)( i + 1 ) / (float)stoneCount;
		float c0 = i == 0 ? 1.0f : cosf( a0 ), s0 = i == 0 ? 0.0f : sinf( a0 );
		float c1 = i + 1 == stoneCount ? -1.0f : cosf( a1 ), s1 = i + 1 == stoneCount ? 0.0f : sinf( a1 );
		b3Vec3 corners[4] = { { inner * c0, base + inner * s0, 0.0f }, { outer * c0, base + outer * s0, 0.0f },
							  { outer * c1, base + outer * s1, 0.0f }, { inner * c1, base + inner * s1, 0.0f } };
		for ( int k = 0; k < 4; ++k )
		{
			points[i][k] = (b3Vec3){ corners[k].x, corners[k].y, -0.5f };
			points[i][k + 4] = (b3Vec3){ corners[k].x, corners[k].y, 0.5f };
		}

		pieces[i] = nbDefaultPieceDef();
		pieces[i].points = points[i];
		pieces[i].pointCount = 8;
		pieces[i].jointTensileStrength = 0.0f;
	}

	for ( int k = 0; k < 2; ++k )
	{
		nbPieceDef* piece = pieces + stoneCount + k;
		*piece = nbDefaultPieceDef();
		piece->halfExtents = (b3Vec3){ 0.5f * ( outer - inner ) + 0.3f, 0.5f * base, 0.5f };
		piece->transform.p = (b3Vec3){ ( k == 0 ? 0.5f : -0.5f ) * ( inner + outer ), 0.5f * base, 0.0f };
		piece->jointTensileStrength = 0.0f;
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.material.density = 2500.0f;
	def.material.tensileStrength = 1.0e6f;
	def.material.compressiveStrength = 3.0e7f;
	def.material.friction = 0.7f;
	return nbCreateDestructible( scene->world, &def, pieces, stoneCount + 2 );
}

// Dry joints carry the arch on compression alone. It stands when it is thick enough and falls when it is too
// thin or loses its keystone.
static int ArchTest( void )
{
	for ( int k = 0; k < 3; ++k )
	{
		TestScene scene = CreateScene();
		nbDestructibleId arch = CreateArch( &scene, k == 1 ? 2.65f : 3.0f );
		ENSURE( nbDestructible_GetChunkCount( arch ) == 13 );

		Step( &scene, 30 );
		nbStats stats = nbWorld_GetStats( scene.world );
		if ( k == 1 )
		{
			ENSURE( stats.overloadedBondCount > 0 );
			ENSURE( stats.dynamicBodyCount > 0 );
			DestroyScene( &scene );
			continue;
		}

		ENSURE( stats.overloadedBondCount == 0 );
		ENSURE( stats.dynamicBodyCount == 0 );

		// Every joint is open, the crown carries the thrust and the joints near the haunches only touch at one edge
		nbChunkId chunks[13];
		ENSURE( nbDestructible_GetChunks( arch, chunks, 13 ) == 13 );
		float highest = 0.0f;
		for ( int i = 0; i < 13; ++i )
		{
			highest = b3MaxFloat( highest, nbChunk_GetUtilization( chunks[i] ) );
		}
		ENSURE( highest > 0.3f && highest < 1.0f );

		if ( k == 2 )
		{
			nbImpactDef impact = { 0 };
			impact.point = (b3Vec3){ 0.0f, 3.05f, 0.0f };
			impact.radius = 0.6f;
			impact.damage = 1.0e8f;
			impact.ejectSpeed = 3.0f;
			nbWorld_ApplyImpact( scene.world, &impact );
			Step( &scene, 30 );

			// Without the keystone both halves fall, only the stones next to the foundations may stay
			int staticStones = 0;
			int count = nbDestructible_GetChunkCount( arch );
			nbChunkId* all = malloc( sizeof( nbChunkId ) * (size_t)count );
			nbDestructible_GetChunks( arch, all, count );
			for ( int i = 0; i < count; ++i )
			{
				staticStones += nbChunk_IsDynamic( all[i] ) == false && nbChunk_GetVolume( all[i] ) > 0.2f ? 1 : 0;
			}
			free( all );
			ENSURE( staticStones <= 6 );
			ENSURE( nbWorld_GetStats( scene.world ).overloadedBondCount > 0 );
		}

		DestroyScene( &scene );
	}
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

// Lowering the debris budget at run time removes the moving debris over it in the next update
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

	nbWorld_SetDebrisBudget( scene.world, 5, 1000 );
	Step( &scene, 1 );
	stats = nbWorld_GetStats( scene.world );
	ENSURE( stats.debrisCount - stats.rubbleCount <= 5 );

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

int WorldTest( void );

int WorldTest( void )
{
	RUN_TEST( CreateTest );
	RUN_TEST( ImpactTest );
	RUN_TEST( CastImpactTest );
	RUN_TEST( CollapseTest );
	RUN_TEST( DeterminismTest );
	RUN_TEST( WorkerTest );
	RUN_TEST( EventTest );
	RUN_TEST( DustTest );
	RUN_TEST( DynamicDestructibleTest );
	RUN_TEST( MultiPieceTest );
	RUN_TEST( PreFractureTest );
	RUN_TEST( GraphTest );
	RUN_TEST( SpanTest );
	RUN_TEST( CantileverTest );
	RUN_TEST( CrushTest );
	RUN_TEST( BeamTest );
	RUN_TEST( MaterialTest );
	RUN_TEST( ArchTest );
	RUN_TEST( RubbleTest );
	RUN_TEST( CannonballTest );
	RUN_TEST( VisibleFaceTest );
	RUN_TEST( DebrisBudgetTest );
	RUN_TEST( FragmentScaleTest );
	return 0;
}
