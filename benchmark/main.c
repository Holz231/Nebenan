// SPDX-License-Identifier: MIT

// Headless benchmark for the fracture kernel and the full impact pipeline.
// Usage: nebenan_benchmark [workerCount], the worker count applies to the fracture and to Box3D

#include "fracture.h"
#include "hull_builder.h"

#include "nebenan/nebenan.h"

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

// Two stories of brick walls through both floors with doors and windows, pre-fractured into cells, and concrete
// floors inside them, like the house of the demo
static nbDestructibleId CreateHouse( nbWorldId world, b3Vec3 position, uint32_t seed, const nbMaterial* concrete )
{
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;
	float level = story + slab;
	int floors = 2;
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
	return nbCreateDestructible( world, &def, pieces, count );
}

static int CompareFloats( const void* a, const void* b )
{
	float x = *(const float*)a, y = *(const float*)b;
	return x < y ? -1 : ( x > y ? 1 : 0 );
}

// A town of 16 houses wrecked by a grenade every five frames, twelve per second, like holding the fire button
static void BenchmarkTown( int workerCount, float fragmentScale )
{
	Scene scene = CreateScene( workerCount );
	nbWorld_SetFragmentScale( scene.world, fragmentScale );
	nbMaterial concrete = nbDefaultMaterial();
	concrete.strength = 1.1e6f;
	concrete.fragmentSize = 0.13f;

	int houseCount = 0;
	b3Vec3 houses[16];
	uint64_t ticks = b3GetTicks();
	for ( int i = 0; i < 4; ++i )
	{
		for ( int j = 0; j < 4; ++j )
		{
			houses[houseCount] = (b3Vec3){ 14.0f * ( (float)i - 1.5f ), 0.0f, 12.0f * ( (float)j - 1.5f ) };
			CreateHouse( scene.world, houses[houseCount], (uint32_t)( 100 + houseCount ), &concrete );
			houseCount += 1;
		}
	}
	float createTime = b3GetMilliseconds( ticks );
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
	int maxBodies = 0, maxAwake = 0, maxContacts = 0;
	float physicsProfile[8] = { 0 };

	for ( int frame = 0; frame < frameCount; ++frame )
	{
		float impactTime = 0.0f;
		if ( frame % 5 == 0 )
		{
			// A wall of a random house, on either story
			b3Vec3 house = houses[(int)( nbRandomRange( &rng, 0.0f, (float)houseCount - 0.001f ) )];
			int side = (int)nbRandomRange( &rng, 0.0f, 3.999f );
			float along = nbRandomRange( &rng, -1.0f, 1.0f );
			float height = nbRandomRange( &rng, 0.0f, 1.0f ) < 0.5f ? 1.2f : 4.4f;
			b3Vec3 local = side == 0 ? (b3Vec3){ 4.2f * along, height, 3.4f }
						 : side == 1 ? (b3Vec3){ 4.2f * along, height, -3.4f }
						 : side == 2 ? (b3Vec3){ 4.7f, height, 3.0f * along }
									 : (b3Vec3){ -4.7f, height, 3.0f * along };
			impact.point = b3Add( house, local );

			nbImpactResult result = nbWorld_ApplyImpact( scene.world, &impact );
			impactTime = result.totalTime;

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

	printf( "  Box3D per step: collide %.2f ms, solve %.2f ms, %.0f awake contacts\n", physicsProfile[1] / frameCount,
			physicsProfile[2] / frameCount, physicsProfile[6] / frameCount );
	nbStats stats = nbWorld_GetStats( scene.world );
	printf( "  %d grenades, %d storeys collapsed, chunks %d, rubble %d, bodies at most %d (awake %d), contacts at most %d\n",
			frameCount / 5, stats.collapsedStoreyCount, stats.chunkCount, stats.rubbleCount, maxBodies, maxAwake, maxContacts );
	DestroyScene( &scene );
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
	BenchmarkTown( workerCount, 1.0f );
	BenchmarkTown( workerCount, 2.0f );
	return 0;
}
