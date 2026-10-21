// SPDX-License-Identifier: MIT
// Nebenan demo: walls that break into polygon fragments on Box3D, built for speed. Flat shading, no shadows, no
// particles: every millisecond goes to the destruction.

#include "renderer.h"
#include "system_info.h"

#include "imgui.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_imgui.h"
#include "sokol_log.h"

#include "box3d/box3d.h"
#include "nebenan/nebenan.h"

#include <algorithm>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>
#include <vector>

extern "C" bool DemoSaveScreenshot( const char* path, int width, int height );
extern "C" bool DemoSetVsync( bool enabled );
extern "C" int DemoRefreshRate( void );
extern "C" bool DemoUnthrottled( void );

enum Tool
{
	ToolRifle,
	ToolGrenade,
	ToolCannon,
	ToolCount
};

enum SceneKind
{
	SceneWall,
	SceneStress,
	SceneTown,
	SceneCity,
	SceneCount
};

static const char* s_sceneNames[SceneCount] = { "Mauern", "Stresstest", "Stadt", "Große Stadt" };
static const char* s_toolNames[ToolCount] = { "Gewehr", "Granate", "Kanone" };

struct ToolSettings
{
	float radius;
	float damage;
	float ejectSpeed;
	int fragments;
	float rate;
};

struct ChunkVisual
{
	nbChunkId id = {};
	uint16_t generation = 0;
	int slot = -1;
	int mesh = -1;
	b3BodyId body = b3_nullBodyId;

	// Frame the mesh was built in. It already shows the faces exposed in that frame.
	int builtFrame = -1;
	bool alive = false;
};

struct Ball
{
	b3BodyId body;
	int slot;
	int mesh;
	float age;
};

// The slots of the chunks of a body, by the index of the body in Box3D, so the poses of a frame find them quickly, and
// the pose the body was last drawn at, for a chunk that comes to it
struct BodySlots
{
	uint16_t generation = 0;
	std::vector<int> slots;
	b3WorldTransform pose = {};
	bool posed = false;
};


// What the menu and the report show of the worlds and the city, read on the main thread while no job runs
struct WorldView
{
	nbStats stats = {};
	b3Counters counters = {};
	int awakeBodies = 0;
	int loadedHouses = 0;
	int damagedHouses = 0;
	float streamTime = 0.0f;
	int loads = 0;
	int unloads = 0;
	int saves = 0;
	int restores = 0;
	size_t savedBytes = 0;
};

// The town taken down house by house, see RunDemolition
struct Demolition
{
	// The house under fire, its passes, the next shot of the pass, the frames to wait before the house is checked, and
	// where it still stands for a pass after the first
	int house = 0;
	int pass = 0;
	int shot = 0;
	int wait = 0;
	std::vector<b3Vec3> targets;

	// Grenades so far, the frame the last house came down, -1 before, and the most debris moving at once
	int shots = 0;
	int doneFrame = -1;
	int maxMoving = 0;
};

// Parallel against serial physics on this PC with one click: the town taken down house by house, see RunDemolition,
// alternately with and without parallel physics, each with the threads the demo picks for it. Every run takes one step
// per frame like an automated run, so all do the same work and end in the same world. The same runs measure how the
// physics scales with the threads: the town taken down with 1, 2, 4 and so on threads, see nbGetScalingWorkerCounts, all
// with serial physics, and the profile of the library fitted over them, see nbFormatScalingReport.
struct Comparison
{
	enum Kind
	{
		ParallelAgainstSerial,
		Scaling,
	};

	// A first, shorter run warms up and does not count. Then come two runs with and two without parallel physics, with
	// first and last, so a slow drift of the machine falls on both alike. A counted run ends when the debris had time to
	// settle after the last house came down. The scaling warms up with the most threads and then counts a run for each
	// number of threads.
	static const int RunCount = 5;
	static const int MaxRuns = 9;
	static const int WarmupFrames = 300;
	static const int SettleFrames = 300;
	static const int MaxRunFrames = 4000;

	static bool IsParallel( int run )
	{
		return run == 0 || run == 1 || run == RunCount - 1;
	}

	Kind kind = ParallelAgainstSerial;
	int runCount = RunCount;

	// For the scaling: the threads of every run, and the profile of the library over it
	int runWorkers[MaxRuns] = {};
	nbProfile profiles[MaxRuns] = {};

	// The button asks for it, and it starts at the end of the frame, so its first run gets all its steps like the others
	bool requested = false;
	bool active = false;
	int run = 0;
	int frame = 0;

	// What to go back to afterwards
	SceneKind scene = SceneWall;
	Tool tool = ToolRifle;
	int maxDebrisBodies = 0;
	bool parallelPhysics = true;
	bool vsync = false;
	bool paused = false;
	bool slowMotion = false;

	// Per run: its threads and frames, the frame times, the CPU time of the simulation, the graphics and the wait for the
	// physics, the longest simulation and wait in a frame, and where the chunks ended up
	int threads[MaxRuns] = {};
	int frames[MaxRuns] = {};
	std::vector<float> frameTimes[MaxRuns];
	double simulationTime[MaxRuns] = {};
	double graphicsTime[MaxRuns] = {};
	double waitTime[MaxRuns] = {};
	float longestSimulation[MaxRuns] = {};
	float longestWait[MaxRuns] = {};
	uint64_t hashes[MaxRuns] = {};

	// What is left of the town at the end, and how the demolition went
	nbStats stats = {};
	Demolition demolition;

	// The report of the last comparison, and its short form for the menu
	std::string report;
	std::string summary;
};

// Whether a run of the comparison steps the physics on a thread of its own. The scaling runs all without.
static bool RunIsParallel( const Comparison& comparison, int run )
{
	return comparison.kind == Comparison::ParallelAgainstSerial && Comparison::IsParallel( run );
}

// Which run is on, for the menu and the corner
static void DescribeComparisonRun( const Comparison& comparison, char* text, size_t size )
{
	const char* kind = comparison.kind == Comparison::Scaling ? "Skalierung" : "Vergleich";
	if ( comparison.run == 0 )
	{
		snprintf( text, size, "%s, Aufwärmen", kind );
		return;
	}
	if ( comparison.kind == Comparison::Scaling )
	{
		snprintf( text, size, "%s, Lauf %d von %d, %d Threads", kind, comparison.run, comparison.runCount - 1,
				  comparison.runWorkers[comparison.run] );
		return;
	}
	snprintf( text, size, "%s, Lauf %d von %d, %s", kind, comparison.run, comparison.runCount - 1,
			  Comparison::IsParallel( comparison.run ) ? "parallel" : "nacheinander" );
}

struct Automation
{
	int frameLimit = 0;
	const char* screenshotPath = nullptr;
	bool script = false;
	int scene = -1;

	// Run the comparison or the scaling, print it and quit, or take the town down like the comparison does
	bool compare = false;
	bool scaling = false;
	bool demolition = false;

	// Threads for Box3D and Nebenan, zero for one per performance core
	int threads = 0;

	// Samples per pixel and whether to render at the full resolution of high density displays. Both cost fill rate.
	int msaa = 1;
	bool highDpi = false;

	// Per frame: time from frame to frame, CPU time of the simulation, of the main thread and of the graphics, the time
	// the main thread waited for the physics, bytes sent to the GPU, triangles drawn
	std::vector<float> realFrameTimes;
	std::vector<float> simulationTimes;
	std::vector<float> frameTimes;
	std::vector<float> graphicsTimes;
	std::vector<float> waitTimes;
	std::vector<float> uploadTimes;
	std::vector<float> uploadedBytes;
	std::vector<float> triangles;
};

// The real frame rate over the last two seconds: frames per second, the average frame time and the 1 % low, the
// rate at which the slowest 1 % of the frames come. The numbers change four times a second so they stay readable.
struct FpsMeter
{
	static const int Capacity = 1 << 14;
	float times[Capacity] = {};
	int first = 0;
	int count = 0;
	float windowTime = 0.0f;
	float sinceUpdate = 0.0f;
	std::vector<float> scratch;

	float fps = 0.0f;
	float frameTime = 0.0f;
	float lowFps = 0.0f;

	void Add( float milliseconds )
	{
		if ( count == Capacity )
		{
			windowTime -= times[first];
			first = ( first + 1 ) % Capacity;
			count -= 1;
		}
		times[( first + count ) % Capacity] = milliseconds;
		count += 1;
		windowTime += milliseconds;
		while ( count > 1 && windowTime - times[first] >= 2000.0f )
		{
			windowTime -= times[first];
			first = ( first + 1 ) % Capacity;
			count -= 1;
		}

		sinceUpdate += milliseconds;
		if ( sinceUpdate < 250.0f )
		{
			return;
		}
		sinceUpdate = 0.0f;

		scratch.resize( (size_t)count );
		float total = 0.0f;
		for ( int i = 0; i < count; ++i )
		{
			scratch[(size_t)i] = times[( first + i ) % Capacity];
			total += scratch[(size_t)i];
		}
		windowTime = total;
		frameTime = total / (float)count;
		fps = 1000.0f / b3MaxFloat( frameTime, 0.001f );

		size_t k = (size_t)count * 99 / 100;
		std::nth_element( scratch.begin(), scratch.begin() + (ptrdiff_t)k, scratch.end() );
		lowFps = 1000.0f / b3MaxFloat( scratch[k], 0.001f );
	}
};

struct PendingStructure;

// The large city, see BuildCityScene: the stream of the library that loads and unloads its houses, and the houses the
// stream builds right now, by site
struct City
{
	nbStreamId stream = {};
	std::vector<std::unique_ptr<PendingStructure>> building;
};

struct App
{
	b3WorldId physics = b3_nullWorldId;
	nbWorldId destruction = nb_nullWorldId;
	Renderer renderer;
	Camera camera = {};

	std::vector<ChunkVisual> chunks;
	std::vector<BodySlots> bodySlots;
	std::vector<Ball> balls;
	std::vector<GpuVertex> ballVertices;
	std::vector<uint16_t> ballIndices;
	std::vector<GpuVertex> vertexScratch;
	std::vector<uint16_t> indexScratch;
	int groundSlot = -1;
	int groundMesh = -1;

	SceneKind scene = SceneWall;
	Tool tool = ToolRifle;
	ToolSettings tools[ToolCount];

	bool paused = false;

	// Size of the fragments relative to the materials. The main lever of the cost of destruction: larger
	// fragments mean fewer pieces, bodies, contacts and meshes.
	float fragmentScale = 2.0f;

	// Debris bodies Box3D moves at the same time, the lever of the physics cost
	int maxDebrisBodies = nbDefaultWorldDef().maxDebrisBodies;

	// How long debris keeps still before it freezes into rubble
	float debrisRestTime = nbDefaultWorldDef().debrisRestTime;

	// How the storeys give way, the others and the top one, see nbStoreyCollapse: with "Zufall je Etage" every storey
	// picks its values from the ranges, without it every storey takes the values of the sliders
	bool storeyRandom = true;
	nbStoreyCollapse randomCollapse[2] = { nbDefaultWorldDef().storeyCollapse, nbDefaultWorldDef().topStoreyCollapse };
	nbStoreyCollapse fixedCollapse[2] = { { 0.24f, 0.24f, 1.0f, 1.0f }, { 0.22f, 0.22f, 1.0f, 1.0f } };

	// Load check for structures without floors, like the walls, off by default
	float supportScale = nbDefaultWorldDef().supportScale;

	// The large city and the radius around the camera within which its houses are loaded
	City city;
	float cityRadius = 100.0f;

	int workerCount = 1;

	// The threads a run of the scaling takes, zero otherwise, see PreferredWorkers
	int forcedWorkers = 0;

	bool keys[SAPP_MAX_KEYCODES] = {};
	bool mouseLook = false;
	bool mouseDown = false;
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	float fireCooldown = 0.0f;
	bool showUi = true;

	float physicsTime = 0.0f;
	float destructionTime = 0.0f;
	nbImpactResult lastImpact = {};

	// The physics on the thread of Nebenan while the frame is drawn, see nbWorldDef::asyncStep. The steps started last and
	// the time they simulate, whether they are not applied yet, the poses to draw the bodies at, and in this frame the CPU
	// time of the physics and how long the main thread waited for it.
	bool parallelPhysics = true;
	int physicsSteps = 0;
	float physicsStepTime = 0.0f;
	bool physicsPending = false;
	nbDrawPoses poses = {};

	// Draw the bodies between the poses before and after the last step, see nbWorld_GetDrawPoses, else at the poses after
	// it. Slow motion gives the physics a quarter of the time, so the difference shows on any monitor.
	bool interpolate = true;
	bool slowMotion = false;

	// Steps per second of the physics, chosen in the menu, see nbWorldDef::stepRate
	float stepRate = 60.0f;
	float simulationTime = 0.0f;
	float waitTime = 0.0f;
	WorldView view;
	Demolition demolition;
	Comparison comparison;

	// The houses of the town in the order of TownHousePosition, for the demolition
	std::vector<nbDestructibleId> townHouses;

	// Vertical sync is off, so the frame rate shows what the machine can do. Metal always syncs.
	bool vsync = false;
	bool vsyncAvailable = true;

	// Refresh rate of the monitor in Hz, zero if unknown, and whether frames may come faster than that
	int refreshRate = 0;
	bool unthrottled = true;

	// Time between two frames, with the wait for the display or the graphics card
	uint64_t frameTicks = 0;
	FpsMeter fpsMeter;

	Automation automation;
	int frame = 0;

	// The last frames for the performance panel: frame time, CPU time for impacts, simulation and graphics, and the wait
	// of the main thread for the physics
	static const int HistorySize = 300;
	float historyFrame[HistorySize] = {};
	float historyImpacts[HistorySize] = {};
	float historySimulation[HistorySize] = {};
	float historyGraphics[HistorySize] = {};
	float historyWait[HistorySize] = {};
	int historyCount = 0;
	int historyNext = 0;

	std::string cpuName;
	std::string gpuName;
	int performanceCores = 0;
	int copiedFrame = -1000;
};

#ifdef NDEBUG
static const char* s_buildType = "Release";
#else
static const char* s_buildType = "Debug";
#endif

static App* s_app = nullptr;

// The slots of a body, null if it has none. A body that took the index of one that went has none until it gets its own.
static BodySlots* FindBodySlots( App& app, b3BodyId body )
{
	size_t index = (size_t)( body.index1 - 1 );
	if ( index >= app.bodySlots.size() || app.bodySlots[index].generation != body.generation )
	{
		return nullptr;
	}
	return &app.bodySlots[index];
}

static void AttachSlot( App& app, b3BodyId body, int slot )
{
	size_t index = (size_t)( body.index1 - 1 );
	if ( index >= app.bodySlots.size() )
	{
		app.bodySlots.resize( index + 1 );
	}

	// The slots of the body that had the index before are detached or go to other bodies with the events
	BodySlots& entry = app.bodySlots[index];
	if ( entry.generation != body.generation )
	{
		entry.generation = body.generation;
		entry.slots.clear();
		entry.posed = false;
	}
	entry.slots.push_back( slot );

	b3WorldTransform transform = entry.posed ? entry.pose : b3Body_GetTransform( body );
	app.renderer.SetSlot( slot, b3ToVec3( transform.p ), transform.q );
}

static void DetachSlot( App& app, b3BodyId body, int slot )
{
	BodySlots* entry = FindBodySlots( app, body );
	if ( entry == nullptr )
	{
		return;
	}

	std::vector<int>& slots = entry->slots;
	for ( size_t i = 0; i < slots.size(); ++i )
	{
		if ( slots[i] == slot )
		{
			slots[i] = slots.back();
			slots.pop_back();
			break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Chunk visuals follow the destruction events

// Only the faces that can be seen: a face covered by the bonds to its neighbors lies inside the structure. Every
// face is a convex polygon with its own vertices for the flat normal, drawn as an indexed fan.
static void BuildChunkMesh( App& app, ChunkVisual& visual )
{
	nbGeometry geometry = nbChunk_GetGeometry( visual.id );
	bool visible[256];
	int faceCount = nbChunk_GetVisibleFaces( visual.id, visible, 256 );

	app.vertexScratch.clear();
	app.indexScratch.clear();
	for ( int f = 0; f < faceCount; ++f )
	{
		if ( visible[f] == false )
		{
			continue;
		}

		const nbFace& face = geometry.faces[f];
		uint16_t first = (uint16_t)app.vertexScratch.size();
		for ( int k = 0; k < face.indexCount; ++k )
		{
			b3Vec3 position = geometry.vertices[geometry.indices[face.firstIndex + k]];
			app.vertexScratch.push_back(
				Renderer::MakeVertex( position, geometry.planes[f].normal, (int)face.material, visual.slot ) );
		}
		for ( int k = 1; k + 1 < face.indexCount; ++k )
		{
			uint16_t triangle[3] = { first, (uint16_t)( first + k ), (uint16_t)( first + k + 1 ) };
			app.indexScratch.insert( app.indexScratch.end(), triangle, triangle + 3 );
		}
	}

	// A chunk inside the structure has no mesh until a neighbor goes
	visual.mesh = app.renderer.AddMesh( app.vertexScratch.data(), (int)app.vertexScratch.size(), app.indexScratch.data(),
										(int)app.indexScratch.size(), visual.slot );
	visual.builtFrame = app.frame;
}

static void AddChunkVisual( App& app, nbChunkId id )
{
	if ( nbChunk_GetGeometry( id ).faceCount == 0 )
	{
		return;
	}

	if ( (int)app.chunks.size() <= id.index1 )
	{
		app.chunks.resize( (size_t)id.index1 * 2 + 16 );
	}

	ChunkVisual& visual = app.chunks[id.index1];
	visual.id = id;
	visual.generation = id.generation;
	visual.slot = app.renderer.AllocSlot();
	visual.body = nbChunk_GetBody( id );
	visual.alive = true;
	AttachSlot( app, visual.body, visual.slot );
	BuildChunkMesh( app, visual );
}

// A chunk that lost a bond may show more faces. The old mesh stays on the GPU until its page is compacted, so it
// keeps the old slot, which is freed to hide it, and the new mesh gets a slot of its own.
static void RebuildChunkVisual( App& app, ChunkVisual& visual )
{
	if ( visual.builtFrame == app.frame )
	{
		return;
	}

	app.renderer.RemoveMesh( visual.mesh );
	DetachSlot( app, visual.body, visual.slot );
	app.renderer.FreeSlot( visual.slot );
	visual.slot = app.renderer.AllocSlot();
	AttachSlot( app, visual.body, visual.slot );
	BuildChunkMesh( app, visual );
}

static void RemoveChunkVisual( App& app, int index )
{
	ChunkVisual& visual = app.chunks[index];
	DetachSlot( app, visual.body, visual.slot );
	app.renderer.RemoveMesh( visual.mesh );
	app.renderer.FreeSlot( visual.slot );
	visual = ChunkVisual();
}

static void SyncChunks( App& app )
{
	nbEvents events = nbWorld_GetEvents( app.destruction );

	for ( int i = 0; i < events.destroyedCount; ++i )
	{
		nbChunkId id = events.destroyedChunks[i];
		if ( id.index1 < (int)app.chunks.size() && app.chunks[id.index1].alive && app.chunks[id.index1].generation == id.generation )
		{
			RemoveChunkVisual( app, id.index1 );
		}
	}

	for ( int i = 0; i < events.createdCount; ++i )
	{
		nbChunkId id = events.createdChunks[i];
		if ( nbChunk_IsValid( id ) )
		{
			AddChunkVisual( app, id );
		}
	}

	for ( int i = 0; i < events.movedCount; ++i )
	{
		nbChunkId id = events.movedChunks[i];
		if ( nbChunk_IsValid( id ) == false || id.index1 >= (int)app.chunks.size() )
		{
			continue;
		}

		ChunkVisual& visual = app.chunks[id.index1];
		if ( visual.alive == false || visual.generation != id.generation )
		{
			continue;
		}

		DetachSlot( app, visual.body, visual.slot );
		visual.body = nbChunk_GetBody( id );
		AttachSlot( app, visual.body, visual.slot );
	}

	for ( int i = 0; i < events.exposedCount; ++i )
	{
		nbChunkId id = events.exposedChunks[i];
		if ( nbChunk_IsValid( id ) && id.index1 < (int)app.chunks.size() )
		{
			ChunkVisual& visual = app.chunks[id.index1];
			if ( visual.alive && visual.generation == id.generation )
			{
				RebuildChunkVisual( app, visual );
			}
		}
	}
}


//----------------------------------------------------------------------------------------------------------------------
// Static meshes

static void BuildGround( App& app, float h )
{
	if ( app.groundMesh >= 0 )
	{
		app.renderer.RemoveMesh( app.groundMesh );
		app.renderer.FreeSlot( app.groundSlot );
	}

	app.groundSlot = app.renderer.AllocSlot();
	app.renderer.SetSlot( app.groundSlot, b3Vec3_zero, b3Quat_identity );

	b3Vec3 up = { 0.0f, 1.0f, 0.0f };
	GpuVertex vertices[4] = {
		Renderer::MakeVertex( { -h, 0.0f, h }, up, MaterialGround, app.groundSlot ),
		Renderer::MakeVertex( { h, 0.0f, h }, up, MaterialGround, app.groundSlot ),
		Renderer::MakeVertex( { h, 0.0f, -h }, up, MaterialGround, app.groundSlot ),
		Renderer::MakeVertex( { -h, 0.0f, -h }, up, MaterialGround, app.groundSlot ),
	};
	uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
	app.groundMesh = app.renderer.AddMesh( vertices, 4, indices, 6, app.groundSlot );
}

// Icosphere with smooth normals, built for slot zero and re-slotted per ball
static void BuildBallMesh( App& app, float radius )
{
	const float t = 1.6180339f;
	std::vector<b3Vec3> vertices = {
		{ -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 }, { 0, -1, t }, { 0, 1, t },
		{ 0, -1, -t }, { 0, 1, -t }, { t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 },
	};
	std::vector<int> triangles = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
								   3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1 };
	for ( b3Vec3& v : vertices )
	{
		v = b3Normalize( v );
	}

	for ( int level = 0; level < 2; ++level )
	{
		std::vector<int> refined;
		for ( size_t i = 0; i < triangles.size(); i += 3 )
		{
			int a = triangles[i], b = triangles[i + 1], c = triangles[i + 2];
			int ab = (int)vertices.size();
			vertices.push_back( b3Normalize( b3Add( vertices[a], vertices[b] ) ) );
			int bc = (int)vertices.size();
			vertices.push_back( b3Normalize( b3Add( vertices[b], vertices[c] ) ) );
			int ca = (int)vertices.size();
			vertices.push_back( b3Normalize( b3Add( vertices[c], vertices[a] ) ) );
			int next[12] = { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca };
			refined.insert( refined.end(), next, next + 12 );
		}
		triangles = refined;
	}

	// Shared midpoints are not merged, the ball is small enough either way
	app.ballVertices.clear();
	for ( b3Vec3 n : vertices )
	{
		app.ballVertices.push_back( Renderer::MakeVertex( b3MulSV( radius, n ), n, MaterialSteel, 0 ) );
	}
	app.ballIndices.clear();
	for ( int index : triangles )
	{
		app.ballIndices.push_back( (uint16_t)index );
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Scenes

struct MaterialPreset
{
	nbMaterial material;
	uint8_t surface;
	uint8_t interior;
};

static MaterialPreset BrickPreset()
{
	MaterialPreset preset;
	preset.material = nbDefaultMaterial();
	preset.material.density = 1900.0f;
	preset.material.strength = 6.0e5f;
	preset.material.fragmentSize = 0.1f;
	preset.material.friction = 0.8f;
	preset.surface = MaterialBrick;
	preset.interior = MaterialBrickInterior;
	return preset;
}

static MaterialPreset ConcretePreset()
{
	MaterialPreset preset;
	preset.material = nbDefaultMaterial();
	preset.material.density = 2400.0f;
	preset.material.strength = 1.1e6f;
	preset.material.fragmentSize = 0.13f;
	preset.surface = MaterialConcrete;
	preset.interior = MaterialConcreteInterior;
	return preset;
}

static nbPieceDef MakePiece( b3Vec3 center, b3Vec3 halfExtents, const MaterialPreset& preset, uint8_t surface )
{
	nbPieceDef piece = nbDefaultPieceDef();
	piece.halfExtents = halfExtents;
	piece.transform.p = center;
	piece.surfaceMaterial = surface;
	piece.interiorMaterial = preset.interior;
	return piece;
}

static nbDestructibleDef MakeStructureDef( b3Vec3 position, float yaw, const MaterialPreset& preset, uint32_t seed,
											float cellSize )
{
	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.cellSize = cellSize;
	def.position = b3ToPos( position );
	def.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisY, yaw );
	def.material = preset.material;
	def.seed = seed;
	return def;
}

static nbDestructibleId AddStructure( App& app, b3Vec3 position, float yaw, const MaterialPreset& preset,
									  const std::vector<nbPieceDef>& pieces, uint32_t seed, float cellSize = 0.0f )
{
	nbDestructibleDef def = MakeStructureDef( position, yaw, preset, seed, cellSize );
	return nbCreateDestructible( app.destruction, &def, pieces.data(), (int)pieces.size() );
}

// A structure waiting to be created, with everything its pieces point to
struct PendingStructure
{
	nbDestructibleDef def;
	std::vector<nbPieceDef> pieces;
	std::vector<nbOpening> openings[4];
	nbMaterial floorMaterial;
};

// Create the structures at once: while the calling thread builds one into the world, the workers already fracture the
// next, see nbCreateDestructibles
static std::vector<nbDestructibleId> CreateStructures( App& app, const std::vector<std::unique_ptr<PendingStructure>>& pending )
{
	std::vector<nbDestructibleDef> defs;
	std::vector<const nbPieceDef*> pieceLists;
	std::vector<int> pieceCounts;
	for ( const std::unique_ptr<PendingStructure>& structure : pending )
	{
		defs.push_back( structure->def );
		pieceLists.push_back( structure->pieces.data() );
		pieceCounts.push_back( (int)structure->pieces.size() );
	}

	std::vector<nbDestructibleId> ids( pending.size() );
	nbCreateDestructibles( app.destruction, defs.data(), pieceLists.data(), pieceCounts.data(), (int)pending.size(), ids.data() );
	return ids;
}

static void AddWall( App& app, b3Vec3 base, float yaw, b3Vec3 size, const MaterialPreset& preset, uint32_t seed )
{
	std::vector<nbPieceDef> pieces = { MakePiece( { 0.0f, 0.5f * size.y, 0.0f }, b3MulSV( 0.5f, size ), preset, preset.surface ) };
	AddStructure( app, base, yaw, preset, pieces, seed );
}

// Cut the doors and windows of one floor out of a wall that runs through all floors. The corners are along the wall
// and up from the floor. A door reaches below the wall, so it opens it to the ground.
static void AddOpenings( std::vector<nbOpening>& openings, const std::vector<b3Vec2>& corners, float length, float height,
						 float floorY, float thickness )
{
	for ( size_t i = 0; i + 1 < corners.size(); i += 2 )
	{
		b3Vec2 lower = corners[i], upper = corners[i + 1];
		float y0 = lower.y > 0.0f ? floorY + lower.y : floorY - 0.1f;
		float y1 = floorY + upper.y;
		nbOpening opening;
		opening.center = { 0.5f * ( lower.x + upper.x - length ), 0.5f * ( y0 + y1 - height ), 0.0f };
		opening.halfExtents = { 0.5f * ( upper.x - lower.x ), 0.5f * ( y1 - y0 ), thickness };
		openings.push_back( opening );
	}
}

static void BuildWallScene( App& app )
{
	MaterialPreset brick = BrickPreset();
	MaterialPreset concrete = ConcretePreset();
	AddWall( app, { 0.0f, 0.0f, 0.0f }, 0.0f, { 9.0f, 3.2f, 0.3f }, brick, 1 );
	AddWall( app, { 0.0f, 0.0f, -6.0f }, 0.0f, { 11.0f, 4.5f, 0.4f }, concrete, 2 );
	AddWall( app, { -6.5f, 0.0f, -2.5f }, 0.5f * B3_PI, { 5.0f, 2.2f, 0.25f }, brick, 3 );

	app.camera.position = { 0.0f, 1.8f, 9.0f };
	app.camera.yaw = 0.0f;
	app.camera.pitch = -0.05f;
}

// Brick walls through all floors with their doors and windows cut out, and concrete floors inside them. The walls are
// pre-fractured into cells, so a wall that gives way cracks along the cells and not along the floors.
static std::unique_ptr<PendingStructure> PrepareHouse( b3Vec3 position, float yaw, int floors, uint32_t seed )
{
	std::unique_ptr<PendingStructure> house = std::make_unique<PendingStructure>();
	MaterialPreset brick = BrickPreset();
	MaterialPreset concrete = ConcretePreset();

	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;
	float level = story + slab;
	float height = (float)floors * level;
	float inner = depth - 2.0f * t;

	std::vector<b3Vec2> front0 = { { 3.9f, 0.0f }, { 5.1f, 2.2f }, { 1.0f, 1.0f },
								   { 2.6f, 2.2f }, { 6.4f, 1.0f }, { 8.0f, 2.2f } };
	std::vector<b3Vec2> front1 = { { 1.0f, 0.9f }, { 2.6f, 2.2f }, { 3.7f, 0.9f },
								   { 5.3f, 2.2f }, { 6.4f, 0.9f }, { 8.0f, 2.2f } };
	std::vector<b3Vec2> back = { { 1.5f, 0.9f }, { 3.0f, 2.2f }, { 6.0f, 0.9f }, { 7.5f, 2.2f } };
	std::vector<b3Vec2> side = { { 2.4f, 0.9f }, { 3.6f, 2.2f } };

	std::vector<nbOpening>* openings = house->openings;
	for ( int floor = 0; floor < floors; ++floor )
	{
		float y = (float)floor * level;
		AddOpenings( openings[0], floor == 0 ? front0 : front1, width, height, y, t );
		AddOpenings( openings[1], back, width, height, y, t );
		AddOpenings( openings[2], side, inner, height, y, t );
		AddOpenings( openings[3], side, inner, height, y, t );
	}

	// The front and back walls run along x, the end walls between them along z
	b3Quat alongZ = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
	b3Vec3 centers[4] = {
		{ 0.0f, 0.5f * height, 0.5f * ( depth - t ) },
		{ 0.0f, 0.5f * height, -0.5f * ( depth - t ) },
		{ -0.5f * ( width - t ), 0.5f * height, 0.0f },
		{ 0.5f * ( width - t ), 0.5f * height, 0.0f },
	};

	std::vector<nbPieceDef>& pieces = house->pieces;
	for ( int w = 0; w < 4; ++w )
	{
		float length = w < 2 ? width : inner;
		nbPieceDef wall = MakePiece( centers[w], { 0.5f * length, 0.5f * height, 0.5f * t }, brick, MaterialPlaster );
		wall.transform.q = w < 2 ? b3Quat_identity : alongZ;
		wall.openings = openings[w].data();
		wall.openingCount = (int)openings[w].size();
		pieces.push_back( wall );
	}

	// The floors stay whole until something hits them, cells would only cost
	house->floorMaterial = concrete.material;
	for ( int floor = 0; floor < floors; ++floor )
	{
		nbPieceDef floorSlab = MakePiece( { 0.0f, (float)floor * level + story + 0.5f * slab, 0.0f },
										  { 0.5f * width - t, 0.5f * slab, 0.5f * depth - t }, concrete, MaterialConcrete );
		floorSlab.material = &house->floorMaterial;
		floorSlab.cellSize = -1.0f;
		pieces.push_back( floorSlab );
	}

	house->def = MakeStructureDef( position, yaw, brick, seed, 1.2f );
	return house;
}

static void BuildStressScene( App& app )
{
	MaterialPreset brick = BrickPreset();
	MaterialPreset concrete = ConcretePreset();
	uint32_t seed = 100;
	for ( int row = 0; row < 4; ++row )
	{
		for ( int column = 0; column < 4; ++column )
		{
			b3Vec3 base = { ( (float)column - 1.5f ) * 5.5f, 0.0f, -(float)row * 4.0f };
			AddWall( app, base, 0.0f, { 4.5f, 3.0f, 0.3f }, ( row + column ) & 1 ? concrete : brick, seed++ );
		}
	}

	app.camera.position = { 0.0f, 4.0f, 12.0f };
	app.camera.yaw = 0.0f;
	app.camera.pitch = -0.18f;
}

// A town of houses along two streets, to wreck as fast as you can
static const int TownRows = 4;
static const int TownColumns = 5;

static b3Vec3 TownHousePosition( int row, int column )
{
	return { 14.0f * ( (float)column - 0.5f * (float)( TownColumns - 1 ) ), 0.0f, -12.0f * (float)row };
}

// Every third house has a third floor
static int TownHouseFloors( int house )
{
	return house % 3 == 1 ? 3 : 2;
}

static void BuildTownScene( App& app )
{
	std::vector<std::unique_ptr<PendingStructure>> houses;
	uint32_t seed = 200;
	for ( int row = 0; row < TownRows; ++row )
	{
		for ( int column = 0; column < TownColumns; ++column )
		{
			// Every other row faces the other street
			int floors = TownHouseFloors( row * TownColumns + column );
			float yaw = ( row & 1 ) != 0 ? B3_PI : 0.0f;
			houses.push_back( PrepareHouse( TownHousePosition( row, column ), yaw, floors, seed++ ) );
		}
	}
	app.townHouses = CreateStructures( app, houses );

	app.camera.position = { 0.0f, 16.0f, 34.0f };
	app.camera.yaw = 0.0f;
	app.camera.pitch = -0.38f;
}

// The large city: 256 x 256 houses like the ones of the town on 3.6 x 3.1 km, 65 536 of them. Held at once they would
// take some 24 GB, so the stream of the library keeps only the houses within the load radius of the camera and the ones
// near anything that moves, see nbCreateStream. Damaged houses at rest go with their rubble and come back to the bit as
// they were.
static const int CitySide = 256;

static b3Vec3 CityHousePosition( int index )
{
	float half = 0.5f * (float)( CitySide - 1 );
	return { 14.0f * ( (float)( index % CitySide ) - half ), 0.0f, 12.0f * ( (float)( index / CitySide ) - half ) };
}

// Every third house has a third floor
static int CityHouseFloors( int index )
{
	return index % 3 == 1 ? 3 : 2;
}

// Every other row faces the other street
static std::unique_ptr<PendingStructure> PrepareCityHouse( int index )
{
	float yaw = ( ( index / CitySide ) & 1 ) != 0 ? B3_PI : 0.0f;
	return PrepareHouse( CityHousePosition( index ), yaw, CityHouseFloors( index ), (uint32_t)( 1000 + index ) );
}

// The stream asks for a house as it loads it, on the thread of the steps, and gives it back once it is built in
static void PrepareCitySite( int site, nbSiteDef* siteDef, void* context )
{
	City& city = *(City*)context;
	std::unique_ptr<PendingStructure>& house = city.building[(size_t)site];
	house = PrepareCityHouse( site );
	siteDef->def = house->def;
	siteDef->pieces = house->pieces.data();
	siteDef->pieceCount = (int)house->pieces.size();
}

static void ReleaseCitySite( int site, void* context )
{
	City& city = *(City*)context;
	city.building[(size_t)site].reset();
}

static void BuildCityScene( App& app )
{
	City& city = app.city;
	city = City();
	city.building.resize( (size_t)CitySide * CitySide );

	nbStreamDef def = nbDefaultStreamDef();
	def.loadRadius = app.cityRadius;
	def.prepareFcn = PrepareCitySite;
	def.releaseFcn = ReleaseCitySite;
	def.userContext = &city;
	city.stream = nbCreateStream( app.destruction, &def );
	for ( int index = 0; index < CitySide * CitySide; ++index )
	{
		// 9 x 6.5 m, 3.25 m per storey
		b3Vec3 p = CityHousePosition( index );
		float height = 3.25f * (float)CityHouseFloors( index );
		nbStream_AddSite( city.stream, { { p.x - 4.5f, 0.0f, p.z - 3.25f }, { p.x + 4.5f, height, p.z + 3.25f } } );
	}

	// On a street between two rows, looking along it, with the houses around at once
	app.camera.position = { 0.0f, 2.5f, 0.0f };
	app.camera.yaw = 0.5f * B3_PI;
	app.camera.pitch = -0.05f;
	nbStream_LoadAround( city.stream, &app.camera.position, 1 );
}

static void DestroyScene( App& app )
{
	// The worlds wait for their last steps by themselves, and their poses go with them
	app.poses = {};
	app.physicsPending = false;
	app.physicsStepTime = 0.0f;

	if ( nbWorld_IsValid( app.destruction ) )
	{
		nbDestroyWorld( app.destruction );
	}
	if ( b3World_IsValid( app.physics ) )
	{
		b3DestroyWorld( app.physics );
	}

	for ( size_t i = 0; i < app.chunks.size(); ++i )
	{
		if ( app.chunks[i].alive )
		{
			app.renderer.RemoveMesh( app.chunks[i].mesh );
			app.renderer.FreeSlot( app.chunks[i].slot );
		}
	}
	app.chunks.clear();

	for ( Ball& ball : app.balls )
	{
		app.renderer.RemoveMesh( ball.mesh );
		app.renderer.FreeSlot( ball.slot );
	}
	app.balls.clear();
	app.bodySlots.clear();
	app.lastImpact = {};
	app.city = City();
	app.townHouses.clear();
}

// The threads a game would take on this machine, see nbGetRecommendedWorkerCount, unless the command line or a run of the
// scaling sets them
static int PreferredWorkers( const App& app, bool parallelPhysics )
{
	if ( app.forcedWorkers > 0 )
	{
		return app.forcedWorkers;
	}
	return app.automation.threads > 0 ? app.automation.threads : nbGetRecommendedWorkerCount( parallelPhysics );
}

// How every storey gives way: the ranges it picks from with "Zufall je Etage", the values of the sliders without
static const nbStoreyCollapse* ActiveCollapse( const App& app )
{
	return app.storeyRandom ? app.randomCollapse : app.fixedCollapse;
}

static void LoadScene( App& app, SceneKind scene )
{
	DestroyScene( app );
	app.scene = scene;
	app.workerCount = PreferredWorkers( app, app.parallelPhysics );

	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)app.workerCount;
	app.physics = b3CreateWorld( &worldDef );

	// The ground of the large city reaches 100 m beyond its houses
	float groundSize = scene == SceneCity ? 7.0f * (float)CitySide + 100.0f : 150.0f;
	BuildGround( app, groundSize );
	b3BodyDef groundDef = b3DefaultBodyDef();
	groundDef.position = b3ToPos( { 0.0f, -1.0f, 0.0f } );
	b3BodyId ground = b3CreateBody( app.physics, &groundDef );
	b3BoxHull box = b3MakeBoxHull( groundSize, 1.0f, groundSize );
	b3ShapeDef groundShape = b3DefaultShapeDef();
	groundShape.baseMaterial.friction = 0.8f;
	b3CreateHullShape( ground, &groundShape, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = app.physics;
	def.collisionRadiusScale = 0.02f;
	def.workerCount = app.workerCount;
	def.maxDebrisBodies = app.maxDebrisBodies;
	def.debrisRestTime = app.debrisRestTime;
	def.fragmentScale = app.fragmentScale;
	def.supportScale = app.supportScale;
	def.storeyCollapse = ActiveCollapse( app )[0];
	def.topStoreyCollapse = ActiveCollapse( app )[1];
	def.asyncStep = app.parallelPhysics;
	def.stepRate = app.stepRate;
	app.destruction = nbCreateWorld( &def );
	app.stepRate = nbWorld_GetStepRate( app.destruction );

	switch ( scene )
	{
		case SceneWall:
			BuildWallScene( app );
			break;
		case SceneStress:
			BuildStressScene( app );
			break;
		case SceneCity:
			BuildCityScene( app );
			break;
		default:
			BuildTownScene( app );
			break;
	}

	SyncChunks( app );
}

//----------------------------------------------------------------------------------------------------------------------
// Tools

static void AimRay( const App& app, float screenX, float screenY, b3Vec3* origin, b3Vec3* direction )
{
	float width = sapp_widthf();
	float height = sapp_heightf();
	float ndcX = 2.0f * screenX / width - 1.0f;
	float ndcY = 1.0f - 2.0f * screenY / height;
	float tanY = tanf( 0.5f * app.camera.fovY );
	float tanX = tanY * width / height;

	*origin = app.camera.position;
	b3Vec3 d = app.camera.Forward();
	d = b3MulAdd( d, ndcX * tanX, app.camera.Right() );
	d = b3MulAdd( d, ndcY * tanY, app.camera.Up() );
	*direction = b3Normalize( d );
}

static nbImpactDef MakeImpact( const ToolSettings& settings )
{
	nbImpactDef impact = {};
	impact.radius = settings.radius;
	impact.damage = settings.damage;
	impact.ejectSpeed = settings.ejectSpeed;
	impact.fragmentCount = settings.fragments;
	return impact;
}

// Grenade explosion at a point: damage to the structures around it and a push for all bodies nearby
static void Detonate( App& app, b3Pos point )
{
	const ToolSettings& settings = app.tools[ToolGrenade];
	nbImpactDef impact = MakeImpact( settings );
	impact.point = point;
	impact.direction = b3Vec3_zero;
	impact.explosionImpulse = 40.0f * settings.ejectSpeed;
	app.lastImpact = nbWorld_ApplyImpact( app.destruction, &impact );
}

static void FireRay( App& app, b3Vec3 origin, b3Vec3 direction )
{
	const ToolSettings& settings = app.tools[app.tool];

	if ( app.tool == ToolRifle )
	{
		nbImpactDef impact = MakeImpact( settings );
		nbImpactResult result;
		if ( nbWorld_CastImpact( app.destruction, b3ToPos( origin ), b3MulSV( 250.0f, direction ), &impact, &result ) )
		{
			app.lastImpact = result;
		}
		else
		{
			// Push anything else that was hit
			b3RayResult ray = b3World_CastRayClosest( app.physics, b3ToPos( origin ), b3MulSV( 250.0f, direction ),
													  b3DefaultQueryFilter() );
			if ( ray.hit )
			{
				b3BodyId body = b3Shape_GetBody( ray.shapeId );
				if ( b3Body_GetType( body ) == b3_dynamicBody )
				{
					b3Body_ApplyLinearImpulse( body, b3MulSV( 60.0f, direction ), ray.point, true );
				}
			}
		}
	}
	else if ( app.tool == ToolGrenade )
	{
		b3RayResult ray =
			b3World_CastRayClosest( app.physics, b3ToPos( origin ), b3MulSV( 250.0f, direction ), b3DefaultQueryFilter() );
		if ( ray.hit )
		{
			// Detonate slightly in front of the surface
			Detonate( app, b3OffsetPos( ray.point, b3MulSV( 0.15f, ray.normal ) ) );
		}
	}
	else
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = b3ToPos( b3MulAdd( origin, 0.8f, direction ) );
		bodyDef.linearVelocity = b3MulSV( settings.ejectSpeed, direction );
		bodyDef.isBullet = true;
		b3BodyId body = b3CreateBody( app.physics, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 7800.0f;
		shapeDef.enableHitEvents = true;
		shapeDef.baseMaterial.friction = 0.6f;
		b3Sphere sphere = { b3Vec3_zero, settings.radius };
		b3CreateSphereShape( body, &shapeDef, &sphere );

		BuildBallMesh( app, settings.radius );
		Ball ball;
		ball.body = body;
		ball.slot = app.renderer.AllocSlot();
		for ( GpuVertex& v : app.ballVertices )
		{
			v.slot = (float)ball.slot;
		}
		ball.mesh = app.renderer.AddMesh( app.ballVertices.data(), (int)app.ballVertices.size(), app.ballIndices.data(),
										  (int)app.ballIndices.size(), ball.slot );
		ball.age = 0.0f;
		AttachSlot( app, body, ball.slot );
		app.balls.push_back( ball );
	}
}

static void Fire( App& app, float screenX, float screenY )
{
	b3Vec3 origin, direction;
	AimRay( app, screenX, screenY, &origin, &direction );
	FireRay( app, origin, direction );
}

static void FireAt( App& app, Tool tool, b3Vec3 target )
{
	app.tool = tool;
	if ( tool == ToolGrenade )
	{
		// A scripted grenade goes off at its target, whatever debris flies through the line of sight
		Detonate( app, b3ToPos( target ) );
		return;
	}
	FireRay( app, app.camera.position, b3Normalize( b3Sub( target, app.camera.position ) ) );
}

// A ball goes after 20 seconds or when there are too many
static void UpdateBalls( App& app, float dt )
{
	for ( size_t i = 0; i < app.balls.size(); )
	{
		Ball& ball = app.balls[i];
		ball.age += dt;
		if ( ball.age > 20.0f || app.balls.size() > 24 )
		{
			b3DestroyBody( ball.body );
			DetachSlot( app, ball.body, ball.slot );
			app.renderer.RemoveMesh( ball.mesh );
			app.renderer.FreeSlot( ball.slot );
			app.balls[i] = app.balls.back();
			app.balls.pop_back();
			continue;
		}
		++i;
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Simulation

// Start the steps this frame needs at the step rate of the world, see nbWorld_Advance. With parallel physics they run on
// the thread of Nebenan while the frame is drawn, else right here. Either way the main thread does not touch the worlds
// again before it waits for them, see FinishPhysics.
static void StartPhysics( App& app, float frameDt )
{
	// The balls age by the time the steps of the frame before simulated, which are done by now
	UpdateBalls( app, app.physicsStepTime );
	float physicsDt = app.slowMotion ? 0.25f * frameDt : frameDt;
	int steps = app.paused ? 0 : nbWorld_Advance( app.destruction, physicsDt );
	app.physicsSteps = steps;
	app.physicsStepTime = (float)steps / app.stepRate;
	app.physicsPending = steps > 0 || app.scene == SceneCity;

	// The city streams after the steps on their thread, also while the simulation pauses
	if ( app.scene == SceneCity )
	{
		nbStream_Update( app.city.stream, &app.camera.position, 1 );
	}
}

// Wait for the steps of the frame before and take the poses to draw the bodies at. Everything that touches the worlds or
// the city on the main thread comes after this, or waits for the steps by itself.
static void FinishPhysics( App& app )
{
	uint64_t ticks = b3GetTicks();
	float fraction = app.interpolate ? nbWorld_GetStepFraction( app.destruction ) : 1.0f;
	app.poses = nbWorld_GetDrawPoses( app.destruction, fraction );
	app.waitTime += b3GetMilliseconds( ticks );
}

// Hand the poses of the bodies to the renderer
static void ApplyPhysics( App& app )
{
	for ( int i = 0; i < app.poses.count; ++i )
	{
		const nbDrawPose& pose = app.poses.poses[i];
		BodySlots* entry = FindBodySlots( app, pose.bodyId );
		if ( entry == nullptr )
		{
			continue;
		}

		entry->pose = pose.transform;
		entry->posed = true;
		b3Vec3 position = b3ToVec3( pose.transform.p );
		for ( int slot : entry->slots )
		{
			app.renderer.SetSlot( slot, position, pose.transform.q );
		}
	}
	app.poses = {};

	// The CPU time of the steps with their updates, and of the streaming after them
	if ( app.physicsPending == false )
	{
		return;
	}
	app.physicsPending = false;

	nbStats stats = nbWorld_GetStats( app.destruction );
	if ( app.physicsSteps > 0 )
	{
		app.physicsTime = stats.stepTime;
		app.destructionTime = stats.updateTime;
	}
	app.simulationTime += (float)app.physicsSteps * ( stats.stepTime + stats.updateTime );
	app.simulationTime += app.scene == SceneCity ? nbStream_GetStats( app.city.stream ).updateTime : 0.0f;
}

// What the menu shows of the worlds, copied while no job runs
static void TakeWorldView( App& app )
{
	WorldView& view = app.view;
	view.stats = nbWorld_GetStats( app.destruction );
	view.counters = b3World_GetCounters( app.physics );
	view.awakeBodies = b3World_GetAwakeBodyCount( app.physics );

	if ( app.scene == SceneCity )
	{
		nbStreamStats stream = nbStream_GetStats( app.city.stream );
		view.loadedHouses = stream.loadedCount;
		view.damagedHouses = stream.damagedCount;
		view.streamTime = stream.updateTime + stream.markTime;
		view.loads = stream.loads;
		view.unloads = stream.unloads;
		view.saves = stream.saves;
		view.restores = stream.restores;
		view.savedBytes = stream.savedBytes;
	}
}

static void UpdateCamera( App& app, float dt )
{
	if ( ImGui::GetIO().WantCaptureKeyboard )
	{
		return;
	}

	float speed = app.keys[SAPP_KEYCODE_LEFT_SHIFT] ? 16.0f : 6.0f;
	b3Vec3 forward = app.camera.Forward();
	b3Vec3 flat = b3Normalize( b3Vec3{ forward.x, 0.0f, forward.z } );
	b3Vec3 right = app.camera.Right();
	b3Vec3 move = b3Vec3_zero;

	if ( app.keys[SAPP_KEYCODE_W] )
	{
		move = b3Add( move, flat );
	}
	if ( app.keys[SAPP_KEYCODE_S] )
	{
		move = b3Sub( move, flat );
	}
	if ( app.keys[SAPP_KEYCODE_D] )
	{
		move = b3Add( move, right );
	}
	if ( app.keys[SAPP_KEYCODE_A] )
	{
		move = b3Sub( move, right );
	}
	if ( app.keys[SAPP_KEYCODE_E] || app.keys[SAPP_KEYCODE_SPACE] )
	{
		move.y += 1.0f;
	}
	if ( app.keys[SAPP_KEYCODE_Q] || app.keys[SAPP_KEYCODE_LEFT_CONTROL] )
	{
		move.y -= 1.0f;
	}

	if ( b3LengthSquared( move ) > 0.0f )
	{
		app.camera.position = b3MulAdd( app.camera.position, speed * dt, b3Normalize( move ) );
		app.camera.position.y = b3MaxFloat( app.camera.position.y, 0.3f );
	}
}

//----------------------------------------------------------------------------------------------------------------------
// User interface

// Average and maximum over the frame history
static void HistoryStats( const App& app, const float* values, float* average, float* maximum )
{
	float total = 0.0f;
	float peak = 0.0f;
	for ( int i = 0; i < app.historyCount; ++i )
	{
		total += values[i];
		peak = b3MaxFloat( peak, values[i] );
	}
	*average = app.historyCount > 0 ? total / (float)app.historyCount : 0.0f;
	*maximum = peak;
}

static void Appendf( std::string& text, const char* format, ... )
{
	char buffer[512];
	va_list args;
	va_start( args, format );
	vsnprintf( buffer, sizeof( buffer ), format, args );
	va_end( args );
	text += buffer;
}

// The step rate of the physics for the reports, like "30 Hz mit 8 Teilschritten"
// A fraction in percent, or a range of them
static void AppendRange( std::string& text, float low, float high )
{
	if ( low == high )
	{
		Appendf( text, "%.0f %%", 100.0f * low );
	}
	else
	{
		Appendf( text, "%.0f bis %.0f %%", 100.0f * low, 100.0f * high );
	}
}

static void AppendStepRate( std::string& text, const App& app )
{
	int subStepCount = nbGetSubStepCount( app.stepRate );
	Appendf( text, "%.0f Hz mit %d %s", app.stepRate, subStepCount, subStepCount == 1 ? "Teilschritt" : "Teilschritten" );
}

// Everything it takes to tell where the time of a frame goes, to paste into a message
static std::string BuildReport( const App& app )
{
	const WorldView& view = app.view;
	const nbStats& stats = view.stats;
	RenderStats rs = app.renderer.GetStats();
	b3Version version = b3GetVersion();

	float frameAvg, frameMax, impactAvg, impactMax, simulationAvg, simulationMax, graphicsAvg, graphicsMax, waitAvg, waitMax;
	HistoryStats( app, app.historyFrame, &frameAvg, &frameMax );
	HistoryStats( app, app.historyImpacts, &impactAvg, &impactMax );
	HistoryStats( app, app.historySimulation, &simulationAvg, &simulationMax );
	HistoryStats( app, app.historyGraphics, &graphicsAvg, &graphicsMax );
	HistoryStats( app, app.historyWait, &waitAvg, &waitMax );

	std::string text;
	Appendf( text, "Nebenan-Messung: Szene %s, %s-Build, Box3D %d.%d.%d\n", s_sceneNames[app.scene], s_buildType, version.major,
			 version.minor, version.revision );
	Appendf( text, "CPU: %s, %d logische Kerne, %d Performance-Kerne, %d Threads, Löser mit %d Spuren\n",
			 app.cpuName.empty() ? "unbekannt" : app.cpuName.c_str(), nbGetHardwareThreadCount(), app.performanceCores,
			 app.workerCount, b3GetSIMDWidth() );
	Appendf( text, "GPU: %s, Bild %d x %d, MSAA %dx, Monitor %d Hz\n", app.gpuName.empty() ? "unbekannt" : app.gpuName.c_str(),
			 sapp_width(), sapp_height(), sapp_sample_count(), app.refreshRate );
	Appendf( text, "Bild: %.0f FPS, 1%% Low %.0f FPS, Mittel %.2f ms, Spitze %.1f ms (letzte %d Bilder), VSync %s, ungebremst %s\n",
			 app.fpsMeter.fps, app.fpsMeter.lowFps, app.fpsMeter.frameTime, frameMax, app.historyCount, app.vsync ? "an" : "aus",
			 app.unthrottled ? "ja" : "nein" );
	Appendf( text, "CPU pro Bild, Mittel / Spitze: Einschläge %.1f / %.1f, Simulation %.1f / %.1f, Grafik %.1f / %.1f ms\n", impactAvg,
			 impactMax, simulationAvg, simulationMax, graphicsAvg, graphicsMax );
	if ( app.parallelPhysics )
	{
		Appendf( text, "Physik parallel zum Zeichnen: an, der Hauptthread wartet auf sie %.1f / %.1f ms\n", waitAvg, waitMax );
	}
	else
	{
		Appendf( text, "Physik parallel zum Zeichnen: aus\n" );
	}
	Appendf( text, "Physiktakt " );
	AppendStepRate( text, app );
	Appendf( text, ", pro Schritt: Box3D %.2f ms, Zerstörung %.2f ms\n", app.physicsTime, app.destructionTime );
	Appendf( text, "Welt: %d Bruchstücke, %d Verbindungen, %d Trümmerkörper (%d wach), %d Schutt, %d Kontakte, Budget %d, Ruhezeit %.2f s, Bruchstückgröße x%.2f\n",
			 stats.chunkCount, stats.bondCount, stats.dynamicBodyCount, view.awakeBodies, stats.rubbleCount,
			 view.counters.contactCount, app.maxDebrisBodies, app.debrisRestTime, app.fragmentScale );
	const nbStoreyCollapse* collapse = ActiveCollapse( app );
	Appendf( text, "Einsturz: %d Etagen eingestürzt, %s, eine Etage gibt nach bei ", stats.collapsedStoreyCount,
			 app.storeyRandom ? "Zufall je Etage" : "feste Werte" );
	AppendRange( text, collapse[0].damageMin, collapse[0].damageMax );
	Appendf( text, " zerstört, es bleiben " );
	AppendRange( text, collapse[0].residueMin, collapse[0].residueMax );
	Appendf( text, " der übrigen, die oberste bei " );
	AppendRange( text, collapse[1].damageMin, collapse[1].damageMax );
	Appendf( text, ", es bleiben " );
	AppendRange( text, collapse[1].residueMin, collapse[1].residueMax );
	Appendf( text, "; Statik ohne Decken x%.2f, %d Verbindungen gebrochen\n", app.supportScale, stats.overloadedBondCount );
	Appendf( text, "Grafik: %dk Dreiecke, %d Draw Calls, %d kB Upload\n", rs.triangleCount / 1000, rs.drawCalls, rs.uploadedBytes / 1024 );
	if ( app.scene == SceneCity )
	{
		Appendf(
			text,
			"Große Stadt: %d von %d Häusern geladen, Laderadius %.0f m, Streaming %.2f ms, %d geladen, %d entladen, %.0f MB\n",
			view.loadedHouses, CitySide * CitySide, app.cityRadius, view.streamTime, view.loads, view.unloads,
			(double)( stats.byteCount + view.counters.byteCount ) / 1048576.0 );
		Appendf( text, "Beschädigte Häuser: %d gespeichert, %d zurückgeholt, %.0f MB gespeichert\n", view.saves, view.restores,
				 (double)view.savedBytes / 1048576.0 );
	}
	return text;
}

static void StopComparison( App& app );

static void DrawUi( App& app )
{
	float dpi = sapp_dpi_scale();
	ImGui::SetNextWindowPos( ImVec2( 12.0f * dpi, 12.0f * dpi ), ImGuiCond_FirstUseEver );
	ImGui::SetNextWindowSize( ImVec2( 330.0f * dpi, 0.0f ), ImGuiCond_FirstUseEver );
	ImGui::Begin( "Nebenan - Zerstörung", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

	b3Version version = b3GetVersion();
	ImGui::TextDisabled( "Box3D %d.%d.%d  |  %s", version.major, version.minor, version.revision, s_buildType );
#ifndef NDEBUG
	ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.3f, 1.0f ), "Debug-Build: 5- bis 20-mal langsamer.\nFür echte Leistung Release bauen (build.bat)." );
#endif

	// The comparison keeps everything as it is while it runs
	ImGui::BeginDisabled( app.comparison.active );
	ImGui::SeparatorText( "Szene" );
	int scene = (int)app.scene;
	if ( ImGui::Combo( "##scene", &scene, s_sceneNames, SceneCount ) )
	{
		LoadScene( app, (SceneKind)scene );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Neu laden (R)" ) )
	{
		LoadScene( app, app.scene );
	}
	if ( app.scene == SceneCity )
	{
		if ( ImGui::SliderFloat( "Laderadius", &app.cityRadius, 40.0f, 250.0f, "%.0f m" ) )
		{
			nbStream_SetLoadRadius( app.city.stream, app.cityRadius );
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip(
				"Häuser in diesem Umkreis der Kamera sind geladen, dazu alle in der Nähe von\nallem, was sich bewegt. Häuser "
				"weiter weg gehen wieder, beschädigte\nwerden mit ihrem Schutt gespeichert, sobald alles ruht. Größer kostet\nSpeicher "
				"und Grafik." );
		}
		const WorldView& view = app.view;
		ImGui::Text( "Häuser geladen %d von %d, beschädigt %d", view.loadedHouses, CitySide * CitySide, view.damagedHouses );
		ImGui::Text( "Streaming %.2f ms, geladen %d, entladen %d", view.streamTime, view.loads, view.unloads );
		ImGui::Text( "Gespeichert %d, zurückgeholt %d, %.0f MB im Speicher", view.saves, view.restores,
					 (double)view.savedBytes / 1048576.0 );
		ImGui::Text( "Speicher %.0f MB (Nebenan und Box3D)", (double)( view.stats.byteCount + view.counters.byteCount ) / 1048576.0 );
	}

	ImGui::SeparatorText( "Werkzeug" );
	for ( int i = 0; i < ToolCount; ++i )
	{
		char label[32];
		snprintf( label, sizeof( label ), "%s [%d]", s_toolNames[i], i + 1 );
		if ( ImGui::RadioButton( label, app.tool == i ) )
		{
			app.tool = (Tool)i;
		}
		if ( i + 1 < ToolCount )
		{
			ImGui::SameLine();
		}
	}

	ToolSettings& settings = app.tools[app.tool];
	if ( app.tool == ToolCannon )
	{
		ImGui::SliderFloat( "Kugelradius (m)", &settings.radius, 0.1f, 0.6f, "%.2f" );
		ImGui::SliderFloat( "Geschwindigkeit (m/s)", &settings.ejectSpeed, 10.0f, 120.0f, "%.0f" );
		ImGui::SliderFloat( "Schuss pro Sekunde", &settings.rate, 0.5f, 10.0f, "%.1f" );
	}
	else
	{
		ImGui::SliderFloat( "Radius (m)", &settings.radius, 0.1f, 3.0f, "%.2f" );
		ImGui::SliderFloat( "Schaden", &settings.damage, 1.0e3f, 2.0e6f, "%.0f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Auswurf (m/s)", &settings.ejectSpeed, 0.0f, 30.0f, "%.1f" );
		ImGui::SliderInt( "Bruchstücke (0 = auto)", &settings.fragments, 0, 150 );
		ImGui::SliderFloat( "Schuss pro Sekunde", &settings.rate, 0.5f, 20.0f, "%.1f" );
	}

	ImGui::SeparatorText( "Leistung" );
	// A setting that changes the worlds waits for the steps of the frame by itself, see nbWorld_Step
	if ( ImGui::SliderFloat( "Bruchstückgröße", &app.fragmentScale, 0.5f, 4.0f, "x %.2f" ) )
	{
		nbWorld_SetFragmentScale( app.destruction, app.fragmentScale );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Größere Bruchstücke: weniger Teile pro Einschlag, weniger Körper,\nKontakte und Dreiecke. x2 halbiert ungefähr die Zeit\nbei Massenzerstörung. Gilt für die nächsten Einschläge." );
	}
	if ( ImGui::Checkbox( "Zufall je Etage", &app.storeyRandom ) )
	{
		nbWorld_SetStoreyCollapse( app.destruction, ActiveCollapse( app )[0], ActiveCollapse( app )[1] );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "An: Jede Etage wählt beim Laden des Hauses ihre Werte zufällig aus den\nBereichen darunter, für dasselbe Haus immer dieselben. Aus: Jede Etage\nnimmt die Werte der Regler." );
	}
	for ( int top = 0; top < 2; ++top )
	{
		nbStoreyCollapse* collapse = ( app.storeyRandom ? app.randomCollapse : app.fixedCollapse ) + top;
		float damage[2] = { 100.0f * collapse->damageMin, 100.0f * collapse->damageMax };
		float residue[2] = { 100.0f * collapse->residueMin, 100.0f * collapse->residueMax };
		const char* damageLabel = top == 1 ? "Oberste gibt nach" : "Etage gibt nach";
		const char* residueLabel = top == 1 ? "Oberste Stütze" : "Stütze bleibt";
		const char* damageTip =
			top == 1 ? "Dasselbe wie „Etage gibt nach“ für die oberste Etage unter dem Dach."
					 : "Ist so viel der Wände einer Etage zerstört, gibt sie nach. Mit „Zufall je\nEtage“ wählt jede Etage ihren Wert beim Laden aus dem Bereich. Was\n„Stütze bleibt“ sagt, bleibt von den übrigen Wänden stehen, in Zellen\nzerbrochen, der Rest fliegt heraus, und alles darüber kommt in einem\nStück herunter und kippt über das, was blieb. 0 schaltet es ab.";
		const char* residueTip =
			top == 1 ? "Dasselbe wie „Stütze bleibt“ für die oberste Etage unter dem Dach."
					 : "So viel der übrigen Wände einer Etage, die nachgibt, bleibt stehen, in\nZellen zerbrochen: bei Schaden auf einer Seite auf der stärksten Seite,\nsonst an zufälligen Stellen. Der Rest fliegt heraus. 0 wirft alle\nheraus, 100 lässt alle stehen. Gilt für die Etagen, die danach nachgeben.";
		bool changed = false;
		if ( app.storeyRandom )
		{
			changed |= ImGui::DragFloatRange2( damageLabel, damage, damage + 1, 0.1f, 0.0f, 100.0f, "bei %.0f", "bis %.0f %%",
											   ImGuiSliderFlags_AlwaysClamp );
			if ( ImGui::IsItemHovered() )
			{
				ImGui::SetTooltip( "%s", damageTip );
			}
			changed |= ImGui::DragFloatRange2( residueLabel, residue, residue + 1, 0.1f, 0.0f, 100.0f, "%.0f", "bis %.0f %%",
											   ImGuiSliderFlags_AlwaysClamp );
			if ( ImGui::IsItemHovered() )
			{
				ImGui::SetTooltip( "%s", residueTip );
			}
		}
		else
		{
			changed |= ImGui::SliderFloat( damageLabel, damage, 0.0f, 100.0f, "bei %.0f %% zerstört" );
			if ( ImGui::IsItemHovered() )
			{
				ImGui::SetTooltip( "%s", damageTip );
			}
			changed |= ImGui::SliderFloat( residueLabel, residue, 0.0f, 100.0f, "%.0f %% der übrigen Wände" );
			if ( ImGui::IsItemHovered() )
			{
				ImGui::SetTooltip( "%s", residueTip );
			}
			damage[1] = damage[0];
			residue[1] = residue[0];
		}
		if ( changed )
		{
			*collapse = nbStoreyCollapse{ 0.01f * damage[0], 0.01f * damage[1], 0.01f * residue[0], 0.01f * residue[1] };
			nbWorld_SetStoreyCollapse( app.destruction, ActiveCollapse( app )[0], ActiveCollapse( app )[1] );
		}
	}
	if ( ImGui::SliderFloat( "Statik ohne Decken", &app.supportScale, 0.0f, 4.0f, "x %.2f" ) )
	{
		nbWorld_SetSupportScale( app.destruction, app.supportScale );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Lastprüfung für Bauwerke ohne Decken, wie die Mauern: Druck und\nBiegung, was nicht trägt, bricht ein. Häuser prüft sie nie.\n0 schaltet sie ab, so ist es voreingestellt." );
	}
	if ( ImGui::SliderInt( "Bewegte Trümmer", &app.maxDebrisBodies, 100, 5000 ) )
	{
		nbWorld_SetDebrisBudget( app.destruction, app.maxDebrisBodies );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "So viele Trümmer bewegt Box3D höchstens gleichzeitig.\nDie langsamsten darüber erstarren zu Schutt, gelöscht wird nichts.\nWeniger macht die Physik schneller." );
	}
	if ( ImGui::SliderFloat( "Ruhezeit", &app.debrisRestTime, 0.02f, 0.5f, "%.2f s" ) )
	{
		nbWorld_SetDebrisRestTime( app.destruction, app.debrisRestTime );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "So lange muss ein Trümmer fast still liegen, bevor er zu Schutt erstarrt.\nKürzer macht die Physik unter Dauerfeuer schneller, kann aber ein\nkleines Stück anhalten, das beim Kippen nur kurz innehält. Vorher 0,2 s." );
	}
	if ( ImGui::Checkbox( "Physik parallel zum Zeichnen", &app.parallelPhysics ) )
	{
		nbWorld_SetAsyncStep( app.destruction, app.parallelPhysics );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "An: Nebenan rechnet die Physik eines Bilds auf einem eigenen Thread, während\ndas Bild davor gezeichnet wird. Ein Bild kostet so viel wie der längere der\nbeiden Teile statt beide zusammen und zeigt die Welt einen Schritt später.\nAus: alles nacheinander auf einem Thread. Die Welt rechnet in beiden Fällen gleich.\nDie Threads passen sich beim nächsten Laden der Szene an (R)." );
	}
	ImGui::Checkbox( "Zwischen Schritten zeichnen", &app.interpolate );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "An: Die Bibliothek gibt jedem Bild Lagen zwischen den letzten zwei Schritten der\nPhysik. So bewegen sich die Trümmer flüssig, wenn mehr Bilder kommen als Schritte,\netwa auf Monitoren mit 144 Hz. Das Bild liegt dann einen Schritt hinter der Physik.\nAus: Jedes Bild zeigt den Stand nach dem letzten Schritt. Die Physik rechnet gleich." );
	}
	ImGui::SameLine();
	ImGui::Checkbox( "Zeitlupe", &app.slowMotion );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Die Physik bekommt ein Viertel der Zeit. Dann kommen viermal so viele Bilder wie\nSchritte, und der Unterschied von „Zwischen Schritten zeichnen“ zeigt sich auf\njedem Monitor." );
	}
	static const float s_stepRates[3] = { 30.0f, 60.0f, 120.0f };
	static const char* s_stepRateNames[3] = { "30 Hz", "60 Hz", "120 Hz" };
	int stepRateIndex = app.stepRate < 45.0f ? 0 : ( app.stepRate < 90.0f ? 1 : 2 );
	if ( ImGui::Combo( "Physiktakt", &stepRateIndex, s_stepRateNames, 3 ) )
	{
		app.stepRate = s_stepRates[stepRateIndex];
		nbWorld_SetStepRate( app.destruction, app.stepRate );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "So viele Schritte rechnet die Physik pro Sekunde, jeden mit so vielen Teilschritten,\ndass es 240 pro Sekunde sind. 30 Hz kostet weniger, aber Kontakte und Treffer kommen\nbis zu einen Schritt später, und schnelle Trümmer durchdringen einander leichter.\n120 Hz ist genauer und kostet mehr. Mit „Zwischen Schritten zeichnen“ bewegt sich\ndas Bild bei jedem Takt flüssig. Vorgabe 60 Hz." );
	}
	if ( app.vsyncAvailable )
	{
		if ( ImGui::Checkbox( "VSync (V)", &app.vsync ) )
		{
			DemoSetVsync( app.vsync );
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Aus: so viele Bilder wie möglich, die FPS zeigen, was der PC schafft.\nAn: an die Bildwiederholrate des Monitors gebunden, etwa 60 oder 144 Hz." );
		}
		ImGui::SameLine();
	}
	ImGui::Checkbox( "Pause (P)", &app.paused );
	ImGui::SameLine();
	if ( ImGui::Button( "Trümmer entfernen (C)" ) )
	{
		nbWorld_ClearDebris( app.destruction );
	}

	ImGui::EndDisabled();

	ImGui::SeparatorText( "Messung" );
	const nbStats& stats = app.view.stats;
	RenderStats renderStats = app.renderer.GetStats();
	float frameAvg, frameMax, impactAvg, impactMax, simulationAvg, simulationMax, graphicsAvg, graphicsMax, waitAvg, waitMax;
	HistoryStats( app, app.historyFrame, &frameAvg, &frameMax );
	HistoryStats( app, app.historyImpacts, &impactAvg, &impactMax );
	HistoryStats( app, app.historySimulation, &simulationAvg, &simulationMax );
	HistoryStats( app, app.historyGraphics, &graphicsAvg, &graphicsMax );
	HistoryStats( app, app.historyWait, &waitAvg, &waitMax );

	// With parallel physics the main thread draws while the simulation runs and then waits for what is left of it, so the
	// CPU could make a frame in the longer of the two
	float cpuFrame = impactAvg + graphicsAvg + ( app.parallelPhysics ? waitAvg : simulationAvg );
	float cpuLimit = app.parallelPhysics ? b3MaxFloat( simulationAvg, impactAvg + graphicsAvg ) : cpuFrame;
	const FpsMeter& meter = app.fpsMeter;
	ImGui::Text( "Bild        %6.2f ms  %.0f FPS, 1%% Low %.0f FPS", meter.frameTime, meter.fps, meter.lowFps );
	ImGui::Text( "            Spitze %.1f ms (letzte %d Bilder)", frameMax, app.historyCount );
	if ( app.parallelPhysics )
	{
		ImGui::Text( "Hauptthread %6.2f ms  Einschläge %.2f, Grafik %.2f,", cpuFrame, impactAvg, graphicsAvg );
		ImGui::Text( "                      Warten auf die Simulation %.2f", waitAvg );
		ImGui::Text( "Simulation  %6.2f ms  parallel zum Zeichnen", simulationAvg );
	}
	else
	{
		ImGui::Text( "CPU         %6.2f ms  Einschläge %.2f, Simulation %.2f,", cpuFrame, impactAvg, simulationAvg );
		ImGui::Text( "                      Grafik %.2f", graphicsAvg );
	}

	// Whatever a frame takes beyond the CPU time of the main thread is the wait for the display or the graphics card
	ImVec4 hint = ImVec4( 1.0f, 0.75f, 0.3f, 1.0f );
	bool waiting = frameAvg > 1.2f * cpuFrame + 0.5f;

	// Frames that come exactly at the refresh rate with vertical sync off mean something else syncs them
	bool displayLocked = app.vsync == false && app.refreshRate > 0 && cpuFrame < 0.7f * meter.frameTime &&
						 fabsf( meter.frameTime * (float)app.refreshRate / 1000.0f - 1.0f ) < 0.03f;
	if ( displayLocked && app.unthrottled == false )
	{
		ImGui::TextColored( hint, "  Die FPS kleben an den %d Hz des Monitors:\n  Windows erlaubt hier kein Tearing", app.refreshRate );
	}
	else if ( displayLocked )
	{
		ImGui::TextColored( hint, "  Die FPS kleben an den %d Hz des Monitors:\n  VSync ist wohl im Grafiktreiber erzwungen", app.refreshRate );
	}
	else if ( waiting && app.vsync )
	{
		ImGui::TextColored( hint, "  VSync begrenzt, die CPU schafft etwa %.0f FPS", 1000.0f / b3MaxFloat( cpuLimit, 0.01f ) );
	}
	else if ( waiting )
	{
		ImGui::TextColored( hint, "  Die Grafikkarte begrenzt die FPS" );
	}
	else if ( app.parallelPhysics && waitAvg > 0.25f * cpuFrame )
	{
		ImGui::TextDisabled( "  Die Simulation begrenzt die FPS" );
	}
	else
	{
		ImGui::TextDisabled( "  Die CPU begrenzt die FPS" );
	}
	ImGui::Text( "Physik      %6.2f ms  (Box3D Schritt)", app.physicsTime );
	ImGui::Text( "Zerstörung  %6.2f ms  (Update)", app.destructionTime );
	ImGui::Text( "Einschlag   %6.2f ms  davon parallel %.2f ms", app.lastImpact.totalTime, app.lastImpact.fractureTime );
	ImGui::Text( "Bruchstücke %d  Verbindungen %d", stats.chunkCount, stats.bondCount );
	ImGui::Text( "Trümmerkörper %d  wach %d  Kontakte %d", stats.dynamicBodyCount, app.view.awakeBodies,
				 app.view.counters.contactCount );
	ImGui::Text( "Dreiecke %dk  Draw Calls %d  Upload %d kB", renderStats.triangleCount / 1000, renderStats.drawCalls,
				 renderStats.uploadedBytes / 1024 );
	ImGui::TextDisabled( "%s", app.gpuName.empty() ? "Grafikkarte unbekannt" : app.gpuName.c_str() );
	if ( ImGui::Button( "Messwerte kopieren" ) )
	{
		std::string text = BuildReport( app );
		sapp_set_clipboard_string( text.c_str() );
		app.copiedFrame = app.frame;
	}
	ImGui::SameLine();
	ImGui::TextDisabled( app.frame - app.copiedFrame < 180 ? "kopiert, einfach einfügen" : "für den Chat" );

	Comparison& comparison = app.comparison;
	if ( comparison.active )
	{
		char label[64];
		DescribeComparisonRun( comparison, label, sizeof( label ) );
		int houseCount = TownRows * TownColumns;
		ImGui::Text( "%s, Haus %d von %d, Bild %d", label, b3MinInt( app.demolition.house + 1, houseCount ), houseCount,
					 comparison.frame );
		if ( ImGui::Button( "Messung abbrechen" ) )
		{
			StopComparison( app );
		}
	}
	else
	{
		if ( ImGui::Button( "Parallel gegen nacheinander messen" ) )
		{
			comparison.requested = true;
			comparison.kind = Comparison::ParallelAgainstSerial;
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Lädt die Stadt und reißt sie Haus für Haus mit Granaten ab, Wand für Wand von\noben nach unten, bis jedes Haus unten ist, mit Budget 5000 und nur so schnell,\ndass nie mehr als 3000 Trümmer zugleich fliegen: einmal kurz zum Aufwärmen, dann\nparallel, nacheinander, nacheinander und parallel, mit einem Schritt pro Bild und\njeweils mit den passenden Threads. Das Ergebnis kommt in die Zwischenablage,\ndanach lädt die Szene von vorher neu." );
		}
		if ( ImGui::Button( "Skalierung messen" ) )
		{
			comparison.requested = true;
			comparison.kind = Comparison::Scaling;
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip(
				"Reißt die Stadt wie beim Vergleich Haus für Haus ab, einmal kurz zum Aufwärmen,\ndann mit 1, 2, 4 und so weiter "
				"bis %d Threads, alle mit der Physik nacheinander.\nDie Bibliothek misst dabei jede Stufe von Box3D und Nebenan, "
				"und rechnet aus,\nwas mit mehr Threads auf einem Thread übrig bleibt. Das dauert ein paar Minuten.\nDas "
				"Ergebnis kommt in die Zwischenablage, danach lädt die Szene von vorher neu.",
				nbGetRecommendedWorkerCount( false ) );
		}
		if ( comparison.summary.empty() == false )
		{
			ImGui::TextUnformatted( comparison.summary.c_str() );
		}
	}

	ImGui::SeparatorText( "Steuerung" );
	ImGui::TextWrapped( "Linke Maustaste: schießen (halten = Dauerfeuer)\n"
						"Rechte Maustaste halten: umsehen\n"
						"WASD bewegen, Q/E runter/hoch, Shift schneller\n"
						"1-3 Werkzeug, R neu laden, P Pause, C Trümmer weg,\n"
						"V VSync, F1 Menü" );

	ImGui::End();
}

// Frame rate in the top right corner and the crosshair, with and without the menu
static void DrawOverlay( App& app )
{
	float dpi = sapp_dpi_scale();
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	const FpsMeter& meter = app.fpsMeter;

	char fps[32];
	snprintf( fps, sizeof( fps ), "%.0f FPS", meter.fps );
	char detail[96];
	snprintf( detail, sizeof( detail ), "%.2f ms   1%% Low %.0f   VSync %s", meter.frameTime, meter.lowFps, app.vsync ? "an" : "aus" );

	ImFont* font = ImGui::GetFont();
	float smallSize = ImGui::GetFontSize();
	float largeSize = 2.0f * smallSize;
	ImVec2 fpsExtent = font->CalcTextSizeA( largeSize, FLT_MAX, 0.0f, fps );
	ImVec2 detailExtent = font->CalcTextSizeA( smallSize, FLT_MAX, 0.0f, detail );
	float pad = 8.0f * dpi;
	float right = ImGui::GetIO().DisplaySize.x - 12.0f * dpi;
	float top = 12.0f * dpi;
	float width = b3MaxFloat( fpsExtent.x, detailExtent.x ) + 2.0f * pad;
	float height = fpsExtent.y + detailExtent.y + 2.0f * pad;
	draw->AddRectFilled( ImVec2( right - width, top ), ImVec2( right, top + height ), IM_COL32( 16, 20, 26, 180 ), 6.0f * dpi );

	// Green from 60 frames per second, yellow from 30, red below
	ImU32 fpsColor = meter.fps >= 60.0f ? IM_COL32( 120, 230, 120, 255 )
										: ( meter.fps >= 30.0f ? IM_COL32( 250, 210, 90, 255 ) : IM_COL32( 250, 100, 90, 255 ) );
	draw->AddText( font, largeSize, ImVec2( right - pad - fpsExtent.x, top + pad ), fpsColor, fps );
	draw->AddText( font, smallSize, ImVec2( right - pad - detailExtent.x, top + pad + fpsExtent.y ), IM_COL32( 225, 230, 235, 255 ), detail );

	// The comparison shows how far it is, also without the menu
	const Comparison& comparison = app.comparison;
	if ( comparison.active )
	{
		char label[64];
		DescribeComparisonRun( comparison, label, sizeof( label ) );
		char progress[96];
		snprintf( progress, sizeof( progress ), "%s", label );
		ImVec2 extent = font->CalcTextSizeA( smallSize, FLT_MAX, 0.0f, progress );
		draw->AddText( font, smallSize, ImVec2( right - pad - extent.x, top + height + pad ), IM_COL32( 250, 210, 90, 255 ), progress );
	}

	// Crosshair at the aim point
	ImVec2 center = app.mouseLook ? ImVec2( 0.5f * sapp_widthf() / dpi, 0.5f * sapp_heightf() / dpi )
								  : ImVec2( app.mouseX / dpi, app.mouseY / dpi );
	ImU32 color = IM_COL32( 255, 255, 255, 200 );
	draw->AddLine( ImVec2( center.x - 9, center.y ), ImVec2( center.x - 3, center.y ), color, 2.0f );
	draw->AddLine( ImVec2( center.x + 3, center.y ), ImVec2( center.x + 9, center.y ), color, 2.0f );
	draw->AddLine( ImVec2( center.x, center.y - 9 ), ImVec2( center.x, center.y - 3 ), color, 2.0f );
	draw->AddLine( ImVec2( center.x, center.y + 3 ), ImVec2( center.x, center.y + 9 ), color, 2.0f );
}

//----------------------------------------------------------------------------------------------------------------------
// Scripted runs for measurements and screenshots

// The shots of the script for the frame that counts from its start
static void RunScript( App& app, int frame )
{
	int f = frame;
	switch ( app.scene )
	{
		case SceneWall:
			if ( f >= 20 && f < 80 && f % 4 == 0 )
			{
				int k = ( f - 20 ) / 4;
				FireAt( app, ToolRifle, { -3.0f + 1.5f * (float)( k % 5 ), 2.4f - 0.6f * (float)( k / 5 ), 0.15f } );
			}
			else if ( f == 90 )
			{
				FireAt( app, ToolGrenade, { 2.6f, 0.9f, 0.15f } );
			}
			else if ( f == 100 )
			{
				FireAt( app, ToolCannon, { -2.4f, 1.2f, 0.0f } );
			}
			break;

		case SceneStress:
			if ( f >= 20 && f < 120 && f % 5 == 0 )
			{
				int k = ( f - 20 ) / 5;
				FireAt( app, k % 3 == 0 ? ToolGrenade : ToolRifle, { -8.0f + 0.8f * (float)k, 1.5f, 0.15f } );
			}
			break;

		case SceneCity:
		{
			// Down the street at 16 m/s with a grenade on a house on either side every fifth frame, after 20 seconds back
			// again, past the damaged houses that were saved meanwhile
			app.camera.position.x += f < 1200 ? 16.0f / 60.0f : -16.0f / 60.0f;
			if ( f >= 20 && f % 5 == 0 )
			{
				int k = ( f - 20 ) / 5;
				float half = 0.5f * (float)( CitySide - 1 );
				int i = (int)floorf( ( app.camera.position.x + 10.0f + 14.0f * (float)( k % 3 ) ) / 14.0f + half + 0.5f );
				int j = ( k & 1 ) != 0 ? CitySide / 2 : CitySide / 2 - 1;
				b3Vec3 center = CityHousePosition( j * CitySide + i );
				float along = (float)( ( k * 5 ) % 9 ) / 4.0f - 1.0f;
				float height = ( k % 3 ) == 0 ? 4.4f : 1.2f;
				FireAt( app, ToolGrenade, b3Add( center, b3Vec3{ 4.2f * along, height, ( k & 1 ) != 0 ? -3.4f : 3.4f } ) );
			}
			break;
		}

		default:
			if ( f >= 20 && f % 5 == 0 )
			{
				// Twelve grenades a second on the walls of the houses, like holding the fire button
				int k = ( f - 20 ) / 5;
				int house = ( k * 7 ) % ( TownRows * TownColumns );
				b3Vec3 center = TownHousePosition( house / TownColumns, house % TownColumns );
				float along = (float)( ( k * 5 ) % 9 ) / 4.0f - 1.0f;
				float height = ( k % 3 ) == 0 ? 4.4f : 1.2f;
				b3Vec3 local = ( k % 4 ) < 2 ? b3Vec3{ 4.2f * along, height, ( k % 4 ) == 0 ? 3.4f : -3.4f }
											  : b3Vec3{ ( k % 4 ) == 2 ? 4.7f : -4.7f, height, 3.0f * along };
				FireAt( app, ToolGrenade, b3Add( center, local ) );
			}
			break;
	}
}

// Hash of where every chunk is, to compare runs: the same script must end in the same world with and without parallel
// physics
static uint64_t HashChunkTransforms( const App& app )
{
	uint64_t hash = 14695981039346656037ull;
	for ( const ChunkVisual& visual : app.chunks )
	{
		if ( visual.alive == false )
		{
			continue;
		}

		b3WorldTransform transform = b3Body_GetTransform( nbChunk_GetBody( visual.id ) );
		b3Vec3 p = b3ToVec3( transform.p );
		float values[7] = { p.x, p.y, p.z, transform.q.v.x, transform.q.v.y, transform.q.v.z, transform.q.s };
		uint8_t bytes[sizeof( values )];
		memcpy( bytes, values, sizeof( values ) );
		for ( uint8_t byte : bytes )
		{
			hash = ( hash ^ byte ) * 1099511628211ull;
		}
	}
	return hash;
}

// The comparison takes the town down house by house. The first pass puts grenades on every wall of the house, one every
// 1.2 m along it and two per storey up it, row by row from the top storey down, so no storey falls whole before its walls
// are gone. A little later every place where the house still stands bonded higher than 1.5 m gets a grenade of its own,
// highest first, until nothing stands there anymore, at most five more times. Then comes the next house. With the same
// settings every run fires the same shots.
static const int DemolitionShotsPerRow = 24;
static const int DemolitionShotsPerFrame = 4;
static const int DemolitionStart = 20;
static const int DemolitionCheckDelay = 15;
static const int DemolitionMaxPasses = 6;
static const float DemolitionStandingHeight = 1.5f;

// Over budget the slowest debris freezes, and in a burst also pieces still in flight, which then hang in the air. So the
// demolition runs with this budget and fires the next grenade only while less than 3 000 pieces move, the most debris a
// few grenades throw added stays below it.
static const int DemolitionBudget = 5000;
static const int DemolitionMovingLimit = 3000;

// Shot k of the first pass at a house: its rows from the top down, in each the front and back walls with seven shots
// each, then the end walls with five each
static b3Vec3 DemolitionTarget( int house, int k )
{
	int row = 2 * TownHouseFloors( house ) - 1 - k / DemolitionShotsPerRow;
	k %= DemolitionShotsPerRow;
	float height = 3.25f * (float)( row / 2 ) + ( row % 2 == 0 ? 0.9f : 2.2f );
	b3Vec3 local;
	if ( k < 14 )
	{
		local = { -3.6f + 1.2f * (float)( k % 7 ), height, k < 7 ? 3.4f : -3.4f };
	}
	else
	{
		local = { k < 19 ? 4.7f : -4.7f, height, -2.4f + 1.2f * (float)( ( k - 14 ) % 5 ) };
	}
	return b3Add( TownHousePosition( house / TownColumns, house % TownColumns ), local );
}

// The places where the house still stands: chunks still bonded into it that reach higher than 1.5 m, highest first,
// one place per 1.2 m
static void FindStanding( const App& app, int house, std::vector<b3Vec3>& targets )
{
	targets.clear();
	nbDestructibleId id = app.townHouses[(size_t)house];
	std::vector<nbChunkId> chunks( (size_t)nbDestructible_GetChunkCount( id ) );
	int count = nbDestructible_GetChunks( id, chunks.data(), (int)chunks.size() );

	std::vector<std::pair<float, b3Vec3>> standing;
	for ( int i = 0; i < count; ++i )
	{
		nbChunkId chunk = chunks[(size_t)i];
		if ( nbChunk_IsDynamic( chunk ) || nbChunk_GetBondCount( chunk ) == 0 )
		{
			continue;
		}

		b3AABB box = b3Shape_GetAABB( nbChunk_GetShape( chunk ) );
		if ( box.upperBound.y > DemolitionStandingHeight )
		{
			standing.push_back( { box.upperBound.y, b3MulSV( 0.5f, b3Add( box.lowerBound, box.upperBound ) ) } );
		}
	}

	std::stable_sort( standing.begin(), standing.end(),
					  []( const std::pair<float, b3Vec3>& a, const std::pair<float, b3Vec3>& b ) { return a.first > b.first; } );
	for ( const std::pair<float, b3Vec3>& place : standing )
	{
		bool covered = false;
		for ( b3Vec3 target : targets )
		{
			covered = covered || b3LengthSquared( b3Sub( target, place.second ) ) < 1.2f * 1.2f;
		}
		if ( covered == false )
		{
			targets.push_back( place.second );
		}
	}
}

static void FireDemolition( App& app, int frame )
{
	Demolition& demolition = app.demolition;
	int houseCount = (int)app.townHouses.size();
	if ( frame < DemolitionStart || demolition.house >= houseCount )
	{
		return;
	}

	// After a pass the house gets a moment, then whatever still stands is the next pass
	if ( demolition.wait > 0 )
	{
		demolition.wait -= 1;
		if ( demolition.wait > 0 )
		{
			return;
		}

		FindStanding( app, demolition.house, demolition.targets );
		demolition.pass += 1;
		demolition.shot = 0;
		if ( demolition.targets.empty() || demolition.pass == DemolitionMaxPasses )
		{
			demolition.house += 1;
			demolition.pass = 0;
			demolition.targets.clear();
			if ( demolition.house == houseCount )
			{
				demolition.doneFrame = frame;
			}
		}
		return;
	}

	bool first = demolition.pass == 0;
	int passShots = first ? 2 * TownHouseFloors( demolition.house ) * DemolitionShotsPerRow : (int)demolition.targets.size();
	for ( int i = 0; i < DemolitionShotsPerFrame && demolition.shot < passShots; ++i )
	{
		nbStats stats = nbWorld_GetStats( app.destruction );
		if ( stats.debrisCount - stats.rubbleCount >= DemolitionMovingLimit )
		{
			break;
		}

		b3Vec3 target = first ? DemolitionTarget( demolition.house, demolition.shot ) : demolition.targets[(size_t)demolition.shot];
		FireAt( app, ToolGrenade, target );
		demolition.shot += 1;
		demolition.shots += 1;
	}

	if ( demolition.shot == passShots )
	{
		demolition.wait = DemolitionCheckDelay;
	}
}

static void RunDemolition( App& app, int frame )
{
	FireDemolition( app, frame );

	// The debris moving right after the shots, before the next update freezes what is over budget
	nbStats stats = nbWorld_GetStats( app.destruction );
	app.demolition.maxMoving = b3MaxInt( app.demolition.maxMoving, stats.debrisCount - stats.rubbleCount );
}

//----------------------------------------------------------------------------------------------------------------------
// Parallel against serial physics with one click, see Comparison

static void BeginComparisonRun( App& app )
{
	Comparison& comparison = app.comparison;
	app.parallelPhysics = RunIsParallel( comparison, comparison.run );
	app.forcedWorkers = comparison.kind == Comparison::Scaling ? comparison.runWorkers[comparison.run] : 0;
	app.maxDebrisBodies = DemolitionBudget;
	app.demolition = Demolition();
	LoadScene( app, SceneTown );

	// The profile counts the steps of the demolition, not the loading of the town
	nbWorld_ResetProfile( app.destruction );

	int run = comparison.run;
	comparison.threads[run] = app.workerCount;
	comparison.frame = 0;
	comparison.frameTimes[run].clear();
	comparison.simulationTime[run] = 0.0;
	comparison.graphicsTime[run] = 0.0;
	comparison.waitTime[run] = 0.0;
	comparison.longestSimulation[run] = 0.0f;
	comparison.longestWait[run] = 0.0f;
}

static void StartComparison( App& app )
{
	Comparison& comparison = app.comparison;
	comparison.active = true;
	comparison.run = 0;

	// The scaling warms up with the most threads, then takes a run for each number of threads, the fewest first
	comparison.runCount = Comparison::RunCount;
	if ( comparison.kind == Comparison::Scaling )
	{
		int counts[Comparison::MaxRuns - 1];
		int countCount = nbGetScalingWorkerCounts( counts, Comparison::MaxRuns - 1 );
		comparison.runCount = countCount + 1;
		comparison.runWorkers[0] = counts[countCount - 1];
		for ( int i = 0; i < countCount; ++i )
		{
			comparison.runWorkers[i + 1] = counts[i];
		}
	}
	comparison.scene = app.scene;
	comparison.tool = app.tool;
	comparison.maxDebrisBodies = app.maxDebrisBodies;
	comparison.parallelPhysics = app.parallelPhysics;
	comparison.vsync = app.vsync;
	comparison.paused = app.paused;
	comparison.slowMotion = app.slowMotion;

	// Frames as fast as they come, else both would wait for the display, and steps at their normal pace
	app.paused = false;
	app.slowMotion = false;
	if ( app.vsyncAvailable )
	{
		app.vsync = false;
		DemoSetVsync( false );
	}
	BeginComparisonRun( app );
}

// Back to the scene and the settings from before
static void StopComparison( App& app )
{
	Comparison& comparison = app.comparison;
	comparison.active = false;
	app.forcedWorkers = 0;
	app.tool = comparison.tool;
	app.maxDebrisBodies = comparison.maxDebrisBodies;
	app.parallelPhysics = comparison.parallelPhysics;
	app.paused = comparison.paused;
	app.slowMotion = comparison.slowMotion;
	if ( app.vsyncAvailable )
	{
		app.vsync = comparison.vsync;
		DemoSetVsync( app.vsync );
	}
	LoadScene( app, comparison.scene );
}

// The average of some frame times, their 1 % low and the longest
struct FrameStats
{
	float average = 0.0f;
	float lowFps = 0.0f;
	float longest = 0.0f;
};

static FrameStats GetFrameStats( std::vector<float> times )
{
	FrameStats stats;
	if ( times.empty() )
	{
		return stats;
	}

	double total = 0.0;
	for ( float time : times )
	{
		total += time;
	}
	stats.average = (float)( total / (double)times.size() );
	std::sort( times.begin(), times.end() );
	stats.lowFps = 1000.0f / b3MaxFloat( times[times.size() * 99 / 100], 0.001f );
	stats.longest = times.back();
	return stats;
}

// The counted runs with or without parallel physics: their frames, and the CPU time per frame
struct ComparisonResult
{
	int threads = 0;
	FrameStats frames;
	float simulationTime = 0.0f;
	float graphicsTime = 0.0f;
	float waitTime = 0.0f;
};

static ComparisonResult SumComparison( const Comparison& comparison, bool parallel )
{
	ComparisonResult result;
	std::vector<float> times;
	double simulation = 0.0, graphics = 0.0, wait = 0.0;
	for ( int run = 1; run < Comparison::RunCount; ++run )
	{
		if ( Comparison::IsParallel( run ) != parallel )
		{
			continue;
		}

		times.insert( times.end(), comparison.frameTimes[run].begin(), comparison.frameTimes[run].end() );
		simulation += comparison.simulationTime[run];
		graphics += comparison.graphicsTime[run];
		wait += comparison.waitTime[run];
		result.threads = comparison.threads[run];
	}

	double frames = 0.0;
	for ( int run = 1; run < Comparison::RunCount; ++run )
	{
		frames += Comparison::IsParallel( run ) == parallel ? (double)comparison.frames[run] : 0.0;
	}
	frames = b3MaxFloat( (float)frames, 1.0f );
	result.frames = GetFrameStats( times );
	result.simulationTime = (float)( simulation / frames );
	result.graphicsTime = (float)( graphics / frames );
	result.waitTime = (float)( wait / frames );
	return result;
}

// The machine of a report: the CPU with the lanes of the solver, and the GPU with the frame, which can hold up the frames
static void AppendMachine( std::string& text, const App& app )
{
	Appendf( text, "CPU: %s, %d logische Kerne, %d Performance-Kerne, Löser mit %d Spuren\n",
			 app.cpuName.empty() ? "unbekannt" : app.cpuName.c_str(), nbGetHardwareThreadCount(), app.performanceCores,
			 b3GetSIMDWidth() );
	Appendf( text, "GPU: %s, Bild %d x %d, MSAA %dx, Monitor %d Hz, VSync %s\n", app.gpuName.empty() ? "unbekannt" : app.gpuName.c_str(),
			 sapp_width(), sapp_height(), sapp_sample_count(), app.refreshRate, app.vsyncAvailable ? "aus" : "an, lässt sich nicht abschalten" );
}

// The report of the scaling goes to the clipboard, to paste into a chat: the frames of every run and the profile of the
// library fitted over the runs, see nbFormatScalingReport
static void FinishScaling( App& app )
{
	Comparison& comparison = app.comparison;
	const nbProfile* profiles = comparison.profiles + 1;
	int profileCount = comparison.runCount - 1;
	bool same = true;
	for ( int run = 2; run < comparison.runCount; ++run )
	{
		same = same && comparison.hashes[run] == comparison.hashes[1];
	}

	b3Version version = b3GetVersion();
	std::string text;
	Appendf( text, "Nebenan-Skalierung: Abriss der Stadt mit" );
	for ( int run = 1; run < comparison.runCount; ++run )
	{
		Appendf( text, "%s %d", run == 1 ? "" : ( run == comparison.runCount - 1 ? " und" : "," ), comparison.runWorkers[run] );
	}
	Appendf( text, " Threads, Physik nacheinander, %s-Build, Box3D %d.%d.%d\n", s_buildType, version.major, version.minor,
			 version.revision );
	AppendMachine( text, app );
	const Demolition& demolition = comparison.demolition;
	Appendf( text,
			 "Abriss mit %d Granaten, bis zu %d pro Bild, solange weniger als %d Trümmer fliegen, Bruchstückgröße x%.2f, "
			 "Budget %d, Physiktakt ",
			 demolition.shots, DemolitionShotsPerFrame, DemolitionMovingLimit, app.fragmentScale, DemolitionBudget );
	AppendStepRate( text, app );
	Appendf( text, ", 1/60 s pro Bild, erst ein Lauf mit %d Bildern zum Aufwärmen, der nicht zählt\n", Comparison::WarmupFrames );
	if ( demolition.doneFrame >= 0 )
	{
		Appendf( text, "Alle %d Häuser nach %d Bildern unten, %d Bilder pro Lauf\n", demolition.house, demolition.doneFrame,
				 comparison.frames[1] );
	}
	else
	{
		Appendf( text, "Nach %d Bildern erst %d Häuser unten\n", comparison.frames[1], demolition.house );
	}
	Appendf( text, "Die Welt endet in allen Läufen %s\n", same ? "gleich" : "verschieden" );
	for ( int run = 1; run < comparison.runCount; ++run )
	{
		FrameStats stats = GetFrameStats( comparison.frameTimes[run] );
		double frames = b3MaxFloat( (float)comparison.frames[run], 1.0f );
		Appendf( text,
				 "  Lauf %d, %d Threads: %d Bilder, %.2f ms pro Bild, Simulation %.2f ms, 1%% Low %.0f FPS, längste Simulation "
				 "%.1f ms\n",
				 run, comparison.runWorkers[run], comparison.frames[run], stats.average,
				 (float)( comparison.simulationTime[run] / frames ), stats.lowFps, comparison.longestSimulation[run] );
	}

	int length = nbFormatScalingReport( profiles, profileCount, nullptr, 0 );
	std::string table( (size_t)length, ' ' );
	nbFormatScalingReport( profiles, profileCount, table.data(), length + 1 );
	text += table;
	comparison.report = text;

	nbScaling scaling = nbFitScaling( profiles, profileCount );
	float total8 = nbScaling_Project( &scaling, nb_profileTotal, 8 );
	char summary[160];
	snprintf( summary, sizeof( summary ),
			  "Mit 8 Threads hochgerechnet %.2f ms Physik pro Bild,\n%.0f %% davon seriell. In der Zwischenablage.", total8,
			  100.0f * scaling.serial[nb_profileTotal] / b3MaxFloat( total8, 0.001f ) );
	comparison.summary = summary;
	sapp_set_clipboard_string( text.c_str() );

	if ( app.automation.compare )
	{
		printf( "%s", text.c_str() );
		sapp_request_quit();
	}
	StopComparison( app );
}

// The report goes to the clipboard, to paste into a chat
static void FinishComparison( App& app )
{
	Comparison& comparison = app.comparison;
	if ( comparison.kind == Comparison::Scaling )
	{
		FinishScaling( app );
		return;
	}

	ComparisonResult parallel = SumComparison( comparison, true );
	ComparisonResult serial = SumComparison( comparison, false );
	bool same = true;
	for ( int run = 2; run < Comparison::RunCount; ++run )
	{
		same = same && comparison.hashes[run] == comparison.hashes[1];
	}

	int storeys = 0;
	for ( int house = 0; house < TownRows * TownColumns; ++house )
	{
		storeys += TownHouseFloors( house );
	}
	float gain = 100.0f * ( serial.frames.average - parallel.frames.average ) / b3MaxFloat( serial.frames.average, 0.001f );
	const char* direction = gain >= 0.0f ? "schneller" : "langsamer";

	b3Version version = b3GetVersion();
	std::string text;
	Appendf( text, "Nebenan-Vergleich: Physik parallel zum Zeichnen gegen nacheinander, %s-Build, Box3D %d.%d.%d\n", s_buildType,
			 version.major, version.minor, version.revision );
	AppendMachine( text, app );
	const Demolition& demolition = comparison.demolition;
	Appendf( text, "Abriss der Stadt Haus für Haus mit %d Granaten, bis zu %d pro Bild, solange weniger als %d Trümmer fliegen, "
			 "Bruchstückgröße x%.2f, Budget %d, höchstens %d Trümmer zugleich unterwegs, Physiktakt ",
			 demolition.shots, DemolitionShotsPerFrame, DemolitionMovingLimit, app.fragmentScale, DemolitionBudget, demolition.maxMoving );
	AppendStepRate( text, app );
	Appendf( text, ", 1/60 s pro Bild\n" );
	if ( demolition.doneFrame >= 0 )
	{
		Appendf( text, "Alle %d Häuser nach %d Bildern unten, %d Bilder pro Lauf, erst einer mit %d zum Aufwärmen, der nicht zählt\n",
				 demolition.house, demolition.doneFrame, comparison.frames[1], Comparison::WarmupFrames );
	}
	else
	{
		Appendf( text, "Nach %d Bildern erst %d Häuser unten, erst ein Lauf mit %d zum Aufwärmen, der nicht zählt\n", comparison.frames[1],
				 demolition.house, Comparison::WarmupFrames );
	}
	Appendf( text, "Am Ende: %d von %d Etagen eingestürzt, %d Bruchstücke, %d davon als Trümmer, %d Schutt\n",
			 comparison.stats.collapsedStoreyCount, storeys, comparison.stats.chunkCount, comparison.stats.dynamicBodyCount,
			 comparison.stats.rubbleCount );
	Appendf( text, "Parallel mit %d Threads: %.2f ms pro Bild, %.0f FPS, 1%% Low %.0f FPS; Simulation %.2f, Grafik %.2f, Warten %.2f ms\n",
			 parallel.threads, parallel.frames.average, 1000.0f / b3MaxFloat( parallel.frames.average, 0.001f ),
			 parallel.frames.lowFps, parallel.simulationTime, parallel.graphicsTime, parallel.waitTime );
	Appendf( text, "Nacheinander mit %d Threads: %.2f ms pro Bild, %.0f FPS, 1%% Low %.0f FPS; Simulation %.2f, Grafik %.2f ms\n",
			 serial.threads, serial.frames.average, 1000.0f / b3MaxFloat( serial.frames.average, 0.001f ), serial.frames.lowFps,
			 serial.simulationTime, serial.graphicsTime );
	Appendf( text, "Parallel ist %.1f %% %s, die Welt endet in allen Läufen %s\n", fabsf( gain ), direction,
			 same ? "gleich" : "verschieden" );

	// Each run on its own, to see where the slow frames come from
	for ( int run = 0; run < Comparison::RunCount; ++run )
	{
		FrameStats stats = GetFrameStats( comparison.frameTimes[run] );
		bool runParallel = Comparison::IsParallel( run );
		char name[32];
		if ( run == 0 )
		{
			snprintf( name, sizeof( name ), "Aufwärmen" );
		}
		else
		{
			snprintf( name, sizeof( name ), "Lauf %d", run );
		}
		Appendf( text, "  %s, %s: %.2f ms pro Bild, 1%% Low %.0f FPS, längstes Bild %.1f ms, längste Simulation %.1f ms", name,
				 runParallel ? "parallel" : "nacheinander", stats.average, stats.lowFps, stats.longest,
				 comparison.longestSimulation[run] );
		if ( runParallel )
		{
			Appendf( text, ", längstes Warten %.1f ms", comparison.longestWait[run] );
		}
		Appendf( text, "\n" );
	}
	comparison.report = text;

	char summary[160];
	snprintf( summary, sizeof( summary ), "Parallel %.2f ms, nacheinander %.2f ms pro Bild,\nparallel %.0f %% %s. In der Zwischenablage.",
			  parallel.frames.average, serial.frames.average, fabsf( gain ), direction );
	comparison.summary = summary;
	sapp_set_clipboard_string( text.c_str() );

	if ( app.automation.compare )
	{
		printf( "%s", text.c_str() );
		sapp_request_quit();
	}
	StopComparison( app );
}

// The first frame of a run also waited for the town to load, it does not count
static void AdvanceComparison( App& app, float frameTime, float graphicsTime )
{
	Comparison& comparison = app.comparison;
	int run = comparison.run;
	if ( comparison.frame > 0 )
	{
		comparison.frameTimes[run].push_back( frameTime );
	}
	comparison.simulationTime[run] += app.simulationTime;
	comparison.graphicsTime[run] += graphicsTime;
	comparison.waitTime[run] += app.waitTime;
	comparison.longestSimulation[run] = b3MaxFloat( comparison.longestSimulation[run], app.simulationTime );
	comparison.longestWait[run] = b3MaxFloat( comparison.longestWait[run], app.waitTime );

	comparison.frame += 1;
	const Demolition& demolition = app.demolition;
	bool settled = demolition.doneFrame >= 0 && comparison.frame >= demolition.doneFrame + Comparison::SettleFrames;
	if ( run == 0 ? comparison.frame < Comparison::WarmupFrames : settled == false && comparison.frame < Comparison::MaxRunFrames )
	{
		return;
	}

	// The worlds after the steps of the last frame
	FinishPhysics( app );
	ApplyPhysics( app );
	SyncChunks( app );
	comparison.hashes[run] = HashChunkTransforms( app );
	comparison.frames[run] = comparison.frame;
	comparison.stats = nbWorld_GetStats( app.destruction );
	comparison.profiles[run] = nbWorld_GetProfile( app.destruction );
	comparison.demolition = demolition;
	comparison.run += 1;
	if ( comparison.run < comparison.runCount )
	{
		BeginComparisonRun( app );
	}
	else
	{
		FinishComparison( app );
	}
}

//----------------------------------------------------------------------------------------------------------------------
// sokol callbacks

static void OnInit()
{
	App& app = *s_app;

	sg_desc desc = {};
	desc.environment = sglue_environment();
	desc.logger.func = slog_func;
	desc.buffer_pool_size = 1024;
	desc.view_pool_size = 1024;
	sg_setup( &desc );

	simgui_desc_t uiDesc = {};
	uiDesc.color_format = desc.environment.defaults.color_format;
	uiDesc.depth_format = desc.environment.defaults.depth_format;
	uiDesc.sample_count = desc.environment.defaults.sample_count;
	uiDesc.logger.func = slog_func;
	simgui_setup( &uiDesc );

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;
	style.FrameRounding = 4.0f;
	style.ScaleAllSizes( sapp_dpi_scale() );
	ImGui::GetIO().FontGlobalScale = sapp_dpi_scale();
	ImGui::GetIO().IniFilename = nullptr;

	app.renderer.Init();
	app.vsyncAvailable = DemoSetVsync( app.vsync );
	app.vsync = app.vsync || app.vsyncAvailable == false;
	app.refreshRate = DemoRefreshRate();
	app.unthrottled = DemoUnthrottled();

	// For the report, the threads come from nbGetRecommendedWorkerCount, see PreferredWorkers
	app.performanceCores = nbGetPerformanceCoreCount();
	app.cpuName = SystemCpuName();
	app.gpuName = SystemGpuName();

	app.camera.fovY = 60.0f * B3_PI / 180.0f;
	app.tools[ToolRifle] = { 0.35f, 1.2e5f, 9.0f, 0, 8.0f };
	app.tools[ToolGrenade] = { 1.3f, 3.0e5f, 12.0f, 0, 2.0f };
	app.tools[ToolCannon] = { 0.25f, 0.0f, 45.0f, 0, 3.0f };

	int scene = app.automation.scene >= 0 && app.automation.scene < SceneCount ? app.automation.scene : SceneWall;
	LoadScene( app, (SceneKind)scene );
	if ( app.automation.compare )
	{
		app.comparison.kind = app.automation.scaling ? Comparison::Scaling : Comparison::ParallelAgainstSerial;
		StartComparison( app );
	}
	app.frameTicks = b3GetTicks();
}

static void PrintAutomationReport( App& app )
{
	nbStats stats = nbWorld_GetStats( app.destruction );
	printf( "scene %d frame %d: chunks %d bonds %d debris %d rubble %d\n", (int)app.scene, app.frame, stats.chunkCount, stats.bondCount,
			stats.dynamicBodyCount, stats.rubbleCount );
	printf( "  physics %s, chunk transforms hash %016llx\n", app.parallelPhysics ? "parallel" : "serial",
			(unsigned long long)HashChunkTransforms( app ) );
	if ( app.automation.demolition )
	{
		printf( "  demolition: %d grenades, %d of %d houses down, the last at frame %d, %d storeys collapsed, at most %d debris moving\n",
				app.demolition.shots, app.demolition.house, (int)app.townHouses.size(), app.demolition.doneFrame,
				stats.collapsedStoreyCount, app.demolition.maxMoving );
	}

	RenderStats rs = app.renderer.GetStats();
	printf( "  render: draw calls %d, vertices %d (%d of removed meshes), pages %d, slots %d\n", rs.drawCalls, rs.vertexCount,
			rs.deadVertexCount, rs.pageCount, rs.slotCount );

	// Per frame averages, 95th percentile and maximum
	struct Series
	{
		const char* name;
		const std::vector<float>* values;
		float scale;
	};
	Series series[] = {
		{ "frame ms", &app.automation.realFrameTimes, 1.0f },
		{ "simulation ms", &app.automation.simulationTimes, 1.0f },
		{ "cpu frame ms", &app.automation.frameTimes, 1.0f },
		{ "graphics ms", &app.automation.graphicsTimes, 1.0f },
		{ "wait ms", &app.automation.waitTimes, 1.0f },
		{ "upload ms", &app.automation.uploadTimes, 1.0f },
		{ "uploaded kB", &app.automation.uploadedBytes, 1.0f / 1024.0f },
		{ "triangles k", &app.automation.triangles, 1.0f / 1000.0f },
	};
	for ( const Series& entry : series )
	{
		std::vector<float> values = *entry.values;
		std::sort( values.begin(), values.end() );
		float total = 0.0f;
		for ( float v : values )
		{
			total += v;
		}
		size_t n = values.size();
		printf( "  %-14s avg %9.2f  p95 %9.2f  max %9.2f\n", entry.name, entry.scale * total / (float)b3MaxInt( (int)n, 1 ),
				n > 0 ? entry.scale * values[n * 95 / 100] : 0.0f, n > 0 ? entry.scale * values[n - 1] : 0.0f );
	}

	printf( "%s", BuildReport( app ).c_str() );
}

static void OnFrame()
{
	App& app = *s_app;
	float frameTime = b3GetMillisecondsAndReset( &app.frameTicks );
	app.fpsMeter.Add( frameTime );

	float dt = (float)sapp_frame_duration();
	dt = b3ClampFloat( dt, 0.0f, 0.1f );

	// Automated runs and the comparison advance the physics 1/60 s per frame, one step at 60 Hz, so they repeat exactly on
	// any machine and take the same time at every step rate
	if ( app.automation.frameLimit > 0 || app.comparison.active )
	{
		dt = 1.0f / 60.0f;
	}

	// With parallel physics the steps of the frame before ran while it was drawn, and this frame shows their result.
	// Without, the frame before took its steps and their poses itself.
	uint64_t cpuTicks = b3GetTicks();
	app.simulationTime = 0.0f;
	app.waitTime = 0.0f;
	uint64_t graphicsTicks = b3GetTicks();
	float graphicsTime = 0.0f;
	if ( app.parallelPhysics )
	{
		FinishPhysics( app );
		graphicsTicks = b3GetTicks();
		ApplyPhysics( app );
		graphicsTime = b3GetMilliseconds( graphicsTicks );
	}

	// During the comparison the camera stays, it decides what is drawn
	if ( app.comparison.active == false )
	{
		UpdateCamera( app, dt );
	}

	// Automatic fire while the button is held. The worlds change in the same order as without parallel physics: the
	// impacts of a frame come after the steps of the frame before and before its own.
	uint64_t impactTicks = b3GetTicks();
	app.fireCooldown -= dt;
	bool uiWantsMouse = ImGui::GetIO().WantCaptureMouse && app.mouseLook == false;
	if ( app.mouseDown && uiWantsMouse == false && app.fireCooldown <= 0.0f && app.comparison.active == false )
	{
		float x = app.mouseLook ? 0.5f * sapp_widthf() : app.mouseX;
		float y = app.mouseLook ? 0.5f * sapp_heightf() : app.mouseY;
		Fire( app, x, y );
		app.fireCooldown = 1.0f / app.tools[app.tool].rate;
	}

	if ( app.automation.script )
	{
		RunScript( app, app.frame );
	}
	else if ( app.automation.demolition )
	{
		RunDemolition( app, app.frame );
	}
	else if ( app.comparison.active )
	{
		RunDemolition( app, app.comparison.frame );
	}
	float impactTime = b3GetMilliseconds( impactTicks );

	if ( app.parallelPhysics )
	{
		graphicsTicks = b3GetTicks();
		SyncChunks( app );
		TakeWorldView( app );
		graphicsTime += b3GetMilliseconds( graphicsTicks );
		StartPhysics( app, dt );
	}
	else
	{
		StartPhysics( app, dt );
		FinishPhysics( app );
		graphicsTicks = b3GetTicks();
		ApplyPhysics( app );
		SyncChunks( app );
		TakeWorldView( app );
		graphicsTime += b3GetMilliseconds( graphicsTicks );
	}

	graphicsTicks = b3GetTicks();
	int width = sapp_width();
	int height = sapp_height();

	simgui_frame_desc_t frameDesc = {};
	frameDesc.width = width;
	frameDesc.height = height;
	frameDesc.delta_time = sapp_frame_duration();
	frameDesc.dpi_scale = sapp_dpi_scale();
	simgui_new_frame( &frameDesc );
	if ( app.showUi )
	{
		DrawUi( app );
	}
	DrawOverlay( app );

	app.renderer.Render( app.camera, width, height );
	simgui_render();
	sg_end_pass();
	sg_commit();
	graphicsTime += b3GetMilliseconds( graphicsTicks );

	int slot = app.historyNext;
	app.historyFrame[slot] = frameTime;
	app.historyImpacts[slot] = impactTime;
	app.historySimulation[slot] = app.simulationTime;
	app.historyGraphics[slot] = graphicsTime;
	app.historyWait[slot] = app.waitTime;
	app.historyNext = ( slot + 1 ) % App::HistorySize;
	app.historyCount = b3MinInt( app.historyCount + 1, App::HistorySize );

	if ( app.automation.frameLimit > 0 )
	{
		RenderStats rs = app.renderer.GetStats();
		app.automation.realFrameTimes.push_back( frameTime );
		app.automation.simulationTimes.push_back( app.simulationTime );
		app.automation.frameTimes.push_back( b3GetMilliseconds( cpuTicks ) );
		app.automation.graphicsTimes.push_back( graphicsTime );
		app.automation.waitTimes.push_back( app.waitTime );
		app.automation.uploadTimes.push_back( rs.uploadTime );
		app.automation.uploadedBytes.push_back( (float)rs.uploadedBytes );
		app.automation.triangles.push_back( (float)rs.triangleCount );
	}

	if ( app.comparison.requested )
	{
		app.comparison.requested = false;
		StartComparison( app );
	}
	else if ( app.comparison.active )
	{
		AdvanceComparison( app, frameTime, graphicsTime );
	}

	app.frame += 1;
	if ( app.automation.frameLimit > 0 && app.frame == app.automation.frameLimit )
	{
		// The report counts the worlds after the steps of the last frame
		FinishPhysics( app );
		ApplyPhysics( app );
		SyncChunks( app );
		TakeWorldView( app );
		PrintAutomationReport( app );
		if ( app.automation.screenshotPath != nullptr )
		{
			DemoSaveScreenshot( app.automation.screenshotPath, width, height );
		}
		sapp_request_quit();
	}
}

static void OnEvent( const sapp_event* event )
{
	App& app = *s_app;
	simgui_handle_event( event );
	ImGuiIO& io = ImGui::GetIO();

	switch ( event->type )
	{
		case SAPP_EVENTTYPE_KEY_DOWN:
			if ( (int)event->key_code < SAPP_MAX_KEYCODES )
			{
				app.keys[event->key_code] = true;
			}
			if ( io.WantCaptureKeyboard )
			{
				break;
			}

			// During the comparison only the menu goes on and off
			if ( app.comparison.active && event->key_code != SAPP_KEYCODE_F1 )
			{
				break;
			}
			if ( event->key_code == SAPP_KEYCODE_1 || event->key_code == SAPP_KEYCODE_2 || event->key_code == SAPP_KEYCODE_3 )
			{
				app.tool = (Tool)( event->key_code - SAPP_KEYCODE_1 );
			}
			else if ( event->key_code == SAPP_KEYCODE_R )
			{
				LoadScene( app, app.scene );
			}
			else if ( event->key_code == SAPP_KEYCODE_P )
			{
				app.paused = !app.paused;
			}
			else if ( event->key_code == SAPP_KEYCODE_C )
			{
				nbWorld_ClearDebris( app.destruction );
			}
			else if ( event->key_code == SAPP_KEYCODE_F1 )
			{
				app.showUi = !app.showUi;
			}
			else if ( event->key_code == SAPP_KEYCODE_V && app.vsyncAvailable )
			{
				app.vsync = !app.vsync;
				DemoSetVsync( app.vsync );
			}
			else if ( event->key_code == SAPP_KEYCODE_ESCAPE && app.mouseLook )
			{
				app.mouseLook = false;
				sapp_lock_mouse( false );
			}
			break;

		case SAPP_EVENTTYPE_KEY_UP:
			if ( (int)event->key_code < SAPP_MAX_KEYCODES )
			{
				app.keys[event->key_code] = false;
			}
			break;

		case SAPP_EVENTTYPE_MOUSE_DOWN:
			if ( event->mouse_button == SAPP_MOUSEBUTTON_LEFT )
			{
				if ( io.WantCaptureMouse == false || app.mouseLook )
				{
					app.mouseDown = true;
					app.fireCooldown = 0.0f;
				}
			}
			else if ( event->mouse_button == SAPP_MOUSEBUTTON_RIGHT && io.WantCaptureMouse == false )
			{
				app.mouseLook = true;
				sapp_lock_mouse( true );
			}
			break;

		case SAPP_EVENTTYPE_MOUSE_UP:
			if ( event->mouse_button == SAPP_MOUSEBUTTON_LEFT )
			{
				app.mouseDown = false;
			}
			else if ( event->mouse_button == SAPP_MOUSEBUTTON_RIGHT )
			{
				app.mouseLook = false;
				sapp_lock_mouse( false );
			}
			break;

		case SAPP_EVENTTYPE_MOUSE_MOVE:
			if ( app.mouseLook )
			{
				if ( app.comparison.active == false )
				{
					app.camera.yaw += 0.0025f * event->mouse_dx;
					app.camera.pitch = b3ClampFloat( app.camera.pitch - 0.0025f * event->mouse_dy, -1.5f, 1.5f );
				}
			}
			else
			{
				app.mouseX = event->mouse_x;
				app.mouseY = event->mouse_y;
			}
			break;

		case SAPP_EVENTTYPE_MOUSE_SCROLL:
			if ( io.WantCaptureMouse == false && app.comparison.active == false )
			{
				app.camera.position = b3MulAdd( app.camera.position, 0.5f * event->scroll_y, app.camera.Forward() );
			}
			break;

		// A minimized window would otherwise draw as fast as it can
		case SAPP_EVENTTYPE_ICONIFIED:
			if ( app.vsyncAvailable )
			{
				DemoSetVsync( true );
			}
			break;

		case SAPP_EVENTTYPE_RESTORED:
			if ( app.vsyncAvailable )
			{
				DemoSetVsync( app.vsync );
			}
			break;

		default:
			break;
	}
}

static void OnCleanup()
{
	App& app = *s_app;
	DestroyScene( app );

	app.renderer.Shutdown();
	simgui_shutdown();
	sg_shutdown();
}

int main( int argc, char** argv )
{
	static App app;
	s_app = &app;

	for ( int i = 1; i < argc; ++i )
	{
		if ( strcmp( argv[i], "--frames" ) == 0 && i + 1 < argc )
		{
			app.automation.frameLimit = atoi( argv[++i] );
		}
		else if ( strcmp( argv[i], "--screenshot" ) == 0 && i + 1 < argc )
		{
			app.automation.screenshotPath = argv[++i];
		}
		else if ( strcmp( argv[i], "--script" ) == 0 )
		{
			app.automation.script = true;
		}
		else if ( strcmp( argv[i], "--scene" ) == 0 && i + 1 < argc )
		{
			app.automation.scene = atoi( argv[++i] );
		}
		else if ( strcmp( argv[i], "--msaa" ) == 0 && i + 1 < argc )
		{
			app.automation.msaa = b3ClampInt( atoi( argv[++i] ), 1, 8 );
		}
		else if ( strcmp( argv[i], "--highdpi" ) == 0 )
		{
			app.automation.highDpi = true;
		}
		else if ( strcmp( argv[i], "--vsync" ) == 0 )
		{
			app.vsync = true;
		}
		else if ( strcmp( argv[i], "--fragment-scale" ) == 0 && i + 1 < argc )
		{
			app.fragmentScale = b3ClampFloat( (float)atof( argv[++i] ), 0.25f, 8.0f );
		}
		else if ( strcmp( argv[i], "--threads" ) == 0 && i + 1 < argc )
		{
			app.automation.threads = b3ClampInt( atoi( argv[++i] ), 1, 8 );
		}
		else if ( strcmp( argv[i], "--serial-physics" ) == 0 )
		{
			app.parallelPhysics = false;
		}
		else if ( strcmp( argv[i], "--step-rate" ) == 0 && i + 1 < argc )
		{
			app.stepRate = (float)atof( argv[++i] );
		}
		else if ( strcmp( argv[i], "--compare" ) == 0 )
		{
			app.automation.compare = true;
		}
		else if ( strcmp( argv[i], "--scaling" ) == 0 )
		{
			app.automation.compare = true;
			app.automation.scaling = true;
		}
		else if ( strcmp( argv[i], "--demolition" ) == 0 )
		{
			app.automation.demolition = true;
			app.automation.scene = SceneTown;
			app.maxDebrisBodies = DemolitionBudget;
		}
	}

	sapp_desc desc = {};
	desc.init_cb = OnInit;
	desc.frame_cb = OnFrame;
	desc.event_cb = OnEvent;
	desc.cleanup_cb = OnCleanup;
	desc.width = 1600;
	desc.height = 900;
	desc.sample_count = app.automation.msaa;
	desc.high_dpi = app.automation.highDpi;
	desc.enable_clipboard = true;
	desc.clipboard_size = 8192;
	desc.window_title = "Nebenan - polygonale Zerstörung mit Box3D";
	desc.gl.major_version = 4;
	desc.gl.minor_version = 3;
	desc.logger.func = slog_func;
	sapp_run( &desc );
	return 0;
}
