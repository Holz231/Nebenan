// SPDX-License-Identifier: MIT

#pragma once

#include "math3d.h"

#include "box3d/math_functions.h"

#include <stdint.h>
#include <vector>

// Materials understood by the scene shader
enum RenderMaterial
{
	MaterialBrick = 0,
	MaterialBrickInterior = 1,
	MaterialConcrete = 2,
	MaterialConcreteInterior = 3,
	MaterialPlaster = 4,
	MaterialGround = 8,
	MaterialSteel = 9,
};

// 20 byte vertex: position, normal with the material in w, transform slot
struct GpuVertex
{
	float position[3];
	int8_t normal[4];
	float slot;
};

// A dust puff or a chip. 32 bytes, uploaded as one instance each.
struct ParticleInstance
{
	float center[3];
	// Half size in meters, negative for a solid chip
	float size;
	float color[3];
	float alpha;
};

struct Camera
{
	b3Vec3 position;
	float yaw;
	float pitch;
	float fovY;

	b3Vec3 Forward() const;
	b3Vec3 Right() const;
	b3Vec3 Up() const;
};

struct RenderSettings
{
	bool shadows = true;

	// Edge length of the shadow map in texels
	int shadowResolution = 2048;

	// Plain colors instead of the procedural brick, plaster and concrete, for slow graphics cards
	bool simpleMaterials = false;

	bool showChunks = false;
	bool showLoad = false;
	b3Vec3 sunDirection = { 0.52f, 0.68f, 0.52f };
	b3Vec3 sceneCenter = { 0.0f, 2.0f, 0.0f };
	float sceneRadius = 22.0f;
};

struct RenderStats
{
	int drawCalls = 0;
	int vertexCount = 0;
	int triangleCount = 0;
	int particleCount = 0;
	int pageCount = 0;
	int slotCount = 0;

	// Bytes sent to the GPU this frame, and vertices that belong to removed meshes and wait for compaction
	int uploadedBytes = 0;
	int deadVertexCount = 0;

	// CPU time for compaction and uploads
	float uploadTime = 0.0f;
};

// Batched renderer. Meshes are packed into large indexed vertex pages, every vertex references a transform
// slot, and the transforms of all slots are uploaded once per frame into a storage buffer.
//
// A full page lives in an immutable GPU buffer and is never touched again until a third of its vertices belong to
// removed meshes. Then its remaining meshes move to the open page and the page is dropped. Removed meshes stay
// in their page until then, hidden through their freed slot, so removing debris costs no upload at all.
class Renderer
{
public:
	void Init();
	void Shutdown();

	int AllocSlot();

	// Hides the slot. It is handed out again once no vertex page holds a mesh with this slot anymore.
	void FreeSlot( int slot );
	void SetSlot( int slot, b3Vec3 position, b3Quat rotation );

	// Load utilization of the chunk in the slot for the load view, negative for none
	void SetSlotLoad( int slot, float load );

	// Returns a mesh handle. The indices count from the first vertex and all vertices must carry the slot.
	int AddMesh( const GpuVertex* vertices, int vertexCount, const uint16_t* indices, int indexCount, int slot );

	// The slot of a removed mesh has to be freed as well, that hides the vertices until the page is compacted
	void RemoveMesh( int mesh );

	// Pack a vertex
	static GpuVertex MakeVertex( b3Vec3 position, b3Vec3 normal, int material, int slot );

	// Particles to draw this frame, sorted back to front. Drawn after the opaque geometry.
	void SetParticles( const ParticleInstance* particles, int count );

	static const int MaxParticles = 8192;

	void Render( const Camera& camera, const RenderSettings& settings, int width, int height );

	RenderStats GetStats() const
	{
		return m_stats;
	}

	// Worst case clip space conventions differ between backends
	bool IsZeroToOne() const
	{
		return m_zeroToOne;
	}

private:
	struct Page
	{
		uint32_t vertexBuffer = 0;
		uint32_t indexBuffer = 0;
		std::vector<GpuVertex> vertices;
		std::vector<uint16_t> indices;

		// Live meshes, and the slots of removed meshes whose vertices are still in the page
		std::vector<int> meshes;
		std::vector<int> deadSlots;
		int deadVertices = 0;

		// The open page takes new meshes and lives in a dynamic buffer, a full page in an immutable one
		bool open = false;
		bool dirty = false;
		bool dynamic = false;
		int drawCount = 0;
	};

	struct Mesh
	{
		int page = -1;
		int firstVertex = 0;
		int vertexCount = 0;
		int firstIndex = 0;
		int indexCount = 0;
		int slot = -1;

		// Position in the mesh list of the page
		int pageEntry = -1;
		bool alive = false;
	};

	void CreateShadowMap( int resolution );
	void DestroyShadowMap();

	int OpenPage( int vertexCount, int indexCount );
	void Append( int meshIndex, const GpuVertex* vertices, const uint16_t* indices, int indexBase );
	void ReleaseSlotReference( int slot );
	void DropPage( int pageIndex );
	void Compact();
	void Upload();

	std::vector<Page> m_pages;
	std::vector<int> m_emptyPages;
	int m_openPage = -1;
	std::vector<Mesh> m_meshes;
	std::vector<int> m_freeMeshes;

	std::vector<float> m_slotData;
	std::vector<int> m_freeSlots;

	// Meshes in the pages that still carry the slot, alive or removed, and whether the slot was freed
	std::vector<int> m_slotReferences;
	std::vector<uint8_t> m_slotFreed;
	int m_slotCount = 0;
	int m_slotCapacity = 0;
	bool m_slotsDirty = true;

	uint32_t m_transformBuffer = 0;
	uint32_t m_transformView = 0;
	uint32_t m_litPipeline = 0;
	uint32_t m_shadowPipeline = 0;
	uint32_t m_skyPipeline = 0;
	uint32_t m_litShader = 0;
	uint32_t m_shadowShader = 0;
	uint32_t m_skyShader = 0;
	uint32_t m_particlePipeline = 0;
	uint32_t m_particleShader = 0;
	uint32_t m_cornerBuffer = 0;
	uint32_t m_instanceBuffer = 0;
	std::vector<ParticleInstance> m_particles;
	uint32_t m_shadowImage = 0;
	uint32_t m_shadowAttachment = 0;
	uint32_t m_shadowTexture = 0;
	uint32_t m_shadowSampler = 0;
	int m_shadowResolution = 0;

	bool m_zeroToOne = true;
	float m_uvYSign = -1.0f;
	RenderStats m_stats;
};
