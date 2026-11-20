#version 460 core
#define STAGE_VERTEX 1
// the surface of the drifts (vk14::snow_drifts, shaders/ogl46/snow_surface.vert and snow_surface.geom in one stage): a grid over the
// maps around the camera, a vertex in the middle of every texel, at the height of the surface of the snow there (snow.glsl). made
// of the numbers of the vertices, two triangles a square of four texels. the triangles over the places with too little snow and
// across the steps of the scenery (the eaves of a roof, the edge of a platform, a wall: the snow lies on a roof and on the ground
// beside the house, not on a sheet hung between them) are left out; the ground and the roofs keep the snow the shading puts on
// them there. the opengl renderer leaves them out in a geometry shader; here every vertex works out the three corners of its
// triangle (they're numbers, like its own) and the triangle left out is collapsed into a point outside the view. drawn with the
// camera of the pass at the origin; it sinks into the ground towards the edge of the maps

layout(location = 0) out vec3 f_normal;
layout(location = 3) out vec4 f_pos;
layout(location = 4) out vec4 f_clip_pos;
layout(location = 5) out vec4 f_clip_future_pos;
layout(location = 7) out vec4 UserData;
// x: the depth, y: the kind (snow.glsl), z: 1 where the vertex is to be drawn, w: the height of the surface of the snow over the
// camera, m
layout(location = 12) out vec4 f_snow;

#include "common.glsl"
#include "weather_common.glsl"
#include "snow.glsl"

// the parameters of the draw (the uniform of the opengl renderer) in the push constants. x: the texels across the grid (fewer
// than across the maps, which reach further than they're filled), y: the texels skipped between two vertices
#define snow_grid pc.data[0]

const int CORNERS[6] = int[](0, 1, 2, 2, 1, 3);
// m the surface may rise over a texel, with what the steepest drifts and slopes of the ground make
const float RISE_PER_METRE = 1.5;
const float RISE = 0.35;

float texel_size()
{
	return sky_occlusion_params.x / max(sky_occlusion_params.y, 1.0);
}

vec4 cell_at(ivec2 texel)
{
	int side = int(sky_occlusion_params.y + 0.5);
	ivec2 t = ((texel % side) + side) % side;
	return snow_cells[t.y * side + t.x];
}

// the height of the surface of the snow at a texel, or Fallback where there's none
float surface_at(ivec2 texel, float fallback)
{
	vec4 cell = cell_at(texel);
	return ((cell.x < SNOW_NONE) || (abs(cell.x - fallback) > 2.0)) ? fallback : cell.x;
}

// the texel of the maps under a corner (0..3) of a square of the grid
ivec2 texel_of(int quad, int corner, int across, int stride, ivec2 camera)
{
	ivec2 cellindex = ivec2(quad % (across - 1), quad / (across - 1)) + ivec2(corner & 1, corner >> 1);
	return camera + (cellindex - ivec2(across / 2)) * stride;
}

// the middle of a texel, x and z relative to the camera of the pass
vec2 place_of(ivec2 t)
{
	return (vec2(t) + 0.5) * texel_size() - sky_occlusion_origin.xy;
}

// the share of the depth of the snow drawn at a place: towards the edge of the maps the snow sinks into the ground
float edge_at(vec2 place)
{
	return 1.0 - smoothstep(sky_occlusion_params.z * 0.85, sky_occlusion_params.z, max(abs(place.x - sky_occlusion_center.x), abs(place.y - sky_occlusion_center.y)));
}

// -1 where nothing is known (the triangle is left out), else 1 with enough snow and 0 without, interpolated across a triangle:
// the fragments cut the surface where it falls under a half, so its edge runs across the texels (along a track at any angle)
// instead of going in steps of whole triangles. the thin snow is left to the surfaces under it the farther the more, so the two
// don't fight over the depth
float drawn_at(vec4 cell, vec2 place)
{
	if (cell.x <= SNOW_NONE)
		return -1.0;
	float least = 0.04 + length(place) * 0.0006;
	return (cell.y * edge_at(place) > least) ? 1.0 : 0.0;
}

void collapse()
{
	gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
	f_normal = vec3(0.0, 1.0, 0.0);
	f_pos = vec4(0.0);
	f_clip_pos = gl_Position;
	f_clip_future_pos = gl_Position;
	UserData = vec4(0.0);
	f_snow = vec4(0.0);
}

void main()
{
	// drawn only with the cells in the pass set (vk14::snow_drifts::drawn()); nothing is read past the end of their stand-in
	if (false == snow_maps())
	{
		collapse();
		return;
	}
	int across = int(snow_grid.x + 0.5);
	int stride = max(int(snow_grid.y + 0.5), 1);
	int quad = gl_VertexIndex / 6;
	int within = gl_VertexIndex % 6;
	float texel = texel_size();
	// the texel of the camera in the maps
	ivec2 camera = ivec2(floor(sky_occlusion_origin.xy / texel));

	// the triangle of the vertex: nothing known under a corner, or no snow at any of them: none of it is drawn (a triangle partly
	// over the snow is kept, the fragments cut it where its corners say, snow_surface.frag); nor across a step of the scenery
	int first = (within / 3) * 3;
	bool unknown = false;
	bool snowy = false;
	float low = 1.0e9;
	float high = -1.0e9;
	for (int i = 0; i < 3; ++i)
	{
		ivec2 other = texel_of(quad, CORNERS[first + i], across, stride, camera);
		vec4 othercell = cell_at(other);
		float otherdrawn = drawn_at(othercell, place_of(other));
		unknown = unknown || (otherdrawn < -0.5);
		snowy = snowy || (otherdrawn > 0.5);
		float otherheight = othercell.x - sky_occlusion_origin.z;
		low = min(low, otherheight);
		high = max(high, otherheight);
	}
	if (unknown || (false == snowy) || (high - low > texel * float(stride) * RISE_PER_METRE + RISE))
	{
		collapse();
		return;
	}

	// the vertex itself
	ivec2 t = texel_of(quad, CORNERS[within], across, stride, camera);
	vec4 cell = cell_at(t);
	vec2 place = place_of(t);
	float ground = cell.x - cell.y;
	float drawn = drawn_at(cell, place);
	float height = ground + cell.y * edge_at(place);

	// the normal from the heights around
	float h = cell.x;
	float hl = surface_at(t - ivec2(stride, 0), h);
	float hr = surface_at(t + ivec2(stride, 0), h);
	float hd = surface_at(t - ivec2(0, stride), h);
	float hu = surface_at(t + ivec2(0, stride), h);
	float span = 2.0 * texel * float(stride);
	vec3 normal = normalize(vec3((hl - hr) / span, 1.0, (hd - hu) / span));

	vec3 position = vec3(place.x, height - sky_occlusion_origin.z, place.y);
	f_pos = modelview * vec4(position, 1.0);
	f_normal = normalize(modelviewnormal * normal);
	f_clip_pos = projection * f_pos;
	f_clip_future_pos = (projection * future) * f_pos;
	gl_Position = f_clip_pos;
	UserData = vec4(0.0);
	// the heated blades of a switch are a track like the rest here: the snow their heaters leave
	float kind = (cell.w > SNOW_TRACKED + SNOW_HEATED * 0.5) ? cell.w - SNOW_HEATED : cell.w;
	f_snow = vec4(cell.y, kind, drawn, cell.x - sky_occlusion_origin.z);
}
