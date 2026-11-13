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
#include <filesystem>
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "editor/editorFormat.hpp"
#include "world/Track.h"
#include "simulation/simulation.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "application/editorprojection.h"
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
#include <sstream>
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
	render_palette_entries();
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputText(STR_C("Material"), m_terrain_material, IM_ARRAYSIZE(m_terrain_material));
	material_drop(m_terrain_material, IM_ARRAYSIZE(m_terrain_material));
	ImGui::SameLine();
	if (ImGui::Button(STR_C("Add to the palette")) && m_terrain_material[0] != '\0')
		m_terrain_layer = m_streamer.layer(m_terrain_material, m_terrain_material_size);
	// the paint of all of it gone at once: a chunk with more than one material gets the first of the palette alone
	auto &clearing{m_paint_clearing};
	if (false == clearing.packs.empty())
	{
		ImGui::ProgressBar(1.0f - static_cast<float>(clearing.packs.size()) / std::max<std::size_t>(1, clearing.total), ImVec2(-FLT_MIN, 0.0f),
		                   format(STR_C("Clearing the paint: %zu of %zu files, %zu chunks"), clearing.total - clearing.packs.size(), clearing.total, clearing.chunks).c_str());
	}
	else if (change_button(STR_C("Clear the paint of the whole terrain...")))
		ImGui::OpenPopup("##clearpaint");
	if (ImGui::BeginPopup("##clearpaint"))
	{
		auto const &layers{m_streamer.manifest().layers};
		ImGui::TextWrapped("%s", STR_C("Every chunk of the terrain painted with more than one material gets the first material of the palette alone. "
		                               "The shape of the ground, the holes and the 3D grass stay; what was painted into the terrain goes, the track bed and the slopes too "
		                               "(their surfaces stay listed, to be painted again). A copy of the terrain files is taken first; Ctrl+Z doesn't take it back."));
		if (false == layers.empty())
			ImGui::TextDisabled(STR_C("Left everywhere: %s"), layers.front().material.c_str());
		if (main_button(STR_C("Clear it"), ImVec2(160.0f, 0.0f)))
		{
			clear_paint_start();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(STR_C("Cancel")))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void editor_mode::clear_paint_start()
{
	auto &clearing{m_paint_clearing};
	auto const directory{m_streamer.directory()};
	if (directory.empty())
		return;
	// what's edited and not saved yet goes to the files first, then the copy of them is taken
	std::string error;
	if (false == m_streamer.save(&error))
	{
		ui()->set_status(format(STR_C("The terrain couldn't be saved before clearing: %s"), error.c_str()));
		return;
	}
	auto folder{directory};
	while (false == folder.empty() && (folder.back() == '/' || folder.back() == '\\'))
		folder.pop_back();
	auto backup{folder + "_przed_czyszczeniem"};
	std::error_code ec;
	for (int n = 2; std::filesystem::exists(backup, ec); ++n)
		backup = folder + "_przed_czyszczeniem" + std::to_string(n);
	std::filesystem::copy(folder, backup, std::filesystem::copy_options::recursive, ec);
	if (ec)
	{
		ui()->set_status(format(STR_C("The copy of the terrain couldn't be made, nothing cleared: %s"), ec.message().c_str()));
		return;
	}
	clearing.backup = backup;
	clearing.packs = m_streamer.pack_keys();
	clearing.total = clearing.packs.size();
	clearing.chunks = 0;
}

void editor_mode::clear_paint_step()
{
	auto &clearing{m_paint_clearing};
	if (clearing.packs.empty())
		return;
	// a file a frame, written at once, so the chunks read for it can go from memory again
	auto const pack{clearing.packs.back()};
	clearing.packs.pop_back();
	clearing.chunks += m_streamer.clear_paint(pack, 0);
	std::string error;
	m_streamer.save(&error);
	if (clearing.packs.empty())
	{
		forget_ground();
		ui()->set_status(format(STR_C("Paint cleared: %zu chunks had more than one material, now the first one alone. A copy of before: %s"), clearing.chunks,
		                        clearing.backup.c_str()));
	}
}

// the group of a palette entry: set by hand (kept in palette_groups.txt beside the terrain), or guessed from the name of the material
std::string editor_mode::palette_group(std::size_t const Layer)
{
	auto const directory{m_streamer.directory()};
	if (m_palette_groups_dir != directory)
	{
		m_palette_groups_dir = directory;
		m_palette_groups.clear();
		std::ifstream file{directory + "palette_groups.txt"};
		std::string line;
		while (std::getline(file, line))
		{
			auto const tab{line.find('\t')};
			if (tab != std::string::npos)
				m_palette_groups[std::atoi(line.substr(0, tab).c_str())] = line.substr(tab + 1);
		}
	}
	if (auto const found{m_palette_groups.find(static_cast<int>(Layer))}; found != m_palette_groups.end() && false == found->second.empty())
		return found->second;
	auto const &layers{m_streamer.manifest().layers};
	if (Layer >= layers.size())
		return STR("Other");
	auto name{layers[Layer].material};
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
	auto const has = [&](std::initializer_list<char const *> Words) {
		return std::any_of(Words.begin(), Words.end(), [&](char const *Word) { return name.find(Word) != std::string::npos; });
	};
	if (has({"water", "woda"}))
		return STR("Water");
	if (has({"forest", "las", "leaves", "lisc", "mech", "moss", "sciolk"}))
		return STR("Forest");
	if (has({"gravel", "zwir", "tlucz", "ballast", "1435mm", "tpbps", "kamien", "stone"}))
		return STR("Gravel and ballast");
	if (has({"asphalt", "asfalt", "concrete", "beton", "cobble", "kostka", "bruk", "plyty"}))
		return STR("Asphalt and concrete");
	if (has({"dirt", "soil", "ziem", "ground", "mud", "bloto", "field", "pole", "sand", "piach", "piasek", "orka"}))
		return STR("Earth and fields");
	if (has({"gras", "traw", "meadow", "laka"}))
		return STR("Grass");
	return STR("Other");
}

void editor_mode::palette_group(std::size_t const Layer, std::string const &Group)
{
	palette_group(Layer); // read the file first, if it wasn't
	m_palette_groups[static_cast<int>(Layer)] = Group;
	std::ofstream file{m_streamer.directory() + "palette_groups.txt"};
	for (auto const &entry : m_palette_groups)
		file << entry.first << '\t' << entry.second << '\n';
}

// what's painted at the point of the terrain under the cursor: the materials and how much of each, and how many of the 8 a chunk
// holds it has. returns: false if there's no terrain under it
bool editor_mode::paint_at_cursor(std::vector<std::pair<std::uint16_t, int>> &Weights, std::size_t &Used) const
{
	glm::dvec3 world;
	if (false == terrain_cursor(world))
		return false;
	auto const *chunk{m_streamer.terrain_at(world.x, world.z)};
	if (chunk == nullptr)
		return false;
	auto const &data{chunk->data()};
	Weights.clear();
	Used = data.layer_count();
	if (data.paint <= 0)
	{
		Weights.emplace_back(data.layers[0], 100);
		return true;
	}
	auto const low{data.corner()};
	auto const step{heightmap::chunk_size / data.paint};
	auto const ix{std::clamp(static_cast<int>(std::lround((world.x - low.x) / step)), 0, data.paint)};
	auto const iz{std::clamp(static_cast<int>(std::lround((world.z - low.y) / step)), 0, data.paint)};
	for (std::size_t slot = 0; slot < Used; ++slot)
	{
		auto const weight{data.weight(slot, ix, iz)};
		if (weight > 0)
			Weights.emplace_back(data.layers[slot], (weight * 100 + 127) / 255);
	}
	std::sort(Weights.begin(), Weights.end(), [](auto const &A, auto const &B) { return A.second > B.second; });
	return true;
}

void editor_mode::render_palette_entries()
{
	auto const &layers{m_streamer.manifest().layers};
	if (layers.empty())
		return;
	m_terrain_layer = std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1);
	auto const order{m_streamer.layer_order()};
	if (layers[m_terrain_layer].hidden && false == order.empty())
		m_terrain_layer = order.front();
	auto const label_of = [&](std::size_t const I) {
		auto label{layers[I].size > 0.f ? format("%zu: %s (%.1f m)", I, layers[I].material.c_str(), layers[I].size) : format("%zu: %s", I, layers[I].material.c_str())};
		if (layers[I].rotation != 0.f)
			label += format(STR_C(", turned %.0f°"), layers[I].rotation);
		return label;
	};
	auto &io{ImGui::GetIO()};

	// what the brush does with it: mixes it in, covers with it fully, or takes the paint back to the first material
	ImGui::TextUnformatted(STR_C("Brush:"));
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("Mix##brush"), &m_paint_mode, 0);
	item_tooltip("The material blends into what's painted there, the more the longer you hold it");
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("Cover##brush"), &m_paint_mode, 1);
	item_tooltip("The material at once, all of it, under the brush: no mixing");
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("Erase##brush"), &m_paint_mode, 2);
	item_tooltip("Back to the first material of the palette, the one of the new chunks");

	// under the cursor: what's painted there, and the room for materials of its chunk
	{
		std::vector<std::pair<std::uint16_t, int>> weights;
		std::size_t used{0};
		if (paint_at_cursor(weights, used))
		{
			std::string text;
			for (auto const &entry : weights)
				text += format("%s%s %d%%", text.empty() ? "" : ", ", entry.first < layers.size() ? layers[entry.first].material.c_str() : "?", entry.second);
			ImGui::TextDisabled(STR_C("Under the cursor: %s"), text.c_str());
			bool const full{used >= heightmap::max_chunk_layers};
			ImGui::SameLine();
			if (full)
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), STR_C("| %zu/%zu materials: no room for another here"), used, heightmap::max_chunk_layers);
			else
				ImGui::TextDisabled(STR_C("| %zu/%zu materials"), used, heightmap::max_chunk_layers);
			// Alt+LMB on the terrain: the material most of it is
			if (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && false == io.WantCaptureMouse && false == weights.empty())
				m_terrain_layer = weights.front().first;
		}
	}
	// 1..9: the first nine of the palette as listed
	if (false == io.WantTextInput && m_terrain_tool == terrain_tool::paint)
		for (int k = 0; k < 9 && k < static_cast<int>(order.size()); ++k)
			if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + k), false) && false == io.KeyCtrl && false == io.KeyAlt)
				m_terrain_layer = order[k];

	static int removing{-1};
	static int into{0};
	bool open_remove{false};
	// the groups in the order their first entry is listed
	std::vector<std::string> groups;
	for (auto const i : order)
	{
		auto const group{palette_group(i)};
		if (std::find(groups.begin(), groups.end(), group) == groups.end())
			groups.push_back(group);
	}
	float constexpr tile{72.0f};
	auto const spacing{ImGui::GetStyle().ItemSpacing.x};
	for (auto const &group : groups)
	{
		ImGui::PushID(group.c_str());
		bool const open{ImGui::CollapsingHeader(group.c_str(), ImGuiTreeNodeFlags_DefaultOpen)};
		// an entry dropped on the name of a group goes to that group
		if (ImGui::BeginDragDropTarget())
		{
			if (auto const *payload{ImGui::AcceptDragDropPayload("PALETTE_ENTRY")})
				palette_group(static_cast<std::size_t>(*static_cast<int const *>(payload->Data)), group);
			ImGui::EndDragDropTarget();
		}
		if (open)
		{
			auto const width{ImGui::GetContentRegionAvail().x};
			float x{0.0f};
			for (std::size_t at = 0; at < order.size(); ++at)
			{
				auto const i{static_cast<std::size_t>(order[at])};
				if (palette_group(i) != group)
					continue;
				if (x > 0.0f && x + tile > width)
					x = 0.0f;
				else if (x > 0.0f)
					ImGui::SameLine();
				x += tile + spacing;
				ImGui::PushID(static_cast<int>(i));
				ImGui::BeginGroup();
				auto const corner{ImGui::GetCursorScreenPos()};
				if (ImGui::InvisibleButton("##tile", ImVec2(tile, tile)))
					m_terrain_layer = static_cast<int>(i);
				auto *draw{ImGui::GetWindowDrawList()};
				texture_browser::item item{layers[i].material, layers[i].material, true};
				auto const texture{texture_thumbnail(item)};
				if (texture != 0)
					draw->AddImage(static_cast<ImTextureID>(texture), corner, ImVec2(corner.x + tile, corner.y + tile), ImVec2(0, 1), ImVec2(1, 0));
				else
					draw->AddRectFilled(corner, ImVec2(corner.x + tile, corner.y + tile), IM_COL32(60, 60, 60, 255));
				bool const chosen{static_cast<int>(i) == m_terrain_layer};
				draw->AddRect(corner, ImVec2(corner.x + tile, corner.y + tile), chosen ? IM_COL32(255, 200, 40, 255) : IM_COL32(0, 0, 0, 160), 0.0f, 0, chosen ? 4.0f : 1.0f);
				if (at < 9)
					draw->AddText(ImVec2(corner.x + 4.0f, corner.y + 2.0f), IM_COL32(255, 255, 255, 230), std::to_string(at + 1).c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s\n%s", label_of(i).c_str(),
					                  i == 0 ? STR_C("Covers the new chunks and the ones not painted. Drag to move it; RMB: its settings") : STR_C("Drag to move it, onto the name of a group to put it there; RMB: its settings"));
				// moved by dragging: onto another tile it takes that place, onto the name of a group it goes to that group
				if (ImGui::BeginDragDropSource())
				{
					int const dragged{static_cast<int>(i)};
					ImGui::SetDragDropPayload("PALETTE_ENTRY", &dragged, sizeof(dragged));
					ImGui::TextUnformatted(layers[i].material.c_str());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget())
				{
					if (auto const *payload{ImGui::AcceptDragDropPayload("PALETTE_ENTRY")})
					{
						auto const dragged{*static_cast<int const *>(payload->Data)};
						auto const from{std::find(order.begin(), order.end(), static_cast<std::uint16_t>(dragged)) - order.begin()};
						auto const step{static_cast<int>(static_cast<std::ptrdiff_t>(at) - from)};
						if (step != 0)
							m_streamer.move_layer(static_cast<std::uint16_t>(dragged), step);
						palette_group(static_cast<std::size_t>(dragged), group);
					}
					ImGui::EndDragDropTarget();
				}
				// a texture dragged from the browser takes the place of the material, wherever it's painted
				std::string dropped;
				if (material_drop(dropped))
					m_streamer.layer_material(static_cast<std::uint16_t>(i), dropped);
				if (ImGui::BeginPopupContextItem("##entry"))
				{
					ImGui::TextUnformatted(label_of(i).c_str());
					float size{layers[i].size};
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::DragFloat(STR_C("Repeats every (m)##entry"), &size, 0.1f, 0.0f, 100.0f, size > 0.0f ? "%.1f" : STR_C("as the material says")))
						m_streamer.layer_size(static_cast<std::uint16_t>(i), std::max(0.0f, size));
					float rotation{layers[i].rotation};
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::SliderFloat(STR_C("Turned by (deg)##entry"), &rotation, -180.0f, 180.0f, "%.0f"))
						m_streamer.layer_rotation(static_cast<std::uint16_t>(i), rotation);
					static char material[128]{};
					if (ImGui::IsWindowAppearing())
						std::snprintf(material, sizeof(material), "%s", layers[i].material.c_str());
					ImGui::SetNextItemWidth(180.0f);
					ImGui::InputText(STR_C("Material##entry"), material, sizeof(material));
					ImGui::SameLine();
					if (ImGui::SmallButton(STR_C("Change##entry")) && material[0] != '\0')
						m_streamer.layer_material(static_cast<std::uint16_t>(i), material);
					item_tooltip("Everywhere it's painted, at once: no new entry, no room taken in the chunks");
					static char moved[64]{};
					if (ImGui::IsWindowAppearing())
						std::snprintf(moved, sizeof(moved), "%s", group.c_str());
					ImGui::SetNextItemWidth(180.0f);
					ImGui::InputText(STR_C("Group##entry"), moved, sizeof(moved));
					ImGui::SameLine();
					if (ImGui::SmallButton(STR_C("Move##entry")) && moved[0] != '\0')
						palette_group(i, moved);
					ImGui::Separator();
					ImGui::BeginDisabled(i == 0 || order.size() < 2);
					if (ImGui::MenuItem(STR_C("Off the palette...")))
					{
						removing = static_cast<int>(i);
						into = order.front() == i ? order[1] : order.front();
						open_remove = true;
					}
					ImGui::EndDisabled();
					ImGui::EndPopup();
				}
				ImGui::SetNextItemWidth(tile);
				auto short_name{layers[i].material.substr(layers[i].material.find_last_of('/') + 1)};
				if (short_name.size() > 11)
					short_name = short_name.substr(0, 10) + "~";
				ImGui::TextUnformatted(short_name.c_str());
				ImGui::EndGroup();
				ImGui::PopID();
			}
		}
		ImGui::PopID();
	}
	if (open_remove)
		ImGui::OpenPopup("##removelayer");
	if (ImGui::BeginPopup("##removelayer"))
	{
		if (removing >= 0 && static_cast<std::size_t>(removing) < layers.size())
		{
			ImGui::TextUnformatted(format(STR_C("\"%s\" off the palette"), layers[removing].material.c_str()).c_str());
			ImGui::TextDisabled("%s", STR_C("Where it's painted, the terrain gets instead:"));
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::BeginCombo("##into", label_of(static_cast<std::size_t>(into)).c_str()))
			{
				for (auto const other : order)
					if (static_cast<int>(other) != removing && ImGui::Selectable(label_of(other).c_str(), static_cast<int>(other) == into))
						into = other;
				ImGui::EndCombo();
			}
			if (change_button(STR_C("Take it off")))
			{
				if (m_streamer.hide_layer(static_cast<std::uint16_t>(removing), static_cast<std::uint16_t>(into)))
				{
					if (m_terrain_layer == removing)
						m_terrain_layer = into;
					ui()->set_status(STR_C("The material is off the palette, the terrain painted with it has the other one; Ctrl+S saves it"));
				}
				removing = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button(STR_C("Cancel")))
			{
				removing = -1;
				ImGui::CloseCurrentPopup();
			}
		}
		else
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void editor_mode::render_fields_ui()
{
	if (false == m_streamer.active())
	{
		ImGui::TextDisabled("%s", STR_C("The scenery has no terrain yet"));
		return;
	}
	auto const &layers{m_streamer.manifest().layers};
	ImGui::TextWrapped("%s", STR_C("LMB adds a point of the outline, Shift+LMB takes the last one back. The field gets the material chosen in the palette and is "
	                               "kept in the list below, where its material can be changed later"));
	if (false == layers.empty())
	{
		auto const &chosen{layers[std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1)]};
		ImGui::Text(STR_C("Material: %s (chosen in the palette above)"), chosen.material.c_str());
	}
	ImGui::Text(STR_C("Outline: %zu points"), m_paint_outline.size());
	int kind{m_cut_separate ? 1 : 0};
	ImGui::RadioButton(STR_C("Painted into the terrain##fields"), &kind, 0);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("The material goes into the paint of the chunks: no seam, the edge blends into the ground around; a chunk holds up to 8 materials"));
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("A surface of its own##fields"), &kind, 1);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("The ground is cut away within the outline and the field drawn over the hole as a mesh of its own, following the heights of the terrain:\n"
		                              "it doesn't take one of the 8 materials of a chunk, but its edge is sharp and the seam can show"));
	m_cut_separate = kind == 1;
	render_crop_options();
	ImGui::SeparatorText(STR_C("Shape of the outline"));
	render_outline_options();
	ImGui::SeparatorText(STR_C("Edge"));
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
	if (m_crop > 0)
		item_tooltip("With a crop: turned from along the rows by this much (0: the rows of the texture along the rows of the field)");
	else
		grass_choice(STR_C("3D grass under it##cut"), m_cut_grass);
	render_waves_options();
	ImGui::Spacing();
	ImGui::BeginDisabled(m_paint_outline.size() < 3 || layers.empty());
	if (main_button(STR_C("Cut out the field"), ImVec2(-FLT_MIN, tall_button_height())))
		run_busy(STR_C("Cutting out the field"), [this]() { cut_out_surface(); });
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", STR_C("The area within the outline gets the material chosen in the palette, turned by the angle above, with the transition\n"
		                              "and the noise above; it's kept in the list below, where its material can be changed later"));
	if (ImGui::Button(STR_C("Clear the outline##fields"), ImVec2(-FLT_MIN, 0.0f)))
	{
		m_paint_outline.clear();
		m_patch_reoutline.clear(); // the next cut is a new field
	}
	ImGui::Spacing();
	if (ImGui::TreeNodeEx(STR_C("Fields and surfaces cut out##fields"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		render_cut_surfaces();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode(STR_C("Natural slopes##fields")))
	{
		render_natural_slopes();
		ImGui::TreePop();
	}
	if (false == m_terrain_status.empty())
		ImGui::TextWrapped("%s", m_terrain_status.c_str());
}

void editor_mode::render_natural_slopes()
{
	auto &natural{m_natural};
	ImGui::TextWrapped("%s", STR_C("The embankments and the cuttings not drawn with a ruler: the formation and the edges of the track bed stay as designed (ST-T1-A6), "
	                               "the slopes and their foot wander, get gentler here and there and meet the ground softly"));
	auto drag = [](char const *Label, double &Value, float Speed, float Low, float High, char const *Format, char const *Tip) {
		float value{static_cast<float>(Value)};
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::DragFloat(Label, &value, Speed, Low, High, Format))
			Value = std::clamp(static_cast<double>(value), static_cast<double>(Low), static_cast<double>(High));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", Tip);
	};
	drag(STR_C("Foot wanders by (m)"), natural.wander, 0.05f, 0.0f, 5.0f, "%.2f", STR_C("How far the foot of the slope goes in and out of the line of the slope 1:1.5"));
	drag(STR_C("Waves along the line (m)"), natural.scale, 0.5f, 2.0f, 100.0f, "%.0f", STR_C("About how long the waves of the foot are along the line"));
	drag(STR_C("Gentler by up to"), natural.gentler, 0.01f, 0.0f, 0.6f, "%.2f", STR_C("0.2: the slope 1:1.5 gets as gentle as 1:1.8 here and there, never steeper than designed"));
	drag(STR_C("Soft foot (m/m)"), natural.soften, 0.01f, 0.0f, 2.0f, "%.2f", STR_C("Metres the foot is rounded off over, for a metre of the height of the slope: high embankments meet the ground softer"));
	drag(STR_C("Bumps on the face (m)"), natural.roughness, 0.01f, 0.0f, 0.5f, "%.2f", STR_C("Metres the face of the slope is uneven by, as earth settled for years"));
	ImGui::Spacing();
	render_scope("natural");
	ImGui::BeginDisabled(m_streamer.modifiers().empty());
	if (main_button(STR_C("Make the slopes natural"), ImVec2(-FLT_MIN, tall_button_height())))
		run_busy(STR_C("Making the slopes natural"), [this]() { naturalise_slopes(true); });
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", STR_C("Around the camera as far as set above, or in the whole scenery when that's ticked; the terrain is shaped anew. Ctrl+Z takes it back.\n"
		                              "The ground under the slopes comes from the heights before the modifiers (HBAS) the terrain keeps; a terrain without them has its slopes\n"
		                              "only pushed out and roughened, not made gentler. Paint the track bed and the slopes again after it"));
	if (change_button(STR_C("Back to the slopes as designed"), ImVec2(-FLT_MIN, 0.0f)))
		run_busy(STR_C("Bringing back the slopes as designed"), [this]() { naturalise_slopes(false); });
	ImGui::EndDisabled();
}

void editor_mode::render_cut_surfaces()
{
	auto const &patches{m_streamer.patches()};
	if (patches.empty())
		ImGui::TextDisabled("%s", STR_C("None yet"));
	auto const meshes{static_cast<std::size_t>(std::count_if(patches.begin(), patches.end(), [](heightmap::surface_patch const &Patch) { return false == Patch.paint; }))};
	// the button always there: with nothing to paint over it's greyed out and says why
	if (meshes > 0)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.3f, 1.0f));
		ImGui::TextWrapped(STR_C("%zu of them are cut out with a hole and a mesh of their own (the seam shows): paint them into the terrain"), meshes);
		ImGui::PopStyleColor();
	}
	else if (false == patches.empty())
		ImGui::TextDisabled("%s", STR_C("All of them are painted into the terrain already, none with a hole"));
	render_scope("convert");
	ImGui::BeginDisabled(meshes == 0);
	if (change_button(STR_C("Paint them into the terrain##convert")))
		run_busy(STR_C("Painting the surfaces into the terrain"), [this]() { convert_patches_to_paint(); });
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", meshes > 0 ? STR_C("Where it's set just above. The holes are closed and the surfaces painted with their materials. Ctrl+Z takes it back")
		                                   : STR_C("Nothing to paint over: no surface here is cut out with a hole and a mesh of its own"));
	render_patches_ui();
}

bool editor_mode::features_escape()
{
	auto const outlined{false == m_paint_outline.empty() || false == m_patch_reoutline.empty() || false == m_parcels.empty()};
	auto const chosen{m_patch_selected >= 0 || false == m_zielsko_preview.empty() || false == m_rails_plans.empty() || false == m_selection.empty() || m_node != nullptr};
	if (false == outlined && false == chosen)
		return false;
	m_paint_outline.clear();
	m_patch_reoutline.clear();
	m_parcels.clear();
	m_parcel_ids.clear();
	m_outline_stroke.clear();
	m_outline_moving = -1;
	m_outline_pressed = false;
	m_patch_selected = -1;
	m_zielsko_preview.clear();
	m_rails_plans.clear();
	m_selection.clear();
	m_node = nullptr;
	ui()->set_node(nullptr);
	return true;
}

void editor_mode::create_chunk(heightmap::chunk_key const &Key)
{
	if (m_streamer.manifest().georeferenced)
	{
		nmt_make({Key});
		m_terrain_status = m_nmt_state.status;
		return;
	}
	if (ensure_terrain() && add_chunk_from_ground(Key, m_terrain_spacing))
		m_terrain_status = STR_C("The chunk was made from the ground of the scenery");
}

void editor_mode::render_create_chunks()
{
	if (false == m_streamer.active())
		return;
	auto const home{heightmap::chunk_at(Camera.Pos.x, Camera.Pos.z)};
	double ground{Camera.Pos.y - 50.0};
	m_streamer.height_at(Camera.Pos.x, Camera.Pos.z, ground);
	screen_projection const projection;
	auto const *viewport{ImGui::GetMainViewport()};
	int const reach{3};
	for (auto x{home.first - reach}; x <= home.first + reach; ++x)
		for (auto z{home.second - reach}; z <= home.second + reach; ++z)
		{
			heightmap::chunk_key const key{x, z};
			if (false == m_streamer.absent(key))
				continue;
			glm::dvec3 const middle{(x + 0.5) * heightmap::chunk_size, ground, (z + 0.5) * heightmap::chunk_size};
			ImVec2 at;
			if (false == projection.project(middle, at) || at.x < viewport->WorkPos.x || at.y < viewport->WorkPos.y || at.x > viewport->WorkPos.x + viewport->WorkSize.x || at.y > viewport->WorkPos.y + viewport->WorkSize.y)
				continue;
			ImGui::SetNextWindowPos(at, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			auto const name{format("##createchunk%d_%d", x, z)};
			auto const flags{ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav};
			if (ImGui::Begin(name.c_str(), nullptr, flags))
			{
				if (main_button(STR_C("CREATE##chunk")))
					create_chunk(key);
				item_tooltip(STR_C("No terrain here: the chunk is made from the terrain model (NMT) where the terrain knows its place, else from the ground of the scenery and the edges of the chunks beside it"));
			}
			ImGui::End();
		}
}

void editor_mode::choose_feature_tool(feature_tool const Tool)
{
	ui()->show_tool_options();
	m_feature_tool = Tool;
	// the outline of the trees and of the fields is drawn as with Fill, the fields with their own tool
	m_terrain_tool = Tool == feature_tool::fields ? terrain_tool::fields : Tool == feature_tool::zielsko ? terrain_tool::fill : terrain_tool::none;
}

void editor_mode::render_features_ui()
{
	// the tools of GeoTools as tabs, as in the toolbar
	struct tab
	{
		char const *label;
		feature_tool tool;
	};
	tab const tabs[]{{STR_C("Fields##geotools"), feature_tool::fields},
	                 {STR_C("Trees##geotools"), feature_tool::zielsko},
	                 {STR_C("Tracks##geotools"), feature_tool::rails},
	                 {STR_C("Track bed and slopes##geotools"), feature_tool::track_bed},
	                 {STR_C("Go to##geotools"), feature_tool::go_to}};
	if (ImGui::BeginTabBar("##geotools"))
	{
		// a tool chosen in the toolbar (or by the work area) shows its tab; a tab clicked chooses its tool
		auto const outside{m_feature_tab_shown != m_feature_tool};
		for (auto const &entry : tabs)
		{
			auto const flags{outside && entry.tool == m_feature_tool ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None};
			if (ImGui::BeginTabItem(entry.label, nullptr, flags))
			{
				if (false == outside && entry.tool != m_feature_tool)
					choose_feature_tool(entry.tool);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
		m_feature_tab_shown = m_feature_tool;
	}
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
	if (m_feature_tool == feature_tool::zielsko)
	{
		render_zielsko_ui();
		return;
	}
	if (m_feature_tool == feature_tool::rails)
	{
		render_rails_ui();
		return;
	}
	if (m_feature_tool == feature_tool::fields)
	{
		render_fields_ui();
		return;
	}
	auto const &layers{m_streamer.manifest().layers};
	if (ImGui::CollapsingHeader(STR_C("Material##features"), ImGuiTreeNodeFlags_DefaultOpen))
		render_material_palette();

	{
		if (ImGui::CollapsingHeader(STR_C("Track bed and slopes"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto &settings{m_track_bed};
			ImGui::TextWrapped("%s", STR_C("Along the formation modifiers of the terrain (the ones the tracks shape it with) the track bed, the embankments and the cuttings "
			                               "are cut out as surfaces of their own, each up to the length below, to be given textures of their own. "
			                               "The embankments stay shaped by the modifiers; the pieces keep the heights of the terrain"));
			ImGui::TextDisabled(STR_C("Formation modifiers: %zu"), m_streamer.modifiers().size());

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
			grass_choice(STR_C("3D grass on the track bed##bed"), settings.bed_grass);
			grass_choice(STR_C("3D grass on the slopes##bed"), settings.slope_grass);
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
			render_scope("bed");
			ImGui::BeginDisabled(m_streamer.modifiers().empty());
			if (main_button(STR_C("Cut the track bed and the slopes"), ImVec2(-FLT_MIN, tall_button_height())))
				run_busy(STR_C("Cutting the track bed and the slopes"), [this]() { cut_track_bed(); });
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", STR_C("The pieces cut before along the same modifiers are made anew. Ctrl+Z takes it back"));
		}
	}
	if (ImGui::CollapsingHeader(STR_C("Natural slopes"), ImGuiTreeNodeFlags_DefaultOpen))
		render_natural_slopes();
	if (ImGui::CollapsingHeader(STR_C("Surfaces cut out of the terrain"), ImGuiTreeNodeFlags_DefaultOpen))
		render_cut_surfaces();
	render_film_ui();
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
	std::size_t done_modifiers{0};
	for (auto const index : chosen_modifiers)
	{
		// Esc: the pieces cut so far are kept
		if (false == editor_busy_progress(0.5 * static_cast<double>(done_modifiers++) / static_cast<double>(chosen_modifiers.size())))
			break;
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
	// the ones made before with a hole of their own get it closed, the holes of other fields over them kept
	std::vector<heightmap::surface_patch> closed;
	for (auto const &patch : patches)
		if (false == patch.paint && replaced(patch))
		{
			m_streamer.cut_field(patch.outline, false);
			closed.push_back(patch);
		}
	patches.erase(std::remove_if(patches.begin(), patches.end(), replaced), patches.end());
	for (auto const &gone : closed)
		m_streamer.recut_fields_over(patches, gone);
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
		patch.grass = made.lift > 0.025f ? settings.bed_grass : settings.slope_grass;
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

namespace
{
// what can be sown in a field cut out: its texture, its 3D grass, the shape of the ground under it
struct crop_preset
{
	char const *name;
	char const *about;
	char const *material; // of the textures of the simulator (uprawy/: made by tools/zielsko/uprawy.py from them)
	float size; // metres the texture repeats at
	// the 3D grass; a height of 0: none, the ground bare
	glm::vec3 base;
	glm::vec3 tip;
	float height;
	float variation;
	float width;
	float density;
	float bend;
	float seasonal;
	// the ground
	bool field; // the cultivated field of terrain_waves, or rolling hills
	float rows; // metres, of the rows from the furrow to the ridge
	float row_length; // metres, from a row to the next
	float roughness;
	float tramlines;
	float lands;
	float headland;
};


std::array<crop_preset, 15> const &crop_presets()
{
	static std::array<crop_preset, 15> const presets{{
	    {"Ploughed", "Turned over in autumn or before sowing: dark furrows of soil, clods, the lands with their dead furrows; no grass", "uprawy/zaorane", 6.f, {}, {}, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
	     true, 0.16f, 1.5f, 0.5f, 0.f, 30.f, 12.f},
	    {"Harrowed and sown", "Smooth, crumbly soil just sown, the tramlines already driven", "uprawy/zbronowane", 6.f, {}, {}, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, true, 0.03f, 1.5f,
	     0.15f, 24.f, 30.f, 18.f},
	    {"Winter crop, young", "Wheat, rye or barley sown in autumn: a low green carpet in rows over the soil, from October till April", "uprawy/ozimina", 6.f, {0.17f, 0.30f, 0.08f},
	     {0.34f, 0.55f, 0.15f}, 0.12f, 0.3f, 0.006f, 220.f, 0.25f, 0.3f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Cereal, green", "A cereal grown up but still green, May and June: dense knee-high stalks", "uprawy/zboze_zielone", 6.f, {0.15f, 0.29f, 0.07f}, {0.37f, 0.57f, 0.17f}, 0.55f, 0.15f,
	     0.010f, 180.f, 0.15f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Cereal, heading", "Ears out, the green going yellow: the end of June", "uprawy/zboze_klos", 6.f, {0.24f, 0.35f, 0.12f}, {0.66f, 0.68f, 0.32f}, 0.85f, 0.12f, 0.010f, 180.f,
	     0.12f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Wheat, ripe", "Golden, waist-high, ears upright: July and August till the harvest", "uprawy/pszenica_dojrzala", 6.f, {0.52f, 0.42f, 0.20f}, {0.90f, 0.77f, 0.42f}, 0.85f,
	     0.10f, 0.010f, 180.f, 0.08f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Barley, ripe", "Paler than wheat, lower, the ears bowed: ripe from the start of July", "uprawy/jeczmien_dojrzaly", 6.f, {0.60f, 0.52f, 0.32f}, {0.93f, 0.86f, 0.60f}, 0.7f,
	     0.12f, 0.010f, 180.f, 0.35f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Rye, ripe", "The tallest of them, greyish straw: a man's height, the poorer sandy soils", "uprawy/zyto_dojrzale", 6.f, {0.45f, 0.42f, 0.30f}, {0.80f, 0.72f, 0.50f}, 1.3f,
	     0.15f, 0.010f, 160.f, 0.2f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Oats, ripe", "Light, loose panicles instead of ears, a greenish straw", "uprawy/owies_dojrzaly", 6.f, {0.50f, 0.48f, 0.33f}, {0.88f, 0.84f, 0.60f}, 0.9f, 0.15f, 0.010f,
	     150.f, 0.3f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Rape in flower", "Yellow over the whole field, high: the end of April and May", "uprawy/rzepak_kwitnacy", 6.f, {0.20f, 0.35f, 0.10f}, {1.00f, 0.88f, 0.15f}, 1.3f, 0.15f,
	     0.020f, 110.f, 0.2f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Rape, ripe", "Brown pods on dry stalks: July before the harvest", "uprawy/rzepak_dojrzaly", 6.f, {0.35f, 0.32f, 0.18f}, {0.55f, 0.48f, 0.25f}, 1.3f, 0.15f, 0.015f, 110.f,
	     0.2f, 0.f, true, 0.03f, 1.5f, 0.1f, 24.f, 30.f, 18.f},
	    {"Stubble", "After the harvest: short straw in rows over the soil, the tramlines still there", "uprawy/sciernisko", 6.f, {0.60f, 0.50f, 0.30f}, {0.85f, 0.76f, 0.50f}, 0.12f,
	     0.3f, 0.004f, 200.f, 0.05f, 0.f, true, 0.02f, 1.5f, 0.15f, 24.f, 30.f, 18.f},
	    {"High meadow", "Mown once or twice a year, the grass and the herbs up to the knees, the ground gently uneven", "uprawy/laka_wysoka", 5.f, {0.20f, 0.32f, 0.09f},
	     {0.55f, 0.62f, 0.28f}, 0.6f, 0.5f, 0.010f, 200.f, 0.45f, 1.f, false, 0.4f, 40.f, 0.3f, 0.f, 0.f, 0.f},
	    {"Low meadow, pasture", "Grazed or mown often: short dense grass, tussocks and trodden places", "uprawy/pastwisko", 5.f, {0.20f, 0.33f, 0.09f}, {0.45f, 0.60f, 0.22f}, 0.1f, 0.4f,
	     0.006f, 260.f, 0.3f, 1.f, false, 0.3f, 30.f, 0.45f, 0.f, 0.f, 0.f},
	    {"Fallow", "Left unsown: grass, weeds and dry stalks of last year, tussocks", "uprawy/ugor", 5.f, {0.30f, 0.33f, 0.15f}, {0.62f, 0.58f, 0.35f}, 0.45f, 0.6f, 0.010f, 150.f,
	     0.45f, 1.f, false, 0.3f, 25.f, 0.45f, 0.f, 0.f, 0.f},
	}};
	return presets;
}
} // namespace

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
	patch.paint = false == m_cut_separate;
	patch.feather = m_cut_feather;
	patch.noise = m_cut_noise;
	patch.noise_scale = m_cut_noise_scale;
	patch.grass = m_cut_grass;
	patch.outline = shaped_outline();
	patch.shape = outline_controls();
	if (patch.outline.size() < 3)
		return;
	// the rows along the longest side, the texture along the rows (the rows of the textures of the fields run up the picture)
	if (m_waves.on && m_waves.kind == terrain_waves::field && m_waves.auto_direction)
		m_waves.angle = longest_side(patch.outline);
	auto const &presets{crop_presets()};
	if (m_crop > 0 && m_crop <= static_cast<int>(presets.size()))
	{
		auto const &preset{presets[m_crop - 1]};
		patch.material = preset.material;
		patch.size = preset.size;
		if (preset.field)
			patch.rotation = static_cast<float>(std::remainder(static_cast<double>(m_waves.angle) + 90.0 + m_paint_rotation, 360.0));
		patch.grass = -1;
		if (preset.height > 0.f)
		{
			// its grass from the palette, added the first time
			auto const grass_name{STR(preset.name)};
			auto const &kinds{m_streamer.grass_kinds()};
			auto const found{std::find_if(kinds.begin(), kinds.end(), [&](heightmap::grass_def const &Kind) { return Kind.name == grass_name; })};
			if (found != kinds.end())
				patch.grass = static_cast<int>(found - kinds.begin());
			else
			{
				heightmap::grass_def kind;
				kind.name = grass_name;
				kind.base = preset.base;
				kind.tip = preset.tip;
				kind.height = preset.height;
				kind.variation = preset.variation;
				kind.width = preset.width;
				kind.density = preset.density;
				kind.bend = preset.bend;
				kind.dry = preset.tip;
				kind.seasonal = preset.seasonal;
				patch.grass = m_streamer.add_grass_kind(std::move(kind));
			}
		}
	}
	if (m_waves.on && m_waves.kind == terrain_waves::field && m_waves.tramlines >= 1.0f && patch.paint)
		patch.field = format("tram=%g angle=%g head=%g drift=%g soil=uprawy/zbronowane", m_waves.tramlines, m_waves.angle, m_waves.headland, m_waves.drift);
	auto patches{m_streamer.patches()};
	// the outline of a field put back to be drawn again: that field gets it, the rest of it as it was
	auto const again{std::find_if(patches.begin(), patches.end(), [this](heightmap::surface_patch const &Other) { return Other.name == m_patch_reoutline; })};
	m_patch_reoutline.clear();
	if (again != patches.end())
	{
		auto const old{*again};
		auto const index{static_cast<std::size_t>(again - patches.begin())};
		auto outline{std::move(patch.outline)};
		auto shape{std::move(patch.shape)};
		patch = old;
		patch.outline = std::move(outline);
		patch.shape = std::move(shape);
		terrain_history_step();
		if (m_waves.on && apply_waves(patch.outline))
			forget_ground();
		auto const name{patch.name};
		patches[index] = patch;
		if (false == patch.paint)
		{
			m_streamer.cut_field(old.outline, false);
			m_streamer.recut_fields_over(patches, old);
			m_streamer.cut_field(patch.outline, true);
			m_streamer.patches(std::move(patches));
			forget_ground();
		}
		else
			repaint_patches(std::move(patches), {index}, {old});
		terrain_history_step();
		m_patch_selected = static_cast<int>(index);
		m_paint_outline.clear();
		m_terrain_status = format(STR_C("%s outlined again, Ctrl+Z puts it back"), name.c_str());
		return;
	}
	// a name not taken yet
	for (int number = static_cast<int>(patches.size()) + 1;; ++number)
	{
		patch.name = format(STR_C("field %d"), number);
		if (std::none_of(patches.begin(), patches.end(), [&](heightmap::surface_patch const &Other) { return Other.name == patch.name; }))
			break;
	}
	auto const name{patch.name};
	auto const material{patch.material};
	// the waves of the ground and the paint are one step of the undo
	terrain_history_step();
	auto const waved{m_waves.on && apply_waves(patch.outline)};
	if (waved)
		forget_ground();
	auto const separate{false == patch.paint};
	auto const outline{patch.outline};
	patches.push_back(std::move(patch));
	auto const index{patches.size() - 1};
	if (separate)
	{
		// the ground cut away under it, the field drawn over the hole with the heights the terrain has there
		m_streamer.cut_field(outline, true);
		m_streamer.patches(std::move(patches));
		forget_ground();
	}
	else
		repaint_patches(std::move(patches), {index});
	terrain_history_step();
	m_patch_selected = static_cast<int>(index);
	// the chunks it reached with no terrain, told of after; looked at while the outline is still there
	m_outline_missing_at = -1.0;
	auto const missing{outline_missing_chunks().size()};
	m_paint_outline.clear();
	m_outline_missing_at = -1.0;
	m_terrain_status = format(STR_C("Cut out as %s with %s, Ctrl+Z puts it back"), name.c_str(), material.c_str());
	if (missing > 0)
		m_terrain_status += format(STR_C(". %zu chunks under the outline have no terrain: the field ends at its edge until they're added (then it goes onto them)"), missing);
}

namespace
{
// value noise of the waves, 0..1, smooth, the same for the same place: the waves of the ground don't change as it's cut again
double wave_hash(std::int64_t const X, std::int64_t const Z)
{
	auto h{static_cast<std::uint64_t>(X) * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint64_t>(Z) * 0xC2B2AE3D27D4EB4Full};
	h ^= h >> 31;
	h *= 0xBF58476D1CE4E5B9ull;
	h ^= h >> 29;
	return static_cast<double>(h & 0xffffff) / static_cast<double>(0xffffff);
}

double wave_noise(double const X, double const Z)
{
	auto const ix{static_cast<std::int64_t>(std::floor(X))};
	auto const iz{static_cast<std::int64_t>(std::floor(Z))};
	auto const fx{X - std::floor(X)};
	auto const fz{Z - std::floor(Z)};
	auto const sx{fx * fx * (3.0 - 2.0 * fx)};
	auto const sz{fz * fz * (3.0 - 2.0 * fz)};
	auto const a{wave_hash(ix, iz) + (wave_hash(ix + 1, iz) - wave_hash(ix, iz)) * sx};
	auto const b{wave_hash(ix, iz + 1) + (wave_hash(ix + 1, iz + 1) - wave_hash(ix, iz + 1)) * sx};
	return a + (b - a) * sz;
}

// a few octaves of it, 0..1
double wave_fractal(double const X, double const Z, int const Octaves)
{
	double sum{0.0};
	double weight{1.0};
	double total{0.0};
	double scale{1.0};
	for (int i = 0; i < Octaves; ++i)
	{
		sum += wave_noise(X * scale + i * 17.3, Z * scale - i * 9.1) * weight;
		total += weight;
		weight *= 0.5;
		scale *= 2.0;
	}
	return sum / total;
}

// true if Point lies within Outline; Distance: metres from it to the nearest side
bool within_outline(std::vector<glm::dvec2> const &Outline, glm::dvec2 const &Point, double &Distance)
{
	bool inside{false};
	auto nearest{std::numeric_limits<double>::max()};
	for (std::size_t i = 0, j = Outline.size() - 1; i < Outline.size(); j = i++)
	{
		auto const &a{Outline[i]};
		auto const &b{Outline[j]};
		if ((a.y > Point.y) != (b.y > Point.y) && Point.x < (b.x - a.x) * (Point.y - a.y) / (b.y - a.y) + a.x)
			inside = !inside;
		auto const run{b - a};
		auto const run2{glm::dot(run, run)};
		auto const t{run2 > 1e-12 ? std::clamp(glm::dot(Point - a, run) / run2, 0.0, 1.0) : 0.0};
		nearest = std::min(nearest, glm::length(Point - (a + run * t)));
	}
	Distance = nearest;
	return inside;
}

double smooth01(double T)
{
	T = std::clamp(T, 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

// the layout of a cultivated field, the same for its relief and for its paint: the rows driven along the angle, never quite
// straight, the headland along the edge with its ragged inner side, the tramlines of the sprayer
struct field_layout
{
	glm::dvec2 centre{0.0};
	glm::dvec2 along{1.0, 0.0};
	glm::dvec2 across{0.0, 1.0};
	double drift{0.8};
	double headland{18.0};
	double tramlines{24.0};

	field_layout(std::vector<glm::dvec2> const &Outline, double const Angle, double const Drift, double const Headland, double const Tramlines)
	    : drift{std::max(0.0, Drift)}, headland{std::max(0.0, Headland)}, tramlines{std::max(0.0, Tramlines)}
	{
		glm::dvec2 low{std::numeric_limits<double>::max()}, high{-std::numeric_limits<double>::max()};
		for (auto const &point : Outline)
		{
			low = glm::min(low, point);
			high = glm::max(high, point);
		}
		centre = (low + high) * 0.5;
		auto const radians{glm::radians(Angle)};
		along = {std::cos(radians), std::sin(radians)};
		across = {-along.y, along.x};
	}
	// metres across the rows, the rows wandering sideways as they were driven: a slow bend over a hundred metres or so
	double across_rows(glm::dvec2 const &Point) const
	{
		auto const u{glm::dot(Point - centre, across)};
		auto const v{glm::dot(Point - centre, along)};
		return u + drift * (wave_fractal(v / 110.0 + 0.31, u / 600.0 + 7.7, 2) * 2.0 - 1.0) * 1.6;
	}
	// how far in from the edge the headland goes here: its inner side ragged, where the rows end
	double headland_at(glm::dvec2 const &Point) const
	{
		return headland > 0.0 ? headland + 1.5 * (wave_noise(Point.x / 7.0 + 3.3, Point.y / 7.0 - 1.1) * 2.0 - 1.0) : 0.0;
	}
	// 0 in the headland, 1 in the body of the field, over 3 m between them
	double body(glm::dvec2 const &Point, double const Edge) const
	{
		return headland > 0.0 ? smooth01((Edge - headland_at(Point)) / 3.0 + 0.5) : 1.0;
	}
	// 0..1, how much of a rut of the tramlines lies at Point, Edge metres in from the outline: the two ruts 1.8 m apart, each
	// Width metres from its middle to where it ends; in the headland the tramline goes round the field in its middle
	double rut(glm::dvec2 const &Point, double const Edge, double const Width) const
	{
		if (tramlines <= 0.0)
			return 0.0;
		auto const ring{headland > 0.0 ? Edge - headland * 0.5 : 1e9};
		auto const u{across_rows(Point)};
		auto const line{u - tramlines * (std::floor(u / tramlines) + 0.5)};
		auto const at = [&](double const D) { return smooth01(1.0 - std::abs(std::abs(D) - 0.9) / std::max(0.05, Width)); };
		auto const inner{body(Point, Edge)};
		return std::max(at(ring) * (1.0 - inner), Edge > headland ? at(line) * inner : 0.0);
	}
};

// the field of a surface, as heightmap::surface_patch::field keeps it: "tram=24 angle=12.5 head=18 drift=0.8 soil=farm03"
struct field_spec
{
	double tramlines{0.0};
	double angle{0.0};
	double headland{0.0};
	double drift{0.0};
	std::string soil{"farm03"};
};

field_spec read_field(std::string const &Text)
{
	field_spec spec;
	std::istringstream words{Text};
	std::string word;
	while (words >> word)
	{
		auto const equals{word.find('=')};
		if (equals == std::string::npos)
			continue;
		auto const key{word.substr(0, equals)};
		auto const value{word.substr(equals + 1)};
		if (key == "soil")
			spec.soil = value;
		else if (key == "tram")
			spec.tramlines = std::atof(value.c_str());
		else if (key == "angle")
			spec.angle = std::atof(value.c_str());
		else if (key == "head")
			spec.headland = std::atof(value.c_str());
		else if (key == "drift")
			spec.drift = std::atof(value.c_str());
	}
	return spec;
}
} // namespace

void editor_mode::render_waves_options()
{
	auto &waves{m_waves};
	ImGui::SeparatorText(STR_C("Waves of the ground"));
	ImGui::Checkbox(STR_C("Make the ground under the surface wavy"), &waves.on);
	item_tooltip("The heights of the terrain within the outline get the waves chosen below, fading in from the edge: a field ploughed into furrows, a meadow of gentle hills, a hollow. Ctrl+Z takes them back with the surface");
	if (false == waves.on)
		return;
	char const *const kinds[]{STR_C("Rolling hills"), STR_C("Furrows of a field"), STR_C("Bumps and molehills"), STR_C("Long waves"), STR_C("A hollow or a mound"),
	                          STR_C("A grid of hummocks"), STR_C("Strips of fields"), STR_C("A cultivated field")};
	char const *const tips[]{STR_C("Gentle hills of all sizes, as a meadow or a pasture has them"), STR_C("Ridges and furrows side by side, along the angle below, as a ploughed field or the beds of a garden"),
	                         STR_C("Small mounds scattered about, as molehills or tussocks"), STR_C("Long even waves across the angle below, as a field rolling over a hill or dunes"),
	                         STR_C("The middle lowered (a negative height raises it): a hollow by a pond, a dip in a field, or a mound"),
	                         STR_C("Waves along the angle and across it at once: hummocks in rows, as a drained meadow or an orchard on ridges"),
	                         STR_C("Long strips along the angle, each a little higher or lower than the next, with banks between them: the fields of a village cut up between farms"),
	                         STR_C("A field as it's worked: the rows never quite straight, the lands with a crown and dead furrows, the headland along the edge "
	                               "with the rows going round, the ruts of the tramlines; the direction along the longest side")};
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo(STR_C("Kind##waves"), kinds[std::clamp(waves.kind, 0, static_cast<int>(terrain_waves::count) - 1)]))
	{
		for (int i = 0; i < terrain_waves::count; ++i)
		{
			if (ImGui::Selectable(kinds[i], i == waves.kind))
			{
				waves.kind = i;
				// the sizes a kind usually has
				static float const heights[]{0.6f, 0.25f, 0.2f, 1.0f, -0.8f, 0.3f, 0.6f, 0.0f};
				static float const lengths[]{40.0f, 1.5f, 3.0f, 60.0f, 0.0f, 6.0f, 25.0f, 1.5f};
				waves.height = heights[i];
				if (lengths[i] > 0.0f)
					waves.length = lengths[i];
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tips[i]);
		}
		ImGui::EndCombo();
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Height (m)##waves"), &waves.height, 0.01f, -5.0f, 5.0f, "%.2f");
	item_tooltip("From the bottom of a wave to its top; for the hollow how deep its middle goes, negative: a mound");
	if (waves.kind != terrain_waves::hollow)
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Length of a wave (m)##waves"), &waves.length, 0.1f, 0.5f, 300.0f, "%.1f");
		waves.length = std::clamp(waves.length, 0.5f, 300.0f);
		item_tooltip("From one top to the next: furrows 0.7-2 m, the beds of a garden 1.5-3 m, hills 20-100 m");
	}
	if (waves.kind == terrain_waves::field)
		render_field_layout();
	if (waves.kind == terrain_waves::furrows || waves.kind == terrain_waves::waves || waves.kind == terrain_waves::grid || waves.kind == terrain_waves::strips ||
	    (waves.kind == terrain_waves::field && false == waves.auto_direction))
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat(STR_C("Direction (deg)##waves"), &waves.angle, -180.0f, 180.0f, "%.0f");
		item_tooltip("Which way the furrows or the ridges of the waves run, counterclockwise from the x axis of the scenery; the texture turned by the same angle lies along them");
		ImGui::SameLine();
		if (ImGui::SmallButton(STR_C("as the texture")))
			waves.angle = m_paint_rotation;
	}
	if (waves.kind != terrain_waves::hollow)
	{
		char const *const shapes[]{STR_C("Rounded"), STR_C("Sharp ridges"), STR_C("Sharp valleys"), STR_C("Terraces"), STR_C("Flat tops")};
		char const *const shape_tips[]{STR_C("Every wave a gentle curve, as before"), STR_C("Tops pointed, sides straight: ploughed ridges, a dune crest, a bank between fields"),
		                               STR_C("Bottoms pointed: drainage ditches, gullies cut by water, the furrow left by a plough"),
		                               STR_C("The slopes broken into flat steps: terraced fields on a hillside, a quarry, the shelves of a vineyard"),
		                               STR_C("Tops and bottoms cut flat with steep sides between: beds of a garden, fields on raised strips, plateaus")};
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::BeginCombo(STR_C("Profile##waves"), shapes[std::clamp(waves.shape, 0, static_cast<int>(terrain_waves::profiles) - 1)]))
		{
			for (int i = 0; i < terrain_waves::profiles; ++i)
			{
				if (ImGui::Selectable(shapes[i], i == waves.shape))
					waves.shape = i;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", shape_tips[i]);
			}
			ImGui::EndCombo();
		}
		item_tooltip("The cross-section of a wave; the rounded one makes the fields look soft, the others give them edges");
		if (waves.shape != terrain_waves::smooth)
		{
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderFloat(STR_C("Sharpness##waves"), &waves.sharpness, 0.0f, 1.0f, "%.2f");
			item_tooltip("0: the profile barely changed from the rounded one; 1: its edges and corners as sharp as they get");
		}
		if (waves.shape == terrain_waves::terraces)
		{
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderInt(STR_C("Steps##waves"), &waves.steps, 2, 12);
			item_tooltip("How many flat steps a slope is broken into, from the bottom of a wave to its top");
		}
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat(STR_C("Roughness##waves"), &waves.roughness, 0.0f, 1.0f, "%.2f");
	item_tooltip("Clods and hummocks of about 3 m over the whole surface, up to 20 cm high: a ploughed field, a trodden pasture.\n"
	             "The terrain under the surface gets a point every metre for them (and for waves shorter than 15 m), the files grow");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat(STR_C("Differences between them##waves"), &waves.variation, 0.0f, 1.0f, "%.2f");
	item_tooltip("0: every wave alike, as drawn; 1: waves of quite different heights and places, as nature has them");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Fading in from the edge (m)##waves"), &waves.edge, 0.1f, 0.0f, 100.0f, "%.1f");
	waves.edge = std::max(0.0f, waves.edge);
	item_tooltip("Metres inside the outline over which the waves grow from nothing, so the ground around meets the surface without a step");
}

bool editor_mode::apply_waves(std::vector<glm::dvec2> const &Outline)
{
	auto const flat{std::abs(m_waves.height) < 0.001f && m_waves.roughness < 0.001f};
	auto const field_flat{m_waves.kind != terrain_waves::field || ((m_waves.land_height < 0.001f || m_waves.lands < 1.0f) && (m_waves.ruts < 0.001f || m_waves.tramlines < 1.0f))};
	if (Outline.size() < 3 || (flat && field_flat))
		return false;
	auto const waves{m_waves};
	glm::dvec2 low{std::numeric_limits<double>::max()}, high{-std::numeric_limits<double>::max()};
	for (auto const &point : Outline)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	auto const centre{(low + high) * 0.5};
	auto const radians{glm::radians(static_cast<double>(waves.angle))};
	glm::dvec2 const along{std::cos(radians), std::sin(radians)};
	glm::dvec2 const across{-along.y, along.x};
	auto const length{std::max(0.5, static_cast<double>(waves.length))};
	auto const variation{std::clamp(static_cast<double>(waves.variation), 0.0, 1.0)};
	auto const edge{std::max(0.0, static_cast<double>(waves.edge))};
	// for the hollow: how far in the middle of it is, the most distance from the edge found in the outline
	double deepest{edge};
	auto const inside_by = [&](glm::dvec2 const &Point, double &Distance) { return within_outline(Outline, Point, Distance); };
	if (waves.kind == terrain_waves::hollow)
	{
		// a coarse look for the middle, every few metres
		auto const step{std::max(1.0, std::max(high.x - low.x, high.y - low.y) / 60.0)};
		auto const rows{static_cast<int>(std::floor((high.y - low.y) / step))};
		auto const columns{static_cast<int>(std::floor((high.x - low.x) / step))};
		for (int row = 0; row <= rows; ++row)
			for (int column = 0; column <= columns; ++column)
				if (double distance; inside_by({low.x + column * step, low.y + row * step}, distance))
					deepest = std::max(deepest, distance);
	}
	auto const shape = [&](glm::dvec2 const &Point) -> double {
		auto const u{glm::dot(Point - centre, across)}; // across the furrows and the ridges
		auto const v{glm::dot(Point - centre, along)};
		switch (waves.kind)
		{
		case terrain_waves::hills:
			return wave_fractal(Point.x / length, Point.y / length, variation > 0.0 ? 3 : 1) * 2.0 - 1.0;
		case terrain_waves::furrows:
		{
			// the furrows wander a little sideways and change their depth along the field
			auto const wander{(wave_noise(v / (length * 12.0), u / (length * 40.0)) - 0.5) * length * variation};
			auto const depth{1.0 - variation * 0.5 * wave_noise(v / (length * 8.0) + 3.1, u / (length * 8.0))};
			return std::sin(glm::two_pi<double>() * (u + wander) / length) * depth;
		}
		case terrain_waves::bumps:
		{
			auto const n{wave_fractal(Point.x / length, Point.y / length, 2)};
			auto const threshold{0.55 - 0.15 * variation};
			auto const t{std::clamp((n - threshold) / (1.0 - threshold), 0.0, 1.0)};
			return t * t * 2.0; // 0..2 of the half height: the ground between the bumps stays as it is
		}
		case terrain_waves::waves:
		{
			auto const shift{(wave_noise(v / (length * 3.0), 0.37) - 0.5) * length * variation};
			auto const size{1.0 - variation * 0.6 * wave_noise(u / (length * 2.0), v / (length * 2.0) + 5.7)};
			return std::sin(glm::two_pi<double>() * (u + shift) / length) * size;
		}
		case terrain_waves::grid:
		{
			auto const shift{(wave_noise(u / (length * 4.0), v / (length * 4.0) + 1.3) - 0.5) * length * variation};
			auto const a{std::sin(glm::two_pi<double>() * (u + shift) / length)};
			auto const b{std::sin(glm::two_pi<double>() * (v - shift) / length)};
			return a * b;
		}
		case terrain_waves::strips:
		{
			// strips of about the length across, each of its own height, the banks between them as steep as the sharpness
			auto const width{length * (1.0 + variation * 0.6 * (wave_noise(v / (length * 10.0), 2.9) - 0.5))};
			auto const place{u / width};
			auto const strip{std::floor(place)};
			auto const in{place - strip};
			auto const level = [&](double const S) { return wave_noise(S * 0.731 + 0.17, 4.4) * 2.0 - 1.0; };
			auto const bank{0.5 * (0.35 - 0.3 * static_cast<double>(waves.sharpness))}; // share of the strip each side of the edge
			auto const here{level(strip)};
			if (in < bank)
			{
				auto t{(in + bank) / (2.0 * bank)};
				t = t * t * (3.0 - 2.0 * t);
				return level(strip - 1.0) + (here - level(strip - 1.0)) * t;
			}
			if (in > 1.0 - bank)
			{
				auto t{(in - (1.0 - bank)) / (2.0 * bank)};
				t = t * t * (3.0 - 2.0 * t);
				return here + (level(strip + 1.0) - here) * t;
			}
			return here;
		}
		default:
			return 0.0;
		}
	};
	// the cross-section of a wave: the value -1..1 of the shape bent to the profile chosen
	auto const sharp{std::clamp(static_cast<double>(waves.sharpness), 0.0, 1.0)};
	auto const steps{static_cast<double>(std::clamp(waves.steps, 2, 12))};
	auto const profile = [&](double S) -> double {
		S = std::clamp(S, -1.0, 1.0);
		switch (waves.shape)
		{
		case terrain_waves::ridges:
		case terrain_waves::valleys:
		{
			// the sine straightened into a triangle (straight sides, corners at the top and the bottom) as far as it's sharp,
			// then the valleys widened under pointed ridges, or the other way round
			auto const triangle{std::asin(S) * 2.0 / glm::pi<double>()};
			auto const t{(S + (triangle - S) * sharp) * 0.5 + 0.5};
			auto const power{1.0 + 2.0 * sharp};
			auto const bent{waves.shape == terrain_waves::ridges ? std::pow(t, power) : 1.0 - std::pow(1.0 - t, power)};
			return bent * 2.0 - 1.0;
		}
		case terrain_waves::terraces:
		{
			auto const q{(S * 0.5 + 0.5) * steps};
			auto const step{std::floor(q)};
			auto const in{q - step};
			// the ramp between the flat steps narrows with the sharpness
			auto const ramp{std::max(0.04, 1.0 - sharp * 0.92)};
			auto t{std::clamp((in - (1.0 - ramp)) / ramp, 0.0, 1.0)};
			t = t * t * (3.0 - 2.0 * t);
			return std::clamp((step + t) / steps, 0.0, 1.0) * 2.0 - 1.0;
		}
		case terrain_waves::plateaus:
		{
			// cut flat above and below: the sides steeper the sharper
			auto const gain{1.0 + sharp * 6.0};
			return std::tanh(S * gain) / std::tanh(gain);
		}
		default:
			return S;
		}
	};
	auto const rough{std::clamp(static_cast<double>(waves.roughness), 0.0, 1.0)};
	auto const shaped = [&](glm::dvec2 const &Point) -> double {
		double value;
		if (waves.kind == terrain_waves::bumps)
			value = profile(shape(Point) - 1.0) + 1.0; // the bumps stand on untouched ground: shaped from -1 up
		else if (waves.kind == terrain_waves::strips)
			value = shape(Point); // the strips have their own banks
		else
			value = profile(shape(Point));
		return value;
	};
	auto const half{static_cast<double>(waves.height) * 0.5};
	// a grid point every 5 m can't carry waves of 3 m or clods: the chunks under the surface made as fine as they have to be
	// (the shortest wave three points, a clod of 3 m every metre), never coarser than they are
	{
		auto finest{5.0};
		if (waves.kind != terrain_waves::hollow && std::abs(waves.height) >= 0.001f)
			finest = std::min(finest, length / 3.0);
		if (rough > 0.0 || (waves.kind == terrain_waves::field && waves.tramlines >= 1.0f && waves.ruts >= 0.001f))
			finest = std::min(finest, 1.0); // a clod of 3 m, a tramline: a point a metre
		float target{heightmap::spacings.front()};
		for (auto const spacing : heightmap::spacings)
			if (spacing <= finest + 1e-6)
				target = spacing;
		auto const first{heightmap::chunk_at(low.x, low.y)}, last{heightmap::chunk_at(high.x, high.y)};
		bool finer{false};
		for (int cx = std::min(first.first, last.first); cx <= std::max(first.first, last.first); ++cx)
			for (int cz = std::min(first.second, last.second); cz <= std::max(first.second, last.second); ++cz)
			{
				auto const centre{heightmap::chunk_corner({cx, cz}) + glm::dvec2{heightmap::chunk_size * 0.5}};
				auto const *chunk{m_streamer.terrain_at(centre.x, centre.y)};
				if (chunk != nullptr && chunk->data().spacing() > target + 1e-3 && m_streamer.spacing(cx, cz, target))
					finer = true;
			}
		if (finer)
		{
			// as after Point spacing: the tiles of the ground made from the old grid
			forget_ground_in(heightmap::chunk_corner(first), heightmap::chunk_corner(last) + glm::dvec2{heightmap::chunk_size});
		}
	}
	// the cultivated field: the rows across the body, round the headland along the edge, the lands, the ruts of the tramlines
	field_layout const layout{Outline, static_cast<double>(waves.angle), static_cast<double>(waves.drift), static_cast<double>(waves.headland),
	                          static_cast<double>(waves.tramlines)};
	auto const lands{static_cast<double>(waves.lands)};
	auto const field_relief = [&](glm::dvec2 const &Point, double const Edge) -> double {
		auto const inner{layout.body(Point, Edge)};
		auto const u{layout.across_rows(Point)};
		auto const row = [&](double const Across) {
			auto const depth{1.0 - variation * 0.5 * wave_noise(Point.x / (length * 30.0) + 3.1, Point.y / (length * 30.0))};
			return profile(std::sin(glm::two_pi<double>() * Across / length) * depth);
		};
		auto value{(row(u) * inner + row(Edge) * (1.0 - inner)) * half};
		if (lands >= 1.0)
		{
			// a crown in the middle of every land, the dead furrows between them, a little uneven along
			auto const crown{0.5 + 0.5 * std::cos(glm::two_pi<double>() * u / lands)};
			auto const uneven{1.0 + variation * 0.4 * (wave_noise(Point.x / 40.0, Point.y / 40.0 + 2.2) * 2.0 - 1.0)};
			value += (crown - 0.5) * static_cast<double>(waves.land_height) * uneven * inner;
		}
		value -= static_cast<double>(waves.ruts) * layout.rut(Point, Edge, 0.45);
		return value;
	};
	return m_streamer.reshape(low, high, [&](double const X, double const Z, float &Height) {
		glm::dvec2 const point{X, Z};
		double distance;
		if (false == inside_by(point, distance))
			return false;
		auto fade{edge > 0.0 ? std::clamp(distance / edge, 0.0, 1.0) : 1.0};
		fade = fade * fade * (3.0 - 2.0 * fade);
		double offset;
		if (waves.kind == terrain_waves::hollow)
		{
			// lowered most in the middle, a smooth bowl to the edge, a little uneven
			auto t{std::clamp(distance / std::max(deepest, 0.5), 0.0, 1.0)};
			t = t * t * (3.0 - 2.0 * t);
			auto const uneven{1.0 + variation * 0.3 * (wave_noise(X / 10.0, Z / 10.0) * 2.0 - 1.0)};
			offset = -static_cast<double>(waves.height) * t * uneven;
		}
		else if (waves.kind == terrain_waves::field)
			offset = field_relief(point, distance) * fade;
		else
			offset = shaped(point) * half * fade;
		// clods and hummocks of about 3 m, up to 20 cm, whatever the waves (the grid made a point a metre for them)
		if (rough > 0.0)
			offset += (wave_fractal(X / 3.0 + 17.0, Z / 3.0 - 5.0, 3) * 2.0 - 1.0) * 0.2 * rough * fade;
		if (std::abs(offset) < 1e-4)
			return false;
		Height += static_cast<float>(offset);
		return true;
	});
}


float editor_mode::longest_side(std::vector<glm::dvec2> const &Outline)
{
	double longest{0.0};
	double angle{0.0};
	for (std::size_t i = 0, j = Outline.size() - 1; i < Outline.size(); j = i++)
	{
		auto const run{Outline[i] - Outline[j]};
		if (auto const length{glm::length(run)}; length > longest)
		{
			longest = length;
			angle = glm::degrees(std::atan2(run.y, run.x));
		}
	}
	// the rows run either way along it: kept within -90..90
	return static_cast<float>(std::remainder(angle, 180.0));
}

void editor_mode::choose_crop(int const Crop)
{
	m_crop = Crop;
	if (Crop <= 0 || Crop > static_cast<int>(crop_presets().size()))
		return;
	auto const &preset{crop_presets()[Crop - 1]};
	auto &waves{m_waves};
	waves.on = true;
	waves.kind = preset.field ? terrain_waves::field : terrain_waves::hills;
	waves.height = preset.rows;
	waves.length = preset.row_length;
	waves.roughness = preset.roughness;
	waves.tramlines = preset.tramlines;
	waves.lands = preset.lands;
	waves.land_height = preset.lands > 0.f ? 0.12f : 0.f;
	waves.headland = preset.headland;
	waves.shape = terrain_waves::smooth;
	waves.variation = 0.35f;
	waves.edge = preset.field ? 2.0f : 6.0f;
	m_cut_feather = preset.field ? 1.0f : 3.0f;
	m_cut_noise = preset.field ? 0.4f : 1.5f;
}

void editor_mode::render_crop_options()
{
	auto const &presets{crop_presets()};
	auto const name = [&](int const Crop) { return Crop <= 0 ? STR_C("none: the material of the palette") : STR_C(presets[Crop - 1].name); };
	ImGui::SeparatorText(STR_C("Crop##fields"));
	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo(STR_C("Sown with##fields"), name(m_crop)))
	{
		for (int crop = 0; crop <= static_cast<int>(presets.size()); ++crop)
		{
			if (ImGui::Selectable(name(crop), crop == m_crop))
				choose_crop(crop);
			if (crop > 0 && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", STR_C(presets[crop - 1].about));
		}
		ImGui::EndCombo();
	}
	item_tooltip("The field gets the texture of the crop instead of the material of the palette, its 3D grass (added to the grass palette the first time) "
	             "and the shape of the ground below; the texture lies along the rows. Each of them can be changed below before cutting out");
	if (m_crop > 0)
		ImGui::TextColored(ImVec4(0.96f, 0.79f, 0.24f, 1.0f), STR_C("Texture: %s"), presets[m_crop - 1].material);
}

void editor_mode::render_field_layout()
{
	auto &waves{m_waves};
	ImGui::Checkbox(STR_C("Rows along the longest side##waves"), &waves.auto_direction);
	item_tooltip("The direction of the rows taken from the outline when it's cut out, as a farmer drives along the longest side; off: the direction below");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Rows wander (m)##waves"), &waves.drift, 0.05f, 0.0f, 5.0f, "%.2f");
	item_tooltip("How far the rows bend sideways over a hundred metres or so: nothing in a field is drawn with a ruler. 0.5-1 m looks driven by hand");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Lands (m)##waves"), &waves.lands, 0.5f, 0.0f, 100.0f, waves.lands > 0.0f ? "%.1f" : STR_C("none"));
	item_tooltip("The width of a land: ploughed towards its middle for years it has a crown, the dead furrows lie between them. 20-40 m; 0: none");
	if (waves.lands > 0.0f)
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Crown of a land (m)##waves"), &waves.land_height, 0.01f, 0.0f, 1.0f, "%.2f");
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Headland (m)##waves"), &waves.headland, 0.5f, 0.0f, 50.0f, waves.headland > 0.0f ? "%.1f" : STR_C("none"));
	item_tooltip("The strip along the edge where the tractor turns: its rows go round the field, its inner side ragged where the rows end. 12-24 m; 0: none");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Tramlines every (m)##waves"), &waves.tramlines, 0.5f, 0.0f, 48.0f, waves.tramlines > 0.0f ? "%.1f" : STR_C("none"));
	item_tooltip("The two ruts of the sprayer, 1.8 m apart, across the field every 18, 24 or 36 m and round it in the middle of the headland: "
	             "the soil painted in them, the grass taken away. 0: none");
	if (waves.tramlines > 0.0f)
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Ruts deep (m)##waves"), &waves.ruts, 0.005f, 0.0f, 0.3f, "%.3f");
		item_tooltip("How deep the ruts go into the ground; the terrain gets a point every metre under the field for them, so they come out as shallow wide "
		             "dips; the paint does the rest. 0: painted only");
	}
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
	auto const painted{m_streamer.fill(Patch.outline, Patch.feather, layer, 1.0, Patch.noise, Patch.noise_scale)};
	// the 3D grass: the texture alone wouldn't do, the blades would go on growing over the gravel
	if (Patch.grass >= -1)
	{
		m_streamer.grass(Patch.outline, 0, true);
		if (Patch.grass >= 0 && Patch.grass < static_cast<int>(m_streamer.grass_kinds().size()))
			m_streamer.grass(Patch.outline, static_cast<std::uint16_t>(Patch.grass), false);
	}
	if (Patch.paint && false == Patch.field.empty())
		paint_tramlines(Patch);
	return painted;
}

void editor_mode::paint_tramlines(heightmap::surface_patch const &Patch)
{
	auto const spec{read_field(Patch.field)};
	if (spec.tramlines < 1.0)
		return;
	field_layout const layout{Patch.outline, spec.angle, spec.drift, spec.headland, spec.tramlines};
	glm::dvec2 low, high;
	Patch.bounds(low, high);
	// the soil in the ruts, as wide as the paint samples let it be, half of the way
	auto const soil{m_streamer.layer(spec.soil, 0.f, Patch.rotation)};
	auto const in_rut = [&](double const Width, double const Strength) {
		return [&layout, &Patch, Width, Strength](double const X, double const Z) {
			double edge;
			if (false == within_outline(Patch.outline, {X, Z}, edge) || edge < 1.0)
				return 0.0;
			return layout.rut({X, Z}, edge, Width) * Strength;
		};
	};
	m_streamer.paint_where(low, high, soil, in_rut(0.75, 0.5));
	// thinned, not bare: from afar a darker stripe through the crop, not holes in it
	m_streamer.grass_where(low, high, 0, true, in_rut(0.5, 0.65));
}

bool editor_mode::grass_choice(char const *Label, int &Choice)
{
	auto const &kinds{m_streamer.grass_kinds()};
	auto const name = [&](int const Value) -> std::string {
		if (Value == -2)
			return STR("left as it grows");
		if (Value == -1)
			return STR("taken away");
		return Value < static_cast<int>(kinds.size()) ? format(STR_C("grown: %s"), kinds[Value].name.c_str()) : std::string{"?"};
	};
	bool changed{false};
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::BeginCombo(Label, name(Choice).c_str()))
	{
		for (int value = -2; value < static_cast<int>(kinds.size()); ++value)
			if (ImGui::Selectable(name(value).c_str(), value == Choice))
			{
				Choice = value;
				changed = true;
			}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("The 3D grass under the surface: left as it grows, taken away (gravel, a yard), or another kind of the grass palette grown there instead"));
	return changed;
}

void editor_mode::fields_on_new_chunks()
{
	auto const added{m_streamer.take_added()};
	if (added.empty())
		return;
	auto const &all{m_streamer.patches()};
	std::vector<std::size_t> repaint;
	for (std::size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].outline.size() < 3)
			continue;
		glm::dvec2 low, high;
		reach_of(all[i], low, high);
		for (auto const &key : added)
		{
			auto const corner{heightmap::chunk_corner(key)};
			if (high.x < corner.x || low.x > corner.x + heightmap::chunk_size || high.y < corner.y || low.y > corner.y + heightmap::chunk_size)
				continue;
			// a surface of its own lies over a hole of the ground, a painted one is in the paint of the chunks
			if (all[i].paint)
			{
				repaint.push_back(i);
				break;
			}
			m_streamer.mark_field_hole(key, all[i].outline);
		}
	}
	if (false == repaint.empty())
		repaint_patches(all, repaint);
	WriteLog(format("Editor: %zu chunks added, the surfaces cut out over them put onto them (%zu painted again)", added.size(), repaint.size()));
}

void editor_mode::repaint_patches(std::vector<heightmap::surface_patch> All, std::vector<std::size_t> const &Changed, std::vector<heightmap::surface_patch> const &Cleared)
{
	// the ones changed, and the painted ones over them they touch: the transitions between them made again
	std::vector<char> paint(All.size(), 0);
	double constexpr cell{100.0};
	auto const key = [](double const X, double const Z) {
		return (static_cast<std::int64_t>(std::floor(X / cell)) << 32) ^ (static_cast<std::int64_t>(std::floor(Z / cell)) & 0xffffffff);
	};
	auto const cell_of = [](double const Value) { return static_cast<std::int64_t>(std::floor(Value / cell)); };
	std::unordered_map<std::int64_t, std::vector<std::size_t>> grid;
	if (false == Changed.empty() || false == Cleared.empty())
		for (std::size_t i = 0; i < All.size(); ++i)
		{
			if (false == All[i].paint)
				continue;
			glm::dvec2 low, high;
			reach_of(All[i], low, high);
			for (auto z{cell_of(low.y)}; z <= cell_of(high.y); ++z)
				for (auto x{cell_of(low.x)}; x <= cell_of(high.x); ++x)
					grid[key(static_cast<double>(x) * cell, static_cast<double>(z) * cell)].push_back(i);
		}
	auto const touching = [&](glm::dvec2 const &Low, glm::dvec2 const &High, auto const &Take) {
		for (auto z{cell_of(Low.y)}; z <= cell_of(High.y); ++z)
			for (auto x{cell_of(Low.x)}; x <= cell_of(High.x); ++x)
			{
				auto const lookup{grid.find(key(static_cast<double>(x) * cell, static_cast<double>(z) * cell))};
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
		// the grass back as the terrain grows it most: the first kind of the palette, where it was taken away
		base.grass = gone.grass == -2 ? -2 : (m_streamer.grass_kinds().empty() ? -1 : 0);
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
		if (false == editor_busy_progress(0.5 * static_cast<double>(i) / static_cast<double>(patches.size())))
			break;
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
		m_streamer.cut_field(patch.outline, false);
		patch.paint = true;
		patch.follow = true;
		patch.heights.clear();
		patch.columns = 0;
		patch.rows = 0;
		auto const priority{paint_priority(patch)};
		patch.feather = priority == 1 ? settings.bed_feather : priority == 0 ? settings.slope_feather : m_cut_feather;
		patch.grass = priority == 1 ? settings.bed_grass : priority == 0 ? settings.slope_grass : m_cut_grass;
		patch.noise = m_cut_noise;
		patch.noise_scale = m_cut_noise_scale;
		converted.push_back(i);
	}
	if (converted.empty())
	{
		m_terrain_status = STR_C("No surface with a hole of its own there");
		return;
	}
	// the holes of the separate ones left over them, closed with theirs, cut again
	for (auto const i : converted)
		m_streamer.recut_fields_over(patches, patches[i]);
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
		auto const pressed{main_button(STR_C("Go to the place found"), ImVec2(-FLT_MIN, tall_button_height())) || entered};
		if (auto const chosen{static_cast<std::size_t>(std::max(0, m_goto_found))}; pressed && located && m_goto_found >= 0 && chosen < std::size(places))
		{
			auto const &place{places[chosen]};
			go_to_plan(to_plan({static_cast<double>(place.east), static_cast<double>(place.north)}));
		}
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

void editor_mode::sync_terrain_origin()
{
	if (false == m_streamer.active())
	{
		m_terrain_origin_synced = false;
		return;
	}
	if (m_terrain_origin_synced)
		return;
	m_terrain_origin_synced = true;
	load_orthophoto_settings();
	read_georeference();
	auto const &terrain{m_streamer.manifest()};
	auto config{m_orthophoto.settings()};
	if (terrain.georeferenced)
	{
		if (config.north == terrain.origin_north && config.east == terrain.origin_east)
			return;
		auto const before{config.north != 0.0 || config.east != 0.0};
		config.north = terrain.origin_north;
		config.east = terrain.origin_east;
		m_orthophoto.settings(config);
		m_orthophoto_origin_edit = {config.north, config.east};
		save_orthophoto_settings();
		nmt_locate();
		auto const text{format(before ? STR_C("The origin of the scenery taken from its terrain again: X %.0f, Y %.0f in PUWG 1992 (it was moved elsewhere)")
		                              : STR_C("The origin of the scenery taken from its terrain: X %.0f, Y %.0f in PUWG 1992"),
		                       config.north, config.east)};
		ui()->set_status(text, false);
		WriteLog("Editor: " + text, logtype::generic);
		return;
	}
	// a terrain which doesn't know where it lies, in a scenery which does (its //$g line): the terrain keeps it from now on
	if (m_georeference)
	{
		m_streamer.georeference(m_georeference_origin.x, m_georeference_origin.y);
		WriteLog(format("Editor: the terrain keeps the origin of the scenery (%s)", m_georeference_line.c_str()), logtype::generic);
		if (config.north != m_georeference_origin.x || config.east != m_georeference_origin.y)
		{
			config.north = m_georeference_origin.x;
			config.east = m_georeference_origin.y;
			m_orthophoto.settings(config);
			m_orthophoto_origin_edit = {config.north, config.east};
			save_orthophoto_settings();
			nmt_locate();
		}
	}
}

void editor_mode::naturalise_slopes(bool const Natural)
{
	auto const &settings{m_track_bed};
	glm::dvec2 const camera{Camera.Pos.x, Camera.Pos.z};
	std::vector<heightmap::modifier> changed;
	for (auto const &modifier : m_streamer.modifiers())
	{
		if (modifier.points.size() < 2)
			continue;
		if (false == settings.everywhere)
		{
			auto const [low, high]{modifier.bounds()};
			if (glm::distance(glm::clamp(camera, low, high), camera) > settings.radius)
				continue;
		}
		auto copy{modifier};
		copy.natural = Natural ? m_natural : heightmap::modifier::natural_shape{};
		changed.push_back(std::move(copy));
	}
	if (changed.empty())
	{
		m_terrain_status = STR_C("No formation modifier there");
		return;
	}
	auto const count{changed.size()};
	m_streamer.modifiers(std::move(changed));
	forget_ground();
	m_terrain_status = format(Natural ? STR_C("The slopes of %zu modifiers made natural, the terrain shaped anew. Ctrl+Z takes it back")
	                                  : STR_C("The slopes of %zu modifiers as designed again. Ctrl+Z takes it back"),
	                          count);
	WriteLog("Editor: " + m_terrain_status);
}

// where an action over an area works: around the camera as far as set, or the whole scenery; shown beside every button which
// works so, all of them sharing it, so none depends on a tick hidden in another panel
void editor_mode::render_scope(char const *Id)
{
	auto &settings{m_track_bed};
	ImGui::PushID(Id);
	int scope{settings.everywhere ? 1 : 0};
	ImGui::RadioButton(STR_C("Around the camera##scope"), &scope, 0);
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("The whole scenery##scope"), &scope, 1);
	settings.everywhere = scope == 1;
	if (false == settings.everywhere)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragFloat(STR_C("m##scope"), &settings.radius, 10.0f, 100.0f, 20000.0f, "%.0f");
	}
	ImGui::PopID();
}
