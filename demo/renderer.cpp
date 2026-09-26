// SPDX-License-Identifier: MIT

#include "renderer.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "shaders/generated/scene.glsl.h"

#include <string.h>

static const int PageCapacity = 1 << 16;
static const int ShadowResolution = 4096;

// Floats per transform slot: position and scale, rotation, load and three spare
static const int SlotFloats = 12;

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
	m_uvYSign = m_zeroToOne ? -1.0f : 1.0f;

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

	// Shadow map
	sg_image_desc shadowDesc = {};
	shadowDesc.usage.depth_stencil_attachment = true;
	shadowDesc.width = ShadowResolution;
	shadowDesc.height = ShadowResolution;
	shadowDesc.pixel_format = SG_PIXELFORMAT_DEPTH;
	shadowDesc.sample_count = 1;
	shadowDesc.label = "shadow_map";
	m_shadowImage = sg_make_image( &shadowDesc ).id;

	sg_view_desc attachmentDesc = {};
	attachmentDesc.depth_stencil_attachment.image = sg_image{ m_shadowImage };
	m_shadowAttachment = sg_make_view( &attachmentDesc ).id;

	sg_view_desc textureDesc = {};
	textureDesc.texture.image = sg_image{ m_shadowImage };
	m_shadowTexture = sg_make_view( &textureDesc ).id;

	sg_sampler_desc samplerDesc = {};
	samplerDesc.min_filter = SG_FILTER_LINEAR;
	samplerDesc.mag_filter = SG_FILTER_LINEAR;
	samplerDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
	samplerDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
	samplerDesc.compare = SG_COMPAREFUNC_LESS_EQUAL;
	samplerDesc.label = "shadow_sampler";
	m_shadowSampler = sg_make_sampler( &samplerDesc ).id;

	const sg_environment environment = sglue_environment();

	m_litShader = sg_make_shader( scene_lit_shader_desc( backend ) ).id;
	sg_pipeline_desc litDesc = {};
	litDesc.shader = sg_shader{ m_litShader };
	litDesc.layout.buffers[0].stride = sizeof( GpuVertex );
	litDesc.layout.attrs[ATTR_scene_lit_in_position].format = SG_VERTEXFORMAT_FLOAT3;
	litDesc.layout.attrs[ATTR_scene_lit_in_position].offset = 0;
	litDesc.layout.attrs[ATTR_scene_lit_in_normal].format = SG_VERTEXFORMAT_BYTE4N;
	litDesc.layout.attrs[ATTR_scene_lit_in_normal].offset = 12;
	litDesc.layout.attrs[ATTR_scene_lit_in_slot].format = SG_VERTEXFORMAT_FLOAT;
	litDesc.layout.attrs[ATTR_scene_lit_in_slot].offset = 16;
	litDesc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
	litDesc.depth.write_enabled = true;
	litDesc.depth.pixel_format = environment.defaults.depth_format;
	litDesc.cull_mode = SG_CULLMODE_BACK;
	litDesc.face_winding = SG_FACEWINDING_CCW;
	litDesc.sample_count = environment.defaults.sample_count;
	litDesc.label = "lit_pipeline";
	m_litPipeline = sg_make_pipeline( &litDesc ).id;

	m_shadowShader = sg_make_shader( scene_shadow_shader_desc( backend ) ).id;
	sg_pipeline_desc shadowPipelineDesc = {};
	shadowPipelineDesc.shader = sg_shader{ m_shadowShader };
	shadowPipelineDesc.layout.buffers[0].stride = sizeof( GpuVertex );
	shadowPipelineDesc.layout.attrs[ATTR_scene_shadow_in_position].format = SG_VERTEXFORMAT_FLOAT3;
	shadowPipelineDesc.layout.attrs[ATTR_scene_shadow_in_position].offset = 0;
	shadowPipelineDesc.layout.attrs[ATTR_scene_shadow_in_slot].format = SG_VERTEXFORMAT_FLOAT;
	shadowPipelineDesc.layout.attrs[ATTR_scene_shadow_in_slot].offset = 16;
	shadowPipelineDesc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
	shadowPipelineDesc.depth.write_enabled = true;
	shadowPipelineDesc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
	shadowPipelineDesc.depth.bias = 1.0f;
	shadowPipelineDesc.depth.bias_slope_scale = 1.5f;
	shadowPipelineDesc.color_count = 0;
	shadowPipelineDesc.cull_mode = SG_CULLMODE_NONE;
	shadowPipelineDesc.sample_count = 1;
	shadowPipelineDesc.label = "shadow_pipeline";
	m_shadowPipeline = sg_make_pipeline( &shadowPipelineDesc ).id;

	m_skyShader = sg_make_shader( scene_sky_shader_desc( backend ) ).id;
	sg_pipeline_desc skyDesc = {};
	skyDesc.shader = sg_shader{ m_skyShader };
	skyDesc.depth.compare = SG_COMPAREFUNC_ALWAYS;
	skyDesc.depth.write_enabled = false;
	skyDesc.depth.pixel_format = environment.defaults.depth_format;
	skyDesc.sample_count = environment.defaults.sample_count;
	skyDesc.label = "sky_pipeline";
	m_skyPipeline = sg_make_pipeline( &skyDesc ).id;

	// Particles: a static quad and a stream of instances
	static const float corners[12] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
	sg_buffer_desc cornerDesc = {};
	cornerDesc.data = MakeRange( corners, sizeof( corners ) );
	cornerDesc.label = "particle_corners";
	m_cornerBuffer = sg_make_buffer( &cornerDesc ).id;

	sg_buffer_desc instanceDesc = {};
	instanceDesc.usage.vertex_buffer = true;
	instanceDesc.usage.stream_update = true;
	instanceDesc.size = MaxParticles * sizeof( ParticleInstance );
	instanceDesc.label = "particle_instances";
	m_instanceBuffer = sg_make_buffer( &instanceDesc ).id;

	m_particleShader = sg_make_shader( scene_particle_shader_desc( backend ) ).id;
	sg_pipeline_desc particleDesc = {};
	particleDesc.shader = sg_shader{ m_particleShader };
	particleDesc.layout.buffers[0].stride = 2 * sizeof( float );
	particleDesc.layout.buffers[1].stride = sizeof( ParticleInstance );
	particleDesc.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
	particleDesc.layout.attrs[ATTR_scene_particle_in_corner].format = SG_VERTEXFORMAT_FLOAT2;
	particleDesc.layout.attrs[ATTR_scene_particle_in_corner].buffer_index = 0;
	particleDesc.layout.attrs[ATTR_scene_particle_in_center_size].format = SG_VERTEXFORMAT_FLOAT4;
	particleDesc.layout.attrs[ATTR_scene_particle_in_center_size].buffer_index = 1;
	particleDesc.layout.attrs[ATTR_scene_particle_in_center_size].offset = 0;
	particleDesc.layout.attrs[ATTR_scene_particle_in_color_alpha].format = SG_VERTEXFORMAT_FLOAT4;
	particleDesc.layout.attrs[ATTR_scene_particle_in_color_alpha].buffer_index = 1;
	particleDesc.layout.attrs[ATTR_scene_particle_in_color_alpha].offset = 16;
	particleDesc.colors[0].blend.enabled = true;
	particleDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
	particleDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	particleDesc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
	particleDesc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	particleDesc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
	particleDesc.depth.write_enabled = false;
	particleDesc.depth.pixel_format = environment.defaults.depth_format;
	particleDesc.cull_mode = SG_CULLMODE_NONE;
	particleDesc.sample_count = environment.defaults.sample_count;
	particleDesc.label = "particle_pipeline";
	m_particlePipeline = sg_make_pipeline( &particleDesc ).id;
}

void Renderer::Shutdown()
{
	for ( Page* page : m_pages )
	{
		sg_destroy_buffer( sg_buffer{ page->buffer } );
		delete page;
	}
	m_pages.clear();
	m_meshes.clear();
	m_freeMeshes.clear();

	sg_destroy_pipeline( sg_pipeline{ m_litPipeline } );
	sg_destroy_pipeline( sg_pipeline{ m_shadowPipeline } );
	sg_destroy_pipeline( sg_pipeline{ m_skyPipeline } );
	sg_destroy_pipeline( sg_pipeline{ m_particlePipeline } );
	sg_destroy_shader( sg_shader{ m_particleShader } );
	sg_destroy_buffer( sg_buffer{ m_cornerBuffer } );
	sg_destroy_buffer( sg_buffer{ m_instanceBuffer } );
	sg_destroy_shader( sg_shader{ m_litShader } );
	sg_destroy_shader( sg_shader{ m_shadowShader } );
	sg_destroy_shader( sg_shader{ m_skyShader } );
	sg_destroy_view( sg_view{ m_transformView } );
	sg_destroy_buffer( sg_buffer{ m_transformBuffer } );
	sg_destroy_view( sg_view{ m_shadowAttachment } );
	sg_destroy_view( sg_view{ m_shadowTexture } );
	sg_destroy_image( sg_image{ m_shadowImage } );
	sg_destroy_sampler( sg_sampler{ m_shadowSampler } );
}

void Renderer::SetParticles( const ParticleInstance* particles, int count )
{
	count = count < MaxParticles ? count : MaxParticles;
	m_particles.assign( particles, particles + count );
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

	SetSlot( slot, b3Vec3_zero, b3Quat_identity, 1.0f );
	SetSlotLoad( slot, -1.0f );
	return slot;
}

void Renderer::FreeSlot( int slot )
{
	// Hide anything that still references the slot
	SetSlot( slot, b3Vec3_zero, b3Quat_identity, 0.0f );
	SetSlotLoad( slot, -1.0f );
	m_freeSlots.push_back( slot );
}

void Renderer::SetSlotLoad( int slot, float load )
{
	float* data = m_slotData.data() + (size_t)slot * SlotFloats;
	if ( data[8] != load )
	{
		data[8] = load;
		m_slotsDirty = true;
	}
}

void Renderer::SetSlot( int slot, b3Vec3 position, b3Quat rotation, float scale )
{
	float* data = m_slotData.data() + (size_t)slot * SlotFloats;
	data[0] = position.x;
	data[1] = position.y;
	data[2] = position.z;
	data[3] = scale;
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

int Renderer::AddMesh( const GpuVertex* vertices, int count )
{
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
	mesh.vertices.assign( vertices, vertices + count );
	mesh.alive = true;

	// Append to the newest page with room
	int pageIndex = -1;
	for ( int i = (int)m_pages.size() - 1; i >= 0 && i >= (int)m_pages.size() - 2; --i )
	{
		if ( (int)m_pages[i]->vertices.size() + count <= PageCapacity )
		{
			pageIndex = i;
			break;
		}
	}

	if ( pageIndex < 0 )
	{
		Page* page = new Page();
		sg_buffer_desc desc = {};
		desc.usage.vertex_buffer = true;
		desc.usage.dynamic_update = true;
		desc.size = (size_t)PageCapacity * sizeof( GpuVertex );
		desc.label = "vertex_page";
		page->buffer = sg_make_buffer( &desc ).id;
		page->vertices.reserve( PageCapacity );
		m_pages.push_back( page );
		pageIndex = (int)m_pages.size() - 1;
	}

	Page* page = m_pages[pageIndex];
	mesh.page = pageIndex;
	mesh.first = (int)page->vertices.size();
	page->vertices.insert( page->vertices.end(), vertices, vertices + count );
	page->meshes.push_back( meshIndex );
	page->dirty = true;
	return meshIndex;
}

void Renderer::RemoveMesh( int meshIndex )
{
	Mesh& mesh = m_meshes[meshIndex];
	if ( mesh.alive == false )
	{
		return;
	}

	Page* page = m_pages[mesh.page];
	for ( size_t i = 0; i < page->meshes.size(); ++i )
	{
		if ( page->meshes[i] == meshIndex )
		{
			page->meshes[i] = page->meshes.back();
			page->meshes.pop_back();
			break;
		}
	}
	page->dirty = true;
	page->needsRebuild = true;

	mesh.alive = false;
	mesh.vertices.clear();
	mesh.vertices.shrink_to_fit();
	m_freeMeshes.push_back( meshIndex );
}

void Renderer::RebuildPage( int pageIndex )
{
	Page* page = m_pages[pageIndex];
	page->vertices.clear();
	for ( int meshIndex : page->meshes )
	{
		Mesh& mesh = m_meshes[meshIndex];
		mesh.first = (int)page->vertices.size();
		page->vertices.insert( page->vertices.end(), mesh.vertices.begin(), mesh.vertices.end() );
	}
	page->needsRebuild = false;
}

void Renderer::Upload()
{
	m_stats.uploadedBytes = 0;
	for ( int i = 0; i < (int)m_pages.size(); ++i )
	{
		Page* page = m_pages[i];
		if ( page->dirty == false )
		{
			continue;
		}

		if ( page->needsRebuild )
		{
			RebuildPage( i );
		}

		if ( page->vertices.empty() == false )
		{
			size_t bytes = page->vertices.size() * sizeof( GpuVertex );
			sg_update_buffer( sg_buffer{ page->buffer }, MakeRange( page->vertices.data(), bytes ) );
			m_stats.uploadedBytes += (int)bytes;
		}
		page->uploadedCount = (int)page->vertices.size();
		page->dirty = false;
	}

	if ( m_slotsDirty && m_slotCount > 0 )
	{
		size_t bytes = (size_t)m_slotCount * SlotFloats * sizeof( float );
		sg_update_buffer( sg_buffer{ m_transformBuffer }, MakeRange( m_slotData.data(), bytes ) );
		m_stats.uploadedBytes += (int)bytes;
		m_slotsDirty = false;
	}
}

static Vec4 MakeVec4( b3Vec3 v, float w )
{
	return Vec4{ v.x, v.y, v.z, w };
}

void Renderer::Render( const Camera& camera, const RenderSettings& settings, int width, int height )
{
	Upload();

	m_stats.drawCalls = 0;
	m_stats.vertexCount = 0;
	m_stats.pageCount = (int)m_pages.size();
	m_stats.slotCount = m_slotCount - (int)m_freeSlots.size();

	b3Vec3 sun = b3Normalize( settings.sunDirection );

	// Directional light fitted around the scene
	float radius = settings.sceneRadius;
	b3Vec3 lightEye = b3MulAdd( settings.sceneCenter, 2.0f * radius, sun );
	b3Vec3 lightUp = fabsf( sun.y ) > 0.95f ? b3Vec3{ 1.0f, 0.0f, 0.0f } : b3Vec3{ 0.0f, 1.0f, 0.0f };
	Mat4 lightView = Mat4LookAt( lightEye, settings.sceneCenter, lightUp );
	Mat4 lightProj = Mat4Ortho( -radius, radius, -radius, radius, 0.1f, 4.0f * radius, m_zeroToOne );
	Mat4 lightViewProj = Mat4Mul( lightProj, lightView );

	float aspect = height > 0 ? (float)width / (float)height : 1.0f;
	b3Vec3 forward = camera.Forward();
	Mat4 view = Mat4LookAt( camera.position, b3Add( camera.position, forward ), b3Vec3{ 0.0f, 1.0f, 0.0f } );
	Mat4 projection = Mat4Perspective( camera.fovY, aspect, 0.05f, 400.0f, m_zeroToOne );
	Mat4 viewProj = Mat4Mul( projection, view );

	// Shadow pass
	{
		sg_pass pass = {};
		pass.action.depth.load_action = SG_LOADACTION_CLEAR;
		pass.action.depth.store_action = SG_STOREACTION_STORE;
		pass.action.depth.clear_value = 1.0f;
		pass.attachments.depth_stencil = sg_view{ m_shadowAttachment };
		pass.label = "shadow_pass";
		sg_begin_pass( &pass );

		if ( settings.shadows )
		{
			sg_apply_pipeline( sg_pipeline{ m_shadowPipeline } );
			scene_shadow_params_vs_t params = {};
			params.shadow_view_proj = lightViewProj;
			sg_apply_uniforms( UB_scene_shadow_params_vs, MakeRange( &params, sizeof( params ) ) );

			for ( Page* page : m_pages )
			{
				if ( page->uploadedCount == 0 )
				{
					continue;
				}

				sg_bindings bindings = {};
				bindings.vertex_buffers[0] = sg_buffer{ page->buffer };
				bindings.views[VIEW_scene_transforms] = sg_view{ m_transformView };
				sg_apply_bindings( &bindings );
				sg_draw( 0, page->uploadedCount, 1 );
				m_stats.drawCalls += 1;
			}
		}
		sg_end_pass();
	}

	// Main pass
	sg_pass pass = {};
	pass.action.colors[0].load_action = SG_LOADACTION_DONTCARE;
	pass.action.depth.load_action = SG_LOADACTION_CLEAR;
	pass.action.depth.clear_value = 1.0f;
	pass.swapchain = sglue_swapchain();
	pass.label = "main_pass";
	sg_begin_pass( &pass );

	b3Vec3 zenith = { 0.12f, 0.26f, 0.58f };
	b3Vec3 horizon = { 0.55f, 0.64f, 0.74f };
	b3Vec3 groundColor = { 0.24f, 0.21f, 0.18f };

	{
		float tanY = tanf( 0.5f * camera.fovY );
		scene_sky_vs_params_t vsParams = {};
		vsParams.camera_forward = MakeVec4( forward, tanY * aspect );
		vsParams.camera_right = MakeVec4( camera.Right(), tanY );
		vsParams.camera_up = MakeVec4( camera.Up(), 0.0f );

		scene_sky_fs_params_t fsParams = {};
		fsParams.sky_sun_dir = MakeVec4( sun, 0.0f );
		fsParams.sky_zenith = MakeVec4( zenith, 0.0f );
		fsParams.sky_horizon = MakeVec4( horizon, 0.0f );
		fsParams.sky_ground = MakeVec4( groundColor, 0.0f );

		sg_apply_pipeline( sg_pipeline{ m_skyPipeline } );
		sg_apply_uniforms( UB_scene_sky_vs_params, MakeRange( &vsParams, sizeof( vsParams ) ) );
		sg_apply_uniforms( UB_scene_sky_fs_params, MakeRange( &fsParams, sizeof( fsParams ) ) );
		sg_draw( 0, 3, 1 );
		m_stats.drawCalls += 1;
	}

	scene_vs_params_t vsParams = {};
	vsParams.view_proj = viewProj;
	vsParams.light_view_proj = lightViewProj;

	scene_fs_params_t fsParams = {};
	fsParams.sun_dir = MakeVec4( sun, 0.0f );
	fsParams.sun_color = Vec4{ 1.75f, 1.62f, 1.42f, 0.42f };
	fsParams.sky_color = Vec4{ 0.5f, 0.62f, 0.82f, 0.0035f };
	fsParams.ground_color = MakeVec4( groundColor, 0.0f );
	fsParams.camera_pos = MakeVec4( camera.position, 0.0f );
	fsParams.fog_color = MakeVec4( horizon, 0.0f );
	fsParams.shadow_params =
		Vec4{ m_uvYSign, 1.0f / (float)ShadowResolution, m_zeroToOne ? 1.0f : 0.5f, m_zeroToOne ? 0.0f : 0.5f };
	fsParams.options = Vec4{ settings.showChunks ? 1.0f : 0.0f, settings.shadows ? 1.0f : 0.0f, settings.showLoad ? 1.0f : 0.0f, 0.0f };

	sg_apply_pipeline( sg_pipeline{ m_litPipeline } );
	sg_apply_uniforms( UB_scene_vs_params, MakeRange( &vsParams, sizeof( vsParams ) ) );
	sg_apply_uniforms( UB_scene_fs_params, MakeRange( &fsParams, sizeof( fsParams ) ) );

	for ( Page* page : m_pages )
	{
		if ( page->uploadedCount == 0 )
		{
			continue;
		}

		sg_bindings bindings = {};
		bindings.vertex_buffers[0] = sg_buffer{ page->buffer };
		bindings.views[VIEW_scene_transforms] = sg_view{ m_transformView };
		bindings.views[VIEW_scene_shadow_map] = sg_view{ m_shadowTexture };
		bindings.samplers[SMP_scene_shadow_sampler] = sg_sampler{ m_shadowSampler };
		sg_apply_bindings( &bindings );
		sg_draw( 0, page->uploadedCount, 1 );
		m_stats.drawCalls += 1;
		m_stats.vertexCount += page->uploadedCount;
	}

	// Dust and chips on top, without depth writes
	m_stats.particleCount = (int)m_particles.size();
	if ( m_particles.empty() == false )
	{
		sg_update_buffer( sg_buffer{ m_instanceBuffer }, MakeRange( m_particles.data(), m_particles.size() * sizeof( ParticleInstance ) ) );

		scene_particle_vs_params_t particleVs = {};
		particleVs.particle_view_proj = viewProj;
		particleVs.particle_right = MakeVec4( camera.Right(), 0.0f );
		particleVs.particle_up = MakeVec4( camera.Up(), 0.0f );

		// Dust is lit from all sides: the sky plus a good part of the sun
		scene_particle_fs_params_t particleFs = {};
		particleFs.particle_light = Vec4{ 0.5f * 0.42f + 0.6f * 1.75f, 0.62f * 0.42f + 0.6f * 1.62f, 0.82f * 0.42f + 0.6f * 1.42f, 0.0f };
		particleFs.particle_camera = MakeVec4( camera.position, fsParams.sky_color.w );
		particleFs.particle_fog = fsParams.fog_color;

		sg_apply_pipeline( sg_pipeline{ m_particlePipeline } );
		sg_apply_uniforms( UB_scene_particle_vs_params, MakeRange( &particleVs, sizeof( particleVs ) ) );
		sg_apply_uniforms( UB_scene_particle_fs_params, MakeRange( &particleFs, sizeof( particleFs ) ) );

		sg_bindings bindings = {};
		bindings.vertex_buffers[0] = sg_buffer{ m_cornerBuffer };
		bindings.vertex_buffers[1] = sg_buffer{ m_instanceBuffer };
		sg_apply_bindings( &bindings );
		sg_draw( 0, 6, (int)m_particles.size() );
		m_stats.drawCalls += 1;
	}

	// The pass stays open for the user interface, see EndFrame in the demo
}
