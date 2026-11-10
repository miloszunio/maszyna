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

void editor_mode::zielsko_harvest_start()
{
	auto const &manifest{m_streamer.manifest()};
	if (false == manifest.georeferenced)
	{
		m_zielsko_status = STR_C("The terrain doesn't know where it lies (PUWG 1992): set its origin first");
		return;
	}
	auto const east0{manifest.origin_east};
	auto const north0{manifest.origin_north};
	// the chunks of the terrain on the disk and in memory
	std::set<heightmap::chunk_key> chunks;
	for (auto const &[key, cells] : m_streamer.stored_chunks())
		chunks.insert(key);
	// the cells of 50 m within the reach of a rail track, over the terrain
	zielsko::area area;
	track_points rails;
	rails.gather(1, 25.0);
	auto const reach{static_cast<double>(m_zielsko.reach)};
	auto const span{static_cast<int>(std::ceil(reach / zielsko::area::cell))};
	for (auto const &[key, points] : rails.buckets)
		for (auto const &p : points)
		{
			// the cell of the point in PUWG 1992, and those around it
			auto const east{east0 - p.x};
			auto const north{north0 + p.z};
			auto const ce{std::floor(east / zielsko::area::cell)};
			auto const cn{std::floor(north / zielsko::area::cell)};
			for (int de = -span; de <= span; ++de)
				for (int dn = -span; dn <= span; ++dn)
				{
					auto const e{(ce + de + 0.5) * zielsko::area::cell};
					auto const n{(cn + dn + 0.5) * zielsko::area::cell};
					if (std::hypot(e - east, n - north) > reach + zielsko::area::cell * 0.71)
						continue;
					if (area.contains(e, n))
						continue;
					if (chunks.count(heightmap::chunk_at(east0 - e, n - north0)) == 0)
						continue;
					area.add(e, n);
				}
		}
	if (area.empty())
	{
		m_zielsko_status = STR_C("No rail track over the terrain: nothing to look for");
		return;
	}
	auto probes{area.sample(2500.0)};
	m_zielsko_harvest.start(zielsko_folder(), std::move(area), std::move(probes), m_zielsko.again);
	m_zielsko_status.clear();
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
	// the trees kept, all the counties
	std::vector<zielsko::tree> trees;
	std::error_code error;
	for (auto const &entry : std::filesystem::directory_iterator{zielsko_folder(), error})
		if (entry.path().extension() == ".drz")
		{
			++Counties;
			if (zielsko::read_cache(entry.path().string(), trees))
				++Read;
		}
	if (trees.empty())
		return result;
	track_points rails, roads;
	rails.gather(1, 4.0);
	roads.gather(2, 4.0);
	auto const &outline{shaped_outline()};
	auto const in_outline{settings.scope == 2 && outline.size() >= 3};
	auto const &water{m_streamer.water()};
	auto const camera{glm::dvec2{Camera.Pos.x, Camera.Pos.z}};
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
			continue;
		glm::dvec2 const at{east0 - t.east, t.north - north0};
		if (settings.scope == 0 && glm::distance(at, camera) > settings.radius)
			continue;
		if (in_outline && false == geometry::point_in_polygon(outline, at))
			continue;
		auto const track{rails.distance(at.x, at.y, farthest)};
		if (track > farthest || track < settings.nearest)
			continue;
		if (false == roads.empty() && roads.distance(at.x, at.y, settings.road) < settings.road)
			continue;
		auto wet{false};
		for (auto const &body : water)
			if (geometry::point_in_polygon(body.outline, at))
			{
				wet = true;
				break;
			}
		if (wet)
			continue;
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
				continue;
			placed[k].push_back(at);
		}
		result.push_back({at, t.height, static_cast<float>(track)});
	}
	// the nearest to the tracks first, as far as the limit
	std::sort(result.begin(), result.end(), [](zielsko_candidate const &A, zielsko_candidate const &B) { return A.track < B.track; });
	if (result.size() > static_cast<std::size_t>(std::max(0, settings.limit)))
		result.resize(static_cast<std::size_t>(std::max(0, settings.limit)));
	return result;
}

void editor_mode::zielsko_count()
{
	std::size_t counties, read;
	m_zielsko_preview = zielsko_candidates(counties, read);
	if (counties == 0)
		m_zielsko_status = STR_C("No county read yet: find and read them first");
	else
		m_zielsko_status = format(STR_C("%zu trees would be planted (from %zu of %zu counties read)"), m_zielsko_preview.size(), read, counties);
}

std::size_t editor_mode::zielsko_remove()
{
	std::vector<TAnimModel *> found;
	for (auto *model : simulation::Instances.sequence())
		if (model != nullptr && starts_with(model->name(), zielsko_prefix))
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
		return;
	}
	std::size_t counties, read;
	auto const candidates{zielsko_candidates(counties, read)};
	m_zielsko_preview.clear();
	if (candidates.empty())
	{
		m_zielsko_status = counties == 0 ? std::string{STR_C("No county read yet: find and read them first")} : std::string{STR_C("No tree passes the sieve")};
		return;
	}
	auto const removed{zielsko_remove()};
	std::mt19937 random{1992u};
	auto const uniform = [&](double const Low, double const High) { return std::uniform_real_distribution<double>{Low, High}(random); };
	// the height of each template: given, or about the radius of its model, which stands on its foot
	std::unordered_map<std::string, double> heights;
	std::size_t planted{0};
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		if ((i & 0xff) == 0)
			editor_busy_progress(0.6 + 0.4 * static_cast<double>(i) / static_cast<double>(candidates.size()));
		auto const &c{candidates[i]};
		double ground{0.0};
		if (false == m_streamer.height_at(c.at.x, c.at.y, ground, true))
			continue;
		glm::dvec3 const location{c.at.x, ground, c.at.y};
		if (false == simulation::Region->point_inside(location))
			continue;
		auto const &pool{c.height < settings.small_below ? low_pool : tall_pool};
		auto const &src{pool[std::min(pool.size() - 1, static_cast<std::size_t>(uniform(0.0, 1.0) * pool.size()))]};
		auto *model{simulation::State.create_model(src, zielsko_prefix + std::to_string(planted), location)};
		if (model == nullptr)
			continue;
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
	m_zielsko_status = format(STR_C("Planted %zu trees (%zu planted before taken away). The source: GUGiK, 3D tree models, CC BY 4.0"), planted, removed);
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
	if (ImGui::CollapsingHeader(STR_C("ZIELSKO from space"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextWrapped("%s", STR_C("The trees of the 3D tree models of geoportal.gov.pl (GUGiK, from airborne laser scanning, trees over 4 m): where each "
		                               "stands and how tall it is. The counties under the scenery are found and their packages (gigabytes each) read over the "
		                               "network in the background; only the trees near the tracks are kept, beside the terrain"));
		ImGui::SeparatorText(STR_C("1. The data"));
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Kept up to (m from the tracks)##zielsko"), &settings.reach, 10.0f, 50.0f, 2000.0f, "%.0f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("The trees further from a rail track aren't kept at all; a wider band means reading the counties again"));
		ImGui::Checkbox(STR_C("Read again the counties read before##zielsko"), &settings.again);
		auto const running{m_zielsko_harvest.running()};
		if (false == running)
		{
			if (main_button(STR_C("Find the counties and read their trees"), ImVec2(-FLT_MIN, tall_button_height())))
				run_busy(STR_C("Finding the band along the tracks"), [this]() { zielsko_harvest_start(); });
		}
		else
		{
			ImGui::ProgressBar(static_cast<float>(m_zielsko_harvest.progress()), ImVec2(-FLT_MIN, 0.0f));
			if (ImGui::Button(STR_C("Stop reading##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
				m_zielsko_harvest.stop();
		}
		auto const status{m_zielsko_harvest.status()};
		if (false == status.empty())
			ImGui::TextWrapped("%s", status.c_str());
		auto const counties{m_zielsko_harvest.counties()};
		if (false == counties.empty())
		{
			std::string list;
			for (auto const &c : counties)
				list += (list.empty() ? "" : ", ") + c.teryt;
			ImGui::TextDisabled(STR_C("Counties (TERYT): %s"), list.c_str());
		}

		ImGui::SeparatorText(STR_C("2. The sieve"));
		ImGui::RadioButton(STR_C("Around the camera##zielsko"), &settings.scope, 0);
		ImGui::SameLine();
		ImGui::RadioButton(STR_C("The whole scenery##zielsko"), &settings.scope, 1);
		ImGui::SameLine();
		ImGui::RadioButton(STR_C("Within the outline##zielsko"), &settings.scope, 2);
		if (settings.scope == 0)
		{
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Radius (m)##zielsko"), &settings.radius, 10.0f, 100.0f, 20000.0f, "%.0f");
		}
		if (settings.scope == 2)
			ImGui::TextDisabled("%s", STR_C("LMB adds a point of the outline, Shift+LMB takes the last one back"));
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Nearest to a track (m)##zielsko"), &settings.nearest, 0.1f, 0.0f, 100.0f, "%.1f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("From the axis of a rail track: the clearance, the poles and the cess stay free"));
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Farthest from a track (m)##zielsko"), &settings.farthest, 1.0f, 10.0f, 2000.0f, "%.0f");
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
		if (ImGui::Button(STR_C("Count and show##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
			run_busy(STR_C("Sifting the trees"), [this]() { zielsko_count(); });
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("The trees which would be planted, as dots up to 1.5 km from the camera: yellow low, green tall"));
		if (main_button(STR_C("Plant"), ImVec2(-FLT_MIN, tall_button_height())))
			run_busy(STR_C("Planting the trees"), [this]() { zielsko_plant(); });
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("The trees planted before by ZIELSKO go first, then the ones through the sieve are planted"));
		if (change_button(STR_C("Clear the trees planted##zielsko"), ImVec2(-FLT_MIN, 0.0f)))
		{
			auto const removed{zielsko_remove()};
			m_zielsko_preview.clear();
			m_zielsko_status = format(STR_C("Taken away: %zu trees planted by ZIELSKO"), removed);
		}
		if (false == m_zielsko_status.empty())
			ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.24f, 1.0f), "%s", m_zielsko_status.c_str());
		ImGui::TextDisabled("%s", STR_C("The source: GUGiK, 3D tree models (geoportal.gov.pl), CC BY 4.0: name it with the scenery"));
	}
}
