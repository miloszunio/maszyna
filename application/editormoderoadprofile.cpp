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
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include "model/AnimModel.h"

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

// a model of the structure of a tunnel, written as a .t3d: triangles of one material, both faces of each so it's seen from
// anywhere; the points are metres from Origin, the way the scenery has them
struct structure_mesh
{
	std::vector<glm::dvec3> points;
	std::vector<glm::vec2> uvs;

	void quad(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C, glm::dvec3 const &D, glm::vec2 const &Ta, glm::vec2 const &Tb, glm::vec2 const &Tc, glm::vec2 const &Td)
	{
		triangle(A, B, C, Ta, Tb, Tc);
		triangle(A, C, D, Ta, Tc, Td);
	}
	void triangle(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C, glm::vec2 const &Ta, glm::vec2 const &Tb, glm::vec2 const &Tc)
	{
		points.insert(points.end(), {A, B, C, A, C, B});
		uvs.insert(uvs.end(), {Ta, Tb, Tc, Ta, Tc, Tb});
	}
	bool write(std::string const &Path, glm::dvec3 const &Origin, std::string const &Material) const
	{
		std::ofstream file{Path};
		if (false == file.good())
			return false;
		file << "// the structure of a tunnel under a road, made by the editor\n"
		     << "Parent: none\nType: Mesh\nName: tunel\nAnim: false\nAmbient: 255 255 255\nDiffuse: 255 255 255\nSpecular: 0 0 0\n"
		     << "SelfIllum: false\nWire: false\nWireSize: 1\nOpacity: 1\nMap: " << Material << "\nMaxDistance: 3000\nMinDistance: 0\n"
		     << "Transform:\n 1 0 0 0\n 0 1 0 0\n 0 0 1 0\n 0 0 0 1\n\n";
		file << "NumIndices: " << points.size() << "\n";
		for (std::size_t i = 0; i < points.size(); i += 3)
			file << i << ' ' << i + 1 << ' ' << i + 2 << "\n";
		file << "NumVerts: " << points.size() << "\n";
		file.precision(4);
		file << std::fixed;
		for (std::size_t i = 0; i < points.size(); i += 3)
		{
			auto const a{points[i] - Origin}, b{points[i + 1] - Origin}, c{points[i + 2] - Origin};
			auto normal{glm::cross(b - a, c - a)};
			normal = glm::length(normal) > 1e-12 ? glm::normalize(normal) : glm::dvec3{0.0, 1.0, 0.0};
			for (int k = 0; k < 3; ++k)
			{
				auto const p{points[i + k] - Origin};
				file << p.x << ' ' << p.y << ' ' << p.z << ' ' << normal.x << ' ' << normal.y << ' ' << normal.z << ' ' << uvs[i + k].x << ' ' << uvs[i + k].y
				     << " 1 0 0 1\n";
			}
		}
		return file.good();
	}
};

struct oriented_piece
{
	road_node *road;
	bool reversed;
};

// the pieces of a road in order along it, each with the way it goes: from an end no other piece of the road meets; at a fork
// the run stops
std::vector<oriented_piece> ordered_chain(std::vector<road_node *> const &Pieces)
{
	auto const &pieces{Pieces};
	using oriented = oriented_piece;
	// the pieces in order along the road, each with the way it goes: from an end no other piece of the road meets
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

	return order;
}

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
	auto const order{ordered_chain(pieces)};
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
	{
		// the structures of before go: made again where the tunnels are now, or not at all when they're closed
		std::vector<TAnimModel *> old;
		for (auto *model : simulation::Instances.sequence())
		{
			if (model == nullptr)
				continue;
			for (auto const *road : pieces)
				if (model->name().rfind("tunel " + name_of(road) + " ", 0) == 0)
				{
					old.push_back(model);
					break;
				}
		}
		for (auto *model : old)
		{
			if (m_node == model)
			{
				m_node = nullptr;
				ui()->set_node(nullptr);
			}
			erase_model(model);
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
			if (false == Close && tool.tunnel_structure)
				road_tunnel_structure(*road, run.first, run.second, steps, beside, tunnels);
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

void editor_mode::road_profile_window_open()
{
	m_road_profile.open = true;
	m_road_profile.road = m_roadtool.selected;
	road_profile_sample();
}

void editor_mode::road_profile_window_toggle()
{
	if (m_road_profile.open)
		m_road_profile.open = false;
	else
		road_profile_window_open();
}

void editor_mode::road_profile_sample()
{
	auto &view{m_road_profile};
	view.runs.clear();
	view.heights.clear();
	view.ground.clear();
	if (view.road == nullptr)
		return;
	auto const order{ordered_chain(editor_road::chain(*view.road))};
	double run{0.0};
	glm::dvec3 previous{0.0};
	bool first{true};
	for (auto const &entry : order)
	{
		auto const &state{entry.road->definition()};
		int constexpr steps{48};
		for (int i = 0; i <= steps; ++i)
		{
			if (false == first && i == 0)
				continue;
			auto const t{entry.reversed ? 1.0 - i / static_cast<double>(steps) : i / static_cast<double>(steps)};
			auto const point{state.point(t)};
			if (false == first)
				run += glm::length(glm::dvec2{point.x - previous.x, point.z - previous.z});
			first = false;
			previous = point;
			double ground{0.0};
			view.runs.push_back(static_cast<float>(run));
			view.heights.push_back(static_cast<float>(point.y));
			view.ground.push_back(m_streamer.height_at(point.x, point.z, ground, true) ? static_cast<float>(ground) : std::numeric_limits<float>::quiet_NaN());
		}
	}
}

void editor_mode::render_road_profile_window()
{
	auto &view{m_road_profile};
	if (false == view.open)
		return;
	// the road gone (deleted, or another scenery): the window too
	bool alive{false};
	for (auto const *road : simulation::Roads.sequence())
		if (road == view.road)
			alive = true;
	if (false == alive)
		view.road = nullptr;
	auto &tool{m_roadtool};
	// the window follows the road selected: another piece clicked, the profile of its road read
	if (tool.selected != nullptr && tool.selected != view.road)
	{
		view.road = tool.selected;
		road_profile_sample();
	}
	ImGui::SetNextWindowSize(ImVec2(760.0f, 470.0f), ImGuiCond_FirstUseEver);
	if (false == ImGui::Begin(STR_C("Vertical profile of the road###roadprofile"), &view.open))
	{
		ImGui::End();
		return;
	}
	if (view.road == nullptr)
	{
		ImGui::TextWrapped("%s", STR_C("Select a road (Roads, Select, LMB on a piece) and its profile shows here"));
		ImGui::End();
		return;
	}
	ImGui::TextUnformatted(view.road->name().c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton(STR_C("Read again##roadprofile")))
		road_profile_sample();
	// the graph: the ground brown, the road grey, heights stretched to fill it
	auto const size{ImVec2(ImGui::GetContentRegionAvail().x, 200.0f)};
	auto const origin{ImGui::GetCursorScreenPos()};
	ImGui::InvisibleButton("##roadprofilegraph", size);
	auto *draw{ImGui::GetWindowDrawList()};
	draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(25, 28, 32, 255));
	if (view.runs.size() >= 2)
	{
		float low{std::numeric_limits<float>::max()}, high{-low};
		for (std::size_t i = 0; i < view.runs.size(); ++i)
		{
			low = std::min(low, view.heights[i]);
			high = std::max(high, view.heights[i]);
			if (std::isfinite(view.ground[i]))
			{
				low = std::min(low, view.ground[i]);
				high = std::max(high, view.ground[i]);
			}
		}
		high = std::max(high, low + 2.0f);
		auto const margin{(high - low) * 0.08f};
		low -= margin;
		high += margin;
		auto const length{std::max(1.0f, view.runs.back())};
		auto const at = [&](float const Run, float const Height) {
			return ImVec2(origin.x + Run / length * size.x, origin.y + size.y - (Height - low) / (high - low) * size.y);
		};
		for (std::size_t i = 1; i < view.runs.size(); ++i)
			if (std::isfinite(view.ground[i - 1]) && std::isfinite(view.ground[i]))
				draw->AddLine(at(view.runs[i - 1], view.ground[i - 1]), at(view.runs[i], view.ground[i]), IM_COL32(170, 120, 70, 255), 2.0f);
		for (std::size_t i = 1; i < view.runs.size(); ++i)
			draw->AddLine(at(view.runs[i - 1], view.heights[i - 1]), at(view.runs[i], view.heights[i]), IM_COL32(220, 220, 225, 255), 2.0f);
		// where the cursor is: the run, the heights and the grade
		if (ImGui::IsItemHovered())
		{
			auto const mouse{ImGui::GetIO().MousePos};
			auto const run{(mouse.x - origin.x) / size.x * length};
			auto const it{std::lower_bound(view.runs.begin(), view.runs.end(), run)};
			auto const i{static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(it - view.runs.begin(), 1, static_cast<std::ptrdiff_t>(view.runs.size()) - 1))};
			auto const grade{(view.heights[i] - view.heights[i - 1]) / std::max(0.01f, view.runs[i] - view.runs[i - 1]) * 100.0f};
			draw->AddLine(ImVec2(mouse.x, origin.y), ImVec2(mouse.x, origin.y + size.y), IM_COL32(255, 255, 255, 60));
			ImGui::SetTooltip(STR_C("%.0f m: road %.2f m, ground %.2f m, grade %.1f %%"), view.runs[i], view.heights[i], view.ground[i], grade);
		}
		ImGui::TextDisabled(STR_C("%.0f m long, heights %.1f - %.1f m; the road grey, the ground brown"), length, low + margin, high - margin);
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Steepest grade (%)##road"), &tool.maxgrade, 0.1f, 0.5f, 20.0f, "%.1f");
	item_tooltip("A road in the country 6-8 %, a main road 4-5 %, a motorway 4 %");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Crest radius (m)##road"), &tool.crest, 50.0f, 50.0f, 50000.0f, "%.0f");
	item_tooltip("The smallest radius of the vertical curve over a hilltop: the larger, the farther a driver sees over it");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Dip radius (m)##road"), &tool.sag, 50.0f, 50.0f, 50000.0f, "%.0f");
	item_tooltip("The smallest radius of the vertical curve in a dip");
	auto const selected_before{tool.selected};
	tool.selected = view.road;
	if (ImGui::Button(STR_C("Even out the profile of the road")))
	{
		road_profile_smooth();
		road_profile_sample();
	}
	item_tooltip("The heights of all the pieces of the road: grades no steeper and curves no sharper than set, as near the heights it had as that lets;\n"
	             "its two ends stay where they are, so it keeps meeting the junctions. Ctrl+Z takes it back");
	ImGui::SeparatorText(STR_C("Terrain##roadprofile"));
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::InputFloat(STR_C("Bank slope 1:n"), &tool.bankgrade, 0.25f, 0.5f, "%.2f"))
		tool.bankgrade = std::clamp(tool.bankgrade, 0.25f, 10.0f);
	ImGui::Checkbox(STR_C("All the roads of the scenery##roadterrain"), &tool.every_road);
	if (ImGui::Button(STR_C("Lead the terrain to the road")))
	{
		road_terrain(false);
		road_profile_sample();
	}
	item_tooltip("The terrain made flat under the roadway and its sides, with slopes of the grade set for the banks (1:n) down or up to the ground around,\n"
	             "rough as the natural slopes of the track bed are set. It follows the road when it's led again. Ctrl+Z takes it back");
	ImGui::SameLine();
	if (ImGui::Button(STR_C("Let go##roadterrain")))
	{
		road_terrain(true);
		road_profile_sample();
	}
	item_tooltip("The terrain no longer shaped by the road (of the selected one, or of all of them)");
	ImGui::SeparatorText(STR_C("Tunnels and underpasses##roadprofile"));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Tunnel where the ground is over the road by (m)##road"), &tool.tunnel_cover, 0.1f, 0.5f, 30.0f, "%.1f");
	item_tooltip("Where the ground at the axis is this much over the surface of the road, the road goes under it: through an embankment, a hill");
	ImGui::Checkbox(STR_C("Build the structure##tunnel"), &tool.tunnel_structure);
	item_tooltip("Walls along the road, the deck or the vault over it and the wing walls at the portals, as a model of its own placed with the tunnel");
	if (ImGui::Button(STR_C("Cut a tunnel")))
	{
		road_tunnel(false);
		road_profile_sample();
	}
	item_tooltip("Along the road, where it runs into an embankment or a hill, a hole cut in the ground as wide as the road and its sides:\n"
	             "the embankment and the track on it stay over the road, the terrain led to the road stops at the portals. Ctrl+Z takes it back");
	ImGui::SameLine();
	if (ImGui::Button(STR_C("Close it##tunnel")))
	{
		road_tunnel(true);
		road_profile_sample();
	}
	item_tooltip("The holes along the road closed again, its structures taken away");
	tool.selected = selected_before != nullptr ? selected_before : view.road;
	if (false == tool.status.empty())
		ImGui::TextWrapped("%s", tool.status.c_str());
	ImGui::End();
}


void editor_mode::road_tunnel_structure(road_node const &Road, int const First, int const Last, int const Steps, double const Beside, std::size_t const Number)
{
	auto const &state{Road.definition()};
	auto const name_of = [](road_node const *R) { return "road " + (R->name().empty() ? std::to_string(reinterpret_cast<std::uintptr_t>(R)) : R->name()); };
	double constexpr clearance{4.7}; // m over the road to the soffit, as under a bridge over a road
	double constexpr deck{0.8}; // m the deck or the vault is thick
	double constexpr wing{4.0}; // m the wing walls reach out of the portals
	structure_mesh mesh;
	struct section
	{
		glm::dvec3 left, right; // the feet of the walls, at the surface of the road
		double top; // of the structure: the ground over it, or the soffit and the deck where the ground is far up (a hill)
		double along;
	};
	std::vector<section> sections;
	double along{0.0};
	glm::dvec3 previous{state.point(First / static_cast<double>(Steps))};
	for (int i = First; i <= Last; ++i)
	{
		auto const t{i / static_cast<double>(Steps)};
		auto const point{state.point(t)};
		along += glm::length(point - previous);
		previous = point;
		auto const tangent{state.tangent(t)};
		glm::dvec3 across{tangent.z, 0.0, -tangent.x};
		if (glm::length(across) < 1e-9)
			continue;
		across = glm::normalize(across) * (0.5 * state.span(t) + Beside);
		double ground{point.y + clearance + deck};
		m_streamer.height_at(point.x, point.z, ground, true);
		// under an embankment the deck is its top; under a hill a vault at the clearance, the ground above it closed by its top
		auto const top{std::max(point.y + clearance + deck, std::min(ground, point.y + clearance + deck + 1.0))};
		sections.push_back({point + across - glm::dvec3{0.0, 0.3, 0.0}, point - across - glm::dvec3{0.0, 0.3, 0.0}, std::max(top, ground - 0.05), along});
	}
	if (sections.size() < 2)
		return;
	double constexpr tile{4.0};
	for (std::size_t i = 1; i < sections.size(); ++i)
	{
		auto const &a{sections[i - 1]};
		auto const &b{sections[i]};
		auto const ua{static_cast<float>(a.along / tile)}, ub{static_cast<float>(b.along / tile)};
		// the walls, from below the road up to the top
		for (auto const side : {0, 1})
		{
			auto const &fa{side == 0 ? a.left : a.right};
			auto const &fb{side == 0 ? b.left : b.right};
			glm::dvec3 const ta{fa.x, a.top, fa.z}, tb{fb.x, b.top, fb.z};
			mesh.quad(fa, fb, tb, ta, {ua, 0.f}, {ub, 0.f}, {ub, static_cast<float>((b.top - fb.y) / tile)}, {ua, static_cast<float>((a.top - fa.y) / tile)});
		}
		// the soffit at the clearance and the top of the deck, as wide as between the walls
		for (auto const level : {0, 1})
		{
			auto const ya{level == 0 ? std::min(a.top - deck, a.left.y + 0.3 + clearance) : a.top};
			auto const yb{level == 0 ? std::min(b.top - deck, b.left.y + 0.3 + clearance) : b.top};
			glm::dvec3 const la{a.left.x, ya, a.left.z}, ra{a.right.x, ya, a.right.z}, lb{b.left.x, yb, b.left.z}, rb{b.right.x, yb, b.right.z};
			auto const width{static_cast<float>(glm::distance(a.left, a.right) / tile)};
			mesh.quad(la, lb, rb, ra, {ua, 0.f}, {ub, 0.f}, {ub, width}, {ua, width});
		}
	}
	// at the portals: the face of the deck over the opening, and the wing walls going out at 45 degrees, falling to the road
	for (auto const end : {0, 1})
	{
		auto const &s{end == 0 ? sections.front() : sections.back()};
		auto const &n{end == 0 ? sections[1] : sections[sections.size() - 2]};
		auto out{glm::dvec3{s.left.x - n.left.x, 0.0, s.left.z - n.left.z}};
		if (glm::length(out) < 1e-9)
			continue;
		out = glm::normalize(out);
		auto const soffit{std::min(s.top - deck, s.left.y + 0.3 + clearance)};
		glm::dvec3 const l0{s.left.x, soffit, s.left.z}, r0{s.right.x, soffit, s.right.z}, l1{s.left.x, s.top, s.left.z}, r1{s.right.x, s.top, s.right.z};
		auto const width{static_cast<float>(glm::distance(s.left, s.right) / tile)};
		mesh.quad(l0, r0, r1, l1, {0.f, 0.f}, {width, 0.f}, {width, static_cast<float>((s.top - soffit) / tile)}, {0.f, static_cast<float>((s.top - soffit) / tile)});
		auto const sideways{glm::normalize(s.left - s.right)};
		for (auto const side : {0, 1})
		{
			auto const &foot{side == 0 ? s.left : s.right};
			auto const away{side == 0 ? sideways : -sideways};
			auto const tip{foot + (out + away) * (wing / std::sqrt(2.0))};
			glm::dvec3 const top{foot.x, s.top, foot.z};
			mesh.quad(foot, tip, glm::dvec3{tip.x, foot.y + 0.6, tip.z}, top, {0.f, 0.f}, {static_cast<float>(wing / tile), 0.f}, {static_cast<float>(wing / tile), 0.15f},
			          {0.f, static_cast<float>((s.top - foot.y) / tile)});
		}
	}
	// a file of its own each time: the models read once are kept by the simulation under their names
	auto const stamp{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()};
	std::string clean{name_of(&Road)};
	for (auto &c : clean)
		if (false == std::isalnum(static_cast<unsigned char>(c)))
			c = '_';
	auto const relative{"kabina_tunele/" + clean + "_" + std::to_string(Number) + "_" + std::to_string(stamp) + ".t3d"};
	std::error_code error;
	std::filesystem::create_directories("models/kabina_tunele", error);
	auto const origin{sections.front().left};
	if (false == mesh.write("models/" + relative, origin, m_roadtool.tunnel_material))
		return;
	auto *model{simulation::State.create_model("node 3000 0 none model 0 0 0 0 " + relative + " none endmodel",
	                                           "tunel " + name_of(&Road) + " " + std::to_string(Number), origin)};
	(void)model;
}
