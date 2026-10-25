// SPDX-License-Identifier: MIT
// Nebenan demo: walls that break into polygon fragments on Box3D, built for speed. Flat shading, no shadows, no
// particles: every millisecond goes to the destruction.

#include "image_file.h"
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
	SceneFigures,
	SceneKeyframes,
	SceneCount
};

static const char* s_sceneNames[SceneCount] = { "Mauern", "Stresstest", "Stadt", "Große Stadt", "Figuren", "Keyframes" };
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

// A figure of the library with the meshes of its bones, which follow the bodies, and the thinner dark meshes of the pose
// its joints drive to, drawn over them, see BuildFigureScene. With balance two flat markers on the ground show its
// capture point and where a step lands.
struct FigureVisual
{
	nbArticulationId id;
	std::vector<int> boneSlots;
	std::vector<int> boneMeshes;
	std::vector<int> targetSlots;
	std::vector<int> targetMeshes;
	int captureSlot = -1;
	int captureMesh = -1;
	int stepSlot = -1;
	int stepMesh = -1;
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


// What the menu shows of a figure: the largest error and motor load of its joints, its hits, the time of its clip and its
// balance
struct FigureView
{
	float error = 0.0f;
	float load = 0.0f;
	int hits = 0;
	float clipTime = 0.0f;
	nbBalance balance = {};
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
	std::vector<FigureView> figures;

	// The figures as a whole, see nbWorld_GetFigureStats, and the profile of the library since the scene loaded, with
	// what the figures cost, both only with figures
	nbFigureStats figureStats = {};
	nbProfile profile = {};
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
// alternately with and without parallel physics, each with the threads the library picks for it. Every run takes one
// step per frame like an automated run, so all do the same work and end in the same world. The same runs measure the
// threads the library takes against one fewer, with and without parallel physics, see nbGetRecommendedWorkerCount, and
// how the physics scales with the threads: the town taken down with 1, 2, 4 and so on threads, see
// nbGetScalingWorkerCounts, all with serial physics, and the profile of the library fitted over them, see
// nbFormatScalingReport.
struct Comparison
{
	enum Kind
	{
		ParallelAgainstSerial,
		ThreadCounts,
		Scaling,
	};

	// A first, shorter run warms up and does not count. Then come two runs with and two without parallel physics, with
	// first and last, so a slow drift of the machine falls on both alike. The thread counts take both ways with both
	// counts, and then the same runs the other way round. A counted run ends when the debris had time to settle after the
	// last house came down. The scaling warms up with the most threads and then counts a run for each number of threads.
	static const int RunCount = 5;
	static const int MaxRuns = 9;
	static const int WarmupFrames = 300;
	static const int SettleFrames = 300;
	static const int MaxRunFrames = 4000;

	Kind kind = ParallelAgainstSerial;
	int runCount = RunCount;

	// Whether every run steps the physics on a thread of its own and with how many threads, zero for those the library
	// picks, and for the scaling the profile of the library over every run
	bool runParallel[MaxRuns] = {};
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

	// What is left of the town at the end, what its figures did, and how the demolition went
	nbStats stats = {};
	nbFigureStats figureStats = {};
	Demolition demolition;

	// The report of the last comparison, and its short form for the menu
	std::string report;
	std::string summary;
};

// Whether a run of the comparison steps the physics on a thread of its own. The scaling runs all without.
static bool RunIsParallel( const Comparison& comparison, int run )
{
	return comparison.runParallel[run];
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
	const char* way = RunIsParallel( comparison, comparison.run ) ? "parallel" : "nacheinander";
	if ( comparison.kind == Comparison::ThreadCounts )
	{
		snprintf( text, size, "%s, Lauf %d von %d, %s, %d Threads", kind, comparison.run, comparison.runCount - 1, way,
				  comparison.runWorkers[comparison.run] );
		return;
	}
	snprintf( text, size, "%s, Lauf %d von %d, %s", kind, comparison.run, comparison.runCount - 1, way );
}

struct Automation
{
	int frameLimit = 0;
	const char* screenshotPath = nullptr;
	bool script = false;
	int scene = -1;

	// Run the comparison, the one of the thread counts or the scaling, print it and quit, or take the town down like the
	// comparison does
	bool compare = false;
	bool threadCounts = false;
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

// The keyframe editor, see BuildEditorScene: the humanoid, the clip it edits and where it is saved, the frame, the chosen
// keyframe and bone, the checks of the clip, the meshes of its pose, the figure that plays it, the clip the library records
// of that figure with what it finds in it, and the reference image of the frame
struct Editor
{
	nbBoneDef bones[nb_humanoidBoneCount] = {};
	nbRigDef rig = {};
	float rootHeight = 0.0f;
	nbPoseClip* clip = nullptr;
	char path[256] = "clip.json";
	char proposals[256] = "vorschlaege.json";
	char images[256] = "frames/%04d.bmp";
	int firstImage = 1;
	std::string message;

	float frame = 0.0f;
	bool playing = false;
	int keyframe = -1;
	int bone = nb_humanoidUpperArmLeft;
	int spread = 0;
	std::vector<nbPoseIssue> issues;
	bool changed = true;

	std::vector<int> poseSlots;
	std::vector<int> poseMeshes;
	int markerSlot = -1;
	int markerMesh = -1;

	nbArticulationId figure = {};
	bool simulating = false;
	bool balance = false;
	float motorError = 0.0f;
	int motorBone = -1;
	nbBalanceState balanceState = nb_balanceOff;

	// The recording, see nbArticulation_RecordPoseClip, and what the menu shows of it, taken between the steps, which
	// write it: how far it fell behind the clip, its issues with their frames, and its events
	bool bake = false;
	nbPoseClip* baked = nullptr;
	bool recording = false;
	int bakedCount = -1;
	float behind = 0.0f;
	int behindFrame = -1;
	int behindBone = -1;
	std::vector<nbPoseIssue> bakedIssues;
	std::vector<int> bakedFrames;
	std::vector<nbPoseEvent> bakedEvents;

	int imageIndex = -1;
	int imageWidth = 0;
	int imageHeight = 0;
	sg_image image = {};
	sg_view imageView = {};
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

	// The figures of the figure scene, the clip of three keyframes the second one plays, the motors of the figures that
	// stand and whether they keep their balance, whether the pose they drive to and their balance are drawn, how hard a
	// shot hits them and the number of the next hit
	std::vector<FigureVisual> figures;
	std::vector<b3Quat> clipPoses;
	std::vector<nbKeyframe> clipKeyframes;
	nbClipDef armClip = {};
	nbMotorProfile figureProfile = nbDefaultMotorProfile();
	bool figureMotors = true;
	bool figureBalance = true;
	bool showTargets = true;
	bool showBalance = true;
	float hitImpulse = 60.0f;
	Editor editor;

	// Humanoids in the streets of the town, see AddTownFigures
	int townFigures = 0;
	uint32_t hitCounter = 0;
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

	// How the storeys give way, the others and the top one, see nbStoreyCollapse: with "Zufall je Etage" the ranges of
	// the library, from which every storey picks its values, and the sliders are off, without it every storey takes the
	// values of the sliders
	bool storeyRandom = true;
	const nbStoreyCollapse randomCollapse[2] = { nbDefaultWorldDef().storeyCollapse, nbDefaultWorldDef().topStoreyCollapse };
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

// A capsule between two points with smooth normals, for a bone
static void BuildCapsuleMesh( std::vector<GpuVertex>& vertices, std::vector<uint16_t>& indices, b3Vec3 start, b3Vec3 end,
							  float radius, int material, int slot )
{
	vertices.clear();
	indices.clear();

	b3Vec3 axis = b3Sub( end, start );
	float length = b3Length( axis );
	axis = length > 1.0e-6f ? b3MulSV( 1.0f / length, axis ) : b3Vec3_axisY;
	b3Vec3 helper = fabsf( axis.y ) < 0.9f ? b3Vec3_axisY : b3Vec3_axisX;
	b3Vec3 u = b3Normalize( b3Cross( helper, axis ) );
	b3Vec3 v = b3Cross( axis, u );

	// Rings of the lower hemisphere around the start, then of the upper around the end
	const int segments = 12;
	const int rings = 4;
	for ( int half = 0; half < 2; ++half )
	{
		b3Vec3 center = half == 0 ? start : end;
		for ( int i = 0; i <= rings; ++i )
		{
			float latitude = ( half == 0 ? -0.5f : 0.0f ) * B3_PI + 0.5f * B3_PI * (float)i / (float)rings;
			float c = cosf( latitude );
			float s = sinf( latitude );
			for ( int k = 0; k < segments; ++k )
			{
				float theta = 2.0f * B3_PI * (float)k / (float)segments;
				b3Vec3 n = b3MulAdd( b3MulSV( c, b3Add( b3MulSV( cosf( theta ), u ), b3MulSV( sinf( theta ), v ) ) ), s, axis );
				vertices.push_back( Renderer::MakeVertex( b3MulAdd( center, radius, n ), n, material, slot ) );
			}
		}
	}

	int rows = 2 * ( rings + 1 );
	for ( int row = 0; row + 1 < rows; ++row )
	{
		for ( int k = 0; k < segments; ++k )
		{
			uint16_t a = (uint16_t)( row * segments + k );
			uint16_t b = (uint16_t)( row * segments + ( k + 1 ) % segments );
			uint16_t c = (uint16_t)( ( row + 1 ) * segments + k );
			uint16_t d = (uint16_t)( ( row + 1 ) * segments + ( k + 1 ) % segments );
			indices.insert( indices.end(), { a, b, d, a, d, c } );
		}
	}
}

// A box with flat normals, for a foot. The corners of a face run around its normal, counter clockwise seen from outside.
static void BuildBoxMesh( std::vector<GpuVertex>& vertices, std::vector<uint16_t>& indices, b3Vec3 center, b3Vec3 halfExtents,
						  int material, int slot )
{
	vertices.clear();
	indices.clear();
	const float a[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
	const float b[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
	for ( int face = 0; face < 6; ++face )
	{
		int axis = face / 2;
		float sign = face % 2 == 0 ? 1.0f : -1.0f;
		b3Vec3 n = { axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f };
		b3Vec3 t = axis == 0 ? b3Vec3_axisY : ( axis == 1 ? b3Vec3_axisZ : b3Vec3_axisX );
		b3Vec3 bt = b3Cross( n, t );
		uint16_t base = (uint16_t)vertices.size();
		for ( int corner = 0; corner < 4; ++corner )
		{
			b3Vec3 d = b3Add( n, b3Add( b3MulSV( a[corner], t ), b3MulSV( b[corner], bt ) ) );
			b3Vec3 p = { center.x + d.x * halfExtents.x, center.y + d.y * halfExtents.y, center.z + d.z * halfExtents.z };
			vertices.push_back( Renderer::MakeVertex( p, n, material, slot ) );
		}
		const uint16_t order[6] = { 0, 1, 2, 0, 2, 3 };
		for ( uint16_t o : order )
		{
			indices.push_back( (uint16_t)( base + o ) );
		}
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

//----------------------------------------------------------------------------------------------------------------------
// Figures: humanoids of the library that hold a pose, play a clip or hang passive, drawn with the pose they drive to

// The bone and ghost meshes of a figure. The bones follow their bodies through the slots like the balls, the ghosts are
// moved every frame to the pose the joints drive to, see UpdateFigures.
static void AddFigureVisual( App& app, nbArticulationId id, const nbBoneDef* bones, int boneCount )
{
	FigureVisual visual;
	visual.id = id;
	for ( int i = 0; i < boneCount; ++i )
	{
		const nbBoneDef& bone = bones[i];
		for ( int ghost = 0; ghost < 2; ++ghost )
		{
			int slot = app.renderer.AllocSlot();
			int material = ghost == 0 ? MaterialPlaster : MaterialSteel;
			float scale = ghost == 0 ? 1.0f : 0.4f;
			if ( bone.shape == nb_boxBone )
			{
				BuildBoxMesh( app.vertexScratch, app.indexScratch, bone.center, b3MulSV( scale, bone.halfExtents ), material, slot );
			}
			else
			{
				BuildCapsuleMesh( app.vertexScratch, app.indexScratch, bone.start, bone.end, scale * bone.radius, material, slot );
			}
			int mesh = app.renderer.AddMesh( app.vertexScratch.data(), (int)app.vertexScratch.size(), app.indexScratch.data(),
											 (int)app.indexScratch.size(), slot );
			if ( ghost == 0 )
			{
				AttachSlot( app, nbArticulation_GetBody( id, i ), slot );
				visual.boneSlots.push_back( slot );
				visual.boneMeshes.push_back( mesh );
			}
			else
			{
				app.renderer.SetSlotVisible( slot, app.showTargets );
				visual.targetSlots.push_back( slot );
				visual.targetMeshes.push_back( mesh );
			}
		}
	}

	// The markers of the balance, hidden until UpdateFigures places them
	for ( int marker = 0; marker < 2; ++marker )
	{
		int slot = app.renderer.AllocSlot();
		b3Vec3 halfExtents = marker == 0 ? b3Vec3{ 0.06f, 0.005f, 0.06f } : b3Vec3{ 0.05f, 0.005f, 0.13f };
		BuildBoxMesh( app.vertexScratch, app.indexScratch, b3Vec3_zero, halfExtents, marker == 0 ? MaterialBrick : MaterialSteel,
					  slot );
		int mesh = app.renderer.AddMesh( app.vertexScratch.data(), (int)app.vertexScratch.size(), app.indexScratch.data(),
										 (int)app.indexScratch.size(), slot );
		app.renderer.SetSlotVisible( slot, false );
		( marker == 0 ? visual.captureSlot : visual.stepSlot ) = slot;
		( marker == 0 ? visual.captureMesh : visual.stepMesh ) = mesh;
	}
	app.figures.push_back( visual );
}

// The clip of three keyframes of chapter 33: the left arm comes up, the elbow bends, the arm goes down, around again
static void MakeArmClip( App& app, int boneCount )
{
	app.clipPoses.assign( (size_t)( 3 * boneCount ), b3Quat_identity );
	b3Quat* poses = app.clipPoses.data();
	poses[1 * boneCount + nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.3f );
	poses[1 * boneCount + nb_humanoidForearmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 1.5f );
	poses[2 * boneCount + nb_humanoidUpperArmLeft] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.2f );
	poses[2 * boneCount + nb_humanoidUpperArmRight] = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.8f );

	float times[3] = { 0.0f, 0.8f, 1.6f };
	app.clipKeyframes.resize( 3 );
	for ( int k = 0; k < 3; ++k )
	{
		app.clipKeyframes[k].time = times[k];
		app.clipKeyframes[k].rotations = poses + k * boneCount;
	}
	app.armClip.keyframes = app.clipKeyframes.data();
	app.armClip.keyframeCount = 3;
	app.armClip.boneCount = boneCount;
	app.armClip.loop = true;
}

// The motors of the figures that stand and their balance, as the menu has them. A passive figure has no balance.
static void ApplyFigureProfile( App& app )
{
	nbMotorProfile passive = nbPassiveMotorProfile();
	nbBalanceDef balance = nbDefaultBalanceDef();
	for ( size_t i = 0; i < app.figures.size(); ++i )
	{
		// The third figure stays passive
		if ( i < 2 )
		{
			nbArticulationId id = app.figures[i].id;
			nbArticulation_SetMotorProfile( id, app.figureMotors ? &app.figureProfile : &passive );
			bool wanted = app.figureMotors && app.figureBalance;
			bool has = nbArticulation_GetBalance( id ).state != nb_balanceOff;
			if ( wanted != has )
			{
				nbArticulation_EnableBalance( id, wanted ? &balance : nullptr );
			}
		}
	}
}

// Three humanoids in front of a brick wall: one stands, one plays the arm clip, one falls passive. The two that stand
// keep their balance with steps, fall and get up. The rifle hits them, the wall shows what a thrown figure does to a
// house.
static void BuildFigureScene( App& app )
{
	MaterialPreset brick = BrickPreset();
	AddWall( app, { 0.0f, 0.0f, -4.0f }, 0.0f, { 8.0f, 2.6f, 0.25f }, brick, 7 );

	nbBoneDef bones[nb_humanoidBoneCount];
	float rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	MakeArmClip( app, nb_humanoidBoneCount );

	nbMotorProfile passive = nbPassiveMotorProfile();
	float xs[3] = { -1.5f, 0.0f, 1.5f };
	for ( int i = 0; i < 3; ++i )
	{
		bool isPassive = i == 2;
		const nbMotorProfile* profile = isPassive || app.figureMotors == false ? &passive : &app.figureProfile;
		b3Vec3 position = { xs[i], rootHeight + ( isPassive ? 0.3f : 0.0f ), 0.0f };
		b3Quat rotation = isPassive ? b3MakeQuatFromAxisAngle( b3Vec3_axisX, 0.3f ) : b3Quat_identity;
		nbArticulationId id = nbCreateArticulation( app.destruction, &rig, b3ToPos( position ), rotation, profile );
		if ( i == 1 )
		{
			nbArticulation_PlayClip( id, &app.armClip, 1.0f );
		}
		if ( isPassive == false && app.figureMotors && app.figureBalance )
		{
			nbBalanceDef balance = nbDefaultBalanceDef();
			nbArticulation_EnableBalance( id, &balance );
		}
		AddFigureVisual( app, id, bones, nb_humanoidBoneCount );
	}

	app.camera.position = { 0.0f, 1.6f, 5.5f };
	app.camera.yaw = 0.0f;
	app.camera.pitch = -0.08f;
}

// Move the ghosts to the pose the joints drive to, placed on the root where it is drawn: between two steps the bones
// are drawn up to a step behind the physics, see nbWorld_GetDrawPoses
static void UpdateFigures( App& app )
{
	b3WorldTransform transforms[nb_humanoidBoneCount];
	for ( FigureVisual& figure : app.figures )
	{
		int count = nbArticulation_GetTargetTransforms( figure.id, transforms, nb_humanoidBoneCount );
		if ( count == 0 )
		{
			continue;
		}

		b3WorldTransform root = transforms[0];
		const BodySlots* slots = FindBodySlots( app, nbArticulation_GetBody( figure.id, 0 ) );
		b3WorldTransform drawn = slots != nullptr && slots->posed ? slots->pose : root;
		for ( int i = 0; i < count && i < (int)figure.targetSlots.size(); ++i )
		{
			b3WorldTransform pose = b3MulWorldTransforms( drawn, b3InvMulWorldTransforms( root, transforms[i] ) );
			app.renderer.SetSlot( figure.targetSlots[i], b3ToVec3( pose.p ), pose.q );
		}

		// The capture point on the ground while the figure is on its feet, and where the swinging foot lands, flat under
		// its ankle and turned like the figure
		nbBalance balance = nbArticulation_GetBalance( figure.id );
		bool upright = balance.state == nb_balanceStanding || balance.state == nb_balanceStepping;
		bool stepping = balance.state == nb_balanceStepping;
		app.renderer.SetSlotVisible( figure.captureSlot, app.showBalance && upright );
		app.renderer.SetSlotVisible( figure.stepSlot, app.showBalance && stepping );
		if ( upright )
		{
			b3Vec3 capture = b3ToVec3( balance.capturePoint );
			app.renderer.SetSlot( figure.captureSlot, b3Add( capture, { 0.0f, 0.006f, 0.0f } ), b3Quat_identity );
		}
		if ( stepping )
		{
			b3Vec3 facing = b3RotateVector( root.q, b3Vec3_axisZ );
			float yaw = b3Atan2( facing.x, facing.z );
			b3Vec3 target = b3ToVec3( balance.stepTarget );
			b3Vec3 sole = b3Add( target, b3RotateVector( b3MakeQuatFromAxisAngle( b3Vec3_axisY, yaw ), { 0.0f, 0.0f, 0.09f } ) );
			sole.y = b3ToVec3( balance.capturePoint ).y + 0.006f;
			app.renderer.SetSlot( figure.stepSlot, sole, b3MakeQuatFromAxisAngle( b3Vec3_axisY, yaw ) );
		}
	}
}

// One hit on the chest of every figure that stands, backward or from its right, with the impulse of the menu
static void HitChests( App& app, b3Vec3 direction )
{
	for ( size_t i = 0; i < app.figures.size() && i < 2; ++i )
	{
		b3BodyId torso = nbArticulation_GetBody( app.figures[i].id, nb_humanoidTorso );
		nbHitDef hit = {};
		hit.id = ++app.hitCounter;
		hit.bone = nb_humanoidTorso;
		hit.impulse = b3MulSV( app.hitImpulse, direction );
		hit.point = b3Body_GetWorldPoint( torso, { 0.0f, 0.3f, 0.1f } );
		nbArticulation_ApplyHit( app.figures[i].id, &hit );
	}
}

static const char* BalanceName( nbBalanceState state )
{
	static const char* names[] = { "ohne Balance", "steht", "macht einen Schritt", "fällt", "liegt", "steht auf" };
	return names[state];
}

//----------------------------------------------------------------------------------------------------------------------
// Keyframe editor: a clip of the humanoid as a file, posed by hand over reference images, checked, approved and locked,
// proposals of an estimator or an assistant taken in, and the clip played on a figure, see the roadmap of the game,
// chapters 32.5, 32.6 and 33.2. The clip, its rules and its checks are the library's, here are the menu and the drawing.

static const char* s_boneLabels[nb_humanoidBoneCount] = { "Becken",
														  "Rumpf",
														  "Kopf",
														  "Oberarm links",
														  "Unterarm links",
														  "Hand links",
														  "Oberarm rechts",
														  "Unterarm rechts",
														  "Hand rechts",
														  "Oberschenkel links",
														  "Unterschenkel links",
														  "Fuß links",
														  "Oberschenkel rechts",
														  "Unterschenkel rechts",
														  "Fuß rechts" };
static const char* s_originLabels[nb_originCount] = { "Hand", "Schätzer", "Assistent", "Simulation" };
static const char* s_issueLabels[nb_issueKindCount] = { "Knochen fehlt", "Knochen unbekannt", "nicht normiert",
														"Gelenkgrenze",	 "unsicher",		  "im Boden",
														"Sprung",		 "Fuß rutscht",		  "Knochenlänge" };

// The events of the balance in a recording, see nbArticulation_RecordPoseClip, else the name of the event
static const char* EventLabel( const char* name )
{
	static const char* names[6] = { "off", "stand", "step", "fall", "down", "getUp" };
	static const char* labels[6] = { "ohne Balance", "steht", "macht einen Schritt", "fällt", "liegt", "steht auf" };
	for ( int i = 0; i < 6; ++i )
	{
		if ( strcmp( name, names[i] ) == 0 )
		{
			return labels[i];
		}
	}
	return name;
}

static bool ReadTextFile( const char* path, std::string& text )
{
	FILE* file = fopen( path, "rb" );
	if ( file == nullptr )
	{
		return false;
	}
	fseek( file, 0, SEEK_END );
	long size = ftell( file );
	fseek( file, 0, SEEK_SET );
	text.resize( size > 0 ? (size_t)size : 0 );
	bool ok = size >= 0 && fread( &text[0], 1, text.size(), file ) == text.size();
	fclose( file );
	return ok;
}

static bool WriteTextFile( const char* path, const std::string& text )
{
	FILE* file = fopen( path, "wb" );
	if ( file == nullptr )
	{
		return false;
	}
	bool ok = fwrite( text.data(), 1, text.size(), file ) == text.size();
	fclose( file );
	return ok;
}

static std::string ClipText( const nbPoseClip* clip )
{
	std::string text( (size_t)nbWritePoseClip( clip, nullptr, 0 ) + 1, '\0' );
	nbWritePoseClip( clip, &text[0], (int)text.size() );
	text.pop_back();
	return text;
}

// Rotations by axis and angle and as angles about x, y and z in degrees, one after the other: for the sliders only, the
// clip keeps the rotations
static b3Quat AxisAngle( b3Vec3 axis, float radians )
{
	float s = sinf( 0.5f * radians );
	return { { s * axis.x, s * axis.y, s * axis.z }, cosf( 0.5f * radians ) };
}

static b3Quat EulerToQuat( b3Vec3 degrees )
{
	float r = B3_PI / 180.0f;
	return b3NormalizeQuat(
		b3MulQuat( b3MulQuat( AxisAngle( b3Vec3_axisX, degrees.x * r ), AxisAngle( b3Vec3_axisY, degrees.y * r ) ),
				   AxisAngle( b3Vec3_axisZ, degrees.z * r ) ) );
}

static b3Vec3 QuatToEuler( b3Quat q )
{
	float x = q.v.x, y = q.v.y, z = q.v.z, w = q.s;
	float r00 = 1.0f - 2.0f * ( y * y + z * z );
	float r01 = 2.0f * ( x * y - w * z );
	float r02 = 2.0f * ( x * z + w * y );
	float r12 = 2.0f * ( y * z - w * x );
	float r22 = 1.0f - 2.0f * ( x * x + y * y );
	float d = 180.0f / B3_PI;
	return { atan2f( -r12, r22 ) * d, asinf( b3ClampFloat( r02, -1.0f, 1.0f ) ) * d, atan2f( -r01, r00 ) * d };
}

// The clip of three keyframes of chapter 33.4 to start with: the left arm comes up, the elbow bends, the arm goes down
static nbPoseClip* MakeExampleClip( const Editor& e )
{
	nbPoseClip* clip = nbCreatePoseClip( &e.rig, NB_HUMANOID_SKELETON, "Beispiel", 30 );
	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root = { { 0.0f, e.rootHeight, 0.0f }, b3Quat_identity };
	for ( int k = 0; k < 3; ++k )
	{
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			pose[i] = b3Quat_identity;
		}
		if ( k == 1 )
		{
			pose[nb_humanoidUpperArmLeft] = AxisAngle( b3Vec3_axisX, 1.3f );
			pose[nb_humanoidForearmLeft] = AxisAngle( b3Vec3_axisX, 1.5f );
		}
		int index = nbPoseClip_AddKeyframe( clip, 24 * k, pose, root, nb_originHand );
		nbPoseClip_SetContacts( clip, index, nb_contactLeftFoot | nb_contactRightFoot );
	}
	nbPoseClip_SetLooping( clip, true );
	return clip;
}

static void SetEditorClip( Editor& e, nbPoseClip* clip )
{
	nbDestroyPoseClip( e.clip );
	e.clip = clip;
	e.keyframe = nbPoseClip_GetKeyframeCount( clip ) > 0 ? 0 : -1;
	e.frame = 0.0f;
	e.changed = true;
}

static void LoadEditorClip( Editor& e )
{
	std::string text;
	if ( ReadTextFile( e.path, text ) == false )
	{
		e.message = std::string( "Keine Datei " ) + e.path;
		return;
	}

	nbPoseClipError error;
	nbPoseClip* clip = nbParsePoseClip( text.data(), (int)text.size(), &error );
	if ( clip == nullptr )
	{
		char line[256];
		snprintf( line, sizeof( line ), "%s, Zeile %d, Spalte %d: %s", e.path, error.line, error.column, error.message );
		e.message = line;
		return;
	}

	nbPoseIssue issues[4];
	nbPoseCheckDef def = nbDefaultPoseCheckDef();
	int count = nbPoseClip_Check( clip, &e.rig, &def, issues, 4 );
	if ( count > 0 && ( issues[0].kind == nb_issueBoneMissing || issues[0].kind == nb_issueBoneUnknown ) )
	{
		e.message = std::string( e.path ) + " ist kein Clip des Humanoiden";
		nbDestroyPoseClip( clip );
		return;
	}
	SetEditorClip( e, clip );
	e.message = std::string( "Geladen: " ) + e.path;
}

static void SaveEditorClip( Editor& e, const nbPoseClip* clip, const char* path )
{
	e.message = WriteTextFile( path, ClipText( clip ) ) ? std::string( "Gespeichert: " ) + path
														: std::string( "Nicht gespeichert, Ordner fehlt? " ) + path;
}

// Proposals as a clip file, from an assistant, or as a keypoint file, from a pose estimator. Approved keyframes refuse them.
static void LoadProposals( Editor& e )
{
	std::string text;
	if ( ReadTextFile( e.proposals, text ) == false )
	{
		e.message = std::string( "Keine Datei " ) + e.proposals;
		return;
	}

	nbPoseClipError clipError;
	nbPoseClipError keypointError;
	nbPoseClip* proposals = nbParsePoseClip( text.data(), (int)text.size(), &clipError );
	if ( proposals == nullptr )
	{
		proposals = nbParseKeypointClip( text.data(), (int)text.size(), 1.8f, &keypointError );
	}
	if ( proposals == nullptr )
	{
		char line[256];
		snprintf( line, sizeof( line ), "Zeile %d, Spalte %d: %s", clipError.line, clipError.column, clipError.message );
		e.message = line;
		return;
	}

	int refused = 0;
	int merged = nbPoseClip_MergeProposals( e.clip, proposals, &refused );
	nbDestroyPoseClip( proposals );
	char line[160];
	if ( merged < 0 )
	{
		snprintf( line, sizeof( line ), "Die Vorschläge passen nicht: andere Knochen oder andere Bildrate" );
	}
	else
	{
		snprintf( line, sizeof( line ), "%d Vorschläge übernommen, %d von freigegebenen Keyframes abgelehnt", merged, refused );
	}
	e.message = line;
	e.changed = true;
}

// The pose meshes of the bones, solid, and a marker for the chosen bone, both placed every frame
static void BuildEditorScene( App& app )
{
	Editor& e = app.editor;
	e.rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, e.bones, nb_humanoidBoneCount );
	e.rig = nbDefaultRigDef();
	e.rig.bones = e.bones;
	e.rig.boneCount = nb_humanoidBoneCount;

	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		const nbBoneDef& bone = e.bones[i];
		int slot = app.renderer.AllocSlot();
		if ( bone.shape == nb_boxBone )
		{
			BuildBoxMesh( app.vertexScratch, app.indexScratch, bone.center, bone.halfExtents, MaterialPlaster, slot );
		}
		else
		{
			BuildCapsuleMesh( app.vertexScratch, app.indexScratch, bone.start, bone.end, bone.radius, MaterialPlaster, slot );
		}
		e.poseSlots.push_back( slot );
		e.poseMeshes.push_back( app.renderer.AddMesh( app.vertexScratch.data(), (int)app.vertexScratch.size(),
													  app.indexScratch.data(), (int)app.indexScratch.size(), slot ) );
	}
	e.markerSlot = app.renderer.AllocSlot();
	BuildBoxMesh( app.vertexScratch, app.indexScratch, b3Vec3_zero, { 0.035f, 0.035f, 0.035f }, MaterialBrick, e.markerSlot );
	e.markerMesh = app.renderer.AddMesh( app.vertexScratch.data(), (int)app.vertexScratch.size(), app.indexScratch.data(),
										 (int)app.indexScratch.size(), e.markerSlot );

	// The clip of the last session, or the example
	std::string text;
	if ( e.clip == nullptr )
	{
		if ( ReadTextFile( e.path, text ) )
		{
			LoadEditorClip( e );
		}
		if ( e.clip == nullptr )
		{
			SetEditorClip( e, MakeExampleClip( e ) );
		}
	}
	e.changed = true;

	app.camera.position = { 0.0f, 1.2f, 3.4f };
	app.camera.yaw = 0.0f;
	app.camera.pitch = -0.05f;
}

static void RemoveFigureVisual( App& app, size_t index )
{
	FigureVisual& figure = app.figures[index];
	for ( size_t i = 0; i < figure.boneSlots.size(); ++i )
	{
		if ( nbArticulation_IsValid( figure.id ) )
		{
			DetachSlot( app, nbArticulation_GetBody( figure.id, (int)i ), figure.boneSlots[i] );
		}
		app.renderer.RemoveMesh( figure.boneMeshes[i] );
		app.renderer.FreeSlot( figure.boneSlots[i] );
	}
	for ( size_t i = 0; i < figure.targetSlots.size(); ++i )
	{
		app.renderer.RemoveMesh( figure.targetMeshes[i] );
		app.renderer.FreeSlot( figure.targetSlots[i] );
	}
	app.renderer.RemoveMesh( figure.captureMesh );
	app.renderer.RemoveMesh( figure.stepMesh );
	app.renderer.FreeSlot( figure.captureSlot );
	app.renderer.FreeSlot( figure.stepSlot );
	app.figures.erase( app.figures.begin() + (long)index );
}

static void StopEditorSimulation( App& app )
{
	Editor& e = app.editor;
	if ( e.simulating == false )
	{
		return;
	}
	for ( size_t i = 0; i < app.figures.size(); ++i )
	{
		if ( app.figures[i].id.index1 == e.figure.index1 && app.figures[i].id.generation == e.figure.generation )
		{
			RemoveFigureVisual( app, i );
			break;
		}
	}
	nbDestroyArticulation( e.figure );
	e.figure = nb_nullArticulationId;
	e.simulating = false;
	e.recording = false;
}

// A figure in the first pose of the clip, at its root, that plays the clip with the motors of the figure scene. With
// baking, the library records the pose it reaches into a clip of its own, frame by frame of the clip in its steps.
static void StartEditorSimulation( App& app )
{
	Editor& e = app.editor;
	StopEditorSimulation( app );
	if ( nbPoseClip_GetKeyframeCount( e.clip ) == 0 )
	{
		return;
	}

	nbPoseKeyframe first = nbPoseClip_GetKeyframe( e.clip, 0 );
	b3WorldTransform root = { b3ToPos( first.root.p ), first.root.q };
	nbMotorProfile profile = app.figureProfile;
	e.figure = nbCreateArticulation( app.destruction, &e.rig, root.p, root.q, &profile );
	nbArticulation_PlacePose( e.figure, root, first.rotations );
	if ( e.balance )
	{
		nbBalanceDef balance = nbDefaultBalanceDef();
		nbArticulation_EnableBalance( e.figure, &balance );
	}
	nbArticulation_PlayPoseClip( e.figure, e.clip, 1.0f );
	AddFigureVisual( app, e.figure, e.bones, nb_humanoidBoneCount );
	e.simulating = true;

	nbDestroyPoseClip( e.baked );
	e.baked = nullptr;
	e.recording = false;
	e.bakedCount = -1;
	if ( e.bake )
	{
		e.baked = nbCreatePoseClip( &e.rig, NB_HUMANOID_SKELETON,
									( std::string( nbPoseClip_GetName( e.clip ) ) + " simuliert" ).c_str(),
									nbPoseClip_GetFrameRate( e.clip ) );
		nbPoseClip_SetKind( e.baked, nb_clipSimulated );
		e.recording = nbArticulation_RecordPoseClip( e.figure, e.baked, nullptr );
	}
}

// The reference image of the frame, loaded once it changes
static void LoadReferenceImage( Editor& e, int index )
{
	if ( index == e.imageIndex )
	{
		return;
	}
	e.imageIndex = index;
	if ( e.imageView.id != SG_INVALID_ID )
	{
		sg_destroy_view( e.imageView );
		sg_destroy_image( e.image );
		e.imageView = {};
		e.image = {};
	}

	char path[300];
	snprintf( path, sizeof( path ), e.images, index );
	std::vector<uint8_t> pixels;
	int width = 0;
	int height = 0;
	if ( LoadImageFile( path, &width, &height, pixels ) == false )
	{
		e.imageWidth = 0;
		return;
	}

	sg_image_desc desc = {};
	desc.width = width;
	desc.height = height;
	desc.pixel_format = SG_PIXELFORMAT_RGBA8;
	desc.data.mip_levels[0] = { pixels.data(), pixels.size() };
	e.image = sg_make_image( &desc );
	sg_view_desc view = {};
	view.texture.image = e.image;
	e.imageView = sg_make_view( &view );
	e.imageWidth = width;
	e.imageHeight = height;
}

static void DestroyEditorScene( App& app )
{
	Editor& e = app.editor;
	for ( size_t i = 0; i < e.poseSlots.size(); ++i )
	{
		app.renderer.RemoveMesh( e.poseMeshes[i] );
		app.renderer.FreeSlot( e.poseSlots[i] );
	}
	e.poseSlots.clear();
	e.poseMeshes.clear();
	if ( e.markerSlot >= 0 )
	{
		app.renderer.RemoveMesh( e.markerMesh );
		app.renderer.FreeSlot( e.markerSlot );
		e.markerSlot = -1;
	}
	if ( e.imageView.id != SG_INVALID_ID )
	{
		sg_destroy_view( e.imageView );
		sg_destroy_image( e.image );
		e.imageView = {};
		e.image = {};
	}
	e.imageIndex = -1;
	e.simulating = false;
	e.recording = false;
	e.figure = nb_nullArticulationId;
	e.playing = false;
}

// Every frame after the physics, between the steps: the frame plays on, the pose of the clip there is drawn, and what the
// menu shows of the figure and its recording is taken
static void UpdateEditor( App& app, float dt )
{
	Editor& e = app.editor;
	if ( app.scene != SceneKeyframes || e.clip == nullptr )
	{
		return;
	}

	int length = nbPoseClip_GetLength( e.clip );
	float rate = (float)nbPoseClip_GetFrameRate( e.clip );
	if ( e.simulating && nbArticulation_IsValid( e.figure ) )
	{
		// The frame of the figure is the time of its clip
		e.frame = nbArticulation_GetClipTime( e.figure ) * rate;
		if ( nbPoseClip_IsLooping( e.clip ) && length > 0 )
		{
			e.frame = fmodf( e.frame, (float)length );
		}

		// The joint furthest from its target now, and the balance
		e.motorError = 0.0f;
		e.motorBone = -1;
		for ( int i = 1; i < nb_humanoidBoneCount; ++i )
		{
			float error = nbArticulation_GetJointError( e.figure, i );
			if ( error > e.motorError )
			{
				e.motorError = error;
				e.motorBone = i;
			}
		}
		e.balanceState = nbArticulation_GetBalance( e.figure ).state;
		e.recording = nbArticulation_IsRecording( e.figure );
	}

	// The recording so far, measured against the clip and checked by the rules of the clip: slides, bones in the ground,
	// jumps, and the events of the balance
	if ( e.baked != nullptr && nbPoseClip_GetKeyframeCount( e.baked ) != e.bakedCount )
	{
		e.bakedCount = nbPoseClip_GetKeyframeCount( e.baked );
		e.behind = nbPoseClip_Compare( e.clip, e.baked, &e.behindFrame, &e.behindBone );
		nbPoseCheckDef def = nbDefaultPoseCheckDef();
		int count = nbPoseClip_Check( e.baked, &e.rig, &def, nullptr, 0 );
		e.bakedIssues.resize( (size_t)count );
		e.bakedFrames.resize( (size_t)count );
		nbPoseClip_Check( e.baked, &e.rig, &def, e.bakedIssues.data(), count );
		for ( int i = 0; i < count; ++i )
		{
			int keyframe = e.bakedIssues[i].keyframe;
			e.bakedFrames[i] = keyframe >= 0 ? nbPoseClip_GetKeyframe( e.baked, keyframe ).frame : -1;
		}
		e.bakedEvents.resize( (size_t)nbPoseClip_GetEventCount( e.baked ) );
		for ( size_t i = 0; i < e.bakedEvents.size(); ++i )
		{
			e.bakedEvents[i] = nbPoseClip_GetEvent( e.baked, (int)i );
		}
	}
	else if ( e.playing && length > 0 )
	{
		e.frame += dt * rate;
		if ( e.frame > (float)length )
		{
			e.frame = nbPoseClip_IsLooping( e.clip ) ? fmodf( e.frame, (float)length ) : (float)length;
			e.playing = nbPoseClip_IsLooping( e.clip );
		}
	}

	// The pose of the clip at the frame, beside the figure while it plays
	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root;
	nbPoseClip_Sample( e.clip, e.frame, pose, &root );
	if ( e.simulating )
	{
		root.p.x -= 1.2f;
	}
	b3Transform transforms[nb_humanoidBoneCount];
	nbComputePoseTransforms( &e.rig, root, pose, transforms );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		app.renderer.SetSlot( e.poseSlots[i], transforms[i].p, transforms[i].q );
	}
	app.renderer.SetSlot( e.markerSlot, transforms[e.bone].p, transforms[e.bone].q );

	LoadReferenceImage( e, e.firstImage + (int)e.frame );

	if ( e.changed )
	{
		nbPoseCheckDef def = nbDefaultPoseCheckDef();
		int count = nbPoseClip_Check( e.clip, &e.rig, &def, nullptr, 0 );
		e.issues.resize( (size_t)count );
		nbPoseClip_Check( e.clip, &e.rig, &def, e.issues.data(), count );
		e.changed = false;
	}
}

// The camera onto the root of the clip from the front, the side or above
static void LookAtEditorPose( App& app, int view )
{
	Editor& e = app.editor;
	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root;
	nbPoseClip_Sample( e.clip, e.frame, pose, &root );
	b3Vec3 at = root.p;
	if ( view == 0 )
	{
		app.camera.position = { at.x, at.y + 0.2f, at.z + 3.4f };
		app.camera.yaw = 0.0f;
		app.camera.pitch = -0.05f;
	}
	else if ( view == 1 )
	{
		app.camera.position = { at.x + 3.4f, at.y + 0.2f, at.z };
		app.camera.yaw = -0.5f * B3_PI;
		app.camera.pitch = -0.05f;
	}
	else
	{
		app.camera.position = { at.x, at.y + 3.4f, at.z + 0.01f };
		app.camera.yaw = 0.0f;
		app.camera.pitch = -1.55f;
	}
}

static void DrawEditorUi( App& app )
{
	Editor& e = app.editor;
	if ( e.clip == nullptr )
	{
		return;
	}

	float dpi = sapp_dpi_scale();
	ImGui::SetNextWindowPos( ImVec2( sapp_widthf() - 400.0f * dpi, 12.0f * dpi ), ImGuiCond_FirstUseEver );
	ImGui::SetNextWindowSize( ImVec2( 388.0f * dpi, 0.0f ), ImGuiCond_FirstUseEver );
	ImGui::Begin( "Keyframe-Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

	// The file
	ImGui::SeparatorText( "Clip" );
	ImGui::InputText( "Datei", e.path, sizeof( e.path ) );
	if ( ImGui::Button( "Neu" ) )
	{
		StopEditorSimulation( app );
		SetEditorClip( e, MakeExampleClip( e ) );
		e.message = "Neuer Clip aus dem Beispiel";
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Laden" ) )
	{
		StopEditorSimulation( app );
		LoadEditorClip( e );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Speichern" ) )
	{
		SaveEditorClip( e, e.clip, e.path );
	}
	if ( e.message.empty() == false )
	{
		ImGui::TextWrapped( "%s", e.message.c_str() );
	}

	// Time
	int length = nbPoseClip_GetLength( e.clip );
	ImGui::SeparatorText( "Zeit" );
	ImGui::BeginDisabled( e.simulating );
	ImGui::SliderFloat( "Frame", &e.frame, 0.0f, (float)b3MaxInt( length, 1 ), "%.0f" );
	if ( ImGui::Button( e.playing ? "Anhalten" : "Abspielen" ) )
	{
		e.playing = !e.playing;
	}
	ImGui::SameLine();
	bool loop = nbPoseClip_IsLooping( e.clip );
	if ( ImGui::Checkbox( "Schleife", &loop ) )
	{
		nbPoseClip_SetLooping( e.clip, loop );
	}
	ImGui::SameLine();
	ImGui::Text( "%d Bilder/s, %d Frames", nbPoseClip_GetFrameRate( e.clip ), length );
	ImGui::EndDisabled();

	// Keyframes
	ImGui::SeparatorText( "Keyframes" );
	int count = nbPoseClip_GetKeyframeCount( e.clip );
	if ( ImGui::BeginTable( "keyframes", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
							ImVec2( 0.0f, 150.0f * dpi ) ) )
	{
		ImGui::TableSetupColumn( "Frame" );
		ImGui::TableSetupColumn( "Status" );
		ImGui::TableSetupColumn( "Herkunft" );
		ImGui::TableSetupColumn( "Bild, sicher" );
		ImGui::TableHeadersRow();
		for ( int k = 0; k < count; ++k )
		{
			nbPoseKeyframe key = nbPoseClip_GetKeyframe( e.clip, k );
			float sure = 1.0f;
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				sure = b3MinFloat( sure, key.confidence[i] );
			}
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			char label[32];
			snprintf( label, sizeof( label ), "%d##key%d", key.frame, k );
			if ( ImGui::Selectable( label, e.keyframe == k, ImGuiSelectableFlags_SpanAllColumns ) )
			{
				e.keyframe = k;
				e.frame = (float)key.frame;
				e.playing = false;
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( key.status == nb_keyframeApproved ? "freigegeben" : "Vorschlag" );
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( s_originLabels[key.origin] );
			ImGui::TableNextColumn();
			if ( key.sourceFrame >= 0 )
			{
				ImGui::Text( "%d, %.0f %%", key.sourceFrame, 100.0f * sure );
			}
			else
			{
				ImGui::Text( "-, %.0f %%", 100.0f * sure );
			}
		}
		ImGui::EndTable();
	}

	ImGui::BeginDisabled( e.simulating );
	if ( ImGui::Button( "Keyframe hier" ) )
	{
		b3Quat pose[nb_humanoidBoneCount];
		b3Transform root;
		int frame = (int)( e.frame + 0.5f );
		nbPoseClip_Sample( e.clip, (float)frame, pose, &root );
		int index = nbPoseClip_AddKeyframe( e.clip, frame, pose, root, nb_originHand );
		if ( index >= 0 )
		{
			e.keyframe = index;
			e.frame = (float)frame;
			if ( e.imageWidth > 0 )
			{
				nbPoseClip_SetSourceFrame( e.clip, index, e.imageIndex );
			}
		}
		e.message = index >= 0 ? "Keyframe angelegt" : "Hier liegt ein freigegebener Keyframe";
		e.changed = true;
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Ein Keyframe am Frame, in der Pose, die der Clip dort hat. Mit Referenzbild merkt er sich die\n"
						   "Nummer des Bildes." );
	}

	bool valid = 0 <= e.keyframe && e.keyframe < count;
	nbPoseKeyframe key = valid ? nbPoseClip_GetKeyframe( e.clip, e.keyframe ) : nbPoseKeyframe{};
	bool approved = valid && key.status == nb_keyframeApproved;
	if ( valid )
	{
		ImGui::SameLine();
		if ( ImGui::Button( approved ? "Entsperren" : "Freigeben" ) )
		{
			nbPoseClip_Approve( e.clip, e.keyframe, approved == false );
			e.changed = true;
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Freigegeben ist ein Keyframe gesperrt: Weder Vorschläge noch Korrekturen an Nachbarn\n"
							   "ändern ihn, und von Hand erst nach dem Entsperren." );
		}
		ImGui::SameLine();
		ImGui::BeginDisabled( approved );
		if ( ImGui::Button( "Löschen" ) )
		{
			nbPoseClip_RemoveKeyframe( e.clip, e.keyframe );
			e.keyframe = -1;
			e.changed = true;
		}
		ImGui::EndDisabled();
	}

	// The pose of the chosen keyframe, bone by bone
	if ( valid && e.keyframe < nbPoseClip_GetKeyframeCount( e.clip ) )
	{
		key = nbPoseClip_GetKeyframe( e.clip, e.keyframe );
		ImGui::SeparatorText( "Pose" );
		if ( approved )
		{
			ImGui::TextDisabled( "Freigegeben und gesperrt, zum Ändern erst entsperren." );
		}
		ImGui::BeginDisabled( approved );
		ImGui::Combo( "Knochen", &e.bone, s_boneLabels, nb_humanoidBoneCount );
		ImGui::SliderInt( "Nachbarn", &e.spread, 0, 30, "%d Frames" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Eine Korrektur wirkt auch auf die Vorschläge in so vielen Frames Abstand, schwächer mit\n"
							   "dem Abstand. Freigegebene Keyframes ändert sie nie." );
		}

		b3Quat pose[nb_humanoidBoneCount];
		memcpy( pose, key.rotations, sizeof( pose ) );
		b3Transform root = key.root;
		bool edited = false;
		if ( e.bone == nb_humanoidPelvis )
		{
			b3Vec3 euler = QuatToEuler( root.q );
			edited |= ImGui::DragFloat3( "Lage", &root.p.x, 0.005f, -10.0f, 10.0f, "%.3f m" );
			if ( ImGui::DragFloat3( "Drehung", &euler.x, 0.5f, -180.0f, 180.0f, "%.1f°" ) )
			{
				root.q = EulerToQuat( euler );
				edited = true;
			}
		}
		else if ( e.bones[e.bone].joint == nb_hingeJoint )
		{
			b3Vec3 euler = QuatToEuler( pose[e.bone] );
			float bend = euler.x;
			if ( ImGui::DragFloat( "Beugung", &bend, 0.5f, -180.0f, 180.0f, "%.1f°" ) )
			{
				pose[e.bone] = AxisAngle( b3Vec3_axisX, bend * B3_PI / 180.0f );
				edited = true;
			}
		}
		else
		{
			b3Vec3 euler = QuatToEuler( pose[e.bone] );
			if ( ImGui::DragFloat3( "um x, y, z", &euler.x, 0.5f, -180.0f, 180.0f, "%.1f°" ) )
			{
				pose[e.bone] = EulerToQuat( euler );
				edited = true;
			}
		}
		if ( edited )
		{
			nbPoseClip_SetPose( e.clip, e.keyframe, pose, root, e.spread );
			e.changed = true;
		}

		static const char* contactLabels[4] = { "Fuß links", "Fuß rechts", "Hand links", "Hand rechts" };
		uint32_t contacts = key.contacts;
		for ( int c = 0; c < 4; ++c )
		{
			bool on = ( contacts & ( 1u << c ) ) != 0;
			if ( c % 2 == 1 )
			{
				ImGui::SameLine();
			}
			if ( ImGui::Checkbox( contactLabels[c], &on ) )
			{
				contacts = on ? contacts | ( 1u << c ) : contacts & ~( 1u << c );
				nbPoseClip_SetContacts( e.clip, e.keyframe, contacts );
				e.changed = true;
			}
		}
		ImGui::EndDisabled();
	}
	ImGui::EndDisabled();

	// Reference images and proposals
	ImGui::SeparatorText( "Referenz und Vorschläge" );
	ImGui::InputText( "Bilder", e.images, sizeof( e.images ) );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Ein Bild je Frame, BMP oder PPM, mit der Bildrate des Clips aus dem Video geschnitten:\n"
						   "ffmpeg -i video.mp4 -vf fps=30 frames/%%04d.bmp" );
	}
	if ( ImGui::InputInt( "Erste Nummer", &e.firstImage ) )
	{
		e.imageIndex = -1;
	}
	ImGui::Text( e.imageWidth > 0 ? "Bild %d, %d x %d" : "Kein Bild %d", e.imageIndex, e.imageWidth, e.imageHeight );
	ImGui::InputText( "Vorschläge", e.proposals, sizeof( e.proposals ) );
	if ( ImGui::Button( "Vorschläge laden" ) )
	{
		LoadProposals( e );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Eine Clipdatei, etwa von einem Assistenten, oder eine Keypoint-Datei eines Pose-Schätzers.\n"
						   "Freigegebene Keyframes lehnen Vorschläge ab." );
	}

	// The checks of the library
	ImGui::SeparatorText( "Prüfung" );
	if ( e.issues.empty() )
	{
		ImGui::TextUnformatted( "Keine Funde" );
	}
	else
	{
		ImGui::Text( "%d Funde", (int)e.issues.size() );
		ImGui::BeginChild( "issues", ImVec2( 0.0f, 110.0f * dpi ), ImGuiChildFlags_Borders );
		for ( size_t i = 0; i < e.issues.size(); ++i )
		{
			const nbPoseIssue& issue = e.issues[i];
			char line[160];
			int frame =
				issue.keyframe >= 0 && issue.keyframe < count ? nbPoseClip_GetKeyframe( e.clip, issue.keyframe ).frame : -1;
			const char* bone = issue.bone >= 0 && issue.bone < nb_humanoidBoneCount ? s_boneLabels[issue.bone] : "Wurzel";
			snprintf( line, sizeof( line ), "Frame %d, %s: %s %.2f##issue%d", frame, bone, s_issueLabels[issue.kind],
					  (double)issue.value, (int)i );
			if ( ImGui::Selectable( line ) && issue.keyframe >= 0 && issue.keyframe < count && e.simulating == false )
			{
				e.keyframe = issue.keyframe;
				e.frame = (float)frame;
				e.bone = issue.bone >= 0 ? issue.bone : nb_humanoidPelvis;
			}
		}
		ImGui::EndChild();
	}

	// The clip on a figure
	ImGui::SeparatorText( "Simulation" );
	ImGui::BeginDisabled( e.simulating );
	ImGui::Checkbox( "Balance", &e.balance );
	ImGui::SameLine();
	ImGui::Checkbox( "Backen", &e.bake );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Die Bibliothek nimmt die Pose, die die Figur erreicht, Frame für Frame in einen eigenen Clip\n"
						   "auf, mit den Füßen und Händen, die etwas berühren, und den Wechseln der Balance. Der Clip hier\n"
						   "bleibt, wie er ist." );
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if ( ImGui::Button( e.simulating ? "Stoppen" : "Simulieren" ) )
	{
		if ( e.simulating )
		{
			StopEditorSimulation( app );
		}
		else
		{
			StartEditorSimulation( app );
		}
	}

	// The motors of the figure, the same as in the figure scene, also while it plays
	bool motors = ImGui::SliderFloat( "Steifigkeit", &app.figureProfile.stiffness, 0.0f, 3000.0f, "%.0f N m/rad" );
	motors |= ImGui::SliderFloat( "Drehmoment", &app.figureProfile.maxTorque, 0.0f, 500.0f, "%.0f N m" );
	motors |= ImGui::SliderFloat( "Aufrecht halten", &app.figureProfile.uprightTorque, 0.0f, 1000.0f, "%.0f N m" );
	if ( motors && e.simulating )
	{
		nbArticulation_SetMotorProfile( e.figure, &app.figureProfile );
	}
	if ( e.simulating )
	{
		ImGui::Text( "Motorfehler jetzt %.2f rad%s%s", (double)e.motorError, e.motorBone > 0 ? " am " : "",
					 e.motorBone > 0 ? s_boneLabels[e.motorBone] : "" );
		if ( e.balanceState != nb_balanceOff )
		{
			ImGui::Text( "Balance: %s", BalanceName( e.balanceState ) );
		}
	}
	if ( e.baked != nullptr && e.bakedCount > 0 )
	{
		ImGui::Text( e.recording ? "Backt: %d Frames" : "Gebacken: %d Frames", e.bakedCount );
		ImGui::SameLine();
		ImGui::BeginDisabled( e.recording );
		if ( ImGui::Button( "Speichern##baked" ) )
		{
			std::string path = e.path;
			size_t dot = path.rfind( ".json" );
			path = ( dot != std::string::npos ? path.substr( 0, dot ) : path ) + "-simuliert.json";
			SaveEditorClip( e, e.baked, path.c_str() );
		}
		ImGui::EndDisabled();
		if ( e.behindBone > 0 )
		{
			ImGui::Text( "Hinter dem Clip bis %.2f rad, %s bei Frame %d", (double)e.behind, s_boneLabels[e.behindBone],
						 e.behindFrame );
		}
		for ( const nbPoseEvent& event : e.bakedEvents )
		{
			ImGui::Text( "Frame %d: %s", event.frame, EventLabel( event.name ) );
		}
		if ( e.bakedIssues.empty() == false )
		{
			ImGui::BeginChild( "bakedIssues", ImVec2( 0.0f, 80.0f * dpi ), ImGuiChildFlags_Borders );
			for ( size_t i = 0; i < e.bakedIssues.size(); ++i )
			{
				const nbPoseIssue& issue = e.bakedIssues[i];
				const char* bone = issue.bone >= 0 && issue.bone < nb_humanoidBoneCount ? s_boneLabels[issue.bone] : "Wurzel";
				ImGui::Text( "Frame %d, %s: %s %.2f", e.bakedFrames[i], bone, s_issueLabels[issue.kind], (double)issue.value );
			}
			ImGui::EndChild();
		}
	}

	ImGui::SeparatorText( "Ansicht" );
	static const char* views[3] = { "Vorn", "Seite", "Oben" };
	for ( int v = 0; v < 3; ++v )
	{
		if ( v > 0 )
		{
			ImGui::SameLine();
		}
		if ( ImGui::Button( views[v] ) )
		{
			LookAtEditorPose( app, v );
		}
	}
	ImGui::End();

	// The reference image of the frame
	if ( e.imageWidth > 0 )
	{
		float width = 360.0f * dpi;
		float height = width * (float)e.imageHeight / (float)e.imageWidth;
		ImGui::SetNextWindowPos( ImVec2( 12.0f * dpi, sapp_heightf() - height - 60.0f * dpi ), ImGuiCond_FirstUseEver );
		ImGui::Begin( "Referenzbild", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		ImGui::Image( ImTextureRef( (ImTextureID)simgui_imtextureid( e.imageView ) ), ImVec2( width, height ) );
		ImGui::End();
	}
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

// The figures of the town: humanoids with balance in the streets between the rows, three and a half meters apart, playing
// the arm clip, as many as the menu says. The demolition of the comparison throws the houses on them, see the roadmap of
// the game, 27.14.
static void AddTownFigures( App& app )
{
	if ( app.townFigures <= 0 )
	{
		return;
	}

	nbBoneDef bones[nb_humanoidBoneCount];
	float rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	MakeArmClip( app, nb_humanoidBoneCount );
	nbBalanceDef balance = nbDefaultBalanceDef();
	for ( int k = 0; k < app.townFigures; ++k )
	{
		int street = k % ( TownRows - 1 );
		int slot = k / ( TownRows - 1 );
		b3Vec3 position = { -28.0f + 3.5f * (float)slot, rootHeight, -6.0f - 12.0f * (float)street };
		nbArticulationId id =
			nbCreateArticulation( app.destruction, &rig, b3ToPos( position ), b3Quat_identity, &app.figureProfile );
		nbArticulation_EnableBalance( id, &balance );
		nbArticulation_PlayClip( id, &app.armClip, 1.0f );
		AddFigureVisual( app, id, bones, nb_humanoidBoneCount );
	}
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
	AddTownFigures( app );

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

	// The figures went with the world, their meshes go here
	for ( FigureVisual& figure : app.figures )
	{
		for ( int mesh : figure.boneMeshes )
		{
			app.renderer.RemoveMesh( mesh );
		}
		for ( int mesh : figure.targetMeshes )
		{
			app.renderer.RemoveMesh( mesh );
		}
		for ( int slot : figure.boneSlots )
		{
			app.renderer.FreeSlot( slot );
		}
		for ( int slot : figure.targetSlots )
		{
			app.renderer.FreeSlot( slot );
		}
		app.renderer.RemoveMesh( figure.captureMesh );
		app.renderer.RemoveMesh( figure.stepMesh );
		app.renderer.FreeSlot( figure.captureSlot );
		app.renderer.FreeSlot( figure.stepSlot );
	}
	app.figures.clear();
	DestroyEditorScene( app );
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

// How every storey gives way: the fixed ranges it picks from with "Zufall je Etage", the values of the sliders without
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
		case SceneFigures:
			BuildFigureScene( app );
			break;
		case SceneKeyframes:
			BuildEditorScene( app );
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
				int bone = -1;
				nbArticulationId figure = nbWorld_GetArticulationFromBody( app.destruction, body, &bone );
				if ( NB_IS_NON_NULL( figure ) )
				{
					// A hit on a figure lands once through the library, with the impulse of the menu
					nbHitDef hit = {};
					hit.id = ++app.hitCounter;
					hit.bone = bone;
					hit.impulse = b3MulSV( app.hitImpulse, direction );
					hit.point = ray.point;
					nbArticulation_ApplyHit( figure, &hit );
				}
				else if ( b3Body_GetType( body ) == b3_dynamicBody )
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

// How many figures stand in the streets of the town, what they do and what they cost, from the view taken before the
// steps, see AddTownFigures
static void DrawTownFigures( App& app )
{
	char preview[16];
	snprintf( preview, sizeof( preview ), app.townFigures > 0 ? "%d" : "keine", app.townFigures );
	if ( ImGui::BeginCombo( "Figuren", preview ) )
	{
		const int counts[4] = { 0, 1, 10, 50 };
		for ( int count : counts )
		{
			char label[16];
			snprintf( label, sizeof( label ), count > 0 ? "%d" : "keine", count );
			if ( ImGui::Selectable( label, count == app.townFigures ) && count != app.townFigures )
			{
				app.townFigures = count;
				LoadScene( app, SceneTown );
			}
		}
		ImGui::EndCombo();
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Humanoide mit Balance in den Straßen, die den Clip aus drei Keyframes spielen, mit den Motoren\n"
						   "der Figurenszene. Die Messung „Parallel gegen nacheinander“ reißt die Häuser über ihnen ab und\n"
						   "misst sie mit." );
	}

	const nbFigureStats& figures = app.view.figureStats;
	if ( figures.figureCount == 0 )
	{
		return;
	}
	const float* stages = app.view.profile.stages;
	ImGui::Text( "Stehen %d, Schritt %d, fallen %d, liegen %d, stehen auf %d", figures.stateCounts[nb_balanceStanding],
				 figures.stateCounts[nb_balanceStepping], figures.stateCounts[nb_balanceFalling],
				 figures.stateCounts[nb_balanceDown], figures.stateCounts[nb_balanceGettingUp] );
	ImGui::Text( "Stürze %d, aufgestanden %d, abgebrochen %d, Motoren am Anschlag %d", figures.fallCount, figures.getUpCount,
				 figures.failedGetUpCount, figures.saturatedJointCount );
	ImGui::Text( "Je Schritt %.3f ms, %.2f µs je Figur: Clips %.3f, Balance %.3f, Motoren %.3f", stages[nb_profileFigures],
				 1000.0f * stages[nb_profileFigures] / (float)figures.figureCount, stages[nb_profileFigureClips],
				 stages[nb_profileFigureBalance], stages[nb_profileFigureMotors] );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Was die Figuren im Update der Bibliothek kosten, im Mittel seit die Szene geladen ist. Ihre\n"
						   "Gelenke und Kontakte löst Box3D im Schritt, das steht unter Leistung." );
	}
	ImGui::Text( "Knochen wach %d von %d, Gelenke %d", figures.awakeBoneCount, figures.boneCount, figures.jointCount );
	if ( ImGui::Button( "Alle umwerfen" ) )
	{
		for ( const FigureVisual& figure : app.figures )
		{
			nbArticulation_Knockdown( figure.id );
		}
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

	view.figures.clear();
	view.figureStats = {};
	if ( app.figures.empty() == false )
	{
		view.figureStats = nbWorld_GetFigureStats( app.destruction );
		view.profile = nbWorld_GetProfile( app.destruction );
	}
	for ( const FigureVisual& figure : app.figures )
	{
		if ( app.scene != SceneFigures )
		{
			break;
		}

		FigureView figureView;
		int boneCount = nbArticulation_GetBoneCount( figure.id );
		for ( int bone = 1; bone < boneCount; ++bone )
		{
			figureView.error = b3MaxFloat( figureView.error, nbArticulation_GetJointError( figure.id, bone ) );
			figureView.load = b3MaxFloat( figureView.load, nbArticulation_GetMotorLoad( figure.id, bone ) );
		}
		figureView.hits = nbArticulation_GetHitCount( figure.id );
		figureView.clipTime = nbArticulation_GetClipTime( figure.id );
		figureView.balance = nbArticulation_GetBalance( figure.id );
		view.figures.push_back( figureView );
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
			 app.storeyRandom ? "Zufall je Etage" : "Werte der Regler" );
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

	// The figures, see nbWorld_GetFigureStats and nb_profileFigures, with the cost since the scene loaded
	const nbFigureStats& figures = view.figureStats;
	if ( figures.figureCount > 0 )
	{
		const float* stages = view.profile.stages;
		Appendf( text,
				 "Figuren: %d, je Schritt %.3f ms, %.2f µs je Figur, davon Clips %.3f, Balance %.3f und Motoren %.3f ms; "
				 "es stehen %d, fallen %d, liegen %d, stehen auf %d, %d Stürze, %d aufgestanden, %d abgebrochen, %d Motoren am "
				 "Anschlag, %d von %d Knochen wach\n",
				 figures.figureCount, stages[nb_profileFigures], 1000.0f * stages[nb_profileFigures] / (float)figures.figureCount,
				 stages[nb_profileFigureClips], stages[nb_profileFigureBalance], stages[nb_profileFigureMotors],
				 figures.stateCounts[nb_balanceStanding] + figures.stateCounts[nb_balanceStepping],
				 figures.stateCounts[nb_balanceFalling], figures.stateCounts[nb_balanceDown],
				 figures.stateCounts[nb_balanceGettingUp], figures.fallCount, figures.getUpCount, figures.failedGetUpCount,
				 figures.saturatedJointCount, figures.awakeBoneCount, figures.boneCount );
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

	if ( app.scene == SceneTown )
	{
		DrawTownFigures( app );
	}

	if ( app.scene == SceneFigures )
	{
		ImGui::SeparatorText( "Figuren" );
		bool changed = ImGui::Checkbox( "Motoren", &app.figureMotors );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "An: die linke und die mittlere Figur halten ihre Pose mit Federn in den Gelenken,\nbegrenzt im "
							   "Drehmoment. Aus: alle drei sind passive Ragdolls. Die rechte ist es immer." );
		}
		changed |= ImGui::SliderFloat( "Steifigkeit", &app.figureProfile.stiffness, 0.0f, 3000.0f, "%.0f N m/rad" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Drehmoment je Radian Abweichung von der Zielpose, in jedem Gelenk gleich." );
		}
		changed |= ImGui::SliderFloat( "Drehmoment", &app.figureProfile.maxTorque, 0.0f, 500.0f, "%.0f N m" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Mehr als das bringt kein Gelenk auf, mal der Stärke des Knochens: Oberschenkel 2, Rumpf 1,6,\nUnterschenkel 1,5, Oberarme 0,6, Hals "
							   "0,3. Darüber gibt die Figur nach." );
		}
		changed |= ImGui::SliderFloat( "Aufrecht halten", &app.figureProfile.uprightTorque, 0.0f, 1000.0f, "%.0f N m" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Ein äußeres Drehmoment hält das Becken aufrecht, bis hierhin. Kein Muskel, eine\nSpielhilfe. Ohne "
							   "Balance kippt ein stärkerer Stoß die Figur, mit Balance fängt sie ihn mit Schritten ab." );
		}
		changed |= ImGui::Checkbox( "Balance", &app.figureBalance );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "An: die linke und die mittlere Figur fangen Stöße mit Schritten ab, fallen mit Schutzhaltung,\n"
							   "wenn kein Schritt reicht, und stehen wieder auf, sobald sie ruhig liegen und Platz haben.\n"
							   "Nur mit Motoren." );
		}
		if ( changed )
		{
			ApplyFigureProfile( app );
		}
		if ( ImGui::Checkbox( "Zielpose zeigen", &app.showTargets ) )
		{
			for ( FigureVisual& figure : app.figures )
			{
				for ( int slot : figure.targetSlots )
				{
					app.renderer.SetSlotVisible( slot, app.showTargets );
				}
			}
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Die Pose, zu der die Gelenke ziehen, als dünnes dunkles Skelett über den Knochen.\nWo sie "
							   "herausschaut, kommt die Figur nicht hin." );
		}
		ImGui::SameLine();
		ImGui::Checkbox( "Balance zeigen", &app.showBalance );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Rot: der Capture Point, wo der Schwerpunkt zur Ruhe käme. Liegt er außerhalb der Füße,\nsetzt "
							   "die Figur einen Schritt. Dunkel: wohin der Fuß tritt." );
		}
		ImGui::SliderFloat( "Treffer", &app.hitImpulse, 5.0f, 300.0f, "%.0f N s" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Impuls eines Gewehrtreffers auf eine Figur und des Brusttreffers. Ohne Balance steht die Figur\nbei "
							   "20 N s auf der Brust, 60 N s werfen sie um. Mit Balance fängt sie 60 N s mit Schritten ab,\n"
							   "120 N s werfen sie um, dann steht sie wieder auf." );
		}
		if ( ImGui::Button( "Brusttreffer" ) )
		{
			HitChests( app, { 0.0f, 0.0f, -1.0f } );
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Seitenstoß" ) )
		{
			HitChests( app, { -1.0f, 0.0f, 0.0f } );
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Umwerfen" ) )
		{
			for ( size_t i = 0; i < app.figures.size() && i < 2; ++i )
			{
				nbArticulation_Knockdown( app.figures[i].id );
			}
		}
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Die Figuren mit Balance fallen sofort, liegen still und stehen wieder auf." );
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Clip neu starten" ) )
		{
			for ( size_t i = 0; i < app.figures.size() && i < 2; ++i )
			{
				nbArticulation_PlayClip( app.figures[i].id, &app.armClip, 1.0f );
			}
		}
		// From the view taken before the step, reading the worlds here would wait for it
		for ( size_t i = 0; i < app.view.figures.size(); ++i )
		{
			const FigureView& figure = app.view.figures[i];
			ImGui::Text( "Figur %d: Abweichung %.2f rad, Motorlast %.0f %%, Treffer %d, Clip %.2f s", (int)i + 1, figure.error,
						 100.0f * figure.load, figure.hits, figure.clipTime );
			if ( figure.balance.state != nb_balanceOff && figure.balance.supportCount == 0 )
			{
				ImGui::Text( "  %s seit %.1f s, Schritte %d, kein Fuß am Boden", BalanceName( figure.balance.state ),
							 figure.balance.stateTime, figure.balance.stepCount );
			}
			else if ( figure.balance.state != nb_balanceOff )
			{
				ImGui::Text( "  %s seit %.1f s, Schritte %d, Capture Point %.0f cm %s", BalanceName( figure.balance.state ),
							 figure.balance.stateTime, figure.balance.stepCount, 100.0f * fabsf( figure.balance.captureMargin ),
							 figure.balance.captureMargin <= 0.0f ? "innen" : "außen" );
			}
		}
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
		ImGui::SetTooltip( "An: Es gelten die festen Bereiche darunter, die Regler sind aus. Jede Etage\nzieht beim Laden des Hauses ihre Werte daraus, für dasselbe Haus immer\ndieselben. Aus: Die Regler sind an, und jede Etage nimmt ihre Werte." );
	}
	for ( int top = 0; top < 2; ++top )
	{
		const char* damageLabel = top == 1 ? "Oberste gibt nach" : "Etage gibt nach";
		const char* residueLabel = top == 1 ? "Oberste Stütze" : "Stütze bleibt";
		const char* damageTip =
			top == 1 ? "Dasselbe wie „Etage gibt nach“ für die oberste Etage unter dem Dach."
					 : "Ist so viel der Wände einer Etage zerstört, gibt sie nach. Mit „Zufall je\nEtage“ gilt der feste Bereich, aus dem jede Etage ihren Wert beim Laden\nzieht. Was „Stütze bleibt“ sagt, bleibt von den übrigen Wänden stehen, in\nZellen zerbrochen, der Rest fliegt heraus. Alles darüber sinkt in einem\nStück darauf und kippt oder bleibt stehen, bis weitere Treffer die Reste\nwegschießen. 0 schaltet es ab.";
		const char* residueTip =
			top == 1 ? "Dasselbe wie „Stütze bleibt“ für die oberste Etage unter dem Dach."
					 : "So viel der übrigen Wände einer Etage, die nachgibt, bleibt stehen, in\nZellen zerbrochen: bei Schaden auf einer Seite auf der stärksten Seite,\nsonst an zufälligen Stellen. Der Rest fliegt heraus. 0 wirft alle\nheraus, 100 lässt alle stehen. Gilt für die Etagen, die danach nachgeben.";
		if ( app.storeyRandom )
		{
			// The fixed ranges, shown with the sliders off
			const nbStoreyCollapse* collapse = app.randomCollapse + top;
			float damage[2] = { 100.0f * collapse->damageMin, 100.0f * collapse->damageMax };
			float residue[2] = { 100.0f * collapse->residueMin, 100.0f * collapse->residueMax };
			ImGui::BeginDisabled();
			if ( damage[0] < damage[1] )
			{
				ImGui::DragFloatRange2( damageLabel, damage, damage + 1, 0.1f, 0.0f, 100.0f, "bei %.0f", "bis %.0f %%" );
			}
			else
			{
				ImGui::SliderFloat( damageLabel, damage, 0.0f, 100.0f, "bei %.0f %% zerstört" );
			}
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
			{
				ImGui::SetTooltip( "%s", damageTip );
			}
			if ( residue[0] < residue[1] )
			{
				ImGui::DragFloatRange2( residueLabel, residue, residue + 1, 0.1f, 0.0f, 100.0f, "%.0f", "bis %.0f %%" );
			}
			else
			{
				ImGui::SliderFloat( residueLabel, residue, 0.0f, 100.0f, "%.0f %% der übrigen Wände" );
			}
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
			{
				ImGui::SetTooltip( "%s", residueTip );
			}
			ImGui::EndDisabled();
			continue;
		}

		nbStoreyCollapse* collapse = app.fixedCollapse + top;
		float damage = 100.0f * collapse->damageMin;
		float residue = 100.0f * collapse->residueMin;
		bool changed = ImGui::SliderFloat( damageLabel, &damage, 0.0f, 100.0f, "bei %.0f %% zerstört" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "%s", damageTip );
		}
		changed |= ImGui::SliderFloat( residueLabel, &residue, 0.0f, 100.0f, "%.0f %% der übrigen Wände" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "%s", residueTip );
		}
		if ( changed )
		{
			*collapse = nbStoreyCollapse{ 0.01f * damage, 0.01f * damage, 0.01f * residue, 0.01f * residue };
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
		ImGui::SetTooltip( "So viele Trümmer bewegt Box3D höchstens gleichzeitig, sobald sie liegen.\nDie langsamsten darüber erstarren zu Schutt, nie im Flug, gelöscht\nwird nichts. Weniger macht die Physik schneller." );
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
			ImGui::SetTooltip(
				"Lädt die Stadt und reißt sie Haus für Haus mit Granaten ab, Wand für Wand von\noben nach unten, bis jedes Haus "
				"unten ist, mit Budget 5000 und nur so schnell,\ndass nie mehr als 3000 Trümmer zugleich fliegen: einmal kurz "
				"zum Aufwärmen, dann\nparallel, nacheinander, nacheinander und parallel, mit einem Schritt pro Bild und\njeweils "
				"mit den passenden Threads. Mit Figuren in der Stadt, siehe Szene, misst er\nsie mit. Das Ergebnis kommt in die "
				"Zwischenablage, danach lädt die Szene von\nvorher neu." );
		}
		// The threads the library takes without parallel physics against one fewer, both ways
		int more = nbGetRecommendedWorkerCount( false );
		int fewer = b3MaxInt( more - 1, 1 );
		char threadsLabel[64];
		snprintf( threadsLabel, sizeof( threadsLabel ), "%d gegen %d Threads messen", fewer, more );
		ImGui::BeginDisabled( fewer == more );
		if ( ImGui::Button( threadsLabel ) )
		{
			comparison.requested = true;
			comparison.kind = Comparison::ThreadCounts;
		}
		ImGui::EndDisabled();
		if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
		{
			if ( fewer == more )
			{
				ImGui::SetTooltip( "Auf diesem PC nimmt die Bibliothek nur einen Thread." );
			}
			else
			{
				ImGui::SetTooltip( "Reißt die Stadt wie beim Vergleich Haus für Haus ab, einmal kurz zum Aufwärmen, dann\n"
								   "parallel und nacheinander, jeweils mit %d und mit %d Threads, und dieselben vier\n"
								   "Läufe noch einmal rückwärts, damit eine Drift des PCs alle gleich trifft. Die\n"
								   "Bibliothek nimmt parallel %d und nacheinander %d Threads. Das dauert doppelt so\n"
								   "lange wie der Vergleich. Das Ergebnis kommt in die Zwischenablage, danach lädt die\n"
								   "Szene von vorher neu.",
								   fewer, more, nbGetRecommendedWorkerCount( true ), more );
			}
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
		case SceneKeyframes:
		{
			// The example clip with its first keyframe approved on a figure, baked, both saved, and the baked frames taken
			// back in as proposals, which the approved keyframe refuses
			Editor& e = app.editor;
			if ( f == 10 )
			{
				nbPoseClip_Approve( e.clip, 0, true );
				e.bake = true;
				StartEditorSimulation( app );
			}
			else if ( f == 200 && e.baked != nullptr && e.recording == false )
			{
				SaveEditorClip( e, e.clip, e.path );
				snprintf( e.proposals, sizeof( e.proposals ), "%s.simuliert.json", e.path );
				SaveEditorClip( e, e.baked, e.proposals );
				printf( "Editor: %d Frames gebacken, bis %.3f rad hinter dem Clip, %d Funde und %d Ereignisse in der Aufnahme\n",
						e.bakedCount, (double)e.behind, (int)e.bakedIssues.size(), (int)e.bakedEvents.size() );
				LoadProposals( e );
				printf( "Editor: %s\n", e.message.c_str() );
			}
			break;
		}

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
	app.forcedWorkers = comparison.runWorkers[comparison.run];
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

	// Windows would slow the demo down while another window is in front, see nbKeepFullSpeed
	nbKeepFullSpeed( true );

	// Parallel, serial, serial and parallel, each with the threads the library picks for it
	comparison.runCount = Comparison::RunCount;
	for ( int run = 0; run < Comparison::MaxRuns; ++run )
	{
		comparison.runParallel[run] = run == 0 || run == 1 || run == Comparison::RunCount - 1;
		comparison.runWorkers[run] = 0;
	}

	// Both ways with the threads the library takes without parallel physics and with one fewer, then the same the other
	// way round
	if ( comparison.kind == Comparison::ThreadCounts )
	{
		int more = nbGetRecommendedWorkerCount( false );
		int fewer = b3MaxInt( more - 1, 1 );
		const bool parallel[] = { true, true, true, false, false, false, false, true, true };
		const int workers[] = { fewer, fewer, more, more, fewer, fewer, more, more, fewer };
		comparison.runCount = Comparison::MaxRuns;
		for ( int run = 0; run < Comparison::MaxRuns; ++run )
		{
			comparison.runParallel[run] = parallel[run];
			comparison.runWorkers[run] = workers[run];
		}
	}

	// The scaling warms up with the most threads, then takes a run for each number of threads, the fewest first
	if ( comparison.kind == Comparison::Scaling )
	{
		int counts[Comparison::MaxRuns - 1];
		int countCount = nbGetScalingWorkerCounts( counts, Comparison::MaxRuns - 1 );
		comparison.runCount = countCount + 1;
		comparison.runWorkers[0] = counts[countCount - 1];
		comparison.runParallel[0] = false;
		for ( int i = 0; i < countCount; ++i )
		{
			comparison.runWorkers[i + 1] = counts[i];
			comparison.runParallel[i + 1] = false;
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
	nbKeepFullSpeed( false );
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

// The counted runs with or without parallel physics, with so many threads or, with zero, any
static ComparisonResult SumComparison( const Comparison& comparison, bool parallel, int threads )
{
	ComparisonResult result;
	std::vector<float> times;
	double simulation = 0.0, graphics = 0.0, wait = 0.0, frames = 0.0;
	for ( int run = 1; run < comparison.runCount; ++run )
	{
		if ( RunIsParallel( comparison, run ) != parallel || ( threads > 0 && comparison.threads[run] != threads ) )
		{
			continue;
		}

		times.insert( times.end(), comparison.frameTimes[run].begin(), comparison.frameTimes[run].end() );
		simulation += comparison.simulationTime[run];
		graphics += comparison.graphicsTime[run];
		wait += comparison.waitTime[run];
		frames += (double)comparison.frames[run];
		result.threads = comparison.threads[run];
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

// Whether every counted run ended with the same world
static bool ComparisonRunsAgree( const Comparison& comparison )
{
	bool same = true;
	for ( int run = 2; run < comparison.runCount; ++run )
	{
		same = same && comparison.hashes[run] == comparison.hashes[1];
	}
	return same;
}

// How much faster the frames of a than those of b, in percent, and the word for it
static float FrameGain( const ComparisonResult& a, const ComparisonResult& b, const char** direction )
{
	float gain = 100.0f * ( b.frames.average - a.frames.average ) / b3MaxFloat( b.frames.average, 0.001f );
	*direction = gain >= 0.0f ? "schneller" : "langsamer";
	return fabsf( gain );
}

// The demolition of a comparison and what is left of the town at the end
static void AppendDemolition( std::string& text, const App& app )
{
	const Comparison& comparison = app.comparison;
	int storeys = 0;
	for ( int house = 0; house < TownRows * TownColumns; ++house )
	{
		storeys += TownHouseFloors( house );
	}
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

	// The figures in the streets: what they cost in the counted runs, see nb_profileFigures, and what they did in the last
	const nbFigureStats& figures = comparison.figureStats;
	if ( figures.figureCount > 0 )
	{
		float stages[4] = {};
		const nbProfileStage parts[4] = { nb_profileFigures, nb_profileFigureClips, nb_profileFigureBalance,
										  nb_profileFigureMotors };
		for ( int run = 1; run < comparison.runCount; ++run )
		{
			for ( int k = 0; k < 4; ++k )
			{
				stages[k] += comparison.profiles[run].stages[parts[k]] / (float)( comparison.runCount - 1 );
			}
		}
		Appendf( text,
				 "%d Figuren in den Straßen mit Balance und Clip: je Schritt %.3f ms, %.2f µs je Figur, davon Clips %.3f, "
				 "Balance %.3f und Motoren %.3f ms. Am Ende stehen %d, fallen %d, liegen %d, stehen auf %d, %d Stürze, %d "
				 "aufgestanden, %d abgebrochen\n",
				 figures.figureCount, stages[0], 1000.0f * stages[0] / (float)figures.figureCount, stages[1], stages[2],
				 stages[3], figures.stateCounts[nb_balanceStanding] + figures.stateCounts[nb_balanceStepping],
				 figures.stateCounts[nb_balanceFalling], figures.stateCounts[nb_balanceDown],
				 figures.stateCounts[nb_balanceGettingUp], figures.fallCount, figures.getUpCount, figures.failedGetUpCount );
	}
}

// The counted runs of one way and number of threads together
static void AppendResult( std::string& text, const ComparisonResult& result, bool parallel )
{
	Appendf( text, "%s mit %d Threads: %.2f ms pro Bild, %.0f FPS, 1%% Low %.0f FPS; Simulation %.2f, Grafik %.2f",
			 parallel ? "Parallel" : "Nacheinander", result.threads, result.frames.average,
			 1000.0f / b3MaxFloat( result.frames.average, 0.001f ), result.frames.lowFps, result.simulationTime,
			 result.graphicsTime );
	if ( parallel )
	{
		Appendf( text, ", Warten %.2f", result.waitTime );
	}
	Appendf( text, " ms\n" );
}

// Each run on its own, to see where the slow frames come from, with its threads if they change from run to run
static void AppendRuns( std::string& text, const Comparison& comparison, bool threads )
{
	for ( int run = 0; run < comparison.runCount; ++run )
	{
		FrameStats stats = GetFrameStats( comparison.frameTimes[run] );
		bool runParallel = RunIsParallel( comparison, run );
		std::string name;
		if ( run == 0 )
		{
			Appendf( name, "Aufwärmen, %s", runParallel ? "parallel" : "nacheinander" );
		}
		else
		{
			Appendf( name, "Lauf %d, %s", run, runParallel ? "parallel" : "nacheinander" );
		}
		if ( threads )
		{
			Appendf( name, ", %d Threads", comparison.threads[run] );
		}
		Appendf( text, "  %s: %.2f ms pro Bild, 1%% Low %.0f FPS, längstes Bild %.1f ms, längste Simulation %.1f ms",
				 name.c_str(), stats.average, stats.lowFps, stats.longest, comparison.longestSimulation[run] );
		if ( runParallel )
		{
			Appendf( text, ", längstes Warten %.1f ms", comparison.longestWait[run] );
		}
		Appendf( text, "\n" );
	}
}

// The report goes to the clipboard, to paste into a chat, and with --compare to the console
static void PublishComparison( App& app, const std::string& text, const char* summary )
{
	Comparison& comparison = app.comparison;
	comparison.report = text;
	comparison.summary = summary;
	sapp_set_clipboard_string( text.c_str() );

	if ( app.automation.compare )
	{
		printf( "%s", text.c_str() );
		sapp_request_quit();
	}
	StopComparison( app );
}

// Both ways with the threads the library takes and one fewer: what one thread more or less changes, and what the
// library's choice does, parallel against serial each with the threads the library takes for it
static void FinishThreadCounts( App& app )
{
	Comparison& comparison = app.comparison;
	int fewer = comparison.runWorkers[1];
	int more = comparison.runWorkers[2];
	ComparisonResult parallelFewer = SumComparison( comparison, true, fewer );
	ComparisonResult parallelMore = SumComparison( comparison, true, more );
	ComparisonResult serialFewer = SumComparison( comparison, false, fewer );
	ComparisonResult serialMore = SumComparison( comparison, false, more );

	b3Version version = b3GetVersion();
	std::string text;
	Appendf( text,
			 "Nebenan-Threads: Physik parallel zum Zeichnen und nacheinander mit %d und %d Threads, %s-Build, Box3D %d.%d.%d\n",
			 fewer, more, s_buildType, version.major, version.minor, version.revision );
	AppendMachine( text, app );
	AppendDemolition( text, app );
	AppendResult( text, parallelFewer, true );
	AppendResult( text, parallelMore, true );
	AppendResult( text, serialFewer, false );
	AppendResult( text, serialMore, false );

	const char* parallelWay;
	const char* serialWay;
	const char* choiceWay;
	float parallelGain = FrameGain( parallelFewer, parallelMore, &parallelWay );
	float serialGain = FrameGain( serialFewer, serialMore, &serialWay );
	int parallelChoice = nbGetRecommendedWorkerCount( true );
	float choiceGain = FrameGain( parallelChoice == fewer ? parallelFewer : parallelMore, serialMore, &choiceWay );
	Appendf( text, "Mit %d statt %d Threads ist parallel %.1f %% %s, nacheinander %.1f %% %s\n", fewer, more, parallelGain,
			 parallelWay, serialGain, serialWay );
	Appendf( text, "Parallel mit %d gegen nacheinander mit %d Threads, wie die Bibliothek sie wählt: parallel %.1f %% %s\n",
			 parallelChoice, more, choiceGain, choiceWay );
	Appendf( text, "Die Welt endet in allen Läufen %s\n", ComparisonRunsAgree( comparison ) ? "gleich" : "verschieden" );
	AppendRuns( text, comparison, true );

	char summary[160];
	snprintf( summary, sizeof( summary ),
			  "Parallel %.2f und %.2f, nacheinander %.2f und %.2f ms\npro Bild mit %d und %d Threads. In der Zwischenablage.",
			  parallelFewer.frames.average, parallelMore.frames.average, serialFewer.frames.average, serialMore.frames.average,
			  fewer, more );
	PublishComparison( app, text, summary );
}

static void FinishComparison( App& app )
{
	Comparison& comparison = app.comparison;
	if ( comparison.kind == Comparison::Scaling )
	{
		FinishScaling( app );
		return;
	}
	if ( comparison.kind == Comparison::ThreadCounts )
	{
		FinishThreadCounts( app );
		return;
	}

	ComparisonResult parallel = SumComparison( comparison, true, 0 );
	ComparisonResult serial = SumComparison( comparison, false, 0 );
	const char* direction;
	float gain = FrameGain( parallel, serial, &direction );

	b3Version version = b3GetVersion();
	std::string text;
	Appendf( text, "Nebenan-Vergleich: Physik parallel zum Zeichnen gegen nacheinander, %s-Build, Box3D %d.%d.%d\n", s_buildType,
			 version.major, version.minor, version.revision );
	AppendMachine( text, app );
	AppendDemolition( text, app );
	AppendResult( text, parallel, true );
	AppendResult( text, serial, false );
	Appendf( text, "Parallel ist %.1f %% %s, die Welt endet in allen Läufen %s\n", gain, direction,
			 ComparisonRunsAgree( comparison ) ? "gleich" : "verschieden" );
	AppendRuns( text, comparison, false );

	char summary[160];
	snprintf( summary, sizeof( summary ),
			  "Parallel %.2f ms, nacheinander %.2f ms pro Bild,\nparallel %.0f %% %s. In der Zwischenablage.",
			  parallel.frames.average, serial.frames.average, gain, direction );
	PublishComparison( app, text, summary );
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
	comparison.figureStats = nbWorld_GetFigureStats( app.destruction );
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
		Comparison::Kind kind = app.automation.threadCounts ? Comparison::ThreadCounts : Comparison::ParallelAgainstSerial;
		app.comparison.kind = app.automation.scaling ? Comparison::Scaling : kind;
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
		UpdateFigures( app );
		UpdateEditor( app, dt );
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
		UpdateFigures( app );
		UpdateEditor( app, dt );
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
		if ( app.scene == SceneKeyframes )
		{
			DrawEditorUi( app );
		}
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
		else if ( strcmp( argv[i], "--compare-threads" ) == 0 )
		{
			app.automation.compare = true;
			app.automation.threadCounts = true;
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
		else if ( strcmp( argv[i], "--figures" ) == 0 && i + 1 < argc )
		{
			app.townFigures = b3ClampInt( atoi( argv[++i] ), 0, 50 );
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
	// The reports of the measurements go to the clipboard, the scaling with the stages of the team takes more than 8 KB
	desc.enable_clipboard = true;
	desc.clipboard_size = 32768;
	desc.window_title = "Nebenan - polygonale Zerstörung mit Box3D";
	desc.gl.major_version = 4;
	desc.gl.minor_version = 3;
	desc.logger.func = slog_func;
	sapp_run( &desc );
	return 0;
}
