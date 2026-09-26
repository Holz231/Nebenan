// SPDX-License-Identifier: MIT

#include "renderer.h"

#include "box3d/base.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "shaders/generated/scene.glsl.h"

#include <string.h>

// A page holds up to 32768 vertices, so 16 bit indices reach all of them. Convex chunks need about 1.8 indices per
// vertex, boxes 1.5.
static const int PageVertexCapacity = 1 << 15;
static const int PageIndexCapacity = 1 << 16;

// A page is compacted when a third of its vertices belong to removed meshes. Up to two pages worth of vertices
// move per frame, pages without a live mesh are dropped for free.
static const int CompactionBudget = 2 * PageVertexCapacity;

// Floats per transform slot: position and visibility, rotation
static const int SlotFloats = 8;

b3Vec3 Camera::Forward() const
{
	return { cosf( pitch ) * sinf( yaw ), sinf( pitch ), -cosf( pitch ) * cosf( yaw ) };
}

b3Vec3 Camera::Right() const
{
	return b3Normalize( b3Cross( Forward(), b3Vec3{ 0.0f, 1.0f, 0.0f } ) );
}

b3Vec3 Camera::Up() const
{
	return b3Cross( Right(), Forward() );
}

static sg_range MakeRange( const void* ptr, size_t size )
{
	sg_range range;
	range.ptr = ptr;
	range.size = size;
	return range;
}

void Renderer::Init()
{
	sg_backend backend = sg_query_backend();
	m_zeroToOne = backend != SG_BACKEND_GLCORE && backend != SG_BACKEND_GLES3;

	// Transform slots, grown on demand
	m_slotCapacity = 16384;
	m_slotData.resize( (size_t)m_slotCapacity * SlotFloats, 0.0f );

	sg_buffer_desc transformDesc = {};
	transformDesc.usage.storage_buffer = true;
	transformDesc.usage.stream_update = true;
	transformDesc.size = (size_t)m_slotCapacity * SlotFloats * sizeof( float );
	transformDesc.label = "slot_transforms";
	m_transformBuffer = sg_make_buffer( &transformDesc ).id;

	sg_view_desc transformViewDesc = {};
	transformViewDesc.storage_buffer.buffer = sg_buffer{ m_transformBuffer };
	transformViewDesc.label = "slot_transforms_view";
	m_transformView = sg_make_view( &transformViewDesc ).id;

	const sg_environment environment = sglue_environment();
	m_shader = sg_make_shader( scene_scene_shader_desc( backend ) ).id;
	sg_pipeline_desc desc = {};
	desc.shader = sg_shader{ m_shader };
	desc.layout.buffers[0].stride = sizeof( GpuVertex );
	desc.layout.attrs[ATTR_scene_scene_in_position].format = SG_VERTEXFORMAT_FLOAT3;
	desc.layout.attrs[ATTR_scene_scene_in_position].offset = 0;
	desc.layout.attrs[ATTR_scene_scene_in_normal].format = SG_VERTEXFORMAT_BYTE4N;
	desc.layout.attrs[ATTR_scene_scene_in_normal].offset = 12;
	desc.layout.attrs[ATTR_scene_scene_in_slot].format = SG_VERTEXFORMAT_FLOAT;
	desc.layout.attrs[ATTR_scene_scene_in_slot].offset = 16;
	desc.index_type = SG_INDEXTYPE_UINT16;
	desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
	desc.depth.write_enabled = true;
	desc.depth.pixel_format = environment.defaults.depth_format;
	desc.cull_mode = SG_CULLMODE_BACK;
	desc.face_winding = SG_FACEWINDING_CCW;
	desc.sample_count = environment.defaults.sample_count;
	desc.label = "scene_pipeline";
	m_pipeline = sg_make_pipeline( &desc ).id;
}

void Renderer::Shutdown()
{
	for ( Page& page : m_pages )
	{
		sg_destroy_buffer( sg_buffer{ page.vertexBuffer } );
		sg_destroy_buffer( sg_buffer{ page.indexBuffer } );
	}
	m_pages.clear();
	m_emptyPages.clear();
	m_openPage = -1;
	m_meshes.clear();
	m_freeMeshes.clear();

	sg_destroy_pipeline( sg_pipeline{ m_pipeline } );
	sg_destroy_shader( sg_shader{ m_shader } );
	sg_destroy_view( sg_view{ m_transformView } );
	sg_destroy_buffer( sg_buffer{ m_transformBuffer } );
}

int Renderer::AllocSlot()
{
	int slot;
	if ( m_freeSlots.empty() == false )
	{
		slot = m_freeSlots.back();
		m_freeSlots.pop_back();
	}
	else
	{
		slot = m_slotCount++;
	}

	if ( m_slotCount > m_slotCapacity )
	{
		// Grow the storage buffer. The old one is dropped, the next upload writes everything.
		m_slotCapacity *= 2;
		m_slotData.resize( (size_t)m_slotCapacity * SlotFloats, 0.0f );
		sg_destroy_view( sg_view{ m_transformView } );
		sg_destroy_buffer( sg_buffer{ m_transformBuffer } );

		sg_buffer_desc transformDesc = {};
		transformDesc.usage.storage_buffer = true;
		transformDesc.usage.stream_update = true;
		transformDesc.size = (size_t)m_slotCapacity * SlotFloats * sizeof( float );
		transformDesc.label = "slot_transforms";
		m_transformBuffer = sg_make_buffer( &transformDesc ).id;

		sg_view_desc viewDesc = {};
		viewDesc.storage_buffer.buffer = sg_buffer{ m_transformBuffer };
		m_transformView = sg_make_view( &viewDesc ).id;
	}

	if ( (int)m_slotReferences.size() < m_slotCapacity )
	{
		m_slotReferences.resize( (size_t)m_slotCapacity, 0 );
		m_slotFreed.resize( (size_t)m_slotCapacity, 0 );
	}

	m_slotFreed[slot] = 0;
	float* data = m_slotData.data() + (size_t)slot * SlotFloats;
	data[3] = 1.0f;
	SetSlot( slot, b3Vec3_zero, b3Quat_identity );
	return slot;
}

void Renderer::FreeSlot( int slot )
{
	// Hide anything that still references the slot
	float* data = m_slotData.data() + (size_t)slot * SlotFloats;
	data[3] = 0.0f;
	m_slotsDirty = true;

	m_slotFreed[slot] = 1;
	if ( m_slotReferences[slot] == 0 )
	{
		m_freeSlots.push_back( slot );
	}
}

void Renderer::ReleaseSlotReference( int slot )
{
	m_slotReferences[slot] -= 1;
	if ( m_slotReferences[slot] == 0 && m_slotFreed[slot] != 0 )
	{
		m_freeSlots.push_back( slot );
	}
}

void Renderer::SetSlot( int slot, b3Vec3 position, b3Quat rotation )
{
	float* data = m_slotData.data() + (size_t)slot * SlotFloats;
	data[0] = position.x;
	data[1] = position.y;
	data[2] = position.z;
	data[4] = rotation.v.x;
	data[5] = rotation.v.y;
	data[6] = rotation.v.z;
	data[7] = rotation.s;
	m_slotsDirty = true;
}

GpuVertex Renderer::MakeVertex( b3Vec3 position, b3Vec3 normal, int material, int slot )
{
	GpuVertex vertex;
	vertex.position[0] = position.x;
	vertex.position[1] = position.y;
	vertex.position[2] = position.z;
	vertex.normal[0] = (int8_t)lrintf( b3ClampFloat( normal.x, -1.0f, 1.0f ) * 127.0f );
	vertex.normal[1] = (int8_t)lrintf( b3ClampFloat( normal.y, -1.0f, 1.0f ) * 127.0f );
	vertex.normal[2] = (int8_t)lrintf( b3ClampFloat( normal.z, -1.0f, 1.0f ) * 127.0f );
	vertex.normal[3] = (int8_t)material;
	vertex.slot = (float)slot;
	return vertex;
}

// The open page if the mesh fits, else a new open page. The full page keeps its content until compaction.
int Renderer::OpenPage( int vertexCount, int indexCount )
{
	if ( m_openPage >= 0 )
	{
		Page& page = m_pages[m_openPage];
		if ( (int)page.vertices.size() + vertexCount <= PageVertexCapacity && (int)page.indices.size() + indexCount <= PageIndexCapacity )
		{
			return m_openPage;
		}

		// Moves into an immutable buffer
		page.open = false;
		page.dirty = true;
	}

	int pageIndex;
	if ( m_emptyPages.empty() == false )
	{
		pageIndex = m_emptyPages.back();
		m_emptyPages.pop_back();
	}
	else
	{
		pageIndex = (int)m_pages.size();
		m_pages.emplace_back();
	}

	Page& page = m_pages[pageIndex];
	page.vertices.reserve( PageVertexCapacity );
	page.indices.reserve( PageIndexCapacity );
	page.open = true;
	page.dirty = true;
	m_openPage = pageIndex;
	return pageIndex;
}

// Append the mesh to the open page. The indices count from indexBase.
void Renderer::Append( int meshIndex, const GpuVertex* vertices, const uint16_t* indices, int indexBase )
{
	Mesh& mesh = m_meshes[meshIndex];
	int pageIndex = OpenPage( mesh.vertexCount, mesh.indexCount );
	Page& page = m_pages[pageIndex];

	mesh.page = pageIndex;
	mesh.firstVertex = (int)page.vertices.size();
	mesh.firstIndex = (int)page.indices.size();
	mesh.pageEntry = (int)page.meshes.size();
	page.vertices.insert( page.vertices.end(), vertices, vertices + mesh.vertexCount );
	for ( int i = 0; i < mesh.indexCount; ++i )
	{
		page.indices.push_back( (uint16_t)( (int)indices[i] - indexBase + mesh.firstVertex ) );
	}
	page.meshes.push_back( meshIndex );
	page.dirty = true;
}

int Renderer::AddMesh( const GpuVertex* vertices, int vertexCount, const uint16_t* indices, int indexCount, int slot )
{
	if ( vertexCount <= 0 || indexCount <= 0 || vertexCount > PageVertexCapacity || indexCount > PageIndexCapacity )
	{
		return -1;
	}

	int meshIndex;
	if ( m_freeMeshes.empty() == false )
	{
		meshIndex = m_freeMeshes.back();
		m_freeMeshes.pop_back();
	}
	else
	{
		meshIndex = (int)m_meshes.size();
		m_meshes.push_back( Mesh() );
	}

	Mesh& mesh = m_meshes[meshIndex];
	mesh = Mesh();
	mesh.vertexCount = vertexCount;
	mesh.indexCount = indexCount;
	mesh.slot = slot;
	mesh.alive = true;
	m_slotReferences[slot] += 1;

	Append( meshIndex, vertices, indices, 0 );
	return meshIndex;
}

void Renderer::RemoveMesh( int meshIndex )
{
	if ( meshIndex < 0 || meshIndex >= (int)m_meshes.size() || m_meshes[meshIndex].alive == false )
	{
		return;
	}

	// The vertices stay where they are, hidden by the freed slot, until the page is compacted
	Mesh& mesh = m_meshes[meshIndex];
	Page& page = m_pages[mesh.page];
	int last = page.meshes.back();
	page.meshes[mesh.pageEntry] = last;
	m_meshes[last].pageEntry = mesh.pageEntry;
	page.meshes.pop_back();
	page.deadSlots.push_back( mesh.slot );
	page.deadVertices += mesh.vertexCount;

	mesh.alive = false;
	m_freeMeshes.push_back( meshIndex );
}

void Renderer::DropPage( int pageIndex )
{
	Page& page = m_pages[pageIndex];
	for ( int slot : page.deadSlots )
	{
		ReleaseSlotReference( slot );
	}

	sg_destroy_buffer( sg_buffer{ page.vertexBuffer } );
	sg_destroy_buffer( sg_buffer{ page.indexBuffer } );
	page.vertexBuffer = 0;
	page.indexBuffer = 0;
	page.vertices.clear();
	page.indices.clear();
	page.meshes.clear();
	page.deadSlots.clear();
	page.deadVertices = 0;
	page.open = false;
	page.dirty = false;
	page.dynamic = false;
	page.drawCount = 0;

	if ( m_openPage == pageIndex )
	{
		m_openPage = -1;
	}
	m_emptyPages.push_back( pageIndex );
}

// Pages that consist mostly of removed meshes hand their live meshes to the open page and are dropped
void Renderer::Compact()
{
	int budget = CompactionBudget;
	for ( int pageIndex = 0; pageIndex < (int)m_pages.size(); ++pageIndex )
	{
		Page* page = &m_pages[pageIndex];
		int vertexCount = (int)page->vertices.size();
		if ( page->deadVertices == 0 || 3 * page->deadVertices < vertexCount )
		{
			continue;
		}

		int liveVertices = vertexCount - page->deadVertices;
		if ( liveVertices > budget )
		{
			continue;
		}
		budget -= liveVertices;

		// The live meshes of the open page move into a fresh open page as well
		if ( m_openPage == pageIndex )
		{
			page->open = false;
			m_openPage = -1;
		}

		// Moving a mesh may add a page, so the page is looked up again every time
		std::vector<int> meshes;
		meshes.swap( page->meshes );
		for ( int meshIndex : meshes )
		{
			const Mesh& mesh = m_meshes[meshIndex];
			const Page& source = m_pages[pageIndex];
			const GpuVertex* vertices = source.vertices.data() + mesh.firstVertex;
			const uint16_t* indices = source.indices.data() + mesh.firstIndex;
			Append( meshIndex, vertices, indices, mesh.firstVertex );
		}

		DropPage( pageIndex );
	}
}

void Renderer::Upload()
{
	Compact();

	m_stats.uploadedBytes = 0;
	for ( Page& page : m_pages )
	{
		if ( page.dirty == false )
		{
			continue;
		}
		page.dirty = false;
		page.drawCount = (int)page.indices.size();

		size_t vertexBytes = page.vertices.size() * sizeof( GpuVertex );
		size_t indexBytes = page.indices.size() * sizeof( uint16_t );
		if ( page.open && page.dynamic == false )
		{
			// The open page changes from frame to frame
			sg_destroy_buffer( sg_buffer{ page.vertexBuffer } );
			sg_destroy_buffer( sg_buffer{ page.indexBuffer } );

			sg_buffer_desc desc = {};
			desc.usage.vertex_buffer = true;
			desc.usage.dynamic_update = true;
			desc.size = (size_t)PageVertexCapacity * sizeof( GpuVertex );
			desc.label = "open_vertex_page";
			page.vertexBuffer = sg_make_buffer( &desc ).id;

			desc = {};
			desc.usage.index_buffer = true;
			desc.usage.dynamic_update = true;
			desc.size = (size_t)PageIndexCapacity * sizeof( uint16_t );
			desc.label = "open_index_page";
			page.indexBuffer = sg_make_buffer( &desc ).id;
			page.dynamic = true;
		}

		if ( page.open )
		{
			if ( indexBytes > 0 )
			{
				sg_update_buffer( sg_buffer{ page.vertexBuffer }, MakeRange( page.vertices.data(), vertexBytes ) );
				sg_update_buffer( sg_buffer{ page.indexBuffer }, MakeRange( page.indices.data(), indexBytes ) );
			}
		}
		else
		{
			// A full page goes into immutable buffers, which the driver can keep in video memory
			sg_destroy_buffer( sg_buffer{ page.vertexBuffer } );
			sg_destroy_buffer( sg_buffer{ page.indexBuffer } );
			page.vertexBuffer = 0;
			page.indexBuffer = 0;
			page.dynamic = false;
			if ( indexBytes > 0 )
			{
				sg_buffer_desc desc = {};
				desc.usage.vertex_buffer = true;
				desc.data = MakeRange( page.vertices.data(), vertexBytes );
				desc.label = "vertex_page";
				page.vertexBuffer = sg_make_buffer( &desc ).id;

				desc = {};
				desc.usage.index_buffer = true;
				desc.data = MakeRange( page.indices.data(), indexBytes );
				desc.label = "index_page";
				page.indexBuffer = sg_make_buffer( &desc ).id;
			}
		}

		if ( indexBytes > 0 )
		{
			m_stats.uploadedBytes += (int)( vertexBytes + indexBytes );
		}
	}

	if ( m_slotsDirty && m_slotCount > 0 )
	{
		size_t bytes = (size_t)m_slotCount * SlotFloats * sizeof( float );
		sg_update_buffer( sg_buffer{ m_transformBuffer }, MakeRange( m_slotData.data(), bytes ) );
		m_stats.uploadedBytes += (int)bytes;
		m_slotsDirty = false;
	}
}

void Renderer::Render( const Camera& camera, int width, int height )
{
	uint64_t ticks = b3GetTicks();
	Upload();
	m_stats.uploadTime = b3GetMilliseconds( ticks );

	m_stats.drawCalls = 0;
	m_stats.vertexCount = 0;
	m_stats.triangleCount = 0;
	m_stats.pageCount = (int)m_pages.size() - (int)m_emptyPages.size();
	m_stats.slotCount = m_slotCount - (int)m_freeSlots.size();
	m_stats.deadVertexCount = 0;
	for ( const Page& page : m_pages )
	{
		m_stats.deadVertexCount += page.deadVertices;
	}

	float aspect = height > 0 ? (float)width / (float)height : 1.0f;
	b3Vec3 forward = camera.Forward();
	Mat4 view = Mat4LookAt( camera.position, b3Add( camera.position, forward ), b3Vec3{ 0.0f, 1.0f, 0.0f } );
	Mat4 projection = Mat4Perspective( camera.fovY, aspect, 0.05f, 400.0f, m_zeroToOne );

	sg_pass pass = {};
	pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
	pass.action.colors[0].clear_value = { 0.62f, 0.72f, 0.82f, 1.0f };
	pass.action.depth.load_action = SG_LOADACTION_CLEAR;
	pass.action.depth.clear_value = 1.0f;
	pass.swapchain = sglue_swapchain();
	pass.label = "main_pass";
	sg_begin_pass( &pass );

	scene_vs_params_t params = {};
	params.view_proj = Mat4Mul( projection, view );
	b3Vec3 sun = b3Normalize( b3Vec3{ 0.52f, 0.68f, 0.52f } );
	params.sun_dir = Vec4{ sun.x, sun.y, sun.z, 0.0f };

	sg_apply_pipeline( sg_pipeline{ m_pipeline } );
	sg_apply_uniforms( UB_scene_vs_params, MakeRange( &params, sizeof( params ) ) );
	for ( const Page& page : m_pages )
	{
		if ( page.drawCount == 0 )
		{
			continue;
		}

		sg_bindings bindings = {};
		bindings.vertex_buffers[0] = sg_buffer{ page.vertexBuffer };
		bindings.index_buffer = sg_buffer{ page.indexBuffer };
		bindings.views[VIEW_scene_transforms] = sg_view{ m_transformView };
		sg_apply_bindings( &bindings );
		sg_draw( 0, page.drawCount, 1 );
		m_stats.drawCalls += 1;
		m_stats.vertexCount += (int)page.vertices.size();
		m_stats.triangleCount += page.drawCount / 3;
	}
}
