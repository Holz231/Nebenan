// SPDX-License-Identifier: MIT
// Nebenan demo: polygonal destruction on Box3D.

#include "renderer.h"

#include "imgui.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_imgui.h"
#include "sokol_log.h"

#include "box3d/box3d.h"
#include "nebenan/nebenan.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" bool DemoSaveScreenshot( const char* path, int width, int height );

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
	SceneHouse,
	SceneColonnade,
	SceneTower,
	SceneStress,
	SceneCount
};

static const char* s_sceneNames[SceneCount] = { "Mauern", "Haus", "Säulenhalle", "Turm", "Stresstest" };
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
	uint16_t generation = 0;
	int slot = -1;
	int mesh = -1;
	b3BodyId body = b3_nullBodyId;
	bool alive = false;
};

struct Ball
{
	b3BodyId body;
	int slot;
	int mesh;
	float age;
};

// Dust puff or chip. Pure decoration, simulated on the CPU.
struct Particle
{
	b3Vec3 position;
	b3Vec3 velocity;
	float size;
	float growth;
	float age;
	float life;
	float alpha;
	float color[3];
	bool chip;
};

struct Automation
{
	int frameLimit = 0;
	const char* screenshotPath = nullptr;
	bool script = false;
	int scene = -1;
};

struct App
{
	b3WorldId physics = b3_nullWorldId;
	nbWorldId destruction = nb_nullWorldId;
	Renderer renderer;
	Camera camera = {};
	RenderSettings renderSettings;

	std::vector<ChunkVisual> chunks;
	std::unordered_map<uint64_t, std::vector<int>> bodySlots;
	std::vector<Ball> balls;
	std::vector<GpuVertex> ballMesh;
	std::vector<nbMeshVertex> meshScratch;
	std::vector<GpuVertex> gpuScratch;
	int groundSlot = -1;
	int groundMesh = -1;

	SceneKind scene = SceneWall;
	Tool tool = ToolRifle;
	ToolSettings tools[ToolCount];

	bool paused = false;
	float timeScale = 1.0f;
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

	float frameTime = 0.0f;
	float physicsTime = 0.0f;
	float destructionTime = 0.0f;
	nbImpactResult lastImpact = {};
	float averageFrame = 16.0f;

	std::vector<Particle> particles;
	std::vector<ParticleInstance> particleInstances;
	std::vector<std::pair<float, int>> particleOrder;
	uint32_t particleRandom = 0x9e3779b9u;
	bool showDust = true;
	float frameSimTime = 0.0f;

	Automation automation;
	int frame = 0;
};

static App* s_app = nullptr;

static uint64_t BodyKey( b3BodyId id )
{
	return ( (uint64_t)(uint32_t)id.index1 << 32 ) | ( (uint64_t)id.world0 << 16 ) | id.generation;
}

static void AttachSlot( App& app, b3BodyId body, int slot )
{
	app.bodySlots[BodyKey( body )].push_back( slot );
	b3WorldTransform transform = b3Body_GetTransform( body );
	app.renderer.SetSlot( slot, b3ToVec3( transform.p ), transform.q, 1.0f );
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

static void AddChunkVisual( App& app, nbChunkId id )
{
	int count = nbChunk_GetMeshVertexCount( id );
	if ( count == 0 )
	{
		return;
	}

	app.meshScratch.resize( (size_t)count );
	count = nbChunk_BuildMesh( id, app.meshScratch.data(), count, 1.0f );

	if ( (int)app.chunks.size() <= id.index1 )
	{
		app.chunks.resize( (size_t)id.index1 * 2 + 16 );
	}

	ChunkVisual& visual = app.chunks[id.index1];
	visual.generation = id.generation;
	visual.slot = app.renderer.AllocSlot();
	visual.body = nbChunk_GetBody( id );
	visual.alive = true;

	app.gpuScratch.resize( (size_t)count );
	for ( int i = 0; i < count; ++i )
	{
		const nbMeshVertex& v = app.meshScratch[i];
		app.gpuScratch[i] = Renderer::MakeVertex( v.position, v.normal, (int)v.material, visual.slot );
	}
	visual.mesh = app.renderer.AddMesh( app.gpuScratch.data(), count );
	AttachSlot( app, visual.body, visual.slot );
}

static void RemoveChunkVisual( App& app, int index )
{
	ChunkVisual& visual = app.chunks[index];
	DetachSlot( app, visual.body, visual.slot );
	app.renderer.RemoveMesh( visual.mesh );
	app.renderer.FreeSlot( visual.slot );
	visual = ChunkVisual();
}

//----------------------------------------------------------------------------------------------------------------------
// Dust and chips

static const int MaxParticles = 6000;

// Small xorshift generator, the particles do not need the deterministic one of the library
static float RandomFloat( App& app )
{
	uint32_t x = app.particleRandom;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	app.particleRandom = x;
	return (float)( x >> 8 ) * ( 1.0f / 16777216.0f );
}

static float RandomRange( App& app, float lower, float upper )
{
	return lower + ( upper - lower ) * RandomFloat( app );
}

static b3Vec3 RandomInSphere( App& app )
{
	for ( ;; )
	{
		b3Vec3 v = { RandomRange( app, -1.0f, 1.0f ), RandomRange( app, -1.0f, 1.0f ), RandomRange( app, -1.0f, 1.0f ) };
		if ( b3LengthSquared( v ) <= 1.0f )
		{
			return v;
		}
	}
}

static b3Vec3 DustColor( int material )
{
	switch ( material )
	{
		case MaterialBrick:
		case MaterialBrickInterior:
			return { 0.58f, 0.36f, 0.26f };
		case MaterialPlaster:
			return { 0.78f, 0.76f, 0.72f };
		default:
			return { 0.6f, 0.59f, 0.56f };
	}
}

// Whole particles for the integer part, the fraction becomes a probability
static int StochasticCount( App& app, float count )
{
	int whole = (int)count;
	return whole + ( RandomFloat( app ) < count - (float)whole ? 1 : 0 );
}

static void SpawnDust( App& app, const nbDustEvent& dust )
{
	b3Vec3 base = DustColor( dust.material );
	b3Vec3 point = { (float)dust.point.x, (float)dust.point.y, (float)dust.point.z };
	float radius = b3ClampFloat( dust.radius, 0.05f, 2.0f );

	// About 1500 puffs per cubic meter of dust, chips mostly at impacts
	float chipRate = dust.type == nb_dustImpact ? 900.0f : ( dust.type == nb_dustCollision ? 300.0f : 0.0f );
	int puffCount = StochasticCount( app, b3MinFloat( 1500.0f * dust.volume, 48.0f ) );
	int chipCount = StochasticCount( app, b3MinFloat( chipRate * dust.volume, 36.0f ) );

	for ( int i = 0; i < puffCount && (int)app.particles.size() < MaxParticles; ++i )
	{
		Particle particle;
		particle.position = b3MulAdd( point, 0.5f * radius, RandomInSphere( app ) );
		particle.velocity = b3MulAdd( b3MulSV( 0.6f, dust.velocity ), 0.8f + radius, RandomInSphere( app ) );
		particle.size = b3ClampFloat( RandomRange( app, 0.25f, 0.5f ) * radius, 0.05f, 0.6f );
		particle.growth = RandomRange( app, 0.2f, 0.5f );
		particle.age = 0.0f;
		particle.life = RandomRange( app, 1.8f, 4.0f );
		particle.alpha = RandomRange( app, 0.3f, 0.5f );
		float shade = RandomRange( app, 0.9f, 1.1f );
		particle.color[0] = base.x * shade;
		particle.color[1] = base.y * shade;
		particle.color[2] = base.z * shade;
		particle.chip = false;
		app.particles.push_back( particle );
	}

	for ( int i = 0; i < chipCount && (int)app.particles.size() < MaxParticles; ++i )
	{
		Particle particle;
		particle.position = b3MulAdd( point, 0.3f * radius, RandomInSphere( app ) );
		particle.velocity = b3MulAdd( b3MulSV( 1.2f, dust.velocity ), RandomRange( app, 2.0f, 6.0f ), RandomInSphere( app ) );
		particle.velocity.y += RandomRange( app, 0.5f, 2.5f );
		particle.size = RandomRange( app, 0.008f, 0.025f );
		particle.growth = 0.0f;
		particle.age = 0.0f;
		particle.life = RandomRange( app, 1.5f, 3.5f );
		particle.alpha = 1.0f;
		float shade = RandomRange( app, 0.55f, 0.8f );
		particle.color[0] = base.x * shade;
		particle.color[1] = base.y * shade;
		particle.color[2] = base.z * shade;
		particle.chip = true;
		app.particles.push_back( particle );
	}
}

static void UpdateParticles( App& app, float dt )
{
	if ( dt <= 0.0f )
	{
		return;
	}

	float drag = expf( -2.0f * dt );
	float growthDecay = expf( -0.7f * dt );
	for ( size_t i = 0; i < app.particles.size(); )
	{
		Particle& particle = app.particles[i];
		particle.age += dt;
		if ( particle.age >= particle.life )
		{
			app.particles[i] = app.particles.back();
			app.particles.pop_back();
			continue;
		}

		if ( particle.chip )
		{
			particle.velocity.y -= 9.81f * dt;
			particle.position = b3MulAdd( particle.position, dt, particle.velocity );
			if ( particle.position.y < particle.size && particle.velocity.y < 0.0f )
			{
				// Bounce off the ground and lose most of the sliding speed
				particle.position.y = particle.size;
				particle.velocity.y *= -0.3f;
				particle.velocity.x *= 0.5f;
				particle.velocity.z *= 0.5f;
			}
		}
		else
		{
			// Dust slows down quickly in the air, rises a little and spreads out
			particle.velocity = b3MulSV( drag, particle.velocity );
			particle.velocity.y += 0.15f * dt;
			particle.position = b3MulAdd( particle.position, dt, particle.velocity );
			particle.size += particle.growth * dt;
			particle.growth *= growthDecay;
			float floor = 0.3f * particle.size;
			if ( particle.position.y < floor )
			{
				particle.position.y = floor;
				particle.velocity.y = b3MaxFloat( particle.velocity.y, 0.0f );
			}
		}
		++i;
	}
}

static void BuildParticleInstances( App& app )
{
	app.particleInstances.clear();
	app.particleOrder.clear();

	// Back to front, so the soft dust blends correctly
	for ( int i = 0; i < (int)app.particles.size(); ++i )
	{
		float distance = b3DistanceSquared( app.particles[i].position, app.camera.position );
		app.particleOrder.push_back( { distance, i } );
	}
	std::sort( app.particleOrder.begin(), app.particleOrder.end(),
			   []( const std::pair<float, int>& a, const std::pair<float, int>& b ) { return a.first > b.first; } );

	for ( const std::pair<float, int>& entry : app.particleOrder )
	{
		const Particle& particle = app.particles[entry.second];
		float t = particle.age / particle.life;
		float alpha;
		if ( particle.chip )
		{
			alpha = t > 0.8f ? ( 1.0f - t ) * 5.0f : 1.0f;
		}
		else
		{
			float fadeIn = b3MinFloat( particle.age / 0.08f, 1.0f );
			alpha = particle.alpha * fadeIn * powf( 1.0f - t, 1.5f );
		}

		ParticleInstance instance;
		instance.center[0] = particle.position.x;
		instance.center[1] = particle.position.y;
		instance.center[2] = particle.position.z;
		instance.size = particle.chip ? -particle.size : particle.size;
		instance.color[0] = particle.color[0];
		instance.color[1] = particle.color[1];
		instance.color[2] = particle.color[2];
		instance.alpha = alpha;
		app.particleInstances.push_back( instance );
	}

	app.renderer.SetParticles( app.particleInstances.data(), (int)app.particleInstances.size() );
}

static void SyncChunks( App& app )
{
	nbEvents events = nbWorld_GetEvents( app.destruction );

	if ( app.showDust )
	{
		for ( int i = 0; i < events.dustCount; ++i )
		{
			SpawnDust( app, events.dust[i] );
		}
	}

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
			app.renderer.SetSlot( slot, position, event.transform.q, 1.0f );
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Static meshes

static void AddQuad( std::vector<GpuVertex>& out, b3Vec3 a, b3Vec3 b, b3Vec3 c, b3Vec3 d, b3Vec3 n, int material, int slot )
{
	b3Vec3 corners[6] = { a, b, c, a, c, d };
	for ( b3Vec3 p : corners )
	{
		out.push_back( Renderer::MakeVertex( p, n, material, slot ) );
	}
}

static void BuildGround( App& app )
{
	app.groundSlot = app.renderer.AllocSlot();
	app.renderer.SetSlot( app.groundSlot, b3Vec3_zero, b3Quat_identity, 1.0f );

	std::vector<GpuVertex> vertices;
	float h = 150.0f;
	AddQuad( vertices, { -h, 0.0f, h }, { h, 0.0f, h }, { h, 0.0f, -h }, { -h, 0.0f, -h }, { 0.0f, 1.0f, 0.0f }, MaterialGround,
			 app.groundSlot );
	app.groundMesh = app.renderer.AddMesh( vertices.data(), (int)vertices.size() );
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

	app.ballMesh.clear();
	for ( int index : triangles )
	{
		b3Vec3 n = vertices[index];
		app.ballMesh.push_back( Renderer::MakeVertex( b3MulSV( radius, n ), n, MaterialSteel, 0 ) );
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
	preset.material.tensileStrength = 0.3e6f;
	preset.material.compressiveStrength = 6.0e6f;
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
	preset.material.tensileStrength = 2.0e6f;
	preset.material.compressiveStrength = 3.0e7f;
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
	app.renderSettings.sceneCenter = { 0.0f, 2.0f, -2.0f };
	app.renderSettings.sceneRadius = 16.0f;
}

static void BuildHouseScene( App& app )
{
	MaterialPreset brick = BrickPreset();
	MaterialPreset concrete = ConcretePreset();

	std::vector<nbPieceDef> pieces;
	float width = 9.0f, depth = 6.5f, story = 3.0f, t = 0.3f, slab = 0.25f;

	for ( int floor = 0; floor < 2; ++floor )
	{
		float y = floor * ( story + slab );
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

	// Brick walls and concrete slabs in one structure. The slabs stay single chunks: glued to the masonry they
	// would lift off the walls when they bend, which the joints do not survive.
	AddStructure( app, { 0.0f, 0.0f, 0.0f }, 0.0f, brick, pieces, 7 );

	app.camera.position = { 6.0f, 3.0f, 14.0f };
	app.camera.yaw = -0.4f;
	app.camera.pitch = -0.12f;
	app.renderSettings.sceneCenter = { 0.0f, 3.0f, 0.0f };
	app.renderSettings.sceneRadius = 14.0f;
}

static void BuildColonnadeScene( App& app )
{
	MaterialPreset concrete = ConcretePreset();
	std::vector<nbPieceDef> pieces;

	float spacing = 2.8f, height = 4.0f;
	for ( int row = 0; row < 2; ++row )
	{
		float z = row == 0 ? -2.0f : 2.0f;
		for ( int i = 0; i < 6; ++i )
		{
			float x = ( (float)i - 2.5f ) * spacing;
			pieces.push_back( MakePiece( { x, 0.5f * height, z }, { 0.3f, 0.5f * height, 0.3f }, concrete, MaterialConcrete ) );
		}
		pieces.push_back( MakePiece( { 0.0f, height + 0.3f, z }, { 3.0f * spacing, 0.3f, 0.4f }, concrete, MaterialConcrete ) );
	}
	pieces.push_back( MakePiece( { 0.0f, height + 0.75f, 0.0f }, { 3.0f * spacing + 0.4f, 0.15f, 3.0f }, concrete, MaterialConcrete ) );

	// Pre-fractured into cells, so beams and roof can break along their span once the columns are gone
	AddStructure( app, { 0.0f, 0.0f, 0.0f }, 0.0f, concrete, pieces, 11, 1.2f );

	app.camera.position = { 3.0f, 2.4f, 13.0f };
	app.camera.yaw = -0.2f;
	app.camera.pitch = -0.05f;
	app.renderSettings.sceneCenter = { 0.0f, 2.5f, 0.0f };
	app.renderSettings.sceneRadius = 14.0f;
}

static void BuildTowerScene( App& app )
{
	MaterialPreset brick = BrickPreset();
	std::vector<nbPieceDef> pieces;

	// Hollow tower in rings, every ring rotated so the corners interlock like masonry
	float half = 1.8f, t = 0.35f, ring = 1.5f;
	for ( int level = 0; level < 8; ++level )
	{
		float y = level * ring + 0.5f * ring;
		bool odd = ( level & 1 ) != 0;
		float longHalf = half, shortHalf = half - t;
		if ( odd )
		{
			pieces.push_back( MakePiece( { 0.0f, y, half - 0.5f * t }, { longHalf, 0.5f * ring, 0.5f * t }, brick, brick.surface ) );
			pieces.push_back( MakePiece( { 0.0f, y, -half + 0.5f * t }, { longHalf, 0.5f * ring, 0.5f * t }, brick, brick.surface ) );
			pieces.push_back( MakePiece( { half - 0.5f * t, y, 0.0f }, { 0.5f * t, 0.5f * ring, shortHalf }, brick, brick.surface ) );
			pieces.push_back( MakePiece( { -half + 0.5f * t, y, 0.0f }, { 0.5f * t, 0.5f * ring, shortHalf }, brick, brick.surface ) );
		}
		else
		{
			pieces.push_back( MakePiece( { half - 0.5f * t, y, 0.0f }, { 0.5f * t, 0.5f * ring, longHalf }, brick, brick.surface ) );
			pieces.push_back( MakePiece( { -half + 0.5f * t, y, 0.0f }, { 0.5f * t, 0.5f * ring, longHalf }, brick, brick.surface ) );
			pieces.push_back( MakePiece( { 0.0f, y, half - 0.5f * t }, { shortHalf, 0.5f * ring, 0.5f * t }, brick, brick.surface ) );
			pieces.push_back( MakePiece( { 0.0f, y, -half + 0.5f * t }, { shortHalf, 0.5f * ring, 0.5f * t }, brick, brick.surface ) );
		}
	}

	AddStructure( app, { 0.0f, 0.0f, 0.0f }, 0.3f, brick, pieces, 13 );

	app.camera.position = { 2.0f, 4.0f, 16.0f };
	app.camera.yaw = -0.1f;
	app.camera.pitch = 0.1f;
	app.renderSettings.sceneCenter = { 0.0f, 6.0f, 0.0f };
	app.renderSettings.sceneRadius = 16.0f;
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
	app.renderSettings.sceneCenter = { 0.0f, 2.0f, -6.0f };
	app.renderSettings.sceneRadius = 20.0f;
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
	app.particles.clear();
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
	def.maxDebrisBodies = 4000;
	def.collisionRadiusScale = 0.02f;
	def.workerCount = app.workerCount;
	app.destruction = nbCreateWorld( &def );

	switch ( scene )
	{
		case SceneWall:
			BuildWallScene( app );
			break;
		case SceneHouse:
			BuildHouseScene( app );
			break;
		case SceneColonnade:
			BuildColonnadeScene( app );
			break;
		case SceneTower:
			BuildTowerScene( app );
			break;
		default:
			BuildStressScene( app );
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
		for ( GpuVertex& v : app.ballMesh )
		{
			v.slot = (float)ball.slot;
		}
		ball.mesh = app.renderer.AddMesh( app.ballMesh.data(), (int)app.ballMesh.size() );
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

	const float step = 1.0f / 60.0f;
	int steps = 0;
	float physicsTime = 0.0f;
	float destructionTime = 0.0f;

	auto runStep = [&]( float dt ) {
		app.frameSimTime += dt;
		uint64_t ticks = b3GetTicks();
		b3World_Step( app.physics, dt, 4 );
		physicsTime += b3GetMilliseconds( ticks );
		SyncTransforms( app );

		ticks = b3GetTicks();
		nbWorld_Update( app.destruction, dt );
		destructionTime += b3GetMilliseconds( ticks );
		UpdateBalls( app, dt );
	};

	if ( app.timeScale < 0.999f )
	{
		// Slow motion runs one short step per frame so the motion stays smooth
		runStep( b3MinFloat( frameDt, 1.0f / 30.0f ) * app.timeScale );
		steps = 1;
	}
	else
	{
		app.accumulator += b3MinFloat( frameDt, 0.1f );
		while ( app.accumulator >= step && steps < 3 )
		{
			runStep( step );
			app.accumulator -= step;
			steps += 1;
		}
		if ( steps == 3 )
		{
			app.accumulator = 0.0f;
		}
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

static void DrawUi( App& app )
{
	float dpi = sapp_dpi_scale();
	ImGui::SetNextWindowPos( ImVec2( 12.0f * dpi, 12.0f * dpi ), ImGuiCond_FirstUseEver );
	ImGui::SetNextWindowSize( ImVec2( 330.0f * dpi, 0.0f ), ImGuiCond_FirstUseEver );
	ImGui::Begin( "Nebenan - Zerstörung", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

	b3Version version = b3GetVersion();
	ImGui::TextDisabled( "Box3D %d.%d.%d  |  Polygone statt Voxel", version.major, version.minor, version.revision );

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

	ImGui::SeparatorText( "Simulation" );
	ImGui::Checkbox( "Pause (P)", &app.paused );
	ImGui::SameLine();
	bool slowMotion = app.timeScale < 0.999f;
	if ( ImGui::Checkbox( "Zeitlupe (T)", &slowMotion ) )
	{
		app.timeScale = slowMotion ? 0.2f : 1.0f;
	}
	if ( slowMotion )
	{
		ImGui::SliderFloat( "Zeitfaktor", &app.timeScale, 0.02f, 0.99f, "%.2f" );
	}
	if ( ImGui::SliderInt( "Threads", &app.workerCount, 1, app.maxWorkers ) )
	{
		b3World_SetWorkerCount( app.physics, app.workerCount );
		nbWorld_SetWorkerCount( app.destruction, app.workerCount );
	}
	if ( ImGui::Button( "Trümmer entfernen (C)" ) )
	{
		nbWorld_ClearDebris( app.destruction );
	}

	ImGui::SeparatorText( "Anzeige" );
	ImGui::Checkbox( "Schatten", &app.renderSettings.shadows );
	ImGui::SameLine();
	ImGui::Checkbox( "Bruchstücke einfärben (F)", &app.renderSettings.showChunks );
	if ( ImGui::Checkbox( "Staub und Splitter", &app.showDust ) && app.showDust == false )
	{
		app.particles.clear();
	}

	ImGui::SeparatorText( "Leistung" );
	nbStats stats = nbWorld_GetStats( app.destruction );
	b3Counters counters = b3World_GetCounters( app.physics );
	RenderStats renderStats = app.renderer.GetStats();
	ImGui::Text( "Bild        %6.2f ms  (%.0f FPS)", app.averageFrame, 1000.0f / b3MaxFloat( app.averageFrame, 0.01f ) );
	ImGui::Text( "Physik      %6.2f ms  (Box3D Schritt)", app.physicsTime );
	ImGui::Text( "Zerstörung  %6.2f ms  (Update)", app.destructionTime );
	ImGui::Text( "Einschlag   %6.2f ms  davon Voronoi %.2f ms", app.lastImpact.totalTime, app.lastImpact.fractureTime );
	ImGui::Text( "  %d neue Stücke, %d Verbindungen gerissen, %d gelöst", app.lastImpact.createdChunkCount,
				 app.lastImpact.brokenBondCount, app.lastImpact.detachedChunkCount );
	ImGui::Text( "Bruchstücke %d  Verbindungen %d", stats.chunkCount, stats.bondCount );
	ImGui::Text( "Überlastet %d  (Lastnachweis)", stats.overloadedBondCount );
	ImGui::Text( "Trümmerkörper %d  wach %d  Kontakte %d", stats.dynamicBodyCount, b3World_GetAwakeBodyCount( app.physics ),
				 counters.contactCount );
	ImGui::Text( "Draw Calls %d  Vertices %d  Partikel %d", renderStats.drawCalls, renderStats.vertexCount, renderStats.particleCount );

	ImGui::SeparatorText( "Steuerung" );
	ImGui::TextWrapped( "Linke Maustaste: schießen (halten = Dauerfeuer)\n"
						"Rechte Maustaste halten: umsehen\n"
						"WASD bewegen, Q/E runter/hoch, Shift schneller\n"
						"1-3 Werkzeug, R neu laden, P Pause, T Zeitlupe, F1 Menü" );

	ImGui::End();

	// Crosshair at the aim point
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	ImVec2 center = app.mouseLook ? ImVec2( 0.5f * sapp_widthf() / dpi, 0.5f * sapp_heightf() / dpi )
								  : ImVec2( app.mouseX / dpi, app.mouseY / dpi );
	ImU32 color = IM_COL32( 255, 255, 255, 200 );
	draw->AddLine( ImVec2( center.x - 9, center.y ), ImVec2( center.x - 3, center.y ), color, 2.0f );
	draw->AddLine( ImVec2( center.x + 3, center.y ), ImVec2( center.x + 9, center.y ), color, 2.0f );
	draw->AddLine( ImVec2( center.x, center.y - 9 ), ImVec2( center.x, center.y - 3 ), color, 2.0f );
	draw->AddLine( ImVec2( center.x, center.y + 3 ), ImVec2( center.x, center.y + 9 ), color, 2.0f );
}

//----------------------------------------------------------------------------------------------------------------------
// Scripted run for automated screenshots

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

		case SceneHouse:
			if ( f >= 30 && f <= 130 && f % 10 == 0 )
			{
				// Blast the ground floor front, right and back walls. The floors above hang on the left wall.
				int k = ( f - 30 ) / 10;
				b3Vec3 targets[11] = { { -3.4f, 1.2f, 3.25f },	{ -1.1f, 1.2f, 3.25f }, { 1.1f, 1.2f, 3.25f },  { 3.4f, 1.2f, 3.25f },
									   { 4.5f, 1.2f, 2.0f },	{ 4.5f, 1.2f, 0.0f },	{ 4.5f, 1.2f, -2.0f },	{ 3.4f, 1.2f, -3.25f },
									   { 1.1f, 1.2f, -3.25f }, { -1.1f, 1.2f, -3.25f }, { -3.4f, 1.2f, -3.25f } };
				FireAt( app, ToolGrenade, targets[k] );
			}
			break;

		case SceneColonnade:
			if ( f >= 20 && f <= 95 && f % 15 == 5 )
			{
				// Blow out the front row of columns one after another
				int k = ( f - 20 ) / 15;
				FireAt( app, ToolGrenade, { ( (float)k - 2.5f ) * 2.8f, 1.5f, 2.3f } );
			}
			break;

		case SceneTower:
			if ( f >= 20 && f <= 70 && f % 10 == 0 )
			{
				FireAt( app, ToolGrenade, { -1.0f + 0.5f * (float)( ( f - 20 ) / 10 ), 0.8f, 1.9f } );
			}
			break;

		default:
			if ( f >= 20 && f < 120 && f % 5 == 0 )
			{
				int k = ( f - 20 ) / 5;
				FireAt( app, k % 3 == 0 ? ToolGrenade : ToolRifle, { -8.0f + 0.8f * (float)k, 1.5f, 0.15f } );
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

	unsigned hardware = std::thread::hardware_concurrency();
	app.maxWorkers = hardware > 0 ? (int)b3MinFloat( (float)hardware, 16.0f ) : 4;
	app.workerCount = app.maxWorkers > 1 ? b3MinInt( app.maxWorkers, 8 ) : 1;

	app.camera.fovY = 60.0f * B3_PI / 180.0f;
	app.tools[ToolRifle] = { 0.35f, 1.2e5f, 9.0f, 0, 8.0f };
	app.tools[ToolGrenade] = { 1.3f, 3.0e5f, 12.0f, 0, 2.0f };
	app.tools[ToolCannon] = { 0.25f, 0.0f, 45.0f, 0, 3.0f };

	LoadScene( app, app.automation.scene >= 0 ? (SceneKind)app.automation.scene : SceneWall );
}

static void OnFrame()
{
	App& app = *s_app;
	float dt = (float)sapp_frame_duration();
	dt = b3ClampFloat( dt, 0.0f, 0.1f );
	app.averageFrame = 0.95f * app.averageFrame + 0.05f * 1000.0f * dt;

	// Automated runs advance one physics step per frame, so they repeat exactly on any machine
	if ( app.automation.frameLimit > 0 )
	{
		dt = 1.0f / 60.0f;
	}

	UpdateCamera( app, dt );

	// Automatic fire while the button is held
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

	app.frameSimTime = 0.0f;
	StepSimulation( app, dt );
	SyncChunks( app );
	UpdateParticles( app, app.frameSimTime );
	BuildParticleInstances( app );

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

	app.renderer.Render( app.camera, app.renderSettings, width, height );
	simgui_render();
	sg_end_pass();
	sg_commit();

	app.frame += 1;
	if ( app.automation.frameLimit > 0 && app.frame >= app.automation.frameLimit )
	{
		nbStats stats = nbWorld_GetStats( app.destruction );
		printf( "scene %d frame %d: chunks %d bonds %d debris %d overloaded %d\n", (int)app.scene, app.frame, stats.chunkCount,
				stats.bondCount, stats.dynamicBodyCount, stats.overloadedBondCount );

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
			else if ( event->key_code == SAPP_KEYCODE_T )
			{
				app.timeScale = app.timeScale < 0.999f ? 1.0f : 0.2f;
			}
			else if ( event->key_code == SAPP_KEYCODE_C )
			{
				nbWorld_ClearDebris( app.destruction );
			}
			else if ( event->key_code == SAPP_KEYCODE_F )
			{
				app.renderSettings.showChunks = !app.renderSettings.showChunks;
			}
			else if ( event->key_code == SAPP_KEYCODE_F1 )
			{
				app.showUi = !app.showUi;
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
	}

	sapp_desc desc = {};
	desc.init_cb = OnInit;
	desc.frame_cb = OnFrame;
	desc.event_cb = OnEvent;
	desc.cleanup_cb = OnCleanup;
	desc.width = 1600;
	desc.height = 900;
	desc.sample_count = 4;
	desc.high_dpi = true;
	desc.window_title = "Nebenan - polygonale Zerstörung mit Box3D";
	desc.gl.major_version = 4;
	desc.gl.minor_version = 3;
	desc.logger.func = slog_func;
	sapp_run( &desc );
	return 0;
}
