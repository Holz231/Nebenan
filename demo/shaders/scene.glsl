// SPDX-License-Identifier: MIT
// Scene shaders for the Nebenan demo. Compile with sokol-shdc:
//   sokol-shdc --input scene.glsl --output generated/scene.glsl.h --slang hlsl5:metal_macos:glsl430
//
// Every chunk, ball and the ground are drawn from shared vertex pages. Each vertex carries the index
// of a transform slot. The slot transforms of all bodies live in one storage buffer that is uploaded
// once per frame, so a whole destructible scene takes one draw call per vertex page.

#pragma sokol @module scene

#pragma sokol @ctype mat4 Mat4
#pragma sokol @ctype vec4 Vec4

#pragma sokol @block slot_transform
struct slot_transform
{
	// .xyz = position, .w = uniform scale (zero hides the slot)
	vec4 position_scale;
	// quaternion x, y, z, w
	vec4 rotation;
	// .x = load utilization of the chunk, negative for none
	vec4 extra;
};

layout( binding = 0 ) readonly buffer transforms
{
	slot_transform slots[];
};

vec3 rotate_vector( vec4 q, vec3 v )
{
	vec3 t = 2.0 * cross( q.xyz, v );
	return v + q.w * t + cross( q.xyz, t );
}
#pragma sokol @end

//----------------------------------------------------------------------------------------------------------------------
// Lit geometry

#pragma sokol @vs vs
#pragma sokol @include_block slot_transform

layout( binding = 0 ) uniform vs_params
{
	mat4 view_proj;
	mat4 light_view_proj;
};

in vec3 in_position;
in vec4 in_normal; // .xyz = local normal, .w = material / 127
in float in_slot;

out vec3 v_world_pos;
out vec3 v_world_normal;
out vec3 v_local_pos;
out vec3 v_local_normal;
out vec4 v_light_pos;
flat out float v_material;
flat out float v_variation;
flat out float v_load;

void main()
{
	int slot = int( in_slot + 0.5 );
	slot_transform xf = slots[slot];
	vec3 world_pos = rotate_vector( xf.rotation, in_position * xf.position_scale.w ) + xf.position_scale.xyz;

	v_load = xf.extra.x;
	v_world_pos = world_pos;
	v_world_normal = rotate_vector( xf.rotation, in_normal.xyz );
	v_local_pos = in_position;
	v_local_normal = in_normal.xyz;
	v_light_pos = light_view_proj * vec4( world_pos, 1.0 );
	v_material = floor( in_normal.w * 127.0 + 0.5 );
	v_variation = fract( sin( float( slot ) * 12.9898 + 78.233 ) * 43758.5453 );
	gl_Position = view_proj * vec4( world_pos, 1.0 );
}
#pragma sokol @end

#pragma sokol @fs fs

layout( binding = 1 ) uniform fs_params
{
	vec4 sun_dir;		// .xyz = direction toward the sun
	vec4 sun_color;		// .rgb = sun radiance, .a = ambient strength
	vec4 sky_color;		// .rgb = sky ambient, .a = fog density
	vec4 ground_color;	// .rgb = ground bounce
	vec4 camera_pos;	// .xyz = eye
	vec4 fog_color;		// .rgb
	vec4 shadow_params; // .x = uv y sign, .y = texel size, .z = depth scale, .w = depth offset
	vec4 options;		// .x = show chunks, .y = shadows enabled, .z = load view
};

#pragma sokol @image_sample_type shadow_map depth
layout( binding = 1 ) uniform texture2D shadow_map;
#pragma sokol @sampler_type shadow_sampler comparison
layout( binding = 0 ) uniform samplerShadow shadow_sampler;

in vec3 v_world_pos;
in vec3 v_world_normal;
in vec3 v_local_pos;
in vec3 v_local_normal;
in vec4 v_light_pos;
flat in float v_material;
flat in float v_variation;
flat in float v_load;

out vec4 frag_color;

float hash12( vec2 p )
{
	vec3 p3 = fract( vec3( p.xyx ) * 0.1031 );
	p3 += dot( p3, p3.yzx + 33.33 );
	return fract( ( p3.x + p3.y ) * p3.z );
}

float hash13( vec3 p3 )
{
	p3 = fract( p3 * 0.1031 );
	p3 += dot( p3, p3.zyx + 31.32 );
	return fract( ( p3.x + p3.y ) * p3.z );
}

float value_noise( vec3 p )
{
	vec3 i = floor( p );
	vec3 f = fract( p );
	f = f * f * ( 3.0 - 2.0 * f );
	float n000 = hash13( i );
	float n100 = hash13( i + vec3( 1.0, 0.0, 0.0 ) );
	float n010 = hash13( i + vec3( 0.0, 1.0, 0.0 ) );
	float n110 = hash13( i + vec3( 1.0, 1.0, 0.0 ) );
	float n001 = hash13( i + vec3( 0.0, 0.0, 1.0 ) );
	float n101 = hash13( i + vec3( 1.0, 0.0, 1.0 ) );
	float n011 = hash13( i + vec3( 0.0, 1.0, 1.0 ) );
	float n111 = hash13( i + vec3( 1.0, 1.0, 1.0 ) );
	float x00 = mix( n000, n100, f.x );
	float x10 = mix( n010, n110, f.x );
	float x01 = mix( n001, n101, f.x );
	float x11 = mix( n011, n111, f.x );
	return mix( mix( x00, x10, f.y ), mix( x01, x11, f.y ), f.z );
}

float fbm( vec3 p )
{
	float sum = 0.5 * value_noise( p );
	sum += 0.25 * value_noise( p * 2.03 );
	sum += 0.125 * value_noise( p * 4.11 );
	return sum / 0.875;
}

// Planar projection on the dominant axis of the local normal. The destructible frame is shared by all
// of its chunks, so the surface texture stays continuous across fracture lines.
vec2 box_uv( vec3 p, vec3 n )
{
	vec3 a = abs( n );
	if ( a.x > a.y && a.x > a.z )
	{
		return p.zy;
	}
	if ( a.y > a.z )
	{
		return p.xz;
	}
	return p.xy;
}

float line_mask( float coordinate, float width )
{
	float w = max( fwidth( coordinate ), 1.0e-4 );
	float d = min( fract( coordinate ), 1.0 - fract( coordinate ) );
	return 1.0 - smoothstep( width - w, width + w, d );
}

vec3 brick_surface( vec3 p, vec3 n, out float roughness )
{
	vec2 uv = box_uv( p, n );
	vec2 size = vec2( 0.25, 0.075 );
	float row = floor( uv.y / size.y );
	float shift = mod( row, 2.0 ) * 0.5;
	vec2 cell = vec2( floor( uv.x / size.x + shift ), row );
	float mortar = max( line_mask( uv.x / size.x + shift, 0.045 ), line_mask( uv.y / size.y, 0.11 ) );

	float h = hash12( cell );
	vec3 brick = mix( vec3( 0.3, 0.075, 0.045 ), vec3( 0.52, 0.19, 0.09 ), h );
	brick *= 0.78 + 0.4 * value_noise( p * 38.0 );
	vec3 joint = vec3( 0.46, 0.44, 0.4 ) * ( 0.85 + 0.3 * value_noise( p * 60.0 ) );
	roughness = 0.9;
	return mix( brick, joint, mortar );
}

vec3 brick_interior( vec3 p )
{
	// Broken brick with crumbled mortar pockets
	float n = fbm( p * 14.0 );
	vec3 brick = mix( vec3( 0.42, 0.13, 0.06 ), vec3( 0.62, 0.27, 0.13 ), value_noise( p * 30.0 ) );
	vec3 joint = vec3( 0.5, 0.47, 0.42 );
	return mix( brick, joint, smoothstep( 0.62, 0.7, n ) ) * ( 0.85 + 0.25 * v_variation );
}

vec3 concrete_surface( vec3 p, vec3 n, out float roughness )
{
	vec2 uv = box_uv( p, n );
	float stains = fbm( p * 1.7 );
	vec3 color = mix( vec3( 0.36, 0.36, 0.35 ), vec3( 0.52, 0.51, 0.48 ), stains );
	color *= 0.92 + 0.12 * value_noise( p * 45.0 );

	// Formwork joints and tie holes
	float joints = max( line_mask( uv.y / 0.6, 0.006 ), line_mask( uv.x / 1.2, 0.004 ) );
	vec2 hole = fract( uv / vec2( 0.6, 0.6 ) ) - 0.5;
	float tie = 1.0 - smoothstep( 0.012, 0.02, length( hole * vec2( 0.6, 0.6 ) ) );
	roughness = 0.75;
	return color * ( 1.0 - 0.35 * joints - 0.4 * tie );
}

vec3 concrete_interior( vec3 p )
{
	// Aggregate in a cement matrix
	float grain = value_noise( p * 70.0 );
	float stones = smoothstep( 0.58, 0.64, value_noise( p * 22.0 + 7.0 ) );
	vec3 cement = vec3( 0.44, 0.43, 0.4 ) * ( 0.85 + 0.3 * grain );
	vec3 stone = mix( vec3( 0.28, 0.27, 0.25 ), vec3( 0.55, 0.49, 0.41 ), value_noise( p * 9.0 ) );
	return mix( cement, stone, stones ) * ( 0.88 + 0.2 * v_variation );
}

vec3 plaster_surface( vec3 p, out float roughness )
{
	float n = fbm( p * 3.0 );
	roughness = 0.85;
	return vec3( 0.6, 0.56, 0.49 ) * ( 0.9 + 0.12 * n ) * ( 0.96 + 0.08 * value_noise( p * 80.0 ) );
}

vec3 ground_surface( vec3 p, out float roughness )
{
	vec2 uv = p.xz;
	float tiles = max( line_mask( uv.x / 2.0, 0.006 ), line_mask( uv.y / 2.0, 0.006 ) );
	vec2 cell = floor( uv / 2.0 );
	vec3 color = vec3( 0.2, 0.205, 0.2 ) * ( 0.9 + 0.15 * hash12( cell ) );
	color *= 0.85 + 0.25 * fbm( vec3( uv * 0.8, 0.0 ) );
	roughness = 0.95;
	return color * ( 1.0 - 0.45 * tiles );
}

float sample_shadow( vec3 normal )
{
	if ( options.y < 0.5 )
	{
		return 1.0;
	}

	vec3 ndc = v_light_pos.xyz / v_light_pos.w;
	vec2 uv = vec2( ndc.x * 0.5 + 0.5, ndc.y * shadow_params.x * 0.5 + 0.5 );
	float depth = ndc.z * shadow_params.z + shadow_params.w;
	if ( uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth > 1.0 )
	{
		return 1.0;
	}

	// Slope scaled bias against acne on faces at grazing sun angles
	float n_dot_l = clamp( dot( normal, sun_dir.xyz ), 0.0, 1.0 );
	float bias = 0.0006 + 0.0025 * sqrt( 1.0 - n_dot_l * n_dot_l ) / max( n_dot_l, 0.2 );
	float texel = shadow_params.y;
	float sum = 0.0;
	for ( int y = -1; y <= 1; ++y )
	{
		for ( int x = -1; x <= 1; ++x )
		{
			vec2 offset = vec2( float( x ), float( y ) ) * texel * 1.25;
			sum += texture( sampler2DShadow( shadow_map, shadow_sampler ), vec3( uv + offset, depth - bias ) );
		}
	}
	return sum / 9.0;
}

void main()
{
	vec3 n = normalize( v_world_normal );
	vec3 local_n = normalize( v_local_normal );
	int material = int( v_material );

	float roughness = 0.9;
	float specular = 0.0;
	vec3 albedo;
	if ( material == 0 )
	{
		albedo = brick_surface( v_local_pos, local_n, roughness );
	}
	else if ( material == 1 )
	{
		albedo = brick_interior( v_local_pos );
	}
	else if ( material == 2 )
	{
		albedo = concrete_surface( v_local_pos, local_n, roughness );
		specular = 0.04;
	}
	else if ( material == 3 )
	{
		albedo = concrete_interior( v_local_pos );
	}
	else if ( material == 4 )
	{
		albedo = plaster_surface( v_local_pos, roughness );
		specular = 0.03;
	}
	else if ( material == 8 )
	{
		albedo = ground_surface( v_world_pos, roughness );
	}
	else if ( material == 9 )
	{
		// Steel ball
		albedo = vec3( 0.12, 0.12, 0.13 );
		roughness = 0.3;
		specular = 0.6;
	}
	else
	{
		albedo = vec3( 0.7 );
	}

	if ( options.x > 0.5 && material < 8 )
	{
		// Debug view: every chunk in its own color
		albedo = 0.35 + 0.55 * vec3( fract( v_variation * 7.13 ), fract( v_variation * 3.71 ), fract( v_variation * 5.29 ) );
	}

	if ( options.z > 0.5 && material < 8 )
	{
		// Load view: green where the structure is relaxed, yellow, red where bonds are about to break, grey debris
		if ( v_load < 0.0 )
		{
			albedo = vec3( 0.4 );
		}
		else
		{
			float t = clamp( v_load, 0.0, 1.0 );
			vec3 relaxed = vec3( 0.16, 0.6, 0.27 );
			vec3 loaded = vec3( 0.95, 0.78, 0.14 );
			vec3 critical = vec3( 0.9, 0.1, 0.07 );
			albedo = t < 0.5 ? mix( relaxed, loaded, 2.0 * t ) : mix( loaded, critical, 2.0 * t - 1.0 );
		}
		roughness = 0.85;
		specular = 0.05;
	}

	vec3 l = normalize( sun_dir.xyz );
	float n_dot_l = max( dot( n, l ), 0.0 );
	float shadow = n_dot_l > 0.0 ? sample_shadow( n ) : 0.0;

	vec3 ambient = mix( ground_color.rgb, sky_color.rgb, 0.5 + 0.5 * n.y ) * sun_color.a;
	vec3 color = albedo * ( ambient + sun_color.rgb * n_dot_l * shadow );

	vec3 v = normalize( camera_pos.xyz - v_world_pos );
	vec3 h = normalize( v + l );
	float shininess = mix( 256.0, 8.0, roughness );
	color += sun_color.rgb * specular * pow( max( dot( n, h ), 0.0 ), shininess ) * shadow * n_dot_l;

	float distance = length( camera_pos.xyz - v_world_pos );
	float fog = 1.0 - exp( -distance * sky_color.a );
	color = mix( color, fog_color.rgb, fog );

	// Filmic curve and gamma
	color = color * ( 2.51 * color + 0.03 ) / ( color * ( 2.43 * color + 0.59 ) + 0.14 );
	frag_color = vec4( pow( clamp( color, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) ), 1.0 );
}
#pragma sokol @end

#pragma sokol @program lit vs fs

//----------------------------------------------------------------------------------------------------------------------
// Shadow caster, depth only

#pragma sokol @vs shadow_vs
#pragma sokol @include_block slot_transform

layout( binding = 0 ) uniform shadow_params_vs
{
	mat4 shadow_view_proj;
};

in vec3 in_position;
in float in_slot;

void main()
{
	int slot = int( in_slot + 0.5 );
	slot_transform xf = slots[slot];
	vec3 world_pos = rotate_vector( xf.rotation, in_position * xf.position_scale.w ) + xf.position_scale.xyz;
	gl_Position = shadow_view_proj * vec4( world_pos, 1.0 );
}
#pragma sokol @end

#pragma sokol @fs shadow_fs
void main()
{
}
#pragma sokol @end

#pragma sokol @program shadow shadow_vs shadow_fs

//----------------------------------------------------------------------------------------------------------------------
// Sky, one fullscreen triangle behind everything

#pragma sokol @vs sky_vs

layout( binding = 0 ) uniform sky_vs_params
{
	vec4 camera_forward; // .w = tan(fov_y / 2) * aspect
	vec4 camera_right;	 // .w = tan(fov_y / 2)
	vec4 camera_up;
};

out vec3 v_direction;

void main()
{
	vec2 ndc = vec2( float( ( gl_VertexIndex << 1 ) & 2 ), float( gl_VertexIndex & 2 ) ) * 2.0 - 1.0;
	v_direction = camera_forward.xyz + ndc.x * camera_forward.w * camera_right.xyz + ndc.y * camera_right.w * camera_up.xyz;
	gl_Position = vec4( ndc, 0.0, 1.0 );
}
#pragma sokol @end

#pragma sokol @fs sky_fs

layout( binding = 1 ) uniform sky_fs_params
{
	vec4 sky_sun_dir;
	vec4 sky_zenith;
	vec4 sky_horizon;
	vec4 sky_ground;
};

in vec3 v_direction;
out vec4 sky_color_out;

void main()
{
	vec3 d = normalize( v_direction );
	float t = clamp( d.y, -1.0, 1.0 );
	vec3 color = t > 0.0 ? mix( sky_horizon.rgb, sky_zenith.rgb, pow( t, 0.45 ) ) : mix( sky_horizon.rgb, sky_ground.rgb, pow( -t, 0.35 ) );

	float sun = max( dot( d, normalize( sky_sun_dir.xyz ) ), 0.0 );
	color += vec3( 1.0, 0.85, 0.6 ) * ( pow( sun, 900.0 ) * 6.0 + pow( sun, 12.0 ) * 0.25 );

	color = color * ( 2.51 * color + 0.03 ) / ( color * ( 2.43 * color + 0.59 ) + 0.14 );
	sky_color_out = vec4( pow( clamp( color, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) ), 1.0 );
}
#pragma sokol @end

#pragma sokol @program sky sky_vs sky_fs

//----------------------------------------------------------------------------------------------------------------------
// Dust and chips: camera facing quads, one instance per particle, premultiplied alpha

#pragma sokol @vs particle_vs

layout( binding = 0 ) uniform particle_vs_params
{
	mat4 particle_view_proj;
	vec4 particle_right; // .xyz = camera right
	vec4 particle_up;	 // .xyz = camera up
};

in vec2 in_corner;		// per vertex: quad corner in [-1, 1]
in vec4 in_center_size; // per instance: .xyz = center, .w = half size, negative for a solid chip
in vec4 in_color_alpha; // per instance: .rgb = albedo, .a = opacity

out vec2 v_corner;
out vec4 v_particle_color;
out vec3 v_particle_pos;
flat out float v_chip;

void main()
{
	float size = abs( in_center_size.w );
	vec3 position = in_center_size.xyz + ( particle_right.xyz * in_corner.x + particle_up.xyz * in_corner.y ) * size;
	v_corner = in_corner;
	v_particle_color = in_color_alpha;
	v_particle_pos = position;
	v_chip = in_center_size.w < 0.0 ? 1.0 : 0.0;
	gl_Position = particle_view_proj * vec4( position, 1.0 );
}
#pragma sokol @end

#pragma sokol @fs particle_fs

layout( binding = 1 ) uniform particle_fs_params
{
	vec4 particle_light;  // .rgb = sun and sky light on a dust cloud
	vec4 particle_camera; // .xyz = eye, .w = fog density
	vec4 particle_fog;	  // .rgb = fog color
};

in vec2 v_corner;
in vec4 v_particle_color;
in vec3 v_particle_pos;
flat in float v_chip;

out vec4 particle_color_out;

void main()
{
	// Chips are small solid diamonds, dust puffs are soft discs
	float alpha;
	if ( v_chip > 0.5 )
	{
		if ( abs( v_corner.x ) + abs( v_corner.y ) > 1.0 )
		{
			discard;
		}
		alpha = v_particle_color.a;
	}
	else
	{
		float r2 = dot( v_corner, v_corner );
		if ( r2 > 1.0 )
		{
			discard;
		}
		alpha = v_particle_color.a * ( 1.0 - r2 ) * ( 1.0 - r2 );
	}

	vec3 color = v_particle_color.rgb * particle_light.rgb;
	float distance = length( particle_camera.xyz - v_particle_pos );
	float fog = 1.0 - exp( -distance * particle_camera.w );
	color = mix( color, particle_fog.rgb, fog );

	// Same filmic curve and gamma as the lit geometry
	color = color * ( 2.51 * color + 0.03 ) / ( color * ( 2.43 * color + 0.59 ) + 0.14 );
	color = pow( clamp( color, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) );
	particle_color_out = vec4( color * alpha, alpha );
}
#pragma sokol @end

#pragma sokol @program particle particle_vs particle_fs
