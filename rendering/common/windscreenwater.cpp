/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "rendering/common/windscreenwater.h"

#include "model/Model3d.h"
#include "model/material.h"
#include "rendering/renderer.h"
#include "simulation/simulation.h"
#include "simulation/simulationenvironment.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "vehicle/DynObj.h"
#include "vehicle/Train.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <unordered_map>

namespace gfx
{

namespace
{

// the param of mat_rain_windscreen the slot of a glass goes to (water_slot: param[2].z) and the one of the size of its drops
// (rain_scale: param[2].y)
std::size_t constexpr SLOT_PARAM{2};
// how fast the rain and the snow fall, m/s (as the particles of the rain and the snow, gfx::gpu_particles)
float constexpr RAIN_FALL{7.f};
float constexpr SNOW_FALL{1.1f};
// the share of the wind the rain drifts with (as the particles of the rain and the snow)
double constexpr WIND_SHARE{0.5};
// the smallest raindrop landing as a drop of its own, mm: the smaller ones are the mist of the wet glass
float constexpr LEAST_DROP{0.5f};
// the water a flake of snow brings, mm3 on average (water_spawn.comp: 0.1 .. 0.7): a flake of 2 to 6 mm weighs some tenths of a mg
float constexpr FLAKE_WATER{0.4f};
// the share of the water of a rain the same snow brings: a snowstorm gives 1.5 to 2.5 mm of water an hour, a downpour 30
float constexpr SNOW_WATER{0.06f};
// the most landings tried on a glass in a step
std::uint32_t constexpr MOST_TRIES{1024};
// the longest step, seconds: a longer frame slows the water down rather than let it jump
double constexpr LONGEST_STEP{0.05};
// gfx.windscreen.auto: what tells a glass of the cab among its translucent parts (one material may hold the windscreens and the
// side windows together). a part of it faces ahead or back when its normal is this close to z of the model (the length of the
// vehicle); the least area of those parts, m2; how far out from the middle of the cab they lie along z, a share of half its length
float constexpr FOUND_FACING{0.7f};
float constexpr FOUND_LEAST_AREA{0.15f};
float constexpr FOUND_OUTER{0.3f};
// a side window lies this far out from the middle of the cab across it, a share of half its width
float constexpr FOUND_SIDE{0.6f};
// the least area of a single submodel to be a windscreen or a side window, m2: the glasses of the gauges of the desk, of the same
// material and facing ahead as well, are a few hundredths
float constexpr FOUND_PART_AHEAD{0.1f};
float constexpr FOUND_PART_ASIDE{0.05f};
// of a glass found by its shape only the triangles this close to its outermost (m) are laid into its map: the windscreen may lean
// back by this much, the glasses of the gauges of the desk lie deeper in the cab
float constexpr FOUND_DEPTH_AHEAD{0.45f};
float constexpr FOUND_DEPTH_ASIDE{0.3f};
// the same on the model of the vehicle seen from outside: the windscreens only, not the glasses of the lamps
float constexpr FOUND_OUTSIDE_AREA{0.3f};
// the wipers laid over a glass found by its shape: the pivot under the bottom edge, a share of the height of the glass; the reach
// of the blade, a share of the height above the pivot, and where the blade starts on its arm, a share of its reach; the arc it
// sweeps, degrees from the horizontal of the glass, from where it rests to the far end
float constexpr WIPER_PIVOT_BELOW{0.05f};
// the part of a windscreen the wipers go over: facing ahead or back at least this much, its curved edges included
float constexpr WIPED_FACING{0.5f};
float constexpr WIPER_REACH{1.05f};
float constexpr WIPER_ARM{0.08f};
float constexpr WIPER_REST{178.f};
float constexpr WIPER_END{2.f};
// the windscreens of a glass found by its shape told apart across it: in so many bins, by a gap of so many of them; and the least
// width of one, m
std::size_t constexpr PANE_BINS{128};
std::size_t constexpr PANE_GAP_BINS{3};
float constexpr PANE_LEAST_WIDTH{0.2f};
// a vehicle without wipers of its own (no animwiperprefix in the .mmd) gets one on each windscreen found by its shape, run by its
// wiper switch (TMoverParameters::wiperSwitchPos: 1 slow, 2 and up fast): seconds of a sweep there and back
double constexpr VIRTUAL_SLOW{2.4};
double constexpr VIRTUAL_FAST{1.2};

// whether the material is a glass: one of mat_rain_windscreen, with a mask of the wipers
bool is_glass(opengl_material const *Material)
{
	return (Material != nullptr) && (Material->shader != nullptr) && (Material->shader->texture_conf.count("wipermask") > 0) && (Material->shader->param_conf.count("water_slot") > 0);
}

// calls Visit for every submodel of the branch, its children and its siblings
template <typename Visit> void visit_submodels(TSubModel *Submodel, Visit &&Visitor)
{
	while (Submodel != nullptr)
	{
		Visitor(Submodel);
		visit_submodels(Submodel->ChildGet(), Visitor);
		Submodel = Submodel->NextGet();
	}
}

// the vertices of a submodel, wherever they are kept
gfx::vertex_array const &submodel_vertices(TSubModel const *Submodel)
{
	return (Submodel->m_geometry.handle != null_handle ? GfxRenderer->Vertices(Submodel->m_geometry.handle) : Submodel->Vertices);
}

// the transformation of a submodel to the space of the model
glm::mat4 submodel_transform(TSubModel const *Submodel)
{
	float4x4 matrix;
	Submodel->ParentMatrix(&matrix);
	return glm::make_mat4(matrix.readArray());
}

// calls Visit with the corners of every triangle of a submodel drawn with triangles: the places in the space of the model and the
// coordinates of the texture
template <typename Visit> void visit_triangles(TSubModel *Submodel, Visit &&Visitor)
{
	if (Submodel->eType != GL_TRIANGLES)
	{
		return;
	}
	auto const &vertices{submodel_vertices(Submodel)};
	if (vertices.size() < 3)
	{
		return;
	}
	auto const transform{submodel_transform(Submodel)};
	gfx::index_array const *indices{nullptr};
	if ((Submodel->m_geometry.handle != null_handle) && (Submodel->m_geometry.index_count > 0))
	{
		indices = &GfxRenderer->Indices(Submodel->m_geometry.handle);
	}
	else if (false == Submodel->Indices.empty())
	{
		indices = &Submodel->Indices;
	}
	auto const count{indices != nullptr ? indices->size() : vertices.size()};
	for (std::size_t first = 0; first + 2 < count; first += 3)
	{
		std::array<glm::vec3, 3> position;
		std::array<glm::vec2, 3> uv;
		bool valid{true};
		for (std::size_t corner = 0; corner < 3; ++corner)
		{
			auto const index{indices != nullptr ? static_cast<std::size_t>((*indices)[first + corner]) : first + corner};
			if (index >= vertices.size())
			{
				valid = false;
				break;
			}
			position[corner] = glm::vec3{transform * glm::vec4{vertices[index].position, 1.f}};
			uv[corner] = vertices[index].texture;
		}
		if (valid)
		{
			Visitor(position, uv);
		}
	}
}

// the shape of a translucent submodel of the model of a cab, what tells a glass among its translucent parts
struct part_shape
{
	TSubModel *submodel{nullptr};
	float area{0.f};
	float ahead{0.f}; // the area of its triangles facing ahead or back (FOUND_FACING)
	float aside{0.f}; // the area of its triangles facing a side
	glm::vec3 centre{0.f}; // the middles of its triangles times their areas
};

// a triangle of a glass laid into its map: for the texels of the uv on it, the step along the glass for a step of u and of v, and
// which way is out of the cab (Inside: a point inside it); Places, when wanted, where each texel lies in the model of the cab
void lay_triangle(std::array<glm::vec3, 3> const &position, std::array<glm::vec2, 3> uv, glm::vec3 const &Inside, std::vector<glm::vec4> &Axes, std::vector<glm::vec3> *Places)
{
	auto const size{static_cast<int>(gl::WATER_MAP)};
	// the steps along the glass for the steps of u and v: the edges of the triangle in both spaces
	glm::mat2 const uvedges{uv[1] - uv[0], uv[2] - uv[0]};
	auto const det{glm::determinant(uvedges)};
	if (std::abs(det) < 1e-12f)
	{
		return;
	}
	auto const inverse{glm::inverse(uvedges)};
	auto const e1{position[1] - position[0]};
	auto const e2{position[2] - position[0]};
	// [tu tv] = [e1 e2] * inverse
	auto const tu{e1 * inverse[0][0] + e2 * inverse[0][1]};
	auto const tv{e1 * inverse[1][0] + e2 * inverse[1][1]};
	auto const centre{(position[0] + position[1] + position[2]) / 3.f};
	auto const out{glm::dot(glm::cross(tu, tv), centre - Inside) >= 0.f ? 1.f : -1.f};
	// a triangle laid on a repeat of the texture is taken to the first one (the mask of the wipers repeats with it)
	auto const shift{glm::floor((uv[0] + uv[1] + uv[2]) / 3.f)};
	for (auto &corner : uv)
	{
		corner -= shift;
	}
	auto const lower{glm::max(glm::ivec2{glm::floor(glm::min(glm::min(uv[0], uv[1]), uv[2]) * static_cast<float>(size))}, glm::ivec2{0})};
	auto const upper{glm::min(glm::ivec2{glm::ceil(glm::max(glm::max(uv[0], uv[1]), uv[2]) * static_cast<float>(size))}, glm::ivec2{size - 1})};
	for (int y = lower.y; y <= upper.y; ++y)
	{
		for (int x = lower.x; x <= upper.x; ++x)
		{
			// the middle of the texel inside the triangle (barycentric)
			auto const point{(glm::vec2{static_cast<float>(x), static_cast<float>(y)} + 0.5f) / static_cast<float>(size)};
			auto const weights{inverse * (point - uv[0])};
			if ((weights.x < -1e-4f) || (weights.y < -1e-4f) || (weights.x + weights.y > 1.f + 1e-4f))
			{
				continue;
			}
			auto const texel{static_cast<std::size_t>(y) * gl::WATER_MAP + static_cast<std::size_t>(x)};
			Axes[texel * 2] = glm::vec4{tu, 1.f};
			Axes[texel * 2 + 1] = glm::vec4{tv, out};
			if (Places != nullptr)
			{
				(*Places)[texel] = position[0] + e1 * weights.x + e2 * weights.y;
			}
		}
	}
}

// the angle of a place about a pivot, degrees from the horizontal of the glass (H), towards its up (U)
float angle_about(glm::vec2 const &Place, glm::vec2 const &Pivot)
{
	return glm::degrees(std::atan2(Place.y - Pivot.y, Place.x - Pivot.x));
}


// the box around the whole of a model, in its own space
void model_box(TSubModel *Root, glm::vec3 &Lower, glm::vec3 &Upper)
{
	Lower = glm::vec3{std::numeric_limits<float>::max()};
	Upper = glm::vec3{std::numeric_limits<float>::lowest()};
	visit_submodels(Root, [&Lower, &Upper](TSubModel *Submodel) {
		auto const &vertices{submodel_vertices(Submodel)};
		if (vertices.empty())
			return;
		auto const transform{submodel_transform(Submodel)};
		for (auto const &vertex : vertices)
		{
			auto const position{glm::vec3{transform * glm::vec4{vertex.position, 1.f}}};
			Lower = glm::min(Lower, position);
			Upper = glm::max(Upper, position);
		}
	});
}

// a glass found by its shape in its own plane: across it and up it, for each texel on it
struct glass_plane
{
	bool valid{false};
	std::vector<glm::vec2> flat;
};

// the plane of a glass from its map: the metric of the uv (the steps along the glass for the steps of u and v) and the way up the
// glass in the uv, averaged over the texels on it; each texel then across the glass and up it, in metres
glass_plane plane_of(std::vector<glm::vec4> const &Axes, std::vector<bool> const &On)
{
	glass_plane plane;
	auto const size{static_cast<std::size_t>(gl::WATER_MAP)};
	glm::dvec3 metric{0.0}; // the uv steps along the glass squared: u u, u v, v v
	glm::dvec2 rising{0.0}; // the step of the uv going up the glass
	std::size_t count{0};
	for (std::size_t texel = 0; texel < On.size(); ++texel)
	{
		if (false == On[texel])
			continue;
		glm::dvec3 const tu{Axes[texel * 2]};
		glm::dvec3 const tv{Axes[texel * 2 + 1]};
		auto const normal{glm::cross(tu, tv)};
		if (glm::length(normal) <= 0.0)
			continue;
		auto const n{glm::normalize(normal)};
		auto up{glm::dvec3{0.0, 1.0, 0.0} - n * n.y};
		if (glm::length(up) < 1e-3)
			continue;
		up = glm::normalize(up);
		glm::dmat2 const gram{glm::dot(tu, tu), glm::dot(tu, tv), glm::dot(tu, tv), glm::dot(tv, tv)};
		if (std::abs(glm::determinant(gram)) < 1e-18)
			continue;
		metric += glm::dvec3{gram[0][0], gram[0][1], gram[1][1]};
		rising += glm::inverse(gram) * glm::dvec2{glm::dot(tu, up), glm::dot(tv, up)};
		++count;
	}
	if (count == 0)
	{
		// a glass lying flat has no bottom edge for the wipers
		return plane;
	}
	metric /= static_cast<double>(count);
	rising /= static_cast<double>(count);
	auto const dot = [&metric](glm::dvec2 const &P, glm::dvec2 const &Q) { return metric.x * P.x * Q.x + metric.y * (P.x * Q.y + P.y * Q.x) + metric.z * P.y * Q.y; };
	if (dot(rising, rising) <= 0.0)
	{
		return plane;
	}
	auto const up{rising / std::sqrt(dot(rising, rising))};
	glm::dvec2 across{-(metric.y * up.x + metric.z * up.y), metric.x * up.x + metric.y * up.y};
	if (dot(across, across) <= 0.0)
	{
		return plane;
	}
	across /= std::sqrt(dot(across, across));
	plane.flat.assign(On.size(), glm::vec2{0.f});
	for (std::size_t texel = 0; texel < On.size(); ++texel)
	{
		if (false == On[texel])
			continue;
		glm::dvec2 const uv{(static_cast<double>(texel % size) + 0.5) / static_cast<double>(size), (static_cast<double>(texel / size) + 0.5) / static_cast<double>(size)};
		plane.flat[texel] = glm::vec2{static_cast<float>(dot(uv, across)), static_cast<float>(dot(uv, up))};
	}
	plane.valid = true;
	return plane;
}

// the windscreens of a glass: the runs across it without a gap (the pillar between two windscreens), as ranges across it
std::vector<glm::vec2> panes_of(glass_plane const &Plane, std::vector<bool> const &On)
{
	std::vector<glm::vec2> panes;
	if (false == Plane.valid)
	{
		return panes;
	}
	auto low{std::numeric_limits<float>::max()};
	auto high{std::numeric_limits<float>::lowest()};
	for (std::size_t texel = 0; texel < On.size(); ++texel)
	{
		if (false == On[texel])
			continue;
		low = std::min(low, Plane.flat[texel].x);
		high = std::max(high, Plane.flat[texel].x);
	}
	if (high - low < PANE_LEAST_WIDTH)
	{
		return panes;
	}
	std::array<bool, PANE_BINS> occupied{};
	auto const binwidth{(high - low) / static_cast<float>(PANE_BINS)};
	for (std::size_t texel = 0; texel < On.size(); ++texel)
	{
		if (On[texel])
			occupied[std::min<std::size_t>(static_cast<std::size_t>((Plane.flat[texel].x - low) / binwidth), PANE_BINS - 1)] = true;
	}
	// a gap of the glass wider than a few bins parts two windscreens; a run too narrow to wipe is left out
	std::size_t start{0};
	std::size_t bin{0};
	while (bin < PANE_BINS)
	{
		if (occupied[bin])
		{
			++bin;
			continue;
		}
		auto gap{bin};
		while ((gap < PANE_BINS) && (false == occupied[gap]))
			++gap;
		if ((gap - bin >= PANE_GAP_BINS) || (gap == PANE_BINS))
		{
			auto const from{low + static_cast<float>(start) * binwidth};
			auto const to{low + static_cast<float>(bin) * binwidth};
			if (to - from >= PANE_LEAST_WIDTH)
				panes.emplace_back(from, to);
			start = gap;
		}
		bin = gap;
	}
	if (start < PANE_BINS)
	{
		auto const from{low + static_cast<float>(start) * binwidth};
		if (high - from >= PANE_LEAST_WIDTH)
			panes.emplace_back(from, high);
	}
	return panes;
}

// the mask of one large blade over a windscreen, put in the channel Channel of Mask: its pivot under the middle of the bottom edge,
// long enough to reach the top corners, sweeping across the windscreen
void lay_blade(glass_plane const &Plane, std::vector<bool> const &On, glm::vec2 const &Pane, glm::length_t const Channel, std::vector<glm::vec4> &Mask)
{
	auto bottom{std::numeric_limits<float>::max()};
	auto top{std::numeric_limits<float>::lowest()};
	auto const inside = [&Plane, &On, &Pane](std::size_t const Texel) { return On[Texel] && (Plane.flat[Texel].x >= Pane.x) && (Plane.flat[Texel].x <= Pane.y); };
	for (std::size_t texel = 0; texel < On.size(); ++texel)
	{
		if (false == inside(texel))
			continue;
		bottom = std::min(bottom, Plane.flat[texel].y);
		top = std::max(top, Plane.flat[texel].y);
	}
	if (top <= bottom)
	{
		return;
	}
	glm::vec2 const pivot{0.5f * (Pane.x + Pane.y), bottom - WIPER_PIVOT_BELOW * (top - bottom)};
	auto const reach{WIPER_REACH * glm::length(glm::vec2{0.5f * (Pane.y - Pane.x), top - pivot.y})};
	auto const arm{WIPER_ARM * reach};
	for (std::size_t texel = 0; texel < On.size(); ++texel)
	{
		auto const distance{glm::length(Plane.flat[texel] - pivot)};
		auto const angle{angle_about(Plane.flat[texel], pivot)};
		if ((false == inside(texel)) || (distance < arm) || (distance > reach) || (angle > WIPER_REST) || (angle < WIPER_END))
		{
			continue;
		}
		Mask[texel][Channel] = std::max((WIPER_REST - angle) / (WIPER_REST - WIPER_END), 0.001f);
	}
}

} // namespace

bool cab_windows_heated()
{
	auto const *vehicle{simulation::Train != nullptr ? simulation::Train->Dynamic() : nullptr};
	return (vehicle == nullptr) || vehicle->MoverParameters->windows_heated();
}

float glass_precipitation()
{
	if (Global.Weather != "snow:")
	{
		return 0.f;
	}
	return cab_windows_heated() ? 1.f : 2.f;
}

bool windscreen_water::available()
{
	if (m_state == 0)
	{
		m_state = (gpu_init() ? 1 : -1);
		if (m_state < 0)
		{
			WriteLog("windscreen water: the simulation can't be used, the windscreens make their water up");
		}
	}
	return m_state > 0;
}

void windscreen_water::release()
{
	assign(false);
	m_glasses = {};
	m_glasscount = 0;
	m_vehicle = nullptr;
	m_cab = nullptr;
}

void windscreen_water::reset()
{
	release();
	m_state = 0;
}

void windscreen_water::assign(bool const Assigned)
{
	if (false == bool(m_host.material))
	{
		return;
	}
	for (std::size_t slot = 0; slot < m_glasscount; ++slot)
	{
		auto const &glass{m_glasses[slot]};
		if (auto *material{m_host.material(glass.material)}; is_glass(material))
		{
			material->params[SLOT_PARAM].z = (Assigned && glass.baked ? static_cast<float>(slot + 1) : 0.f);
		}
	}
}

void windscreen_water::rasterize(TSubModel *Submodel, material_handle const Material, glm::vec3 const &Inside, std::vector<glm::vec4> &Axes, std::vector<glm::vec3> *Places, glm::vec4 const &Outer) const
{
	if (Submodel->m_material != Material)
	{
		return;
	}
	// Outer: an axis (x, y, z) and the least distance from the middle along it a triangle is laid at; w below zero lays them all
	visit_triangles(Submodel, [&Inside, &Axes, Places, &Outer](std::array<glm::vec3, 3> const &Position, std::array<glm::vec2, 3> const &Uv) {
		if (Outer.w >= 0.f)
		{
			auto const axis{static_cast<glm::length_t>(Outer.w)};
			if (std::abs((Position[0][axis] + Position[1][axis] + Position[2][axis]) / 3.f - Inside[axis]) < Outer[axis])
				return;
		}
		lay_triangle(Position, Uv, Inside, Axes, Places);
	});
}

namespace
{
// the outside of the cab a glass found by its shape lies at: the axis along which it faces (z for a windscreen, x for a side
// window) and the least distance from the middle along it its triangles are taken at, as rasterize() takes it
glm::vec4 outer_of(TSubModel *Root, material_handle const Material, glm::vec3 const &Inside, bool const Ahead)
{
	glm::length_t const axis{Ahead ? 2 : 0};
	auto furthest{0.f};
	visit_submodels(Root, [Material, &Inside, axis, &furthest](TSubModel *Submodel) {
		if (Submodel->m_material != Material)
			return;
		visit_triangles(Submodel, [&Inside, axis, &furthest](std::array<glm::vec3, 3> const &Position, std::array<glm::vec2, 3> const &) {
			furthest = std::max(furthest, std::abs((Position[0][axis] + Position[1][axis] + Position[2][axis]) / 3.f - Inside[axis]));
		});
	});
	glm::vec4 outer{0.f};
	outer[axis] = std::max(furthest - (Ahead ? FOUND_DEPTH_AHEAD : FOUND_DEPTH_ASIDE), 0.f);
	outer.w = static_cast<float>(axis);
	return outer;
}
} // namespace

void windscreen_water::build(TModel3d &Cab)
{
	auto *root{Cab.GetSMRoot()};
	if ((root == nullptr) || (false == bool(m_host.material)))
	{
		return;
	}
	// the materials of the glasses, in the order they come in the model; and a point inside the cab, the middle of the box
	// around the whole model (the glasses are around it)
	std::vector<material_handle> materials;
	glm::vec3 lower{std::numeric_limits<float>::max()};
	glm::vec3 upper{std::numeric_limits<float>::lowest()};
	visit_submodels(root, [&](TSubModel *Submodel) {
		auto const &vertices{submodel_vertices(Submodel)};
		if (false == vertices.empty())
		{
			auto const transform{submodel_transform(Submodel)};
			for (auto const &vertex : vertices)
			{
				auto const position{glm::vec3{transform * glm::vec4{vertex.position, 1.f}}};
				lower = glm::min(lower, position);
				upper = glm::max(upper, position);
			}
		}
		auto const material{Submodel->m_material};
		if ((material != null_handle) && (material > 0) && (Submodel->eType == GL_TRIANGLES) && (std::find(materials.begin(), materials.end(), material) == materials.end()) && is_glass(m_host.material(material)))
		{
			materials.push_back(material);
		}
	});
	// a cab without glasses of rain_windscreen: with gfx.windscreen.auto they are told by their shape
	auto const found{materials.empty() && Global.gfx_windscreen_auto};
	if (found)
	{
		materials = find_glasses(root, lower, upper, FOUND_LEAST_AREA);
	}
	if (materials.empty())
	{
		return;
	}
	if (materials.size() > gl::WATER_SLOTS)
	{
		WriteLog("windscreen water: the cab has " + std::to_string(materials.size()) + " glasses, the water of the first " + std::to_string(gl::WATER_SLOTS) + " is simulated");
		materials.resize(gl::WATER_SLOTS);
	}
	auto const inside{(lower + upper) * 0.5f};
	auto const texels{static_cast<std::size_t>(gl::WATER_MAP) * gl::WATER_MAP};
	auto const size{static_cast<int>(gl::WATER_MAP)};
	std::vector<found_glass> foundglasses;
	for (auto const material : materials)
	{
		std::vector<glm::vec4> axes(texels * 2, glm::vec4{0.f});
		std::vector<glm::vec3> places(found ? texels : 0);
		auto const kind{m_foundahead.find(material)};
		auto const outer{(found && (kind != m_foundahead.end())) ? outer_of(root, material, inside, kind->second) : glm::vec4{0.f, 0.f, 0.f, -1.f}};
		visit_submodels(root, [this, material, &inside, &axes, &places, found, &outer](TSubModel *Submodel) { rasterize(Submodel, material, inside, axes, found ? &places : nullptr, outer); });
		auto &glass{m_glasses[m_glasscount]};
		glass = windscreen_water::glass{};
		glass.material = material;
		glass.found = found;
		auto const *opengl{m_host.material(material)};
		glass.mask = opengl->textures[opengl->shader->texture_conf.at("wipermask").id];
		for (std::size_t texel = 0; texel < texels; ++texel)
		{
			if (axes[texel * 2].w > 0.f)
			{
				auto const density{glm::length(glm::cross(glm::vec3{axes[texel * 2]}, glm::vec3{axes[texel * 2 + 1]}))};
				glass.densest = std::max(glass.densest, density);
				glass.area += density / static_cast<float>(texels);
			}
		}
		if (glass.densest <= 0.f)
		{
			continue;
		}
		// the edges spread out by two texels, so the filtering of the map near them takes the glass along
		for (int pass = 0; pass < 2; ++pass)
		{
			auto const source{axes};
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
				{
					auto const texel{static_cast<std::size_t>(y * size + x)};
					if (source[texel * 2].w > 0.f)
					{
						continue;
					}
					for (int k = 0; k < 8; ++k)
					{
						static int const dx[]{1, -1, 0, 0, 1, 1, -1, -1};
						static int const dy[]{0, 0, 1, -1, 1, -1, 1, -1};
						auto const nx{x + dx[k]};
						auto const ny{y + dy[k]};
						if ((nx < 0) || (ny < 0) || (nx >= size) || (ny >= size))
						{
							continue;
						}
						auto const neighbour{static_cast<std::size_t>(ny * size + nx)};
						if (source[neighbour * 2].w > 0.f)
						{
							axes[texel * 2] = glm::vec4{glm::vec3{source[neighbour * 2]}, 0.5f};
							axes[texel * 2 + 1] = source[neighbour * 2 + 1];
							break;
						}
					}
				}
			}
		}
		gpu_setup(m_glasscount, axes);
		WriteLog("windscreen water: glass " + std::to_string(m_glasscount + 1) + " of the cab, material " + opengl->GetName() + ", " + std::to_string(glass.area) + " m2" + (found ? " (found by its shape)" : ""));
		if (found)
		{
			auto &entry{foundglasses.emplace_back()};
			entry.slot = m_glasscount;
			entry.places = std::move(places);
			entry.on.resize(texels);
			for (std::size_t texel = 0; texel < texels; ++texel)
			{
				// the wipers go over the part facing ahead or back, not the side windows of the same material
				auto const cross{glm::cross(glm::vec3{axes[texel * 2]}, glm::vec3{axes[texel * 2 + 1]})};
				entry.on[texel] = (axes[texel * 2].w >= 1.f) && (std::abs(cross.z) >= WIPED_FACING * glm::length(cross));
			}
			auto plane{plane_of(axes, entry.on)};
			entry.planar = plane.valid;
			entry.flat = std::move(plane.flat);
		}
		++m_glasscount;
	}
	if (false == foundglasses.empty())
	{
		lay_wipers(foundglasses);
	}
}

std::vector<material_handle> windscreen_water::find_glasses(TSubModel *Root, glm::vec3 const &Lower, glm::vec3 const &Upper, float const Least)
{
	if (false == bool(m_host.create))
	{
		return {};
	}
	auto const inside{(Lower + Upper) * 0.5f};
	auto const halflength{std::max((Upper.z - Lower.z) * 0.5f, 0.01f)};
	auto const halfwidth{std::max((Upper.x - Lower.x) * 0.5f, 0.01f)};
	// the shapes of the translucent submodels, translucent as the model was loaded: the textures, which tell it too, are mostly
	// not read yet
	std::vector<part_shape> parts;
	visit_submodels(Root, [&parts](TSubModel *Submodel) {
		if ((Submodel->m_material <= 0) || ((Submodel->iFlags & 0x20) == 0))
			return;
		auto &part{parts.emplace_back()};
		part.submodel = Submodel;
		visit_triangles(Submodel, [&part](std::array<glm::vec3, 3> const &Position, std::array<glm::vec2, 3> const &) {
			auto const cross{glm::cross(Position[1] - Position[0], Position[2] - Position[0])};
			auto const area{0.5f * glm::length(cross)};
			if (area <= 0.f)
				return;
			part.area += area;
			part.centre += area * (Position[0] + Position[1] + Position[2]) / 3.f;
			if (std::abs(cross.z) >= FOUND_FACING * 2.f * area)
				part.ahead += area;
			else if (std::abs(cross.x) >= FOUND_FACING * 2.f * area)
				part.aside += area;
		});
	});
	auto const at_end = [&inside, halflength](part_shape const &Part) { return (Part.area > 0.f) && (std::abs(Part.centre.z / Part.area - inside.z) >= FOUND_OUTER * halflength); };
	auto const at_side = [&inside, halfwidth](part_shape const &Part) { return (Part.area > 0.f) && (std::abs(Part.centre.x / Part.area - inside.x) >= FOUND_SIDE * halfwidth); };
	// the windscreens: the material with the most of its area facing ahead or back at an end of the cab, the glasses of the cab
	// mostly being one material, unlike the lit panels and screens of the desk
	std::unordered_map<material_handle, float> windscreens;
	for (auto const &part : parts)
	{
		auto &area{windscreens[part.submodel->m_material]};
		if (at_end(part) && (part.ahead >= FOUND_PART_AHEAD))
			area += part.ahead;
	}
	material_handle best{null_handle};
	float bestarea{0.f};
	for (auto const &[material, area] : windscreens)
	{
		if ((area >= Least) && (area > bestarea))
		{
			best = material;
			bestarea = area;
		}
	}
	if (best == null_handle)
	{
		return {};
	}
	// of it the windscreens (with the wipers) and the side windows (without) are given materials of rain_windscreen of their own,
	// so the two have maps of their own even where they share the texture; the glasses inside the vehicle stay as they are
	std::vector<TSubModel *> ahead;
	std::vector<TSubModel *> aside;
	for (auto const &part : parts)
	{
		if (part.submodel->m_material != best)
			continue;
		if ((part.ahead >= 0.5f * part.area) && (part.ahead >= FOUND_PART_AHEAD) && at_end(part))
			ahead.push_back(part.submodel);
		else if ((part.aside >= 0.5f * part.area) && (part.aside >= FOUND_PART_ASIDE) && at_side(part))
			aside.push_back(part.submodel);
	}
	std::vector<material_handle> result;
	for (auto const *group : {&ahead, &aside})
	{
		if (group->empty())
			continue;
		auto const glass{glass_material(best, group == &ahead ? "ahead" : "aside")};
		if (glass == null_handle)
			continue;
		for (auto *submodel : *group)
			submodel->m_material = glass;
		m_foundahead[glass] = (group == &ahead);
		result.push_back(glass);
	}
	return result;
}

material_handle windscreen_water::glass_material(material_handle const Source, std::string const &Kind)
{
	auto const *source{m_host.material(Source)};
	if ((source == nullptr) || (source->textures[0] == null_handle))
	{
		return null_handle;
	}
	if (m_nowipers == null_handle)
	{
		// a mask of the wipers with no blade anywhere: the drawing reads the mask the simulation has, worked out from the glass
		m_nowipers = GfxRenderer->Fetch_Texture("internal_src:windscreen/nowipers", true);
		if (m_nowipers == null_handle)
		{
			return null_handle;
		}
		std::array<std::uint8_t, 4 * 4 * 4> const none{};
		GfxRenderer->Texture(m_nowipers).make_from_memory(4, 4, none.data());
	}
	auto const diffuse{std::string{GfxRenderer->Texture(source->textures[0]).get_name()}};
	auto const definition{"shader: rain_windscreen\n"
	                      "texture_diffuse: " +
	                      diffuse +
	                      "\n"
	                      "texture_raindropsatlas: textures/fx/raindrops-atlas\n"
	                      "texture_wipermask: internal_src:windscreen/nowipers\n"};
	auto const glass{m_host.create("internal_src:windscreen/" + Kind + "/" + source->GetName(), definition)};
	if ((glass == null_handle) || (false == is_glass(m_host.material(glass))))
	{
		ErrorLog("windscreen water: no material of rain_windscreen for the glass " + source->GetName());
		return null_handle;
	}
	return glass;
}

void windscreen_water::lay_wipers(std::vector<found_glass> const &Glasses)
{
	// the windscreens of all the glasses from the left to the right of the cab (x of the model): a blade each, in that order
	struct pane
	{
		std::size_t glass;
		glm::vec2 range;
		float across;
	};
	std::vector<glass_plane> planes;
	std::vector<pane> panes;
	for (std::size_t i = 0; i < Glasses.size(); ++i)
	{
		auto const &glass{Glasses[i]};
		auto const &plane{planes.emplace_back(glass_plane{glass.planar, glass.flat})};
		for (auto const &range : panes_of(plane, glass.on))
		{
			auto middle{0.f};
			std::size_t count{0};
			for (std::size_t texel = 0; texel < glass.on.size(); ++texel)
			{
				if (glass.on[texel] && (plane.flat[texel].x >= range.x) && (plane.flat[texel].x <= range.y))
				{
					middle += glass.places[texel].x;
					++count;
				}
			}
			panes.push_back({i, range, middle / static_cast<float>(std::max<std::size_t>(count, 1))});
		}
	}
	std::ranges::sort(panes, [](pane const &A, pane const &B) { return A.across < B.across; });
	auto const own{m_vehicle != nullptr ? m_vehicle->dWiperPos.size() : 0};
	// a vehicle without wipers of its own gets virtual ones, run by its wiper switch
	m_virtualwipers = (own == 0);
	auto const blades{std::min<std::size_t>({own == 0 ? panes.size() : own, panes.size(), 4})};
	m_virtualblades = m_virtualwipers ? blades : 0;
	std::vector<std::vector<glm::vec4>> masks(Glasses.size(), std::vector<glm::vec4>(static_cast<std::size_t>(gl::WATER_MAP) * gl::WATER_MAP, glm::vec4{0.f}));
	for (std::size_t blade = 0; blade < blades; ++blade)
	{
		auto const &entry{panes[blade]};
		lay_blade(planes[entry.glass], Glasses[entry.glass].on, entry.range, static_cast<glm::length_t>(blade), masks[entry.glass]);
	}
	for (std::size_t i = 0; i < Glasses.size(); ++i)
	{
		gpu_blades(Glasses[i].slot, masks[i]);
		m_glasses[Glasses[i].slot].baked = true;
	}
	WriteLog("windscreen water: " + std::to_string(panes.size()) + " windscreens found by their shape, " + std::to_string(blades) + (m_virtualwipers ? " virtual" : "") +
	         " wiper blades laid over them");
}

void windscreen_water::update(double const Seconds)
{
	if (false == available())
	{
		return;
	}
	auto const *vehicle{simulation::Train != nullptr ? simulation::Train->Dynamic() : nullptr};
	auto *cab{vehicle != nullptr ? vehicle->mdKabina : nullptr};
	if ((vehicle != m_vehicle) || (cab != m_cab))
	{
		// another cab: its glasses dry
		release();
		m_vehicle = vehicle;
		m_cab = cab;
		m_bladesbefore = glm::vec4{-1.f};
		if (cab != nullptr)
		{
			build(*cab);
		}
		if (Global.gfx_windscreen_auto && (vehicle != nullptr) && (vehicle->mdModel != nullptr))
		{
			dress_outside(*vehicle->mdModel);
		}
	}
	if ((vehicle == nullptr) || (m_glasscount == 0))
	{
		// no glasses without a cab, which comes with the vehicle
		return;
	}

	gl::water_ubs block{};
	// the blades of the wipers of the vehicle, as the glasses read them (the channels of the mask follow the wipers of the cab
	// the vehicle is driven from)
	glm::vec4 blades{-1.f};
	if (auto const cabactive{vehicle->MoverParameters->CabActive}; cabactive != 0)
	{
		auto const count{static_cast<int>(vehicle->dWiperPos.size())};
		for (int i = 0; i < 4; ++i)
		{
			if (i < count)
			{
				auto const index{cabactive > 0 ? i : count - 1 - i};
				blades[i] = static_cast<float>(std::clamp(vehicle->dWiperPos[index], 0.0, 1.0));
			}
		}
		if ((count == 0) && m_virtualwipers)
		{
			auto const position{virtual_wipers(*vehicle->MoverParameters, Seconds)};
			for (std::size_t i = 0; i < m_virtualblades; ++i)
				blades[static_cast<glm::length_t>(i)] = position;
		}
	}
	// the space of the model of the cab, drawn with the matrix of the vehicle
	auto const toworld{glm::dmat3{vehicle->mMatrix}};
	auto const tomodel{glm::inverse(toworld)};
	auto const up{glm::normalize(glm::vec3{tomodel * glm::dvec3{0.0, 1.0, 0.0}})};
	auto const air{glm::vec3{tomodel * (glm::dvec3{simulation::Environment.wind()} * WIND_SHARE - vehicle->VectorFront() * vehicle->MoverParameters->V)}};
	auto const heavy{std::clamp(Global.Overcast - 1.f, 0.f, 1.f)};
	auto const snow{Global.Weather == "snow:"};
	auto const fall{snow ? SNOW_FALL : RAIN_FALL};

	// the masks of the wipers baked into the maps once the textures are there
	for (std::size_t slot = 0; slot < m_glasscount; ++slot)
	{
		auto &glass{m_glasses[slot]};
		if (false == glass.baked)
		{
			block.counts = glm::uvec4{0u, 0u, static_cast<std::uint32_t>(slot), 0u};
			glass.baked = gpu_bake(slot, glass.mask, block);
		}
	}
	assign(true);

	auto const seconds{std::clamp(Seconds, 0.0, LONGEST_STEP)};
	if (seconds <= 0.0)
	{
		return;
	}
	m_time += seconds;
	++m_step;

	// the drops landing: as many raindrops a second come to a square metre of the ground as the rain gives water (from light rain at
	// 1 mm/h to a downpour at 30 mm/h) in the drops of the Marshall-Palmer distribution above the least landing as a drop of its own
	// (exponential, the mean diameter falling with the rain); a glass gets them in the share it faces the rain coming at it
	// against the cab (water_spawn.comp)
	float perground{0.f}; // the drops coming to a square metre of the ground in a second
	auto const rate{1.f + 29.f * std::pow(heavy, 1.5f)}; // mm/h
	auto const lambda{4.1f * std::pow(rate, -0.21f)}; // 1 / mm
	if (heavy > 0.f)
	{
		auto const water{rate / 3600.f * 1e6f}; // mm3 a second on a square metre
		if (snow)
		{
			perground = water * SNOW_WATER / FLAKE_WATER;
		}
		else
		{
			// the share of the water in the drops above the least one, and their mean volume
			auto const x{lambda * LEAST_DROP};
			auto const share{std::exp(-x) * (1.f + x + x * x / 2.f + x * x * x / 6.f)};
			auto const d0{LEAST_DROP};
			auto const mean{0.5236f * (d0 * d0 * d0 + 3.f * d0 * d0 / lambda + 6.f * d0 / (lambda * lambda) + 6.f / (lambda * lambda * lambda))};
			perground = water * share / mean;
		}
	}
	// the landings tried on a glass: as if the whole of its uv held the glass at its densest, the ones at the less dense places and
	// facing the rain less are left out. the share of a landing left over goes to the next step
	auto const coming{glm::length(air - up * fall)};
	std::uint32_t most{0};
	m_tries = 0;
	for (std::size_t slot = 0; slot < m_glasscount; ++slot)
	{
		auto &glass{m_glasses[slot]};
		auto const *material{m_host.material(glass.material)};
		auto const simulated{glass.baked && is_glass(material)};
		glass.landings += static_cast<double>(perground / fall * coming * glass.densest) * seconds;
		auto const whole{std::floor(glass.landings)};
		glass.landings -= whole;
		auto const tries{simulated ? static_cast<std::uint32_t>(std::min<double>(whole, MOST_TRIES)) : 0u};
		most = std::max(most, tries);
		m_tries += tries;
		auto const scale{(material != nullptr) && std::isfinite(material->params[SLOT_PARAM].y) ? std::max(material->params[SLOT_PARAM].y, 0.05f) : 1.f};
		block.slots[slot] = glm::vec4{static_cast<float>(tries), 1.f / glass.densest, simulated ? 1.f : 0.f, scale};
	}

	block.step = glm::vec4{static_cast<float>(seconds), static_cast<float>(m_time), heavy, snow ? 1.f : 0.f};
	block.up = glm::vec4{up, Global.AirTemperature};
	block.air = glm::vec4{air, fall};
	block.blades = blades;
	block.blades_before = glm::vec4{
	    m_bladesbefore.x < 0.f ? blades.x : m_bladesbefore.x, m_bladesbefore.y < 0.f ? blades.y : m_bladesbefore.y, m_bladesbefore.z < 0.f ? blades.z : m_bladesbefore.z, m_bladesbefore.w < 0.f ? blades.w : m_bladesbefore.w};
	// w: 1 when the glasses aren't heated, the flakes on them melting only in the air
	block.rain = glm::vec4{LEAST_DROP, lambda, 1.f, vehicle->MoverParameters->windows_heated() ? 0.f : 1.f};
	block.counts = glm::uvec4{m_step * 2654435761u, most, 0u, 0u};
	m_bladesbefore = blades;
	gpu_step(block, most);
}

void windscreen_water::dress_outside(TModel3d &Model)
{
	if (std::ranges::find(m_dressed, &Model) != m_dressed.end())
		return;
	m_dressed.push_back(&Model);
	auto *root{Model.GetSMRoot()};
	if (root == nullptr)
		return;
	glm::vec3 lower;
	glm::vec3 upper;
	model_box(root, lower, upper);
	auto const glasses{find_glasses(root, lower, upper, FOUND_OUTSIDE_AREA)};
	if (false == glasses.empty())
		WriteLog("windscreen water: " + std::to_string(glasses.size()) + " windscreens of the vehicle seen from outside found by their shape");
}

float windscreen_water::virtual_wipers(TMoverParameters const &Mover, double const Seconds)
{
	// a sweep there and back runs on; switched off, the blades finish it and rest
	auto const running{(Mover.wiperSwitchPos > 0) && Mover.Battery};
	if (running || (m_wiperphase > 0.0))
	{
		m_wiperphase += std::max(Seconds, 0.0) / (Mover.wiperSwitchPos >= 2 ? VIRTUAL_FAST : VIRTUAL_SLOW);
		if (m_wiperphase >= 1.0)
			m_wiperphase = running ? m_wiperphase - std::floor(m_wiperphase) : 0.0;
	}
	return static_cast<float>(m_wiperphase < 0.5 ? 2.0 * m_wiperphase : 2.0 - 2.0 * m_wiperphase);
}

std::string windscreen_water::info() const
{
	if ((m_state <= 0) || (m_glasscount == 0))
	{
		return {};
	}
	return "windscreen water: " + std::to_string(m_glasscount) + " glasses, " + std::to_string(m_tries) + " drops landing tried in the frame\n";
}

} // namespace gfx
