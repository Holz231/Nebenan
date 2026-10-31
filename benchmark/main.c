// SPDX-License-Identifier: MIT

// Headless benchmark for the fracture kernel and the full impact pipeline.
// Usage: nebenan_benchmark [workerCount], the worker count applies to the fracture and to Box3D
//        nebenan_benchmark scaling [rounds], the towns under fire with 1, 2, 4 and so on workers, see
//        nbGetScalingWorkerCounts, and the scaling report of the library for each town
//        nebenan_benchmark figures [workerCount] [rounds] [off], the figures alone, see BenchmarkFigures, as ragdolls in
//        a heap of debris, see BenchmarkFigurePile, and in a town under fire, see BenchmarkTown, with off the joints keep
//        the barrier per stage, see b3WorldDef::enableJointPacks

#include "fracture.h"
#include "hull_builder.h"

#include "nebenan/nebenan.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Scene
{
	b3WorldId physicsWorld;
	nbWorldId world;
} Scene;

// Whether the worlds of the benchmark solve joints in the packs of islands, see b3WorldDef::enableJointPacks
static bool s_jointPacks = true;

static Scene CreateScene( int workerCount )
{
	Scene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	worldDef.enableJointPacks = s_jointPacks;
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

// A hash of every bone of the figures, the same with any number of workers
static uint32_t HashFigures( const nbFigureId* figures, int count )
{
	uint32_t hash = 2166136261u;
	for ( int k = 0; k < count; ++k )
	{
		b3WorldTransform transforms[nb_humanoidBoneCount];
		int boneCount = nbFigure_GetBoneTransforms( figures[k], transforms, nb_humanoidBoneCount );
		const uint8_t* bytes = (const uint8_t*)transforms;
		for ( size_t b = 0; b < sizeof( b3WorldTransform ) * (size_t)boneCount; ++b )
		{
			hash = ( hash ^ bytes[b] ) * 16777619u;
		}
	}
	return hash;
}

// What a town cost over its simulated time, see BenchmarkRates
typedef struct TownCost
{
	float step, update, impacts;
	int steps, chunks, rubble, collapsed;
} TownCost;

// A town of 16 houses wrecked by a grenade every five frames, twelve per second, like holding the fire button
// A block of side x side houses under fire for 20 simulated seconds at stepRate steps per second: every interval
// sixtieths of a second as many grenades, each on a wall of a random house, at the same times at every rate. The profile
// of the library over the frames under fire goes to profile and the cost to cost if they are not null. With figures two
// stand in the streets beside every house, see BenchmarkFigures, one a metre before its east wall and one before its
// south wall, where the grenades, their debris and the storeys that come down knock them over and bury them.
static void BenchmarkTown( int workerCount, float fragmentScale, int side, int grenadesPerVolley, int interval,
						   nbProfile* profile, int stepRate, TownCost* cost, bool figures )
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

	// The figures in the streets, facing their walls or away from them
	int figureCount = figures ? 2 * houseCount : 0;
	nbFigureId* figureIds = figureCount > 0 ? malloc( sizeof( nbFigureId ) * (size_t)figureCount ) : NULL;
	nbBoneDef bones[nb_humanoidBoneCount];
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbFigureDef figureDef = nbDefaultFigureDef();
	figureDef.rig = &rig;
	figureDef.standHeight = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	for ( int k = 0; k < figureCount; ++k )
	{
		b3Vec3 house = houses[k / 2];
		b3Vec3 local = k % 2 == 0 ? (b3Vec3){ 5.7f, 0.0f, 1.5f } : (b3Vec3){ -1.5f, 0.0f, -4.4f };
		figureDef.position = b3ToPos( b3Add( house, local ) );
		figureDef.heading = 0.5f * B3_PI * (float)( k % 4 );
		figureIds[k] = nbCreateFigure( scene.world, &figureDef );
	}

	enum
	{
		sixtiethCount = 1200
	};

	int frameCount = sixtiethCount * stepRate / 60;
	float timeStep = 1.0f / (float)stepRate;
	int subStepCount = nbGetSubStepCount( (float)stepRate );
	float* impactTimes = malloc( sizeof( float ) * (size_t)frameCount );
	float* updateTimes = malloc( sizeof( float ) * (size_t)frameCount );
	float* stepTimes = malloc( sizeof( float ) * (size_t)frameCount );
	float* frameTimes = malloc( sizeof( float ) * (size_t)frameCount );
	int sixtieth = 0;
	nbRandom rng = nbMakeRandom( 9, 0 );
	nbImpactDef impact = { 0 };
	impact.radius = 1.3f;
	impact.damage = 3.0e5f;
	impact.ejectSpeed = 12.0f;
	impact.explosionImpulse = 40.0f * impact.ejectSpeed;
	int maxBodies = 0, maxAwake = 0, maxContacts = 0, grenades = 0;
	float physicsProfile[8] = { 0 };
	nbWorld_ResetProfile( scene.world );

	for ( int frame = 0; frame < frameCount; ++frame )
	{
		// The volleys of the sixtieths of a second up to the time of this step
		float impactTime = 0.0f;
		int sixtiethEnd = frame * 60 / stepRate + 1;
		for ( ; sixtieth < sixtiethEnd; ++sixtieth )
		{
			for ( int g = 0; g < ( sixtieth % interval == 0 ? grenadesPerVolley : 0 ); ++g )
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
			}
		}

		ticks = b3GetTicks();
		b3World_Step( scene.physicsWorld, timeStep, subStepCount );
		float stepTime = b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		nbWorld_Update( scene.world, timeStep );
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
	float totals[4] = { 0 };
	for ( int k = 0; k < 4; ++k )
	{
		float total = 0.0f;
		for ( int i = 0; i < frameCount; ++i )
		{
			total += series[k][i];
		}
		totals[k] = total;
		qsort( series[k], frameCount, sizeof( float ), CompareFloats );
		printf( "  %-13s avg %6.2f ms  p95 %6.2f ms  max %6.2f ms\n", names[k], total / (float)frameCount,
				series[k][frameCount * 95 / 100], series[k][frameCount - 1] );
	}

	printf( "  99 %% of the frames up to %.2f ms, %d frames over 16.7 ms\n", frameTimes[frameCount * 99 / 100], slowFrames );
	nbProfile townProfile = nbWorld_GetProfile( scene.world );
	printf( "  Box3D per step: collide %.2f ms, solve %.2f ms, %.0f awake contacts, packs in %d and domains in %d of %d steps\n",
			physicsProfile[1] / frameCount, physicsProfile[2] / frameCount, physicsProfile[6] / frameCount,
			townProfile.packStepCount, townProfile.domainStepCount, townProfile.stepCount );
	nbStats stats = nbWorld_GetStats( scene.world );
	printf( "  %d grenades, %d storeys collapsed, chunks %d, rubble %d, bodies at most %d (awake %d), contacts at most %d\n",
			grenades, stats.collapsedStoreyCount, stats.chunkCount, stats.rubbleCount, maxBodies, maxAwake, maxContacts );
	if ( figureCount > 0 )
	{
		nbFigureStats figureStats = nbWorld_GetFigureStats( scene.world );
		printf( "  %d figures: ragdolls awake after %d steps, at most %d at once, %d balance steps, %d caught, %d knockdowns, "
				"%d get-ups, %d moves to get up, %d aborts, at the end %d standing, %d catching themselves, %d lying, "
				"%d getting up, figures hash 0x%08x\n",
				figureCount, townProfile.ragdollStepCount, townProfile.maxRagdollCount, figureStats.stepCount,
				figureStats.catchCount, figureStats.knockdownCount, figureStats.getUpCount, figureStats.shiftCount,
				figureStats.abortCount, figureStats.animatedCount, figureStats.balancingCount, figureStats.ragdollCount,
				figureStats.gettingUpCount, HashFigures( figureIds, figureCount ) );
	}
	if ( profile != NULL )
	{
		*profile = townProfile;
	}
	if ( cost != NULL )
	{
		*cost = (TownCost){ totals[1], totals[2], totals[3], frameCount, stats.chunkCount, stats.rubbleCount,
							stats.collapsedStoreyCount };
	}
	free( impactTimes );
	free( updateTimes );
	free( stepTimes );
	free( frameTimes );
	free( houses );
	free( figureIds );
	DestroyScene( &scene );
}

// The towns of the scaling and the step rates: 16 houses with a grenade every 1/12 s at fragment scale 1 and 2, and 64
// houses with four every 1/60 s at fragment scale 4
typedef struct Town
{
	const char* name;
	float fragmentScale;
	int side, grenadesPerVolley, interval;
} Town;

static const Town s_towns[3] = {
	{ "16 houses, fragment scale 1, a grenade every 1/12 s", 1.0f, 4, 1, 5 },
	{ "16 houses, fragment scale 2, a grenade every 1/12 s", 2.0f, 4, 1, 5 },
	{ "64 houses, fragment scale 4, four grenades every 1/60 s", 4.0f, 8, 4, 1 },
};

// The towns of the benchmark with every worker count of nbGetScalingWorkerCounts, rounds times in turn, and the scaling
// report of the library for each
static void BenchmarkScaling( int rounds )
{
	int counts[8];
	int countCount = nbGetScalingWorkerCounts( counts, 8 );
	printf( "Scaling of the towns under fire over" );
	for ( int i = 0; i < countCount; ++i )
	{
		printf( "%s %d", i == 0 ? "" : ( i == countCount - 1 ? " and" : "," ), counts[i] );
	}
	printf( " workers, %d round%s\n", rounds, rounds == 1 ? "" : "s" );

	int profileCount = rounds * countCount;
	nbProfile* profiles = malloc( sizeof( nbProfile ) * (size_t)profileCount );
	for ( int t = 0; t < 3; ++t )
	{
		const Town* town = s_towns + t;
		printf( "\n%s\n", town->name );
		for ( int r = 0; r < rounds; ++r )
		{
			for ( int i = 0; i < countCount; ++i )
			{
				printf( " %d workers\n", counts[i] );
				BenchmarkTown( counts[i], town->fragmentScale, town->side, town->grenadesPerVolley, town->interval,
							   profiles + r * countCount + i, 60, NULL, false );
			}
		}

		int length = nbFormatScalingReport( profiles, profileCount, NULL, 0 );
		char* report = malloc( (size_t)length + 1 );
		nbFormatScalingReport( profiles, profileCount, report, length + 1 );
		printf( "\n%s", report );
		free( report );
	}
	free( profiles );
}

// The towns of the benchmark at 30, 60 and 120 steps per second over the same 20 simulated seconds, with the grenades at
// the same times, and what a simulated second costs at each rate, rounds times in turn
static void BenchmarkRates( int rounds )
{
	int workerCount = nbGetRecommendedWorkerCount( false );
	printf( "Towns under fire at 30, 60 and 120 steps per second, 240 substeps per second, %d workers, %d round%s\n",
			workerCount, rounds, rounds == 1 ? "" : "s" );

	const int rates[3] = { 30, 60, 120 };

	for ( int t = 0; t < 3; ++t )
	{
		const Town* town = s_towns + t;
		printf( "\n%s\n", town->name );
		TownCost sums[3] = { 0 };
		for ( int r = 0; r < rounds; ++r )
		{
			for ( int k = 0; k < 3; ++k )
			{
				printf( " %d Hz with %d substeps\n", rates[k], nbGetSubStepCount( (float)rates[k] ) );
				TownCost cost;
				BenchmarkTown( workerCount, town->fragmentScale, town->side, town->grenadesPerVolley, town->interval, NULL,
							   rates[k], &cost, false );
				sums[k].step += cost.step;
				sums[k].update += cost.update;
				sums[k].impacts += cost.impacts;
				sums[k].steps = cost.steps;
				sums[k].chunks = cost.chunks;
				sums[k].rubble = cost.rubble;
				sums[k].collapsed = cost.collapsed;
			}
		}

		printf( "\nms per simulated second   step  update  impacts   total | steps chunks rubble storeys\n" );
		for ( int k = 0; k < 3; ++k )
		{
			float scale = 1.0f / ( 20.0f * (float)rounds );
			const TownCost* c = sums + k;
			printf( "  %3d Hz                %7.1f %7.1f %8.1f %7.1f | %5d %6d %6d %7d\n", rates[k], scale * c->step,
					scale * c->update, scale * c->impacts, scale * ( c->step + c->update + c->impacts ), c->steps, c->chunks,
					c->rubble, c->collapsed );
		}
	}
}

// Figures after the hybrid of Overgrowth, see nbCreateFigure and the roadmap of the game, 26.8 and 27.14: humanoids in
// rows of up to ten, 2.5 m apart, each row 1.5 m behind a brick wall. They stand, flinch from hits every sixth of a
// second, are knocked down all at once, lie and get up, and then grenades on the other side break the walls, whose debris
// hits them. Per phase the Box3D step and the update in milliseconds per step, mean, p95, p99 and max, from the profile
// of the library the figures in the update and the solver and the contacts in the step, and what the figures did. At the
// end the memory and a hash of every bone, the same with any number of workers.
typedef struct FigurePhase
{
	const char* name;
	int steps;
} FigurePhase;

#define FIGURE_MAX 50
#define FIGURE_PHASE_STEPS 600

static void BenchmarkFigures( int workerCount, int figureCount )
{
	figureCount = b3ClampInt( figureCount, 1, FIGURE_MAX );
	Scene scene = CreateScene( workerCount );
	nbBoneDef bones[nb_humanoidBoneCount];
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &rig;
	def.standHeight = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );

	int columns = b3MinInt( figureCount, 10 );
	int rows = ( figureCount + columns - 1 ) / columns;
	float width = 2.5f * (float)columns;
	for ( int row = 0; row < rows; ++row )
	{
		nbDestructibleDef wall = nbDefaultDestructibleDef();
		wall.position = (b3Vec3){ 0.5f * width - 1.25f, 1.1f, 1.5f - 6.0f * (float)row };
		wall.seed = (uint32_t)( 40 + row );
		nbCreateBox( scene.world, &wall, (b3Vec3){ 0.5f * width, 1.1f, 0.15f } );
	}
	nbFigureId figures[FIGURE_MAX];
	for ( int k = 0; k < figureCount; ++k )
	{
		def.position = b3ToPos( (b3Vec3){ 2.5f * (float)( k % columns ), 0.0f, -6.0f * (float)( k / columns ) } );
		def.heading = 0.3f * (float)( k % 7 ) - 0.9f;
		figures[k] = nbCreateFigure( scene.world, &def );
	}

	const FigurePhase phases[5] = {
		{ "stand", 120 },
		{ "flinch", 120 },
		{ "knockdown", 90 },
		{ "get up", FIGURE_PHASE_STEPS },
		{ "cover breaks", FIGURE_PHASE_STEPS },
	};
	static float stepTimes[FIGURE_PHASE_STEPS], updateTimes[FIGURE_PHASE_STEPS];
	printf( "  %d figure%s in %d row%s, a wall before each\n", figureCount, figureCount == 1 ? "" : "s", rows,
			rows == 1 ? "" : "s" );
	printf( "    phase        steps | step avg    p95    p99    max | update avg    p95    p99    max | figures  solve collide | "
			"packs domains | awake ragdolls, balance steps, caught, knockdowns, get-ups, moves to get up, aborts\n" );
	uint32_t hitId = 1;
	for ( int p = 0; p < 5; ++p )
	{
		nbWorld_ResetProfile( scene.world );
		nbFigureStats before = nbWorld_GetFigureStats( scene.world );
		int awake = 0;
		int steps = 0;
		for ( int step = 0; step < phases[p].steps; ++step )
		{
			for ( int k = 0; k < figureCount; ++k )
			{
				// Flinch from 30 N s every tenth step, all knocked down by 120 N s at once, each from its own side
				float angle = 2.0f * B3_PI * (float)k / (float)figureCount + 0.7f * (float)( step / 10 );
				float strength = p == 1 && step % 10 == 0 ? 30.0f : ( p == 2 && step == 0 ? 120.0f : 0.0f );
				if ( strength > 0.0f )
				{
					b3BodyId chest = nbFigure_GetBody( figures[k], nb_humanoidChest );
					b3CosSin cs = b3ComputeCosSin( angle );
					nbHitDef hit = { hitId++, nb_humanoidChest, { strength * cs.cosine, 0.0f, strength * cs.sine },
									 b3Body_GetWorldPoint( chest, (b3Vec3){ 0.0f, 0.2f, 0.0f } ) };
					nbFigure_ApplyHit( figures[k], &hit );
				}
			}

			// Grenades 1 m beyond the walls, one every 2.5 m
			if ( p == 4 && step == 0 )
			{
				nbImpactDef impact = { 0 };
				impact.radius = 1.3f;
				impact.damage = 3.0e5f;
				impact.ejectSpeed = 12.0f;
				impact.explosionImpulse = 40.0f * impact.ejectSpeed;
				for ( int row = 0; row < rows; ++row )
				{
					for ( int column = 0; column < columns; ++column )
					{
						impact.point = b3ToPos( (b3Vec3){ 2.5f * (float)column, 1.0f, 2.65f - 6.0f * (float)row } );
						nbWorld_ApplyImpact( scene.world, &impact );
					}
				}
			}

			uint64_t ticks = b3GetTicks();
			b3World_Step( scene.physicsWorld, 1.0f / 60.0f, 4 );
			stepTimes[step] = b3GetMilliseconds( ticks );
			ticks = b3GetTicks();
			nbWorld_Update( scene.world, 1.0f / 60.0f );
			updateTimes[step] = b3GetMilliseconds( ticks );
			steps += 1;

			nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
			awake = b3MaxInt( awake, stats.awakeRagdollCount );
			if ( p == 3 && stats.animatedCount == figureCount )
			{
				break;
			}
		}

		float* series[2] = { stepTimes, updateTimes };
		float values[2][4];
		for ( int k = 0; k < 2; ++k )
		{
			float total = 0.0f;
			for ( int i = 0; i < steps; ++i )
			{
				total += series[k][i];
			}
			qsort( series[k], (size_t)steps, sizeof( float ), CompareFloats );
			values[k][0] = total / (float)steps;
			values[k][1] = series[k][steps * 95 / 100];
			values[k][2] = series[k][steps * 99 / 100];
			values[k][3] = series[k][steps - 1];
		}
		nbProfile profile = nbWorld_GetProfile( scene.world );
		nbFigureStats after = nbWorld_GetFigureStats( scene.world );
		printf( "    %-12s %5d | %8.3f %6.3f %6.3f %6.3f | %10.3f %6.3f %6.3f %6.3f | %7.3f %6.3f %7.3f | %5d %7d | %d, %d, %d, "
				"%d, %d, %d, %d\n",
				phases[p].name, steps, values[0][0], values[0][1], values[0][2], values[0][3], values[1][0], values[1][1],
				values[1][2], values[1][3], profile.stages[nb_profileFigures], profile.stages[nb_profileSolve],
				profile.stages[nb_profileCollide], profile.packStepCount, profile.domainStepCount, awake,
				after.stepCount - before.stepCount, after.catchCount - before.catchCount,
				after.knockdownCount - before.knockdownCount, after.getUpCount - before.getUpCount,
				after.shiftCount - before.shiftCount, after.abortCount - before.abortCount );
	}

	nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
	printf( "    at the end %d standing, %d catching themselves, %d lying, %d getting up, memory %.1f MB, figures hash 0x%08x\n",
			stats.animatedCount, stats.balancingCount, stats.ragdollCount, stats.gettingUpCount,
			(double)( nbGetByteCount() + b3GetByteCount() ) / 1048576.0, HashFigures( figures, figureCount ) );
	DestroyScene( &scene );
}

// Ragdolls in a heap of debris: 20 humanoids knocked down on a plate over two layers of 4 513 cubes, one large island
// whose domains solve the joints of the ragdolls, see b3Counters::domainJointCount. The plate never sleeps and the figures
// stay down, so every measured step solves the same heap, after the figures fell and came to rest on the plate.
#define PILE_FIGURES 20
#define PILE_SIDE 48
#define PILE_SETTLE_STEPS 180
#define PILE_STEPS 420

static void BenchmarkFigurePile( int workerCount )
{
	Scene scene = CreateScene( workerCount );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BoxHull cube = b3MakeBoxHull( 0.05f, 0.05f, 0.05f );
	int cubeCount = 0;
	for ( int layer = 0; layer < 2; ++layer )
	{
		int cubes = PILE_SIDE - layer;
		float offset = 0.06f * (float)( cubes - 1 );
		for ( int i = 0; i < cubes * cubes; ++i )
		{
			bodyDef.position = (b3Vec3){ 0.12f * (float)( i % cubes ) - offset, 0.05f + 0.1f * (float)layer,
										 0.12f * (float)( i / cubes ) - offset };
			b3BodyId body = b3CreateBody( scene.physicsWorld, &bodyDef );
			b3CreateHullShape( body, &shapeDef, &cube.base );
			cubeCount += 1;
		}
	}

	bodyDef.position = (b3Vec3){ 0.0f, 0.251f, 0.0f };
	bodyDef.enableSleep = false;
	b3BodyId plate = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull slab = b3MakeBoxHull( 0.06f * (float)PILE_SIDE, 0.05f, 0.06f * (float)PILE_SIDE );
	shapeDef.density = 20000.0f;
	b3CreateHullShape( plate, &shapeDef, &slab.base );

	nbBoneDef bones[nb_humanoidBoneCount];
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbFigureDef def = nbDefaultFigureDef();
	def.rig = &rig;
	def.standHeight = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	def.canGetUp = false;
	nbFigureId figures[PILE_FIGURES];
	for ( int k = 0; k < PILE_FIGURES; ++k )
	{
		def.position = b3ToPos( (b3Vec3){ -1.2f + 0.6f * (float)( k % 5 ), 0.301f, -0.9f + 0.6f * (float)( k / 5 ) } );
		def.heading = 0.7f * (float)k;
		figures[k] = nbCreateFigure( scene.world, &def );
	}

	static float stepTimes[PILE_STEPS];
	for ( int step = 0; step < PILE_SETTLE_STEPS + PILE_STEPS; ++step )
	{
		if ( step == 10 )
		{
			for ( int k = 0; k < PILE_FIGURES; ++k )
			{
				nbFigure_Knockdown( figures[k] );
			}
		}
		if ( step == PILE_SETTLE_STEPS )
		{
			nbWorld_ResetProfile( scene.world );
		}

		uint64_t ticks = b3GetTicks();
		b3World_Step( scene.physicsWorld, 1.0f / 60.0f, 4 );
		float stepTime = b3GetMilliseconds( ticks );
		nbWorld_Update( scene.world, 1.0f / 60.0f );
		if ( step >= PILE_SETTLE_STEPS )
		{
			stepTimes[step - PILE_SETTLE_STEPS] = stepTime;
		}
	}

	float total = 0.0f;
	for ( int i = 0; i < PILE_STEPS; ++i )
	{
		total += stepTimes[i];
	}
	qsort( stepTimes, PILE_STEPS, sizeof( float ), CompareFloats );
	nbProfile profile = nbWorld_GetProfile( scene.world );
	b3Counters counters = b3World_GetCounters( scene.physicsWorld );
	nbFigureStats stats = nbWorld_GetFigureStats( scene.world );
	int onPlate = 0;
	for ( int k = 0; k < PILE_FIGURES; ++k )
	{
		onPlate += b3Body_GetPosition( nbFigure_GetBody( figures[k], nb_humanoidPelvis ) ).y > 0.3 ? 1 : 0;
	}

	printf( "  %d ragdolls knocked down on a plate over %d cubes, staying down\n", PILE_FIGURES, cubeCount );
	printf( "    steps | step avg    p95    p99    max | solve constraints overflow | domains copies joints | awake "
			"ragdolls, on the plate\n" );
	printf( "    %5d | %8.3f %6.3f %6.3f %6.3f | %5.3f %11.3f %8.3f | %7d %6d %6d | %d, %d\n", PILE_STEPS,
			total / (float)PILE_STEPS, stepTimes[PILE_STEPS * 95 / 100], stepTimes[PILE_STEPS * 99 / 100],
			stepTimes[PILE_STEPS - 1], profile.stages[nb_profileSolve], profile.stages[nb_profileConstraints],
			profile.stages[nb_profileOverflow], counters.domainCount, counters.domainCopyCount, counters.domainJointCount,
			stats.awakeRagdollCount, onPlate );
	printf( "    at the end memory %.1f MB, figures hash 0x%08x\n",
			(double)( nbGetByteCount() + b3GetByteCount() ) / 1048576.0, HashFigures( figures, PILE_FIGURES ) );
	DestroyScene( &scene );
}

// The streamed city of BenchmarkCity. The stream of the library decides which houses are loaded, the city says what
// stands where and keeps the damaged houses that go with their rubble in a file.
typedef struct City
{
	Scene scene;
	nbMaterial concrete;
	int side;
	nbStreamId stream;

	// The houses the stream builds right now, by site
	House** building;

	// Where the file keeps every saved house
	FILE* store;
	long storeBytes;
	long* saveOffsets;
	size_t* saveSizes;
} City;

static b3Vec3 CityHousePosition( const City* city, int index )
{
	int i = index % city->side, j = index / city->side;
	float half = 0.5f * (float)( city->side - 1 );
	return (b3Vec3){ 14.0f * ( (float)i - half ), 0.0f, 12.0f * ( (float)j - half ) };
}

static void PrepareCityHouse( int site, nbSiteDef* siteDef, void* context )
{
	City* city = context;
	House* house = malloc( sizeof( House ) );
	BuildHouse( house, CityHousePosition( city, site ), (uint32_t)( 100 + site ), &city->concrete );
	city->building[site] = house;
	siteDef->def = house->def;
	siteDef->pieces = house->pieces;
	siteDef->pieceCount = house->pieceCount;
}

static void ReleaseCityHouse( int site, void* context )
{
	City* city = context;
	free( city->building[site] );
	city->building[site] = NULL;
}

// A damaged house goes to the end of the file
static bool StoreCityHouse( int site, const void* data, size_t size, void* context )
{
	City* city = context;
	fseek( city->store, city->storeBytes, SEEK_SET );
	if ( fwrite( data, 1, size, city->store ) != size )
	{
		return false;
	}
	city->saveOffsets[site] = city->storeBytes;
	city->saveSizes[site] = size;
	city->storeBytes += (long)size;
	return true;
}

static bool FetchCityHouse( int site, void* buffer, size_t size, void* context )
{
	City* city = context;
	fseek( city->store, city->saveOffsets[site], SEEK_SET );
	return size == city->saveSizes[site] && fread( buffer, 1, size, city->store ) == size;
}

// A city of 256 x 256 houses, far too large to hold at once, streamed around a camera that flies 900 m through it at
// 30 m/s and 300 m back, see nbCreateStream. The houses within 120 m of the camera are loaded, nearest first and at most
// two per frame, and so are the houses within 10 m of any body that moves. The workers prepare them in the background,
// and four frames later they are built in: the benchmark runs its frames back to back, a game of 16 ms per frame needs
// one. The ones beyond 140 m go as soon as no body moved within 20 m of them for five seconds: an intact house once
// nbDestructible_CanUnload lets it, a damaged one once nbCanSaveDestructibles lets it, saved with its rubble to a file, at
// most one per frame, and brought back from there when it is needed again. A grenade falls every fifth frame on a house
// within 60 m of the camera.
static void BenchmarkCity( int workerCount )
{
	enum
	{
		side = 256,
		outFrames = 1800,
		frameCount = 2400,
	};
	const float fireRadius = 60.0f, speed = 30.0f;

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
	int houseCount = side * side;
	city.building = calloc( (size_t)houseCount, sizeof( House* ) );
	city.saveOffsets = calloc( (size_t)houseCount, sizeof( long ) );
	city.saveSizes = calloc( (size_t)houseCount, sizeof( size_t ) );

	nbStreamDef streamDef = nbDefaultStreamDef();
	streamDef.loadRadius = 120.0f;
	streamDef.unloadMargin = 20.0f;
	streamDef.loadDelay = 4;
	streamDef.prepareFcn = PrepareCityHouse;
	streamDef.releaseFcn = ReleaseCityHouse;
	streamDef.storeFcn = StoreCityHouse;
	streamDef.fetchFcn = FetchCityHouse;
	streamDef.userContext = &city;
	city.stream = nbCreateStream( city.scene.world, &streamDef );
	for ( int h = 0; h < houseCount; ++h )
	{
		// Two stories of 3.25 m on 9 x 6.5 m
		b3Vec3 p = CityHousePosition( &city, h );
		nbStream_AddSite( city.stream, (b3AABB){ { p.x - 4.5f, 0.0f, p.z - 3.25f }, { p.x + 4.5f, 6.5f, p.z + 3.25f } } );
	}

	// Everything around the start at once, then in the background as the camera flies
	b3Vec3 camera = { -450.0f, 0.0f, 6.0f };
	int64_t baseBytes = nbGetByteCount() + b3GetByteCount();
	uint64_t ticks = b3GetTicks();
	nbStream_LoadAround( city.stream, &camera, 1 );
	float startTime = b3GetMilliseconds( ticks );
	int startCount = nbStream_GetStats( city.stream ).loadedCount;
	int64_t houseBytes = ( nbGetByteCount() + b3GetByteCount() - baseBytes ) / b3MaxInt( startCount, 1 );
	int64_t maxBytes = 0;
	int maxLoaded = startCount;

	static float streamTimes[frameCount], impactTimes[frameCount], stepTimes[frameCount], updateTimes[frameCount],
		frameTimes[frameCount];
	nbRandom rng = nbMakeRandom( 11, 0 );
	int missed = 0;
	nbImpactDef impact = { 0 };
	impact.radius = 1.3f;
	impact.damage = 3.0e5f;
	impact.ejectSpeed = 12.0f;
	impact.explosionImpulse = 40.0f * impact.ejectSpeed;
	for ( int frame = 0; frame < frameCount; ++frame )
	{
		camera.x += frame < outFrames ? speed / 60.0f : -speed / 60.0f;

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
				missed += NB_IS_NULL( nbStream_GetDestructible( city.stream, j * side + i ) ) ? 1 : 0;
				b3Vec3 local = wall == 0 ? (b3Vec3){ 4.2f * along, height, 3.4f }
							 : wall == 1 ? (b3Vec3){ 4.2f * along, height, -3.4f }
							 : wall == 2 ? (b3Vec3){ 4.7f, height, 3.0f * along }
										 : (b3Vec3){ -4.7f, height, 3.0f * along };
				impact.point = b3Add( CityHousePosition( &city, j * side + i ), local );
				impactTime = nbWorld_ApplyImpact( city.scene.world, &impact ).totalTime;
			}
		}

		ticks = b3GetTicks();
		b3World_Step( city.scene.physicsWorld, 1.0f / 60.0f, 4 );
		float stepTime = b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		nbWorld_Update( city.scene.world, 1.0f / 60.0f );
		float updateTime = b3GetMilliseconds( ticks );
		ticks = b3GetTicks();
		nbStream_Update( city.stream, &camera, 1 );
		float streamTime = b3GetMilliseconds( ticks );

		// The update marks the houses near the bodies that moved for the stream, that counts as streaming
		nbStreamStats stats = nbStream_GetStats( city.stream );
		streamTime += stats.markTime;
		updateTime -= stats.markTime;
		maxLoaded = b3MaxInt( maxLoaded, stats.loadedCount );

		streamTimes[frame] = streamTime;
		impactTimes[frame] = impactTime;
		stepTimes[frame] = stepTime;
		updateTimes[frame] = updateTime;
		frameTimes[frame] = streamTime + impactTime + stepTime + updateTime;
		int64_t bytes = nbGetByteCount() + b3GetByteCount();
		maxBytes = bytes > maxBytes ? bytes : maxBytes;
	}

	nbStreamStats stream = nbStream_GetStats( city.stream );
	printf( "  %d houses on %.1f x %.1f km, %d of them loaded at first in %.0f ms, %.2f MB each, all would take %.1f GB\n",
			houseCount, 14.0f * (float)side / 1000.0f, 12.0f * (float)side / 1000.0f, startCount, startTime,
			(double)houseBytes / 1048576.0, (double)houseBytes * (double)houseCount / 1073741824.0 );
	printf( "  then %d houses loaded, %d of them for moving bodies, and %d unloaded in %d frames, at most %d at once, %d "
			"grenades on houses not loaded\n",
			stream.loads - startCount, stream.urgentLoads, stream.unloads, frameCount, maxLoaded, missed );
	printf( "  %d damaged houses saved, %d brought back, %d in the store and %d loaded at the end, %.0f KB each\n", stream.saves,
			stream.restores, stream.savedCount, stream.damagedCount,
			(double)city.storeBytes / 1024.0 / b3MaxInt( stream.saves, 1 ) );

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

	// The houses still in the background go with the world, and the stream gives back what it got for them
	DestroyScene( &city.scene );
	free( city.building );
	free( city.saveOffsets );
	free( city.saveSizes );
	fclose( city.store );
}

int main( int argc, char** argv )
{
	if ( argc > 1 && strcmp( argv[1], "figures" ) == 0 )
	{
		int workerCount = argc > 2 ? atoi( argv[2] ) : 0;
		workerCount = workerCount <= 0 ? nbGetRecommendedWorkerCount( false ) : workerCount;
		int rounds = argc > 3 ? atoi( argv[3] ) : 1;
		s_jointPacks = argc <= 4 || strcmp( argv[4], "off" ) != 0;
		printf( "Figures after Overgrowth, %d workers, %d round%s%s\n", workerCount, rounds, rounds == 1 ? "" : "s",
				s_jointPacks ? "" : ", joints outside the packs" );
		for ( int r = 0; r < b3MaxInt( rounds, 1 ); ++r )
		{
			BenchmarkFigures( workerCount, 1 );
			BenchmarkFigures( workerCount, 10 );
			BenchmarkFigures( workerCount, 50 );
			BenchmarkFigurePile( workerCount );
			BenchmarkTown( workerCount, 1.0f, 4, 1, 5, NULL, 60, NULL, true );
		}
		return 0;
	}

	if ( argc > 1 && strcmp( argv[1], "rates" ) == 0 )
	{
		int rounds = argc > 2 ? atoi( argv[2] ) : 1;
		BenchmarkRates( rounds < 1 ? 1 : rounds );
		return 0;
	}

	if ( argc > 1 && strcmp( argv[1], "scaling" ) == 0 )
	{
		int rounds = argc > 2 ? atoi( argv[2] ) : 1;
		BenchmarkScaling( rounds < 1 ? 1 : rounds );
		return 0;
	}

	// Zero takes as many as a game would on this machine, see nbGetRecommendedWorkerCount
	int workerCount = argc > 1 ? atoi( argv[1] ) : 1;
	workerCount = workerCount == 0 ? nbGetRecommendedWorkerCount( false ) : ( workerCount < 1 ? 1 : workerCount );

	nbVersion version = nbGetVersion();
	b3Version b3version = b3GetVersion();
	printf( "Nebenan %d.%d.%d on Box3D %d.%d.%d, workers for fracture and physics: %d, contact solver lanes: %d\n",
			version.major, version.minor, version.revision, b3version.major, b3version.minor, b3version.revision, workerCount,
			b3GetSIMDWidth() );

	BenchmarkKernel();

	printf( "\nImpact pipeline (fracture + support graph + Box3D bodies), physics at 60 Hz with 4 substeps\n" );
	BenchmarkRifle( workerCount );
	BenchmarkExplosions( workerCount );
	BenchmarkBuilding( workerCount );

	// Larger fragments are the main lever of the cost of mass destruction
	printf( "\nTown under fire (every step: grenades, Box3D step, destruction update)\n" );
	BenchmarkTown( workerCount, 1.0f, 4, 1, 5, NULL, 60, NULL, false );
	BenchmarkTown( workerCount, 2.0f, 4, 1, 5, NULL, 60, NULL, false );

	// Fragments four times as large keep the cost of a grenade down, so a block four times the size takes four every step
	printf( "\nBlock of 64 houses under a barrage, fragment scale 4.0 (four grenades every step, Box3D step, update)\n" );
	BenchmarkTown( workerCount, 4.0f, 8, 4, 1, NULL, 60, NULL, false );

	printf( "\nFigures after Overgrowth: standing, hit, knocked down, getting up, behind walls that break (Box3D step, update)\n" );
	BenchmarkFigures( workerCount, 1 );
	BenchmarkFigures( workerCount, 10 );
	BenchmarkFigures( workerCount, 50 );

	// The figures in the streets of the first town, knocked over and buried by what comes down
	printf( "\nTown of 16 houses under fire with two figures beside every house, fragment scale 1.0 (grenades, Box3D step, "
			"update)\n" );
	BenchmarkTown( workerCount, 1.0f, 4, 1, 5, NULL, 60, NULL, true );

	printf( "\nCity streamed around a camera flying through it, fragment scale 4.0 (streaming, grenades, Box3D step, update)\n" );
	BenchmarkCity( workerCount );
	return 0;
}
