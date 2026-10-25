// SPDX-License-Identifier: MIT

// Headless benchmark for the fracture kernel and the full impact pipeline.
// Usage: nebenan_benchmark [workerCount], the worker count applies to the fracture and to Box3D
//        nebenan_benchmark scaling [rounds], the towns under fire with 1, 2, 4 and so on workers, see
//        nbGetScalingWorkerCounts, and the scaling report of the library for each town
//        nebenan_benchmark figures [workerCount] [noself], only the towns with 0, 1, 10 and 50 figures, noself without
//        collisions between the bones of a figure
//        nebenan_benchmark repeat [count] [workerCount] [system], the town under fire count times in the same process,
//        system leaves its speed to the system, see nbKeepFullSpeed
//        nebenan_benchmark joints [workerCount], only what each kind of joint of a figure costs in the solver

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

// What a town cost over its simulated time, see BenchmarkRates
typedef struct TownCost
{
	float step, update, impacts;
	int steps, chunks, rubble, collapsed;
} TownCost;

// A block of side x side houses under fire for 20 simulated seconds at stepRate steps per second, see RunTown: every
// interval sixtieths of a second as many grenades, each on a wall of a random house, at the same times at every rate,
// and figureCount humanoids in its streets, see AddFigures, whose bones collide with each other unless noSelfCollision,
// and a pendulum far away if pendulum, see AddPendulum
typedef struct TownSetup
{
	int workerCount;
	float fragmentScale;
	int side, grenadesPerVolley, interval, stepRate, figureCount;
	bool noSelfCollision;
	bool pendulum;
} TownSetup;

// What RunTown measured: the houses built, the times of every frame by s_townSeries, sorted, and their sums, what Box3D
// spent and held per frame, the memory at most, what the figures took, and at the end the stats, the profile of the
// library over the frames and what the figures did
typedef struct TownRun
{
	int houseCount, builtChunks, frameCount, grenades, slowFrames;
	float createTime;
	float* times[4];
	float totals[4];
	float collide, solve, awakeContacts;
	int maxBodies, maxAwake, maxContacts, maxJoints;
	int64_t maxBytes, figureBytes;
	nbStats stats;
	nbProfile profile;
	nbFigureStats figures;
} TownRun;

static const char* s_townSeries[4] = { "frame", "physics step", "update", "impacts" };

// The humanoids of the towns with figures: in the streets between the rows of houses, three meters apart, with balance,
// playing the arm clip of chapter 33.4 of the game roadmap over and over, see PushFigures
static void AddFigures( Scene* scene, int side, int figureCount, bool selfCollision, nbArticulationId* figures )
{
	nbBoneDef bones[nb_humanoidBoneCount];
	float rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	rig.selfCollision = selfCollision;
	nbMotorProfile motors = nbDefaultMotorProfile();
	nbBalanceDef balance = nbDefaultBalanceDef();

	b3Quat poses[3][nb_humanoidBoneCount];
	for ( int k = 0; k < 3; ++k )
	{
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			poses[k][i] = b3Quat_identity;
		}
	}
	poses[1][nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.3f );
	poses[1][nb_humanoidForearmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.5f );
	nbKeyframe keyframes[3] = { { 0.0f, poses[0] }, { 0.6f, poses[1] }, { 1.2f, poses[2] } };
	nbClipDef clip = { keyframes, 3, nb_humanoidBoneCount, true };

	// The streets run along x halfway between the rows, the first figure of each three meters in from the end of the town
	int streets = b3MaxInt( side - 1, 1 );
	float half = 0.5f * (float)( side - 1 );
	for ( int k = 0; k < figureCount; ++k )
	{
		int street = k % streets;
		int slot = k / streets;
		b3Vec3 position = { -14.0f * half - 3.0f + 3.0f * (float)slot, rootHeight, 12.0f * ( (float)street - half ) + 6.0f };
		figures[k] = nbCreateArticulation( scene->world, &rig, b3ToPos( position ), b3Quat_identity, &motors );
		nbArticulation_EnableBalance( figures[k], &balance );
		nbArticulation_PlayClip( figures[k], &clip, 1.0f );
	}
}

// A pendulum far from the town that swings for ever, a single joint in an island of its own: it shows what a joint that is
// awake costs the packs of islands, see b3SolvePacks, and adds nothing else
static void AddPendulum( Scene* scene )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = b3ToPos( (b3Vec3){ 90.0f, 5.0f, 0.0f } );
	b3BodyId anchor = b3CreateBody( scene->physicsWorld, &bodyDef );
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = b3ToPos( (b3Vec3){ 91.0f, 5.0f, 0.0f } );
	b3BodyId bob = b3CreateBody( scene->physicsWorld, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3Sphere sphere = { b3Vec3_zero, 0.1f };
	b3CreateSphereShape( bob, &shapeDef, &sphere );

	b3RevoluteJointDef jointDef = b3DefaultRevoluteJointDef();
	jointDef.base.bodyIdA = anchor;
	jointDef.base.bodyIdB = bob;
	jointDef.base.localFrameB.p = (b3Vec3){ -1.0f, 0.0f, 0.0f };
	b3CreateRevoluteJoint( scene->physicsWorld, &jointDef );
}

// A figure that stands takes a push on the chest every six seconds, the figures one after the other over the first six:
// in turn one it catches with steps and one that throws it, from the front, a side and the back, see the roadmap 26.14
// and 26.15. A figure that does not stand waits for its next time. The pushes are the hits of a game, the grenades add
// debris.
static void PushFigures( const nbArticulationId* figures, int figureCount, int frame, int* pushes, uint32_t* hitCount )
{
	static const b3Vec3 directions[4] = {
		{ 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } };
	for ( int f = 0; f < figureCount; ++f )
	{
		int offset = frame - 30 - f * 360 / figureCount;
		if ( offset < 0 || offset % 360 != 0 || nbArticulation_GetBalance( figures[f] ).state != nb_balanceStanding )
		{
			continue;
		}

		b3BodyId torso = nbArticulation_GetBody( figures[f], nb_humanoidTorso );
		nbHitDef hit = { 0 };
		hit.id = ++*hitCount;
		hit.bone = nb_humanoidTorso;
		hit.impulse = b3MulSV( pushes[f] % 2 == 0 ? 60.0f : 150.0f, directions[( f + pushes[f] ) % 4] );
		hit.point = b3Body_GetWorldPoint( torso, (b3Vec3){ 0.0f, 0.3f, 0.1f } );
		nbArticulation_ApplyHit( figures[f], &hit );
		pushes[f] += 1;
	}
}

static void RunTown( const TownSetup* setup, TownRun* run )
{
	memset( run, 0, sizeof( TownRun ) );
	Scene scene = CreateScene( setup->workerCount );
	nbWorld_SetFragmentScale( scene.world, setup->fragmentScale );
	nbMaterial concrete = nbDefaultMaterial();
	concrete.strength = 1.1e6f;
	concrete.fragmentSize = 0.13f;

	// All houses at once: while the calling thread builds one into the world, the workers fracture the next
	int side = setup->side;
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
	run->createTime = b3GetMilliseconds( ticks );
	free( town );
	free( defs );
	free( pieceLists );
	free( pieceCounts );
	free( ids );
	run->houseCount = houseCount;
	run->builtChunks = nbWorld_GetStats( scene.world ).chunkCount;

	// The figures, and what they take in Nebenan and Box3D
	int figureCount = setup->figureCount;
	nbArticulationId* figures = malloc( sizeof( nbArticulationId ) * (size_t)b3MaxInt( figureCount, 1 ) );
	int* pushes = calloc( (size_t)b3MaxInt( figureCount, 1 ), sizeof( int ) );
	int64_t bytes = nbGetByteCount() + b3GetByteCount();
	AddFigures( &scene, side, figureCount, setup->noSelfCollision == false, figures );
	run->figureBytes = nbGetByteCount() + b3GetByteCount() - bytes;
	if ( setup->pendulum )
	{
		AddPendulum( &scene );
	}
	uint32_t hitCount = 0;

	enum
	{
		sixtiethCount = 1200
	};

	int stepRate = setup->stepRate;
	int frameCount = sixtiethCount * stepRate / 60;
	float timeStep = 1.0f / (float)stepRate;
	int subStepCount = nbGetSubStepCount( (float)stepRate );
	for ( int k = 0; k < 4; ++k )
	{
		run->times[k] = malloc( sizeof( float ) * (size_t)frameCount );
	}
	float* frameTimes = run->times[0];
	float* stepTimes = run->times[1];
	float* updateTimes = run->times[2];
	float* impactTimes = run->times[3];
	int sixtieth = 0;
	nbRandom rng = nbMakeRandom( 9, 0 );
	nbImpactDef impact = { 0 };
	impact.radius = 1.3f;
	impact.damage = 3.0e5f;
	impact.ejectSpeed = 12.0f;
	impact.explosionImpulse = 40.0f * impact.ejectSpeed;
	float collide = 0.0f, solve = 0.0f, awakeContacts = 0.0f;
	nbWorld_ResetProfile( scene.world );

	for ( int frame = 0; frame < frameCount; ++frame )
	{
		// The volleys of the sixtieths of a second up to the time of this step
		float impactTime = 0.0f;
		int sixtiethEnd = frame * 60 / stepRate + 1;
		for ( ; sixtieth < sixtiethEnd; ++sixtieth )
		{
			for ( int g = 0; g < ( sixtieth % setup->interval == 0 ? setup->grenadesPerVolley : 0 ); ++g )
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
				run->grenades += 1;
			}
		}
		PushFigures( figures, figureCount, frame, pushes, &hitCount );

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
		collide += profile.collide;
		solve += profile.solve;
		awakeContacts += (float)counters.awakeContactCount;
		run->maxBodies = b3MaxInt( run->maxBodies, counters.bodyCount );
		run->maxAwake = b3MaxInt( run->maxAwake, b3World_GetAwakeBodyCount( scene.physicsWorld ) );
		run->maxContacts = b3MaxInt( run->maxContacts, counters.contactCount );
		run->maxJoints = b3MaxInt( run->maxJoints, counters.jointCount );
		int64_t frameBytes = nbGetByteCount() + b3GetByteCount();
		run->maxBytes = frameBytes > run->maxBytes ? frameBytes : run->maxBytes;
	}

	for ( int i = 0; i < frameCount; ++i )
	{
		run->slowFrames += frameTimes[i] > 1000.0f / 60.0f ? 1 : 0;
	}
	for ( int k = 0; k < 4; ++k )
	{
		float total = 0.0f;
		for ( int i = 0; i < frameCount; ++i )
		{
			total += run->times[k][i];
		}
		run->totals[k] = total;
		qsort( run->times[k], frameCount, sizeof( float ), CompareFloats );
	}

	run->frameCount = frameCount;
	run->collide = collide / frameCount;
	run->solve = solve / frameCount;
	run->awakeContacts = awakeContacts / frameCount;
	run->stats = nbWorld_GetStats( scene.world );
	run->profile = nbWorld_GetProfile( scene.world );
	run->figures = nbWorld_GetFigureStats( scene.world );
	free( pushes );
	free( figures );
	free( houses );
	DestroyScene( &scene );
}

static void FreeTownRun( TownRun* run )
{
	for ( int k = 0; k < 4; ++k )
	{
		free( run->times[k] );
	}
}

// A town of 16 houses wrecked by a grenade every five frames, twelve per second, like holding the fire button
// A block of side x side houses under fire, see TownSetup. The profile of the library over the frames under fire goes to
// profile and the cost to cost if they are not null.
static void BenchmarkTown( int workerCount, float fragmentScale, int side, int grenadesPerVolley, int interval,
						   nbProfile* profile, int stepRate, TownCost* cost )
{
	TownSetup setup = { workerCount, fragmentScale, side, grenadesPerVolley, interval, stepRate, 0 };
	TownRun run;
	RunTown( &setup, &run );
	printf( "  town of %d houses with %d chunks built in %.2f ms, fragment scale %.1f\n", run.houseCount, run.builtChunks,
			run.createTime, fragmentScale );

	int n = run.frameCount;
	for ( int k = 0; k < 4; ++k )
	{
		printf( "  %-13s avg %6.2f ms  p95 %6.2f ms  max %6.2f ms\n", s_townSeries[k], run.totals[k] / (float)n,
				run.times[k][n * 95 / 100], run.times[k][n - 1] );
	}
	printf( "  99 %% of the frames up to %.2f ms, %d frames over 16.7 ms\n", run.times[0][n * 99 / 100], run.slowFrames );
	printf( "  Box3D per step: collide %.2f ms, solve %.2f ms, %.0f awake contacts\n", run.collide, run.solve,
			run.awakeContacts );
	printf( "  %d grenades, %d storeys collapsed, chunks %d, rubble %d, bodies at most %d (awake %d), contacts at most %d\n",
			run.grenades, run.stats.collapsedStoreyCount, run.stats.chunkCount, run.stats.rubbleCount, run.maxBodies,
			run.maxAwake, run.maxContacts );
	if ( profile != NULL )
	{
		*profile = run.profile;
	}
	if ( cost != NULL )
	{
		*cost = (TownCost){ run.totals[1],
							run.totals[2],
							run.totals[3],
							n,
							run.stats.chunkCount,
							run.stats.rubbleCount,
							run.stats.collapsedStoreyCount };
	}
	FreeTownRun( &run );
}

// Stage 6 of 27.14 of the game roadmap: 1, 10 and 50 figures in the town of 16 houses at fragment scale 2 against the
// town without figures, quiet and under a grenade every 1/12 s, see AddFigures and PushFigures, and the town without
// figures but with a pendulum, see AddPendulum. Where the time goes, in the solver too, what the figures hold in Box3D
// and in memory, and what they did. Without self collision for what the contacts between the bones of a figure cost,
// see 27.13 of the game roadmap.
static void BenchmarkFigures( int workerCount, bool selfCollision )
{
	enum
	{
		runCount = 5
	};

	const int counts[runCount] = { 0, 0, 1, 10, 50 };
	const bool pendulums[runCount] = { false, true, false, false, false };
	const char* labels[runCount] = { "0", "0+joint", "1", "10", "50" };
	for ( int fire = 0; fire < 2; ++fire )
	{
		printf( fire == 0
					? "\nFigures in the quiet town (16 houses, fragment scale 2, figures with balance playing a clip, a push "
					  "every 6 s)\n"
					: "\nFigures in the town under fire (the same with a grenade every 1/12 s)\n" );
		if ( selfCollision == false )
		{
			printf( "  without collisions between the bones of a figure\n" );
		}
		TownRun runs[runCount];
		for ( int r = 0; r < runCount; ++r )
		{
			TownSetup setup = { workerCount, 2.0f, 4, fire, 5, 60, counts[r], selfCollision == false, pendulums[r] };
			RunTown( &setup, runs + r );
		}

		printf( "  figures | frame ms  p50     p95     p99   p99.9     max |   step  update impacts | per step ms: figures  "
				"clips balance motors\n" );
		for ( int r = 0; r < runCount; ++r )
		{
			const TownRun* run = runs + r;
			const float* frames = run->times[0];
			int n = run->frameCount;
			const float* stages = run->profile.stages;
			printf( "  %7s |      %7.3f %7.3f %7.3f %7.3f %7.3f | %6.3f %7.3f %7.3f |              %7.4f %6.4f %7.4f %6.4f\n",
					labels[r], frames[n / 2], frames[n * 95 / 100], frames[n * 99 / 100], frames[n * 999 / 1000], frames[n - 1],
					run->totals[1] / (float)n, run->totals[2] / (float)n, run->totals[3] / (float)n, stages[nb_profileFigures],
					stages[nb_profileFigureClips], stages[nb_profileFigureBalance], stages[nb_profileFigureMotors] );
		}

		printf( "  figures | a figure per step us: total  clips balance motors | Box3D ms: collide  solve | at most: bodies "
				"joints contacts | MB, KB a figure\n" );
		for ( int r = 0; r < runCount; ++r )
		{
			const TownRun* run = runs + r;
			const float* stages = run->profile.stages;
			float scale = counts[r] > 0 ? 1000.0f / (float)counts[r] : 0.0f;
			printf( "  %7s |                        %6.2f %6.2f %7.2f %6.2f |          %7.3f %6.3f |        %6d %6d %8d | %3.0f, "
					"%.0f\n",
					labels[r], scale * stages[nb_profileFigures], scale * stages[nb_profileFigureClips],
					scale * stages[nb_profileFigureBalance], scale * stages[nb_profileFigureMotors], run->collide, run->solve,
					run->maxBodies, run->maxJoints, run->maxContacts, (double)run->maxBytes / 1048576.0,
					counts[r] > 0 ? (double)run->figureBytes / 1024.0 / counts[r] : 0.0 );
		}

		printf( "  figures | at the end: standing stepping falling down getting up | falls got up failed | motors at the "
				"limit | chunks rubble storeys\n" );
		for ( int r = 0; r < runCount; ++r )
		{
			const TownRun* run = runs + r;
			const nbFigureStats* f = &run->figures;
			printf( "  %7s |             %8d %8d %7d %4d %10d | %5d %6d %6d | %19d | %6d %6d %7d\n", labels[r],
					f->stateCounts[nb_balanceStanding], f->stateCounts[nb_balanceStepping], f->stateCounts[nb_balanceFalling],
					f->stateCounts[nb_balanceDown], f->stateCounts[nb_balanceGettingUp], f->fallCount, f->getUpCount,
					f->failedGetUpCount, f->saturatedJointCount, run->stats.chunkCount, run->stats.rubbleCount,
					run->stats.collapsedStoreyCount );
		}

		// Where the solver works, in the steps it times, see nbProfile::solverJoints, and the steps it solved in packs of
		// islands, see nbProfile::packStepCount
		printf( "  figures | solver CPU ms in a timed step: joints contacts bodies | constraints ms | timed steps | steps "
				"in packs %%, with a team %%\n" );
		for ( int r = 0; r < runCount; ++r )
		{
			const nbProfile* p = &runs[r].profile;
			float steps = (float)b3MaxInt( p->stepCount, 1 );
			printf(
				"  %7s |                                %7.3f  %7.3f %6.3f |        %7.3f | %11d |                %6.1f %6.1f\n",
				labels[r], p->solverJoints, p->solverContacts, p->solverBodies, p->stages[nb_profileConstraints],
				p->solverSampleCount, 100.0f * (float)p->packStepCount / steps, 100.0f * (float)p->teamStepCount / steps );
		}

		for ( int r = 0; r < runCount; ++r )
		{
			FreeTownRun( runs + r );
		}
	}
}

// The town under fire again and again in the same process, for whether a town costs the same early and late. Without
// nbKeepFullSpeed Windows slows a process without a window in front down after a few seconds and moves its threads to
// its slowest cores, and the later towns cost more.
static void BenchmarkRepeat( int count, int workerCount )
{
	printf( "The town under fire %d times in the same process (16 houses, fragment scale 2, a grenade every 1/12 s), %d "
			"workers\n",
			count, workerCount );
	printf( "  town |  step ms collide   solve  update | chunks rubble\n" );
	float first = 0.0f, later = 0.0f, lowest = 1.0e9f, highest = 0.0f;
	for ( int r = 0; r < count; ++r )
	{
		TownSetup setup = { workerCount, 2.0f, 4, 1, 5, 60, 0, false, false };
		TownRun run;
		RunTown( &setup, &run );
		int n = run.frameCount;
		float step = run.totals[1] / (float)n;
		printf( "  %4d |  %7.3f %7.3f %7.3f %7.3f | %6d %6d\n", r + 1, step, run.collide, run.solve, run.totals[2] / (float)n,
				run.stats.chunkCount, run.stats.rubbleCount );
		fflush( stdout );
		first += r < 2 ? step : 0.0f;
		later += r < 2 ? 0.0f : step;
		lowest = step < lowest ? step : lowest;
		highest = step > highest ? step : highest;
		FreeTownRun( &run );
	}
	if ( count > 2 )
	{
		printf( "  the first two towns %.3f ms per step, the others %.3f ms, from the fastest to the slowest town +%.1f %%\n",
				first / 2.0f, later / (float)( count - 2 ), 100.0f * ( highest - lowest ) / lowest );
	}
}

// A rig of the joint benchmark: the humanoid, a chain of bones joined by one kind of joint, or a single bone
typedef struct JointRig
{
	const char* name;
	bool humanoid;
	int boneCount;
	nbJointType joint;
	bool springs;
} JointRig;

// What the joints of one rig cost the solver: 64 articulations on the ground of a world without sleep, 2 s to settle and
// 8 s measured, the solver time of the joints in a timed step, see nbProfile::solverJoints. Chains have as many bones as
// the humanoid and lie along x, 30 cm a bone, with the cone of a ball joint along the bone and the axis of a hinge across
// it, and the limits of the default bone. Without springs the joints are passive, the upright hold stays.
static float RunJointRig( int workerCount, const JointRig* setup, int* jointCount )
{
	Scene scene = CreateScene( workerCount );
	b3World_EnableSleeping( scene.physicsWorld, false );

	nbBoneDef bones[nb_humanoidBoneCount];
	float rootHeight = 0.06f;
	if ( setup->humanoid )
	{
		rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, bones, nb_humanoidBoneCount );
	}
	else
	{
		b3Quat zToX = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f * B3_PI );
		for ( int i = 0; i < setup->boneCount; ++i )
		{
			nbBoneDef def = nbDefaultBoneDef();
			def.parent = i - 1;
			def.restTransform.p = i == 0 ? b3Vec3_zero : (b3Vec3){ 0.3f, 0.0f, 0.0f };
			def.start = (b3Vec3){ 0.05f, 0.0f, 0.0f };
			def.end = (b3Vec3){ 0.25f, 0.0f, 0.0f };
			def.mass = 3.0f;
			def.joint = setup->joint;
			def.jointFrame = setup->joint == nb_ballJoint ? zToX : b3Quat_identity;
			bones[i] = def;
		}
	}

	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = setup->boneCount;
	nbMotorProfile motors = nbDefaultMotorProfile();
	motors.stiffness = setup->springs ? motors.stiffness : 0.0f;
	for ( int k = 0; k < 64; ++k )
	{
		b3Vec3 position = { 6.0f * ( (float)( k % 8 ) - 3.5f ), rootHeight, 3.0f * ( (float)( k / 8 ) - 3.5f ) };
		nbCreateArticulation( scene.world, &rig, b3ToPos( position ), b3Quat_identity, &motors );
	}
	*jointCount = b3World_GetCounters( scene.physicsWorld ).jointCount;

	float timeStep = 1.0f / 60.0f;
	int subStepCount = nbGetSubStepCount( 60.0f );
	for ( int frame = 0; frame < 600; ++frame )
	{
		if ( frame == 120 )
		{
			nbWorld_ResetProfile( scene.world );
		}
		b3World_Step( scene.physicsWorld, timeStep, subStepCount );
		nbWorld_Update( scene.world, timeStep );
	}

	float joints = nbWorld_GetProfile( scene.world ).solverJoints;
	DestroyScene( &scene );
	return joints;
}

// What each kind of joint of a figure costs in the solver, see RunJointRig: the humanoid without balance and clip, the
// upright hold alone, one bone per articulation, and chains of 15 bones joined by ball joints or hinges, with springs and
// without, or welds. Every articulation holds its root upright, see nbMotorProfile::uprightTorque, so a joint of a chain
// costs a fourteenth of its articulation without the upright hold.
static void BenchmarkJoints( int workerCount )
{
	enum
	{
		rigCount = 7
	};

	static const JointRig rigs[rigCount] = {
		{ "humanoid, 10 balls, 4 hinges, upright", true, nb_humanoidBoneCount, nb_ballJoint, true },
		{ "upright hold alone", false, 1, nb_ballJoint, true },
		{ "ball joints, cone, twist, spring", false, nb_humanoidBoneCount, nb_ballJoint, true },
		{ "ball joints, cone, twist", false, nb_humanoidBoneCount, nb_ballJoint, false },
		{ "hinges, limit, spring", false, nb_humanoidBoneCount, nb_hingeJoint, true },
		{ "hinges, limit", false, nb_humanoidBoneCount, nb_hingeJoint, false },
		{ "welds", false, nb_humanoidBoneCount, nb_fixedJoint, false },
	};

	printf( "\nJoints in the solver (64 articulations of a rig on the ground, no sleep, %d workers, solver CPU time of the "
			"joints in the timed steps)\n",
			workerCount );
	printf( "  rig                                   | joints |    ms | us per step: articulation  joint | a joint of the "
			"chain\n" );
	float perArticulation[rigCount];
	for ( int r = 0; r < rigCount; ++r )
	{
		int jointCount = 0;
		float ms = RunJointRig( workerCount, rigs + r, &jointCount );
		perArticulation[r] = 1000.0f * ms / 64.0f;
		float upright = perArticulation[1];
		printf( "  %-37s | %6d | %5.3f |              %11.2f %6.3f |", rigs[r].name, jointCount, ms, perArticulation[r],
				1000.0f * ms / (float)b3MaxInt( jointCount, 1 ) );
		if ( r >= 2 )
		{
			printf( " %8.3f", ( perArticulation[r] - upright ) / (float)( rigs[r].boneCount - 1 ) );
		}
		printf( "\n" );
		fflush( stdout );
	}

	// The humanoid against its parts: 10 ball joints and 4 hinges with springs and the upright hold
	float ball = ( perArticulation[2] - perArticulation[1] ) / (float)( nb_humanoidBoneCount - 1 );
	float hinge = ( perArticulation[4] - perArticulation[1] ) / (float)( nb_humanoidBoneCount - 1 );
	printf( "  the humanoid costs %.2f us per step, its parts as in the chains %.2f us: 10 x %.3f ball, 4 x %.3f hinge, "
			"%.3f upright\n",
			perArticulation[0], 10.0f * ball + 4.0f * hinge + perArticulation[1], ball, hinge, perArticulation[1] );
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
							   profiles + r * countCount + i, 60, NULL );
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
							   rates[k], &cost );
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
	// A benchmark has no window in front, and Windows would slow it down after a few seconds, see nbKeepFullSpeed
	bool system = argc > 4 && strcmp( argv[1], "repeat" ) == 0 && strcmp( argv[4], "system" ) == 0;
	bool fullSpeed = system == false && nbKeepFullSpeed( true );
	printf( "Full speed without a window in front: %s\n", fullSpeed ? "yes" : "no, the system decides" );

	if ( argc > 1 && strcmp( argv[1], "repeat" ) == 0 )
	{
		int count = argc > 2 ? atoi( argv[2] ) : 6;
		int workers = argc > 3 ? atoi( argv[3] ) : 2;
		BenchmarkRepeat( count < 1 ? 1 : count, workers < 1 ? 1 : workers );
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

	if ( argc > 1 && strcmp( argv[1], "joints" ) == 0 )
	{
		int workers = argc > 2 ? atoi( argv[2] ) : 1;
		BenchmarkJoints( workers < 1 ? 1 : workers );
		return 0;
	}

	if ( argc > 1 && strcmp( argv[1], "figures" ) == 0 )
	{
		int workers = argc > 2 ? atoi( argv[2] ) : 1;
		bool selfCollision = argc > 3 && strcmp( argv[3], "noself" ) == 0 ? false : true;
		BenchmarkFigures( workers == 0 ? nbGetRecommendedWorkerCount( false ) : ( workers < 1 ? 1 : workers ), selfCollision );
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
	BenchmarkTown( workerCount, 1.0f, 4, 1, 5, NULL, 60, NULL );
	BenchmarkTown( workerCount, 2.0f, 4, 1, 5, NULL, 60, NULL );

	// Fragments four times as large keep the cost of a grenade down, so a block four times the size takes four every step
	printf( "\nBlock of 64 houses under a barrage, fragment scale 4.0 (four grenades every step, Box3D step, update)\n" );
	BenchmarkTown( workerCount, 4.0f, 8, 4, 1, NULL, 60, NULL );

	printf( "\nCity streamed around a camera flying through it, fragment scale 4.0 (streaming, grenades, Box3D step, update)\n" );
	BenchmarkCity( workerCount );

	// Stage 6 of 27.14 of the game roadmap: figures in a town that is quiet and one under fire
	BenchmarkFigures( workerCount, true );

	// What each kind of joint of a figure costs
	BenchmarkJoints( workerCount );
	return 0;
}
