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

#include "application/editormodeshared.h"
#include "editor/editorGeoMap.hpp"
#include "utilities/Logs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace
{

// the towns and villages of the area of the IRT network, for the search of the Go to tool
struct place
{
	char const *name;
	char const *where;
	int east; // EPSG:2180
	int north;
	int city;
};
place const places[] = {
#include "application/editorplaces.inc"
};

// a name for the search: lower case, the Polish letters without their marks, so "zabkowice" finds "Ząbkowice"
std::string folded(char const *Text)
{
	static char const *const pairs[][2] = {{"ą", "a"}, {"ć", "c"}, {"ę", "e"}, {"ł", "l"}, {"ń", "n"}, {"ó", "o"}, {"ś", "s"}, {"ź", "z"}, {"ż", "z"},
	                                       {"Ą", "a"}, {"Ć", "c"}, {"Ę", "e"}, {"Ł", "l"}, {"Ń", "n"}, {"Ó", "o"}, {"Ś", "s"}, {"Ź", "z"}, {"Ż", "z"}};
	std::string result;
	for (auto const *c = Text; *c != '\0';)
	{
		bool replaced{false};
		if (static_cast<unsigned char>(*c) >= 0x80)
			for (auto const &pair : pairs)
			{
				auto const length{std::strlen(pair[0])};
				if (std::strncmp(c, pair[0], length) == 0)
				{
					result += pair[1];
					c += length;
					replaced = true;
					break;
				}
			}
		if (replaced)
			continue;
		auto const letter{static_cast<unsigned char>(*c)};
		result += (letter < 0x80) ? static_cast<char>(std::tolower(letter)) : *c;
		++c;
	}
	return result;
}

// the order the painted surfaces go on in: the slopes, then the track bed over them, then the ones drawn by hand over both
int paint_priority(heightmap::surface_patch const &Patch)
{
	if (Patch.name.find(" km ") != std::string::npos)
	{
		if (Patch.name.find(" torowisko ") != std::string::npos)
			return 1;
		if (Patch.name.find(" nasyp ") != std::string::npos || Patch.name.find(" przekop ") != std::string::npos)
			return 0;
	}
	return 2;
}

void reach_of(heightmap::surface_patch const &Patch, glm::dvec2 &Low, glm::dvec2 &High)
{
	Patch.bounds(Low, High);
	auto const margin{Patch.paint ? Patch.feather * 0.5 + Patch.noise + 1.0 : 1.0};
	Low -= glm::dvec2{margin};
	High += glm::dvec2{margin};
}

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
	if (m_feature_tool == feature_tool::go_to)
	{
		render_go_to_ui();
		return;
	}
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
			ImGui::TextWrapped("%s", STR_C("LMB adds a point of the outline, Shift+LMB takes the last one back. The surface is painted into the terrain with the material "
			                               "chosen above: the heights stay those of the terrain, there's no hole and no seam"));
			ImGui::Text(STR_C("Outline: %zu points"), m_paint_outline.size());
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Transition (m)##cut"), &m_cut_feather, 0.05f, 0.0f, 20.0f, m_cut_feather > 0.0f ? "%.2f" : STR_C("sharp"));
			m_cut_feather = std::max(0.0f, m_cut_feather);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("Metres across the edge the material passes into the one around over, half of them inside the surface.\n"
				                              "The paint of the terrain is kept every metre (every 0.5 m in the chunks converted to 0.5 m), narrower comes out in steps"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Noise of the edge (m)##cut"), &m_cut_noise, 0.05f, 0.0f, 10.0f, m_cut_noise > 0.0f ? "%.2f" : STR_C("none"));
			m_cut_noise = std::max(0.0f, m_cut_noise);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("How far the edge wanders in and out of the outline, so it isn't drawn with a ruler"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Waves of the noise (m)##cut"), &m_cut_noise_scale, 0.1f, 0.5f, 50.0f, "%.1f");
			m_cut_noise_scale = std::max(0.5f, m_cut_noise_scale);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("About how long the waves of the edge are: small - frayed, large - gently wavy"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderFloat(STR_C("Texture turned by (deg)"), &m_paint_rotation, -180.0f, 180.0f, "%.0f");
			ImGui::Spacing();
			ImGui::BeginDisabled(m_paint_outline.size() < 3 || layers.empty());
			if (main_button(STR_C("Cut out as a separate surface"), ImVec2(-FLT_MIN, tall_button_height())))
				cut_out_surface();
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", STR_C("The area within the outline gets the material chosen in the palette, turned by the angle above, with the transition\n"
				                              "and the noise above; it's kept in the list below, where its material can be changed later"));
			if (ImGui::Button(STR_C("Clear the outline##features"), ImVec2(-FLT_MIN, 0.0f)))
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
			ImGui::DragFloat(STR_C("Track bed into the slope (m)"), &settings.bed_feather, 0.05f, 0.0f, 10.0f, "%.2f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("The transition from the material of the track bed into the one of the slope"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Slope into the ground (m)"), &settings.slope_feather, 0.05f, 0.0f, 20.0f, "%.2f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("The transition from the material of the slope into the ground around"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Noise of the edge (m)##bed"), &m_cut_noise, 0.05f, 0.0f, 10.0f, m_cut_noise > 0.0f ? "%.2f" : STR_C("none"));
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Waves of the noise (m)##bed"), &m_cut_noise_scale, 0.1f, 0.5f, 50.0f, "%.1f");
			m_cut_noise = std::max(0.0f, m_cut_noise);
			m_cut_noise_scale = std::max(0.5f, m_cut_noise_scale);
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(STR_C("Slope tolerance (m)"), &settings.tolerance, 0.01f, 0.02f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("How far the ground may lie off the slope of the modifier (1:1.5) and still count as the slope"));
			ImGui::Spacing();
			ImGui::BeginDisabled(m_streamer.modifiers().empty());
			if (main_button(STR_C("Cut the track bed and the slopes"), ImVec2(-FLT_MIN, tall_button_height())))
				cut_track_bed();
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", STR_C("The pieces cut before along the same modifiers are made anew. Ctrl+Z takes it back"));
		}
	}
	if (ImGui::CollapsingHeader(STR_C("Surfaces cut out of the terrain"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto const &patches{m_streamer.patches()};
		if (patches.empty())
			ImGui::TextDisabled("%s", STR_C("None yet"));
		auto const meshes{static_cast<std::size_t>(std::count_if(patches.begin(), patches.end(), [](heightmap::surface_patch const &Patch) { return false == Patch.paint; }))};
		if (meshes > 0)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.3f, 1.0f));
			ImGui::TextWrapped(STR_C("%zu of them are cut out with a hole and a mesh of their own (the seam shows): paint them into the terrain"), meshes);
			ImGui::PopStyleColor();
			if (ImGui::Button(STR_C("Paint them into the terrain##convert")))
				convert_patches_to_paint();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("Around the camera as far as set in Track bed and slopes, or in the whole scenery when that's ticked there.\n"
				                              "The holes are closed and the surfaces painted with their materials, the transitions as set there. Ctrl+Z takes it back"));
		}
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
	// the ones made before with a hole of their own get it closed
	for (auto const &patch : patches)
		if (false == patch.paint && replaced(patch))
			m_streamer.cut(patch.outline, false);
	patches.erase(std::remove_if(patches.begin(), patches.end(), replaced), patches.end());
	auto const removed{before - patches.size()};
	std::vector<std::size_t> made_now;
	for (auto &made : pieces)
	{
		if (made.outline.size() < 3)
			continue;
		heightmap::surface_patch patch;
		patch.name = std::move(made.name);
		patch.material = chosen.material;
		patch.size = chosen.size;
		patch.rotation = chosen.rotation;
		patch.follow = true;
		patch.paint = true;
		patch.spacing = 2.0;
		patch.feather = made.lift > 0.025f ? settings.bed_feather : settings.slope_feather;
		patch.noise = m_cut_noise;
		patch.noise_scale = m_cut_noise_scale;
		patch.outline = std::move(made.outline);
		made_now.push_back(patches.size());
		patches.push_back(std::move(patch));
	}
	repaint_patches(std::move(patches), made_now);
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

void editor_mode::cut_out_surface()
{
	auto const &layers{m_streamer.manifest().layers};
	if (m_paint_outline.size() < 3 || layers.empty())
		return;
	auto const &chosen{layers[std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1)]};
	heightmap::surface_patch patch;
	patch.material = chosen.material;
	patch.size = chosen.size;
	patch.rotation = static_cast<float>(std::remainder(static_cast<double>(chosen.rotation + m_paint_rotation), 360.0));
	patch.follow = true;
	patch.paint = true;
	patch.feather = m_cut_feather;
	patch.noise = m_cut_noise;
	patch.noise_scale = m_cut_noise_scale;
	for (auto const &point : m_paint_outline)
		patch.outline.emplace_back(point.x, point.z);
	auto patches{m_streamer.patches()};
	// a name not taken yet
	for (int number = static_cast<int>(patches.size()) + 1;; ++number)
	{
		patch.name = format(STR_C("surface %d"), number);
		if (std::none_of(patches.begin(), patches.end(), [&](heightmap::surface_patch const &Other) { return Other.name == patch.name; }))
			break;
	}
	auto const name{patch.name};
	patches.push_back(std::move(patch));
	auto const index{patches.size() - 1};
	repaint_patches(std::move(patches), {index});
	m_patch_selected = static_cast<int>(index);
	m_paint_outline.clear();
	m_terrain_status = format(STR_C("Cut out as %s with %s, Ctrl+Z puts it back"), name.c_str(), chosen.material.c_str());
}

bool editor_mode::paint_patch(heightmap::surface_patch const &Patch)
{
	auto const &layers{m_streamer.manifest().layers};
	if (Patch.outline.size() < 3 || layers.empty())
		return false;
	// the material as the palette has it, turned and repeated as the surface wants it: another entry of the palette when it differs
	std::uint16_t layer{0};
	if (false == Patch.material.empty())
		layer = m_streamer.layer(Patch.material, Patch.size, Patch.rotation);
	return m_streamer.fill(Patch.outline, Patch.feather, layer, 1.0, Patch.noise, Patch.noise_scale);
}

void editor_mode::repaint_patches(std::vector<heightmap::surface_patch> All, std::vector<std::size_t> const &Changed, std::vector<heightmap::surface_patch> const &Cleared)
{
	// the ones changed, and the painted ones over them they touch: the transitions between them made again
	std::vector<char> paint(All.size(), 0);
	double constexpr cell{100.0};
	auto const key = [](double const X, double const Z) {
		return (static_cast<std::int64_t>(std::floor(X / cell)) << 32) ^ (static_cast<std::int64_t>(std::floor(Z / cell)) & 0xffffffff);
	};
	std::unordered_map<std::int64_t, std::vector<std::size_t>> grid;
	if (false == Changed.empty() || false == Cleared.empty())
		for (std::size_t i = 0; i < All.size(); ++i)
		{
			if (false == All[i].paint)
				continue;
			glm::dvec2 low, high;
			reach_of(All[i], low, high);
			for (auto z{std::floor(low.y / cell)}; z <= std::floor(high.y / cell); z += 1.0)
				for (auto x{std::floor(low.x / cell)}; x <= std::floor(high.x / cell); x += 1.0)
					grid[key(x * cell, z * cell)].push_back(i);
		}
	auto const touching = [&](glm::dvec2 const &Low, glm::dvec2 const &High, auto const &Take) {
		for (auto z{std::floor(Low.y / cell)}; z <= std::floor(High.y / cell); z += 1.0)
			for (auto x{std::floor(Low.x / cell)}; x <= std::floor(High.x / cell); x += 1.0)
			{
				auto const lookup{grid.find(key(x * cell, z * cell))};
				if (lookup == grid.end())
					continue;
				for (auto const other : lookup->second)
				{
					glm::dvec2 low, high;
					reach_of(All[other], low, high);
					if (low.x <= High.x && high.x >= Low.x && low.y <= High.y && high.y >= Low.y)
						Take(other);
				}
			}
	};
	for (auto const index : Changed)
	{
		if (index >= All.size() || false == All[index].paint)
			continue;
		paint[index] = 1;
		glm::dvec2 low, high;
		reach_of(All[index], low, high);
		auto const priority{paint_priority(All[index])};
		// over it: higher in the order, or the same and later in the list
		touching(low, high, [&](std::size_t const Other) {
			auto const other{paint_priority(All[Other])};
			if (other > priority || (other == priority && Other > index))
				paint[Other] = 1;
		});
	}
	for (auto const &gone : Cleared)
	{
		glm::dvec2 low, high;
		reach_of(gone, low, high);
		// what was under it shows again; all the painted ones it touched get their edges back
		touching(low, high, [&](std::size_t const Other) { paint[Other] = 1; });
	}
	std::vector<std::size_t> order;
	for (std::size_t i = 0; i < All.size(); ++i)
		if (paint[i] != 0)
			order.push_back(i);
	std::stable_sort(order.begin(), order.end(), [&](std::size_t const A, std::size_t const B) { return paint_priority(All[A]) < paint_priority(All[B]); });
	m_streamer.patches(All);
	for (auto const &gone : Cleared)
	{
		auto base{gone};
		base.material.clear(); // the first material of the palette, as the ground was
		base.size = 0.f;
		base.rotation = 0.f;
		paint_patch(base);
	}
	std::size_t failed{0};
	for (auto const index : order)
		if (false == paint_patch(All[index]))
			++failed;
	if (failed > 0)
		WriteLog(format("Editor: %zu surfaces painted nothing: no chunk under them, or no room for another material in a chunk (8 at most)", failed));
	forget_ground();
}

void editor_mode::convert_patches_to_paint()
{
	auto const &settings{m_track_bed};
	glm::dvec2 const camera{Camera.Pos.x, Camera.Pos.z};
	auto patches{m_streamer.patches()};
	std::vector<std::size_t> converted;
	for (std::size_t i = 0; i < patches.size(); ++i)
	{
		auto &patch{patches[i]};
		if (patch.paint)
			continue;
		if (false == settings.everywhere)
		{
			glm::dvec2 low, high;
			patch.bounds(low, high);
			if (glm::distance(glm::clamp(camera, low, high), camera) > settings.radius)
				continue;
		}
		// the hole under it closed, the mesh gone
		m_streamer.cut(patch.outline, false);
		patch.paint = true;
		patch.follow = true;
		patch.heights.clear();
		patch.columns = 0;
		patch.rows = 0;
		auto const priority{paint_priority(patch)};
		patch.feather = priority == 1 ? settings.bed_feather : priority == 0 ? settings.slope_feather : m_cut_feather;
		patch.noise = m_cut_noise;
		patch.noise_scale = m_cut_noise_scale;
		converted.push_back(i);
	}
	if (converted.empty())
	{
		m_terrain_status = STR_C("No surface with a hole of its own there");
		return;
	}
	repaint_patches(std::move(patches), converted);
	m_terrain_status = format(STR_C("%zu surfaces painted into the terrain, their holes closed. Ctrl+Z takes it back"), converted.size());
	WriteLog("Editor: " + m_terrain_status);
}

void editor_mode::read_camera_places()
{
	if (m_camera_places_of == Global.SceneryFile)
		return;
	m_camera_places_of = Global.SceneryFile;
	m_camera_places.clear();
	// next to the scenery: <scenery>.kamery, a place a line: x y z pitch yaw roll name
	std::ifstream file{Global.asCurrentSceneryPath + Global.SceneryFile + ".kamery"};
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		std::istringstream words{line};
		camera_place place;
		if (false == static_cast<bool>(words >> place.position.x >> place.position.y >> place.position.z >> place.angle.x >> place.angle.y >> place.angle.z))
			continue;
		std::getline(words >> std::ws, place.name);
		if (false == place.name.empty() && place.name.back() == '\r')
			place.name.pop_back();
		m_camera_places.push_back(std::move(place));
	}
}

void editor_mode::write_camera_places() const
{
	std::ofstream file{Global.asCurrentSceneryPath + Global.SceneryFile + ".kamery", std::ios::trunc};
	file << "# the places of the camera kept in the editor (Features, Go to): x y z pitch yaw roll name\n";
	char line[160];
	for (auto const &place : m_camera_places)
	{
		std::snprintf(line, sizeof(line), "%.2f %.2f %.2f %.5f %.5f %.5f ", place.position.x, place.position.y, place.position.z, place.angle.x, place.angle.y, place.angle.z);
		file << line << place.name << '\n';
	}
	if (false == static_cast<bool>(file))
		WriteLog("Editor: can't write the places of the camera next to the scenery", logtype::generic);
}

void editor_mode::render_go_to_ui()
{
	read_camera_places();
	// the origin of the scenery in PUWG 1992: E = E0 - x, N = N0 + z
	load_orthophoto_settings();
	auto const &config{m_orthophoto.settings()};
	auto const located{config.north != 0.0 || config.east != 0.0};
	auto const to_plan = [&](glm::dvec2 const &Puwg) { return glm::dvec2{config.east - Puwg.x, Puwg.y - config.north}; };
	glm::dvec2 const camera{config.east - Camera.Pos.x, config.north + Camera.Pos.z};

	if (ImGui::CollapsingHeader(STR_C("Map##goto"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (false == located)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.3f, 1.0f));
			ImGui::TextWrapped("%s", STR_C("The origin of the scenery in PUWG 1992 isn't known: the map can't show the camera. It's set in Terrain, Terrain model, Location"));
			ImGui::PopStyleColor();
		}
		else
		{
			m_goto_map.mark(camera, static_cast<double>(Camera.Angle.y));
			if (m_goto_follow)
				m_goto_map.follow(camera);
		}
		auto const width{ImGui::GetContentRegionAvail().x};
		auto const height{std::round(std::min(width * 0.8f, ImGui::GetFontSize() * 20.0f))};
		ImGui::BeginChild("##gotomap", ImVec2(width, height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		m_goto_map.render({width, height});
		ImGui::EndChild();
		ImGui::Checkbox(STR_C("The map follows the camera"), &m_goto_follow);
		if (located && m_goto_map.picked())
		{
			ImGui::SameLine();
			if (ImGui::Button(STR_C("Go to the point clicked")))
				go_to_plan(to_plan(m_goto_map.point()));
		}
		if (located)
		{
			auto const degrees{editor_geo::to_degrees(camera)};
			ImGui::TextDisabled(STR_C("Camera: %.5f N, %.5f E (X %.0f, Y %.0f in PUWG 1992)"), degrees.x, degrees.y, camera.y, camera.x);
		}
	}

	if (ImGui::CollapsingHeader(STR_C("Towns and villages##goto"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
		auto const entered{ImGui::InputTextWithHint("##gotosearch", STR_C("Find a town or village..."), m_goto_query, sizeof(m_goto_query), ImGuiInputTextFlags_EnterReturnsTrue)};
		auto const query{folded(m_goto_query)};
		std::vector<int> found;
		if (query.size() >= 2)
		{
			// the names starting so first, then the ones holding it; the towns before the villages
			for (int pass = 0; pass < 2 && found.size() < 40; ++pass)
				for (int i = 0; i < static_cast<int>(std::size(places)) && found.size() < 40; ++i)
				{
					auto const name{folded(places[i].name)};
					auto const at{name.find(query)};
					if (at == std::string::npos || (pass == 0) != (at == 0))
						continue;
					found.push_back(i);
				}
			std::stable_sort(found.begin(), found.end(), [](int const A, int const B) { return places[A].city > places[B].city; });
		}
		if (std::find(found.begin(), found.end(), m_goto_found) == found.end())
			m_goto_found = found.empty() ? -1 : found.front();
		if (false == found.empty() && ImGui::BeginListBox("##gotofound", ImVec2(-FLT_MIN, std::min(6.0f, static_cast<float>(found.size()) + 0.4f) * ImGui::GetTextLineHeightWithSpacing())))
		{
			for (auto const i : found)
			{
				auto const label{format("%s%s, %s##%d", places[i].name, places[i].city ? STR_C(" - town") : "", places[i].where, i)};
				if (ImGui::Selectable(label.c_str(), i == m_goto_found))
				{
					m_goto_found = i;
					m_goto_follow = false;
					m_goto_map.pick({static_cast<double>(places[i].east), static_cast<double>(places[i].north)});
					m_goto_map.show({static_cast<double>(places[i].east), static_cast<double>(places[i].north)}, 8.0);
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && located)
					go_to_plan(to_plan({static_cast<double>(places[i].east), static_cast<double>(places[i].north)}));
			}
			ImGui::EndListBox();
		}
		else if (query.size() >= 2)
			ImGui::TextDisabled("%s", STR_C("Nothing found in the area of the network"));
		ImGui::BeginDisabled(m_goto_found < 0 || false == located);
		if (main_button(STR_C("Go to the place found"), ImVec2(-FLT_MIN, tall_button_height())) || (entered && m_goto_found >= 0 && located))
			go_to_plan(to_plan({static_cast<double>(places[m_goto_found].east), static_cast<double>(places[m_goto_found].north)}));
		ImGui::EndDisabled();
		ImGui::TextDisabled("%s", STR_C("The towns and villages of the area of the IRT network (Lower Silesia), PRNG 2021"));
	}

	if (ImGui::CollapsingHeader(STR_C("Places of the camera##goto"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(STR_C("Add the camera")).x - ImGui::GetStyle().FramePadding.x * 4.0f));
		auto const named{ImGui::InputTextWithHint("##placename", STR_C("Name of the place..."), m_camera_place_name, sizeof(m_camera_place_name), ImGuiInputTextFlags_EnterReturnsTrue)};
		ImGui::SameLine();
		if (ImGui::Button(STR_C("Add the camera")) || named)
		{
			camera_place place;
			place.position = Camera.Pos;
			place.angle = glm::dvec3{Camera.Angle};
			place.name = m_camera_place_name[0] != '\0' ? std::string{m_camera_place_name} : format(STR_C("place %zu"), m_camera_places.size() + 1);
			m_camera_places.push_back(std::move(place));
			m_camera_place_name[0] = '\0';
			write_camera_places();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("Keeps where the camera stands and where it looks; kept next to the scenery, in <scenery>.kamery"));
		if (m_camera_places.empty())
			ImGui::TextDisabled("%s", STR_C("None kept yet"));
		int removed{-1};
		for (int i = 0; i < static_cast<int>(m_camera_places.size()); ++i)
		{
			auto const &place{m_camera_places[i]};
			ImGui::PushID(i);
			if (ImGui::Button(STR_C("Jump##place")))
			{
				m_focus_active = false;
				Camera.Pos = place.position;
				Camera.Angle = glm::vec3{place.angle};
				Camera.Velocity = glm::dvec3{0.0};
				Global.pCamera = Camera;
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(place.name.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("x %.1f, y %.1f, z %.1f", place.position.x, place.position.y, place.position.z);
			ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x * 3.0f));
			if (ImGui::SmallButton("X"))
				removed = i;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C("Forget this place"));
			ImGui::PopID();
		}
		if (removed >= 0)
		{
			m_camera_places.erase(m_camera_places.begin() + removed);
			write_camera_places();
		}
	}
}
