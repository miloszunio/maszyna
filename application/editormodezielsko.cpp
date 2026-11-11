/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// ZIELSKO z kosmosu (ficzery2): the trees of the 3D tree models of geoportal.gov.pl planted along the tracks of the scenery.
// The counties under the scenery are asked of the WMS of the models, their packages read in a thread of their own and the
// trees near the tracks kept beside the terrain (editor/editorZielsko.cpp); here the trees kept are sifted (the band from the
// tracks, the roads, the water, the outline, a spacing, a limit) and planted as models of the node bank, scaled to their height

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editormodeshared.h"
#include "application/editoruilayer.h"
#include "application/editorprojection.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"
#include "model/AnimModel.h"
#include "model/Model3d.h"
#include "simulation/simulation.h"
#include "world/Track.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <set>
#include <unordered_map>

namespace
{

char const *const zielsko_prefix{"zielsko_"};

bool starts_with(std::string const &Text, char const *Prefix)
{
	return Text.rfind(Prefix, 0) == 0;
}

std::string lower(std::string Text)
{
	std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
	return Text;
}

// points along the paths of the tracks of a kind (1 rail, 2 road), every Step metres, with the half of their width
struct track_points
{
	static constexpr double bucket{50.0};
	struct point
	{
		double x, z;
		float half;
	};
	std::unordered_map<std::uint64_t, std::vector<point>> buckets;
	float widest{0.0f};

	static std::uint64_t key(std::int64_t const X, std::int64_t const Z) { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(X)) << 32) | static_cast<std::uint32_t>(Z); }
	void add(double const X, double const Z, float const Half)
	{
		buckets[key(static_cast<std::int64_t>(std::floor(X / bucket)), static_cast<std::int64_t>(std::floor(Z / bucket)))].push_back({X, Z, Half});
		widest = std::max(widest, Half);
	}
	void gather(int const Category, double const Step)
	{
		for (auto *track : simulation::Paths.sequence())
		{
			if (track == nullptr || track->m_editorremoved || track->m_paths.empty() || (track->iCategoryFlag & Category) == 0)
				continue;
			auto const half{0.5f * std::max(track->fTrackWidth, track->fTrackWidth2)};
			for (auto const &path : track->m_paths)
			{
				geometry::bezier const curve{path};
				auto const length{curve.plan_length()};
				auto const steps{std::max(1, static_cast<int>(std::ceil(length / Step)))};
				for (int i = 0; i <= steps; ++i)
				{
					auto const p{curve.point(static_cast<double>(i) / steps)};
					add(p.x, p.z, Category == 1 ? 0.0f : half);
				}
			}
		}
	}
	bool empty() const { return buckets.empty(); }
	// the distance in the plan to the nearest point within Reach, less its half width; Reach+1 if none is that near
	double distance(double const X, double const Z, double const Reach) const
	{
		auto best{Reach + 1.0};
		auto const range{static_cast<int>(std::ceil((Reach + widest) / bucket))};
		auto const bx{static_cast<std::int64_t>(std::floor(X / bucket))};
		auto const bz{static_cast<std::int64_t>(std::floor(Z / bucket))};
		for (auto dx = -range; dx <= range; ++dx)
			for (auto dz = -range; dz <= range; ++dz)
			{
				auto const found{buckets.find(key(bx + dx, bz + dz))};
				if (found == buckets.end())
					continue;
				for (auto const &p : found->second)
					best = std::min(best, std::hypot(p.x - X, p.z - Z) - p.half);
			}
		return best;
	}
};

} // namespace

std::string editor_mode::zielsko_folder() const
{
	return m_streamer.directory() + "zielsko/";
}

bool editor_mode::zielsko_in_scope(glm::dvec2 const &At) const
{
	auto const &settings{m_zielsko};
	if (settings.scope == 0)
		return glm::distance(At, glm::dvec2{Camera.Pos.x, Camera.Pos.z}) <= settings.radius;
	if (settings.scope == 2)
	{
		auto const &outline{m_zielsko_outline};
		return outline.size() >= 3 && geometry::point_in_polygon(outline, At);
	}
	return true;
}

void editor_mode::zielsko_harvest_start(bool const Whole, bool const PlantAfter)
{
	auto const &manifest{m_streamer.manifest()};
	if (false == manifest.georeferenced)
	{
		m_zielsko_status = STR_C("The terrain doesn't know where it lies (PUWG 1992): set its origin first");
		return;
	}
	auto const east0{manifest.origin_east};
	auto const north0{manifest.origin_north};
	auto const &settings{m_zielsko};
	if (settings.scope == 2)
		m_zielsko_outline = shaped_outline();
	if (settings.scope == 2 && m_zielsko_outline.size() < 3)
	{
		m_zielsko_status = STR_C("Draw the outline first");
		return;
	}
	// the points to be covered: every 400 m of the area (the sheets are about 1.1 km; a sheet across the border of two counties
	// is in the packages of both); the whole scenery: every kilometre over the chunks of the terrain
	std::vector<zielsko::probe> probes;
	auto const add = [&probes, east0, north0](double const X, double const Z) {
		auto const east{east0 - X};
		auto const north{north0 + Z};
		probes.push_back({east, north, zielsko::sheet_of(east, north)});
	};
	auto const whole{Whole || settings.scope == 1};
	if (whole)
	{
		std::set<std::pair<long long, long long>> cells;
		for (auto const &[key, info] : m_streamer.stored_chunks())
		{
			auto const corner{heightmap::chunk_corner(key)};
			auto const centre{corner + glm::dvec2{heightmap::chunk_size * 0.5, heightmap::chunk_size * 0.5}};
			// on a grid of 1 km in PUWG 1992, so the answers of the WMS kept come back for the same points
			auto const e{std::floor((east0 - centre.x) / 1000.0) * 1000.0 + 500.0};
			auto const n{std::floor((north0 + centre.y) / 1000.0) * 1000.0 + 500.0};
			if (cells.insert({std::llround(e), std::llround(n)}).second)
				add(east0 - e, n - north0);
		}
	}
	else
	{
		// the box of the area, on a grid of 400 m in PUWG 1992
		glm::dvec2 low, high;
		if (settings.scope == 0)
		{
			low = glm::dvec2{Camera.Pos.x, Camera.Pos.z} - static_cast<double>(settings.radius);
			high = glm::dvec2{Camera.Pos.x, Camera.Pos.z} + static_cast<double>(settings.radius);
		}
		else
		{
			low = high = m_zielsko_outline.front();
			for (auto const &p : m_zielsko_outline)
			{
				low = glm::min(low, p);
				high = glm::max(high, p);
			}
		}
		constexpr double step{400.0};
		for (auto e = std::floor((east0 - high.x) / step) * step; e <= east0 - low.x + step; e += step)
			for (auto n = std::floor((north0 + low.y) / step) * step; n <= north0 + high.y + step; n += step)
			{
				// the point of the grid, or for a cell on the edge the nearest point of the area in it
				glm::dvec2 at{east0 - e, n - north0};
				if (false == zielsko_in_scope(at))
				{
					auto const nearest{glm::clamp(at, low, high)};
					auto inside{false};
					for (int i = -1; i <= 1 && false == inside; ++i)
						for (int j = -1; j <= 1 && false == inside; ++j)
						{
							glm::dvec2 const nearby{at.x + i * step * 0.5, at.y + j * step * 0.5};
							if (zielsko_in_scope(nearby))
								inside = true;
						}
					if (false == inside && false == zielsko_in_scope(nearest))
						continue;
				}
				add(at.x, at.y);
			}
	}
	if (probes.empty())
	{
		m_zielsko_status = STR_C("The area has no terrain");
		return;
	}
	m_zielsko_plant_after = PlantAfter;
	m_zielsko_harvest.start(zielsko_folder(), std::move(probes), whole);
	m_zielsko_status.clear();
}

void editor_mode::zielsko_refresh_counties()
{
	m_zielsko_counties.clear();
	std::error_code error;
	for (auto const &entry : std::filesystem::directory_iterator{zielsko_folder(), error})
		if (entry.path().extension() == ".drz")
		{
			zielsko::county_cache cache;
			if (false == zielsko::read_cache(entry.path().string(), cache))
				continue;
			auto const teryt{entry.path().stem().string()};
			std::string state;
			if (cache.complete)
				state = format(STR_C("%s: read whole, %zu trees"), teryt.c_str(), cache.trees.size());
			else if (cache.band && cache.sheets.empty())
				state = format(STR_C("%s: only the band along the tracks (the first version), %zu trees"), teryt.c_str(), cache.trees.size());
			else
				state = format(STR_C("%s: %zu sheets read, %zu trees"), teryt.c_str(), cache.sheets.size(), cache.trees.size());
			m_zielsko_counties.push_back(std::move(state));
		}
	std::sort(m_zielsko_counties.begin(), m_zielsko_counties.end());
}

std::vector<editor_mode::zielsko_candidate> editor_mode::zielsko_candidates(std::size_t &Counties, std::size_t &Read)
{
	std::vector<zielsko_candidate> result;
	Counties = 0;
	Read = 0;
	auto const &manifest{m_streamer.manifest()};
	if (false == manifest.georeferenced)
		return result;
	auto const east0{manifest.origin_east};
	auto const north0{manifest.origin_north};
	auto const &settings{m_zielsko};
	// the trees kept, all the counties; a tree of a sheet across a border, given by both, once
	std::vector<zielsko::tree> trees;
	std::unordered_set<std::uint64_t> seen;
	std::error_code error;
	for (auto const &entry : std::filesystem::directory_iterator{zielsko_folder(), error})
		if (entry.path().extension() == ".drz")
		{
			++Counties;
			zielsko::county_cache cache;
			if (false == zielsko::read_cache(entry.path().string(), cache))
				continue;
			++Read;
			for (auto const &t : cache.trees)
				if (seen.insert(zielsko::metre_of(t.east, t.north)).second)
					trees.push_back(t);
		}
	m_zielsko_sieve.clear();
	if (trees.empty())
		return result;
	// how many each step takes out: told in the status and the log, so a sieve that leaves nothing says why
	std::size_t low{0}, outside{0}, far{0}, near{0}, road{0}, water_{0}, crowded{0};
	track_points rails, roads;
	rails.gather(1, 4.0);
	roads.gather(2, 4.0);
	if (settings.scope == 2)
		m_zielsko_outline = shaped_outline();
	auto const &water{m_streamer.water()};
	auto const farthest{static_cast<double>(settings.farthest)};
	// the spacing kept between the trees, the taller first
	std::sort(trees.begin(), trees.end(), [](zielsko::tree const &A, zielsko::tree const &B) { return A.height > B.height; });
	std::unordered_map<std::uint64_t, std::vector<glm::dvec2>> placed;
	auto const spacing{static_cast<double>(settings.spacing)};
	auto const spacing_key = [&](double const X, double const Z) {
		auto const s{std::max(1.0, spacing)};
		return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(std::floor(X / s)))) << 32) |
		       static_cast<std::uint32_t>(static_cast<std::int64_t>(std::floor(Z / s)));
	};
	std::size_t index{0};
	for (auto const &t : trees)
	{
		if ((++index & 0x3fff) == 0)
			editor_busy_progress(0.6 * static_cast<double>(index) / static_cast<double>(trees.size()));
		if (t.height < settings.lowest)
		{
			++low;
			continue;
		}
		glm::dvec2 const at{east0 - t.east, t.north - north0};
		if (false == zielsko_in_scope(at))
		{
			++outside;
			continue;
		}
		auto const track{rails.distance(at.x, at.y, farthest)};
		if (track > farthest)
		{
			++far;
			continue;
		}
		if (track < settings.nearest)
		{
			++near;
			continue;
		}
		if (false == roads.empty() && roads.distance(at.x, at.y, settings.road) < settings.road)
		{
			++road;
			continue;
		}
		auto wet{false};
		for (auto const &body : water)
			if (geometry::point_in_polygon(body.outline, at))
			{
				wet = true;
				break;
			}
		if (wet)
		{
			++water_;
			continue;
		}
		if (spacing > 0.0)
		{
			auto too_near{false};
			auto const k{spacing_key(at.x, at.y)};
			auto const kx{static_cast<std::int32_t>(k >> 32)};
			auto const kz{static_cast<std::int32_t>(k & 0xffffffffu)};
			for (int dx = -1; dx <= 1 && false == too_near; ++dx)
				for (int dz = -1; dz <= 1 && false == too_near; ++dz)
				{
					auto const found{placed.find((static_cast<std::uint64_t>(static_cast<std::uint32_t>(kx + dx)) << 32) | static_cast<std::uint32_t>(kz + dz))};
					if (found == placed.end())
						continue;
					for (auto const &other : found->second)
						if (glm::distance(other, at) < spacing)
						{
							too_near = true;
							break;
						}
				}
			if (too_near)
			{
				++crowded;
				continue;
			}
			placed[k].push_back(at);
		}
		result.push_back({at, t.height, static_cast<float>(track)});
	}
	// the nearest to the tracks first, as far as the limit
	std::sort(result.begin(), result.end(), [](zielsko_candidate const &A, zielsko_candidate const &B) { return A.track < B.track; });
	std::size_t over_limit{0};
	if (result.size() > static_cast<std::size_t>(std::max(0, settings.limit)))
	{
		over_limit = result.size() - static_cast<std::size_t>(std::max(0, settings.limit));
		result.resize(static_cast<std::size_t>(std::max(0, settings.limit)));
	}
	m_zielsko_sieve = format(STR_C("Of %zu trees read: %zu too low, %zu outside the area, %zu too far from the tracks, %zu too near them, %zu by a road, "
	                               "%zu in water, %zu too close to another, %zu over the limit; %zu left"),
	                         trees.size(), low, outside, far, near, road, water_, crowded, over_limit, result.size());
	if (rails.empty())
		m_zielsko_sieve += STR_C(". No tracks found in the scenery: the distance to the tracks takes every tree out");
	WriteLog("ZIELSKO: " + m_zielsko_sieve);
	return result;
}

void editor_mode::zielsko_count()
{
	std::size_t counties, read;
	m_zielsko_preview = zielsko_candidates(counties, read);
	if (counties == 0)
		m_zielsko_status = STR_C("Nothing read yet: Plant reads what the area needs first");
	else
		m_zielsko_status = format(STR_C("%zu trees would be planted (from %zu of %zu counties read)"), m_zielsko_preview.size(), read, counties);
}

std::size_t editor_mode::zielsko_remove(bool const InScope)
{
	if (InScope && m_zielsko.scope == 2)
		m_zielsko_outline = shaped_outline();
	std::vector<TAnimModel *> found;
	for (auto *model : simulation::Instances.sequence())
		if (model != nullptr && starts_with(model->name(), zielsko_prefix) &&
		    (false == InScope || zielsko_in_scope(glm::dvec2{model->location().x, model->location().z})))
			found.push_back(model);
	for (auto *model : found)
	{
		if (m_node == model)
		{
			m_node = nullptr;
			ui()->set_node(nullptr);
		}
		erase_model(model);
	}
	return found.size();
}

void editor_mode::zielsko_plant()
{
	auto &settings{m_zielsko};
	// the templates of the groups of the node bank of the names given
	auto const templates = [&](char const *Group) {
		std::vector<std::string> result;
		auto &bank{ui()->nodebank()};
		auto const names{bank.group_names()};
		auto const wanted{lower(Group)};
		for (std::size_t i = 0; i < names.size(); ++i)
		{
			auto name{names[i]};
			name.erase(0, name.find_first_not_of(" \t"));
			name.erase(name.find_last_not_of(" \t\r") + 1);
			if (lower(name) != wanted)
				continue;
			for (auto const &entry : bank.group_templates(i))
				if (lower(entry).find("model") != std::string::npos)
					result.push_back(entry);
		}
		return result;
	};
	auto const tall_pool{templates(settings.trees_group)};
	auto low_pool{templates(settings.low_group)};
	if (low_pool.empty())
		low_pool = tall_pool;
	if (tall_pool.empty())
	{
		m_zielsko_status = format(STR_C("No group %s in the node bank: nothing to plant with"), settings.trees_group);
		WriteLog("ZIELSKO: " + m_zielsko_status);
		return;
	}
	std::size_t counties, read;
	auto const candidates{zielsko_candidates(counties, read)};
	m_zielsko_preview.clear();
	if (candidates.empty())
	{
		m_zielsko_status = counties == 0 ? std::string{STR_C("Nothing read yet: Plant reads what the area needs first")}
		                                 : std::string{STR_C("No tree passes the sieve")} + (m_zielsko_sieve.empty() ? std::string{} : ": " + m_zielsko_sieve);
		WriteLog("ZIELSKO: " + m_zielsko_status);
		return;
	}
	// the trees planted before within the area go first; the names go on after the highest one taken
	auto const removed{zielsko_remove(true)};
	std::size_t number{0};
	for (auto *model : simulation::Instances.sequence())
		if (model != nullptr && starts_with(model->name(), zielsko_prefix))
			number = std::max<std::size_t>(number, std::strtoull(model->name().c_str() + std::strlen(zielsko_prefix), nullptr, 10) + 1);
	std::mt19937 random{1992u};
	auto const uniform = [&](double const Low, double const High) { return std::uniform_real_distribution<double>{Low, High}(random); };
	// the height of each template: given, or about the radius of its model, which stands on its foot
	std::unordered_map<std::string, double> heights;
	std::size_t planted{0};
	std::size_t no_ground{0}, off_region{0}, no_model{0};
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		if ((i & 0xff) == 0)
			editor_busy_progress(0.6 + 0.4 * static_cast<double>(i) / static_cast<double>(candidates.size()));
		auto const &c{candidates[i]};
		double ground{0.0};
		if (false == m_streamer.height_at(c.at.x, c.at.y, ground, true))
		{
			++no_ground;
			continue;
		}
		glm::dvec3 const location{c.at.x, ground, c.at.y};
		if (false == simulation::Region->point_inside(location))
		{
			++off_region;
			continue;
		}
		auto const &pool{c.height < settings.small_below ? low_pool : tall_pool};
		auto const &src{pool[std::min(pool.size() - 1, static_cast<std::size_t>(uniform(0.0, 1.0) * pool.size()))]};
		auto *model{simulation::State.create_model(src, zielsko_prefix + std::to_string(number + planted), location)};
		if (model == nullptr)
		{
			++no_model;
			if (no_model == 1)
				WriteLog("ZIELSKO: the model of " + src + " can't be made");
			continue;
		}
		auto own{static_cast<double>(settings.model_height)};
		if (own <= 0.0)
		{
			auto const known{heights.find(src)};
			if (known != heights.end())
				own = known->second;
			else
			{
				own = model->Model() != nullptr ? model->Model()->bounding_radius() : 0.0;
				if (own < 1.0)
					own = 15.0;
				heights.emplace(src, own);
			}
		}
		auto const scale{std::clamp(c.height / own * uniform(1.0 - settings.jitter, 1.0 + settings.jitter), 0.15, 6.0)};
		auto const width{scale * uniform(1.0 - settings.jitter, 1.0 + settings.jitter)};
		m_editor.rotate(model, glm::vec3{0.0f, static_cast<float>(uniform(0.0, 360.0)), 0.0f}, 1);
		model->Scale(glm::vec3{static_cast<float>(width), static_cast<float>(scale), static_cast<float>(width)});
		++planted;
	}
	m_zielsko_status = format(STR_C("Planted %zu trees (%zu planted before within the area taken away). The source: GUGiK, 3D tree models, CC BY 4.0"), planted, removed);
	if (no_ground + off_region + no_model > 0)
		m_zielsko_status += format(STR_C(". Not planted: %zu with no terrain under them, %zu outside the scenery, %zu whose model couldn't be made"), no_ground,
		                           off_region, no_model);
	WriteLog("ZIELSKO: " + m_zielsko_status);
}

void editor_mode::draw_zielsko_preview(ImDrawList *Drawlist)
{
	if (m_zielsko_preview.empty())
		return;
	screen_projection const projection;
	glm::dvec2 const camera{Camera.Pos.x, Camera.Pos.z};
	std::size_t drawn{0};
	for (auto const &c : m_zielsko_preview)
	{
		if (glm::distance(c.at, camera) > 1500.0)
			continue;
		double ground{Camera.Pos.y};
		m_streamer.height_at(c.at.x, c.at.y, ground);
		ImVec2 dot;
		if (false == projection.project(glm::dvec3{c.at.x, ground + 0.5, c.at.y}, dot))
			continue;
		// low ones yellow, the tall ones deep green
		auto const t{std::clamp((c.height - 4.0f) / 26.0f, 0.0f, 1.0f)};
		auto const colour{IM_COL32(static_cast<int>(230 - 200 * t), static_cast<int>(220 - 60 * t), static_cast<int>(60 - 20 * t), 220)};
		Drawlist->AddCircleFilled(dot, 2.5f + 2.5f * t, colour);
		if (++drawn > 20000)
			break;
	}
}

void editor_mode::render_zielsko_ui()
{
	auto &settings{m_zielsko};
	// Plant waiting for the data of the area: planted once they've come
	if (m_zielsko_plant_after && false == m_zielsko_harvest.running())
	{
		m_zielsko_plant_after = false;
		zielsko_refresh_counties();
		if (m_zielsko_harvest.finished())
			run_busy(STR_C("Planting the trees"), [this]() { zielsko_plant(); });
	}
	if (false == m_zielsko_counties_read)
	{
		m_zielsko_counties_read = true;
		zielsko_refresh_counties();
	}
	if (false == ImGui::CollapsingHeader(STR_C("ZIELSKO from space"), ImGuiTreeNodeFlags_DefaultOpen))
		return;
	ImGui::TextWrapped("%s", STR_C("The trees of the 3D tree models of geoportal.gov.pl (GUGiK, from airborne laser scanning, trees over 4 m): where each "
	                               "stands and how tall it is. What the area needs is read over the network when planting, only once: every sheet of "
	                               "the map read is kept beside the terrain, all its trees"));
	auto const running{m_zielsko_harvest.running()};

	ImGui::SeparatorText(STR_C("1. The area"));
	ImGui::BeginDisabled(running);
	ImGui::RadioButton(STR_C("Around the camera##zielsko"), &settings.scope, 0);
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("Within the outline##zielsko"), &settings.scope, 2);
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("The whole scenery##zielsko"), &settings.scope, 1);
	ImGui::EndDisabled();
	if (settings.scope == 0)
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Radius (m)##zielsko"), &settings.radius, 10.0f, 100.0f, 20000.0f, "%.0f");
	}
	if (settings.scope == 2)
		ImGui::TextDisabled(STR_C("LMB adds a point of the outline, Shift+LMB takes the last one back (%zu points)"), m_paint_outline.size());
	if (settings.scope == 1)
		ImGui::TextDisabled("%s", STR_C("The counties under the whole terrain are read to the end: tens of gigabytes for a large scenery"));

	ImGui::SeparatorText(STR_C("2. The sieve"));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Nearest to a track (m)##zielsko"), &settings.nearest, 0.1f, 0.0f, 100.0f, "%.1f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("From the axis of a rail track: the clearance, the poles and the cess stay free"));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Farthest from a track (m)##zielsko"), &settings.farthest, 1.0f, 10.0f, 20000.0f, "%.0f");
	settings.farthest = std::max(settings.farthest, settings.nearest + 1.0f);
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Off the roads (m)##zielsko"), &settings.road, 0.1f, 0.0f, 30.0f, "%.1f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("From the edge of a road of the scenery"));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Lowest tree (m)##zielsko"), &settings.lowest, 0.1f, 4.0f, 40.0f, "%.1f");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Spacing (m)##zielsko"), &settings.spacing, 0.1f, 0.0f, 30.0f, settings.spacing > 0.0f ? "%.1f" : STR_C("none"));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("A wood thinned out: no two trees nearer than this, the taller kept"));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragInt(STR_C("At most##zielsko"), &settings.limit, 100.0f, 100, 1000000);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("The trees nearest to the tracks first; each one is a model of the scenery"));
	ImGui::TextDisabled("%s", STR_C("The water bodies of the terrain stay free of trees"));

	ImGui::SeparatorText(STR_C("3. The models"));
	ImGui::SetNextItemWidth(180.0f);
	ImGui::InputText(STR_C("Group of the node bank, trees##zielsko"), settings.trees_group, sizeof(settings.trees_group));
	ImGui::SetNextItemWidth(180.0f);
	ImGui::InputText(STR_C("Group of the node bank, low ones##zielsko"), settings.low_group, sizeof(settings.low_group));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Low ones below (m)##zielsko"), &settings.small_below, 0.1f, 4.0f, 30.0f, "%.1f");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Height of the models (m)##zielsko"), &settings.model_height, 0.1f, 0.0f, 60.0f, settings.model_height > 0.0f ? "%.1f" : STR_C("from the model"));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("How tall the models of the groups are at the scale of 1, so a tree of the data gets its own height.\n"
		                              "From the model: about the radius of the model, which suits the models standing on their foot"));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat(STR_C("Scatter of the size##zielsko"), &settings.jitter, 0.0f, 0.3f, "%.2f");

	ImGui::Spacing();
	ImGui::BeginDisabled(running);
	if (main_button(STR_C("Plant"), ImVec2(-FLT_MIN, tall_button_height())))
		run_busy(STR_C("Finding what the area needs"), [this]() { zielsko_harvest_start(false, true); });
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", STR_C("The sheets of the map of the area not read yet are read first, in the background (the package of a county is read only "
		                              "as far as they come); then the trees planted before within the area go and the ones through the sieve are planted"));
	if (ImGui::Button(STR_C("Count and show##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
		run_busy(STR_C("Sifting the trees"), [this]() { zielsko_count(); });
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", STR_C("From what's read already: the trees which would be planted, as dots up to 1.5 km from the camera, yellow low, green tall"));
	if (change_button(STR_C("Clear the trees planted within the area##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
	{
		auto const removed{zielsko_remove(true)};
		m_zielsko_preview.clear();
		m_zielsko_status = format(STR_C("Taken away: %zu trees planted by ZIELSKO"), removed);
	}
	ImGui::EndDisabled();
	if (false == m_zielsko_status.empty())
		ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.24f, 1.0f), "%s", m_zielsko_status.c_str());

	ImGui::SeparatorText(STR_C("4. The data"));
	if (running)
	{
		ImGui::ProgressBar(static_cast<float>(m_zielsko_harvest.progress()), ImVec2(-FLT_MIN, 0.0f));
		if (ImGui::Button(STR_C("Stop reading##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
		{
			m_zielsko_plant_after = false;
			m_zielsko_harvest.stop();
			zielsko_refresh_counties();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("What has come is kept; nothing is planted"));
		if (m_zielsko_plant_after)
			ImGui::TextDisabled("%s", STR_C("The trees are planted when the reading ends"));
	}
	auto const status{m_zielsko_harvest.status()};
	if (false == status.empty())
		ImGui::TextWrapped("%s", status.c_str());
	for (auto const &c : m_zielsko_harvest.counties())
	{
		if (c.needed == 0)
			ImGui::TextDisabled(STR_C("County %s: read before"), c.teryt.c_str());
		else
			ImGui::TextDisabled(STR_C("County %s: %zu sheets to read, the package up to %.0f MB"), c.teryt.c_str(), c.needed, c.size / 1048576.0);
	}
	if (ImGui::TreeNode(STR_C("Kept beside the terrain##zielsko")))
	{
		if (m_zielsko_counties.empty())
			ImGui::TextDisabled("%s", STR_C("Nothing yet"));
		for (auto const &line : m_zielsko_counties)
			ImGui::TextUnformatted(line.c_str());
		if (ImGui::SmallButton(STR_C("Refresh##zielsko")))
			zielsko_refresh_counties();
		ImGui::TreePop();
	}
	ImGui::BeginDisabled(running);
	if (ImGui::Button(STR_C("Read the whole scenery in the background##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
		run_busy(STR_C("Finding the counties under the terrain"), [this]() { zielsko_harvest_start(true, false); });
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", STR_C("All the counties under the terrain read to the end, the ones read whole before passed over: for a night, tens of gigabytes "
		                              "for a large scenery. Nothing is planted"));
	ImGui::EndDisabled();
	ImGui::TextDisabled("%s", STR_C("The source: GUGiK, 3D tree models (geoportal.gov.pl), CC BY 4.0: name it with the scenery"));
}
