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
// curve of the radius of the switch, or a reverse curve no tighter than 180 m with the straight between the curves the speed asks
// for, ST-T1-A6 tab. 9.2 and pkt 9.6), the rest of the track kept as it lies. The template armed is tried first, then the other
// switches for the rail from the steepest (1:9) on, as a tight place takes the shortest switch

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editormodeshared.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"

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
double constexpr kMagicMinimumRadius{180.0}; // m, the tightest curve of the link, Rmin P2 of tab. 9.2

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
	                              tool.line.length, std::round(tool.along * 2.0), static_cast<double>(Plan.forward), static_cast<double>(Plan.side), static_cast<double>(tool.armed), static_cast<double>(tool.templates.size()),
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
	auto const &tool{m_switch};
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
		auto const insert{editor_track::reverse_insert(std::min(editor_track::diverging_speed(radius), design_speed(nullptr, Plan.target.track)))};
		auto const reach{std::min(kMagicReach, length - 1.0)};
		auto const attempt = [&](double const Station, editor_track::branch_end &To) {
			auto const here{sample_at(frame, Station)};
			auto const ahead{sample_at(frame, std::min(Station + 1.0, length))};
			auto const run{std::max(1e-6, ahead.station - here.station)};
			To = {here.position, here.tangent, (ahead.position.y - here.position.y) / run, static_cast<double>(here.roll)};
			auto link{editor_track::link_branch(branch_start(paths[1]), radius, To, kink, insert, false)};
			if (false == link.valid || std::abs(link.trim) > 0.01 || link.pieces.empty() || link.before < -0.01 || (link.reverse && link.radius < kMagicMinimumRadius))
				link.valid = false;
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
	m_switch.status = summary + (m_switch.status.empty() ? std::string{} : "\n" + m_switch.status);
	ui()->set_status(m_switch.status, false);
	WriteLog("Editor: " + m_switch.status);
}
