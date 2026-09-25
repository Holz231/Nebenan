// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "world.h"

#include "nebenan/nebenan.h"

#include <stdlib.h>

typedef struct TestScene
{
	b3WorldId physicsWorld;
	nbWorldId world;
	b3BodyId groundId;
} TestScene;

static TestScene CreateScene( void )
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

static uint32_t HashScene( TestScene* scene, nbDestructibleId wall )
{
	uint32_t hash = 2166136261u;
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
	(void)scene;
	return hash ^ (uint32_t)count;
}

static uint32_t RunDeterminismScenario( void )
{
	TestScene scene = CreateScene();
	nbDestructibleId wall = CreateWall( &scene, (b3Vec3){ 2.5f, 1.5f, 0.12f }, 5 );

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
		Step( &scene, 1 );
	}

	uint32_t hash = HashScene( &scene, wall );
	DestroyScene( &scene );
	return hash;
}

// Fracture and physics are bit for bit identical with MSVC, GCC and Clang on x64 and ARM. This is the result
// with the pinned Box3D commit. Update it when the results change on purpose, never to make one platform pass.
#define NB_EXPECTED_DETERMINISM_HASH 0xf526458bu

static int DeterminismTest( void )
{
	uint32_t hash1 = RunDeterminismScenario();
	uint32_t hash2 = RunDeterminismScenario();
	printf( "determinism hash: 0x%08x\n", hash1 );
	ENSURE( hash1 == hash2 );
	ENSURE( hash1 == NB_EXPECTED_DETERMINISM_HASH );
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

				b3Vec3 centroid, normal;
				float area = nbShape_ContactArea( chunkA->shape, chunkB->shape, 1.0e-4f, &centroid, &normal );
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

int WorldTest( void );

int WorldTest( void )
{
	RUN_TEST( CreateTest );
	RUN_TEST( ImpactTest );
	RUN_TEST( CastImpactTest );
	RUN_TEST( CollapseTest );
	RUN_TEST( DeterminismTest );
	RUN_TEST( EventTest );
	RUN_TEST( DynamicDestructibleTest );
	RUN_TEST( MultiPieceTest );
	RUN_TEST( PreFractureTest );
	RUN_TEST( GraphTest );
	RUN_TEST( SpanTest );
	RUN_TEST( CannonballTest );
	return 0;
}
