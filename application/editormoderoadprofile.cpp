/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the vertical profile of a road evened out as a designer would, and the terrain shaped to the roads

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editormodeshared.h"
#include "editor/editorFormat.hpp"
#include "editor/editorRoad.hpp"
#include "utilities/translation.h"
#include "world/Road.h"
#include "simulation/simulation.h"

#include <algorithm>
#include <cmath>

namespace
{

glm::dvec3 bezier(std::array<glm::dvec3, 4> const &P, double const T)
{
	auto const u{1.0 - T};
	return P[0] * (u * u * u) + P[1] * (3.0 * u * u * T) + P[2] * (3.0 * u * T * T) + P[3] * (T * T * T);
}

double flat_length(std::array<glm::dvec3, 4> const &P)
{
	double length{0.0};
	glm::dvec3 previous{P[0]};
	for (int i = 1; i <= 24; ++i)
	{
		auto const point{bezier(P, i / 24.0)};
		length += glm::length(glm::dvec2{point.x - previous.x, point.z - previous.z});
		previous = point;
	}
	return length;
}

// the bezier parts a path is made of: its pieces, or the path itself
std::vector<std::array<glm::dvec3, 4> *> parts(segment_data &Path)
{
	std::vector<std::array<glm::dvec3, 4> *> result;
	if (Path.pieces.empty())
		result.push_back(&Path.points);
	else
		for (auto &piece : Path.pieces)
			result.push_back(&piece.points);
	return result;
}

double constexpr kJoin{0.5}; // m: ends this close are the same point

} // namespace

void editor_mode::road_profile_smooth()
{
	auto &tool{m_roadtool};
	if (tool.selected == nullptr)
		return;
	auto pieces{editor_road::chain(*tool.selected)};
	std::string reason;
	for (auto const *road : pieces)
		if (false == editor_road::can_edit(*road, &reason))
		{
			tool.status = format(STR_C("\"%s\" can't be changed: %s"), road->name().c_str(), reason.c_str());
			return;
		}
	// the pieces in order along the road, each with the way it goes: from an end no other piece of the road meets
	struct oriented
	{
		road_node *road;
		bool reversed;
	};
	auto const end_of = [](road_node const *Road, int const End) { return Road->definition().point(static_cast<double>(End)); };
	auto const meets = [&](glm::dvec3 const &Point, road_node const *Except) {
		for (auto const *other : pieces)
			if (other != Except && (glm::distance(end_of(other, 0), Point) < kJoin || glm::distance(end_of(other, 1), Point) < kJoin))
				return true;
		return false;
	};
	road_node *start{pieces.front()};
	bool start_reversed{false};
	for (auto *road : pieces)
	{
		if (false == meets(end_of(road, 0), road))
		{
			start = road;
			start_reversed = false;
			break;
		}
		if (false == meets(end_of(road, 1), road))
		{
			start = road;
			start_reversed = true;
			break;
		}
	}
	std::vector<oriented> order{{start, start_reversed}};
	std::vector<road_node *> left{pieces};
	left.erase(std::find(left.begin(), left.end(), start));
	while (false == left.empty())
	{
		auto const tail{end_of(order.back().road, order.back().reversed ? 0 : 1)};
		auto next{left.end()};
		bool reversed{false};
		for (auto it{left.begin()}; it != left.end(); ++it)
		{
			if (glm::distance(end_of(*it, 0), tail) < kJoin)
			{
				next = it;
				reversed = false;
				break;
			}
			if (glm::distance(end_of(*it, 1), tail) < kJoin)
			{
				next = it;
				reversed = true;
				break;
			}
		}
		if (next == left.end())
			break; // a fork: the rest isn't in line with this run
		order.push_back({*next, reversed});
		left.erase(next);
	}

	// the heights along the road as they are, every couple of metres of the run seen from above
	std::vector<double> runs;
	std::vector<double> heights;
	std::vector<double> piece_start; // run at the start (in the order) of each piece
	double run{0.0};
	for (auto const &entry : order)
	{
		auto const &state{entry.road->definition()};
		piece_start.push_back(run);
		glm::dvec3 previous{state.point(entry.reversed ? 1.0 : 0.0)};
		int constexpr steps{64};
		for (int i = (runs.empty() ? 0 : 1); i <= steps; ++i)
		{
			auto const t{entry.reversed ? 1.0 - i / static_cast<double>(steps) : i / static_cast<double>(steps)};
			auto const point{state.point(t)};
			run += glm::length(glm::dvec2{point.x - previous.x, point.z - previous.z});
			previous = point;
			runs.push_back(run);
			heights.push_back(point.y);
		}
	}
	if (runs.size() < 3 || run < 5.0)
	{
		tool.status = STR_C("The road is too short to even out");
		return;
	}
	double constexpr step{2.0};
	auto const count{static_cast<std::size_t>(std::ceil(run / step)) + 1};
	auto const ds{run / static_cast<double>(count - 1)};
	std::vector<double> design(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		auto const s{ds * static_cast<double>(i)};
		auto const it{std::lower_bound(runs.begin(), runs.end(), s)};
		auto const k{static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(it - runs.begin(), 1, static_cast<std::ptrdiff_t>(runs.size()) - 1))};
		auto const t{(s - runs[k - 1]) / std::max(1e-9, runs[k] - runs[k - 1])};
		design[i] = heights[k - 1] + (heights[k] - heights[k - 1]) * std::clamp(t, 0.0, 1.0);
	}
	auto const original{design};
	// evened out: the ends stay; the grade no steeper than set, the bend of the profile no sharper than the radius of a crest
	// (bending down) or a dip (bending up), and as near the heights it had as that lets it
	auto const grade{std::clamp(static_cast<double>(tool.maxgrade), 0.1, 30.0) / 100.0};
	auto const crest{std::max(50.0, static_cast<double>(tool.crest))};
	auto const sag{std::max(50.0, static_cast<double>(tool.sag))};
	for (int pass = 0; pass < 4000; ++pass)
	{
		// pulled back towards where it was in the first half of the passes, then only the limits kept
		auto const pull{pass < 2000 ? 0.02 : 0.0};
		double moved{0.0};
		for (std::size_t i = 1; i + 1 < count; ++i)
		{
			design[i] += (original[i] - design[i]) * pull;
			auto const bend{design[i - 1] - 2.0 * design[i] + design[i + 1]}; // > 0: a dip
			auto const limit{ds * ds / (bend > 0.0 ? sag : crest)};
			if (std::abs(bend) > limit)
			{
				auto const excess{(std::abs(bend) - limit) * (bend > 0.0 ? 1.0 : -1.0)};
				design[i] += excess * 0.5;
				moved = std::max(moved, std::abs(excess));
			}
		}
		// the grade, both ways, the two ends where they are
		for (std::size_t i = 1; i + 1 < count; ++i)
			design[i] = std::clamp(design[i], design[i - 1] - grade * ds, design[i - 1] + grade * ds);
		for (std::size_t i = count - 2; i > 0; --i)
			design[i] = std::clamp(design[i], design[i + 1] - grade * ds, design[i + 1] + grade * ds);
		if (pass > 2200 && moved < 1e-5)
			break;
	}
	auto const height_at = [&](double const S) {
		auto const x{std::clamp(S / ds, 0.0, static_cast<double>(count - 1))};
		auto const i{std::min(static_cast<std::size_t>(x), count - 2)};
		return design[i] + (design[i + 1] - design[i]) * (x - static_cast<double>(i));
	};
	auto const grade_at = [&](double const S) { return (height_at(S + ds) - height_at(S - ds)) / (2.0 * ds); };

	// the heights given to the ends and the handles of every bezier part of the pieces; a handle rises with the grade at its end
	std::vector<std::pair<road_node *, road_node::state>> changes;
	double steepest{0.0}, highest{0.0};
	for (std::size_t n = 0; n < order.size(); ++n)
	{
		auto const &entry{order[n]};
		auto state{entry.road->definition()};
		auto list{parts(state.axis)};
		// the run along the piece in its own direction: from its start, which is at the end of the run of it when it's reversed
		double total{0.0};
		std::vector<double> lengths;
		for (auto *part : list)
		{
			lengths.push_back(flat_length(*part));
			total += lengths.back();
		}
		auto const piece_run{n + 1 < piece_start.size() ? piece_start[n + 1] - piece_start[n] : run - piece_start[n]};
		auto const scale{total > 1e-9 ? piece_run / total : 1.0};
		double along{0.0};
		auto const run_of = [&](double const Along) { return entry.reversed ? piece_start[n] + (total - Along) * scale : piece_start[n] + Along * scale; };
		auto const direction{entry.reversed ? -1.0 : 1.0};
		for (std::size_t k = 0; k < list.size(); ++k)
		{
			auto &p{*list[k]};
			auto const s0{run_of(along)}, s1{run_of(along + lengths[k])};
			auto const handle0{glm::length(glm::dvec2{p[1].x - p[0].x, p[1].z - p[0].z})};
			auto const handle1{glm::length(glm::dvec2{p[3].x - p[2].x, p[3].z - p[2].z})};
			highest = std::max(highest, std::max(std::abs(p[0].y - height_at(s0)), std::abs(p[3].y - height_at(s1))));
			p[0].y = height_at(s0);
			p[3].y = height_at(s1);
			p[1].y = p[0].y + grade_at(s0) * direction * handle0;
			p[2].y = p[3].y - grade_at(s1) * direction * handle1;
			steepest = std::max(steepest, std::abs(grade_at(s0)));
			along += lengths[k];
		}
		if (false == state.axis.pieces.empty())
		{
			// the path itself approximates its pieces: its ends and handles follow theirs
			auto &whole{state.axis.points};
			auto const &first{state.axis.pieces.front().points};
			auto const &last{state.axis.pieces.back().points};
			auto const handle0{glm::length(glm::dvec2{whole[1].x - whole[0].x, whole[1].z - whole[0].z})};
			auto const handle1{glm::length(glm::dvec2{whole[3].x - whole[2].x, whole[3].z - whole[2].z})};
			auto const g0{(first[1].y - first[0].y) / std::max(1e-9, glm::length(glm::dvec2{first[1].x - first[0].x, first[1].z - first[0].z}))};
			auto const g1{(last[3].y - last[2].y) / std::max(1e-9, glm::length(glm::dvec2{last[3].x - last[2].x, last[3].z - last[2].z}))};
			whole[0].y = first[0].y;
			whole[3].y = last[3].y;
			whole[1].y = whole[0].y + g0 * handle0;
			whole[2].y = whole[3].y - g1 * handle1;
		}
		changes.emplace_back(entry.road, std::move(state));
	}
	editor_road::record record;
	for (auto const &change : changes)
		record.roads.emplace_back(change.first, change.first->definition());
	editor_road::apply(changes);
	push_road_snapshot(std::move(record));
	road_select(tool.selected);
	tool.status = format(STR_C("Profile evened out over %.0f m, pieces: %zu; steepest %.1f %%, moved by up to %.2f m. Ctrl+Z takes it back"), run, changes.size(),
	                     steepest * 100.0, highest);
	if (order.size() < pieces.size())
		tool.status += std::string{". "} + STR_C("The road forks: only the run from its end to the fork was evened out");
}

void editor_mode::road_terrain(bool const Remove)
{
	auto &tool{m_roadtool};
	std::vector<road_node *> pieces;
	if (tool.every_road)
	{
		for (auto *road : simulation::Roads.sequence())
			if (road != nullptr)
				pieces.push_back(road);
	}
	else if (tool.selected != nullptr)
		pieces = editor_road::chain(*tool.selected);
	if (pieces.empty())
	{
		tool.status = STR_C("Select a road, or tick all the roads");
		return;
	}
	auto const name_of = [](road_node const *Road) { return "road " + (Road->name().empty() ? std::to_string(reinterpret_cast<std::uintptr_t>(Road)) : Road->name()); };
	terrain_history_step();
	if (Remove)
	{
		std::vector<std::string> names;
		for (auto const *road : pieces)
		{
			names.push_back(name_of(road));
			for (auto const &modifier : m_streamer.modifiers())
				if (modifier.name.rfind(name_of(road) + " #", 0) == 0)
					names.push_back(modifier.name);
		}
		auto const removed{m_streamer.remove_modifiers(names)};
		terrain_history_step();
		forget_ground();
		tool.status = format(STR_C("The terrain let go of the roads: %zu modifiers taken away. Ctrl+Z takes it back"), removed);
		return;
	}
	std::vector<heightmap::modifier> modifiers;
	for (auto const *road : pieces)
	{
		auto const &state{road->definition()};
		heightmap::modifier modifier;
		modifier.name = name_of(road);
		modifier.slope = std::clamp(static_cast<double>(tool.bankgrade), 0.25, 10.0);
		modifier.reach = 40.0;
		modifier.rounding = 1.0;
		modifier.natural = m_natural;
		// past the roadway: the kerbs and what lies beside it, half a metre more for the shoulder of the earth
		double beside{0.5};
		for (int side = 0; side < 2; ++side)
		{
			auto const &data{state.sides[side]};
			auto const extra{(state.kerbs[side] ? state.kerbwidth : 0.f) + (data.type != road_node::side_type::none ? data.width : 0.f)};
			beside = std::max(beside, 0.5 + static_cast<double>(extra));
		}
		glm::dvec3 previous{state.point(0.0)};
		double length{0.0};
		for (int i = 1; i <= 32; ++i)
		{
			auto const point{state.point(i / 32.0)};
			length += glm::length(glm::dvec2{point.x - previous.x, point.z - previous.z});
			previous = point;
		}
		auto const steps{std::clamp(static_cast<int>(std::ceil(length / 2.0)), 2, 2000)};
		// a stretch in a tunnel (a hole in the ground over the road) is left alone: the modifier is cut in two there, the
		// embankment over the road stays; each part ends square at the portal
		int part{0};
		auto const flush = [&](bool const Into) {
			if (modifier.points.size() >= 2)
			{
				auto piece{modifier};
				piece.name = part == 0 ? modifier.name : modifier.name + " #" + std::to_string(part);
				piece.open_end = Into;
				modifiers.push_back(std::move(piece));
				++part;
			}
			modifier.points.clear();
			modifier.open_start = Into;
		};
		for (int i = 0; i <= steps; ++i)
		{
			auto const t{i / static_cast<double>(steps)};
			auto const point{state.point(t)};
			auto const *chunk{m_streamer.terrain_at(point.x, point.z)};
			if (chunk != nullptr && chunk->data().hole_at(point.x, point.z))
			{
				flush(true);
				continue;
			}
			// just under the surface, so the ground doesn't show through the road
			modifier.points.push_back({{point.x, point.z}, point.y - 0.05, 0.5 * state.span(t) + beside});
		}
		flush(false);
	}
	{
		// the parts of before go first: the tunnels may be elsewhere now
		std::vector<std::string> old;
		for (auto const *road : pieces)
			for (auto const &modifier : m_streamer.modifiers())
				if (modifier.name == name_of(road) || modifier.name.rfind(name_of(road) + " #", 0) == 0)
					old.push_back(modifier.name);
		if (false == old.empty())
			m_streamer.remove_modifiers(old);
	}
	auto const count{modifiers.size()};
	m_streamer.modifiers(std::move(modifiers));
	terrain_history_step();
	forget_ground();
	tool.status = format(STR_C("The terrain shaped to the roads, pieces: %zu, slopes 1:%.2f. Ctrl+Z takes it back"), count, static_cast<double>(tool.bankgrade));
}

void editor_mode::road_tunnel(bool const Close)
{
	auto &tool{m_roadtool};
	if (tool.selected == nullptr)
		return;
	auto const pieces{editor_road::chain(*tool.selected)};
	auto const name_of = [](road_node const *Road) { return "road " + (Road->name().empty() ? std::to_string(reinterpret_cast<std::uintptr_t>(Road)) : Road->name()); };
	// the terrain led to these roads goes first, so the ground over them is the embankment or the hill and not the cutting the
	// modifier made through it; it's led again after, around the tunnels
	bool led{false};
	{
		std::vector<std::string> old;
		for (auto const *road : pieces)
			for (auto const &modifier : m_streamer.modifiers())
				if (modifier.name == name_of(road) || modifier.name.rfind(name_of(road) + " #", 0) == 0)
					old.push_back(modifier.name);
		terrain_history_step();
		if (false == old.empty())
		{
			m_streamer.remove_modifiers(old);
			led = true;
		}
	}
	auto const cover{std::max(0.2, static_cast<double>(tool.tunnel_cover))};
	std::size_t tunnels{0};
	double total{0.0};
	for (auto const *road : pieces)
	{
		auto const &state{road->definition()};
		double beside{0.5};
		for (int side = 0; side < 2; ++side)
		{
			auto const &data{state.sides[side]};
			auto const extra{(state.kerbs[side] ? state.kerbwidth : 0.f) + (data.type != road_node::side_type::none ? data.width : 0.f)};
			beside = std::max(beside, 0.5 + static_cast<double>(extra));
		}
		glm::dvec3 previous{state.point(0.0)};
		double length{0.0};
		for (int i = 1; i <= 32; ++i)
		{
			auto const point{state.point(i / 32.0)};
			length += glm::length(glm::dvec2{point.x - previous.x, point.z - previous.z});
			previous = point;
		}
		auto const steps{std::clamp(static_cast<int>(std::ceil(length)), 2, 20000)};
		// the stretches under the ground: [first, last] samples, a metre apart
		std::vector<std::pair<int, int>> runs;
		for (int i = 0; i <= steps; ++i)
		{
			auto const point{state.point(i / static_cast<double>(steps))};
			double ground{0.0};
			bool const under{m_streamer.height_at(point.x, point.z, ground, true) && ground > point.y + cover};
			if (under)
			{
				if (false == runs.empty() && runs.back().second == i - 1)
					runs.back().second = i;
				else
					runs.emplace_back(i, i);
			}
		}
		for (auto const &run : runs)
		{
			// a metre and a half more at both ends, so the portal opens onto the slope
			auto const extend{static_cast<int>(std::ceil(1.5 * steps / std::max(1.0, length)))};
			auto const first{std::max(0, run.first - extend)}, last{std::min(steps, run.second + extend)};
			if (last - first < 2)
				continue;
			std::vector<glm::dvec2> leftside, rightside;
			for (int i = first; i <= last; ++i)
			{
				auto const t{i / static_cast<double>(steps)};
				auto const point{state.point(t)};
				auto const tangent{state.tangent(t)};
				glm::dvec2 across{tangent.z, -tangent.x};
				if (glm::length(across) < 1e-9)
					continue;
				across = glm::normalize(across) * (0.5 * state.span(t) + beside);
				leftside.push_back(glm::dvec2{point.x, point.z} + across);
				rightside.push_back(glm::dvec2{point.x, point.z} - across);
			}
			if (leftside.size() < 2)
				continue;
			std::vector<glm::dvec2> outline{leftside};
			outline.insert(outline.end(), rightside.rbegin(), rightside.rend());
			m_streamer.cut(outline, false == Close);
			++tunnels;
			total += (last - first) * length / steps;
		}
	}
	terrain_history_step();
	forget_ground();
	if (led)
	{
		// only this road led again, whatever the tick for all of them says
		auto const every{tool.every_road};
		tool.every_road = false;
		road_terrain(false);
		tool.every_road = every;
	}
	if (Close)
		tool.status = format(STR_C("Holes along the road closed: %zu stretches. Ctrl+Z takes it back"), tunnels);
	else if (tunnels == 0)
		tool.status = format(STR_C("The ground is nowhere %.1f m or more over the road: no tunnel cut"), cover);
	else
		tool.status = format(STR_C("Tunnels cut: %zu, %.0f m together. The track and the embankment over them stay; Ctrl+Z takes it back"), tunnels, total);
}
