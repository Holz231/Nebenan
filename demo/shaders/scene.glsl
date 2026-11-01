// SPDX-License-Identifier: MIT
// Scene shader of the Nebenan demo. Compile with sokol-shdc:
//   sokol-shdc --input scene.glsl --output generated/scene.glsl.h --slang hlsl5:metal_macos:glsl430
//
// Every chunk, ball, figure and the ground are drawn from shared vertex pages. Each vertex carries the index of
// a transform slot. The slot transforms of all bodies live in one storage buffer that is uploaded once per
// frame, so the whole scene takes one draw call per vertex page. Every vertex is lit in the vertex shader, and
// its color runs across the triangle: a flat face has its own vertices with its normal and stays one color, a
// round body with smooth normals shades round. The pixel shader only writes the color out, the least work per
// pixel there is.

#pragma sokol @module scene

#pragma sokol @ctype mat4 Mat4
#pragma sokol @ctype vec4 Vec4

#pragma sokol @vs vs

struct slot_transform
{
	// .xyz = position, .w = one for a visible slot, zero for a hidden one
	vec4 position_visible;
	// quaternion x, y, z, w
	vec4 rotation;
};

layout( binding = 0 ) readonly buffer transforms
{
	slot_transform slots[];
};

layout( binding = 0 ) uniform vs_params
{
	mat4 view_proj;
	vec4 sun_dir; // .xyz = direction toward the sun
};

in vec3 in_position;
in vec4 in_normal; // .xyz = local normal, .w = material / 127
in float in_slot;

out vec3 v_color;

vec3 rotate_vector( vec4 q, vec3 v )
{
	vec3 t = 2.0 * cross( q.xyz, v );
	return v + q.w * t + cross( q.xyz, t );
}

vec3 material_color( int material )
{
	if ( material == 0 )
	{
		return vec3( 0.62, 0.27, 0.18 ); // brick
	}
	if ( material == 1 )
	{
		return vec3( 0.74, 0.42, 0.3 ); // broken brick
	}
	if ( material == 2 )
	{
		return vec3( 0.63, 0.63, 0.61 ); // concrete
	}
	if ( material == 3 )
	{
		return vec3( 0.5, 0.48, 0.45 ); // broken concrete
	}
	if ( material == 4 )
	{
		return vec3( 0.86, 0.83, 0.77 ); // plaster
	}
	if ( material == 8 )
	{
		return vec3( 0.36, 0.37, 0.36 ); // ground
	}
	if ( material == 9 )
	{
		return vec3( 0.2, 0.2, 0.22 ); // steel ball
	}
	// The colors of the figures, see AppendFigurePart in demo.cpp
	if ( material == 10 )
	{
		return vec3( 0.97, 0.85, 0.62 ); // skin
	}
	if ( material == 11 )
	{
		return vec3( 0.6, 0.48, 0.4 ); // brown
	}
	if ( material == 12 )
	{
		return vec3( 0.84, 0.2, 0.17 ); // red
	}
	if ( material == 13 )
	{
		return vec3( 0.93, 0.44, 0.16 ); // orange
	}
	if ( material == 14 )
	{
		return vec3( 0.96, 0.76, 0.2 ); // yellow
	}
	if ( material == 15 )
	{
		return vec3( 0.85, 0.3, 0.55 ); // pink
	}
	if ( material == 16 )
	{
		return vec3( 0.2, 0.6, 0.3 ); // green
	}
	if ( material == 17 )
	{
		return vec3( 0.16, 0.62, 0.62 ); // teal
	}
	if ( material == 18 )
	{
		return vec3( 0.62, 0.82, 0.32 ); // lime
	}
	if ( material == 19 )
	{
		return vec3( 0.22, 0.47, 0.84 ); // blue
	}
	if ( material == 20 )
	{
		return vec3( 0.54, 0.32, 0.78 ); // violet
	}
	if ( material == 21 )
	{
		return vec3( 0.3, 0.27, 0.33 ); // dark
	}
	return vec3( 0.7 );
}

void main()
{
	slot_transform xf = slots[int( in_slot + 0.5 )];

	// A hidden slot collapses its triangles into a point
	vec3 world_pos = rotate_vector( xf.rotation, in_position * xf.position_visible.w ) + xf.position_visible.xyz;
	vec3 n = rotate_vector( xf.rotation, in_normal.xyz );

	// Sky from above, a little bounce from below, and the sun
	float light = 0.42 + 0.12 * n.y + 0.55 * max( dot( n, sun_dir.xyz ), 0.0 );
	v_color = material_color( int( in_normal.w * 127.0 + 0.5 ) ) * light;
	gl_Position = view_proj * vec4( world_pos, 1.0 );
}
#pragma sokol @end

#pragma sokol @fs fs

in vec3 v_color;
out vec4 frag_color;

void main()
{
	frag_color = vec4( v_color, 1.0 );
}
#pragma sokol @end

#pragma sokol @program scene vs fs
