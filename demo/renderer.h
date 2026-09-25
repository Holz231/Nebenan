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
	bool showChunks = false;
	b3Vec3 sunDirection = { 0.52f, 0.68f, 0.52f };
	b3Vec3 sceneCenter = { 0.0f, 2.0f, 0.0f };
	float sceneRadius = 22.0f;
};

struct RenderStats
{
	int drawCalls = 0;
	int vertexCount = 0;
	int pageCount = 0;
	int slotCount = 0;
	int uploadedBytes = 0;
};

// Batched renderer. Meshes are packed into large vertex pages, every vertex references a transform
// slot, and the transforms of all slots are uploaded once per frame into a storage buffer.
class Renderer
{
public:
	void Init();
	void Shutdown();

	int AllocSlot();
	void FreeSlot( int slot );
	void SetSlot( int slot, b3Vec3 position, b3Quat rotation, float scale );

	// Returns a mesh handle. The vertices must already carry their slot.
	int AddMesh( const GpuVertex* vertices, int count );
	void RemoveMesh( int mesh );

	// Pack a vertex
	static GpuVertex MakeVertex( b3Vec3 position, b3Vec3 normal, int material, int slot );

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
		uint32_t buffer = 0;
		std::vector<GpuVertex> vertices;
		std::vector<int> meshes;
		bool dirty = false;
		bool needsRebuild = false;
		int uploadedCount = 0;
	};

	struct Mesh
	{
		std::vector<GpuVertex> vertices;
		int page = -1;
		int first = 0;
		bool alive = false;
	};

	void RebuildPage( int pageIndex );
	void Upload();

	std::vector<Page*> m_pages;
	std::vector<Mesh> m_meshes;
	std::vector<int> m_freeMeshes;

	std::vector<float> m_slotData;
	std::vector<int> m_freeSlots;
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
	uint32_t m_shadowImage = 0;
	uint32_t m_shadowAttachment = 0;
	uint32_t m_shadowTexture = 0;
	uint32_t m_shadowSampler = 0;

	bool m_zeroToOne = true;
	float m_uvYSign = -1.0f;
	RenderStats m_stats;
};
