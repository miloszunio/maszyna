// grass of the heightmap terrain: the plant of an instance, made of the numbers of its vertex and its instance (grass.vert of
// opengl46 and vulkan14, the same file in shaders/ogl46 and shaders/vk14). the including shader gives the parameters of the draw
// (grass_tile, grass_part, grass_camera, grass_lod, grass_wind, grass_season), the kinds of the chunk (grass_base, grass_tip,
// grass_shape, grass_dry, grass_head, grass_headshape, grass_light, grass_lods, grass_share, grass_chunk), the textures
// (grass_heights, grass_density, grass_holes), read_kinds(), and VERTEX_ID and INSTANCE_ID.
//
// the instances of a draw stand in a tile of a chunk. a plant takes its place in the tile from a sequence which spreads any number
// of its first members evenly (R2, shifted for each tile), so drawing fewer of them thins the grass out evenly; a plant picks its
// kind by the densities painted there, and gets its height, its turn and its lean from its own numbers, so it's the same in every
// frame. plants of no kind, in the holes, or past the share drawn at their distance are collapsed into a point outside the view.
//
// the plant is a strip of triangles. the blade of grass (archetype 0) goes up in pairs of vertices to its tip, as it always did. the
// crops go up a stalk in pairs, then: an ear (1 cereal, 2 with awns) in pairs bowed by the droop, to its tip; spikelets or flowers
// (3 panicle, 4 rape) as cards joined on by repeated vertices; a stalk cut flat (5 stubble) or lying on the ground (6 straw). the
// vertices a plant doesn't use (the tile draws as many as its plant with the most of them needs) repeat its last one.
// the chunk is drawn relative to its corner: x, z in metres from the corner, y the height as it is

layout(location = 0) out vec3 f_normal;
layout(location = 3) out vec4 f_pos;
layout(location = 12) out vec3 f_albedo;
layout(location = 4) out vec4 f_clip_pos;
layout(location = 5) out vec4 f_clip_future_pos;
// the canopy of a crop, for grass.frag: x: how much of the sun comes through the leaves seen against it; y: the sheen at grazing
// angles; z: how deep in the canopy the point is, 0 at its top .. 1 on the ground; w: how dense the canopy is (leaf area index)
layout(location = 6) out vec4 f_canopy;
layout(location = 7) out vec4 UserData;

const int ARCHETYPE_BLADE = 0;
const int ARCHETYPE_CEREAL = 1;
const int ARCHETYPE_AWNED = 2;
const int ARCHETYPE_PANICLE = 3;
const int ARCHETYPE_RAPE = 4;
const int ARCHETYPE_STUBBLE = 5;
const int ARCHETYPE_STRAW = 6;

uint hash(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

float random(inout uint state)
{
	state = hash(state);
	return float(state >> 8) * (1.0 / 16777216.0);
}

// the share of the blades of the kind drawn at specified distance for speed: fewer further away, each standing for more of them
float lod_thinning(int kind, float distance)
{
	return distance <= grass_lods[kind].x ? 1.0 : pow(grass_lods[kind].x / distance, 1.5);
}

// the share of the blades drawn: the above, and fewer and fewer over the fading distance, where the grass really gets
// sparser; the same as lod_share() of grassfield.cpp
float lod_share(int kind, float distance)
{
	return lod_thinning(kind, distance) * (1.0 - smoothstep(grass_lods[kind].y, grass_lods[kind].z, distance));
}

// the vertices of the plant of an archetype, of the segments of its stalk; the same as grass_field::plant_vertices()
int plant_vertices(int archetype, int segments)
{
	int stalk = 2 * (segments + 1);
	if (archetype == ARCHETYPE_CEREAL)
		return stalk + 2 * segments + 1;
	if (archetype == ARCHETYPE_AWNED)
		return stalk + 2 * segments + 3;
	if ((archetype == ARCHETYPE_PANICLE) || (archetype == ARCHETYPE_RAPE))
		return stalk + 1 + 6 * (segments + 2);
	if ((archetype == ARCHETYPE_STUBBLE) || (archetype == ARCHETYPE_STRAW))
		return stalk;
	return 2 * segments + 1;
}

// height of the ground at a point of the chunk and its normal, the way the full grid is triangulated
float ground(vec2 point, out vec3 normal)
{
	float cells = grass_chunk.y;
	float spacing = grass_chunk.x / cells;
	vec2 place = clamp(point / spacing, vec2(0.0), vec2(cells));
	ivec2 cell = min(ivec2(place), ivec2(int(cells) - 1));
	vec2 t = place - vec2(cell);
	float h00 = texelFetch(grass_heights, cell, 0).r;
	float h10 = texelFetch(grass_heights, cell + ivec2(1, 0), 0).r;
	float h01 = texelFetch(grass_heights, cell + ivec2(0, 1), 0).r;
	float h11 = texelFetch(grass_heights, cell + ivec2(1, 1), 0).r;
	// the quads are cut along the diagonal from (x0, z1) to (x1, z0), as the mesh is made
	if (t.x + t.y <= 1.0)
	{
		normal = normalize(vec3(-(h10 - h00) / spacing, 1.0, -(h01 - h00) / spacing));
		return h00 + t.x * (h10 - h00) + t.y * (h01 - h00);
	}
	normal = normalize(vec3(-(h11 - h01) / spacing, 1.0, -(h11 - h10) / spacing));
	return h11 + (1.0 - t.x) * (h01 - h11) + (1.0 - t.y) * (h10 - h11);
}

void collapse()
{
	gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
	f_normal = vec3(0.0, 1.0, 0.0);
	f_pos = vec4(0.0);
	f_albedo = vec3(0.0);
	f_clip_pos = gl_Position;
	f_clip_future_pos = gl_Position;
	f_canopy = vec4(0.0);
	UserData = vec4(0.0);
}

// what a plant is made of, worked out once for all its vertices
struct plant_s
{
	int kind;
	int archetype;
	int segments;
	vec3 root;
	vec3 groundnormal;
	float height; // to the top of the stalk
	float width; // of the stalk at its root
	vec2 facing; // the face of the stalk
	vec2 across;
	vec2 bend; // where the top of the stalk goes, share of the height
	float bent;
	vec3 toview; // from the top of the plant to the camera
	uint state; // the numbers of its spikelets and flowers
};

// a vertex of a plant: where, which way it faces, how far up the plant (0..1) and how much of the head its colour has
struct vertex_s
{
	vec3 position;
	vec3 normal;
	float t;
	float head;
};

// a point of the stalk, T up it, Side across it (-0.5 .. 0.5), as wide as Halfwidth
vec3 stalk_point(plant_s plant, float t, float side, float halfwidth)
{
	if (plant.archetype == ARCHETYPE_STRAW)
	{
		// lying along its face, the ends lifted a little off the ground
		vec2 xz = plant.root.xz + plant.facing * (plant.height * (t - 0.5)) + plant.across * (side * halfwidth * 2.0);
		return vec3(xz.x, plant.root.y + 0.012 + 0.02 * abs(t - 0.5), xz.y);
	}
	vec2 xz = plant.root.xz + plant.across * (side * halfwidth * 2.0) + plant.bend * (plant.height * t * t);
	float y = plant.root.y - 0.02 + plant.height * t * (1.0 - 0.35 * plant.bent * t);
	return vec3(xz.x, y, xz.y);
}

// which way the stalk goes at its top
vec3 stalk_direction(plant_s plant)
{
	return normalize(vec3(2.0 * plant.bend.x, 1.0 - 0.7 * plant.bent, 2.0 * plant.bend.y));
}

// a vertex of the stalk; the blade of grass narrows to its tip as it always did, the stalks of the crops hardly at all
vertex_s stalk_vertex(plant_s plant, int index)
{
	int level = min(index / 2, plant.segments);
	float side = ((index & 1) == 0) ? -0.5 : 0.5;
	float t = float(level) / float(plant.segments);
	float taper = (plant.archetype == ARCHETYPE_STUBBLE || plant.archetype == ARCHETYPE_STRAW) ? 1.0 : 1.0 - 0.3 * t;
	vertex_s v;
	v.position = stalk_point(plant, t, side, plant.width * 0.5 * taper);
	// round across the stalk, as a cylinder
	vec3 outward = vec3(plant.across.x, 0.0, plant.across.y) * (side * 1.2);
	if (plant.archetype == ARCHETYPE_STRAW)
		v.normal = normalize(plant.groundnormal + outward * 0.3);
	else
		v.normal = normalize(vec3(plant.facing.x, 0.0, plant.facing.y) * 0.6 + outward + plant.groundnormal * 0.5);
	v.t = (plant.archetype == ARCHETYPE_STRAW) ? 0.6 + 0.4 * abs(t - 0.5) : t;
	// the cut end of the stubble is lighter
	v.head = (plant.archetype == ARCHETYPE_STUBBLE && level == plant.segments) ? 1.0 : 0.0;
	return v;
}

// a point of the ear, U along it (0 at the top of the stalk, 1 at its tip, more for the awns), bowed by the droop
vec3 ear_point(plant_s plant, vec3 top, vec3 up, vec3 out_, float u)
{
	float earlength = grass_headshape[plant.kind].x;
	float bow = grass_headshape[plant.kind].z * 2.2;
	if (bow < 1e-3)
		return top + up * (earlength * u);
	float angle = bow * u;
	return top + (up * sin(angle) + out_ * (1.0 - cos(angle))) * (earlength / bow);
}

// a vertex of the ear: Index from the first after the stalk
vertex_s ear_vertex(plant_s plant, int index)
{
	vec3 top = stalk_point(plant, 1.0, 0.0, 0.0);
	vec3 up = stalk_direction(plant);
	// the ear bows the way the stalk leans, or the way of its own face when it stands upright
	vec3 lean = vec3(plant.bend.x, 0.0, plant.bend.y);
	if (dot(lean, lean) < 1e-6)
		lean = vec3(plant.facing.x, 0.0, plant.facing.y);
	vec3 out_ = lean - up * dot(lean, up);
	out_ = dot(out_, out_) > 1e-8 ? normalize(out_) : vec3(1.0, 0.0, 0.0);
	vec4 head = grass_headshape[plant.kind];
	int pairs = plant.segments;
	int pair = index / 2;
	float side = ((index & 1) == 0) ? -0.5 : 0.5;
	float u;
	float halfwidth;
	if (pair < pairs)
	{
		// the body of the ear, widest low down
		u = 0.03 + 0.85 * float(pair) / float(max(pairs, 1));
		halfwidth = head.y * 0.5 * (1.0 - 0.25 * u);
	}
	else if (plant.archetype == ARCHETYPE_AWNED && index < 2 * pairs + 2)
	{
		// the top of the ear, the awns go up from it
		u = 1.0;
		halfwidth = head.y * 0.3;
	}
	else
	{
		// the tip: of the ear, or of the awns
		side = 0.0;
		halfwidth = 0.0;
		u = (plant.archetype == ARCHETYPE_AWNED) ? 1.0 + head.w / max(head.x, 1e-3) : 1.0;
	}
	vec3 centre = ear_point(plant, top, up, out_, u);
	// the ear faces the camera, so it never shows its edge
	vec3 along = normalize(ear_point(plant, top, up, out_, min(u + 0.05, 1.0)) - ear_point(plant, top, up, out_, max(u - 0.05, 0.0)) + up * 1e-4);
	vec3 across = cross(along, plant.toview);
	across = dot(across, across) > 1e-8 ? normalize(across) : vec3(plant.across.x, 0.0, plant.across.y);
	vertex_s v;
	v.position = centre + across * (side * halfwidth * 2.0);
	v.normal = normalize(plant.toview * 0.6 + across * (side * 1.2) + vec3(0.0, 0.4, 0.0));
	v.t = 1.0;
	// the awns are paler than the ear
	v.head = (u > 1.0) ? 1.15 : 1.0;
	return v;
}

// a corner of a card (a spikelet of oats, the flowers of rape): Card of the cards, Corner 0..3 (the top pair, then the bottom one)
vertex_s card_vertex(plant_s plant, int card, int corner)
{
	vec4 head = grass_headshape[plant.kind];
	uint state = hash(plant.state ^ uint(card) * 0x9E3779B9U);
	float around = random(state) * 6.2831853;
	float down = random(state);
	float reach = random(state);
	vec3 radial = vec3(cos(around), 0.0, sin(around));
	vec3 top = stalk_point(plant, 1.0, 0.0, 0.0);
	vec3 anchor;
	vec3 hang;
	float size;
	if (plant.archetype == ARCHETYPE_PANICLE)
	{
		// the spikelets hang from branches along the top of the stalk, the lower branches longer
		float along = (float(card) + 0.5) / float(plant.segments + 2);
		anchor = top - vec3(0.0, head.x * along * 0.9, 0.0) + radial * (head.x * (0.12 + 0.3 * along) * (0.6 + 0.4 * reach));
		hang = normalize(radial * 0.35 + vec3(0.0, mix(1.0, -1.0, head.z), 0.0));
		size = head.y * 2.6;
	}
	else
	{
		// the flowers (the pods) crowd round the top, more of them above
		anchor = top + radial * (head.x * 0.35 * reach) + vec3(0.0, head.x * (down - 0.35), 0.0);
		hang = normalize(vec3(0.0, 1.0, 0.0) + radial * 0.4);
		size = head.y;
	}
	vec3 across = cross(hang, plant.toview);
	across = dot(across, across) > 1e-8 ? normalize(across) : radial;
	float side = ((corner & 1) == 0) ? -0.5 : 0.5;
	float width = (plant.archetype == ARCHETYPE_PANICLE) ? head.y : size;
	vertex_s v;
	v.position = anchor + hang * (corner < 2 ? 0.0 : size) + across * (side * width);
	v.normal = normalize(plant.toview * 0.5 + radial * 0.5 + vec3(0.0, 0.5, 0.0));
	v.t = 1.0;
	v.head = 1.0;
	return v;
}

// the vertex Index of the plant
vertex_s plant_vertex(plant_s plant, int index)
{
	int last = plant_vertices(plant.archetype, plant.segments) - 1;
	index = min(index, last);
	int stalk = 2 * (plant.segments + 1);
	if (index < stalk)
		return stalk_vertex(plant, index);
	if ((plant.archetype == ARCHETYPE_CEREAL) || (plant.archetype == ARCHETYPE_AWNED))
		return ear_vertex(plant, index - stalk);
	// the cards: the stalk ends with its last vertex repeated, each card is its first and its last corner repeated around the
	// four, so the triangles between the cards have no area
	if (index == stalk)
		return stalk_vertex(plant, stalk - 1);
	int card = (index - stalk - 1) / 6;
	int corner = clamp((index - stalk - 1) % 6 - 1, 0, 3);
	return card_vertex(plant, card, corner);
}

void main()
{
	read_kinds();
	int blade = INSTANCE_ID;
	// the numbers of the blade, drawn in the same order whatever becomes of them
	uint state = hash(uint(blade) * 0x9E3779B9U ^ hash(uint(int(grass_part.z)) * 0x85EBCA6BU ^ hash(uint(int(grass_part.w)) + 0x27D4EB2FU)));
	uint tilestate = hash(uint(int(grass_part.z)) * 0x85EBCA6BU ^ hash(uint(int(grass_part.w)) + 0x165667B1U));
	vec2 shift = vec2(random(tilestate), random(tilestate));
	vec2 jitter = vec2(random(state), random(state)) - 0.5;
	float pick = random(state);
	float heightrandom = random(state) * 2.0 - 1.0;
	float widthrandom = random(state);
	float turnrandom = random(state) * 6.2831853;
	float leanrandom = random(state) * 6.2831853;
	float leanamount = random(state);
	float shade = random(state);
	float phase = random(state);
	float early = random(state);

	// place: R2 spreads the first blades evenly at any count, a little jitter breaks its lattice
	vec2 spot = fract(shift + float(blade) * vec2(0.7548776662466927, 0.5698402909980532));
	vec2 point = grass_tile.xy + fract(spot + jitter * (0.7 / sqrt(grass_tile.w))) * grass_tile.z;
	if (any(lessThan(point, vec2(0.0))) || any(greaterThanEqual(point, vec2(grass_chunk.x))))
	{
		collapse();
		return;
	}
	vec2 local = point / grass_chunk.x;
	float samples = grass_chunk.z;
	vec4 density = texture(grass_density, (local * samples + 0.5) / (samples + 1.0));
	float holeside = float(textureSize(grass_holes, 0).x);
	float hole = texture(grass_holes, (local * (holeside - 1.0) + 0.5) / holeside).r;

	// the kind: the blades of the tile are shared among the kinds by their densities here
	vec4 cover = density * grass_share / max(grass_part.x, 1e-3);
	float covered = cover.x + cover.y + cover.z + cover.w;
	if (covered > 1.0)
		cover /= covered;
	int kind = -1;
	float sum = 0.0;
	for (int i = 0; i < 4; ++i)
	{
		sum += cover[i];
		if (pick < sum)
		{
			kind = i;
			break;
		}
	}

	vec3 groundnormal;
	vec3 root = vec3(point.x, ground(point, groundnormal), point.y);
	float distance = length(root - grass_camera.xyz);
	float rank = (float(blade) + 0.5) / grass_tile.w;
	if (kind < 0 || hole > 0.5 || distance > max(grass_lod.z, grass_lods[max(kind, 0)].z))
	{
		collapse();
		return;
	}
	float share = lod_share(kind, distance);
	if (rank >= share || distance > grass_lods[kind].z)
	{
		collapse();
		return;
	}

	vec4 shape = grass_shape[kind];
	float height = shape.x * (1.0 + shape.y * heightrandom);
	// the last of the blades drawn at the distance grow in, and the grass fades out at the end of its reach
	height *= 1.0 - smoothstep(share * 0.8, share, rank);
	// lower in winter, growing in spring
	float seasonal = grass_dry[kind].a;
	height *= mix(1.0, grass_season.z, seasonal);
	// where the paint thins out the blades get shorter; a crop sown sparser stands as tall, only its edge is lower
	int archetype = int(grass_head[kind].a + 0.5);
	height *= mix(0.5, 1.0, clamp(density[kind] * (archetype == ARCHETYPE_BLADE ? 1.5 : 3.0), 0.0, 1.0));
	// the blades drawn further away stand for more of them, so they're wider (the crops as much as it takes to keep the
	// field covered)
	float width = shape.z * (0.8 + 0.4 * widthrandom) / pow(lod_thinning(kind, distance), grass_lods[kind].w);

	// the face of the blade turns towards the camera further away, so the blades don't vanish edge on
	vec2 facing = vec2(cos(turnrandom), sin(turnrandom));
	vec2 toview = grass_camera.xz - root.xz;
	toview = toview / max(length(toview), 1e-3);
	facing = mix(facing, toview, smoothstep(grass_lods[kind].x * 0.5, grass_lods[kind].x * 2.0, distance));
	facing = facing / max(length(facing), 1e-3);
	vec2 across = vec2(-facing.y, facing.x);

	// the lean of the blade and the wind: a steady push along it, with gusts running across the field, and a flutter
	vec2 lean = vec2(cos(leanrandom), sin(leanrandom)) * (shape.w * (0.4 + 0.6 * leanamount));
	float gust = 0.5 + 0.5 * sin(grass_wind.w * 0.9 - dot(root.xz, grass_wind.xy) * 0.12);
	float flutter = sin(grass_wind.w * (2.0 + phase) + phase * 6.2831853);
	vec2 sway = grass_wind.xy * grass_wind.z * (0.15 + 0.45 * gust + 0.08 * flutter);
	vec2 bend = lean + sway;
	// a long lean takes some of the height
	float bent = min(length(bend), 0.9);

	int segments = int(grass_part.y);
	vec3 position;
	vec3 normal;
	float t;
	float headshare = 0.0;
	if (archetype == ARCHETYPE_BLADE)
	{
		int level = min(VERTEX_ID / 2, segments);
		float side = (VERTEX_ID >= segments * 2) ? 0.0 : ((VERTEX_ID & 1) == 0 ? -0.5 : 0.5);
		t = float(level) / float(segments);
		float halfwidth = width * (1.0 - pow(t, 1.5));
		vec2 xz = root.xz + across * (side * halfwidth) + bend * (height * t * t);
		float y = root.y - 0.02 + height * t * (1.0 - 0.35 * bent * t);
		position = vec3(xz.x, y, xz.y);
		// lit mostly as the ground under it, which keeps a field of blades from shimmering
		normal = normalize(groundnormal * 0.8 + vec3(facing.x, 0.0, facing.y) * (side == 0.0 ? 0.0 : 0.25) + vec3(0.0, 0.3, 0.0));
	}
	else
	{
		plant_s plant;
		plant.kind = kind;
		plant.archetype = archetype;
		plant.segments = segments;
		plant.root = root;
		plant.groundnormal = groundnormal;
		// the ear stands on the stalk; the spikelets and the flowers hang around its top
		bool eared = (archetype == ARCHETYPE_CEREAL) || (archetype == ARCHETYPE_AWNED);
		plant.height = max(height - (eared ? grass_headshape[kind].x : 0.0), height * 0.3);
		plant.width = width;
		plant.facing = facing;
		plant.across = across;
		// the heads of the ripe crops are heavy: the wind bows them more
		plant.bend = bend * (eared ? 1.2 : 1.0);
		plant.bent = min(length(plant.bend), 0.9);
		vec3 planttop = root + vec3(0.0, height, 0.0);
		plant.toview = normalize(grass_camera.xyz - planttop + vec3(0.0, 1e-3, 0.0));
		plant.state = state;
		vertex_s v = plant_vertex(plant, VERTEX_ID);
		position = v.position;
		normal = v.normal;
		t = v.t;
		headshare = v.head;
	}

	// the seasons: some blades dry out (and green up) earlier than others, so a field turns gradually; a dry blade takes the
	// dry colour, darker at its root. in spring the green is fresher
	float dry = clamp(grass_season.x * seasonal * (0.6 + 0.8 * early), 0.0, 1.0);
	vec3 bottom = mix(grass_base[kind].rgb, grass_dry[kind].rgb * 0.45, dry);
	vec3 top = mix(grass_tip[kind].rgb, grass_dry[kind].rgb, dry);
	top *= mix(vec3(1.0), vec3(1.08, 1.22, 0.85), grass_season.y * seasonal * (1.0 - dry));
	vec3 albedo = mix(bottom, top, t) * (0.85 + 0.3 * shade);
	if (archetype == ARCHETYPE_BLADE)
	{
		// the lower part of the blades is shaded by the others
		albedo *= mix(0.7, 1.0, t);
		f_canopy = vec4(0.0);
	}
	else
	{
		// the ear, the flowers, the cut end; the awns paler
		vec3 headcolour = grass_head[kind].rgb * (0.9 + 0.2 * shade) * (headshare > 1.0 ? 1.12 : 1.0);
		albedo = mix(albedo, headcolour, clamp(headshare, 0.0, 1.0));
		// the canopy: as dense as the crop is painted, the stubble and the straw hardly one
		float leaves = (archetype == ARCHETYPE_STUBBLE || archetype == ARCHETYPE_STRAW) ? 0.4 : 3.5;
		float lai = leaves * clamp(density[kind] * 1.5, 0.0, 1.0);
		float depth = clamp(1.0 - (position.y - root.y) / max(height, 0.05), 0.0, 1.0);
		// the light of the sky reaches down into the canopy less and less
		albedo *= exp(-0.3 * lai * depth);
		f_canopy = vec4(grass_light[kind].x * (1.0 - dry), grass_light[kind].y, depth, lai);
	}

	f_pos = modelview * vec4(position, 1.0);
	f_normal = normalize(modelviewnormal * normal);
	f_albedo = albedo;
	f_clip_pos = projection * f_pos;
	f_clip_future_pos = (projection * future) * f_pos;
	gl_Position = f_clip_pos;
	UserData = vec4(0.0);
}
