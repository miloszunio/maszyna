#version 460 core
#define STAGE_VERTEX 1
// grass of the heightmap terrain (vk14::grass_field): the parameters of the draw; the plants are made in grass_plant.glsl

#include "common.glsl"

// the parameters of the draw (the uniforms of the opengl renderer): those of the tile and of the frame in the push constants,
// the kinds of the chunk in a table (vk14::grass_field)
// x, z: corner of the tile in the chunk, metres; y: side of the tile; w: blades of the tile where its densest kind is painted fully
#define grass_tile pc.data[0]
// x: the share of the densest kind at the densest place of the tile (the blades of the tile are counted for it); y: segments
// of a blade; z, w: number of the tile in the world
#define grass_part pc.data[1]
// the camera relative to the corner of the chunk (y as it is)
#define grass_camera pc.data[2]
// x: distance the grass starts to thin out at, for speed; y: distance the fading starts at (gfx.grass.distance); z: where the
// grass ends (gfx.grass.fadedistance)
#define grass_lod pc.data[3]
// x, y: direction of the wind (x, z of the world); z: its strength, 0..1; w: time, seconds
#define grass_wind pc.data[4]
// the time of the year: x: how dry the grass is, y: how fresh (spring), z: how grown (share of the height)
#define grass_season pc.data[5]

layout(set = MATERIAL_SET, binding = MATERIAL_TEXTURE_FIRST + 0) uniform sampler2D grass_heights; // heights of the grid points, R32F
layout(set = MATERIAL_SET, binding = MATERIAL_TEXTURE_FIRST + 1) uniform sampler2D grass_density; // densities of the kinds, RGBA8
layout(set = MATERIAL_SET, binding = MATERIAL_TEXTURE_FIRST + 2) uniform sampler2D grass_holes; // holes cut in the ground, R8
// the table of the chunk, RGBA32F, 4 x 9: a column for each kind; the rows: colour at the root and at the tip (linear rgb),
// height, its variation, width, lean; the colour dried out (linear rgb) and how much it follows the seasons; in the fifth row
// the share of the blades of each kind against the densest one of the chunk (x) and the chunk (y: side, metres, cells of its
// grid along a side, grass samples along a side less one); then the plant: colour of its head (linear rgb) and its archetype;
// length, width, droop and awns of the head; translucency, sheen; the distances of the kind (as grass_lod) and the power of
// the share its blades grow wider by
layout(set = MATERIAL_SET, binding = MATERIAL_TEXTURE_FIRST + 3) uniform sampler2D grass_kinds;

vec4 grass_base[4];
vec4 grass_tip[4];
vec4 grass_shape[4];
vec4 grass_dry[4];
vec4 grass_head[4];
vec4 grass_headshape[4];
vec4 grass_light[4];
vec4 grass_lods[4];
vec4 grass_share;
vec4 grass_chunk;

void read_kinds()
{
	for (int i = 0; i < 4; ++i)
	{
		grass_base[i] = texelFetch(grass_kinds, ivec2(i, 0), 0);
		grass_tip[i] = texelFetch(grass_kinds, ivec2(i, 1), 0);
		grass_shape[i] = texelFetch(grass_kinds, ivec2(i, 2), 0);
		grass_dry[i] = texelFetch(grass_kinds, ivec2(i, 3), 0);
		grass_head[i] = texelFetch(grass_kinds, ivec2(i, 5), 0);
		grass_headshape[i] = texelFetch(grass_kinds, ivec2(i, 6), 0);
		grass_light[i] = texelFetch(grass_kinds, ivec2(i, 7), 0);
		grass_lods[i] = texelFetch(grass_kinds, ivec2(i, 8), 0);
	}
	grass_share = texelFetch(grass_kinds, ivec2(0, 4), 0);
	grass_chunk = texelFetch(grass_kinds, ivec2(1, 4), 0);
}

#define VERTEX_ID gl_VertexIndex
#define INSTANCE_ID gl_InstanceIndex

#include "grass_plant.glsl"
