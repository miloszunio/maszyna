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
#include "editor/editorCrossing.hpp"
#include "editor/editorStructure.hpp"
#include "utilities/translation.h"
#include "world/Road.h"
#include "simulation/simulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "model/AnimModel.h"

namespace
{

glm::dvec3 bezier(std::array<glm::dvec3, 4> const &P, double const T)
{
	auto const u{1.0 - T};
	return P[0] * (u * u * u) + P[1] * (3.0 * u * u * T) + P[2] * (3.0 * u * T * T) + P[3] * (T * T * T);
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

namespace
{

// the ends of a bezier part and its handles as points: the scenery keeps the handles as vectors from the ends,
// both zero for a straight
std::array<glm::dvec3, 4> absolute(std::array<glm::dvec3, 4> const &P)
{
	if (P[1] == glm::dvec3{0.0} && P[2] == glm::dvec3{0.0})
		return {P[0], P[0] + (P[3] - P[0]) / 3.0, P[3] - (P[3] - P[0]) / 3.0, P[3]};
	return {P[0], P[0] + P[1], P[3] + P[2], P[3]};
}

double flat(glm::dvec3 const &V)
{
	return glm::length(glm::dvec2{V.x, V.z});
}

// slope of a vector: metres up for a metre seen from above
double slope(glm::dvec3 const &V)
{
	auto const run{flat(V)};
	return run > 1e-6 ? V.y / run : 0.0;
}

// the road of a piece as a profile: the points where its bezier parts meet or end (the nodes), in order along it, with
// the run to each, the height and the grade there (along the run); the parts know which nodes are at their two ends
struct chain_profile
{
	struct part_ref
	{
		std::size_t entry; // in order
		std::size_t part; // of the path of that piece
		std::size_t node0; // node at the start of the part (its point 0)
		std::size_t node3; // node at its end
		std::size_t first; // of the two, the one earlier along the run
	};
	std::vector<oriented_piece> order;
	std::vector<road_node::state> states;
	std::vector<part_ref> refs;
	std::vector<double> runs;
	std::vector<double> heights;
	std::vector<double> grades;
	std::vector<glm::dvec3> positions;

	bool empty() const { return runs.size() < 2; }
};

chain_profile build_profile(road_node *Road)
{
	chain_profile profile;
	if (Road == nullptr)
		return profile;
	profile.order = ordered_chain(editor_road::chain(*Road));
	for (auto const &entry : profile.order)
		profile.states.push_back(entry.road->definition());
	std::vector<double> sums, counts;
	auto const node = [&](glm::dvec3 const &Position, double const Run) {
		profile.positions.push_back(Position);
		profile.runs.push_back(Run);
		sums.push_back(0.0);
		counts.push_back(0.0);
		return profile.positions.size() - 1;
	};
	for (std::size_t n = 0; n < profile.order.size(); ++n)
	{
		auto const reversed{profile.order[n].reversed};
		auto list{parts(profile.states[n].axis)};
		for (std::size_t index = 0; index < list.size(); ++index)
		{
			auto const k{reversed ? list.size() - 1 - index : index};
			auto const a{absolute(*list[k])};
			auto const start{reversed ? a[3] : a[0]};
			auto const end{reversed ? a[0] : a[3]};
			if (profile.runs.empty())
				node(start, 0.0);
			double length{0.0};
			glm::dvec3 previous{a[0]};
			for (int i = 1; i <= 32; ++i)
			{
				auto const point{bezier(a, i / 32.0)};
				length += flat(point - previous);
				previous = point;
			}
			auto const from{profile.positions.size() - 1};
			auto const to{node(end, profile.runs.back() + length)};
			chain_profile::part_ref ref{n, k, reversed ? to : from, reversed ? from : to, from};
			profile.refs.push_back(ref);
			// the grades at the two ends, along the run
			auto const sign{reversed ? -1.0 : 1.0};
			auto const lead{flat(a[1] - a[0]) > 1e-6 ? a[1] - a[0] : a[3] - a[0]};
			auto const tail{flat(a[3] - a[2]) > 1e-6 ? a[3] - a[2] : a[3] - a[0]};
			sums[ref.node0] += sign * slope(lead);
			counts[ref.node0] += 1.0;
			sums[ref.node3] += sign * slope(tail);
			counts[ref.node3] += 1.0;
		}
	}
	for (std::size_t i = 0; i < profile.positions.size(); ++i)
	{
		profile.heights.push_back(profile.positions[i].y);
		profile.grades.push_back(counts[i] > 0.0 ? sums[i] / counts[i] : 0.0);
	}
	return profile;
}

// the grade at a node from its neighbours, for nodes whose heights were set without one
double neighbour_grade(chain_profile const &Profile, std::vector<double> const &Heights, std::size_t const I)
{
	auto const last{Heights.size() - 1};
	auto const a{I > 0 ? I - 1 : I};
	auto const b{I < last ? I + 1 : I};
	auto const run{Profile.runs[b] - Profile.runs[a]};
	return run > 1e-6 ? (Heights[b] - Heights[a]) / run : 0.0;
}

// the road along the run, sampled every couple of metres between two nodes, as it is now
void sample_between(chain_profile const &Profile, std::size_t const First, std::size_t const Last, std::vector<double> &Runs, std::vector<double> &Heights,
                    std::vector<glm::dvec3> *Points = nullptr)
{
	for (auto const &ref : Profile.refs)
	{
		auto const later{ref.first == ref.node0 ? ref.node3 : ref.node0};
		if (ref.first < First || later > Last)
			continue;
		auto list{parts(const_cast<road_node::state &>(Profile.states[ref.entry]).axis)};
		auto const a{absolute(*list[ref.part])};
		auto const forward{ref.first == ref.node0};
		auto const s0{Profile.runs[ref.first]}, s1{Profile.runs[later]};
		int constexpr steps{32};
		std::vector<double> along{0.0};
		std::vector<glm::dvec3> points{bezier(a, forward ? 0.0 : 1.0)};
		glm::dvec3 previous{points.front()};
		for (int i = 1; i <= steps; ++i)
		{
			auto const t{forward ? i / static_cast<double>(steps) : 1.0 - i / static_cast<double>(steps)};
			auto const point{bezier(a, t)};
			along.push_back(along.back() + flat(point - previous));
			points.push_back(point);
			previous = point;
		}
		auto const scale{along.back() > 1e-9 ? (s1 - s0) / along.back() : 0.0};
		for (std::size_t i = (Runs.empty() ? 0 : 1); i < along.size(); ++i)
		{
			Runs.push_back(s0 + along[i] * scale);
			Heights.push_back(points[i].y);
			if (Points != nullptr)
				Points->push_back(points[i]);
		}
	}
}

// evened out between two nodes, which stay where they are: the grade no steeper than set, the bend of the profile no sharper
// than the radius of a crest (bending down) or a dip (bending up), and as near the heights it had as that lets it.
// The heights and the grades of the nodes between come out of it
void smooth_between(chain_profile const &Profile, std::size_t const First, std::size_t const Last, double const Grade, double const Crest, double const Sag,
                    std::vector<double> &Heights, std::vector<double> &Grades)
{
	std::vector<double> runs, heights;
	sample_between(Profile, First, Last, runs, heights);
	if (runs.size() < 3)
		return;
	auto const from{Profile.runs[First]}, to{Profile.runs[Last]};
	auto const length{to - from};
	if (length < 1.0)
		return;
	auto const count{static_cast<std::size_t>(std::ceil(length / 2.0)) + 1};
	auto const ds{length / static_cast<double>(count - 1)};
	std::vector<double> design(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		auto const s{from + ds * static_cast<double>(i)};
		auto const it{std::lower_bound(runs.begin(), runs.end(), s)};
		auto const k{static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(it - runs.begin(), 1, static_cast<std::ptrdiff_t>(runs.size()) - 1))};
		auto const t{(s - runs[k - 1]) / std::max(1e-9, runs[k] - runs[k - 1])};
		design[i] = heights[k - 1] + (heights[k] - heights[k - 1]) * std::clamp(t, 0.0, 1.0);
	}
	design.front() = Heights[First];
	design.back() = Heights[Last];
	auto const original{design};
	for (int pass = 0; pass < 4000; ++pass)
	{
		auto const pull{pass < 2000 ? 0.02 : 0.0};
		double moved{0.0};
		for (std::size_t i = 1; i + 1 < count; ++i)
		{
			design[i] += (original[i] - design[i]) * pull;
			auto const bend{design[i - 1] - 2.0 * design[i] + design[i + 1]}; // > 0: a dip
			auto const limit{ds * ds / (bend > 0.0 ? Sag : Crest)};
			if (std::abs(bend) > limit)
			{
				auto const excess{(std::abs(bend) - limit) * (bend > 0.0 ? 1.0 : -1.0)};
				design[i] += excess * 0.5;
				moved = std::max(moved, std::abs(excess));
			}
		}
		for (std::size_t i = 1; i + 1 < count; ++i)
			design[i] = std::clamp(design[i], design[i - 1] - Grade * ds, design[i - 1] + Grade * ds);
		for (std::size_t i = count - 2; i > 0; --i)
			design[i] = std::clamp(design[i], design[i + 1] - Grade * ds, design[i + 1] + Grade * ds);
		if (pass > 2200 && moved < 1e-5)
			break;
	}
	auto const height_at = [&](double const S) {
		auto const x{std::clamp((S - from) / ds, 0.0, static_cast<double>(count - 1))};
		auto const i{std::min(static_cast<std::size_t>(x), count - 2)};
		return design[i] + (design[i + 1] - design[i]) * (x - static_cast<double>(i));
	};
	auto const grade_at = [&](double const S) {
		auto const a{std::max(from, S - ds)}, b{std::min(to, S + ds)};
		return b - a > 1e-9 ? (height_at(b) - height_at(a)) / (b - a) : 0.0;
	};
	for (std::size_t k = First; k <= Last; ++k)
	{
		if (k != First && k != Last)
			Heights[k] = height_at(Profile.runs[k]);
		Grades[k] = grade_at(Profile.runs[k]);
	}
}

// classes of roads with about the grades and the radii of vertical curves the rules give them
struct road_class
{
	char const *name;
	float grade; // %
	float crest; // m
	float sag; // m
};
road_class const kRoadClasses[]{
    {"Field or forest track", 12.0f, 300.0f, 200.0f},
    {"Access road (D)", 10.0f, 600.0f, 300.0f},
    {"Local road (L)", 8.0f, 1000.0f, 600.0f},
    {"Collector road (Z)", 7.0f, 1500.0f, 1000.0f},
    {"Main road (G)", 6.0f, 2500.0f, 1500.0f},
    {"Main road of fast traffic (GP)", 5.0f, 4500.0f, 2500.0f},
    {"Expressway (S)", 4.5f, 8000.0f, 3500.0f},
    {"Motorway (A)", 4.0f, 12000.0f, 4500.0f},
};

// the heights and the grades given to the nodes written into the pieces: the ends of every part at the heights, its handles
// rising with the grades; a straight gets its handles made so it can bend up and down. The arms of the junctions at the two
// ends of the run go with them
void write_profile(chain_profile &Profile, std::vector<double> const &Given, std::vector<double> const &Grades,
                   std::vector<std::pair<road_node *, road_node::state>> &Changes, std::vector<std::pair<junction_node *, junction_node::state>> &Junctions)
{
	// a run closed in a loop: its first and last node are one point, at one height
	auto Heights{Given};
	auto const closed{Profile.positions.size() > 2 && glm::distance(Profile.positions.front(), Profile.positions.back()) < 0.25};
	if (closed)
	{
		// the one which was changed gives it
		if (std::abs(Heights.back() - Profile.heights.back()) > 1e-9 && std::abs(Heights.front() - Profile.heights.front()) <= 1e-9)
			Heights.front() = Heights.back();
		else
			Heights.back() = Heights.front();
	}
	for (auto const &ref : Profile.refs)
	{
		auto list{parts(Profile.states[ref.entry].axis)};
		auto &p{*list[ref.part]};
		auto const a{absolute(p)};
		if (p[1] == glm::dvec3{0.0} && p[2] == glm::dvec3{0.0})
		{
			p[1] = a[1] - a[0];
			p[2] = a[2] - a[3];
		}
		auto const sign{Profile.order[ref.entry].reversed ? -1.0 : 1.0};
		p[0].y = Heights[ref.node0];
		p[3].y = Heights[ref.node3];
		p[1].y = sign * Grades[ref.node0] * flat(p[1]);
		p[2].y = -sign * Grades[ref.node3] * flat(p[2]);
	}
	for (auto &state : Profile.states)
	{
		if (state.axis.pieces.empty())
			continue;
		// the path itself approximates its pieces: its ends and handles follow theirs
		auto &whole{state.axis.points};
		auto const &first{state.axis.pieces.front().points};
		auto const &last{state.axis.pieces.back().points};
		auto const a{absolute(whole)};
		if (whole[1] == glm::dvec3{0.0} && whole[2] == glm::dvec3{0.0})
		{
			whole[1] = a[1] - a[0];
			whole[2] = a[2] - a[3];
		}
		whole[0].y = first[0].y;
		whole[3].y = last[3].y;
		whole[1].y = slope(first[1]) * flat(whole[1]);
		// the slope at its end is the one of the vector back from the end turned round, so the vectors share it as they are
		whole[2].y = slope(last[2]) * flat(whole[2]);
	}
	for (std::size_t n = 0; n < Profile.order.size(); ++n)
		Changes.emplace_back(Profile.order[n].road, Profile.states[n]);
	for (auto const end : {std::size_t{0}, Profile.positions.size() - 1})
	{
		// the other pieces ending there (a change of lanes, three or more meeting with no junction) go with the end of the run,
		// as the Raise of the road tool takes them, so the road isn't pulled apart
		for (auto const &other : editor_road::ends_at(Profile.positions[end]))
		{
			if (std::any_of(Profile.order.begin(), Profile.order.end(), [&other](oriented_piece const &Piece) { return Piece.road == other.road; }))
				continue;
			auto listed{std::find_if(Changes.begin(), Changes.end(), [&other](auto const &Change) { return Change.first == other.road; })};
			if (listed == Changes.end())
			{
				Changes.emplace_back(other.road, other.road->definition());
				listed = std::prev(Changes.end());
			}
			listed->second.axis.points[other.atend ? segment_data::point::end : segment_data::point::start].y = Heights[end];
		}
		for (auto const &arm : editor_road::arms_at(Profile.positions[end]))
		{
			auto entry{std::find_if(Junctions.begin(), Junctions.end(), [&arm](auto const &Listed) { return Listed.first == arm.first; })};
			if (entry == Junctions.end())
			{
				Junctions.emplace_back(arm.first, arm.first->definition());
				entry = std::prev(Junctions.end());
			}
			entry->second.arms[arm.second].position.y = Heights[end];
		}
	}
}

// whether something else meets the run at that end node (another piece or a junction arm): only there the end keeps its grade,
// a free end takes the grade the evening out gives it
bool end_joined(chain_profile const &Profile, std::size_t const End)
{
	for (auto const &other : editor_road::ends_at(Profile.positions[End]))
		if (std::ranges::none_of(Profile.order, [&other](oriented_piece const &Piece) { return Piece.road == other.road; }))
			return true;
	return false == editor_road::arms_at(Profile.positions[End]).empty();
}

} // namespace

void editor_mode::road_profile_smooth()
{
	auto &tool{m_roadtool};
	auto *road{m_road_profile.road != nullptr ? m_road_profile.road : tool.selected};
	auto profile{build_profile(road)};
	if (profile.empty() || profile.runs.back() < 5.0)
	{
		tool.status = STR_C("The road is too short to even out");
		return;
	}
	auto heights{profile.heights};
	auto grades{profile.grades};
	auto const grade{std::clamp(static_cast<double>(tool.maxgrade), 0.1, 30.0) / 100.0};
	smooth_between(profile, 0, heights.size() - 1, grade, std::max(50.0, static_cast<double>(tool.crest)), std::max(50.0, static_cast<double>(tool.sag)), heights, grades);
	// the ends meet the neighbours as they did: their grades kept, or the road gets a kink there
	if (end_joined(profile, 0))
		grades.front() = profile.grades.front();
	if (end_joined(profile, grades.size() - 1))
		grades.back() = profile.grades.back();
	double moved{0.0}, steepest{0.0};
	for (std::size_t i = 0; i < heights.size(); ++i)
	{
		moved = std::max(moved, std::abs(heights[i] - profile.heights[i]));
		steepest = std::max(steepest, std::abs(grades[i]));
	}
	if (road_profile_commit(road, heights, grades))
		tool.status = format(STR_C("Profile evened out over %.0f m, nodes: %zu; steepest %.1f %%, moved by up to %.2f m. Ctrl+Z takes it back"), profile.runs.back(),
		                     heights.size(), steepest * 100.0, moved);
	if (profile.order.size() < editor_road::chain(*road).size())
		tool.status += std::string{". "} + STR_C("The road forks: only the run from its end to the fork was evened out");
}

bool editor_mode::road_profile_commit(road_node *Road, std::vector<double> const &Heights, std::vector<double> const &Grades)
{
	auto profile{build_profile(Road)};
	if (profile.empty() || Heights.size() != profile.runs.size() || Grades.size() != profile.runs.size())
		return false;
	std::vector<std::pair<road_node *, road_node::state>> changes;
	std::vector<std::pair<junction_node *, junction_node::state>> junctions;
	write_profile(profile, Heights, Grades, changes, junctions);
	if (false == road_points_apply(changes, junctions))
		return false;
	road_profile_sample();
	// the points selected in the view go up or down with the nodes
	if (std::any_of(m_road_profile.selected.begin(), m_road_profile.selected.end(), [](char const Selected) { return Selected != 0; }))
		road_profile_select_points();
	return true;
}

void editor_mode::road_profile_operation()
{
	auto &view{m_road_profile};
	auto &tool{m_roadtool};
	auto profile{build_profile(view.road)};
	if (profile.empty() || view.selected.size() != profile.runs.size())
		return;
	std::size_t first{profile.runs.size()}, last{0}, count{0};
	for (std::size_t i = 0; i < view.selected.size(); ++i)
		if (view.selected[i] != 0)
		{
			first = std::min(first, i);
			last = std::max(last, i);
			++count;
		}
	if (count == 0)
	{
		tool.status = STR_C("Select the nodes in the graph first");
		return;
	}
	auto heights{profile.heights};
	auto grades{profile.grades};
	auto const &runs{profile.runs};
	std::string done;
	switch (view.op)
	{
	case 0: // level, the selected nodes only
		for (std::size_t i = first; i <= last; ++i)
			if (view.selected[i] != 0)
			{
				heights[i] = view.level;
				grades[i] = 0.0;
			}
		done = format(STR_C("Level at %.2f m over %.0f m"), view.level, runs[last] - runs[first]);
		break;
	case 1: // one grade
	{
		if (view.grade_ends && last == first)
		{
			tool.status = STR_C("Select two nodes at least: the grade runs from the first to the last");
			return;
		}
		auto const grade{view.grade_ends ? (heights[last] - heights[first]) / std::max(1e-6, runs[last] - runs[first]) : view.grade / 100.0};
		for (std::size_t i = first; i <= last; ++i)
		{
			heights[i] = profile.heights[first] + grade * (runs[i] - runs[first]);
			grades[i] = grade;
		}
		done = format(STR_C("One grade of %.2f %% over %.0f m"), grade * 100.0, runs[last] - runs[first]);
		break;
	}
	case 2: // a vertical curve between the grade coming in and the one going out
	{
		if (last == first)
		{
			tool.status = STR_C("Select two nodes at least: the curve goes between the first and the last");
			return;
		}
		auto const g1{first > 0 ? (heights[first] - heights[first - 1]) / std::max(1e-6, runs[first] - runs[first - 1]) : grades[first]};
		auto const g2{last + 1 < heights.size() ? (heights[last + 1] - heights[last]) / std::max(1e-6, runs[last + 1] - runs[last]) : grades[last]};
		if (std::abs(g2 - g1) < 1e-5)
		{
			for (std::size_t i = first; i <= last; ++i)
			{
				heights[i] = heights[first] + g1 * (runs[i] - runs[first]);
				grades[i] = g1;
			}
			done = STR_C("The grades on both sides are the same: a straight grade between them");
			break;
		}
		// where the two grades meet
		auto const s_first{runs[first]}, s_last{runs[last]};
		auto const meet{(heights[last] - heights[first] + g1 * s_first - g2 * s_last) / (g1 - g2)};
		if (meet <= s_first || meet >= s_last)
		{
			tool.status = STR_C("The grades before and after the selected nodes don't meet between them: select more of the road, or set a grade first");
			return;
		}
		auto const change{std::abs(g2 - g1)};
		auto length{static_cast<double>(view.radius) * change};
		length = std::min(length, 2.0 * std::min(meet - s_first, s_last - meet));
		auto const start{meet - length * 0.5};
		auto const height_on_first = [&](double const S) { return heights[first] + g1 * (S - s_first); };
		auto const height_on_last = [&](double const S) { return heights[last] + g2 * (S - s_last); };
		for (std::size_t i = first; i <= last; ++i)
		{
			auto const s{runs[i]};
			if (s <= start)
			{
				heights[i] = height_on_first(s);
				grades[i] = g1;
			}
			else if (s >= start + length)
			{
				heights[i] = height_on_last(s);
				grades[i] = g2;
			}
			else
			{
				auto const x{s - start};
				heights[i] = height_on_first(s) + (g2 - g1) / (2.0 * length) * x * x;
				grades[i] = g1 + (g2 - g1) * x / length;
			}
		}
		done = format(g2 < g1 ? STR_C("A crest curve of R %.0f m, %.0f m long, between %.2f %% and %.2f %%") : STR_C("A dip curve of R %.0f m, %.0f m long, between %.2f %% and %.2f %%"),
		              length / change, length, g1 * 100.0, g2 * 100.0);
		if (length / change + 1.0 < view.radius)
			done += std::string{". "} + STR_C("No room for the radius set: the curve is as long as the selection lets it be");
		break;
	}
	case 3: // over the ground
	{
		std::size_t found{0};
		for (std::size_t i = first; i <= last; ++i)
		{
			double ground{0.0};
			if (view.selected[i] != 0 && m_streamer.height_at(profile.positions[i].x, profile.positions[i].z, ground, true))
			{
				heights[i] = ground + view.offset;
				++found;
			}
		}
		for (std::size_t i = first; i <= last; ++i)
			if (view.selected[i] != 0)
				grades[i] = neighbour_grade(profile, heights, i);
		done = format(STR_C("Nodes put %.2f m over the ground: %zu"), view.offset, found);
		break;
	}
	case 4: // moved up or down
		for (std::size_t i = 0; i < heights.size(); ++i)
			if (view.selected[i] != 0)
				heights[i] += view.shift;
		done = format(STR_C("Selected nodes moved by %+.2f m"), view.shift);
		break;
	case 5: // evened out between the first and the last
	{
		if (last == first)
		{
			tool.status = STR_C("Select two nodes at least: what's between them is evened out");
			return;
		}
		auto const grade{std::clamp(static_cast<double>(tool.maxgrade), 0.1, 30.0) / 100.0};
		smooth_between(profile, first, last, grade, std::max(50.0, static_cast<double>(tool.crest)), std::max(50.0, static_cast<double>(tool.sag)), heights, grades);
		if (first == 0 && end_joined(profile, 0))
			grades.front() = profile.grades.front();
		if (last + 1 == grades.size() && end_joined(profile, grades.size() - 1))
			grades.back() = profile.grades.back();
		done = format(STR_C("Evened out over %.0f m: grade up to %.1f %%, crests R %.0f m, dips R %.0f m"), runs[last] - runs[first], tool.maxgrade, tool.crest, tool.sag);
		break;
	}
	default:
		return;
	}
	if (road_profile_commit(view.road, heights, grades))
		tool.status = done + ". " + STR_C("Ctrl+Z takes it back");
}

namespace
{

// the formations of tracks (the modifiers of the terrain not made by roads) which may reach the box
std::vector<heightmap::modifier const *> formations_near(std::vector<heightmap::modifier> const &Modifiers, glm::dvec2 const &Low, glm::dvec2 const &High)
{
	std::vector<heightmap::modifier const *> near_ones;
	for (auto const &modifier : Modifiers)
	{
		if (modifier.name.starts_with("road ") || modifier.points.size() < 2)
			continue;
		auto const [low, high]{modifier.bounds()};
		if (low.x <= High.x && high.x >= Low.x && low.y <= High.y && high.y >= Low.y)
			near_ones.push_back(&modifier);
	}
	return near_ones;
}

// whether the point of a road lies under the embankment of a track: within its formation and the slopes going down from it
// to the height of the road, the formation a metre or more over the road. There the ground is the track's: the road gets
// through it in a structure, not in a cutting
bool under_formation(std::vector<heightmap::modifier const *> const &Formations, glm::dvec3 const &Point)
{
	glm::dvec2 const place{Point.x, Point.z};
	for (auto const *modifier : Formations)
		for (std::size_t i = 0; i + 1 < modifier->points.size(); ++i)
		{
			auto const &from{modifier->points[i]};
			auto const &to{modifier->points[i + 1]};
			auto const leg{to.position - from.position};
			auto const length{glm::dot(leg, leg)};
			auto const share{length > 1e-9 ? std::clamp(glm::dot(place - from.position, leg) / length, 0.0, 1.0) : 0.0};
			auto const formation{glm::mix(from.formation, to.formation, share)};
			auto const over{formation - Point.y};
			if (over < 1.0)
				continue;
			auto const reach{glm::mix(from.half_width, to.half_width, share) + over * std::max(0.5, modifier->slope)};
			if (glm::distance(place, from.position + leg * share) <= reach)
				return true;
		}
	return false;
}

} // namespace

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
	std::size_t done_pieces{0};
	std::size_t under_tracks{0}; // stretches left alone under the embankments of tracks
	auto const bench{std::clamp(static_cast<double>(tool.terrain_beside), 0.0, 20.0)};
	for (auto const *road : pieces)
	{
		editor_busy_progress(0.3 * static_cast<double>(done_pieces++) / static_cast<double>(pieces.size()));
		auto const &state{road->definition()};
		heightmap::modifier modifier;
		modifier.name = name_of(road);
		modifier.slope = std::clamp(static_cast<double>(tool.bankgrade), 0.25, 10.0);
		modifier.reach = 40.0;
		modifier.rounding = 1.0;
		modifier.natural = m_natural;
		// past the roadway: the kerbs and what lies beside it, then the bench of flat earth set, so the grade line of the ground
		// is wider than the road and its edges don't hang over the slope
		double built{0.0};
		for (int side = 0; side < 2; ++side)
		{
			auto const &data{state.sides[side]};
			auto const extra{(state.kerbs[side] ? state.kerbwidth : 0.f) + (data.type != road_node::side_type::none ? data.width : 0.f)};
			built = std::max(built, static_cast<double>(extra));
		}
		auto const beside{built + bench};
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
		glm::dvec2 low{state.point(0.0).x, state.point(0.0).z};
		auto high{low};
		std::vector<glm::dvec2> left;
		std::vector<glm::dvec2> right;
		for (int i = 0; i <= steps; ++i)
		{
			auto const t{i / static_cast<double>(steps)};
			auto const point{state.point(t)};
			low = glm::min(low, glm::dvec2{point.x, point.z});
			high = glm::max(high, glm::dvec2{point.x, point.z});
			auto const tangent{state.tangent(t)};
			if (glm::dvec2 across{tangent.z, -tangent.x}; glm::length(across) > 1e-9)
			{
				across = glm::normalize(across) * (0.5 * state.span(t) + built + 0.25);
				left.push_back(glm::dvec2{point.x, point.z} + across);
				right.push_back(glm::dvec2{point.x, point.z} - across);
			}
		}
		// the grass of the terrain goes from under the surface and what's built beside it
		if (tool.terrain_grass && left.size() >= 2)
		{
			auto outline{left};
			outline.insert(outline.end(), right.rbegin(), right.rend());
			m_streamer.grass(outline, 0, true);
		}
		auto const formations{formations_near(m_streamer.modifiers(), low - glm::dvec2{80.0}, high + glm::dvec2{80.0})};
		auto beneath{false};
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
			// under the embankment of a track the ground stays the track's, also between the faces of a structure built there:
			// the cutting of the road ends square at the foot of the embankment
			if (under_formation(formations, point))
			{
				if (false == beneath)
					++under_tracks;
				beneath = true;
				flush(true);
				continue;
			}
			beneath = false;
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
	if (under_tracks > 0)
		tool.status += "\n" + format(STR_C("Left alone under the embankments of tracks at %zu places: the road gets through there in a structure (below)"), under_tracks);
}

namespace
{

// a road crossing the formation of a track: where, the ways of both, the formation and its slopes there
struct track_crossing
{
	glm::dvec3 at; // on the axis of the road
	glm::dvec2 road; // its way seen from above, unit
	double grade{0.0}; // of the road along that way
	glm::dvec2 track; // the way of the track, unit
	double formation{0.0}; // height of the formation of the track there
	double half_width{3.5}; // of the formation
	double slope{1.5}; // run of its slopes for a metre of height
	double width{7.0}; // of the road with what's beside it
	std::string name; // of the modifier of the track
};

glm::dvec2 flat2(glm::dvec3 const &V)
{
	return {V.x, V.z};
}

double cross2(glm::dvec2 const &A, glm::dvec2 const &B)
{
	return A.x * B.y - A.y * B.x;
}

std::vector<track_crossing> crossings_of(std::vector<road_node *> const &Pieces, std::vector<heightmap::modifier> const &Modifiers)
{
	std::vector<track_crossing> result;
	for (auto const *road : Pieces)
	{
		auto const &state{road->definition()};
		double beside{0.5};
		for (int side = 0; side < 2; ++side)
		{
			auto const &data{state.sides[side]};
			auto const extra{(state.kerbs[side] ? state.kerbwidth : 0.f) + (data.type != road_node::side_type::none ? data.width : 0.f)};
			beside = std::max(beside, 0.5 + static_cast<double>(extra));
		}
		int constexpr steps{64};
		for (int i = 0; i < steps; ++i)
		{
			auto const t0{i / static_cast<double>(steps)}, t1{(i + 1) / static_cast<double>(steps)};
			auto const a{state.point(t0)}, b{state.point(t1)};
			auto const ra{flat2(a)}, rb{flat2(b)};
			auto const rd{rb - ra};
			if (glm::length(rd) < 1e-6)
				continue;
			for (auto const &modifier : Modifiers)
			{
				if (modifier.name.rfind("road ", 0) == 0 || modifier.points.size() < 2)
					continue;
				for (std::size_t k = 0; k + 1 < modifier.points.size(); ++k)
				{
					auto const &p{modifier.points[k]};
					auto const &q{modifier.points[k + 1]};
					auto const td{q.position - p.position};
					auto const denominator{cross2(rd, td)};
					if (std::abs(denominator) < 1e-9)
						continue;
					auto const u{cross2(p.position - ra, td) / denominator}; // along the road
					auto const v{cross2(p.position - ra, rd) / denominator}; // along the track
					if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
						continue;
					track_crossing crossing;
					crossing.at = glm::mix(a, b, u);
					crossing.road = glm::normalize(rd);
					crossing.grade = (b.y - a.y) / glm::length(rd);
					crossing.track = glm::normalize(td);
					crossing.formation = p.formation + (q.formation - p.formation) * v;
					crossing.half_width = p.half_width + (q.half_width - p.half_width) * v;
					crossing.slope = std::max(0.5, modifier.slope);
					crossing.width = state.span(glm::mix(t0, t1, u)) + 2.0 * beside;
					crossing.name = modifier.name;
					// the same crossing seen from two pieces, or two segments meeting at it
					if (std::none_of(result.begin(), result.end(), [&](track_crossing const &Other) { return glm::distance(flat2(Other.at), flat2(crossing.at)) < 5.0; }))
						result.push_back(crossing);
				}
			}
		}
	}
	return result;
}

} // namespace

void editor_mode::road_structures(bool const Remove)
{
	auto &tool{m_roadtool};
	if (tool.selected == nullptr)
		return;
	auto const pieces{editor_road::chain(*tool.selected)};
	auto const name_of = [](road_node const *Road) { return "road " + (Road->name().empty() ? std::to_string(reinterpret_cast<std::uintptr_t>(Road)) : Road->name()); };
	std::string clean{name_of(tool.selected)};
	for (auto &c : clean)
		if (false == std::isalnum(static_cast<unsigned char>(c)))
			c = '_';
	auto const prefix{"obiekt " + clean + "_"};
	std::string const folder{"models/kabina_obiekty/"};
	terrain_history_step();
	// the terrain led to the road goes first: the ground looked at is the embankment of the track, not the cutting the road
	// made through it; it's led again at the end, stopping at the structures
	bool led{false};
	{
		std::vector<std::string> old;
		for (auto const *road : pieces)
			for (auto const &modifier : m_streamer.modifiers())
				if (modifier.name == name_of(road) || modifier.name.rfind(name_of(road) + " #", 0) == 0)
					old.push_back(modifier.name);
		if (false == old.empty())
		{
			m_streamer.remove_modifiers(old);
			led = true;
		}
	}
	// the tunnels of the earlier versions of the editor (cut where the ground was over the road, "tunel road ..."): their models
	// go, and the holes along the road are closed
	{
		std::vector<TAnimModel *> old;
		for (auto *model : simulation::Instances.sequence())
			if (model != nullptr && model->name().rfind("tunel " + name_of(tool.selected).substr(0, 5), 0) == 0)
				for (auto const *road : pieces)
					if (model->name().rfind("tunel " + name_of(road) + " ", 0) == 0)
					{
						old.push_back(model);
						break;
					}
		if (false == old.empty())
		{
			for (auto *model : old)
			{
				if (m_node == model)
				{
					m_node = nullptr;
					ui()->set_node(nullptr);
				}
				erase_model(model);
			}
			for (auto const *road : pieces)
			{
				auto const &state{road->definition()};
				int constexpr steps{64};
				std::vector<glm::dvec2> left, right;
				for (int i = 0; i <= steps; ++i)
				{
					auto const t{i / static_cast<double>(steps)};
					auto const point{state.point(t)};
					auto const tangent{state.tangent(t)};
					glm::dvec2 across{tangent.z, -tangent.x};
					if (glm::length(across) < 1e-9)
						continue;
					across = glm::normalize(across) * (0.5 * state.span(t) + 6.0);
					left.push_back(glm::dvec2{point.x, point.z} + across);
					right.push_back(glm::dvec2{point.x, point.z} - across);
				}
				std::vector<glm::dvec2> outline{left};
				outline.insert(outline.end(), right.rbegin(), right.rend());
				if (outline.size() >= 3)
					m_streamer.cut(outline, false);
			}
		}
	}
	// the structures of before go, the holes of them closed
	std::size_t removed{0};
	{
		std::vector<TAnimModel *> old;
		for (auto *model : simulation::Instances.sequence())
			if (model != nullptr && model->name().rfind(prefix, 0) == 0)
				old.push_back(model);
		for (auto *model : old)
		{
			auto const base{model->name().substr(std::string{"obiekt "}.size())};
			std::ifstream holes{folder + base + ".dziura"};
			std::string line;
			while (std::getline(holes, line))
			{
				std::istringstream values{line};
				std::vector<glm::dvec2> outline;
				double x, z;
				while (values >> x >> z)
					outline.emplace_back(x, z);
				if (outline.size() >= 3)
					m_streamer.cut(outline, false);
			}
			if (m_node == model)
			{
				m_node = nullptr;
				ui()->set_node(nullptr);
			}
			erase_model(model);
			++removed;
		}
	}
	if (Remove)
	{
		terrain_history_step();
		forget_ground();
		if (led)
		{
			auto const every{tool.every_road};
			tool.every_road = false;
			road_terrain(false);
			tool.every_road = every;
		}
		tool.status = format(STR_C("Structures under the tracks taken away: %zu, their holes closed. Ctrl+Z brings the terrain back"), removed);
		return;
	}

	auto const crossings{crossings_of(pieces, m_streamer.modifiers())};
	crossing::options options;
	options.type = static_cast<crossing::kind>(std::clamp(tool.structure_type, 0, static_cast<int>(crossing::kind::count) - 1));
	options.clearance = tool.structure_clearance;
	options.width = tool.structure_width;
	options.spans = tool.structure_spans;
	options.fit = tool.structure_fit;
	options.parapet = tool.structure_parapet;
	options.tile = tool.structure_tile;
	options.material = tool.structure_material;
	options.steel = tool.structure_steel;
	std::vector<std::string> lowered;
	std::vector<std::string> refused;
	std::size_t built{0};
	tool.structure_note.clear();
	// the ground raised over the structures is a touch-up over the formations of the tracks, kept over them
	auto const touchup{m_streamer.touch_up()};
	m_streamer.touch_up(true);
	for (std::size_t number = 0; number < crossings.size(); ++number)
	{
		editor_busy_progress(static_cast<double>(number) / static_cast<double>(crossings.size()));
		auto const &c{crossings[number]};
		crossing::site site;
		site.at = c.at;
		site.road = c.road;
		site.grade = c.grade;
		site.track = c.track;
		site.formation = c.formation;
		site.half_width = c.half_width;
		site.slope = c.slope;
		site.width = c.width;
		site.ground = [this](glm::dvec2 const &P, double &Height) { return m_streamer.height_at(P.x, P.y, Height, true); };
		auto made{crossing::build(site, options)};
		if (false == made.built)
		{
			if (made.refused == crossing::refusal::along_track)
				refused.push_back(format(STR_C("%s: the road runs along the track there (under 20 degrees)"), c.name.c_str()));
			else
				refused.push_back(format(STR_C("%s: not built. The formation is %.1f m over the road and this kind takes %.2f m of it under the formation, which leaves %.2f m of clearance; "
				                               "lower the road under the track by %.2f m, or choose a bridge with the track between its girders (it takes the least)"),
				                         c.name.c_str(), c.formation - c.at.y, c.formation - c.at.y - made.room, made.room, made.short_by));
			continue;
		}
		if (made.lowered)
			lowered.push_back(format(STR_C("%s: built with a clearance of %.2f m, the formation is too low over the road for %.2f m"), c.name.c_str(), made.clearance, options.clearance));
		// written with the outlines of its holes beside it, so it can be taken away again
		auto const stamp{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()};
		auto const base{clean + "_" + std::to_string(number) + "_" + std::to_string(stamp)};
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		if (false == made.mesh.write(folder + base + ".t3d", c.at))
		{
			refused.push_back(format(STR_C("%s: the model couldn't be written in %s"), c.name.c_str(), folder.c_str()));
			continue;
		}
		{
			std::ofstream file{folder + base + ".dziura"};
			file.precision(3);
			file << std::fixed;
			for (auto const &outline : made.holes)
			{
				for (auto const &p : outline)
					file << p.x << ' ' << p.y << ' ';
				file << '\n';
			}
		}
		for (auto const &outline : made.holes)
			m_streamer.cut(outline, true);
		for (auto const &area : made.raise)
			raise_terrain(area.outline, area.height);
		simulation::State.create_model("node 3000 0 none model 0 0 0 0 kabina_obiekty/" + base + ".t3d none endmodel", "obiekt " + base, c.at);
		++built;
	}
	m_streamer.touch_up(touchup);
	if (false == refused.empty())
	{
		for (auto const &reason : refused)
			tool.structure_note += (tool.structure_note.empty() ? "" : "\n") + reason;
	}
	terrain_history_step();
	forget_ground();
	if (led)
	{
		// only this road led again, stopping at the holes before the faces
		auto const every{tool.every_road};
		tool.every_road = false;
		road_terrain(false);
		tool.every_road = every;
	}
	if (crossings.empty())
		tool.status = STR_C("The road crosses no formation of a track (no modifier of the terrain along a track under it)");
	else
		tool.status = format(STR_C("Structures under the tracks: %zu built of %zu crossings. Ctrl+Z brings the terrain back, \"Take them away\" the models too"), built,
		                     crossings.size());
	for (auto const &reason : lowered)
		tool.status += "\n" + reason;
	if (false == refused.empty())
		tool.status += "\n" + format(STR_P("%zu crossing not built, the reason is over the button", "%zu crossings not built, the reasons are over the button", refused.size()), refused.size());
}

void editor_mode::raise_terrain(std::vector<glm::dvec2> const &Outline, double const Height)
{
	if (Outline.size() < 3)
		return;
	glm::dvec2 low{Outline.front()};
	glm::dvec2 high{Outline.front()};
	for (auto const &p : Outline)
	{
		low = glm::min(low, p);
		high = glm::max(high, p);
	}
	auto const inside = [&](double const X, double const Z) {
		bool in{false};
		for (std::size_t i = 0, j = Outline.size() - 1; i < Outline.size(); j = i++)
		{
			auto const &a{Outline[i]};
			auto const &b{Outline[j]};
			if ((a.y > Z) != (b.y > Z) && X < (b.x - a.x) * (Z - a.y) / (b.y - a.y) + a.x)
				in = !in;
		}
		return in;
	};
	auto const target{static_cast<float>(Height)};
	m_streamer.reshape(low, high, [&](double const X, double const Z, float &Ground) {
		if (Ground >= target || false == inside(X, Z))
			return false;
		Ground = target;
		return true;
	});
}

void editor_mode::structure_textures_search()
{
	auto &tool{m_roadtool};
	tool.structure_textures.clear();
	char const *const words[]{"beton", "concrete", "cegl", "cegł", "brick", "kamien", "kamień", "stone", "mur", "wall", "wiadukt", "most", "tunel", "przyczol"};
	std::error_code error;
	for (std::filesystem::recursive_directory_iterator it{"textures", std::filesystem::directory_options::skip_permission_denied, error}, end; it != end; it.increment(error))
	{
		if (error)
			break;
		if (false == it->is_regular_file(error))
			continue;
		auto const extension{it->path().extension().string()};
		if (extension != ".dds" && extension != ".png" && extension != ".jpg" && extension != ".tga" && extension != ".ktx" && extension != ".mat")
			continue;
		auto name{it->path().stem().string()};
		std::string lower{name};
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		if (std::none_of(std::begin(words), std::end(words), [&](char const *Word) { return lower.find(Word) != std::string::npos; }))
			continue;
		auto relative{std::filesystem::relative(it->path(), "textures", error).replace_extension().generic_string()};
		if (std::find(tool.structure_textures.begin(), tool.structure_textures.end(), relative) == tool.structure_textures.end())
			tool.structure_textures.push_back(relative);
		if (tool.structure_textures.size() >= 400)
			break;
	}
	std::sort(tool.structure_textures.begin(), tool.structure_textures.end());
	tool.status = format(STR_C("Textures of walls found: %zu"), tool.structure_textures.size());
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
	view.history = m_history.size();
	view.history_changes = m_history_changes;
	auto const profile{build_profile(view.road)};
	if (profile.empty())
	{
		view.node_runs.clear();
		view.node_heights.clear();
		view.node_positions.clear();
		view.selected.clear();
		return;
	}
	// the selection kept where the nodes are the same ones
	if (profile.positions.size() != view.node_positions.size())
		view.selected.assign(profile.positions.size(), 0);
	view.node_runs.assign(profile.runs.begin(), profile.runs.end());
	view.node_heights.assign(profile.heights.begin(), profile.heights.end());
	view.node_positions = profile.positions;
	std::vector<double> runs, heights;
	std::vector<glm::dvec3> points;
	sample_between(profile, 0, profile.runs.size() - 1, runs, heights, &points);
	for (std::size_t i = 0; i < runs.size(); ++i)
	{
		view.runs.push_back(static_cast<float>(runs[i]));
		view.heights.push_back(static_cast<float>(heights[i]));
	}
	// the ground under the road, every few metres of it
	view.ground_runs.clear();
	double last{-1e9};
	for (std::size_t i = 0; i < points.size(); ++i)
	{
		if (runs[i] - last < 3.0 && i + 1 < points.size())
			continue;
		last = runs[i];
		double ground{0.0};
		view.ground_runs.push_back(static_cast<float>(runs[i]));
		view.ground.push_back(m_streamer.height_at(points[i].x, points[i].z, ground, true) ? static_cast<float>(ground) : std::numeric_limits<float>::quiet_NaN());
	}
}

// the selection of the nodes in the graph given to the road tool as its selected points, so the view shows them too
void editor_mode::road_profile_select_points()
{
	auto &view{m_road_profile};
	auto &tool{m_roadtool};
	tool.points.clear();
	for (std::size_t i = 0; i < view.selected.size() && i < view.node_positions.size(); ++i)
		if (view.selected[i] != 0)
			tool.points.push_back(view.node_positions[i]);
	if (false == tool.points.empty())
	{
		tool.selected = nullptr;
		tool.junction = nullptr;
		tool.marker = nullptr;
	}
	view.synced = tool.points;
}

void editor_mode::render_road_profile_window()
{
	auto &view{m_road_profile};
	if (false == view.open)
		return;
	auto &tool{m_roadtool};
	// the road gone (deleted, or another scenery): the window waits for another one
	if (view.road != nullptr)
	{
		bool alive{false};
		for (auto const *road : simulation::Roads.sequence())
			if (road == view.road && false == road->m_editorremoved)
				alive = true;
		if (false == alive)
		{
			view.road = nullptr;
			road_profile_sample();
		}
	}
	// the window follows the road selected: another piece clicked, the profile of its road read
	if (tool.selected != nullptr && tool.selected != view.road)
	{
		view.road = tool.selected;
		view.selected.clear();
		view.node_positions.clear();
		road_profile_sample();
	}
	// undone or done again: read anew
	if (view.road != nullptr && (view.history != m_history.size() || view.history_changes != m_history_changes))
		road_profile_sample();
	// points selected in the view: the nodes of the graph with them
	if (tool.points != view.synced)
	{
		view.synced = tool.points;
		for (std::size_t i = 0; i < view.selected.size(); ++i)
			view.selected[i] = std::any_of(tool.points.begin(), tool.points.end(), [&](glm::dvec3 const &Point) { return glm::distance(Point, view.node_positions[i]) < 0.3; }) ? 1 : 0;
		// a point of another road: the window goes to it
		if (false == tool.points.empty() && std::none_of(view.selected.begin(), view.selected.end(), [](char const Selected) { return Selected != 0; }))
		{
			auto const ends{editor_road::ends_at(tool.points.front())};
			if (false == ends.empty() && ends.front().road != view.road)
			{
				view.road = ends.front().road;
				view.node_positions.clear();
				road_profile_sample();
				for (std::size_t i = 0; i < view.selected.size(); ++i)
					view.selected[i] = std::any_of(tool.points.begin(), tool.points.end(), [&](glm::dvec3 const &Point) { return glm::distance(Point, view.node_positions[i]) < 0.3; }) ? 1 : 0;
			}
		}
	}
	ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (false == ImGui::Begin(STR_C("Vertical profile of the road###roadprofile"), &view.open))
	{
		ImGui::End();
		return;
	}
	if (view.road == nullptr || view.node_runs.size() < 2)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.24f, 1.0f));
		ImGui::TextWrapped("%s", STR_C("Select a road (Roads, Select, LMB on a piece) and its profile shows here"));
		ImGui::PopStyleColor();
		ImGui::End();
		return;
	}
	ImGui::TextUnformatted(view.road->name().c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton(STR_C("Read again##roadprofile")))
		road_profile_sample();
	ImGui::SameLine();
	char const *const exaggerations[]{STR_C("fit to the window"), "1 : 1", "1 : 2", "1 : 5", "1 : 10", "1 : 20", "1 : 50"};
	int const factors[]{0, 1, 2, 5, 10, 20, 50};
	ImGui::SetNextItemWidth(150.0f);
	ImGui::Combo(STR_C("Heights stretched##roadprofile"), &view.exaggeration, exaggerations, IM_ARRAYSIZE(exaggerations));
	item_tooltip("How much more the heights are drawn than the lengths: fit to the window shows every little change as a hill;\n1:10 is what the profiles of roads are usually drawn at");

	// the graph
	auto const size{ImVec2(ImGui::GetContentRegionAvail().x, 230.0f)};
	auto const origin{ImGui::GetCursorScreenPos()};
	ImGui::InvisibleButton("##roadprofilegraph", size);
	auto const graph_hovered{ImGui::IsItemHovered()};
	auto *draw{ImGui::GetWindowDrawList()};
	draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(25, 28, 32, 255));
	auto const length{std::max(1.0f, view.node_runs.back())};
	float low{std::numeric_limits<float>::max()}, high{-low};
	for (auto const h : view.heights)
	{
		low = std::min(low, h);
		high = std::max(high, h);
	}
	for (auto const h : view.ground)
		if (std::isfinite(h))
		{
			low = std::min(low, h);
			high = std::max(high, h);
		}
	auto const factor{factors[std::clamp(view.exaggeration, 0, 6)]};
	if (factor == 0)
	{
		high = std::max(high, low + 2.0f);
		auto const margin{(high - low) * 0.08f};
		low -= margin;
		high += margin;
	}
	else
	{
		// metres for a pixel across, and that over the factor up
		auto const span{length / size.x * size.y / static_cast<float>(factor)};
		auto const middle{(low + high) * 0.5f};
		low = middle - span * 0.5f;
		high = middle + span * 0.5f;
	}
	auto const at = [&](float const Run, float const Height) {
		return ImVec2(origin.x + Run / length * size.x, origin.y + size.y - (Height - low) / (high - low) * size.y);
	};
	auto const metres_per_pixel{(high - low) / size.y};
	draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
	// the grid: heights every round number of metres, the run every round number of metres
	auto const round_step = [](float const Range, float const Lines) {
		float const steps[]{0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f};
		for (auto const step : steps)
			if (Range / step <= Lines)
				return step;
		return 10000.0f;
	};
	auto const ystep{round_step(high - low, 6.0f)};
	auto const first_line{static_cast<int>(std::ceil(low / ystep))};
	auto const last_line{static_cast<int>(std::floor(high / ystep))};
	for (int line = first_line; line <= last_line; ++line)
	{
		auto const y{static_cast<float>(line) * ystep};
		auto const p{at(0.0f, y)};
		draw->AddLine(ImVec2(origin.x, p.y), ImVec2(origin.x + size.x, p.y), IM_COL32(255, 255, 255, 22));
		auto const label{format("%.*f m", ystep < 1.0f ? 1 : 0, y)};
		draw->AddText(ImVec2(origin.x + 4.0f, p.y - ImGui::GetTextLineHeight()), IM_COL32(200, 200, 200, 140), label.c_str());
	}
	auto const xstep{round_step(length, 8.0f)};
	for (int line = 1; static_cast<float>(line) * xstep < length; ++line)
	{
		auto const x{static_cast<float>(line) * xstep};
		auto const p{at(x, low)};
		draw->AddLine(ImVec2(p.x, origin.y), ImVec2(p.x, origin.y + size.y), IM_COL32(255, 255, 255, 18));
		auto const label{format("%.0f", x)};
		draw->AddText(ImVec2(p.x + 3.0f, origin.y + size.y - ImGui::GetTextLineHeight() - 2.0f), IM_COL32(200, 200, 200, 120), label.c_str());
	}
	// the ground brown, the road grey
	for (std::size_t i = 1; i < view.ground.size(); ++i)
		if (std::isfinite(view.ground[i - 1]) && std::isfinite(view.ground[i]))
			draw->AddLine(at(view.ground_runs[i - 1], view.ground[i - 1]), at(view.ground_runs[i], view.ground[i]), IM_COL32(170, 120, 70, 255), 2.0f);
	for (std::size_t i = 1; i < view.runs.size(); ++i)
		draw->AddLine(at(view.runs[i - 1], view.heights[i - 1]), at(view.runs[i], view.heights[i]), IM_COL32(220, 220, 225, 255), 2.0f);

	// the nodes: picked, dragged up or down, boxed
	auto const &io{ImGui::GetIO()};
	auto const mouse{io.MousePos};
	int hovered_node{-1};
	{
		float best{8.0f * 8.0f};
		for (std::size_t i = 0; i < view.node_runs.size(); ++i)
		{
			auto const p{at(view.node_runs[i], view.node_heights[i])};
			auto const d{(p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y)};
			if (d < best)
			{
				best = d;
				hovered_node = static_cast<int>(i);
			}
		}
	}
	if (view.selected.size() != view.node_runs.size())
		view.selected.assign(view.node_runs.size(), 0);
	auto const drag_offset = [&]() {
		auto offset{(view.drag_mouse - mouse.y) * view.drag_scale};
		if (io.KeyCtrl)
			offset *= 0.1f;
		return std::round(offset * 100.0f) / 100.0f;
	};
	if (graph_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (hovered_node >= 0)
		{
			auto &chosen{view.selected[hovered_node]};
			if (io.KeyShift)
				chosen = chosen != 0 ? 0 : 1;
			else if (chosen == 0)
			{
				std::fill(view.selected.begin(), view.selected.end(), 0);
				chosen = 1;
			}
			if (chosen != 0)
			{
				view.dragging = true;
				view.drag_mouse = mouse.y;
				view.drag_scale = metres_per_pixel;
			}
			road_profile_select_points();
		}
		else
		{
			view.boxing = true;
			view.box_from = mouse;
		}
	}
	if (view.dragging && false == ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		view.dragging = false;
		auto const offset{drag_offset()};
		auto profile{build_profile(view.road)};
		if (std::abs(offset) >= 0.005f && profile.runs.size() == view.selected.size())
		{
			auto heights{profile.heights};
			auto grades{profile.grades};
			for (std::size_t i = 0; i < heights.size(); ++i)
				if (view.selected[i] != 0)
					heights[i] += offset;
			for (std::size_t i = 0; i < heights.size(); ++i)
				if (view.selected[i] != 0)
					grades[i] = neighbour_grade(profile, heights, i);
			if (road_profile_commit(view.road, heights, grades))
			{
				tool.status = format(STR_C("Nodes moved by %+.2f m. Ctrl+Z takes it back"), offset);
				road_profile_select_points();
			}
		}
	}
	if (view.boxing)
	{
		auto const a{ImVec2(std::min(view.box_from.x, mouse.x), std::min(view.box_from.y, mouse.y))};
		auto const b{ImVec2(std::max(view.box_from.x, mouse.x), std::max(view.box_from.y, mouse.y))};
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			draw->AddRectFilled(a, b, IM_COL32(40, 220, 255, 30));
			draw->AddRect(a, b, IM_COL32(40, 220, 255, 160));
		}
		else
		{
			view.boxing = false;
			auto const tiny{b.x - a.x < 3.0f && b.y - a.y < 3.0f};
			if (false == io.KeyShift)
				std::fill(view.selected.begin(), view.selected.end(), 0);
			if (false == tiny)
				for (std::size_t i = 0; i < view.node_runs.size(); ++i)
				{
					auto const p{at(view.node_runs[i], view.node_heights[i])};
					// across the run is enough: a box over a stretch takes all its nodes, whatever their heights
					if (p.x >= a.x && p.x <= b.x)
						view.selected[i] = 1;
				}
			road_profile_select_points();
		}
	}
	auto const preview{view.dragging ? drag_offset() : 0.0f};
	for (std::size_t i = 0; i < view.node_runs.size(); ++i)
	{
		auto const chosen{view.selected[i] != 0};
		auto const p{at(view.node_runs[i], view.node_heights[i] + (chosen ? preview : 0.0f))};
		if (chosen && view.dragging)
			draw->AddLine(at(view.node_runs[i], view.node_heights[i]), p, IM_COL32(255, 210, 60, 200), 1.5f);
		draw->AddCircleFilled(p, chosen ? 5.5f : 4.0f, chosen ? IM_COL32(40, 220, 255, 255) : IM_COL32(255, 255, 255, 230));
		if (static_cast<int>(i) == hovered_node)
			draw->AddCircle(p, 8.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
	}
	draw->PopClipRect();
	if (view.dragging)
		ImGui::SetTooltip(STR_C("%+.2f m"), preview);
	else if (graph_hovered && view.runs.size() >= 2)
	{
		auto const run{(mouse.x - origin.x) / size.x * length};
		auto const it{std::lower_bound(view.runs.begin(), view.runs.end(), run)};
		auto const i{static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(it - view.runs.begin(), 1, static_cast<std::ptrdiff_t>(view.runs.size()) - 1))};
		auto const grade{(view.heights[i] - view.heights[i - 1]) / std::max(0.01f, view.runs[i] - view.runs[i - 1]) * 100.0f};
		auto const g{std::lower_bound(view.ground_runs.begin(), view.ground_runs.end(), run)};
		auto const gi{static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(g - view.ground_runs.begin(), 0, static_cast<std::ptrdiff_t>(view.ground.size()) - 1))};
		draw->AddLine(ImVec2(mouse.x, origin.y), ImVec2(mouse.x, origin.y + size.y), IM_COL32(255, 255, 255, 60));
		if (hovered_node >= 0)
			ImGui::SetTooltip(STR_C("Node %d: %.0f m, height %.2f m"), hovered_node + 1, view.node_runs[hovered_node], view.node_heights[hovered_node]);
		else
			ImGui::SetTooltip(STR_C("%.0f m: road %.2f m, ground %.2f m, grade %.1f %%"), view.runs[i], view.heights[i], view.ground.empty() ? 0.0f : view.ground[gi], grade);
	}
	// the hints, yellow as everywhere
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.24f, 1.0f));
	ImGui::TextWrapped("%s", STR_C("LMB on a node: select it   Shift+LMB: add or take out   drag a node up or down: its height (Ctrl: finer)   drag over the graph: select a stretch   the selected nodes show in the view too; Alt+LMB on a point there drags its height"));
	ImGui::PopStyleColor();
	ImGui::TextDisabled(STR_C("%.0f m long, nodes: %zu, heights %.1f - %.1f m; the road grey, the ground brown"), length, view.node_runs.size(), low, high);

	// what's done to the selected nodes
	std::size_t count{0};
	std::size_t first{view.selected.size()}, last{0};
	float sum{0.0f};
	for (std::size_t i = 0; i < view.selected.size(); ++i)
		if (view.selected[i] != 0)
		{
			++count;
			first = std::min(first, i);
			last = std::max(last, i);
			sum += view.node_heights[i];
		}
	ImGui::SeparatorText(STR_C("Selected nodes##roadprofile"));
	if (ImGui::SmallButton(STR_C("Select all##roadprofile")))
	{
		std::fill(view.selected.begin(), view.selected.end(), 1);
		road_profile_select_points();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(STR_C("Select none##roadprofile")))
	{
		std::fill(view.selected.begin(), view.selected.end(), 0);
		road_profile_select_points();
	}
	ImGui::SameLine();
	if (count == 0)
		ImGui::TextDisabled("%s", STR_C("none: click the nodes in the graph or the points in the view"));
	else
		ImGui::Text(STR_C("%zu, from %.0f m to %.0f m"), count, view.node_runs[first], view.node_runs[last]);
	char const *const operations[]{STR_C("Level##roadop"), STR_C("One grade"), STR_C("Vertical curve"), STR_C("Over the ground"), STR_C("Up or down by"), STR_C("Even out")};
	ImGui::SetNextItemWidth(200.0f);
	ImGui::Combo(STR_C("What to make of them##roadprofile"), &view.op, operations, IM_ARRAYSIZE(operations));
	switch (view.op)
	{
	case 0:
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("Height (m)##roadlevel"), &view.level, 0.1f, 1.0f, "%.2f");
		ImGui::SameLine();
		if (ImGui::SmallButton(STR_C("their average")) && count > 0)
			view.level = std::round(sum / static_cast<float>(count) * 100.0f) / 100.0f;
		item_tooltip("All the selected nodes at this height, with no grade at them");
		break;
	case 1:
		ImGui::Checkbox(STR_C("From the first selected node to the last##roadgrade"), &view.grade_ends);
		if (false == view.grade_ends)
		{
			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputFloat(STR_C("Grade (%)##roadgrade"), &view.grade, 0.1f, 1.0f, "%.2f");
			item_tooltip("From the first selected node on, up along the road where it's more than 0");
		}
		break;
	case 2:
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("Radius (m)##roadcurve"), &view.radius, 100.0f, 1000.0f, "%.0f");
		view.radius = std::clamp(view.radius, 20.0f, 100000.0f);
		item_tooltip("A curve rounding off where the grade before the selected nodes meets the one after them:\n"
		             "the grades are read from the nodes just outside the selection, so select a stretch with some of the road left on both sides");
		break;
	case 3:
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("Over the ground by (m)##roadground"), &view.offset, 0.05f, 0.5f, "%.2f");
		item_tooltip("Each selected node put this much over the ground under it; less than 0 puts it into a cutting");
		break;
	case 4:
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("By (m)##roadshift"), &view.shift, 0.1f, 1.0f, "%+.2f");
		item_tooltip("Less than 0 lowers them: a road into a cutting, then lead the terrain to it");
		break;
	case 5:
		ImGui::TextDisabled("%s", STR_C("By the grade and the radii of the class of the road below; the first and the last selected node stay"));
		break;
	default:
		break;
	}
	ImGui::BeginDisabled(count == 0);
	if (change_button(STR_C("Apply to the selected nodes##roadprofile")))
		road_profile_operation();
	ImGui::EndDisabled();

	ImGui::SeparatorText(STR_C("The whole road##roadprofile"));
	{
		std::vector<char const *> classes;
		for (auto const &entry : kRoadClasses)
			classes.push_back(STR_C(entry.name));
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::Combo(STR_C("Class of the road##roadprofile"), &view.road_class, classes.data(), static_cast<int>(classes.size())))
		{
			auto const &entry{kRoadClasses[std::clamp(view.road_class, 0, static_cast<int>(classes.size()) - 1)]};
			tool.maxgrade = entry.grade;
			tool.crest = entry.crest;
			tool.sag = entry.sag;
			view.radius = entry.crest;
		}
		item_tooltip("Sets the steepest grade and the smallest radii of the vertical curves about as the rules have them for that class");
	}
	if (ImGui::TreeNode(STR_C("Grade and radii##roadprofile")))
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Steepest grade (%)##road"), &tool.maxgrade, 0.1f, 0.5f, 20.0f, "%.1f");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Crest radius (m)##road"), &tool.crest, 50.0f, 50.0f, 50000.0f, "%.0f");
		item_tooltip("The smallest radius of the vertical curve over a hilltop: the larger, the farther a driver sees over it");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Dip radius (m)##road"), &tool.sag, 50.0f, 50.0f, 50000.0f, "%.0f");
		ImGui::TreePop();
	}
	if (change_button(STR_C("Even out the whole road")))
		road_profile_smooth();
	item_tooltip("The heights of the whole road: grades no steeper and curves no sharper than set, as near the heights it had as that lets;\n"
	             "its two ends stay where they are, so it keeps meeting the junctions. Ctrl+Z takes it back");

	// the terrain and the tunnels work on the road shown: it's selected for them while they're done
	ImGui::SeparatorText(STR_C("Terrain##roadprofile"));
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::InputFloat(STR_C("Bank slope 1:n"), &tool.bankgrade, 0.25f, 0.5f, "%.2f"))
		tool.bankgrade = std::clamp(tool.bankgrade, 0.25f, 10.0f);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::InputFloat(STR_C("Flat ground past the road (m)##roadterrain"), &tool.terrain_beside, 0.25f, 1.0f, "%.2f"))
		tool.terrain_beside = std::clamp(tool.terrain_beside, 0.0f, 20.0f);
	item_tooltip(STR_C("The ground is levelled wider than the road by this much either side, then the slopes begin: the edges of the road don't hang over them"));
	ImGui::Checkbox(STR_C("Take the grass from under the road##roadterrain"), &tool.terrain_grass);
	item_tooltip(STR_C("The grass grown on the terrain is taken away under the surface of the road and what's built beside it, so it doesn't stand out of the asphalt"));
	ImGui::Checkbox(STR_C("All the roads of the scenery##roadterrain"), &tool.every_road);
	if (change_button(STR_C("Lead the terrain to the road")))
	{
		run_busy(STR_C("Leading the terrain to the road"), [this, road = view.road]() {
			auto const before{m_roadtool.selected};
			m_roadtool.selected = road;
			road_terrain(false);
			m_roadtool.selected = before;
			road_profile_sample();
		});
	}
	item_tooltip("The terrain made flat under the roadway and its sides, with slopes of the grade set for the banks (1:n) down or up to the ground around,\n"
	             "rough as the natural slopes of the track bed are set. It follows the road when it's led again. Ctrl+Z takes it back");
	ImGui::SameLine();
	if (change_button(STR_C("Let go##roadterrain")))
	{
		run_busy(STR_C("Letting the terrain go"), [this, road = view.road]() {
			auto const before{m_roadtool.selected};
			m_roadtool.selected = road;
			road_terrain(true);
			m_roadtool.selected = before;
			road_profile_sample();
		});
	}
	item_tooltip("The terrain no longer shaped by the road (of the selected one, or of all of them)");
	ImGui::SeparatorText(STR_C("Structure under the track##roadprofile"));
	{
		std::array<char const *, 8> const types{STR_C("Pipe of corrugated steel"),
		                                        STR_C("Box culvert (reinforced concrete)"),
		                                        STR_C("Masonry arch (stone, brick)"),
		                                        STR_C("Frame viaduct (reinforced concrete)"),
		                                        STR_C("Slab of encased steel beams on abutments"),
		                                        STR_C("Deck plate girders, the track over them"),
		                                        STR_C("Through plate girders, the track between them"),
		                                        STR_C("Through truss, the track between the trusses")};
		std::array<float, 8> const clearances{3.5f, 3.5f, 4.7f, 4.7f, 4.7f, 4.7f, 4.7f, 4.7f};
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::Combo(STR_C("Kind##structure"), &tool.structure_type, types.data(), static_cast<int>(types.size())))
			tool.structure_clearance = clearances[static_cast<std::size_t>(std::clamp(tool.structure_type, 0, 7))];
		{
			// how much of the height over the road the kind takes under the formation: the bridges with the track between the girders the least
			auto const span{tool.structure_width > 0.5f ? static_cast<double>(tool.structure_width) : 8.0};
			ImGui::TextDisabled(STR_C("Takes %.2f m under the formation over the clearance"), crossing::depth_under(static_cast<crossing::kind>(std::clamp(tool.structure_type, 0, 7)), span));
		}
		item_tooltip("Where the road crosses the formation of a track (its modifier of the terrain). Under the embankment (a pipe, a box culvert, an arch,\n"
		             "a frame, a slab): faces where the slopes come down to them, wings along the slopes, the ground over them raised to cover them.\n"
		             "Bridges (deck or through plate girders, a truss): the embankment cut away between the abutments, the deck on bearings.\n"
		             "Every edge of the cut ground lies inside a wall or a wing, the opening has a floor of its own");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("Clearance over the road (m)##structure"), &tool.structure_clearance, 0.1f, 0.5f, "%.2f");
		tool.structure_clearance = std::clamp(tool.structure_clearance, 1.5f, 12.0f);
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("Width of the opening (m)##structure"), &tool.structure_width, 0.5f, 1.0f, tool.structure_width > 0.5f ? "%.1f" : STR_C("as the road"));
		tool.structure_width = std::max(0.0f, tool.structure_width);
		item_tooltip("Between the walls; 0: the road with its shoulders and kerbs, half a metre more");
		ImGui::Checkbox(STR_C("As high as fits where the clearance doesn't##structure"), &tool.structure_fit);
		item_tooltip(STR_C("Where the formation of the track isn't high enough over the road for the clearance set, the structure is built with the clearance there's room for (2.5 m at least) and says so, instead of being refused"));
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderInt(STR_C("Spans##structure"), &tool.structure_spans, 1, 6);
		item_tooltip(STR_C("More than one: piers stand between the spans, the width set shared by the spans and the piers; set the width for what's crossed, a road and a river beside it. A span narrower than 2 m isn't made"));
		std::array<char const *, 4> const parapets{STR_C("As the kind has it##parapet"), STR_C("Nothing##parapet"), STR_C("Railing (steel)##parapet"), STR_C("Wall##parapet")};
		ImGui::SetNextItemWidth(160.0f);
		ImGui::Combo(STR_C("On top of the faces##structure"), &tool.structure_parapet, parapets.data(), static_cast<int>(parapets.size()));
		item_tooltip(STR_C("What stands over each face along the track: an arch has a wall of its masonry, a viaduct and a span of girders a railing of steel, a culvert nothing"));
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputFloat(STR_C("Texture repeats every (m)##structure"), &tool.structure_tile, 0.25f, 1.0f, "%.2f");
		tool.structure_tile = std::clamp(tool.structure_tile, 0.25f, 50.0f);
		item_tooltip(STR_C("Lower lays the texture denser on the walls, higher stretches it"));
		char material[256];
		std::snprintf(material, sizeof(material), "%s", tool.structure_material.c_str());
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::InputText(STR_C("Texture##structure"), material, sizeof(material)))
			tool.structure_material = material;
		ImGui::SameLine();
		if (ImGui::BeginCombo("##structuretextures", nullptr, ImGuiComboFlags_NoPreview))
		{
			if (tool.structure_textures.empty())
				ImGui::TextDisabled("%s", STR_C("Look for them first"));
			for (auto const &entry : tool.structure_textures)
				if (ImGui::Selectable(entry.c_str(), entry == tool.structure_material))
					tool.structure_material = entry;
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(STR_C("Look for textures##structure")))
			run_busy(STR_C("Looking for the textures of walls"), [this]() { structure_textures_search(); });
		item_tooltip("Textures under textures/ with concrete, brick, stone or wall in the name; one texture covers the whole structure");
		std::array<char, 256> steelwork{};
		std::snprintf(steelwork.data(), steelwork.size(), "%s", tool.structure_steel.c_str());
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::InputText(STR_C("Texture of the steel##structure"), steelwork.data(), steelwork.size()))
			tool.structure_steel = steelwork.data();
		item_tooltip(STR_C("Of the girders, the railings and the tube; empty: a plain dark colour with no texture"));
		if (change_button(STR_C("Build the structures under the tracks")))
			run_busy(STR_C("Building the structures under the tracks"), [this, road = view.road]() {
				auto const before{m_roadtool.selected};
				m_roadtool.selected = road;
				road_structures(false);
				m_roadtool.selected = before;
				road_profile_sample();
			});
		item_tooltip("At every crossing of this road with the formation of a track, where the road runs under it at the height set in the profile.\n"
		             "The structures of the road built before are taken away first. Ctrl+Z brings the terrain back, the button beside takes the models away");
		ImGui::SameLine();
		if (change_button(STR_C("Take them away##structure")))
			run_busy(STR_C("Taking the structures away"), [this, road = view.road]() {
				auto const before{m_roadtool.selected};
				m_roadtool.selected = road;
				road_structures(true);
				m_roadtool.selected = before;
				road_profile_sample();
			});
		item_tooltip("The structures of this road taken away and the holes before their faces closed");
		if (false == tool.structure_note.empty())
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", tool.structure_note.c_str());
	}
	if (false == tool.status.empty())
		ImGui::TextWrapped("%s", tool.status.c_str());
	ImGui::End();
}


