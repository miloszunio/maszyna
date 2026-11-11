/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the outline of the Fill tool and of the surfaces cut out (Features) not only of straight sides: each of the options can be on
// or off by itself. Rounded corners, a smooth curve through the points (Ctrl+click: a sharp corner), Bezier handles dragged out
// as a point is put, drawing by hand with the button held, a side along a track at a distance of its axis. The points clicked and
// their marks are kept with the surface, so it's outlined again as it was drawn, not as hundreds of points of its curve

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editorprojection.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"
#include "editor/editorParcel.hpp"
#include "rendering/renderer.h"
#include "imgui/ImGuizmo.h"
#include "world/Track.h"
#include "simulation/simulation.h"
#include "scene/scene.h"
#include "utilities/Globals.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <sstream>

namespace
{

double constexpr kOutlineStep{0.5}; // between the points of the curves made

glm::dvec2 plan2(glm::dvec3 const &Point)
{
	return {Point.x, Point.z};
}

// Douglas-Peucker on an open polyline
void simplify(std::vector<glm::dvec2> const &Points, double const Tolerance, std::size_t const From, std::size_t const To, std::vector<char> &Keep)
{
	if (To <= From + 1)
		return;
	auto const a{Points[From]};
	auto const b{Points[To]};
	auto const run{b - a};
	auto const length{glm::length(run)};
	double worst{0.0};
	std::size_t at{From};
	for (auto i = From + 1; i < To; ++i)
	{
		auto const distance{length > 1e-9 ? std::abs((Points[i].x - a.x) * run.y - (Points[i].y - a.y) * run.x) / length : glm::distance(Points[i], a)};
		if (distance > worst)
		{
			worst = distance;
			at = i;
		}
	}
	if (worst <= Tolerance)
		return;
	Keep[at] = 1;
	simplify(Points, Tolerance, From, at, Keep);
	simplify(Points, Tolerance, at, To, Keep);
}

glm::dvec2 cubic(glm::dvec2 const &P0, glm::dvec2 const &P1, glm::dvec2 const &P2, glm::dvec2 const &P3, double const T)
{
	auto const u{1.0 - T};
	return u * u * u * P0 + 3.0 * u * u * T * P1 + 3.0 * u * T * T * P2 + T * T * T * P3;
}

// the tracks near a place, each as points of its axis, for the sides along a track
struct track_axes
{
	std::vector<std::vector<glm::dvec2>> lines;

	track_axes(glm::dvec2 const &Low, glm::dvec2 const &High)
	{
		auto const centre{(Low + High) * 0.5};
		auto const radius{static_cast<float>(glm::length(High - Low) * 0.5 + 80.0)};
		std::vector<TTrack const *> seen;
		for (auto *section : simulation::Region->sections(glm::dvec3{centre.x, 0.0, centre.y}, radius))
			for (auto const &cell : section->m_cells)
				for (auto *track : cell.m_paths)
				{
					if (track == nullptr || track->m_editorremoved || track->m_paths.empty() || track->eType == tt_Cross)
						continue;
					if (std::find(seen.begin(), seen.end(), track) != seen.end())
						continue;
					seen.push_back(track);
					geometry::bezier const curve{track->m_paths.front()};
					auto const length{geometry::chord_length(track->m_paths.front())};
					auto const count{std::clamp(static_cast<int>(length / 2.0), 2, 200)};
					std::vector<glm::dvec2> line;
					for (int i = 0; i <= count; ++i)
						line.push_back(plan2(curve.point(static_cast<double>(i) / count)));
					lines.push_back(std::move(line));
				}
	}

	// the nearest point of an axis, the unit vector along it there; false with no track near
	bool nearest(glm::dvec2 const &Point, glm::dvec2 &On, glm::dvec2 &Along, double const Reach = 80.0) const
	{
		auto best{Reach};
		bool found{false};
		for (auto const &line : lines)
			for (std::size_t i = 0; i + 1 < line.size(); ++i)
			{
				auto const run{line[i + 1] - line[i]};
				auto const length2{glm::dot(run, run)};
				if (length2 < 1e-12)
					continue;
				auto const t{std::clamp(glm::dot(Point - line[i], run) / length2, 0.0, 1.0)};
				auto const at{line[i] + run * t};
				auto const distance{glm::distance(at, Point)};
				if (distance < best)
				{
					best = distance;
					On = at;
					Along = run / std::sqrt(length2);
					found = true;
				}
			}
		return found;
	}
};

} // namespace

std::vector<editor_mode::outline_mark> &editor_mode::outline_marks() const
{
	// kept with the points by their places: a point added, taken back or the outline cleared anywhere leaves the marks of the
	// points which are still there, and plain ones for the new
	auto &marks{m_outline_marks};
	if (marks.size() > m_paint_outline.size())
		marks.resize(m_paint_outline.size());
	for (std::size_t i = 0; i < m_paint_outline.size(); ++i)
	{
		if (i < marks.size() && glm::distance(marks[i].position, m_paint_outline[i]) < 1e-6)
			continue;
		if (i >= marks.size())
			marks.emplace_back();
		else
			marks[i] = {};
		marks[i].position = m_paint_outline[i];
	}
	return marks;
}

void editor_mode::add_outline_point(glm::dvec3 const &World)
{
	auto point{placement_on_ground(World)};
	auto &marks{outline_marks()};
	outline_mark mark;
	mark.sharp = Global.ctrlState;
	auto const &shape{m_outline_shape};
	if (shape.along)
	{
		// onto the line at the distance of the axis of the nearest track: as clicked, or as given
		track_axes const axes{plan2(point) - glm::dvec2{1.0}, plan2(point) + glm::dvec2{1.0}};
		glm::dvec2 on, along;
		if (axes.nearest(plan2(point), on, along))
		{
			glm::dvec2 const left{-along.y, along.x};
			auto const lateral{glm::dot(plan2(point) - on, left)};
			mark.offset = shape.along_from_click ? lateral : std::copysign(static_cast<double>(shape.along_offset), lateral);
			auto const placed{on + left * mark.offset};
			point.x = placed.x;
			point.z = placed.y;
			// the side from the point before along the track
			if (false == marks.empty())
				marks.back().follow = true;
			mark.on_track = true;
		}
	}
	m_paint_outline.push_back(point);
	mark.position = point;
	marks.push_back(mark);
	m_outline_pressed = true;
	m_outline_press_index = m_paint_outline.size() - 1;
	m_outline_stroke.clear();
	m_outline_stroke.push_back(plan2(point));
	m_outline_shaped_key.clear();
}

void editor_mode::outline_drag(bool const Held)
{
	if (false == m_outline_pressed)
		return;
	auto const &shape{m_outline_shape};
	if (false == Held)
	{
		m_outline_pressed = false;
		// the stroke drawn by hand: its points, simplified, after the one pressed
		if (shape.freehand && m_outline_stroke.size() > 2 && m_outline_press_index + 1 == m_paint_outline.size())
		{
			std::vector<char> keep(m_outline_stroke.size(), 0);
			keep.front() = keep.back() = 1;
			simplify(m_outline_stroke, std::max(0.05, static_cast<double>(shape.freehand_tolerance)), 0, m_outline_stroke.size() - 1, keep);
			auto &marks{outline_marks()};
			auto const height{m_paint_outline.back().y};
			for (std::size_t i = 1; i < m_outline_stroke.size(); ++i)
			{
				if (keep[i] == 0)
					continue;
				auto const point{placement_on_ground(glm::dvec3{m_outline_stroke[i].x, height, m_outline_stroke[i].y})};
				m_paint_outline.push_back(point);
				outline_mark mark;
				mark.position = point;
				mark.freehand = true;
				marks.push_back(mark);
			}
			if (false == marks.empty())
				marks[m_outline_press_index].freehand = true;
			m_outline_shaped_key.clear();
		}
		m_outline_stroke.clear();
		return;
	}
	glm::dvec3 world;
	if (false == cursor_on_ground(world))
		return;
	auto &marks{outline_marks()};
	if (m_outline_press_index >= marks.size())
		return;
	auto const pressed{plan2(m_paint_outline[m_outline_press_index])};
	if (shape.freehand)
	{
		if (glm::distance(m_outline_stroke.back(), plan2(world)) >= std::max(0.2, static_cast<double>(shape.freehand_step)))
			m_outline_stroke.push_back(plan2(world));
		return;
	}
	if (shape.bezier)
	{
		// the handle out of the point the way it's dragged, the one into it opposite
		auto const handle{plan2(world) - pressed};
		auto const before{marks[m_outline_press_index].handle};
		marks[m_outline_press_index].handle = glm::length(handle) > 0.5 ? handle : glm::dvec2{0.0};
		if (marks[m_outline_press_index].handle != before)
			m_outline_shaped_key.clear();
	}
}

std::string editor_mode::outline_key() const
{
	std::ostringstream key;
	auto const &shape{m_outline_shape};
	key << shape.round << ' ' << shape.radius << ' ' << shape.spline << ' ' << shape.tension << ' ' << shape.bezier << ' ' << shape.along << ' ';
	auto const &marks{outline_marks()};
	for (std::size_t i = 0; i < m_paint_outline.size(); ++i)
		key << m_paint_outline[i].x << ',' << m_paint_outline[i].z << ',' << marks[i].sharp << marks[i].follow << marks[i].freehand << ',' << marks[i].handle.x << ','
		    << marks[i].handle.y << ',' << marks[i].offset << ';';
	return key.str();
}

std::vector<glm::dvec2> const &editor_mode::shaped_outline() const
{
	auto const key{outline_key()};
	if (key == m_outline_shaped_key)
		return m_outline_shaped;
	m_outline_shaped_key = key;
	auto &result{m_outline_shaped};
	result.clear();
	auto const count{m_paint_outline.size()};
	if (count < 3)
	{
		for (auto const &point : m_paint_outline)
			result.push_back(plan2(point));
		return result;
	}
	auto const &shape{m_outline_shape};
	auto const &marks{outline_marks()};
	std::vector<glm::dvec2> points;
	for (auto const &point : m_paint_outline)
		points.push_back(plan2(point));
	auto const at = [&](std::ptrdiff_t const Index) { return points[static_cast<std::size_t>((Index % static_cast<std::ptrdiff_t>(count) + count) % count)]; };
	auto const smooth = [&](std::size_t const Index) {
		// a curve through the point: the spline (unless the point is sharp), the points drawn by hand, or a handle of its own
		auto const &mark{marks[Index]};
		return glm::length(mark.handle) > 0.0 || (false == mark.sharp && (shape.spline || mark.freehand));
	};
	// the control of the curve at a point (to add going out of it, to take away coming into it): its handle, or the one of the
	// Catmull-Rom spline through the points beside it, loosened or tightened by the tension, never past half the nearer side
	auto const control = [&](std::size_t const Index) {
		auto const &mark{marks[Index]};
		if (shape.bezier && glm::length(mark.handle) > 0.0)
			return mark.handle;
		auto const tension{std::clamp(static_cast<double>(shape.tension), 0.0, 1.0)};
		auto const before{at(static_cast<std::ptrdiff_t>(Index) - 1)};
		auto const after{at(static_cast<std::ptrdiff_t>(Index) + 1)};
		auto vector{(after - before) * (1.0 - tension) / 6.0 * 2.0};
		auto const limit{0.5 * std::min(glm::distance(before, points[Index]), glm::distance(after, points[Index]))};
		auto const length{glm::length(vector)};
		if (length > limit && length > 1e-9)
			vector *= limit / length;
		return vector;
	};
	// the sides, each from its point up to (not with) the next one
	std::vector<std::vector<glm::dvec2>> sides(count);
	std::unique_ptr<track_axes> axes;
	for (std::size_t i = 0; i < count; ++i)
	{
		auto const a{points[i]};
		auto const b{at(static_cast<std::ptrdiff_t>(i) + 1)};
		auto &side{sides[i]};
		auto const next{(i + 1) % count};
		if (shape.along && marks[i].follow && marks[next].on_track)
		{
			// along the track: step by step on the line at the distance of the axis, from one offset to the other
			if (axes == nullptr)
			{
				glm::dvec2 low{std::numeric_limits<double>::max()};
				glm::dvec2 high{-low};
				for (auto const &point : points)
				{
					low = glm::min(low, point);
					high = glm::max(high, point);
				}
				axes = std::make_unique<track_axes>(low, high);
			}
			auto current{a};
			auto const total{std::max(1.0, glm::distance(a, b))};
			side.push_back(a);
			for (int step = 0; step < 20000; ++step)
			{
				glm::dvec2 on, along;
				if (false == axes->nearest(current, on, along) || glm::distance(current, b) < kOutlineStep * 2.0)
					break;
				auto const forward{glm::dot(b - current, along) >= 0.0 ? 1.0 : -1.0};
				auto const progress{std::clamp(1.0 - glm::distance(current, b) / total, 0.0, 1.0)};
				auto const offset{marks[i].offset + (marks[next].offset - marks[i].offset) * progress};
				glm::dvec2 on2, along2;
				auto const ahead{on + along * forward * kOutlineStep * 2.0};
				if (false == axes->nearest(ahead, on2, along2))
					break;
				auto const left{glm::dvec2{-along2.y, along2.x}};
				auto const moved{on2 + left * offset};
				if (glm::distance(moved, b) >= glm::distance(current, b))
					break; // past it, or the track turns away from it
				current = moved;
				side.push_back(current);
			}
			continue;
		}
		if ((shape.spline || shape.bezier || marks[i].freehand || marks[next].freehand) && (smooth(i) || smooth(next)))
		{
			auto const c1{a + (smooth(i) ? control(i) : glm::dvec2{0.0})};
			auto const c2{b - (smooth(next) ? control(next) : glm::dvec2{0.0})};
			auto const length{glm::distance(a, c1) + glm::distance(c1, c2) + glm::distance(c2, b)};
			auto const steps{std::clamp(static_cast<int>(length / kOutlineStep), 2, 4000)};
			for (int k = 0; k < steps; ++k)
				side.push_back(cubic(a, c1, c2, b, static_cast<double>(k) / steps));
			continue;
		}
		side.push_back(a);
	}
	// the corners rounded: where a sharp point is, the sides trimmed by the radius and joined with a curve tangent to both
	if (shape.round && shape.radius > 0.0f)
	{
		auto const radius{static_cast<double>(shape.radius)};
		auto const length_of = [](std::vector<glm::dvec2> const &Line, glm::dvec2 const &End) {
			double length{0.0};
			for (std::size_t k = 1; k < Line.size(); ++k)
				length += glm::distance(Line[k - 1], Line[k]);
			return length + (Line.empty() ? 0.0 : glm::distance(Line.back(), End));
		};
		std::vector<char> rounded(count, 0);
		for (std::size_t i = 0; i < count; ++i)
		{
			if (smooth(i))
				continue;
			auto const previous{(i + count - 1) % count};
			auto const in_length{length_of(sides[previous], points[i])};
			auto const out_length{length_of(sides[i], at(static_cast<std::ptrdiff_t>(i) + 1))};
			auto const cut{std::min({radius, in_length * 0.45, out_length * 0.45})};
			if (cut < 0.05)
				continue;
			rounded[i] = 1;
		}
		// walks a side from its start (or back from its end) by a distance
		auto const walk = [](std::vector<glm::dvec2> Line, double const Distance) {
			double left{Distance};
			for (std::size_t k = 1; k < Line.size(); ++k)
			{
				auto const part{glm::distance(Line[k - 1], Line[k])};
				if (part >= left)
					return std::make_pair(Line[k - 1] + (Line[k] - Line[k - 1]) * (left / std::max(part, 1e-9)), k);
				left -= part;
			}
			return std::make_pair(Line.back(), Line.size());
		};
		std::vector<glm::dvec2> joined;
		for (std::size_t i = 0; i < count; ++i)
		{
			auto const next{(i + 1) % count};
			auto line{sides[i]};
			line.push_back(points[next]);
			auto const total{length_of(sides[i], points[next])};
			std::size_t first{0};
			auto start{line.front()};
			if (rounded[i])
			{
				auto const previous{(i + count - 1) % count};
				auto const cut{std::min({radius, length_of(sides[previous], points[i]) * 0.45, total * 0.45})};
				auto const [point, index]{walk(line, cut)};
				start = point;
				first = index;
				// the corner: from the end of the side before, trimmed the same, through the point
				auto before{sides[previous]};
				before.push_back(points[i]);
				std::reverse(before.begin(), before.end());
				auto const back{walk(before, cut).first};
				for (int k = 0; k <= 12; ++k)
				{
					auto const t{k / 12.0};
					auto const u{1.0 - t};
					joined.push_back(u * u * back + 2.0 * u * t * points[i] + t * t * start);
				}
			}
			else
				joined.push_back(start);
			std::size_t last{line.size() - 1};
			auto end{line.back()};
			if (rounded[next])
			{
				auto const cut{std::min({radius, total * 0.45, length_of(sides[next], at(static_cast<std::ptrdiff_t>(next) + 1)) * 0.45})};
				auto reversed{line};
				std::reverse(reversed.begin(), reversed.end());
				auto const [point, index]{walk(reversed, cut)};
				end = point;
				last = line.size() - index;
			}
			for (auto k = std::max<std::size_t>(first, 1); k < last && k < line.size() - 1; ++k)
				joined.push_back(line[k]);
			if (false == rounded[next])
				continue; // the next side starts with the point
			joined.push_back(end);
		}
		result = std::move(joined);
	}
	else
		for (auto const &side : sides)
			result.insert(result.end(), side.begin(), side.end());
	// no two points on one place
	std::vector<glm::dvec2> clean;
	for (auto const &point : result)
		if (clean.empty() || glm::distance(clean.back(), point) > 0.05)
			clean.push_back(point);
	if (clean.size() > 2 && glm::distance(clean.front(), clean.back()) < 0.05)
		clean.pop_back();
	result = std::move(clean);
	return result;
}

std::string editor_mode::outline_controls() const
{
	// the points as drawn, for the surface to be outlined again: v1 then the options, then a point a group
	std::ostringstream text;
	text.precision(10);
	auto const &shape{m_outline_shape};
	text << "v1 " << shape.round << ' ' << shape.radius << ' ' << shape.spline << ' ' << shape.tension << ' ' << shape.bezier << ' ' << shape.along;
	auto const &marks{outline_marks()};
	for (std::size_t i = 0; i < m_paint_outline.size(); ++i)
		text << " | " << m_paint_outline[i].x << ' ' << m_paint_outline[i].y << ' ' << m_paint_outline[i].z << ' ' << marks[i].sharp << ' ' << marks[i].follow << ' ' << marks[i].freehand
		     << ' ' << marks[i].on_track << ' ' << marks[i].handle.x << ' ' << marks[i].handle.y << ' ' << marks[i].offset;
	return text.str();
}

bool editor_mode::restore_outline(std::string const &Controls)
{
	std::istringstream text{Controls};
	std::string version;
	if (false == static_cast<bool>(text >> version) || version != "v1")
		return false;
	auto &shape{m_outline_shape};
	if (false == static_cast<bool>(text >> shape.round >> shape.radius >> shape.spline >> shape.tension >> shape.bezier >> shape.along))
		return false;
	std::vector<glm::dvec3> points;
	std::vector<outline_mark> marks;
	std::string bar;
	while (text >> bar)
	{
		glm::dvec3 point;
		outline_mark mark;
		if (bar != "|" || false == static_cast<bool>(text >> point.x >> point.y >> point.z >> mark.sharp >> mark.follow >> mark.freehand >> mark.on_track >> mark.handle.x >> mark.handle.y >> mark.offset))
			break;
		mark.position = point;
		points.push_back(point);
		marks.push_back(mark);
	}
	if (points.size() < 3)
		return false;
	m_paint_outline = std::move(points);
	m_outline_marks = std::move(marks);
	m_outline_shaped_key.clear();
	return true;
}

void editor_mode::render_outline_options()
{
	render_wand_options();
	auto &shape{m_outline_shape};
	bool changed{false};
	changed |= ImGui::Checkbox(STR_C("Rounded corners##outline"), &shape.round);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("Each corner gets a curve: the sides trimmed by the radius and joined smoothly. With the smooth curve on, only the sharp points (Ctrl+click) are corners"));
	if (shape.round)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		changed |= ImGui::DragFloat(STR_C("radius (m)##outline"), &shape.radius, 0.1f, 0.0f, 200.0f, "%.1f");
	}
	changed |= ImGui::Checkbox(STR_C("Smooth curve through the points##outline"), &shape.spline);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("The sides are curves passing through the points clicked; Ctrl+click puts a sharp corner"));
	if (shape.spline)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		changed |= ImGui::SliderFloat(STR_C("tension##outline"), &shape.tension, 0.0f, 1.0f, "%.2f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("0: loose, round curves; 1: close to the straight sides"));
	}
	changed |= ImGui::Checkbox(STR_C("Bezier handles (drag as the point is put)##outline"), &shape.bezier);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("Press the button where the point goes and drag: the handles of the curve come out of it, the way it's dragged and the opposite one"));
	changed |= ImGui::Checkbox(STR_C("Drawing by hand (hold the button)##outline"), &shape.freehand);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("Hold the left button and draw over the ground or the orthophoto; the line is simplified and smoothed when it's let go"));
	if (shape.freehand)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		changed |= ImGui::DragFloat(STR_C("simplified to (m)##outline"), &shape.freehand_tolerance, 0.01f, 0.05f, 5.0f, "%.2f");
	}
	changed |= ImGui::Checkbox(STR_C("Sides along a track##outline"), &shape.along);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("The points go onto a line at a distance of the axis of the nearest track, and the side between two such points follows the track, its curves too"));
	if (shape.along)
	{
		ImGui::Indent();
		changed |= ImGui::Checkbox(STR_C("the distance as clicked##outline"), &shape.along_from_click);
		if (false == shape.along_from_click)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			changed |= ImGui::DragFloat(STR_C("from the axis (m)##outline"), &shape.along_offset, 0.1f, 0.0f, 200.0f, "%.1f");
		}
		ImGui::Unindent();
	}
	changed |= ImGui::Checkbox(STR_C("A cadastral parcel (click it)##outline"), &shape.parcel);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("A click on the ground takes the outline of the parcel there from the cadastre (ULDK of GUGiK), Shift+click adds the "
		                              "parcel next to it or takes one away; then its points are moved as any others. Needs the terrain set where it lies (PUWG 1992)"));
	if (shape.parcel)
	{
		ImGui::Indent();
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##parcelnumber", "021903_2.0003.176/5", m_parcel_number, sizeof(m_parcel_number));
		ImGui::SameLine();
		if (ImGui::Button(STR_C("Fetch by the number##parcel")) && m_parcel_number[0] != '\0')
		{
			std::string const number{m_parcel_number};
			auto const add{Global.shiftState};
			run_busy(STR_C("Fetching the parcel from the cadastre (ULDK)"), [this, number, add]() { parcel_fetch({}, number, add); });
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("The full number of the parcel: the district (TERYT), the precinct and the parcel; with Shift held it's added"));
		for (auto const &id : m_parcel_ids)
			ImGui::BulletText("%s", id.c_str());
		if (false == m_parcel_status.empty())
			ImGui::TextWrapped("%s", m_parcel_status.c_str());
		ImGui::Unindent();
	}
	if (m_paint_outline.size() >= 3)
	{
		ImGui::TextDisabled(STR_C("%zu points clicked, %zu of the outline made"), m_paint_outline.size(), shaped_outline().size());
		ImGui::TextColored(ImVec4(0.96f, 0.79f, 0.24f, 1.0f), "%s", STR_C("Drag a point to move it, Alt+click on a point takes it away, on a side puts one in"));
	}
	render_missing_ground();
	if (changed)
		m_outline_shaped_key.clear();
}

void editor_mode::parcel_fetch(glm::dvec3 const &World, std::string const &Number, bool const Add)
{
	auto const &manifest{m_streamer.manifest()};
	if (false == manifest.georeferenced)
	{
		m_parcel_status = STR_C("The terrain doesn't know where it lies (PUWG 1992): set its origin first");
		return;
	}
	auto const east0{manifest.origin_east};
	auto const north0{manifest.origin_north};
	parcel::found found;
	std::string error;
	auto const url{Number.empty() ? parcel::url_at(east0 - World.x, north0 + World.z) : parcel::url_of(Number)};
	if (false == parcel::fetch(url, found, error))
	{
		m_parcel_status = format(STR_C("No parcel: %s"), error.c_str());
		WriteLog("Editor: parcel " + url + ": " + error);
		return;
	}
	auto parcels{Add ? m_parcels : std::vector<std::vector<glm::dvec2>>{}};
	auto ids{Add ? m_parcel_ids : std::vector<std::string>{}};
	// the one taken already is taken away again
	if (auto const known{std::find(ids.begin(), ids.end(), found.id)}; known != ids.end())
	{
		parcels.erase(parcels.begin() + (known - ids.begin()));
		ids.erase(known);
	}
	else
	{
		parcels.push_back(found.ring);
		ids.push_back(found.id);
	}
	std::vector<glm::dvec2> ring;
	if (parcels.size() == 1)
		ring = parcels.front();
	else if (parcels.size() > 1 && false == parcel::merge(parcels, ring))
	{
		m_parcel_status = format(STR_C("%s doesn't touch the parcels taken before along a side: not added"), found.id.c_str());
		return;
	}
	m_parcels = std::move(parcels);
	m_parcel_ids = std::move(ids);
	m_paint_outline.clear();
	m_outline_marks.clear();
	// the terrain loaded for the corners where it isn't yet: one with no ground found took 0 and stood off the outline
	std::vector<bool> grounded;
	double sum{0.0};
	std::size_t found_heights{0};
	for (auto const &point : ring)
	{
		glm::dvec3 const at{east0 - point.x, 0.0, point.y - north0};
		double height{0.0};
		grounded.push_back(m_streamer.height_at(at.x, at.z, height, true));
		if (grounded.back())
		{
			sum += height;
			++found_heights;
		}
		m_paint_outline.emplace_back(at.x, height, at.z);
	}
	// where there's no terrain at all, the height of the others
	for (std::size_t i = 0; i < m_paint_outline.size() && found_heights > 0; ++i)
		if (false == grounded[i])
			m_paint_outline[i].y = sum / static_cast<double>(found_heights);
	// the corners of a parcel stay corners, whatever the curve
	for (auto &mark : outline_marks())
		mark.sharp = true;
	m_outline_shaped_key.clear();
	m_parcel_status = m_parcel_ids.empty() ? std::string{} : format(STR_C("%zu parcels, %zu points"), m_parcel_ids.size(), m_paint_outline.size());
	WriteLog("Editor: parcel " + found.id + ", " + std::to_string(found.ring.size()) + " points");
}

std::vector<heightmap::chunk_key> const &editor_mode::outline_missing_chunks()
{
	// a few times a second: the outline moves while dragged, the chunks come while made
	auto const now{ImGui::GetTime()};
	if (m_outline_missing_at >= 0.0 && now - m_outline_missing_at < 0.5)
		return m_outline_missing;
	m_outline_missing_at = now;
	m_outline_missing.clear();
	if (false == m_streamer.active() || m_paint_outline.size() < 3)
		return m_outline_missing;
	auto const &outline{shaped_outline()};
	if (outline.size() < 3)
		return m_outline_missing;
	glm::dvec2 low{std::numeric_limits<double>::max()}, high{-std::numeric_limits<double>::max()};
	for (auto const &point : outline)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	auto const first{heightmap::chunk_at(low.x, low.y)};
	auto const last{heightmap::chunk_at(high.x, high.y)};
	for (int cz = first.second; cz <= last.second; ++cz)
		for (int cx = first.first; cx <= last.first; ++cx)
		{
			heightmap::chunk_key const key{cx, cz};
			if (m_streamer.exists(key))
				continue;
			// the chunk counts where the outline takes some of it: a point of the outline in it, or a point of a grid of 25 m of it inside the outline
			auto const corner{heightmap::chunk_corner(key)};
			auto const in_square = [&](glm::dvec2 const &Point) {
				return Point.x >= corner.x && Point.x <= corner.x + heightmap::chunk_size && Point.y >= corner.y && Point.y <= corner.y + heightmap::chunk_size;
			};
			auto taken{std::any_of(outline.begin(), outline.end(), in_square)};
			for (double z = corner.y + 12.5; z < corner.y + heightmap::chunk_size && false == taken; z += 25.0)
				for (double x = corner.x + 12.5; x < corner.x + heightmap::chunk_size && false == taken; x += 25.0)
					taken = geometry::point_in_polygon(outline, glm::dvec2{x, z});
			if (taken)
				m_outline_missing.push_back(key);
		}
	return m_outline_missing;
}

void editor_mode::render_missing_ground()
{
	auto const &missing{outline_missing_chunks()};
	if (missing.empty())
		return;
	ImGui::TextColored(ImVec4(0.96f, 0.79f, 0.24f, 1.0f), "%s",
	                   format(STR_C("Part of the outline has no terrain under it: %zu chunks. A field there ends at the edge of the terrain"), missing.size()).c_str());
	auto const model{m_streamer.manifest().georeferenced};
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.48f, 0.08f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.58f, 0.10f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.66f, 0.12f, 1.0f));
	auto const add{ImGui::Button(STR_C("Add the missing chunks under the outline"), ImVec2(-FLT_MIN, 0.0f))};
	ImGui::PopStyleColor(3);
	item_tooltip(model ? STR_C("Made from the terrain model (NMT of GUGiK), as the chunks around the camera are; the fields over them go onto them as they come")
	                   : STR_C("Made from the ground of the scenery and the edges of the chunks beside them; the fields over them go onto them"));
	if (false == add)
		return;
	std::vector<heightmap::chunk_key> keys{missing.begin(), missing.end()};
	m_outline_missing_at = -1.0;
	if (model)
	{
		nmt_make(keys);
		m_parcel_status = m_nmt_state.status;
		return;
	}
	if (false == ensure_terrain())
		return;
	std::size_t made{0};
	for (auto const &key : keys)
		if (add_chunk_from_ground(key, m_terrain_spacing))
			++made;
	m_parcel_status = format(STR_C("%zu chunks made from the ground of the scenery"), made);
}

namespace
{
// how far on the screen from the cursor, in pixels; a large number if it isn't in front of the camera
double pixels_to(screen_projection const &Projection, glm::dvec3 const &Point)
{
	ImVec2 at;
	if (false == Projection.project(Point, at))
		return std::numeric_limits<double>::max();
	auto const mouse{ImGui::GetIO().MousePos};
	return std::hypot(at.x - mouse.x, at.y - mouse.y);
}
} // namespace

bool editor_mode::outline_point_press(glm::dvec3 const &World)
{
	if (m_paint_outline.empty())
		return false;
	screen_projection const projection;
	// the point nearest to the cursor on the screen
	int nearest{-1};
	double best{10.0};
	for (std::size_t i = 0; i < m_paint_outline.size(); ++i)
		if (auto const pixels{pixels_to(projection, outline_point_shown(i))}; pixels < best)
		{
			best = pixels;
			nearest = static_cast<int>(i);
		}
	auto &marks{outline_marks()};
	if (Global.altState)
	{
		if (nearest >= 0)
		{
			m_paint_outline.erase(m_paint_outline.begin() + nearest);
			marks.erase(marks.begin() + nearest);
			m_outline_shaped_key.clear();
			return true;
		}
		// a point put in the side nearest to the click
		if (m_paint_outline.size() < 2)
			return false;
		auto const click{plan2(World)};
		std::size_t side{0};
		auto closest{std::numeric_limits<double>::max()};
		for (std::size_t i = 0; i < m_paint_outline.size(); ++i)
		{
			auto const a{plan2(m_paint_outline[i])};
			auto const b{plan2(m_paint_outline[(i + 1) % m_paint_outline.size()])};
			auto const run{b - a};
			auto const run2{glm::dot(run, run)};
			auto const t{run2 > 1e-12 ? std::clamp(glm::dot(click - a, run) / run2, 0.0, 1.0) : 0.0};
			if (auto const distance{glm::distance(click, a + run * t)}; distance < closest)
			{
				closest = distance;
				side = i;
			}
		}
		auto const point{placement_on_ground(World)};
		m_paint_outline.insert(m_paint_outline.begin() + side + 1, point);
		outline_mark mark;
		mark.position = point;
		mark.sharp = marks[side].sharp;
		marks.insert(marks.begin() + side + 1, mark);
		m_outline_shaped_key.clear();
		return true;
	}
	if (nearest < 0)
		return false;
	m_outline_moving = nearest;
	return true;
}

void editor_mode::outline_edit_drag(bool const Held)
{
	if (false == Held)
	{
		m_outline_moving = -1;
		return;
	}
	glm::dvec3 world;
	if (m_outline_moving < 0 || false == cursor_on_ground(world))
		return;
	if (m_outline_moving >= static_cast<int>(m_paint_outline.size()))
	{
		m_outline_moving = -1;
		return;
	}
	auto &marks{outline_marks()};
	auto const point{placement_on_ground(world)};
	m_paint_outline[m_outline_moving] = point;
	marks[m_outline_moving].position = point; // its mark goes with it
	m_outline_shaped_key.clear();
}

bool editor_mode::rows_arrow(glm::dvec3 &Middle, glm::dvec3 &Tip) const
{
	if (m_terrain_tool != terrain_tool::fields || false == m_waves.on || m_waves.kind != terrain_waves::field)
		return false;
	auto const &outline{shaped_outline()};
	if (outline.size() < 3)
		return false;
	glm::dvec2 low{std::numeric_limits<double>::max()}, high{-std::numeric_limits<double>::max()};
	for (auto const &point : outline)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	auto const middle{(low + high) * 0.5};
	auto const angle{glm::radians(static_cast<double>(m_waves.auto_direction ? longest_side(outline) : m_waves.angle))};
	auto const length{std::max(8.0, glm::length(high - low) * 0.2)};
	auto const tip{middle + glm::dvec2{std::cos(angle), std::sin(angle)} * length};
	double height{0.0};
	m_streamer.height_at(middle.x, middle.y, height);
	Middle = {middle.x, height + 0.5, middle.y};
	Tip = {tip.x, height + 0.5, tip.y};
	return true;
}

bool editor_mode::rows_arrow_press()
{
	// the ring turns the rows by itself (render_rows_gizmo): a click on it puts no point and fetches no parcel
	glm::dvec3 middle, tip;
	return m_rows_gizmo_over && rows_arrow(middle, tip);
}

void editor_mode::render_rows_gizmo()
{
	glm::dvec3 middle, tip;
	if (false == rows_arrow(middle, tip))
	{
		m_rows_gizmo_over = false;
		return;
	}
	ImGuizmo::BeginFrame();
	ImGuizmo::SetOrthographic(Global.EditorOrtho);
	ImGuiIO const &io = ImGui::GetIO();
	ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
	// as the gizmo of the objects: the view camera-relative, the projection a plain one of the same view
	glm::mat4 const view{GfxRenderer->Camera_View_Matrix()};
	glm::mat4 const projection{editor_mode::projection_matrix(io.DisplaySize.y > 0.0f ? io.DisplaySize.x / io.DisplaySize.y : 1.0f)};
	auto const angle{glm::radians(static_cast<double>(m_waves.auto_direction ? longest_side(shaped_outline()) : m_waves.angle))};
	glm::dvec3 const direction{std::cos(angle), 0.0, std::sin(angle)};
	// the axes of the ring turned with the rows: its X along them
	glm::mat4 matrix{1.0f};
	matrix[0] = glm::vec4{glm::vec3{direction}, 0.0f};
	matrix[2] = glm::vec4{glm::vec3{glm::cross(direction, glm::dvec3{0.0, 1.0, 0.0})}, 0.0f}; // right-handed: Z = X x Y
	matrix[3] = glm::vec4{glm::vec3{middle - GfxRenderer->Camera_Position()}, 1.0f};
	glm::vec3 snap{5.0f};
	glm::mat4 delta{1.0f};
	ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), ImGuizmo::ROTATE_Y, ImGuizmo::WORLD, glm::value_ptr(matrix), glm::value_ptr(delta),
	                     Global.ctrlState ? glm::value_ptr(snap) : nullptr);
	m_rows_gizmo_over = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
	if (false == ImGuizmo::IsUsing())
		return;
	// the direction of the rows as the ring left it: the X axis of its matrix, whichever way the turn is counted
	glm::dvec3 const turned{matrix[0]};
	if (glm::length(glm::dvec2{turned.x, turned.z}) < 1e-6)
		return;
	// the rows run either way: kept within -90..90, as the longest side gives it
	m_waves.angle = static_cast<float>(std::remainder(glm::degrees(std::atan2(turned.z, turned.x)), 180.0));
	m_waves.auto_direction = false;
}

glm::dvec3 editor_mode::outline_point_shown(std::size_t const Index)
{
	// on the ground under it, as the line of the outline is drawn; its own height where there's no ground
	auto point{m_paint_outline[Index]};
	double height{0.0};
	if (m_streamer.height_at(point.x, point.z, height))
		point.y = height;
	return point + glm::dvec3{0.0, 0.15, 0.0};
}

void editor_mode::draw_rows_arrow(ImDrawList *Drawlist)
{
	glm::dvec3 middle, tip;
	if (false == rows_arrow(middle, tip))
		return;
	screen_projection const projection;
	auto const colour{IM_COL32(245, 200, 60, 255)};
	// the shaft both ways (the rows run both ways); it's turned with the ring of the gizmo
	auto const back{middle - (tip - middle)};
	projection.line(Drawlist, back, tip, colour, 3.0f);
	ImVec2 a, b;
	if (projection.project(middle, a) && projection.project(tip, b))
	{
		ImVec2 const along{b.x - a.x, b.y - a.y};
		auto const length{std::hypot(along.x, along.y)};
		if (length > 1.0f)
		{
			ImVec2 const unit{along.x / length, along.y / length};
			ImVec2 const side{-unit.y, unit.x};
			Drawlist->AddTriangleFilled(b, ImVec2(b.x - unit.x * 14.0f + side.x * 7.0f, b.y - unit.y * 14.0f + side.y * 7.0f),
			                            ImVec2(b.x - unit.x * 14.0f - side.x * 7.0f, b.y - unit.y * 14.0f - side.y * 7.0f), colour);
		}
		auto const angle{m_waves.auto_direction ? longest_side(shaped_outline()) : m_waves.angle};
		auto const label{format(STR_C("rows %.0f deg%s"), angle, m_waves.auto_direction ? STR_C(" (longest side)") : "")};
		Drawlist->AddText(ImVec2(b.x + 12.0f, b.y - 8.0f), colour, label.c_str());
	}
}

void editor_mode::draw_outline_shape(ImDrawList *Drawlist)
{
	screen_projection const Projection;
	if (m_paint_outline.empty())
		return;
	auto const &marks{outline_marks()};
	auto const &shaped{shaped_outline()};
	auto const ground = [&](glm::dvec2 const &Point, double const Fallback) {
		double height;
		return m_streamer.height_at(Point.x, Point.y, height) ? height : Fallback;
	};
	auto const fallback{m_paint_outline.back().y};
	// the outline as it'll be cut or filled
	if (shaped.size() >= 3)
		for (std::size_t i = 0; i < shaped.size(); ++i)
		{
			auto const &a{shaped[i]};
			auto const &b{shaped[(i + 1) % shaped.size()]};
			Projection.line(Drawlist, glm::dvec3{a.x, ground(a, fallback) + 0.15, a.y}, glm::dvec3{b.x, ground(b, fallback) + 0.15, b.y}, IM_COL32(250, 200, 90, 255), 2.0f);
		}
	else
		for (std::size_t i = 0; i + 1 < m_paint_outline.size(); ++i)
			Projection.line(Drawlist, m_paint_outline[i] + glm::dvec3{0.0, 0.15, 0.0}, m_paint_outline[i + 1] + glm::dvec3{0.0, 0.15, 0.0}, IM_COL32(250, 200, 90, 255), 2.0f);
	// the points clicked: round ones, sharp ones as squares, the handles
	for (std::size_t i = 0; i < m_paint_outline.size(); ++i)
	{
		ImVec2 dot;
		auto const point{outline_point_shown(i)};
		if (false == Projection.project(point, dot))
			continue;
		auto const colour{IM_COL32(250, 200, 90, 255)};
		if (marks[i].sharp)
			Drawlist->AddRectFilled(ImVec2(dot.x - 4.0f, dot.y - 4.0f), ImVec2(dot.x + 4.0f, dot.y + 4.0f), colour);
		else if (false == marks[i].freehand)
			Drawlist->AddCircleFilled(dot, 4.0f, colour);
		if (glm::length(marks[i].handle) > 0.0)
		{
			auto const out{point + glm::dvec3{marks[i].handle.x, 0.0, marks[i].handle.y}};
			auto const in{point - glm::dvec3{marks[i].handle.x, 0.0, marks[i].handle.y}};
			Projection.line(Drawlist, in, out, IM_COL32(120, 200, 255, 220), 1.5f);
			for (auto const &end : {in, out})
			{
				ImVec2 tip;
				if (Projection.project(end, tip))
					Drawlist->AddCircle(tip, 4.0f, IM_COL32(120, 200, 255, 255), 12, 1.5f);
			}
		}
	}
	// the stroke being drawn by hand
	if (m_outline_pressed && m_outline_shape.freehand)
		for (std::size_t i = 0; i + 1 < m_outline_stroke.size(); ++i)
			Projection.line(Drawlist, glm::dvec3{m_outline_stroke[i].x, ground(m_outline_stroke[i], fallback) + 0.2, m_outline_stroke[i].y},
			                glm::dvec3{m_outline_stroke[i + 1].x, ground(m_outline_stroke[i + 1], fallback) + 0.2, m_outline_stroke[i + 1].y}, IM_COL32(255, 255, 255, 230), 2.0f);
}
