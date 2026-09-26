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
#include <thread>
#include <unordered_map>
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
	SceneCount
};

static const char* s_sceneNames[SceneCount] = { "Mauern", "Stresstest", "Stadt" };
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

struct Automation
{
	int frameLimit = 0;
	const char* screenshotPath = nullptr;
	bool script = false;
	int scene = -1;

	// Samples per pixel and whether to render at the full resolution of high density displays. Both cost fill rate.
	int msaa = 1;
	bool highDpi = false;

	// Per frame: CPU time of the simulation, of the whole frame and of the graphics, bytes sent to the GPU,
	// triangles drawn
	std::vector<float> simulationTimes;
	std::vector<float> frameTimes;
	std::vector<float> graphicsTimes;
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

struct App
{
	b3WorldId physics = b3_nullWorldId;
	nbWorldId destruction = nb_nullWorldId;
	Renderer renderer;
	Camera camera = {};

	std::vector<ChunkVisual> chunks;
	std::unordered_map<uint64_t, std::vector<int>> bodySlots;
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

	float accumulator = 0.0f;
	int workerCount = 1;
	int maxWorkers = 1;

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

	// The last frames for the performance panel: frame time, and CPU time for impacts, simulation and graphics
	static const int HistorySize = 300;
	float historyFrame[HistorySize] = {};
	float historyImpacts[HistorySize] = {};
	float historySimulation[HistorySize] = {};
	float historyGraphics[HistorySize] = {};
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

static uint64_t BodyKey( b3BodyId id )
{
	return ( (uint64_t)(uint32_t)id.index1 << 32 ) | ( (uint64_t)id.world0 << 16 ) | id.generation;
}

static void AttachSlot( App& app, b3BodyId body, int slot )
{
	app.bodySlots[BodyKey( body )].push_back( slot );
	b3WorldTransform transform = b3Body_GetTransform( body );
	app.renderer.SetSlot( slot, b3ToVec3( transform.p ), transform.q );
}

static void DetachSlot( App& app, b3BodyId body, int slot )
{
	auto it = app.bodySlots.find( BodyKey( body ) );
	if ( it == app.bodySlots.end() )
	{
		return;
	}

	std::vector<int>& slots = it->second;
	for ( size_t i = 0; i < slots.size(); ++i )
	{
		if ( slots[i] == slot )
		{
			slots[i] = slots.back();
			slots.pop_back();
			break;
		}
	}

	if ( slots.empty() )
	{
		app.bodySlots.erase( it );
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
			app.vertexScratch.push_back( Renderer::MakeVertex( position, face.plane.normal, (int)face.material, visual.slot ) );
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

// Box3D reports every body that moved during a step, only those slots need new transforms
static void SyncTransforms( App& app )
{
	b3BodyEvents events = b3World_GetBodyEvents( app.physics );
	for ( int i = 0; i < events.moveCount; ++i )
	{
		const b3BodyMoveEvent& event = events.moveEvents[i];
		auto it = app.bodySlots.find( BodyKey( event.bodyId ) );
		if ( it == app.bodySlots.end() )
		{
			continue;
		}

		b3Vec3 position = b3ToVec3( event.transform.p );
		for ( int slot : it->second )
		{
			app.renderer.SetSlot( slot, position, event.transform.q );
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Static meshes

static void BuildGround( App& app )
{
	app.groundSlot = app.renderer.AllocSlot();
	app.renderer.SetSlot( app.groundSlot, b3Vec3_zero, b3Quat_identity );

	float h = 150.0f;
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

static nbDestructibleId AddStructure( App& app, b3Vec3 position, float yaw, const MaterialPreset& preset,
									  const std::vector<nbPieceDef>& pieces, uint32_t seed, float cellSize = 0.0f )
{
	nbDestructibleDef def = nbDefaultDestructibleDef();
	def.cellSize = cellSize;
	def.position = b3ToPos( position );
	def.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisY, yaw );
	def.material = preset.material;
	def.seed = seed;
	return nbCreateDestructible( app.destruction, &def, pieces.data(), (int)pieces.size() );
}

static void AddWall( App& app, b3Vec3 base, float yaw, b3Vec3 size, const MaterialPreset& preset, uint32_t seed )
{
	std::vector<nbPieceDef> pieces = { MakePiece( { 0.0f, 0.5f * size.y, 0.0f }, b3MulSV( 0.5f, size ), preset, preset.surface ) };
	AddStructure( app, base, yaw, preset, pieces, seed );
}

// A wall in the local xy plane with rectangular openings, cut into box pieces on the grid of opening edges
static void AddWallWithOpenings( std::vector<nbPieceDef>& pieces, b3Vec3 origin, bool alongX, float width, float height,
								 float thickness, const std::vector<b3Vec2>& openings, const MaterialPreset& preset,
								 uint8_t surface )
{
	std::vector<float> xs = { 0.0f, width };
	std::vector<float> ys = { 0.0f, height };
	for ( size_t i = 0; i < openings.size(); i += 2 )
	{
		xs.push_back( openings[i].x );
		xs.push_back( openings[i + 1].x );
		ys.push_back( openings[i].y );
		ys.push_back( openings[i + 1].y );
	}
	std::sort( xs.begin(), xs.end() );
	std::sort( ys.begin(), ys.end() );

	for ( size_t j = 0; j + 1 < ys.size(); ++j )
	{
		for ( size_t i = 0; i + 1 < xs.size(); ++i )
		{
			float x0 = xs[i], x1 = xs[i + 1], y0 = ys[j], y1 = ys[j + 1];
			if ( x1 - x0 < 1.0e-3f || y1 - y0 < 1.0e-3f )
			{
				continue;
			}

			float cx = 0.5f * ( x0 + x1 );
			float cy = 0.5f * ( y0 + y1 );
			bool inOpening = false;
			for ( size_t k = 0; k < openings.size(); k += 2 )
			{
				if ( openings[k].x < cx && cx < openings[k + 1].x && openings[k].y < cy && cy < openings[k + 1].y )
				{
					inOpening = true;
				}
			}

			if ( inOpening )
			{
				continue;
			}

			b3Vec3 half = alongX ? b3Vec3{ 0.5f * ( x1 - x0 ), 0.5f * ( y1 - y0 ), 0.5f * thickness }
								 : b3Vec3{ 0.5f * thickness, 0.5f * ( y1 - y0 ), 0.5f * ( x1 - x0 ) };
			b3Vec3 center = alongX ? b3Vec3{ origin.x + cx, origin.y + cy, origin.z } : b3Vec3{ origin.x, origin.y + cy, origin.z + cx };
			pieces.push_back( MakePiece( center, half, preset, surface ) );
		}
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

// Brick walls with doors and windows and concrete floors
static void AddHouse( App& app, b3Vec3 position, float yaw, int floors, uint32_t seed )
{
	MaterialPreset brick = BrickPreset();
	MaterialPreset concrete = ConcretePreset();

	std::vector<nbPieceDef> pieces;
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;

	for ( int floor = 0; floor < floors; ++floor )
	{
		float y = (float)floor * ( story + slab );
		std::vector<b3Vec2> front;
		if ( floor == 0 )
		{
			front = { { 3.9f, 0.0f }, { 5.1f, 2.2f }, { 1.0f, 1.0f }, { 2.6f, 2.2f }, { 6.4f, 1.0f }, { 8.0f, 2.2f } };
		}
		else
		{
			front = { { 1.0f, 0.9f }, { 2.6f, 2.2f }, { 3.7f, 0.9f }, { 5.3f, 2.2f }, { 6.4f, 0.9f }, { 8.0f, 2.2f } };
		}
		std::vector<b3Vec2> back = { { 1.5f, 0.9f }, { 3.0f, 2.2f }, { 6.0f, 0.9f }, { 7.5f, 2.2f } };
		std::vector<b3Vec2> side = { { 2.4f, 0.9f }, { 3.6f, 2.2f } };

		AddWallWithOpenings( pieces, { -0.5f * width, y, 0.5f * depth - 0.5f * t }, true, width, story, t, front, brick, MaterialPlaster );
		AddWallWithOpenings( pieces, { -0.5f * width, y, -0.5f * depth + 0.5f * t }, true, width, story, t, back, brick, MaterialPlaster );
		AddWallWithOpenings( pieces, { -0.5f * width + 0.5f * t, y, -0.5f * depth + t }, false, depth - 2.0f * t, story, t, side, brick,
							 MaterialPlaster );
		AddWallWithOpenings( pieces, { 0.5f * width - 0.5f * t, y, -0.5f * depth + t }, false, depth - 2.0f * t, story, t, side, brick,
							 MaterialPlaster );

		// Concrete floor slab on top of the story
		nbPieceDef slabPiece = MakePiece( { 0.0f, y + story + 0.5f * slab, 0.0f }, { 0.5f * width, 0.5f * slab, 0.5f * depth },
										  concrete, MaterialConcrete );
		slabPiece.material = &concrete.material;
		pieces.push_back( slabPiece );
	}

	// Brick walls and concrete slabs in one structure
	AddStructure( app, position, yaw, brick, pieces, seed );
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

static void BuildTownScene( App& app )
{
	uint32_t seed = 200;
	for ( int row = 0; row < TownRows; ++row )
	{
		for ( int column = 0; column < TownColumns; ++column )
		{
			// Every third house has a third floor, every other one faces the other street
			int floors = ( row * TownColumns + column ) % 3 == 1 ? 3 : 2;
			float yaw = ( row & 1 ) != 0 ? B3_PI : 0.0f;
			AddHouse( app, TownHousePosition( row, column ), yaw, floors, seed++ );
		}
	}

	app.camera.position = { 0.0f, 16.0f, 34.0f };
	app.camera.yaw = 0.0f;
	app.camera.pitch = -0.38f;
}

static void DestroyScene( App& app )
{
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
}

static void LoadScene( App& app, SceneKind scene )
{
	DestroyScene( app );
	app.scene = scene;

	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)app.workerCount;
	app.physics = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	groundDef.position = b3ToPos( { 0.0f, -1.0f, 0.0f } );
	b3BodyId ground = b3CreateBody( app.physics, &groundDef );
	b3BoxHull box = b3MakeBoxHull( 150.0f, 1.0f, 150.0f );
	b3ShapeDef groundShape = b3DefaultShapeDef();
	groundShape.baseMaterial.friction = 0.8f;
	b3CreateHullShape( ground, &groundShape, &box.base );

	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = app.physics;
	def.collisionRadiusScale = 0.02f;
	def.workerCount = app.workerCount;
	def.maxDebrisBodies = app.maxDebrisBodies;
	def.fragmentScale = app.fragmentScale;
	app.destruction = nbCreateWorld( &def );

	switch ( scene )
	{
		case SceneWall:
			BuildWallScene( app );
			break;
		case SceneStress:
			BuildStressScene( app );
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
	app.lastImpact = nbWorld_ApplyImpact( app.destruction, &impact );

	b3ExplosionDef explosion = b3DefaultExplosionDef();
	explosion.position = point;
	explosion.radius = settings.radius;
	explosion.falloff = settings.radius;
	explosion.impulsePerArea = 40.0f * settings.ejectSpeed;
	b3World_Explode( app.physics, &explosion );
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

static void UpdateBalls( App& app, float dt )
{
	for ( size_t i = 0; i < app.balls.size(); )
	{
		Ball& ball = app.balls[i];
		ball.age += dt;
		if ( ball.age > 20.0f || app.balls.size() > 24 )
		{
			DetachSlot( app, ball.body, ball.slot );
			app.renderer.RemoveMesh( ball.mesh );
			app.renderer.FreeSlot( ball.slot );
			b3DestroyBody( ball.body );
			app.balls[i] = app.balls.back();
			app.balls.pop_back();
			continue;
		}
		++i;
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Simulation

static void StepSimulation( App& app, float frameDt )
{
	if ( app.paused )
	{
		return;
	}

	// When the simulation cannot keep up, it slows down instead of taking more and more steps per frame, which
	// would slow down every frame further. A second step in the same frame only catches up when steps are cheap.
	const float step = 1.0f / 60.0f;
	int maxSteps = app.physicsTime + app.destructionTime < 4.0f ? 2 : 1;
	int steps = 0;
	float physicsTime = 0.0f;
	float destructionTime = 0.0f;
	app.accumulator += b3MinFloat( frameDt, 0.1f );
	while ( app.accumulator >= step && steps < maxSteps )
	{
		uint64_t ticks = b3GetTicks();
		b3World_Step( app.physics, step, 4 );
		physicsTime += b3GetMilliseconds( ticks );
		SyncTransforms( app );

		ticks = b3GetTicks();
		nbWorld_Update( app.destruction, step );
		destructionTime += b3GetMilliseconds( ticks );
		UpdateBalls( app, step );

		app.accumulator -= step;
		steps += 1;
	}

	if ( steps == maxSteps )
	{
		app.accumulator = b3MinFloat( app.accumulator, step );
	}

	if ( steps > 0 )
	{
		app.physicsTime = physicsTime / (float)steps;
		app.destructionTime = destructionTime / (float)steps;
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

// Everything it takes to tell where the time of a frame goes, to paste into a message
static std::string BuildReport( const App& app )
{
	nbStats stats = nbWorld_GetStats( app.destruction );
	b3Counters counters = b3World_GetCounters( app.physics );
	RenderStats rs = app.renderer.GetStats();
	b3Version version = b3GetVersion();

	float frameAvg, frameMax, impactAvg, impactMax, simulationAvg, simulationMax, graphicsAvg, graphicsMax;
	HistoryStats( app, app.historyFrame, &frameAvg, &frameMax );
	HistoryStats( app, app.historyImpacts, &impactAvg, &impactMax );
	HistoryStats( app, app.historySimulation, &simulationAvg, &simulationMax );
	HistoryStats( app, app.historyGraphics, &graphicsAvg, &graphicsMax );

	std::string text;
	Appendf( text, "Nebenan-Messung: Szene %s, %s-Build, Box3D %d.%d.%d\n", s_sceneNames[app.scene], s_buildType, version.major,
			 version.minor, version.revision );
	Appendf( text, "CPU: %s, %u logische Kerne, %d Performance-Kerne, %d Threads\n", app.cpuName.empty() ? "unbekannt" : app.cpuName.c_str(),
			 std::thread::hardware_concurrency(), app.performanceCores, app.workerCount );
	Appendf( text, "GPU: %s, Bild %d x %d, MSAA %dx, Monitor %d Hz\n", app.gpuName.empty() ? "unbekannt" : app.gpuName.c_str(),
			 sapp_width(), sapp_height(), sapp_sample_count(), app.refreshRate );
	Appendf( text, "Bild: %.0f FPS, 1%% Low %.0f FPS, Mittel %.2f ms, Spitze %.1f ms (letzte %d Bilder), VSync %s, ungebremst %s\n",
			 app.fpsMeter.fps, app.fpsMeter.lowFps, app.fpsMeter.frameTime, frameMax, app.historyCount, app.vsync ? "an" : "aus",
			 app.unthrottled ? "ja" : "nein" );
	Appendf( text, "CPU pro Bild, Mittel / Spitze: Einschläge %.1f / %.1f, Simulation %.1f / %.1f, Grafik %.1f / %.1f ms\n", impactAvg,
			 impactMax, simulationAvg, simulationMax, graphicsAvg, graphicsMax );
	Appendf( text, "Pro Schritt: Box3D %.2f ms, Zerstörung %.2f ms\n", app.physicsTime, app.destructionTime );
	Appendf( text, "Welt: %d Bruchstücke, %d Verbindungen, %d Trümmerkörper (%d wach), %d Schutt, %d Kontakte, Budget %d, Bruchstückgröße x%.2f\n",
			 stats.chunkCount, stats.bondCount, stats.dynamicBodyCount, b3World_GetAwakeBodyCount( app.physics ), stats.rubbleCount,
			 counters.contactCount, app.maxDebrisBodies, app.fragmentScale );
	Appendf( text, "Grafik: %dk Dreiecke, %d Draw Calls, %d kB Upload\n", rs.triangleCount / 1000, rs.drawCalls, rs.uploadedBytes / 1024 );
	return text;
}

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
	if ( ImGui::SliderFloat( "Bruchstückgröße", &app.fragmentScale, 0.5f, 4.0f, "x %.2f" ) )
	{
		nbWorld_SetFragmentScale( app.destruction, app.fragmentScale );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Größere Bruchstücke: weniger Teile pro Einschlag, weniger Körper,\nKontakte und Dreiecke. x2 halbiert ungefähr die Zeit\nbei Massenzerstörung. Gilt für die nächsten Einschläge." );
	}
	if ( ImGui::SliderInt( "Bewegte Trümmer", &app.maxDebrisBodies, 100, 5000 ) )
	{
		nbWorld_SetDebrisBudget( app.destruction, app.maxDebrisBodies, nbDefaultWorldDef().maxRubbleBodies );
	}
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "So viele Trümmer bewegt Box3D höchstens gleichzeitig.\nWeniger macht die Physik schneller." );
	}
	if ( ImGui::SliderInt( "Threads", &app.workerCount, 1, app.maxWorkers ) )
	{
		b3World_SetWorkerCount( app.physics, app.workerCount );
		nbWorld_SetWorkerCount( app.destruction, app.workerCount );
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

	ImGui::SeparatorText( "Messung" );
	nbStats stats = nbWorld_GetStats( app.destruction );
	b3Counters counters = b3World_GetCounters( app.physics );
	RenderStats renderStats = app.renderer.GetStats();
	float frameAvg, frameMax, impactAvg, impactMax, simulationAvg, simulationMax, graphicsAvg, graphicsMax;
	HistoryStats( app, app.historyFrame, &frameAvg, &frameMax );
	HistoryStats( app, app.historyImpacts, &impactAvg, &impactMax );
	HistoryStats( app, app.historySimulation, &simulationAvg, &simulationMax );
	HistoryStats( app, app.historyGraphics, &graphicsAvg, &graphicsMax );
	float cpuFrame = impactAvg + simulationAvg + graphicsAvg;
	const FpsMeter& meter = app.fpsMeter;
	ImGui::Text( "Bild        %6.2f ms  %.0f FPS, 1%% Low %.0f FPS", meter.frameTime, meter.fps, meter.lowFps );
	ImGui::Text( "            Spitze %.1f ms (letzte %d Bilder)", frameMax, app.historyCount );
	ImGui::Text( "CPU         %6.2f ms  Einschläge %.2f, Simulation %.2f,", cpuFrame, impactAvg, simulationAvg );
	ImGui::Text( "                      Grafik %.2f", graphicsAvg );

	// Whatever a frame takes beyond the CPU time is the wait for the display or the graphics card
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
		ImGui::TextColored( hint, "  VSync begrenzt, die CPU schafft etwa %.0f FPS", 1000.0f / b3MaxFloat( cpuFrame, 0.01f ) );
	}
	else if ( waiting )
	{
		ImGui::TextColored( hint, "  Die Grafikkarte begrenzt die FPS" );
	}
	else
	{
		ImGui::TextDisabled( "  Die CPU begrenzt die FPS" );
	}
	ImGui::Text( "Physik      %6.2f ms  (Box3D Schritt)", app.physicsTime );
	ImGui::Text( "Zerstörung  %6.2f ms  (Update)", app.destructionTime );
	ImGui::Text( "Einschlag   %6.2f ms  davon Voronoi %.2f ms", app.lastImpact.totalTime, app.lastImpact.fractureTime );
	ImGui::Text( "Bruchstücke %d  Verbindungen %d", stats.chunkCount, stats.bondCount );
	ImGui::Text( "Trümmerkörper %d  wach %d  Kontakte %d", stats.dynamicBodyCount, b3World_GetAwakeBodyCount( app.physics ),
				 counters.contactCount );
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

static void RunScript( App& app )
{
	int f = app.frame;
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
	BuildGround( app );
	app.vsyncAvailable = DemoSetVsync( app.vsync );
	app.vsync = app.vsync || app.vsyncAvailable == false;
	app.refreshRate = DemoRefreshRate();
	app.unthrottled = DemoUnthrottled();

	// Box3D runs best on the performance cores alone. Hyper-threads and efficiency cores add little or slow it down.
	unsigned hardware = std::thread::hardware_concurrency();
	app.maxWorkers = hardware > 0 ? b3MinInt( (int)hardware, 16 ) : 4;
	app.performanceCores = SystemPerformanceCores();
	int preferredWorkers = app.performanceCores > 0 ? app.performanceCores : app.maxWorkers / 2;
	app.workerCount = b3ClampInt( preferredWorkers, 1, b3MinInt( app.maxWorkers, 8 ) );
	app.cpuName = SystemCpuName();
	app.gpuName = SystemGpuName();

	app.camera.fovY = 60.0f * B3_PI / 180.0f;
	app.tools[ToolRifle] = { 0.35f, 1.2e5f, 9.0f, 0, 8.0f };
	app.tools[ToolGrenade] = { 1.3f, 3.0e5f, 12.0f, 0, 2.0f };
	app.tools[ToolCannon] = { 0.25f, 0.0f, 45.0f, 0, 3.0f };

	int scene = app.automation.scene >= 0 && app.automation.scene < SceneCount ? app.automation.scene : SceneWall;
	LoadScene( app, (SceneKind)scene );
	app.frameTicks = b3GetTicks();
}

static void PrintAutomationReport( App& app )
{
	nbStats stats = nbWorld_GetStats( app.destruction );
	printf( "scene %d frame %d: chunks %d bonds %d debris %d rubble %d\n", (int)app.scene, app.frame, stats.chunkCount, stats.bondCount,
			stats.dynamicBodyCount, stats.rubbleCount );

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
		{ "simulation ms", &app.automation.simulationTimes, 1.0f },
		{ "cpu frame ms", &app.automation.frameTimes, 1.0f },
		{ "graphics ms", &app.automation.graphicsTimes, 1.0f },
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

	// Automated runs advance one physics step per frame, so they repeat exactly on any machine
	if ( app.automation.frameLimit > 0 )
	{
		dt = 1.0f / 60.0f;
	}

	UpdateCamera( app, dt );

	// Automatic fire while the button is held
	uint64_t cpuTicks = b3GetTicks();
	app.fireCooldown -= dt;
	bool uiWantsMouse = ImGui::GetIO().WantCaptureMouse && app.mouseLook == false;
	if ( app.mouseDown && uiWantsMouse == false && app.fireCooldown <= 0.0f )
	{
		float x = app.mouseLook ? 0.5f * sapp_widthf() : app.mouseX;
		float y = app.mouseLook ? 0.5f * sapp_heightf() : app.mouseY;
		Fire( app, x, y );
		app.fireCooldown = 1.0f / app.tools[app.tool].rate;
	}

	if ( app.automation.script )
	{
		RunScript( app );
	}
	float impactTime = b3GetMilliseconds( cpuTicks );

	uint64_t simulationTicks = b3GetTicks();
	StepSimulation( app, dt );
	float simulationTime = b3GetMilliseconds( simulationTicks );

	uint64_t graphicsTicks = b3GetTicks();
	SyncChunks( app );

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
	float graphicsTime = b3GetMilliseconds( graphicsTicks );

	int slot = app.historyNext;
	app.historyFrame[slot] = frameTime;
	app.historyImpacts[slot] = impactTime;
	app.historySimulation[slot] = simulationTime;
	app.historyGraphics[slot] = graphicsTime;
	app.historyNext = ( slot + 1 ) % App::HistorySize;
	app.historyCount = b3MinInt( app.historyCount + 1, App::HistorySize );

	if ( app.automation.frameLimit > 0 )
	{
		RenderStats rs = app.renderer.GetStats();
		app.automation.simulationTimes.push_back( simulationTime );
		app.automation.frameTimes.push_back( b3GetMilliseconds( cpuTicks ) );
		app.automation.graphicsTimes.push_back( graphicsTime );
		app.automation.uploadTimes.push_back( rs.uploadTime );
		app.automation.uploadedBytes.push_back( (float)rs.uploadedBytes );
		app.automation.triangles.push_back( (float)rs.triangleCount );
	}

	app.frame += 1;
	if ( app.automation.frameLimit > 0 && app.frame == app.automation.frameLimit )
	{
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
				app.camera.yaw += 0.0025f * event->mouse_dx;
				app.camera.pitch = b3ClampFloat( app.camera.pitch - 0.0025f * event->mouse_dy, -1.5f, 1.5f );
			}
			else
			{
				app.mouseX = event->mouse_x;
				app.mouseY = event->mouse_y;
			}
			break;

		case SAPP_EVENTTYPE_MOUSE_SCROLL:
			if ( io.WantCaptureMouse == false )
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
