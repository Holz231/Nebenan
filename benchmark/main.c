// SPDX-License-Identifier: MIT

// Headless benchmark for the fracture kernel and the full impact pipeline.
// Usage: nebenan_benchmark [workerCount], the worker count applies to the fracture and to Box3D

#include "fracture.h"
#include "hull_builder.h"

#include "nebenan/nebenan.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Scene
{
	b3WorldId physicsWorld;
	nbWorldId world;
} Scene;

static Scene CreateScene( int workerCount )
{
	Scene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	scene.physicsWorld = b3CreateWorld( &worldDef );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
	b3BodyId groundId = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 100.0f, 1.0f, 100.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( groundId, &shapeDef, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.workerCount = workerCount;
	scene.world = nbCreateWorld( &def );
	return scene;
}

static void DestroyScene( Scene* scene )
{
	nbDestroyWorld( scene->world );
	b3DestroyWorld( scene->physicsWorld );
}

static void BenchmarkKernel( void )
{
	printf( "\nVoronoi kernel (4 x 2 x 0.3 m slab, impact focused sites)\n" );
	printf( "  cells | voronoi ms | direct hulls ms | quickhull ms | cells/s (voronoi + direct hulls)\n" );

	nbArena arena;
	nbArena_Create( &arena, 1 << 20 );
	nbPoly* parent = malloc( sizeof( nbPoly ) );
	nbPoly_MakeBox( parent, (b3Vec3){ 2.0f, 1.0f, 0.15f }, b3Transform_identity, 0 );

	int counts[] = { 16, 32, 64, 128, 256 };
	for ( int c = 0; c < 5; ++c )
	{
		int target = counts[c];
		float fractureTime = 0.0f;
		float hullTime = 0.0f;
		float quickhullTime = 0.0f;
		int cellCount = 0;
		int repeats = 50;

		for ( int r = 0; r < repeats; ++r )
		{
			nbArena_Reset( &arena );
			nbRandom rng = nbMakeRandom( 1000 + r, 0 );
			nbSiteParams params = {
				.center = { 0.3f, 0.1f, 0.0f },
				.radius = 0.7f,
				.innerCount = target * 3 / 4,
				.ringCount = target / 8,
				.outerCount = target / 8,
				.minSpacing = 0.03f,
			};

			b3Vec3 sites[512];
			int siteCount = nbGenerateSites( parent, &params, &rng, sites, 512 );

			uint64_t ticks = b3GetTicks();
			nbFractureOutput output;
			nbComputeVoronoiCells( &arena, parent, sites, siteCount, 1, 1.0e-6f, 0.0f, &output );
			fractureTime += b3GetMilliseconds( ticks );

			// The hull path the library uses: straight from the known topology
			ticks = b3GetTicks();
			for ( int i = 0; i < output.cellCount; ++i )
			{
				nbShape* shape = output.cells[i].shape;
				if ( shape == NULL )
				{
					continue;
				}
				void* memory = nbArena_Alloc( &arena, (size_t)nbGetHullByteCount( shape ) );
				if ( nbBuildHull( shape, memory ) != NULL )
				{
					cellCount += 1;
				}
			}
			hullTime += b3GetMilliseconds( ticks );

			// For comparison: Box3D's general quickhull on the same points
			ticks = b3GetTicks();
			for ( int i = 0; i < output.cellCount; ++i )
			{
				nbShape* shape = output.cells[i].shape;
				if ( shape != NULL )
				{
					b3DestroyHull( b3CreateHull( shape->vertices, shape->vertexCount, B3_MAX_HULL_VERTICES ) );
				}
			}
			quickhullTime += b3GetMilliseconds( ticks );

			for ( int i = 0; i < output.cellCount; ++i )
			{
				nbShape_Destroy( output.cells[i].shape );
			}
		}

		float cellsPerRun = (float)cellCount / (float)repeats;
		fractureTime /= (float)repeats;
		hullTime /= (float)repeats;
		quickhullTime /= (float)repeats;
		printf( "  %5.0f | %10.3f | %15.3f | %12.3f | %31.0f\n", cellsPerRun, fractureTime, hullTime, quickhullTime,
				1000.0f * cellsPerRun / ( fractureTime + hullTime ) );
	}

	free( parent );
	nbArena_Destroy( &arena );
}

typedef struct Timings
{
	float impactTotal;
	float impactMax;
	float fractureTotal;
	float stepTotal;
	float stepMax;
	float updateTotal;
	int impactCount;
	int stepCount;
	int chunks;
} Timings;

static void Report( const char* name, const Timings* t, nbStats stats )
{
	printf( "  %-26s impacts %4d | impact avg %6.3f ms max %6.3f ms (fracture avg %6.3f) | step avg %6.3f ms max %6.3f ms | "
			"update avg %6.3f ms | chunks %5d bodies %5d overloaded %4d\n",
			name, t->impactCount, t->impactTotal / (float)b3MaxInt( t->impactCount, 1 ), t->impactMax,
			t->fractureTotal / (float)b3MaxInt( t->impactCount, 1 ), t->stepTotal / (float)b3MaxInt( t->stepCount, 1 ), t->stepMax,
			t->updateTotal / (float)b3MaxInt( t->stepCount, 1 ), stats.chunkCount, stats.dynamicBodyCount + stats.staticBodyCount,
			stats.overloadedBondCount );
}

static void Step( Scene* scene, Timings* t )
{
	uint64_t ticks = b3GetTicks();
	b3World_Step( scene->physicsWorld, 1.0f / 60.0f, 4 );
	float stepTime = b3GetMilliseconds( ticks );
	ticks = b3GetTicks();
	nbWorld_Update( scene->world, 1.0f / 60.0f );
	t->updateTotal += b3GetMilliseconds( ticks );
	t->stepTotal += stepTime;
	t->stepMax = b3MaxFloat( t->stepMax, stepTime );
	t->stepCount += 1;
}

static void Impact( Scene* scene, Timings* t, const nbImpactDef* def )
{
	nbImpactResult result = nbWorld_ApplyImpact( scene->world, def );
	t->impactTotal += result.totalTime;
	t->impactMax = b3MaxFloat( t->impactMax, result.totalTime );
	t->fractureTotal += result.fractureTime;
	t->impactCount += 1;
}

// A brick wall under rifle fire: many small impacts spread over the wall
static void BenchmarkRifle( int workerCount )
{
	Scene scene = CreateScene( workerCount );
	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.position = (b3Vec3){ 0.0f, 2.0f, 0.0f };
	nbCreateBox( scene.world, &def, (b3Vec3){ 4.0f, 2.0f, 0.15f } );

	Timings t = { 0 };
	nbRandom rng = nbMakeRandom( 77, 0 );
	nbImpactDef impact = { 0 };
	impact.direction = (b3Vec3){ 0.0f, 0.0f, -1.0f };
	impact.radius = 0.3f;
	impact.damage = 6.0e4f;
	impact.ejectSpeed = 8.0f;

	for ( int frame = 0; frame < 600; ++frame )
	{
		if ( frame % 3 == 0 )
		{
			float x = nbRandomRange( &rng, -3.5f, 3.5f );
			float y = nbRandomRange( &rng, 0.5f, 3.8f );
			impact.point = (b3Vec3){ x, y, 0.15f };
			Impact( &scene, &t, &impact );
		}
		Step( &scene, &t );
	}

	Report( "rifle (200 hits)", &t, nbWorld_GetStats( scene.world ) );
	DestroyScene( &scene );
}

// Large explosions on a row of walls
static void BenchmarkExplosions( int workerCount )
{
	Scene scene = CreateScene( workerCount );
	for ( int i = 0; i < 4; ++i )
	{
		nbDestructibleDef def = nbDefaultDestructibleDef();
		def.position = (b3Vec3){ 0.0f, 2.0f, -3.0f * (float)i };
		def.seed = (uint32_t)( 10 + i );
		nbCreateBox( scene.world, &def, (b3Vec3){ 5.0f, 2.0f, 0.2f } );
	}

	Timings t = { 0 };
	nbRandom rng = nbMakeRandom( 5, 0 );
	nbImpactDef impact = { 0 };
	impact.radius = 1.4f;
	impact.damage = 2.0e5f;
	impact.ejectSpeed = 14.0f;

	for ( int frame = 0; frame < 600; ++frame )
	{
		if ( frame % 30 == 0 )
		{
			int wall = ( frame / 30 ) % 4;
			float x = nbRandomRange( &rng, -3.5f, 3.5f );
			float y = nbRandomRange( &rng, 0.5f, 3.5f );
			impact.point = (b3Vec3){ x, y, -3.0f * (float)wall };
			Impact( &scene, &t, &impact );
		}
		Step( &scene, &t );
	}

	Report( "explosions (20 blasts)", &t, nbWorld_GetStats( scene.world ) );
	DestroyScene( &scene );
}

// A building of walls and slabs whose ground floor is blasted on three sides
static void BenchmarkBuilding( int workerCount )
{
	Scene scene = CreateScene( workerCount );

	nbPieceDef pieces[32];
	int pieceCount = 0;
	for ( int floor = 0; floor < 3; ++floor )
	{
		float y = 3.0f * (float)floor;
		b3Vec3 wallHalf[4] = { { 4.0f, 1.35f, 0.15f }, { 4.0f, 1.35f, 0.15f }, { 0.15f, 1.35f, 3.7f }, { 0.15f, 1.35f, 3.7f } };
		b3Vec3 wallPos[4] = { { 0.0f, y + 1.35f, 3.85f }, { 0.0f, y + 1.35f, -3.85f }, { 3.85f, y + 1.35f, 0.0f }, { -3.85f, y + 1.35f, 0.0f } };
		for ( int w = 0; w < 4; ++w )
		{
			nbPieceDef* piece = pieces + pieceCount++;
			*piece = nbDefaultPieceDef();
			piece->halfExtents = wallHalf[w];
			piece->transform.p = wallPos[w];
		}

		nbPieceDef* slab = pieces + pieceCount++;
		*slab = nbDefaultPieceDef();
		slab->halfExtents = (b3Vec3){ 4.0f, 0.15f, 4.0f };
		slab->transform.p = (b3Vec3){ 0.0f, y + 2.85f, 0.0f };
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.cellSize = 1.0f;
	uint64_t ticks = b3GetTicks();
	nbDestructibleId building = nbCreateDestructible( scene.world, &def, pieces, pieceCount );
	float createTime = b3GetMilliseconds( ticks );
	printf( "  building with %d pieces pre-fractured into %d chunks in %.2f ms\n", pieceCount,
			nbDestructible_GetChunkCount( building ), createTime );

	Timings t = { 0 };
	nbImpactDef impact = { 0 };
	impact.radius = 1.2f;
	impact.damage = 1.0e6f;
	impact.ejectSpeed = 10.0f;

	for ( int frame = 0; frame < 480; ++frame )
	{
		if ( frame < 72 && frame % 4 == 0 )
		{
			// Blast three walls of the ground floor, the floors above hang on the last wall
			int k = frame / 4;
			float s = -3.5f + 1.4f * (float)( k % 6 );
			b3Vec3 points[3] = { { s, 1.0f, 3.85f }, { 3.85f, 1.0f, s }, { s, 1.0f, -3.85f } };
			impact.point = points[k / 6];
			Impact( &scene, &t, &impact );
		}
		Step( &scene, &t );
	}

	Report( "building", &t, nbWorld_GetStats( scene.world ) );
	DestroyScene( &scene );
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

// The pieces of a house and the openings they point to, so a whole town can be created at once
typedef struct House
{
	nbOpening openings[4][16];
	nbPieceDef pieces[8];
	int pieceCount;
	nbDestructibleDef def;
} House;

// Two stories of brick walls through both floors with doors and windows, pre-fractured into cells, and concrete
// floors inside them, like the house of the demo
static void BuildHouse( House* house, b3Vec3 position, uint32_t seed, const nbMaterial* concrete )
{
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;
	float level = story + slab;
	int floors = 2;
	float height = (float)floors * level;
	float inner = depth - 2.0f * t;

	nbOpening( *openings )[16] = house->openings;
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
		wall->openings = openings[w];
		wall->openingCount = openingCounts[w];
	}

	// The floors stay whole until something hits them
	for ( int floor = 0; floor < floors; ++floor )
	{
		nbPieceDef* floorSlab = pieces + count++;
		*floorSlab = nbDefaultPieceDef();
		floorSlab->halfExtents = (b3Vec3){ 0.5f * width - t, 0.5f * slab, 0.5f * depth - t };
		floorSlab->transform.p = (b3Vec3){ 0.0f, (float)floor * level + story + 0.5f * slab, 0.0f };
		floorSlab->material = concrete;
		floorSlab->cellSize = -1.0f;
	}

	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.position = position;
	def.seed = seed;
	def.cellSize = 1.2f;
	def.material.density = 1900.0f;
	def.material.strength = 6.0e5f;
	def.material.fragmentSize = 0.1f;
	def.material.friction = 0.8f;
	house->pieceCount = count;
	house->def = def;
}

static int CompareFloats( const void* a, const void* b )
{
	float x = *(const float*)a, y = *(const float*)b;
	return x < y ? -1 : ( x > y ? 1 : 0 );
}

// A town of 16 houses wrecked by a grenade every five frames, twelve per second, like holding the fire button
// A block of side x side houses under fire: every interval steps as many grenades, each on a wall of a random house
static void BenchmarkTown( int workerCount, float fragmentScale, int side, int grenadesPerVolley, int interval )
{
	Scene scene = CreateScene( workerCount );
	nbWorld_SetFragmentScale( scene.world, fragmentScale );
	nbMaterial concrete = nbDefaultMaterial();
	concrete.strength = 1.1e6f;
	concrete.fragmentSize = 0.13f;

	// All houses at once: while the calling thread builds one into the world, the workers fracture the next
	int houseCount = side * side;
	b3Vec3* houses = malloc( sizeof( b3Vec3 ) * (size_t)houseCount );
	House* town = malloc( sizeof( House ) * (size_t)houseCount );
	nbDestructibleDef* defs = malloc( sizeof( nbDestructibleDef ) * (size_t)houseCount );
	const nbPieceDef** pieceLists = malloc( sizeof( nbPieceDef* ) * (size_t)houseCount );
	int* pieceCounts = malloc( sizeof( int ) * (size_t)houseCount );
	nbDestructibleId* ids = malloc( sizeof( nbDestructibleId ) * (size_t)houseCount );
	float half = 0.5f * (float)( side - 1 );
	for ( int h = 0; h < houseCount; ++h )
	{
		int i = h / side, j = h % side;
		houses[h] = (b3Vec3){ 14.0f * ( (float)i - half ), 0.0f, 12.0f * ( (float)j - half ) };
		BuildHouse( town + h, houses[h], (uint32_t)( 100 + h ), &concrete );
		defs[h] = town[h].def;
		pieceLists[h] = town[h].pieces;
		pieceCounts[h] = town[h].pieceCount;
	}

	uint64_t ticks = b3GetTicks();
	nbCreateDestructibles( scene.world, defs, pieceLists, pieceCounts, houseCount, ids );
	float createTime = b3GetMilliseconds( ticks );
	free( town );
	free( defs );
	free( pieceLists );
	free( pieceCounts );
	free( ids );
	nbStats created = nbWorld_GetStats( scene.world );
	printf( "  town of %d houses with %d chunks built in %.2f ms, fragment scale %.1f\n", houseCount, created.chunkCount, createTime,
			fragmentScale );

	enum
	{
		frameCount = 1200
	};

	static float impactTimes[frameCount], updateTimes[frameCount], stepTimes[frameCount], frameTimes[frameCount];
	nbRandom rng = nbMakeRandom( 9, 0 );
	nbImpactDef impact = { 0 };
	impact.radius = 1.3f;
	impact.damage = 3.0e5f;
	impact.ejectSpeed = 12.0f;
	int maxBodies = 0, maxAwake = 0, maxContacts = 0, grenades = 0;
	float physicsProfile[8] = { 0 };

	for ( int frame = 0; frame < frameCount; ++frame )
	{
		float impactTime = 0.0f;
		for ( int g = 0; g < ( frame % interval == 0 ? grenadesPerVolley : 0 ); ++g )
		{
			// A wall of a random house, on either story
			b3Vec3 house = houses[(int)( nbRandomRange( &rng, 0.0f, (float)houseCount - 0.001f ) )];
			int wall = (int)nbRandomRange( &rng, 0.0f, 3.999f );
			float along = nbRandomRange( &rng, -1.0f, 1.0f );
			float height = nbRandomRange( &rng, 0.0f, 1.0f ) < 0.5f ? 1.2f : 4.4f;
			b3Vec3 local = wall == 0 ? (b3Vec3){ 4.2f * along, height, 3.4f }
						 : wall == 1 ? (b3Vec3){ 4.2f * along, height, -3.4f }
						 : wall == 2 ? (b3Vec3){ 4.7f, height, 3.0f * along }
									 : (b3Vec3){ -4.7f, height, 3.0f * along };
			impact.point = b3Add( house, local );

			nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
			impactTime += result.totalTime;
			grenades += 1;

			b3ExplosionDef explosion = b3DefaultExplosionDef();
			explosion.position = impact.point;
			explosion.radius = impact.radius;
			explosion.falloff = impact.radius;
			explosion.impulsePerArea = 40.0f * impact.ejectSpeed;
			b3World_Explode( scene.physicsWorld, &explosion );
		}

		ticks = b3GetTicks();
		b3World_Step( scene.physicsWorld, 1.0f / 60.0f, 4 );
		float stepTime = b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		nbWorld_Update( scene.world, 1.0f / 60.0f );
		float updateTime = b3GetMilliseconds( ticks );

		impactTimes[frame] = impactTime;
		updateTimes[frame] = updateTime;
		stepTimes[frame] = stepTime;
		frameTimes[frame] = impactTime + stepTime + updateTime;

		b3Counters counters = b3World_GetCounters( scene.physicsWorld );
		b3Profile profile = b3World_GetProfile( scene.physicsWorld );
		physicsProfile[1] += profile.collide;
		physicsProfile[2] += profile.solve;
		physicsProfile[6] += (float)counters.awakeContactCount;
		maxBodies = b3MaxInt( maxBodies, counters.bodyCount );
		maxAwake = b3MaxInt( maxAwake, b3World_GetAwakeBodyCount( scene.physicsWorld ) );
		maxContacts = b3MaxInt( maxContacts, counters.contactCount );
	}

	int slowFrames = 0;
	for ( int i = 0; i < frameCount; ++i )
	{
		slowFrames += frameTimes[i] > 1000.0f / 60.0f ? 1 : 0;
	}

	float* series[4] = { frameTimes, stepTimes, updateTimes, impactTimes };
	const char* names[4] = { "frame", "physics step", "update", "impacts" };
	for ( int k = 0; k < 4; ++k )
	{
		float total = 0.0f;
		for ( int i = 0; i < frameCount; ++i )
		{
			total += series[k][i];
		}
		qsort( series[k], frameCount, sizeof( float ), CompareFloats );
		printf( "  %-13s avg %6.2f ms  p95 %6.2f ms  max %6.2f ms\n", names[k], total / (float)frameCount,
				series[k][frameCount * 95 / 100], series[k][frameCount - 1] );
	}

	printf( "  99 %% of the frames up to %.2f ms, %d frames over 16.7 ms\n", frameTimes[frameCount * 99 / 100], slowFrames );
	printf( "  Box3D per step: collide %.2f ms, solve %.2f ms, %.0f awake contacts\n", physicsProfile[1] / frameCount,
			physicsProfile[2] / frameCount, physicsProfile[6] / frameCount );
	nbStats stats = nbWorld_GetStats( scene.world );
	printf( "  %d grenades, %d storeys collapsed, chunks %d, rubble %d, bodies at most %d (awake %d), contacts at most %d\n",
			grenades, stats.collapsedStoreyCount, stats.chunkCount, stats.rubbleCount, maxBodies, maxAwake, maxContacts );
	free( houses );
	DestroyScene( &scene );
}

// A house of the streamed city: its id while it is loaded, the last frame a moving body came near it, and when to ask
// again whether it can go
typedef struct CityHouse
{
	nbDestructibleId id;
	int activeFrame;
	int nextCheck;
	bool urgent;

	// The workers prepare it in the background
	bool loading;

	// Damaged and saved to the store, see SaveCityHouse, with how far its parts reach from its center along the ground
	bool saved;
	float reach;
	long saveOffset;
	size_t saveSize;
} CityHouse;

typedef struct CityCandidate
{
	float distance;
	int index;
} CityCandidate;

static int CompareCandidates( const void* a, const void* b )
{
	const CityCandidate* x = (const CityCandidate*)a;
	const CityCandidate* y = (const CityCandidate*)b;
	if ( x->distance != y->distance )
	{
		return x->distance < y->distance ? -1 : 1;
	}
	return ( x->index > y->index ) - ( x->index < y->index );
}

// Houses created together, with the definitions that must live until they are built in
typedef struct CityBatch
{
	House* houses;
	nbDestructibleDef* defs;
	const nbPieceDef** lists;
	int* counts;
	int* indices;
	nbDestructibleId* ids;
	int capacity;
	int count;
	nbCreationId creation;
} CityBatch;

static void CreateCityBatch( CityBatch* batch, int capacity )
{
	*batch = (CityBatch){ 0 };
	batch->houses = malloc( sizeof( House ) * (size_t)capacity );
	batch->defs = malloc( sizeof( nbDestructibleDef ) * (size_t)capacity );
	batch->lists = malloc( sizeof( nbPieceDef* ) * (size_t)capacity );
	batch->counts = malloc( sizeof( int ) * (size_t)capacity );
	batch->indices = malloc( sizeof( int ) * (size_t)capacity );
	batch->ids = malloc( sizeof( nbDestructibleId ) * (size_t)capacity );
	batch->capacity = capacity;
}

static void DestroyCityBatch( CityBatch* batch )
{
	free( batch->houses );
	free( batch->defs );
	free( batch->lists );
	free( batch->counts );
	free( batch->indices );
	free( batch->ids );
}

// Frames from the start of a creation in the background to the frame that builds it in. The workers have that long to
// prepare it, in a game a frame of 16 ms is plenty, the benchmark runs its frames back to back.
#define CityLoadDelay 4

// A city of houses on a grid of 14 x 12 m, streamed around a camera: the houses near the camera and near anything that
// moves are loaded, the others only exist as a seed and a position
typedef struct City
{
	Scene scene;
	nbMaterial concrete;
	int side;
	CityHouse* houses;
	int* loaded;
	int loadedCount;
	int* urgent;
	int urgentCount;
	CityCandidate* candidates;
	CityBatch first;
	CityBatch batches[CityLoadDelay];
	int frame;
	int loads;
	int urgentLoads;
	int unloads;
	int maxLoaded;

	// Damaged houses at rest go to a file and come back from it, see SaveCityHouse. The farthest a saved house reaches.
	FILE* store;
	long storeBytes;
	uint8_t* buffer;
	size_t bufferSize;
	float maxReach;
	int saves;
	int restores;
	double saveTime;
	double restoreTime;
} City;

static b3Vec3 CityHousePosition( const City* city, int index )
{
	int i = index % city->side, j = index / city->side;
	float half = 0.5f * (float)( city->side - 1 );
	return (b3Vec3){ 14.0f * ( (float)i - half ), 0.0f, 12.0f * ( (float)j - half ) };
}

static float CityDistance( const City* city, int index, b3Vec3 point )
{
	b3Vec3 p = CityHousePosition( city, index );
	return sqrtf( ( p.x - point.x ) * ( p.x - point.x ) + ( p.z - point.z ) * ( p.z - point.z ) );
}

// The grid cells whose houses may lie within the radius of a point
static void CityRange( const City* city, b3Vec3 point, float radius, int* i0, int* i1, int* j0, int* j1 )
{
	float half = 0.5f * (float)( city->side - 1 );
	*i0 = b3MaxInt( 0, (int)floorf( ( point.x - radius ) / 14.0f + half ) );
	*i1 = b3MinInt( city->side - 1, (int)ceilf( ( point.x + radius ) / 14.0f + half ) );
	*j0 = b3MaxInt( 0, (int)floorf( ( point.z - radius ) / 12.0f + half ) );
	*j1 = b3MinInt( city->side - 1, (int)ceilf( ( point.z + radius ) / 12.0f + half ) );
}

// Every body that moved in the last step loads the houses within the load radius that are not loaded yet, before it can
// reach them, and keeps the ones within the hold radius from going. A house reaches 5.5 m from its center and 6.5 m up, a
// saved one as far as its rubble lay.
static void MarkCityActivity( City* city, float loadRadius, float holdRadius, int frame )
{
	b3BodyEvents events = b3World_GetBodyEvents( city->scene.physicsWorld );
	for ( int e = 0; e < events.moveCount; ++e )
	{
		// The chunks of a body lie in the frame of their destructible, its origin can be meters away
		b3Vec3 point = b3ToVec3( b3Body_GetWorldCenter( events.moveEvents[e].bodyId ) );
		if ( point.y > 6.5f + holdRadius )
		{
			continue;
		}

		int i0, i1, j0, j1;
		CityRange( city, point, holdRadius + city->maxReach, &i0, &i1, &j0, &j1 );
		for ( int j = j0; j <= j1; ++j )
		{
			for ( int i = i0; i <= i1; ++i )
			{
				int index = j * city->side + i;
				CityHouse* house = city->houses + index;
				float distance = CityDistance( city, index, point );
				float reach = house->saved ? house->reach : 5.5f;
				if ( distance > holdRadius + reach )
				{
					continue;
				}

				house->activeFrame = frame;
				if ( distance <= loadRadius + reach && NB_IS_NULL( house->id ) && house->urgent == false &&
					 house->loading == false )
				{
					house->urgent = true;
					city->urgent[city->urgentCount++] = index;
				}
			}
		}
	}
}

// The scratch buffer of the store, at least so large
static uint8_t* CityBuffer( City* city, size_t size )
{
	if ( city->bufferSize < size )
	{
		city->bufferSize = size + size / 2;
		city->buffer = realloc( city->buffer, city->bufferSize );
	}
	return city->buffer;
}

// How far the parts of a house reach from its center along the ground, its rubble included
static float CityReach( const City* city, int index, nbDestructibleId id )
{
	int count = nbDestructible_GetChunkCount( id );
	nbChunkId* chunks = malloc( sizeof( nbChunkId ) * (size_t)( count + 1 ) );
	count = nbDestructible_GetChunks( id, chunks, count );
	b3Vec3 center = CityHousePosition( city, index );
	float reach = 5.5f;
	for ( int i = 0; i < count; ++i )
	{
		b3AABB box = b3Shape_GetAABB( nbChunk_GetShape( chunks[i] ) );
		float dx = b3MaxFloat( fabsf( box.lowerBound.x - center.x ), fabsf( box.upperBound.x - center.x ) );
		float dz = b3MaxFloat( fabsf( box.lowerBound.z - center.z ), fabsf( box.upperBound.z - center.z ) );
		reach = b3MaxFloat( reach, sqrtf( dx * dx + dz * dz ) );
	}
	free( chunks );
	return reach;
}

// Save a damaged house at rest to the end of the store and take it out of the world
static void SaveCityHouse( City* city, int index )
{
	uint64_t ticks = b3GetTicks();
	CityHouse* house = city->houses + index;

	// Into the buffer of the store if it is large enough, the call tells how large it has to be
	size_t size = nbSaveDestructibles( &house->id, 1, city->buffer, city->bufferSize );
	if ( size > city->bufferSize )
	{
		CityBuffer( city, size );
		size = nbSaveDestructibles( &house->id, 1, city->buffer, city->bufferSize );
	}
	house->reach = CityReach( city, index, house->id );
	house->saveOffset = city->storeBytes;
	house->saveSize = size;
	fseek( city->store, city->storeBytes, SEEK_SET );
	fwrite( city->buffer, 1, size, city->store );
	city->storeBytes += (long)size;
	city->maxReach = b3MaxFloat( city->maxReach, house->reach );
	nbDestroyDestructible( house->id );
	house->id = nb_nullDestructibleId;
	house->saved = true;
	city->saves += 1;
	city->saveTime += b3GetMilliseconds( ticks );
}

// Bring a saved house back from the store
static void RestoreCityHouse( City* city, int index )
{
	uint64_t ticks = b3GetTicks();
	CityHouse* house = city->houses + index;
	uint8_t* buffer = CityBuffer( city, house->saveSize );
	fseek( city->store, house->saveOffset, SEEK_SET );
	size_t read = fread( buffer, 1, house->saveSize, city->store );
	if ( read != house->saveSize || nbLoadDestructibles( city->scene.world, buffer, house->saveSize, &house->id ) == false )
	{
		printf( "  could not bring house %d back\n", index );
		exit( 1 );
	}
	house->saved = false;
	city->loaded[city->loadedCount++] = index;
	city->maxLoaded = b3MaxInt( city->maxLoaded, city->loadedCount );
	city->restores += 1;
	city->restoreTime += b3GetMilliseconds( ticks );
}

// Pick the houses that moving bodies came near, then the ones within the radius of the camera, nearest first, at most as
// many as fit into the batch. A saved house comes back from the store right away, the others go into the batch.
static void PickCityHouses( City* city, CityBatch* batch, b3Vec3 camera, float radius )
{
	// The urgent ones keep their mark until the camera's are collected, so none comes twice
	int count = 0;
	for ( int k = 0; k < city->urgentCount; ++k )
	{
		city->candidates[count++] = (CityCandidate){ -1.0f, city->urgent[k] };
	}

	int i0, i1, j0, j1;
	CityRange( city, camera, radius, &i0, &i1, &j0, &j1 );
	for ( int j = j0; j <= j1; ++j )
	{
		for ( int i = i0; i <= i1; ++i )
		{
			int index = j * city->side + i;
			const CityHouse* house = city->houses + index;
			float distance = CityDistance( city, index, camera );
			if ( distance <= radius && NB_IS_NULL( house->id ) && house->urgent == false && house->loading == false )
			{
				city->candidates[count++] = (CityCandidate){ distance, index };
			}
		}
	}

	for ( int k = 0; k < city->urgentCount; ++k )
	{
		city->houses[city->urgent[k]].urgent = false;
	}
	city->urgentCount = 0;

	// The urgent ones that do not fit come back with the next step that moves something near them
	qsort( city->candidates, (size_t)count, sizeof( CityCandidate ), CompareCandidates );
	batch->count = 0;
	for ( int k = 0; k < count && k < batch->capacity; ++k )
	{
		int index = city->candidates[k].index;
		city->urgentLoads += city->candidates[k].distance < 0.0f ? 1 : 0;
		if ( city->houses[index].saved )
		{
			RestoreCityHouse( city, index );
			continue;
		}

		int slot = batch->count++;
		BuildHouse( batch->houses + slot, CityHousePosition( city, index ), (uint32_t)( 100 + index ), &city->concrete );
		batch->defs[slot] = batch->houses[slot].def;
		batch->lists[slot] = batch->houses[slot].pieces;
		batch->counts[slot] = batch->houses[slot].pieceCount;
		batch->indices[slot] = index;
		city->houses[index].loading = true;
	}
}

// The houses of a batch got their ids
static void AddCityHouses( City* city, CityBatch* batch )
{
	for ( int k = 0; k < batch->count; ++k )
	{
		CityHouse* house = city->houses + batch->indices[k];
		house->id = batch->ids[k];
		house->loading = false;
		city->loaded[city->loadedCount++] = batch->indices[k];
	}
	city->loads += batch->count;
	city->maxLoaded = b3MaxInt( city->maxLoaded, city->loadedCount );
	batch->count = 0;
}

// Build in the houses the workers started to prepare CityLoadDelay frames ago
static void FinishLoadingCity( City* city )
{
	CityBatch* batch = city->batches + city->frame % CityLoadDelay;
	if ( batch->count > 0 )
	{
		nbFinishCreating( batch->creation, batch->ids );
		AddCityHouses( city, batch );
	}
}

// Start preparing the next houses in the background, after the physics step, so the workers do not take the cores from
// the threads of Box3D while it steps
static void StartLoadingCity( City* city, b3Vec3 camera, float radius )
{
	CityBatch* batch = city->batches + city->frame % CityLoadDelay;
	city->frame += 1;
	PickCityHouses( city, batch, camera, radius );
	batch->creation = nbStartCreating( city->scene.world, batch->defs, batch->lists, batch->counts, batch->count );
}

// Take the houses beyond the radius of the camera out that no moving body came near for five seconds and that can go, at
// most so many, and of those the damaged ones at most saveBudget, which go to the store. A house that cannot go is asked
// again half a second later.
static void UnloadCity( City* city, b3Vec3 camera, float radius, int budget, int saveBudget, int frame )
{
	int count = 0;
	int saveCount = 0;
	for ( int k = 0; k < city->loadedCount && count < budget; )
	{
		int index = city->loaded[k];
		CityHouse* house = city->houses + index;
		if ( frame < house->nextCheck || frame < house->activeFrame + 300 || CityDistance( city, index, camera ) <= radius )
		{
			k += 1;
			continue;
		}

		bool intact = nbDestructible_IsIntact( house->id );
		if ( intact == false && saveCount == saveBudget )
		{
			k += 1;
			continue;
		}

		if ( intact ? nbDestructible_CanUnload( house->id, 2.0f ) == false
					: nbCanSaveDestructibles( &house->id, 1, 2.0f ) == false )
		{
			house->nextCheck = frame + 30;
			k += 1;
			continue;
		}

		if ( intact )
		{
			nbDestroyDestructible( house->id );
			house->id = nb_nullDestructibleId;
			city->unloads += 1;
		}
		else
		{
			SaveCityHouse( city, index );
			saveCount += 1;
		}
		city->loaded[k] = city->loaded[--city->loadedCount];
		count += 1;
	}
}

// A city of 256 x 256 houses, far too large to hold at once, streamed around a camera that flies 900 m through it at
// 30 m/s and 300 m back. The houses within 120 m of the camera are loaded, nearest first and at most two per frame, and
// so are the houses within 10 m of any body that moves. The workers prepare them in the background, and CityLoadDelay
// frames later they are built in. The ones beyond 140 m go as soon as no body moved within 20 m of them for five seconds:
// an intact house once nbDestructible_CanUnload lets it, a damaged one once nbCanSaveDestructibles lets it, saved with its
// rubble to a file, at most one per frame, and brought back from there when it is needed again. A grenade falls every
// fifth frame on a house within 60 m of the camera.
static void BenchmarkCity( int workerCount )
{
	enum
	{
		side = 256,
		outFrames = 1800,
		frameCount = 2400,
		loadBudget = 2,
		unloadBudget = 4,
		saveBudget = 1,
	};
	const float loadRadius = 120.0f, unloadRadius = 140.0f, fireRadius = 60.0f, speed = 30.0f;
	const float activeLoadRadius = 10.0f, activeHoldRadius = 20.0f;

	City city = { 0 };
	city.side = side;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	city.scene.physicsWorld = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
	b3BodyId groundId = b3CreateBody( city.scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 7.0f * (float)side + 50.0f, 1.0f, 6.0f * (float)side + 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( groundId, &shapeDef, &box.base );
	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = city.scene.physicsWorld;
	def.workerCount = workerCount;
	def.fragmentScale = 4.0f;
	city.scene.world = nbCreateWorld( &def );

	city.concrete = nbDefaultMaterial();
	city.concrete.strength = 1.1e6f;
	city.concrete.fragmentSize = 0.13f;
	city.store = tmpfile();
	city.maxReach = 5.5f;
	int houseCount = side * side;
	city.houses = calloc( (size_t)houseCount, sizeof( CityHouse ) );
	city.loaded = malloc( sizeof( int ) * (size_t)houseCount );
	city.urgent = malloc( sizeof( int ) * (size_t)houseCount );
	city.candidates = malloc( sizeof( CityCandidate ) * (size_t)houseCount );
	for ( int h = 0; h < houseCount; ++h )
	{
		city.houses[h].activeFrame = -1000;
	}

	// Room for everything around the start at once, then for the houses of one frame
	int startRoom = ( (int)( 2.0f * loadRadius / 12.0f ) + 3 ) * ( (int)( 2.0f * loadRadius / 12.0f ) + 3 );
	CreateCityBatch( &city.first, startRoom );
	for ( int k = 0; k < CityLoadDelay; ++k )
	{
		CreateCityBatch( city.batches + k, loadBudget );
	}

	// Everything around the start at once, then in the background as the camera flies
	b3Vec3 camera = { -450.0f, 0.0f, 6.0f };
	int64_t baseBytes = nbGetByteCount() + b3GetByteCount();
	uint64_t ticks = b3GetTicks();
	PickCityHouses( &city, &city.first, camera, loadRadius );
	nbCreateDestructibles( city.scene.world, city.first.defs, city.first.lists, city.first.counts, city.first.count,
						   city.first.ids );
	AddCityHouses( &city, &city.first );
	float startTime = b3GetMilliseconds( ticks );
	int startCount = city.loadedCount;
	int64_t houseBytes = ( nbGetByteCount() + b3GetByteCount() - baseBytes ) / b3MaxInt( startCount, 1 );
	int64_t maxBytes = 0;

	static float streamTimes[frameCount], impactTimes[frameCount], stepTimes[frameCount], updateTimes[frameCount],
		frameTimes[frameCount];
	nbRandom rng = nbMakeRandom( 11, 0 );
	int missed = 0;
	nbImpactDef impact = { 0 };
	impact.radius = 1.3f;
	impact.damage = 3.0e5f;
	impact.ejectSpeed = 12.0f;
	for ( int frame = 0; frame < frameCount; ++frame )
	{
		camera.x += frame < outFrames ? speed / 60.0f : -speed / 60.0f;
		ticks = b3GetTicks();
		UnloadCity( &city, camera, unloadRadius, unloadBudget, saveBudget, frame );
		FinishLoadingCity( &city );
		float streamTime = b3GetMilliseconds( ticks );

		float impactTime = 0.0f;
		if ( frame % 5 == 0 )
		{
			// A wall of a random house near the camera, on either story
			float angle = nbRandomRange( &rng, 0.0f, 2.0f * B3_PI );
			float reach = fireRadius * sqrtf( nbRandomRange( &rng, 0.0f, 1.0f ) );
			float half = 0.5f * (float)( side - 1 );
			int i = (int)floorf( ( camera.x + reach * cosf( angle ) ) / 14.0f + half + 0.5f );
			int j = (int)floorf( ( camera.z + reach * sinf( angle ) ) / 12.0f + half + 0.5f );
			int wall = (int)nbRandomRange( &rng, 0.0f, 3.999f );
			float along = nbRandomRange( &rng, -1.0f, 1.0f );
			float height = nbRandomRange( &rng, 0.0f, 1.0f ) < 0.5f ? 1.2f : 4.4f;
			if ( 0 <= i && i < side && 0 <= j && j < side )
			{
				missed += NB_IS_NULL( city.houses[j * side + i].id ) ? 1 : 0;
				b3Vec3 local = wall == 0 ? (b3Vec3){ 4.2f * along, height, 3.4f }
							 : wall == 1 ? (b3Vec3){ 4.2f * along, height, -3.4f }
							 : wall == 2 ? (b3Vec3){ 4.7f, height, 3.0f * along }
										 : (b3Vec3){ -4.7f, height, 3.0f * along };
				impact.point = b3Add( CityHousePosition( &city, j * side + i ), local );
				impactTime = nbWorld_ApplyImpact( city.scene.world, &impact ).totalTime;

				b3ExplosionDef explosion = b3DefaultExplosionDef();
				explosion.position = impact.point;
				explosion.radius = impact.radius;
				explosion.falloff = impact.radius;
				explosion.impulsePerArea = 40.0f * impact.ejectSpeed;
				b3World_Explode( city.scene.physicsWorld, &explosion );
			}
		}

		ticks = b3GetTicks();
		b3World_Step( city.scene.physicsWorld, 1.0f / 60.0f, 4 );
		float stepTime = b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		MarkCityActivity( &city, activeLoadRadius, activeHoldRadius, frame );
		streamTime += b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		nbWorld_Update( city.scene.world, 1.0f / 60.0f );
		float updateTime = b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		StartLoadingCity( &city, camera, loadRadius );
		streamTime += b3GetMilliseconds( ticks );

		streamTimes[frame] = streamTime;
		impactTimes[frame] = impactTime;
		stepTimes[frame] = stepTime;
		updateTimes[frame] = updateTime;
		frameTimes[frame] = streamTime + impactTime + stepTime + updateTime;
		int64_t bytes = nbGetByteCount() + b3GetByteCount();
		maxBytes = bytes > maxBytes ? bytes : maxBytes;
	}

	int damaged = 0, saved = 0;
	for ( int k = 0; k < city.loadedCount; ++k )
	{
		damaged += nbDestructible_IsIntact( city.houses[city.loaded[k]].id ) ? 0 : 1;
	}
	for ( int h = 0; h < houseCount; ++h )
	{
		saved += city.houses[h].saved ? 1 : 0;
	}

	printf( "  %d houses on %.1f x %.1f km, %d of them loaded at first in %.0f ms, %.2f MB each, all would take %.1f GB\n",
			houseCount, 14.0f * (float)side / 1000.0f, 12.0f * (float)side / 1000.0f, startCount, startTime,
			(double)houseBytes / 1048576.0, (double)houseBytes * (double)houseCount / 1073741824.0 );
	printf( "  then %d houses loaded, %d of them for moving bodies, and %d unloaded in %d frames, at most %d at once, %d "
			"grenades on houses not loaded\n",
			city.loads - startCount, city.urgentLoads, city.unloads, frameCount, city.maxLoaded, missed );
	printf( "  %d damaged houses saved, %d brought back, %d in the store and %d loaded at the end, a save %.2f ms and %.0f KB, "
			"bringing one back %.2f ms\n",
			city.saves, city.restores, saved, damaged, city.saveTime / b3MaxInt( city.saves, 1 ),
			(double)city.storeBytes / 1024.0 / b3MaxInt( city.saves, 1 ), city.restoreTime / b3MaxInt( city.restores, 1 ) );

	float* series[5] = { frameTimes, streamTimes, stepTimes, updateTimes, impactTimes };
	const char* names[5] = { "frame", "streaming", "physics step", "update", "impacts" };
	for ( int k = 0; k < 5; ++k )
	{
		float total = 0.0f;
		for ( int i = 0; i < frameCount; ++i )
		{
			total += series[k][i];
		}
		qsort( series[k], frameCount, sizeof( float ), CompareFloats );
		printf( "  %-13s avg %6.2f ms  p95 %6.2f ms  max %6.2f ms\n", names[k], total / (float)frameCount,
				series[k][frameCount * 95 / 100], series[k][frameCount - 1] );
	}

	nbStats stats = nbWorld_GetStats( city.scene.world );
	printf( "  memory at most %.0f MB, at the end %d chunks, %d rubble, %.0f MB written to the store\n",
			(double)maxBytes / 1048576.0, stats.chunkCount, stats.rubbleCount, (double)city.storeBytes / 1048576.0 );

	// The houses still in the background go with the world
	DestroyScene( &city.scene );
	DestroyCityBatch( &city.first );
	for ( int k = 0; k < CityLoadDelay; ++k )
	{
		DestroyCityBatch( city.batches + k );
	}
	free( city.houses );
	free( city.loaded );
	free( city.urgent );
	free( city.candidates );
	free( city.buffer );
	fclose( city.store );
}

int main( int argc, char** argv )
{
	int workerCount = argc > 1 ? atoi( argv[1] ) : 1;
	workerCount = workerCount < 1 ? 1 : workerCount;

	nbVersion version = nbGetVersion();
	b3Version b3version = b3GetVersion();
	printf( "Nebenan %d.%d.%d on Box3D %d.%d.%d, workers for fracture and physics: %d\n", version.major, version.minor,
			version.revision, b3version.major, b3version.minor, b3version.revision, workerCount );

	BenchmarkKernel();

	printf( "\nImpact pipeline (fracture + support graph + Box3D bodies), physics at 60 Hz with 4 substeps\n" );
	BenchmarkRifle( workerCount );
	BenchmarkExplosions( workerCount );
	BenchmarkBuilding( workerCount );

	// Larger fragments are the main lever of the cost of mass destruction
	printf( "\nTown under fire (every step: grenades, Box3D step, destruction update)\n" );
	BenchmarkTown( workerCount, 1.0f, 4, 1, 5 );
	BenchmarkTown( workerCount, 2.0f, 4, 1, 5 );

	// Fragments four times as large keep the cost of a grenade down, so a block four times the size takes four every step
	printf( "\nBlock of 64 houses under a barrage, fragment scale 4.0 (four grenades every step, Box3D step, update)\n" );
	BenchmarkTown( workerCount, 4.0f, 8, 4, 1 );

	printf( "\nCity streamed around a camera flying through it, fragment scale 4.0 (streaming, grenades, Box3D step, update)\n" );
	BenchmarkCity( workerCount );
	return 0;
}
