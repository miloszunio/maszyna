#version 460 core
#define STAGE_VERTEX 1
// grass of the heightmap terrain (opengl46_grass): the parameters of the draw; the plants are made in grass_plant.glsl

#include "common.glsl"

// x, z: corner of the tile in the chunk, metres; y: side of the tile; w: blades of the tile where its densest kind is painted fully
layout(location = 0) uniform vec4 grass_tile;
// x: the share of the densest kind at the densest place of the tile (the blades of the tile are counted for it); y: segments
// of a blade; z, w: number of the tile in the world
layout(location = 1) uniform vec4 grass_part;
// x: side of the chunk, metres; y: cells of its grid along a side; z: grass samples along a side less one
layout(location = 2) uniform vec4 grass_chunk;
// the camera relative to the corner of the chunk (y as it is)
layout(location = 3) uniform vec4 grass_camera;
// x: distance the grass starts to thin out at, for speed; y: distance the fading starts at (gfx.grass.distance); z: where the
// grass ends (gfx.grass.fadedistance)
layout(location = 4) uniform vec4 grass_lod;
// x, y: direction of the wind (x, z of the world); z: its strength, 0..1; w: time, seconds
layout(location = 5) uniform vec4 grass_wind;
// the kinds of the chunk: colour at the root and at the tip (linear rgb), and height, its variation, width, lean
layout(location = 6) uniform vec4 grass_base[4];
layout(location = 10) uniform vec4 grass_tip[4];
layout(location = 14) uniform vec4 grass_shape[4];
// share of the blades of each kind against the densest one of the chunk
layout(location = 18) uniform vec4 grass_share;
// the colour of each kind dried out (linear rgb), and how much it follows the seasons
layout(location = 19) uniform vec4 grass_dry[4];
// the time of the year: x: how dry the grass is, y: how fresh (spring), z: how grown (share of the height)
layout(location = 23) uniform vec4 grass_season;
// the plants of the kinds: colour of the head (linear rgb) and the archetype; length, width, droop and awns of the head;
// translucency, sheen; the distances of the kind (as grass_lod) and the power of the share its blades grow wider by
layout(location = 24) uniform vec4 grass_head[4];
layout(location = 28) uniform vec4 grass_headshape[4];
layout(location = 32) uniform vec4 grass_light[4];
layout(location = 36) uniform vec4 grass_lods[4];

layout(binding = 0) uniform sampler2D grass_heights; // heights of the grid points, R32F
layout(binding = 1) uniform sampler2D grass_density; // densities of the kinds, RGBA8
layout(binding = 2) uniform sampler2D grass_holes; // holes cut in the ground, R8

// the kinds are uniforms here
void read_kinds()
{
}

#define VERTEX_ID gl_VertexID
#define INSTANCE_ID gl_InstanceID

#include "grass_plant.glsl"
