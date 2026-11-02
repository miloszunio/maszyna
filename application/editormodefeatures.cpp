/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the field of work "Features": surfaces cut out of the terrain to give them a texture of their own while they keep the heights
// of the terrain, and the track bed with its embankments and cuttings cut along the formation modifiers into pieces up to 100 m

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "editor/editorFormat.hpp"
#include "world/Track.h"
#include "simulation/simulation.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <regex>
#include <unordered_map>

namespace
{

// the name of a line from the name of a track: L132_t1_00491 -> L132 t1, zw0001_lacz_00000 -> zw0001 lacz
std::string line_name(std::string Name)
{
	static std::regex const number{"_\\d+$"};
	static std::regex const track{"_(t\\d\\w?)$"};
	Name = std::regex_replace(Name, number, "");
	Name = std::regex_replace(Name, track, " $1");
	std::replace(Name.begin(), Name.end(), '_', ' ');
	return Name;
}

// the tracks of the scenery by squares of 200 m, to name the pieces after the nearest
struct track_names
{
	static constexpr double cell{200.0};
	std::unordered_map<std::int64_t, std::vector<std::pair<glm::dvec2, std::string const *>>> grid;
	std::vector<std::string> names;

	static std::int64_t key(double const X, double const Z)
	{
		return (static_cast<std::int64_t>(std::floor(X / cell)) << 32) ^ (static_cast<std::int64_t>(std::floor(Z / cell)) & 0xffffffff);
	}

	track_names()
	{
		auto const &tracks{simulation::Paths.sequence()};
		names.reserve(tracks.size());
		for (auto *track : tracks)
			if (track != nullptr && false == track->name().empty())
				names.push_back(line_name(track->name()));
		std::size_t index{0};
		for (auto *track : tracks)
		{
			if (track == nullptr || track->name().empty())
				continue;
			auto const at{track->location()};
			grid[key(at.x, at.z)].emplace_back(glm::dvec2{at.x, at.z}, &names[index++]);
		}
	}

	std::string nearest(glm::dvec2 const &Point) const
	{
		double best{std::numeric_limits<double>::max()};
		std::string const *found{nullptr};
		for (int dz = -1; dz <= 1; ++dz)
			for (int dx = -1; dx <= 1; ++dx)
			{
				auto const lookup{grid.find(key(Point.x + dx * cell, Point.y + dz * cell))};
				if (lookup == grid.end())
					continue;
				for (auto const &[position, name] : lookup->second)
					if (auto const distance{glm::distance(position, Point)}; distance < best)
					{
						best = distance;
						found = name;
					}
			}
		return found != nullptr ? *found : std::string{};
	}
};

// the axis of a modifier every Step metres: the points, their stations and the normals to the left
struct axis
{
	std::vector<glm::dvec2> points;
	std::vector<double> formation;
	std::vector<double> half_width;
	std::vector<double> station;
	std::vector<glm::dvec2> left;
};

axis resample(heightmap::modifier const &Modifier, double const Step)
{
	axis result;
	auto const &points{Modifier.points};
	std::vector<double> along{0.0};
	for (std::size_t i = 1; i < points.size(); ++i)
		along.push_back(along.back() + glm::distance(points[i].position, points[i - 1].position));
	auto const total{along.back()};
	auto const count{std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(total / Step)))};
	std::size_t segment{0};
	for (std::size_t n = 0; n <= count; ++n)
	{
		auto const s{total * n / count};
		while (segment + 2 < points.size() && along[segment + 1] < s)
			++segment;
		auto const span{std::max(1e-9, along[segment + 1] - along[segment])};
		auto const t{std::clamp((s - along[segment]) / span, 0.0, 1.0)};
		auto const &a{points[segment]};
		auto const &b{points[segment + 1]};
		result.points.push_back(glm::mix(a.position, b.position, t));
		result.formation.push_back(a.formation + (b.formation - a.formation) * t);
		result.half_width.push_back(a.half_width + (b.half_width - a.half_width) * t);
		result.station.push_back(s);
	}
	auto const size{result.points.size()};
	for (std::size_t k = 0; k < size; ++k)
	{
		auto const &before{result.points[k > 0 ? k - 1 : k]};
		auto const &after{result.points[k + 1 < size ? k + 1 : k]};
		auto direction{after - before};
		auto const length{glm::length(direction)};
		direction = length > 1e-9 ? direction / length : glm::dvec2{1.0, 0.0};
		result.left.emplace_back(-direction.y, direction.x);
	}
	return result;
}

struct piece
{
	std::string name;
	float lift{0.02f};
	std::vector<glm::dvec2> outline;
};

} // namespace

void editor_mode::render_material_palette()
{
	auto const &layers{m_streamer.manifest().layers};
	if (layers.empty())
		return;
	m_terrain_layer = std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1);
	for (std::size_t i = 0; i < layers.size(); ++i)
	{
		auto label{layers[i].size > 0.f ? format("%zu: %s (%.1f m)", i, layers[i].material.c_str(), layers[i].size) : format("%zu: %s", i, layers[i].material.c_str())};
		if (layers[i].rotation != 0.f)
			label += format(STR_C(", turned %.0f°"), layers[i].rotation);
		if (ImGui::Selectable(label.c_str(), static_cast<int>(i) == m_terrain_layer))
			m_terrain_layer = static_cast<int>(i);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("New chunks are covered with the first material. A texture dragged from the browser onto an entry of the palette takes its place"));
		std::string dropped;
		if (material_drop(dropped))
			m_streamer.layer_material(static_cast<std::uint16_t>(i), dropped);
	}
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputText(STR_C("Material"), m_terrain_material, IM_ARRAYSIZE(m_terrain_material));
	material_drop(m_terrain_material, IM_ARRAYSIZE(m_terrain_material));
	ImGui::SameLine();
	if (ImGui::Button(STR_C("Add to the palette")) && m_terrain_material[0] != '\0')
		m_terrain_layer = m_streamer.layer(m_terrain_material, m_terrain_material_size);
}

void editor_mode::render_features_ui()
{
	if (false == m_streamer.active())
	{
		ImGui::TextWrapped("%s", STR_C("The scenery has no terrain yet. It's made in the Terrain field of work"));
		return;
	}
	auto const &layers{m_streamer.manifest().layers};
	if (ImGui::CollapsingHeader(STR_C("Material##features"), ImGuiTreeNodeFlags_DefaultOpen))
		render_material_palette();

	if (m_feature_tool == feature_tool::cut_out)
	{
		if (ImGui::CollapsingHeader(STR_C("Cut out a surface"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextWrapped("%s", STR_C("LMB adds a point of the outline, Shift+LMB takes the last one back. The surface cut out keeps the heights of the terrain "
			                               "and follows them when the terrain is changed; only its texture is its own"));
			ImGui::Text(STR_C("Outline: %zu points"), m_paint_outline.size());
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderFloat(STR_C("Texture turned by (deg)"), &m_paint_rotation, -180.0f, 180.0f, "%.0f");
			ImGui::BeginDisabled(m_paint_outline.size() < 3 || layers.empty());
			if (ImGui::Button(STR_C("Cut out as a separate surface")))
				cut_out_surface();
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", STR_C("Takes the area within the outline out of the terrain and puts it back as a separate surface\n"
				                              "with the heights of the terrain and the material chosen in the palette, turned by the angle above"));
			ImGui::SameLine();
			if (ImGui::Button(STR_C("Clear the outline##features")))
				m_paint_outline.clear();
		}
	}
	else
	{
		if (ImGui::CollapsingHeader(STR_C("Track bed and slopes"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto &settings{m_track_bed};
			ImGui::TextWrapped("%s", STR_C("Along the formation modifiers of the terrain (the ones the tracks shape it with) the track bed, the embankments and the cuttings "
			                               "are cut out as surfaces of their own, each up to the length below, to be given textures of their own. "
			                               "The embankments stay shaped by the modifiers; the pieces keep the heights of the terrain"));
			ImGui::TextDisabled(STR_C("Formation modifiers: %zu"), m_streamer.modifiers().size());
			ImGui::Checkbox(STR_C("The whole scenery"), &settings.everywhere);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("All the modifiers; the chunks under them are read from their files, so it takes time and memory"));
			if (false == settings.everywhere)
			{
				ImGui::SetNextItemWidth(120.0f);
				ImGui::DragFloat(STR_C("Around the camera (m)"), &settings.radius, 10.0f, 100.0f, 5000.0f, "%.0f");
			}
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Longest piece (m)"), &settings.length, 1.0f, 10.0f, 500.0f, "%.0f");
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Farthest slope (m)"), &settings.reach, 1.0f, 5.0f, 200.0f, "%.0f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("From the edge of the track bed; the slope ends sooner where it meets the ground"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Slope tolerance (m)"), &settings.tolerance, 0.01f, 0.02f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("How far the ground may lie off the slope of the modifier (1:1.5) and still count as the slope"));
			ImGui::BeginDisabled(m_streamer.modifiers().empty());
			if (ImGui::Button(STR_C("Cut the track bed and the slopes")))
				cut_track_bed();
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", STR_C("The pieces cut before along the same modifiers are made anew. Ctrl+Z takes it back"));
		}
	}
	if (ImGui::CollapsingHeader(STR_C("Surfaces cut out of the terrain"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_streamer.patches().empty())
			ImGui::TextDisabled("%s", STR_C("None yet"));
		render_patches_ui();
	}
	if (false == m_terrain_status.empty())
		ImGui::TextWrapped("%s", m_terrain_status.c_str());
}

void editor_mode::cut_track_bed()
{
	auto const &settings{m_track_bed};
	auto const &layers{m_streamer.manifest().layers};
	if (layers.empty())
		return;
	auto const &chosen{layers[std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1)]};
	glm::dvec2 const camera{Camera.Pos.x, Camera.Pos.z};
	auto const length{std::max(10.0, static_cast<double>(settings.length))};
	auto const step{std::clamp(static_cast<double>(settings.step), 1.0, length / 2.0)};
	auto const reach{std::max(1.0, static_cast<double>(settings.reach))};

	// the modifiers to cut along
	auto const &modifiers{m_streamer.modifiers()};
	std::vector<std::size_t> chosen_modifiers;
	for (std::size_t i = 0; i < modifiers.size(); ++i)
	{
		if (modifiers[i].points.size() < 2)
			continue;
		if (false == settings.everywhere)
		{
			auto const [low, high]{modifiers[i].bounds()};
			auto const nearest{glm::clamp(camera, low, high)};
			if (glm::distance(nearest, camera) > settings.radius)
				continue;
		}
		chosen_modifiers.push_back(i);
	}
	if (chosen_modifiers.empty())
	{
		m_terrain_status = STR_C("No formation modifier there");
		return;
	}

	// the axes of all the modifiers near, every metre, for the borders between tracks side by side
	struct dense_point
	{
		glm::dvec2 point;
		std::size_t owner;
		double station;
	};
	std::unordered_map<std::int64_t, std::vector<dense_point>> dense;
	double constexpr cell{50.0};
	auto const key = [](double const X, double const Z) {
		return (static_cast<std::int64_t>(std::floor(X / cell)) << 32) ^ (static_cast<std::int64_t>(std::floor(Z / cell)) & 0xffffffff);
	};
	std::vector<axis> axes(modifiers.size());
	{
		// the neighbours of the chosen ones too, so the border with a track just outside the circle is kept
		glm::dvec2 low{std::numeric_limits<double>::max()};
		glm::dvec2 high{-std::numeric_limits<double>::max()};
		for (auto const i : chosen_modifiers)
		{
			auto const bounds{modifiers[i].bounds()};
			low = glm::min(low, bounds.first);
			high = glm::max(high, bounds.second);
		}
		for (std::size_t i = 0; i < modifiers.size(); ++i)
		{
			if (modifiers[i].points.size() < 2)
				continue;
			auto const bounds{modifiers[i].bounds()};
			if (bounds.second.x < low.x - reach || bounds.first.x > high.x + reach || bounds.second.y < low.y - reach || bounds.first.y > high.y + reach)
				continue;
			auto const fine{resample(modifiers[i], 1.0)};
			for (std::size_t k = 0; k < fine.points.size(); ++k)
				dense[key(fine.points[k].x, fine.points[k].y)].push_back({fine.points[k], i, fine.station[k]});
		}
	}

	track_names const names;
	std::vector<piece> pieces;
	std::size_t bed{0};
	std::size_t embankment{0};
	std::size_t cutting{0};
	std::vector<std::string> bases;
	for (auto const index : chosen_modifiers)
	{
		auto const &modifier{modifiers[index]};
		auto const line{resample(modifier, step)};
		auto const count{line.points.size()};
		auto const slope{std::max(0.1, modifier.slope)};
		auto const rounding{modifier.rounding};
		auto const maxwidth{*std::max_element(line.half_width.begin(), line.half_width.end())};
		auto base{names.nearest(line.points[count / 2])};
		if (base.empty())
			base = modifier.name;
		bases.push_back(base + " ");

		// the border with the tracks beside: the offset along the normal from which another axis is nearer
		std::vector<double> limit[2]{std::vector<double>(count, std::numeric_limits<double>::infinity()), std::vector<double>(count, std::numeric_limits<double>::infinity())};
		auto const range{maxwidth + reach};
		for (std::size_t k = 0; k < count; ++k)
		{
			auto const &p{line.points[k]};
			auto const cells{static_cast<int>(std::ceil(range / cell))};
			for (int dz = -cells; dz <= cells; ++dz)
				for (int dx = -cells; dx <= cells; ++dx)
				{
					auto const lookup{dense.find(key(p.x + dx * cell, p.y + dz * cell))};
					if (lookup == dense.end())
						continue;
					for (auto const &other : lookup->second)
					{
						if (other.owner == index && std::abs(other.station - line.station[k]) <= 3.0 * range)
							continue;
						auto const q{other.point - p};
						if (glm::dot(q, q) > range * range)
							continue;
						for (int side = 0; side < 2; ++side)
						{
							auto const normal{line.left[k] * (side == 0 ? 1.0 : -1.0)};
							auto const along{glm::dot(q, normal)};
							if (along > 1e-6)
								limit[side][k] = std::min(limit[side][k], glm::dot(q, q) / (2.0 * along));
						}
					}
				}
		}

		auto const point = [&](std::size_t const K, int const Side, double const Offset) { return line.points[K] + line.left[K] * (Side == 0 ? 1.0 : -1.0) * Offset; };
		auto const km = [&](std::size_t const A, std::size_t const B) { return format("%07.3f-%07.3f", line.station[A] / 1000.0, line.station[B] / 1000.0); };

		std::vector<double> edge[2];
		std::vector<double> outer[2];
		std::vector<int> kind[2]; // 1 embankment, -1 cutting, 0 none
		for (int side = 0; side < 2; ++side)
		{
			edge[side].resize(count);
			outer[side].resize(count);
			kind[side].assign(count, 0);
			for (std::size_t k = 0; k < count; ++k)
			{
				auto const hw{line.half_width[k]};
				edge[side][k] = std::min(hw, limit[side][k]);
				outer[side][k] = edge[side][k];
				if (edge[side][k] < hw - 1e-6)
					continue; // a track beside: no slope between them
				auto const form{line.formation[k]};
				// the kind from the rise 1-3 m out of the edge
				double rise{0.0};
				int found{0};
				for (double offset = 1.0; offset <= 3.0; offset += 0.5)
				{
					double height{0.0};
					auto const at{point(k, side, hw + offset)};
					if (m_streamer.height_at(at.x, at.y, height, true))
					{
						rise += height - form;
						++found;
					}
				}
				if (found == 0 || std::abs(rise / found) < 0.1)
					continue; // the ground at the level of the formation: no slope
				auto const fill{rise < 0.0};
				// along the ray as long as the ground keeps to the slope 1:slope of the modifier
				double end{0.0};
				for (double offset = 0.5; offset <= reach; offset += 0.5)
				{
					if (hw + offset > limit[side][k])
						break;
					double height{0.0};
					auto const at{point(k, side, hw + offset)};
					if (false == m_streamer.height_at(at.x, at.y, height, true))
						break;
					auto const expected{fill ? form - offset / slope : form + offset / slope};
					if (std::abs(height - expected) >= settings.tolerance)
						break;
					end = offset;
				}
				end = std::min(end + 0.5 + rounding * 0.5, reach);
				if (end >= settings.least_slope)
				{
					outer[side][k] = std::min(hw + end, limit[side][k]);
					kind[side][k] = fill ? 1 : -1;
				}
			}
			// smoothing along the axis: gaps up to 3 samples between the same slope filled, slopes shorter than 3 samples dropped,
			// the end of the slope the median of 5 samples
			auto &sign{kind[side]};
			for (std::size_t k = 0; k < count;)
			{
				if (sign[k] != 0)
				{
					++k;
					continue;
				}
				auto j{k};
				while (j < count && sign[j] == 0)
					++j;
				bool between{k > 0 && j < count && j - k <= 3 && sign[k - 1] == sign[j]};
				for (auto n = k; between && n < j; ++n)
					between = edge[side][n] >= line.half_width[n] - 1e-6;
				if (between)
					for (auto n = k; n < j; ++n)
					{
						sign[n] = sign[k - 1];
						auto const t{static_cast<double>(n - (k - 1)) / static_cast<double>(j - (k - 1))};
						outer[side][n] = std::min(outer[side][k - 1] + (outer[side][j] - outer[side][k - 1]) * t, limit[side][n]);
					}
				k = j;
			}
			for (std::size_t k = 0; k < count;)
			{
				if (sign[k] == 0)
				{
					++k;
					continue;
				}
				auto j{k};
				while (j < count && sign[j] == sign[k])
					++j;
				if (j - k < 3)
					for (auto n = k; n < j; ++n)
					{
						sign[n] = 0;
						outer[side][n] = edge[side][n];
					}
				k = j;
			}
			auto const raw{outer[side]};
			for (std::size_t k = 0; k < count; ++k)
			{
				if (sign[k] == 0)
					continue;
				std::array<double, 5> window;
				for (int d = -2; d <= 2; ++d)
					window[d + 2] = raw[std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(k) + d, 0, static_cast<std::ptrdiff_t>(count) - 1)];
				std::nth_element(window.begin(), window.begin() + 2, window.end());
				outer[side][k] = std::max(std::min(window[2], limit[side][k]), line.half_width[k] + settings.least_slope);
			}
		}

		// the track bed every Length metres
		auto const parts{std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(line.station.back() / length - 1e-9)))};
		std::vector<std::size_t> bounds{0};
		for (std::size_t p = 1; p < parts; ++p)
		{
			auto const wanted{p * length};
			auto const at{std::lower_bound(line.station.begin(), line.station.end(), wanted)};
			bounds.push_back(static_cast<std::size_t>(std::distance(line.station.begin(), std::min(at, line.station.end() - 1))));
		}
		bounds.push_back(count - 1);
		std::sort(bounds.begin(), bounds.end());
		bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());
		for (std::size_t b = 0; b + 1 < bounds.size(); ++b)
		{
			piece made;
			made.name = format("%s torowisko km %s", base.c_str(), km(bounds[b], bounds[b + 1]).c_str());
			made.lift = 0.03f;
			for (auto k = bounds[b]; k <= bounds[b + 1]; ++k)
				made.outline.push_back(point(k, 0, edge[0][k]));
			for (auto k = bounds[b + 1] + 1; k-- > bounds[b];)
				made.outline.push_back(point(k, 1, edge[1][k]));
			pieces.push_back(std::move(made));
			++bed;
		}
		// the slopes: runs of samples with the same slope on a side, split every Length metres, the pieces next to each other sharing a point
		for (int side = 0; side < 2; ++side)
		{
			auto const label{side == 0 ? "L" : "P"};
			for (std::size_t k = 0; k < count;)
			{
				if (kind[side][k] == 0)
				{
					++k;
					continue;
				}
				auto j{k};
				while (j + 1 < count && kind[side][j + 1] == kind[side][k])
					++j;
				auto const fill{kind[side][k] > 0};
				for (auto start{k}; start < j;)
				{
					auto end{start};
					while (end + 1 <= j && line.station[end + 1] - line.station[start] <= length + 1e-6)
						++end;
					end = std::max(end, start + 1);
					piece made;
					made.name = format("%s %s %s km %s", base.c_str(), fill ? "nasyp" : "przekop", label, km(start, end).c_str());
					made.lift = 0.02f;
					for (auto n = start; n <= end; ++n)
						made.outline.push_back(point(n, side, edge[side][n]));
					for (auto n = end + 1; n-- > start;)
						made.outline.push_back(point(n, side, outer[side][n]));
					pieces.push_back(std::move(made));
					++(fill ? embankment : cutting);
					start = end;
				}
				k = j + 1;
			}
		}
	}

	// the pieces cut before along the same tracks go, the new ones take their place; the hole and the surfaces in the same frame
	auto patches{m_streamer.patches()};
	std::sort(bases.begin(), bases.end());
	bases.erase(std::unique(bases.begin(), bases.end()), bases.end());
	auto const replaced = [&](heightmap::surface_patch const &Patch) {
		if (Patch.name.find(" km ") == std::string::npos)
			return false;
		if (Patch.name.find(" torowisko ") == std::string::npos && Patch.name.find(" nasyp ") == std::string::npos && Patch.name.find(" przekop ") == std::string::npos)
			return false;
		auto const at{std::upper_bound(bases.begin(), bases.end(), Patch.name)};
		if (at == bases.begin())
			return false;
		auto const &candidate{*std::prev(at)};
		if (Patch.name.compare(0, candidate.size(), candidate) != 0)
			return false;
		if (settings.everywhere)
			return true;
		// only those within the circle: another stretch of the same line farther away stays
		glm::dvec2 low, high;
		Patch.bounds(low, high);
		return glm::distance(glm::clamp(camera, low, high), camera) <= settings.radius + reach;
	};
	auto const before{patches.size()};
	patches.erase(std::remove_if(patches.begin(), patches.end(), replaced), patches.end());
	auto const removed{before - patches.size()};
	for (auto &made : pieces)
	{
		if (made.outline.size() < 3)
			continue;
		m_streamer.cut(made.outline, true);
		heightmap::surface_patch patch;
		patch.name = std::move(made.name);
		patch.material = chosen.material;
		patch.size = chosen.size;
		patch.rotation = chosen.rotation;
		patch.lift = made.lift;
		patch.follow = true;
		patch.spacing = 2.0;
		patch.outline = std::move(made.outline);
		patches.push_back(std::move(patch));
	}
	m_streamer.patches(std::move(patches));
	m_patch_selected = -1;
	forget_ground();
	m_terrain_status = format(STR_C("Cut along %zu modifiers: track bed %zu, embankments %zu, cuttings %zu pieces (%zu made before replaced). Ctrl+Z takes it back"),
	                          chosen_modifiers.size(), bed, embankment, cutting, removed);
	WriteLog("Editor: " + m_terrain_status);
}

void editor_mode::go_to_plan(glm::dvec2 const &Plan)
{
	glm::dvec3 target{Plan.x, Camera.Pos.y, Plan.y};
	double ground{0.0};
	auto const known{m_streamer.height_at(Plan.x, Plan.y, ground)};
	if (known)
		target.y = ground;
	if (Global.EditorOrtho || false == known)
	{
		// the top view, or no ground there to look down at: the camera only moves over
		m_focus_active = false;
		Camera.Pos.x = target.x;
		Camera.Pos.z = target.z;
		Camera.Velocity = glm::dvec3{0.0};
		Global.pCamera = Camera;
		return;
	}
	// from the side the camera looks from, down at the point
	auto const yaw{static_cast<double>(Camera.Angle.y)};
	auto const pitch{glm::radians(35.0)};
	glm::dvec3 const look{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
	fly_to(target, look, 150.0);
}
