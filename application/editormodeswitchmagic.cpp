/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// "magic" of the switch tool: a switch dragged out towards a track it's to lead into, where the branch can't reach it as the track
// lies (too tight, the end of the track curved, short or pointing the wrong way). The track behind the diverging end of the switch is
// laid anew: taken back from its free end as far as it takes, and joined to the switch with a curve the switch allows (a straight, the
// curve of the radius of the switch, or a reverse curve), the rest of the track kept as it lies. The link keeps to ST-T1-A6 at the
// thresholds chosen (P1 unless asked otherwise): the least radius of the kind of track (tab. 9.2), the cant deficiency of its
// curves (tab. 9.7) and the sudden changes of cant deficiency where the curvature changes at a point (tab. 9.15), with the straight
// between reverse curves when they'd change too much at once (tab. 9.16). Where nothing fits those, P2 is tried and said so.
// The template armed is tried first, then the other switches for the rail from the steepest (1:9) on, as a tight place takes the
// shortest switch

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editormodeshared.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"
#include "editor/editorNorms.hpp"

#include "world/Track.h"
#include "simulation/simulation.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "application/editormodetrackshared.h"

using namespace track_mode;

namespace
{

double constexpr kMagicReach{400.0}; // m, how far back along the track the landing is looked for
double constexpr kMagicStep{10.0}; // m, the step it's looked for in, then narrowed down to kMagicFine
double constexpr kMagicFine{1.0};
std::size_t constexpr kMagicTemplates{4}; // the armed one and the steepest ones after it

// signed curvature of a piece in the plan, in the direction it's laid: + to the left
double piece_curvature(segment_data const &Piece)
{
	geometry::bezier const curve{Piece};
	auto const from{glm::dvec2{curve.first(0.0).x, curve.first(0.0).z}};
	auto const to{glm::dvec2{curve.first(1.0).x, curve.first(1.0).z}};
	auto const length{curve.plan_length()};
	if (glm::length(from) < 1e-9 || glm::length(to) < 1e-9 || length < 1e-6)
		return 0.0;
	auto const turn{signed_angle(glm::normalize(from), glm::normalize(to))};
	return std::abs(turn) < 1e-7 ? 0.0 : turn / length;
}

} // namespace

std::vector<editor_mode::curve_sample> editor_mode::magic_frame(editor_track::snap_target const &Target, editor_track::chain &Chain) const
{
	std::vector<curve_sample> frame;
	if (Target.track == nullptr || Target.track->eType != tt_Normal)
		return frame;
	std::string error;
	if (false == editor_track::find_run(*Target.track, kMagicReach + 50.0, Chain, error) || Chain.tracks.empty())
		return frame;
	frame = sample_chain(Chain);
	if (frame.size() < 2)
		return {};
	// from the free end on
	if (plan_distance(frame.back().position, Target.position) < plan_distance(frame.front().position, Target.position))
	{
		std::reverse(frame.begin(), frame.end());
		auto const total{frame.front().station};
		for (auto &sample : frame)
		{
			sample.station = total - sample.station;
			sample.tangent = -sample.tangent;
			sample.roll = -sample.roll;
		}
	}
	if (plan_distance(frame.front().position, Target.position) > 0.5)
		return {};
	return frame;
}

bool editor_mode::plan_magic_link(switch_link &Plan) const
{
	auto const &tool{m_switch};
	if (Plan.target.track == nullptr || tool.line.tracks.empty() || tool.armed < 0 || tool.armed >= static_cast<int>(tool.templates.size()))
		return false;
	// the same drag as the frame before: the same answer
	auto const &target{Plan.target};
	std::vector<double> const key{static_cast<double>(reinterpret_cast<std::uintptr_t>(target.track)), target.position.x, target.position.y, target.position.z, tool.line.start.x, tool.line.start.z,
	                              tool.line.length, std::round(tool.along * 2.0), static_cast<double>(Plan.forward), static_cast<double>(Plan.side), static_cast<double>(tool.armed), static_cast<double>(tool.templates.size()), static_cast<double>(tool.magic_level), static_cast<double>(tool.magic_kind),
	                              static_cast<double>(target.track->m_paths.front().points[segment_data::point::start].x), static_cast<double>(target.track->m_paths.front().points[segment_data::point::end].z)};
	if (m_magic_memo.key == key)
	{
		Plan = m_magic_memo.plan;
		return m_magic_memo.found;
	}
	auto const found{plan_magic_search(Plan)};
	m_magic_memo.key = key;
	m_magic_memo.found = found;
	m_magic_memo.plan = Plan;
	return found;
}

bool editor_mode::plan_magic_search(switch_link &Plan) const
{
	auto const level{std::clamp(m_switch.magic_level, 0, 2)};
	if (plan_magic_search(Plan, level))
		return true;
	// the boundary thresholds, with the consent of the line office, before giving up
	if (level < 2)
	{
		auto const reason{Plan.reason};
		if (plan_magic_search(Plan, 2))
			return true;
		Plan.reason = reason;
	}
	return false;
}

bool editor_mode::plan_magic_search(switch_link &Plan, int const Level) const
{
	auto const &tool{m_switch};
	auto const norm{static_cast<plk::level>(Level)};
	editor_track::chain chain;
	auto const frame{magic_frame(Plan.target, chain)};
	if (frame.empty())
	{
		Plan.reason = STR("Magic: the track to lead into has to be a regular track up to its free end");
		return false;
	}
	auto const &line{tool.line};
	// the armed switch first, then the others for the rail, the steepest first
	std::vector<int> order{tool.armed};
	auto const &armed{tool.templates[tool.armed]};
	std::vector<int> others;
	for (int i = 0; i < static_cast<int>(tool.templates.size()); ++i)
	{
		auto const &other{tool.templates[i]};
		if (i == tool.armed || other.double_slip || other.twofold() || other.ratio <= 0.0 || (other.rail != armed.rail && other.rail != "-" && armed.rail != "-"))
			continue;
		others.push_back(i);
	}
	std::sort(others.begin(), others.end(), [&](int const A, int const B) { return tool.templates[A].ratio < tool.templates[B].ratio; });
	if (others.size() > kMagicTemplates - 1)
		others.resize(kMagicTemplates - 1);
	order.insert(order.end(), others.begin(), others.end());

	auto const kink{glm::radians(m_joints.kink_angle() * 0.5)};
	auto const length{frame.back().station};
	for (auto const index : order)
	{
		auto const &shape{tool.templates[index]};
		if (shape.length > line.length - 0.02)
			continue;
		auto const from{switch_from(line, shape.length, tool.along, Plan.forward, true)};
		auto const along{Plan.forward > 0 ? from : from + shape.length};
		auto const point{line.start + glm::dvec3{line.direction.x, line.grade, line.direction.y} * along};
		auto const paths{editor_track::place_switch(shape, point, line.direction * static_cast<double>(Plan.forward), Plan.side, line.grade * Plan.forward)};
		if (paths.size() < 2)
			continue;
		auto const radius{editor_track::branch_radius(paths[1])};
		auto const line_speed{design_speed(nullptr, Plan.target.track)};
		auto const speed{std::min(editor_track::diverging_speed(radius), line_speed)};
		auto const kind{tool.magic_kind == 1 ? plk::track_kind::main : tool.magic_kind == 2 ? plk::track_kind::side : line_speed >= 80.0 ? plk::track_kind::main : plk::track_kind::side};
		auto const insert{std::ceil(editor_track::reverse_insert(speed) / 0.05 - 1e-6) * 0.05};
		auto const reach{std::min(kMagicReach, length - 1.0)};
		auto const sudden_max{plk::at(plk::sudden_deficiency_max(plk::track_kind::connection, speed), norm)};
		// cant deficiency with its sign (+ to the left) of curvature K and, on the track led into, its cant
		auto const signed_deficiency = [&](double const K, double const Cant = 0.0) {
			if (std::abs(K) < 1e-7)
				return 0.0;
			return (K > 0.0 ? 1.0 : -1.0) * (11.8 * speed * speed * std::abs(K) - Cant);
		};
		auto const switch_deficiency{signed_deficiency(piece_curvature(paths[1]))};
		// the norms of the link: radius and cant deficiency of its curve, sudden changes at the switch, between the curves and
		// where it lands; false where it breaks them, with the values kept for the status
		auto const keeps_norms = [&](editor_track::branch_link const &Link, editor_track::branch_end const &To, curve_sample const &Here, double &Deficiency, double &Sudden) {
			Deficiency = 0.0;
			Sudden = 0.0;
			auto const first{piece_curvature(Link.pieces.front())};
			auto const last{piece_curvature(Link.pieces.back())};
			if (Link.reverse)
			{
				if (Link.radius < plk::at(plk::radius_min(kind), norm) - 0.01)
					return false;
				Deficiency = plk::deficiency(speed, Link.radius);
				if (Deficiency > plk::at(plk::deficiency_max(kind, Link.radius), norm) + 0.5)
					return false;
				auto const a{signed_deficiency(first)};
				auto const b{signed_deficiency(last)};
				Sudden = std::abs(a - switch_deficiency);
				// between the curves: one change where they meet or the straight between is too short to keep them apart
				Sudden = std::max(Sudden, Link.insert >= plk::between_sudden_min(speed) - 1e-6 ? std::max(std::abs(a), std::abs(b)) : std::abs(b - a));
			}
			// where it lands on the track led into, of the curvature and cant it has there
			auto const ahead{sample_at(frame, std::min(Here.station + 2.0, length))};
			auto const run{std::max(1e-6, ahead.station - Here.station)};
			auto const bend{signed_angle(Here.tangent, ahead.tangent) / run};
			auto const cant{1500.0 * std::sin(glm::radians(std::abs(static_cast<double>(Here.roll))))};
			Sudden = std::max(Sudden, std::abs(signed_deficiency(bend, cant) - signed_deficiency(last)));
			return Sudden <= sudden_max + 0.5;
		};
		double found_deficiency{0.0};
		double found_sudden{0.0};
		auto const attempt = [&](double const Station, editor_track::branch_end &To) {
			auto const here{sample_at(frame, Station)};
			auto const ahead{sample_at(frame, std::min(Station + 1.0, length))};
			auto const run{std::max(1e-6, ahead.station - here.station)};
			To = {here.position, here.tangent, (ahead.position.y - here.position.y) / run, static_cast<double>(here.roll)};
			double deficiency{0.0};
			double sudden{0.0};
			auto link{editor_track::link_branch(branch_start(paths[1]), radius, To, kink, insert, false)};
			auto const usable = [&](editor_track::branch_link const &Link) {
				return Link.valid && std::abs(Link.trim) <= 0.01 && false == Link.pieces.empty() && Link.before >= -0.01;
			};
			if (usable(link) && false == keeps_norms(link, To, here, deficiency, sudden) && link.reverse && link.insert < insert - 1e-6)
			{
				// the curves meeting change the deficiency too much at once: the straight between them
				auto apart{editor_track::link_branch(branch_start(paths[1]), radius, To, kink, insert, false, true)};
				if (usable(apart))
					link = std::move(apart);
			}
			if (false == usable(link) || false == keeps_norms(link, To, here, deficiency, sudden))
				link.valid = false;
			else
			{
				found_deficiency = deficiency;
				found_sudden = sudden;
			}
			return link;
		};
		// coarse steps up to the first landing which works, then back in fine ones for the least taken back
		editor_track::branch_end to;
		editor_track::branch_link link;
		double station{-1.0};
		for (double coarse = 0.0; coarse <= reach; coarse += kMagicStep)
		{
			if ((link = attempt(coarse, to)).valid)
			{
				station = coarse;
				for (double fine = std::max(0.0, coarse - kMagicStep + kMagicFine); fine < coarse - 0.5 * kMagicFine; fine += kMagicFine)
				{
					editor_track::branch_end closer;
					if (auto found{attempt(fine, closer)}; found.valid)
					{
						link = std::move(found);
						to = closer;
						station = fine;
						break;
					}
				}
				break;
			}
		}
		if (station >= 0.0)
		{
			Plan.along = along;
			Plan.paths = paths;
			Plan.link = std::move(link);
			Plan.reason.clear();
			Plan.magic = true;
			Plan.shape = index;
			Plan.cut = station;
			Plan.landing = to;
			Plan.norm = Level;
			Plan.speed = speed;
			Plan.deficiency = found_deficiency;
			Plan.sudden = found_sudden;
			return true;
		}
	}
	Plan.reason = format(STR_C("Magic: no switch fits with the track taken back up to %.0f m"), std::min(kMagicReach, length));
	return false;
}

// the track the link leads into taken back from its free end by Plan.cut: the paths lying wholly in that stretch retired, the one it ends
// in split there and the part towards the free end retired; returns the path the link joins
TTrack *editor_mode::magic_cut(switch_link const &Plan, std::vector<std::pair<TTrack *, editor_track::state>> &States, std::vector<TTrack *> &Created, std::vector<TTrack *> &Removed)
{
	editor_track::chain chain;
	auto const frame{magic_frame(Plan.target, chain)};
	if (frame.empty())
		return nullptr;
	auto const cut{sample_at(frame, Plan.cut).position};
	if (Plan.cut < 0.05)
		return Plan.target.track;
	// the tracks of the chain from the free end on, each with the stations of its ends
	struct piece
	{
		TTrack *track;
		double low;
		double high;
	};
	std::vector<piece> pieces;
	for (auto *track : chain.tracks)
	{
		auto const &path{track->m_paths.front()};
		auto const a{nearest_station(frame, path.points[segment_data::point::start])};
		auto const b{nearest_station(frame, path.points[segment_data::point::end])};
		pieces.push_back({track, std::min(a, b), std::max(a, b)});
	}
	std::sort(pieces.begin(), pieces.end(), [](piece const &A, piece const &B) { return A.low < B.low; });
	std::string reason;
	std::vector<TTrack *> touched;
	for (auto const &entry : pieces)
		if (entry.low < Plan.cut + 0.05)
			touched.push_back(entry.track);
	if (false == tracks_editable(touched, reason))
	{
		ui()->set_status(reason, true);
		return nullptr;
	}
	TTrack *kept{nullptr};
	for (auto const &entry : pieces)
	{
		if (entry.high <= Plan.cut + 0.05)
		{
			// wholly in the stretch taken back
			editor_track::retire(*entry.track);
			Removed.push_back(entry.track);
			continue;
		}
		if (entry.low >= Plan.cut - 0.05)
		{
			if (kept == nullptr)
				kept = entry.track;
			continue;
		}
		// the cut runs through it
		States.emplace_back(entry.track, editor_track::capture(*entry.track));
		auto *second{editor_track::split_path(*entry.track, editor_track::nearest_parameter(*entry.track, cut))};
		if (second == nullptr)
		{
			kept = entry.track;
			continue;
		}
		editor_track::commit({entry.track, second});
		Created.push_back(second);
		// the part with an end before the cut lies towards the free end
		auto const first_low{std::min(nearest_station(frame, entry.track->m_paths.front().points[segment_data::point::start]), nearest_station(frame, entry.track->m_paths.front().points[segment_data::point::end]))};
		auto const second_low{std::min(nearest_station(frame, second->m_paths.front().points[segment_data::point::start]), nearest_station(frame, second->m_paths.front().points[segment_data::point::end]))};
		auto *gone{first_low < second_low ? entry.track : second};
		kept = (gone == entry.track ? second : entry.track);
		editor_track::retire(*gone);
		Removed.push_back(gone);
	}
	return kept;
}

void editor_mode::insert_magic_switch(switch_link const &Plan)
{
	std::vector<std::pair<TTrack *, editor_track::state>> states;
	std::vector<TTrack *> created;
	std::vector<TTrack *> removed;
	auto const &shape{m_switch.templates[Plan.shape]};
	if (false == place_switch_on_straight(m_switch.line, shape, nullptr, Plan.along, Plan.forward, Plan.side, false, states, created, removed, switch_name_for_new()))
		return;
	auto const placed{created};
	auto *style{Plan.target.track != nullptr && Plan.target.track->eType == tt_Normal ? Plan.target.track : m_switch.line.tracks.front()};
	// the style is taken before the track it comes from may be retired
	auto *kept{magic_cut(Plan, states, created, removed)};
	if (kept == nullptr)
	{
		// the switch stays, the link isn't made
		push_track_snapshot(std::move(states), std::move(created), std::move(removed));
		straight_refresh();
		switch_placed(placed);
		return;
	}
	editor_track::snap_target target{Plan.target};
	target.track = kept;
	target.position = Plan.landing.position;
	editor_track::branch_link link{Plan.link};
	link.trim = 0.0;
	link_tracks(target, link, *(kept != nullptr ? kept : style), states, created);
	push_track_snapshot(std::move(states), std::move(created), std::move(removed));
	straight_refresh();
	m_switch.status.clear();
	switch_placed(placed);
	auto summary{format(STR_C("Magic: %s, the track behind it taken back %.1f m"), switch_template_label(shape).c_str(), Plan.cut)};
	if (Plan.link.reverse)
		summary += format(STR_C(", a reverse curve R %.0f m with %.1f m straight between"), Plan.link.radius, Plan.link.insert);
	else if (Plan.link.radius > 0.0)
		summary += format(STR_C(", a curve R %.0f m"), Plan.link.radius);
	if (Plan.norm >= 0)
	{
		auto const level{static_cast<plk::level>(Plan.norm)};
		summary += format(STR_C("; ST-T1-A6 %s at %.0f km/h: I %.0f mm, sudden change of I %.0f mm"), plk::name(level), Plan.speed, Plan.deficiency, Plan.sudden);
		if (Plan.norm > std::clamp(m_switch.magic_level, 0, 2))
			summary += STR(" - only with the boundary thresholds P2, which take the consent of the line office");
	}
	m_switch.status = summary + (m_switch.status.empty() ? std::string{} : "\n" + m_switch.status);
	ui()->set_status(m_switch.status, false);
	WriteLog("Editor: " + m_switch.status);
}
