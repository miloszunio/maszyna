/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorRailDesign.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace rail_design
{

namespace
{

double const kSlightBend{0.02 * std::numbers::pi / 180.0}; // a bend slighter than this is none: the tangents would be parallel
double const kSlowest{10.0}; // km/h
double const kKept{0.95}; // of the length asked for: a transition laid this long has its room
// the lengths of the transitions tried where the one of the norms has no room, as shares of it
std::array<double, 5> const kShares{1.0, 0.75, 0.5, 0.35, 0.25};

glm::dvec2 plan_of(glm::dvec3 const &Point)
{
	return {Point.x, Point.z};
}

double cross(glm::dvec2 const &Left, glm::dvec2 const &Right)
{
	return Left.x * Right.y - Left.y * Right.x;
}

// the bends which are none taken out: the points lying on the straight through their neighbours
void drop_slight(std::vector<glm::dvec2> &Bends)
{
	for (std::size_t i = 1; i + 1 < Bends.size();)
	{
		auto const in{Bends[i] - Bends[i - 1]};
		auto const out{Bends[i + 1] - Bends[i]};
		if (std::abs(std::atan2(cross(in, out), glm::dot(in, out))) < kSlightBend)
			Bends.erase(Bends.begin() + static_cast<std::ptrdiff_t>(i));
		else
			++i;
	}
}

alignment::design design_through(std::vector<glm::dvec2> const &Bends, double const Speed, alignment::limits const &Norms, alignment::transition_shape const Shape)
{
	alignment::design design;
	design.norms = Norms;
	design.norms.checked = true;
	design.shape = Shape;
	design.speed = Speed;
	auto const last{Bends.size() - 1};
	design.start = {Bends.front().x, 0.0, Bends.front().y};
	design.end = {Bends.back().x, 0.0, Bends.back().y};
	design.start_direction = glm::normalize(Bends[1] - Bends[0]);
	design.end_direction = glm::normalize(Bends[last] - Bends[last - 1]);
	for (std::size_t i = 1; i < last; ++i)
	{
		alignment::vertex bend;
		bend.position = Bends[i];
		design.vertices.push_back(bend);
	}
	if (false == design.vertices.empty())
	{
		// the first and the last bend are placed by how far they lie from the ends
		design.vertices.front().offset = glm::distance(Bends[1], Bends[0]);
		design.vertices.back().offset = glm::distance(Bends[last], Bends[last - 1]);
	}
	return design;
}

double sharpest_of(alignment::curve_report const &Curve)
{
	auto sharpest{Curve.radius};
	for (auto const radius : Curve.radii)
		sharpest = std::min(sharpest, radius);
	return sharpest;
}

// the speed the curve allows, in whole tens, no more than the one of the line
double allowed_speed(alignment::curve_report const &Curve, double const Speed, alignment::limits const &Norms)
{
	auto const limit{std::floor(alignment::speed_limit(sharpest_of(Curve), Norms) / 10.0) * 10.0};
	return std::max(kSlowest, std::min(Speed, limit));
}

// the cant and the transitions of the norms at the speed, the transitions a share of their length and the cant no more
// than a ramp that long takes
void set_curve(alignment::vertex &Vertex, double const Radius, double const Speed, double const Share, alignment::limits const &Norms)
{
	auto const recommended{alignment::recommend(Speed, Radius, Norms)};
	auto const length{recommended.transition * Share};
	auto const cant{std::min({recommended.cant, length * 1000.0 / (Norms.ramp_factor * Speed), Norms.cant_rate * 3.6 * length / Speed})};
	Vertex.cant = cant < 5.0 ? 0.0 : std::floor(cant / 5.0) * 5.0;
	Vertex.transition_in = length;
	Vertex.transition_out = length;
}

bool curve_of_arcs(alignment::design const &Design, alignment::curve_report const &Curve)
{
	return Curve.vertex >= 0 && Curve.vertex < static_cast<int>(Design.vertices.size()) && false == Design.vertices[static_cast<std::size_t>(Curve.vertex)].spirals;
}

// the design with the curves of Eased given the cants and the transitions of the norms, each at the share of its vertex
alignment::design with_norms(alignment::design const &Design, alignment::result const &Eased, std::vector<std::size_t> const &Shares, double const Speed)
{
	auto attempt{Design};
	for (auto const &curve : Eased.curves)
	{
		if (false == curve_of_arcs(Design, curve))
			continue;
		auto const vertex{static_cast<std::size_t>(curve.vertex)};
		set_curve(attempt.vertices[vertex], sharpest_of(curve), allowed_speed(curve, Speed, Design.norms), kShares[Shares[vertex]], Design.norms);
	}
	return attempt;
}

// the vertices whose transitions were laid shorter than asked for get the next share; false when none can get shorter
bool shorten_cramped(alignment::design const &Attempt, alignment::result const &Laid, std::vector<std::size_t> &Shares)
{
	auto changed{false};
	for (auto const &curve : Laid.curves)
	{
		if (false == curve_of_arcs(Attempt, curve))
			continue;
		auto const index{static_cast<std::size_t>(curve.vertex)};
		auto const &vertex{Attempt.vertices[index]};
		auto const cramped{curve.transition_in < kKept * vertex.transition_in || curve.transition_out < kKept * vertex.transition_out};
		if (cramped && Shares[index] + 1 < kShares.size())
		{
			++Shares[index];
			changed = true;
		}
	}
	return changed;
}

bool shorten_all(std::vector<std::size_t> &Shares)
{
	auto changed{false};
	for (auto &share : Shares)
		if (share + 1 < kShares.size())
		{
			++share;
			changed = true;
		}
	return changed;
}

// the curves of the line eased given what the norms ask for; where a transition has no room it's made shorter, by itself.
// The line as eased is left where the norms can't be given at all
void give_norms(plan &Plan, double const Speed)
{
	std::vector<std::size_t> shares(Plan.design.vertices.size(), 0);
	auto const eased{Plan.shape};
	for (std::size_t round = 0; round < 2 * kShares.size(); ++round)
	{
		auto const attempt{with_norms(Plan.design, eased, shares, Speed)};
		auto const laid{alignment::compute(attempt)};
		if (false == laid.valid)
		{
			if (false == shorten_all(shares))
				return;
			continue;
		}
		if (shorten_cramped(attempt, laid, shares))
			continue;
		Plan.design = attempt;
		Plan.shape = laid;
		Plan.shortened = static_cast<std::size_t>(std::ranges::count_if(shares, [](std::size_t const Share) { return Share > 0; }));
		return;
	}
}

// the curves slower than the line, each from the start of its first element to the end of its last
void find_limits(plan &Plan, double const Speed)
{
	for (auto const &curve : Plan.shape.curves)
	{
		auto const allowed{allowed_speed(curve, Speed, Plan.design.norms)};
		if (allowed >= Speed)
			continue;
		speed_limit limit{Plan.shape.length, 0.0, allowed};
		for (auto const &element : Plan.shape.elements)
			if (element.vertex == curve.vertex && element.kind != alignment::element_kind::straight)
			{
				limit.from = std::min(limit.from, element.chainage);
				limit.to = std::max(limit.to, element.chainage + element.length);
			}
		if (limit.to > limit.from)
			Plan.limits.push_back(limit);
	}
}

glm::dvec2 heading_of(segment_data const &Piece, bool const AtEnd)
{
	auto const control{plan_of(Piece.points[AtEnd ? segment_data::point::control2 : segment_data::point::control1])};
	auto const chord{plan_of(Piece.points[segment_data::point::end] - Piece.points[segment_data::point::start])};
	auto const along{glm::length(control) > 1e-6 ? (AtEnd ? -control : control) : chord};
	return glm::length(along) > 1e-9 ? glm::normalize(along) : glm::dvec2{0.0, 1.0};
}

// the curvature of the piece at an end, positive turning to the left
double curvature_of(segment_data const &Piece, bool const AtEnd)
{
	auto const start{plan_of(Piece.points[segment_data::point::start])};
	auto const end{plan_of(Piece.points[segment_data::point::end])};
	auto const first{start + plan_of(Piece.points[segment_data::point::control1])};
	auto const second{end + plan_of(Piece.points[segment_data::point::control2])};
	auto const leg{AtEnd ? end - second : first - start};
	auto const length{glm::length(leg)};
	if (length < 1e-6)
		return 0.0;
	return 2.0 / 3.0 * cross(AtEnd ? second - first : leg, AtEnd ? leg : second - first) / (length * length * length);
}

// where the pieces meet and their two ends, moved to the side: along the mean heading of the pieces meeting there
std::vector<glm::dvec3> joints_beside(std::vector<segment_data> const &Pieces, double const Offset)
{
	std::vector<glm::dvec3> joints;
	for (std::size_t i = 0; i <= Pieces.size(); ++i)
	{
		auto const first{i == 0};
		auto const last{i == Pieces.size()};
		auto const &point{first ? Pieces.front().points[segment_data::point::start] : Pieces[i - 1].points[segment_data::point::end]};
		auto heading{first ? heading_of(Pieces.front(), false) : last ? heading_of(Pieces.back(), true) : heading_of(Pieces[i - 1], true) + heading_of(Pieces[i], false)};
		heading = glm::length(heading) > 1e-9 ? glm::normalize(heading) : glm::dvec2{0.0, 1.0};
		glm::dvec2 const left{-heading.y, heading.x};
		joints.emplace_back(point.x + left.x * Offset, point.y, point.z + left.y * Offset);
	}
	return joints;
}

} // namespace

plan design_of(std::vector<glm::dvec2> Bends, double const Speed, alignment::limits const &Norms, alignment::transition_shape const Shape)
{
	plan made;
	drop_slight(Bends);
	if (Bends.size() < 2 || glm::distance(Bends.front(), Bends.back()) < 1.0)
	{
		made.error = "the stretch is too short";
		return made;
	}
	made.design = design_through(Bends, Speed, Norms, Shape);
	if (made.design.vertices.empty())
		made.shape = alignment::compute(made.design);
	else
	{
		std::vector<char> corners(made.design.vertices.size(), 1);
		made.shape = alignment::ease_corners(made.design, corners, {});
	}
	if (false == made.shape.valid)
	{
		made.error = made.shape.errors.empty() ? std::string{"no line through the bends"} : made.shape.errors.front();
		return made;
	}
	give_norms(made, Speed);
	find_limits(made, Speed);
	return made;
}

deviation deviation_of(alignment::result const &Shape, std::vector<glm::dvec2> const &Points)
{
	deviation result;
	double sum{0.0};
	std::size_t count{0};
	for (auto const &projection : alignment::project(Shape, Points))
	{
		if (projection.chainage < 0.0 || projection.chainage > Shape.length)
			continue; // past the ends: a point of the stretch before or after
		sum += projection.offset * projection.offset;
		result.worst = std::max(result.worst, std::abs(projection.offset));
		++count;
	}
	result.rms = count > 0 ? std::sqrt(sum / static_cast<double>(count)) : 0.0;
	return result;
}

std::vector<segment_data> pieces_of(plan const &Plan, double const Length)
{
	if (false == Plan.shape.valid)
		return {};
	auto const count{std::max(alignment::minimum_pieces(Plan.shape, Plan.design), static_cast<std::size_t>(std::ceil(Plan.shape.length / std::max(5.0, Length))))};
	return alignment::pieces(Plan.shape, Plan.design, count);
}

std::vector<double> speeds_of(plan const &Plan, std::vector<segment_data> const &Pieces, double const Speed)
{
	std::vector<glm::dvec2> middles;
	middles.reserve(Pieces.size());
	for (auto const &piece : Pieces)
		middles.push_back(0.5 * (plan_of(piece.points[segment_data::point::start]) + plan_of(piece.points[segment_data::point::end])));
	std::vector<double> speeds(Pieces.size(), Speed);
	auto const projections{alignment::project(Plan.shape, middles)};
	for (std::size_t i = 0; i < projections.size() && i < speeds.size(); ++i)
		for (auto const &limit : Plan.limits)
			if (projections[i].chainage >= limit.from && projections[i].chainage <= limit.to)
				speeds[i] = std::min(speeds[i], limit.speed);
	return speeds;
}

std::vector<segment_data> beside(std::vector<segment_data> const &Pieces, double const Offset)
{
	std::vector<segment_data> moved;
	if (Pieces.empty())
		return moved;
	auto const joints{joints_beside(Pieces, Offset)};
	for (std::size_t i = 0; i < Pieces.size(); ++i)
	{
		auto const &source{Pieces[i]};
		auto const startcurvature{curvature_of(source, false)};
		auto const endcurvature{curvature_of(source, true)};
		auto const startscale{1.0 - startcurvature * Offset};
		auto const endscale{1.0 - endcurvature * Offset};
		if (startscale < 0.05 || endscale < 0.05)
			return {};
		segment_data piece;
		piece.points[segment_data::point::start] = joints[i];
		piece.points[segment_data::point::end] = joints[i + 1];
		piece.points[segment_data::point::control1] = source.points[segment_data::point::control1] * startscale;
		piece.points[segment_data::point::control2] = source.points[segment_data::point::control2] * endscale;
		piece.rolls = source.rolls;
		auto const middle{0.5 * (startcurvature + endcurvature)};
		if (source.radius != 0.0f && std::abs(middle) > 1e-7)
		{
			auto const radius{static_cast<double>(source.radius)};
			piece.radius = static_cast<float>(std::copysign(std::abs(std::abs(radius) - std::copysign(Offset, middle)), radius));
		}
		moved.push_back(piece);
	}
	return moved;
}

} // namespace rail_design
