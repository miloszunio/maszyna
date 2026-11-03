/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// "Kapelusz" (the hat), in the field of work Features: a rectangle drawn in the plan over the tracks; the track under it is looked
// at, the smallest switch of the catalogue which fits within the rectangle on it is found and laid in, the track cut and joined to
// it. Where the rectangle leaves too little straight track (a curve, a transition curve, cant, another switch) it says how much
// more room it wants and why

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editormodeshared.h"
#include "application/editormodetrackshared.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"
#include "world/Track.h"
#include "simulation/simulation.h"
#include "scene/scene.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

double constexpr kHatSlack{0.05}; // the switch may stick out of the rectangle by this much

glm::dvec2 plan2(glm::dvec3 const &Point)
{
	return {Point.x, Point.z};
}

double cross2(glm::dvec2 const &A, glm::dvec2 const &B)
{
	return A.x * B.y - A.y * B.x;
}

} // namespace

editor_mode::hat_plan editor_mode::plan_hat() const
{
	hat_plan plan;
	if (m_paint_outline.size() < 2)
	{
		plan.report = STR("Click two corners of the rectangle across the track (more points: the rectangle around them)");
		return plan;
	}
	glm::dvec2 low{std::numeric_limits<double>::max()};
	glm::dvec2 high{-std::numeric_limits<double>::max()};
	for (auto const &point : m_paint_outline)
	{
		low = glm::min(low, plan2(point));
		high = glm::max(high, plan2(point));
	}
	auto const centre{(low + high) * 0.5};
	auto const radius{static_cast<float>(glm::length(high - low) * 0.5 + 10.0)};
	// the plain tracks under the rectangle (as drawn in the plan), the one with most of it inside first
	TTrack *main{nullptr};
	double longest{0.0};
	std::vector<TTrack *> under;
	for (auto *section : simulation::Region->sections(glm::dvec3{centre.x, m_paint_outline.front().y, centre.y}, radius))
		for (auto const &cell : section->m_cells)
			for (auto *track : cell.m_paths)
			{
				if (track == nullptr || track->m_editorremoved || track->m_paths.empty() || track->eType == tt_Cross)
					continue;
				geometry::bezier const curve{track->m_paths.front()};
				int inside{0};
				for (int i = 0; i <= 16; ++i)
				{
					auto const at{plan2(curve.point(i / 16.0))};
					inside += at.x >= low.x && at.x <= high.x && at.y >= low.y && at.y <= high.y ? 1 : 0;
				}
				if (inside == 0)
					continue;
				if (std::find(under.begin(), under.end(), track) == under.end())
					under.push_back(track);
				auto const length{geometry::chord_length(track->m_paths.front()) * inside / 17.0};
				if (track->eType == tt_Normal && length > longest)
				{
					longest = length;
					main = track;
				}
			}
	if (main == nullptr)
	{
		plan.report = STR("No plain track under the rectangle");
		return plan;
	}
	plan.main = main;
	auto line{editor_track::find_straight(*main, m_straights.tolerance)};
	if (line.tracks.empty())
	{
		// what the track is instead of a straight
		editor_track::curve curve;
		if (editor_track::find_curve(*main, m_straights.tolerance, m_route.design.norms.gauge, curve))
		{
			if (curve.transition_in > 0.0 || curve.transition_out > 0.0)
				plan.report = format(STR_C("The track under the rectangle is a curve of R %.0f m with transition curves of %.0f and %.0f m%s: a switch goes on a straight, "
				                         "move the rectangle onto the straight before or after it"),
				                     curve.radius, curve.transition_in, curve.transition_out, curve.cant > 0.0 ? format(STR_C(", cant %.0f mm"), curve.cant).c_str() : "");
			else
				plan.report = format(STR_C("The track under the rectangle is a curve of R %.0f m: a switch goes on a straight, move the rectangle onto the straight before or after it"),
				                     curve.radius);
		}
		else
			plan.report = STR("The track under the rectangle is neither a straight nor a curve recognized (a transition curve?): a switch goes on a straight");
		return plan;
	}
	// the rectangle along the track: the points drawn, seen along it and across it
	auto const direction{line.direction};
	glm::dvec2 const left{-direction.y, direction.x};
	auto const origin{plan2(line.start)};
	double u0{std::numeric_limits<double>::max()}, u1{-u0}, v0{u0}, v1{-u0};
	for (auto const &point : m_paint_outline)
	{
		auto const offset{plan2(point) - origin};
		u0 = std::min(u0, glm::dot(offset, direction));
		u1 = std::max(u1, glm::dot(offset, direction));
		v0 = std::min(v0, glm::dot(offset, left));
		v1 = std::max(v1, glm::dot(offset, left));
	}
	if (u1 - u0 < 1.0)
	{
		// two points across the track: the rectangle has no length yet
		plan.report = STR("The rectangle has no length along the track: click its corners diagonally");
		return plan;
	}
	auto const corner = [&](double const U, double const V) {
		auto const at{origin + direction * U + left * V};
		return glm::dvec3{at.x, line.start.y + line.grade * std::clamp(U, 0.0, line.length) + 0.3, at.y};
	};
	plan.box = {corner(u0, v0), corner(u1, v0), corner(u1, v1), corner(u0, v1)};
	// the straight within the rectangle
	auto const from{std::max(u0, 0.0)};
	auto const to{std::min(u1, line.length)};
	auto const room{std::max(0.0, to - from)};
	// the cant on the straight, where it would be taken out under the switch
	double cant{0.0};
	for (std::size_t i = 0; i < line.tracks.size(); ++i)
	{
		auto const &path{line.tracks[i]->m_paths[line.paths[i]]};
		auto const a{line.along(path.points[segment_data::point::start])};
		auto const b{line.along(path.points[segment_data::point::end])};
		if (std::max(a, b) < from || std::min(a, b) > to)
			continue;
		cant = std::max({cant, std::abs(static_cast<double>(path.rolls[0])), std::abs(static_cast<double>(path.rolls[1]))});
	}
	// the side the rectangle reaches further to
	auto const side{v1 >= -v0 ? 1 : -1};
	auto const across{side > 0 ? v1 : -v0};
	auto const rail{editor_track::rail_of(main)};
	auto templates{m_switch.collected ? m_switch.templates : editor_track::standard_switch_templates()};
	std::vector<editor_track::switch_template const *> candidates;
	for (auto const &shape : templates)
	{
		if (shape.double_slip || shape.twofold() || shape.length <= 0.0)
			continue;
		if (shape.source == nullptr && false == rail.empty() && shape.rail != rail && shape.rail != "-")
			continue;
		candidates.push_back(&shape);
	}
	std::sort(candidates.begin(), candidates.end(), [](auto const *A, auto const *B) { return A->length < B->length; });
	if (candidates.empty())
	{
		plan.report = format(STR_C("No switch in the catalogue for the rail %s"), rail.c_str());
		return plan;
	}
	double needed_along{std::numeric_limits<double>::max()};
	double needed_across{std::numeric_limits<double>::max()};
	editor_track::switch_template const *smallest{nullptr};
	for (auto const *shape : candidates)
	{
		auto const length{shape->length};
		auto const offset{std::abs(shape->local[1].points[segment_data::point::end].x)};
		if (length < needed_along)
		{
			needed_along = length;
			needed_across = offset;
			smallest = shape;
		}
		if (length > room + kHatSlack || offset > across + kHatSlack)
			continue;
		for (int const forward : {1, -1})
		{
			// in the middle of the straight within the rectangle
			auto const along{forward > 0 ? from + (room - length) * 0.5 : to - (room - length) * 0.5};
			auto const start{glm::dvec3{line.start.x + direction.x * along, line.start.y + line.grade * along, line.start.z + direction.y * along}};
			auto const paths{editor_track::place_switch(*shape, start, direction * static_cast<double>(forward), side, line.grade * forward)};
			if (paths.size() < 2)
				continue;
			auto const end{plan2(paths[1].points[segment_data::point::end]) - origin};
			auto const u{glm::dot(end, direction)};
			auto const v{glm::dot(end, left)};
			if (u < u0 - kHatSlack || u > u1 + kHatSlack || v < v0 - kHatSlack || v > v1 + kHatSlack)
				continue;
			plan.valid = true;
			plan.line = line;
			plan.shape = *shape;
			plan.along = along;
			plan.forward = forward;
			plan.side = side;
			plan.paths = paths;
			plan.report = format(STR_C("%s fits: %.1f m of the %.1f m of straight in the rectangle%s"), shape->title().c_str(), length, room,
			                     cant > 0.01 ? format(STR_C("; the cant of %.1f deg is taken out under it"), cant).c_str() : "");
			return plan;
		}
	}
	// too little room: how much more, and what stops the straight
	std::string why;
	if (to < u1 - 0.5 || from > u0 + 0.5)
	{
		auto const *beyond{to < u1 - 0.5 ? line.tracks.back() : line.tracks.front()};
		auto const point{to < u1 - 0.5 ? segment_data::point::end : segment_data::point::start};
		TTrack *next{nullptr};
		for (auto *track : under)
			if (track != beyond && false == track->m_paths.empty() &&
			    (glm::distance(track->m_paths.front().points[segment_data::point::start], beyond->m_paths[0].points[point]) < 0.05 ||
			     glm::distance(track->m_paths.front().points[segment_data::point::end], beyond->m_paths[0].points[point]) < 0.05))
				next = track;
		editor_track::curve curve;
		if (next != nullptr && next->eType == tt_Switch)
			why = STR("; the straight ends at a switch");
		else if (next != nullptr && editor_track::find_curve(*next, m_straights.tolerance, m_route.design.norms.gauge, curve))
			why = format(curve.transition_in > 0.0 || curve.transition_out > 0.0 ? STR_C("; the straight ends where a transition curve into R %.0f m begins") : STR_C("; the straight ends at a curve of R %.0f m"),
			             curve.radius);
		else
			why = STR("; the straight ends within the rectangle");
	}
	if (smallest != nullptr && needed_along > room + kHatSlack)
		plan.report = format(STR_C("Too little room: the smallest switch (%s) needs %.1f m of straight, the rectangle holds %.1f m: %.1f m more along the track%s"),
		                     smallest->title().c_str(), needed_along, room, needed_along - room, why.c_str());
	else if (smallest != nullptr)
		plan.report = format(STR_C("Too little room across: the smallest switch (%s) leaves the track by %.2f m at its end, the rectangle reaches %.2f m to that side: %.2f m more"),
		                     smallest->title().c_str(), needed_across, across, needed_across - across);
	return plan;
}

void editor_mode::lay_hat()
{
	auto const plan{plan_hat()};
	if (false == plan.valid)
	{
		ui()->set_status(plan.report, true);
		return;
	}
	std::vector<std::pair<TTrack *, editor_track::state>> states;
	std::vector<TTrack *> created;
	std::vector<TTrack *> removed;
	if (false == place_switch_on_straight(plan.line, plan.shape, nullptr, plan.along, plan.forward, plan.side, false, states, created, removed, switch_name_for_new()))
	{
		ui()->set_status(m_switch.status.empty() ? std::string{STR("The switch can't be laid there")} : m_switch.status, true);
		return;
	}
	auto const placed{created};
	push_track_snapshot(std::move(states), std::move(created), std::move(removed));
	straight_refresh();
	switch_placed(placed);
	m_paint_outline.clear();
	auto const text{format(STR_C("Kapelusz: %s laid in, the track cut and joined to it. Ctrl+Z takes it back"), plan.shape.title().c_str())};
	ui()->set_status(text, false);
	WriteLog("Editor: " + text, logtype::generic);
}

void editor_mode::render_hat_ui()
{
	if (ImGui::CollapsingHeader(STR_C("Kapelusz: a switch within a rectangle"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextWrapped("%s", STR_C("Click two corners of a rectangle over the track, diagonally (the top view helps). The rectangle is laid along the track; the smallest "
		                               "switch of the catalogue which fits within it is laid in, its diverging track to the side the rectangle reaches further to. "
		                               "Shift+LMB takes the last point back"));
		auto const plan{plan_hat()};
		m_hat_box = plan.box;
		if (plan.valid)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.9f, 0.55f, 1.0f));
		else
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.3f, 1.0f));
		ImGui::TextWrapped("%s", plan.report.c_str());
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::BeginDisabled(false == plan.valid);
		if (main_button(STR_C("Lay the switch in"), ImVec2(-FLT_MIN, tall_button_height())))
			lay_hat();
		ImGui::EndDisabled();
		if (ImGui::Button(STR_C("Clear the rectangle"), ImVec2(-FLT_MIN, 0.0f)))
		{
			m_paint_outline.clear();
			m_hat_box.clear();
		}
	}
}
