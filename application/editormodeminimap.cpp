/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the minimap: the tracks of the scenery seen from above, north up, with the camera on them, the towns and villages of the
// area laid out by the origin of the scenery in PUWG 1992, and a search of them; a double click takes the camera there

#include "stdafx.h"
#include "application/editormode.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"
#include "simulation/simulation.h"
#include "world/Track.h"
#include "utilities/Globals.h"
#include "utilities/translation.h"

#include "imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

namespace
{

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

// the origin of the network of the IRT converter, used for its sceneries when the origin isn't set otherwise
double constexpr irt_east{305000.0};
double constexpr irt_north{353000.0};

// how often the tracks of the map are read again, when their count stays
double constexpr lines_refresh{3.0};

// a name for the search: lower case, the Polish letters without their marks, so "zabkowice" finds "Ząbkowice"
std::string folded(char const *Text)
{
	static char const *const pairs[][2] = {{"ą", "a"}, {"ć", "c"}, {"ę", "e"}, {"ł", "l"}, {"ń", "n"}, {"ó", "o"}, {"ś", "s"}, {"ź", "z"}, {"ż", "z"},
	                                       {"Ą", "a"}, {"Ć", "c"}, {"Ę", "e"}, {"Ł", "l"}, {"Ń", "n"}, {"Ó", "o"}, {"Ś", "s"}, {"Ź", "z"}, {"Ż", "z"},
	                                       {"ä", "a"}, {"ö", "o"}, {"ü", "u"}, {"é", "e"}, {"č", "c"}, {"ř", "r"}, {"š", "s"}, {"ž", "z"}};
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

std::vector<std::string> const &folded_names()
{
	static std::vector<std::string> names = [] {
		std::vector<std::string> result;
		result.reserve(std::size(places));
		for (auto const &entry : places)
			result.push_back(folded(entry.name));
		return result;
	}();
	return names;
}

} // namespace

bool editor_mode::minimap_origin(glm::dvec2 &Origin)
{
	load_orthophoto_settings();
	auto const &config{m_orthophoto.settings()};
	if (config.north != 0.0 || config.east != 0.0)
	{
		Origin = {config.east, config.north};
		return true;
	}
	if (m_georeference)
	{
		Origin = {m_georeference_origin.y, m_georeference_origin.x};
		return true;
	}
	return false;
}

void editor_mode::minimap_go_to(glm::dvec2 const &Plan)
{
	glm::dvec3 target{Plan.x, Camera.Pos.y, Plan.y};
	double ground{0.0};
	auto const known{m_streamer.height_at(Plan.x, Plan.y, ground)};
	if (known)
		target.y = ground;
	if (Global.EditorOrtho)
	{
		// the top view only moves over it
		m_focus_active = false;
		Camera.Pos.x = target.x;
		Camera.Pos.z = target.z;
		Camera.Velocity = glm::dvec3{0.0};
		Global.pCamera = Camera;
		return;
	}
	if (false == known)
	{
		// no ground there to look down at: the camera only moved over
		m_focus_active = false;
		Camera.Pos.x = target.x;
		Camera.Pos.z = target.z;
		Camera.Velocity = glm::dvec3{0.0};
		Global.pCamera = Camera;
		return;
	}
	// from the side the camera looks from, down at the point, from a few hundred metres
	auto const yaw{static_cast<double>(Camera.Angle.y)};
	auto const pitch{glm::radians(35.0)};
	glm::dvec3 const look{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
	fly_to(target, look, 300.0);
}

void editor_mode::render_minimap_window()
{
	auto &map{m_minimap};
	if (false == map.open)
		return;
	ImGui::SetNextWindowSize(ImVec2(380.0f * Global.ui_scale, 460.0f * Global.ui_scale), ImGuiCond_FirstUseEver);
	if (false == ImGui::Begin((std::string(STR_C("Minimap")) + "###minimap").c_str(), &map.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	glm::dvec2 origin{0.0};
	auto const located{minimap_origin(origin)};
	// the scenery's x, z of a point in EPSG:2180, and back: X = -(E - E0), Z = N - N0
	auto const to_plan = [&](double const East, double const North) { return glm::dvec2{origin.x - East, North - origin.y}; };

	// --- the search ---
	ImGui::SetNextItemWidth(-1.0f);
	auto const entered{ImGui::InputTextWithHint("##minimapsearch", STR_C("Find a town or village..."), map.query, sizeof(map.query), ImGuiInputTextFlags_EnterReturnsTrue)};
	auto const query{folded(map.query)};
	if (query != map.searched)
	{
		map.searched = query;
		map.found.clear();
		if (query.size() >= 2)
		{
			auto const &names{folded_names()};
			// the names starting with the text first, towns before villages, then the ones having it inside
			for (int pass = 0; pass < 2; ++pass)
				for (int i = 0; i < static_cast<int>(names.size()); ++i)
				{
					auto const at{names[i].find(query)};
					if (at == std::string::npos || (pass == 0) != (at == 0))
						continue;
					map.found.push_back(i);
				}
		}
	}
	auto const choose = [&](int const Index, bool const Go) {
		map.marked = Index;
		auto const plan{to_plan(places[Index].east, places[Index].north)};
		map.follow = false;
		map.centre = plan;
		map.scale = std::min(map.scale, 10.0);
		if (Go)
			minimap_go_to(plan);
	};
	if (entered && false == map.found.empty() && located)
		choose(map.found.front(), true);
	if (false == map.found.empty())
	{
		auto const shown{std::min<std::size_t>(map.found.size(), 200)};
		if (ImGui::BeginListBox("##minimapfound", ImVec2(-FLT_MIN, std::min(6.0f, static_cast<float>(shown) + 0.25f) * ImGui::GetTextLineHeightWithSpacing())))
		{
			for (std::size_t i = 0; i < shown; ++i)
			{
				auto const index{map.found[i]};
				auto const &entry{places[index]};
				ImGui::PushID(index);
				auto const label{format("%s%s  (%s)", entry.name, entry.city ? STR_C(" - town") : "", entry.where)};
				if (ImGui::Selectable(label.c_str(), index == map.marked, ImGuiSelectableFlags_AllowDoubleClick) && located)
					choose(index, ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}
		ImGui::TextDisabled(STR_C("%zu found; a click shows it, a double click or Enter takes the camera there"), map.found.size());
	}
	else if (query.size() >= 2)
		ImGui::TextDisabled("%s", STR_C("Nothing found in the area of the network"));

	if (false == located)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", STR_C("The origin of the scenery in PUWG 1992 isn't known, the towns can't be laid out. It's set with the orthophoto."));
		ImGui::PopTextWrapPos();
		if (ImGui::Button(STR_C("Origin of the IRT network (E 305000, N 353000)")))
		{
			auto config{m_orthophoto.settings()};
			config.east = irt_east;
			config.north = irt_north;
			m_orthophoto_origin_edit = {irt_north, irt_east};
			m_orthophoto.settings(config);
			save_orthophoto_settings();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("The origin the IRT converter lays the tracks of Lower Silesia out from"));
	}

	// --- the tracks in the plan, read again when their count changes or after a while ---
	auto const &tracks{simulation::Paths.sequence()};
	auto const now{ImGui::GetTime()};
	if (tracks.size() != map.tracks || map.built < 0.0 || now - map.built > lines_refresh)
	{
		map.tracks = tracks.size();
		map.built = now;
		map.lines.clear();
		for (auto const *track : tracks)
		{
			if (track == nullptr || track->m_editorremoved)
				continue;
			for (auto const &path : track->m_paths)
			{
				geometry::bezier const curve{path};
				auto const straight{path.points[segment_data::point::control1] == glm::dvec3{} && path.points[segment_data::point::control2] == glm::dvec3{}};
				auto const steps{straight ? 1 : 6};
				auto from{curve.point(0.0)};
				for (int k = 1; k <= steps; ++k)
				{
					auto const to{curve.point(static_cast<double>(k) / steps)};
					map.lines.emplace_back(static_cast<float>(from.x), static_cast<float>(from.z), static_cast<float>(to.x), static_cast<float>(to.z));
					from = to;
				}
			}
		}
	}

	// --- the map ---
	ImGui::Checkbox(STR_C("Follow the camera"), &map.follow);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("On the map: wheel zooms, dragging moves it, a double click takes the camera there"));
	ImGui::SameLine();
	ImGui::TextDisabled(map.scale >= 1000.0 ? STR_C("%.1f km per pixel") : STR_C("%.0f m per pixel"), map.scale >= 1000.0 ? map.scale / 1000.0 : map.scale);
	auto const size{ImGui::GetContentRegionAvail()};
	if (size.x < 32.0f || size.y < 32.0f)
	{
		ImGui::End();
		return;
	}
	auto const corner{ImGui::GetCursorScreenPos()};
	ImGui::InvisibleButton("##minimapcanvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	auto const hovered{ImGui::IsItemHovered()};
	auto const active{ImGui::IsItemActive()};
	auto &io{ImGui::GetIO()};
	if (map.follow)
		map.centre = {Camera.Pos.x, Camera.Pos.z};
	glm::dvec2 const middle{corner.x + size.x * 0.5, corner.y + size.y * 0.5};
	// north up, east to the right: x of the scenery grows to the west
	auto const to_screen = [&](double const X, double const Z) {
		return ImVec2(static_cast<float>(middle.x - (X - map.centre.x) / map.scale), static_cast<float>(middle.y - (Z - map.centre.y) / map.scale));
	};
	auto const to_world = [&](ImVec2 const &Point) {
		return glm::dvec2{map.centre.x - (Point.x - middle.x) * map.scale, map.centre.y - (Point.y - middle.y) * map.scale};
	};
	if (hovered && io.MouseWheel != 0.0f)
	{
		// the point under the cursor stays under it
		auto const under{to_world(io.MousePos)};
		map.scale = std::clamp(map.scale * std::pow(0.8, static_cast<double>(io.MouseWheel)), 0.25, 2000.0);
		if (false == map.follow)
			map.centre += under - to_world(io.MousePos);
	}
	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
	{
		map.follow = false;
		map.centre += glm::dvec2{io.MouseDelta.x * map.scale, io.MouseDelta.y * map.scale};
	}
	if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		minimap_go_to(to_world(io.MousePos));

	auto *draw{ImGui::GetWindowDrawList()};
	ImVec2 const far_corner{corner.x + size.x, corner.y + size.y};
	draw->PushClipRect(corner, far_corner, true);
	draw->AddRectFilled(corner, far_corner, IM_COL32(24, 26, 24, 255));
	// a grid every round number of kilometres, about every 80 pixels
	{
		double spacing{100000.0};
		for (auto const step : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0, 50000.0})
			if (step / map.scale >= 60.0)
			{
				spacing = step;
				break;
			}
		auto const low{to_world(far_corner)};
		auto const high{to_world(corner)};
		for (auto x = std::floor(low.x / spacing) * spacing; x <= high.x; x += spacing)
			draw->AddLine(to_screen(x, high.y), to_screen(x, low.y), IM_COL32(255, 255, 255, 18));
		for (auto z = std::floor(low.y / spacing) * spacing; z <= high.y; z += spacing)
			draw->AddLine(to_screen(low.x, z), to_screen(high.x, z), IM_COL32(255, 255, 255, 18));
	}
	// the tracks seen
	{
		auto const low{to_world(far_corner)};
		auto const high{to_world(corner)};
		auto const width{map.scale < 2.0 ? 2.0f : 1.5f};
		for (auto const &line : map.lines)
		{
			if (std::max(line.x, line.z) < low.x || std::min(line.x, line.z) > high.x || std::max(line.y, line.w) < low.y || std::min(line.y, line.w) > high.y)
				continue;
			draw->AddLine(to_screen(line.x, line.y), to_screen(line.z, line.w), IM_COL32(120, 170, 235, 255), width);
		}
	}
	// the towns always, the villages when the map is close enough for their names
	if (located)
	{
		auto const villages{map.scale <= 25.0};
		for (int i = 0; i < static_cast<int>(std::size(places)); ++i)
		{
			auto const &entry{places[i]};
			if (false == entry.city && false == villages && i != map.marked)
				continue;
			auto const plan{to_plan(entry.east, entry.north)};
			auto const point{to_screen(plan.x, plan.y)};
			if (point.x < corner.x - 100.0f || point.x > far_corner.x + 20.0f || point.y < corner.y - 20.0f || point.y > far_corner.y + 20.0f)
				continue;
			auto const colour{entry.city ? IM_COL32(245, 235, 200, 255) : IM_COL32(200, 200, 190, 220)};
			draw->AddCircleFilled(point, entry.city ? 3.5f : 2.5f, colour);
			draw->AddText(ImVec2(point.x + 5.0f, point.y - ImGui::GetTextLineHeight() * 0.5f), colour, entry.name);
		}
		if (map.marked >= 0)
		{
			auto const plan{to_plan(places[map.marked].east, places[map.marked].north)};
			draw->AddCircle(to_screen(plan.x, plan.y), 9.0f, IM_COL32(255, 120, 60, 255), 0, 2.0f);
		}
	}
	// the camera, pointing where it looks
	{
		auto const at{to_screen(Camera.Pos.x, Camera.Pos.z)};
		auto const yaw{static_cast<double>(Camera.Angle.y)};
		// the direction of the view in the plan, then on the map (x of the scenery to the left)
		glm::dvec2 const ahead{-std::sin(yaw), -std::cos(yaw)};
		ImVec2 const forward{static_cast<float>(-ahead.x), static_cast<float>(-ahead.y)};
		ImVec2 const side{-forward.y, forward.x};
		auto const tip{ImVec2(at.x + forward.x * 11.0f, at.y + forward.y * 11.0f)};
		auto const left{ImVec2(at.x - forward.x * 6.0f + side.x * 6.0f, at.y - forward.y * 6.0f + side.y * 6.0f)};
		auto const right{ImVec2(at.x - forward.x * 6.0f - side.x * 6.0f, at.y - forward.y * 6.0f - side.y * 6.0f)};
		draw->AddTriangleFilled(tip, left, right, IM_COL32(255, 90, 60, 255));
		draw->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 255));
	}
	// north and the point under the cursor
	draw->AddText(ImVec2(far_corner.x - 18.0f, corner.y + 4.0f), IM_COL32(255, 255, 255, 160), "N");
	if (hovered)
	{
		auto const under{to_world(io.MousePos)};
		auto text{format("X %.0f  Z %.0f", under.x, under.y)};
		if (located)
		{
			// the nearest place within a few kilometres, as a hint where this is
			int nearest{-1};
			double best{5000.0 * 5000.0};
			for (int i = 0; i < static_cast<int>(std::size(places)); ++i)
			{
				auto const plan{to_plan(places[i].east, places[i].north)};
				auto const distance{glm::dot(plan - under, plan - under)};
				if (distance < best)
					best = distance, nearest = i;
			}
			if (nearest >= 0)
				text += format(STR_C("  near %s (%.1f km)"), places[nearest].name, std::sqrt(best) / 1000.0);
		}
		draw->AddText(ImVec2(corner.x + 4.0f, far_corner.y - ImGui::GetTextLineHeight() - 4.0f), IM_COL32(255, 255, 255, 200), text.c_str());
	}
	draw->PopClipRect();
	ImGui::End();
}
