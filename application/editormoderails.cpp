/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// GeoTools, Tracks (ficzery2): the lines of a railway map (editor/editorRailData) within the chunks round the camera designed
// as tracks by the norms (editor/editorRailDesign) and laid on the ground, the way the lay tool lays its tracks: the heights
// fitted to the ground as a grade line, each piece named after its place on its line. A line is cut into its stretches by the
// map alone and each stretch is designed whole, so what's laid in one area meets what's laid in the next one, whenever.

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editormodeshared.h"
#include "application/editormodetrackshared.h"
#include "application/editorprojection.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"
#include "editor/editorGroundFollow.hpp"
#include "editor/editorTrack.hpp"
#include "scene/scenelayers.h"
#include "simulation/simulation.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"
#include "world/Track.h"

#include "imgui/imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>

namespace
{

using geometry::grade_of;
using geometry::plan_of;

double const kPreviewReach{6000.0}; // m from the camera the lines of the map are drawn within
double const kLabelReach{2500.0};
double const kJointReach{0.2}; // m in the plan, a free end this near the start of a run is the track it goes on from
double const kJointSearch{60.0}; // m, round the place on the ground the free ends are looked for, whatever their height
double const kFar{10.0}; // m, a line designed this far from the points of the map is shown as one to check
ImU32 const kMapColour{IM_COL32(120, 190, 255, 200)};
ImU32 const kTrackColour{IM_COL32(90, 230, 110, 240)};
ImU32 const kSlowColour{IM_COL32(255, 170, 60, 240)};
ImU32 const kBadColour{IM_COL32(255, 80, 80, 240)};
ImU32 const kAreaColour{IM_COL32(255, 220, 60, 230)};

bool within(glm::dvec2 const &Point, glm::dvec2 const &Low, glm::dvec2 const &High)
{
	return Point.x >= Low.x && Point.x <= High.x && Point.y >= Low.y && Point.y <= High.y;
}

// whether the line through the bends goes through the box
bool crosses(std::vector<glm::dvec2> const &Bends, glm::dvec2 const &Low, glm::dvec2 const &High)
{
	for (std::size_t i = 1; i < Bends.size(); ++i)
	{
		auto const steps{std::max(1, static_cast<int>(std::ceil(glm::distance(Bends[i], Bends[i - 1]) / 25.0)))};
		for (int step = 0; step <= steps; ++step)
			if (within(glm::mix(Bends[i - 1], Bends[i], static_cast<double>(step) / steps), Low, High))
				return true;
	}
	return false;
}

glm::dvec2 middle_of(segment_data const &Piece)
{
	return 0.5 * (plan_of(Piece.points[segment_data::point::start]) + plan_of(Piece.points[segment_data::point::end]));
}

// a point of the piece in the plan, T from 0 at its start to 1 at its end
glm::dvec2 point_on(segment_data const &Piece, double const T)
{
	auto const start{plan_of(Piece.points[segment_data::point::start])};
	auto const end{plan_of(Piece.points[segment_data::point::end])};
	auto const first{start + plan_of(Piece.points[segment_data::point::control1])};
	auto const second{end + plan_of(Piece.points[segment_data::point::control2])};
	auto const rest{1.0 - T};
	return rest * rest * rest * start + 3.0 * rest * rest * T * first + 3.0 * rest * T * T * second + T * T * T * end;
}

glm::dvec2 heading_at(segment_data const &Piece, bool const AtEnd)
{
	auto const control{plan_of(Piece.points[AtEnd ? segment_data::point::control2 : segment_data::point::control1])};
	auto const chord{plan_of(Piece.points[segment_data::point::end] - Piece.points[segment_data::point::start])};
	auto const along{glm::length(control) > 1e-6 ? (AtEnd ? -control : control) : chord};
	return glm::length(along) > 1e-9 ? glm::normalize(along) : glm::dvec2{0.0, 1.0};
}

std::string piece_name(std::string const &Stretch, std::size_t const Track, std::size_t const Index)
{
	return format("%s_t%zu_%05zu", Stretch.c_str(), Track + 1, Index);
}

std::string read_text(std::filesystem::path const &Path)
{
	std::ifstream in{Path, std::ios::binary};
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

char const *electrified_name(int const Code)
{
	return Code > 0 ? STR_C("electrified") : STR_C("not electrified");
}

} // namespace

std::string editor_mode::rails_folder() const
{
	return m_streamer.directory() + "tory/";
}

glm::dvec2 editor_mode::rails_world(glm::dvec2 const &Map) const
{
	auto const &manifest{m_streamer.manifest()};
	return {manifest.origin_east - Map.x, Map.y - manifest.origin_north};
}

void editor_mode::rails_read()
{
	m_rails_read = true;
	m_rails_data = {};
	m_rails_lines.clear();
	m_rails_plans.clear();
	std::size_t files{0};
	std::string problems;
	std::error_code error;
	for (auto const &entry : std::filesystem::directory_iterator{rails_folder(), error})
	{
		if (entry.path().extension() != ".geojson")
			continue;
		std::string reason;
		if (rail_data::read_geojson(read_text(entry.path()), m_rails_data, reason))
			++files;
		else
			problems += "\n" + entry.path().filename().string() + ": " + reason;
	}
	rails_make_lines();
	m_rails_status = files == 0 ? std::string{STR_C("No map of the lines read")} : format(STR_C("Read %zu files: %zu sections of lines, %zu stations; %zu stretches to design"), files, m_rails_data.sections.size(), m_rails_data.stations.size(), m_rails_lines.size());
	m_rails_status += problems;
	WriteLog("Editor: " + m_rails_status);
}

void editor_mode::rails_make_lines()
{
	m_rails_lines.clear();
	m_rails_plans.clear();
	m_rails_cut = m_rails.data;
	for (auto const &chain : rail_data::chains_of(m_rails_data))
		for (auto &stretch : rail_data::stretches_of(chain, m_rails.data))
		{
			rails_line line;
			for (auto const &bend : stretch.bends)
				line.bends.push_back(rails_world(bend));
			for (auto point{stretch.first}; point <= stretch.last; ++point)
				line.points.push_back(rails_world(chain.points[point]));
			line.stretch = std::move(stretch);
			m_rails_lines.push_back(std::move(line));
		}
}

void editor_mode::rails_area(glm::dvec2 &Low, glm::dvec2 &High) const
{
	auto const chunk{heightmap::chunk_at(Camera.Pos.x, Camera.Pos.z)};
	auto const radius{std::max(0, m_rails.radius)};
	Low = glm::dvec2{static_cast<double>(chunk.first - radius), static_cast<double>(chunk.second - radius)} * heightmap::chunk_size;
	High = Low + glm::dvec2{static_cast<double>(2 * radius + 1) * heightmap::chunk_size};
}

std::vector<std::size_t> editor_mode::rails_lines_in_area() const
{
	glm::dvec2 low;
	glm::dvec2 high;
	rails_area(low, high);
	std::vector<std::size_t> found;
	for (std::size_t i = 0; i < m_rails_lines.size(); ++i)
		if (crosses(m_rails_lines[i].bends, low, high))
			found.push_back(i);
	return found;
}

void editor_mode::rails_design()
{
	auto const lines{rails_lines_in_area()};
	auto const norms{laying_norms()};
	auto const offset{0.5 * static_cast<double>(m_rails.spacing)};
	// the stretches designed before with the same settings are kept: designing one takes a while
	std::vector<rails_plan> plans;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		if (false == editor_busy_progress(static_cast<double>(i) / static_cast<double>(lines.size())))
			break;
		auto const kept{std::ranges::find(m_rails_plans, lines[i], &rails_plan::line)};
		if (kept != m_rails_plans.end() && kept->spacing == m_rails.spacing && kept->piece == m_rails.piece)
		{
			plans.push_back(*kept);
			continue;
		}
		auto const &line{m_rails_lines[lines[i]]};
		rails_plan plan;
		plan.line = lines[i];
		plan.spacing = m_rails.spacing;
		plan.piece = m_rails.piece;
		plan.design = rail_design::design_of(line.bends, line.stretch.speed, norms, m_route.design.shape);
		if (plan.design.shape.valid)
		{
			plan.deviation = rail_design::deviation_of(plan.design.shape, line.points);
			plan.axis = rail_design::pieces_of(plan.design, m_rails.piece);
			plan.speeds = rail_design::speeds_of(plan.design, plan.axis, line.stretch.speed);
			plan.tracks = {plan.axis};
			auto left{rail_design::beside(plan.axis, offset)};
			auto right{rail_design::beside(plan.axis, -offset)};
			plan.pair = line.stretch.about.tracks >= 2 && false == left.empty() && false == right.empty();
			if (plan.pair)
				plan.tracks = {std::move(left), std::move(right)};
		}
		plans.push_back(std::move(plan));
	}
	m_rails_plans = std::move(plans);
	rails_count();
	auto const failed{std::ranges::count_if(m_rails_plans, [](rails_plan const &Plan) { return false == Plan.design.shape.valid; })};
	m_rails_status = format(STR_C("Designed %zu stretches of the area, %zu of them couldn't be"), m_rails_plans.size(), static_cast<std::size_t>(failed));
	WriteLog("Editor: " + m_rails_status);
}

// the pieces of the plan lying in the area and not laid yet, as runs of ones following one another: first and last
std::vector<std::pair<std::size_t, std::size_t>> editor_mode::rails_runs(rails_plan const &Plan) const
{
	glm::dvec2 low;
	glm::dvec2 high;
	rails_area(low, high);
	auto const &name{m_rails_lines[Plan.line].stretch.name};
	std::vector<std::pair<std::size_t, std::size_t>> runs;
	std::optional<std::size_t> open;
	for (std::size_t i = 0; i < Plan.axis.size(); ++i)
	{
		auto const wanted{within(middle_of(Plan.axis[i]), low, high) && simulation::Paths.find(piece_name(name, 0, i)) == nullptr};
		if (wanted && false == open.has_value())
			open = i;
		if (false == wanted && open.has_value())
		{
			runs.emplace_back(*open, i - 1);
			open.reset();
		}
	}
	if (open.has_value())
		runs.emplace_back(*open, Plan.axis.size() - 1);
	return runs;
}

void editor_mode::rails_count()
{
	auto const chunk{heightmap::chunk_at(Camera.Pos.x, Camera.Pos.z)};
	m_rails_counted = {chunk.first, chunk.second, m_rails.radius};
	for (auto &plan : m_rails_plans)
	{
		plan.waiting = 0;
		for (auto const &[first, last] : rails_runs(plan))
			plan.waiting += last - first + 1;
	}
}

// the free end of a track at the place in the plan, whatever its height: the track a run goes on from, or comes to
std::optional<editor_track::snap_target> editor_mode::rails_joint(glm::dvec2 const &At) const
{
	double ground{Camera.Pos.y};
	m_streamer.height_at(At.x, At.y, ground);
	glm::dvec3 const place{At.x, ground, At.y};
	for (auto const &end : editor_track::free_ends(nullptr, ::track_mode::kRailCategory, place, kJointSearch, {}))
		if (glm::distance(plan_of(end.position), At) <= kJointReach)
			return end;
	return std::nullopt;
}

// the heights of the pieces of a run of the axis: a grade line of the norms fitted to the ground under it, going on from
// the tracks laid before at its ends where there are some
void editor_mode::rails_heights(std::vector<segment_data> &Run, double const Speed, double const Offset) const
{
	auto const nan{std::numeric_limits<double>::quiet_NaN()};
	ground_follow::ends ends{0.0, nan, 0.0, nan};
	auto const end_of = [&](bool const AtEnd, double &Height, double &Grade) {
		auto const &piece{AtEnd ? Run.back() : Run.front()};
		auto const at{plan_of(piece.points[AtEnd ? segment_data::point::end : segment_data::point::start])};
		auto const heading{heading_at(piece, AtEnd)};
		// of a double line the left track is the one looked for: the axis has no track of its own
		auto const joint{rails_joint(at + glm::dvec2{-heading.y, heading.x} * Offset)};
		if (joint.has_value())
		{
			Height = joint->position.y;
			Grade = AtEnd ? grade_of(joint->direction) : -grade_of(joint->direction);
			return;
		}
		Height = bed_height({at.x, Camera.Pos.y, at.y}, heading, nullptr);
	};
	end_of(false, ends.height0, ends.grade0);
	end_of(true, ends.height1, ends.grade1);
	auto grading{laying_grading(nullptr)};
	grading.settings.speed = Speed;
	auto const bed = [this](std::vector<glm::dvec3> const &Points, std::vector<glm::dvec2> const &Directions) { return bed_heights(Points, Directions, nullptr); };
	ground_follow::follow(Run, ends, bed, grading);
}

// the pieces of one track of a run made paths of the scenery, named after their places on the line
void editor_mode::rails_create(rails_plan const &Plan, std::vector<segment_data> const &Pieces, std::size_t const Track, std::size_t const First, std::vector<TTrack *> &Created) const
{
	auto const &name{m_rails_lines[Plan.line].stretch.name};
	auto style{m_lay.style};
	for (std::size_t i = 0; i < Pieces.size(); ++i)
	{
		style.velocity = Plan.speeds[First + i];
		auto *track{editor_track::create_path(style, Pieces[i])};
		if (track == nullptr)
			continue;
		// a name taken already (a path of the other track laid by itself before) leaves the path the name it was made with
		if (auto const wanted{piece_name(name, Track, First + i)}; simulation::Paths.find(wanted) == nullptr)
			editor_track::rename(*track, wanted);
		Created.push_back(track);
	}
}

std::size_t editor_mode::rails_lay_plan(rails_plan const &Plan, std::vector<TTrack *> &Created) const
{
	auto const offset{Plan.pair ? 0.5 * static_cast<double>(Plan.spacing) : 0.0};
	auto const speed{m_rails_lines[Plan.line].stretch.speed};
	std::size_t laid{0};
	for (auto const &[first, last] : rails_runs(Plan))
	{
		std::vector<segment_data> run(Plan.axis.begin() + static_cast<std::ptrdiff_t>(first), Plan.axis.begin() + static_cast<std::ptrdiff_t>(last) + 1);
		rails_heights(run, speed, offset);
		if (Plan.pair)
		{
			rails_create(Plan, rail_design::beside(run, offset), 0, first, Created);
			rails_create(Plan, rail_design::beside(run, -offset), 1, first, Created);
		}
		else
			rails_create(Plan, run, 0, first, Created);
		laid += run.size();
	}
	return laid;
}

void editor_mode::rails_lay()
{
	if (m_rails_plans.empty())
		rails_design();
	// the tracks take only their heights from the ground: the terrain is shaped under them by the tools of the terrain
	m_nmt_state.laying = true;
	std::vector<TTrack *> created;
	std::size_t pieces{0};
	for (std::size_t i = 0; i < m_rails_plans.size(); ++i)
	{
		if (false == editor_busy_progress(static_cast<double>(i) / static_cast<double>(m_rails_plans.size())))
			break;
		if (m_rails_plans[i].design.shape.valid)
			pieces += rails_lay_plan(m_rails_plans[i], created);
	}
	if (false == created.empty())
	{
		editor_track::commit(created);
		push_track_snapshot({}, created);
	}
	m_nmt_state.laying = false;
	rails_count();
	double length{0.0};
	for (auto const *track : created)
		length += track->Length();
	m_rails_status = created.empty() ? std::string{STR_C("Nothing to lay: the tracks of the lines of the area lie there already, or no line goes through it")}
	                                 : format(STR_C("Laid %zu paths, %.1f km of track, along %zu pieces of the lines. Ctrl+Z takes it back"), created.size(), length / 1000.0, pieces);
	if (scene::Layers.active() == null_handle)
		m_rails_status += "\n" + std::string{STR_C("The scenery isn't opened for editing, the new track won't be saved")};
	WriteLog("Editor: " + m_rails_status);
}

void editor_mode::draw_rails_plan(rails_pen const &Line, rails_plan const &Plan) const
{
	auto const &line{m_rails_lines[Plan.line]};
	if (false == Plan.design.shape.valid)
	{
		Line(line.bends, kBadColour, 3.0f);
		return;
	}
	glm::dvec2 const camera{Camera.Pos.x, Camera.Pos.z};
	auto const astray{Plan.deviation.worst > kFar};
	for (auto const &track : Plan.tracks)
		for (std::size_t i = 0; i < track.size(); ++i)
		{
			if (glm::distance(middle_of(track[i]), camera) > kPreviewReach)
				continue;
			auto const slow{Plan.speeds[i] < line.stretch.speed};
			Line({point_on(track[i], 0.0), point_on(track[i], 0.25), point_on(track[i], 0.5), point_on(track[i], 0.75), point_on(track[i], 1.0)}, astray ? kBadColour : slow ? kSlowColour : kTrackColour, 2.5f);
		}
}

void editor_mode::draw_rails_preview(ImDrawList *Drawlist)
{
	if (false == m_rails_read || false == m_streamer.manifest().georeferenced)
		return;
	screen_projection const projection;
	// a place of the plan on the screen, Above m over the ground; false where it isn't seen
	auto const seen = [this, &projection](glm::dvec2 const &Place, double const Above, ImVec2 &Screen) {
		double ground{Camera.Pos.y};
		m_streamer.height_at(Place.x, Place.y, ground);
		return projection.project(glm::dvec3{Place.x, ground + Above, Place.y}, Screen);
	};
	rails_pen const pen{[Drawlist, &seen](std::vector<glm::dvec2> const &Points, unsigned int const Colour, float const Thickness) {
		ImVec2 before;
		auto drawn{false};
		for (auto const &point : Points)
		{
			ImVec2 at;
			auto const visible{seen(point, 0.3, at)};
			if (visible && drawn)
				Drawlist->AddLine(before, at, Colour, Thickness);
			before = at;
			drawn = visible;
		}
	}};
	glm::dvec2 low;
	glm::dvec2 high;
	rails_area(low, high);
	pen({low, {high.x, low.y}, high, {low.x, high.y}, low}, kAreaColour, 2.0f);
	glm::dvec2 const camera{Camera.Pos.x, Camera.Pos.z};
	for (auto const &line : m_rails_lines)
	{
		auto const nearest{std::ranges::min_element(line.bends, {}, [&camera](glm::dvec2 const &Bend) { return glm::distance(Bend, camera); })};
		auto const away{glm::distance(*nearest, camera)};
		if (away > kPreviewReach)
			continue;
		pen(line.bends, kMapColour, 1.5f);
		if (ImVec2 at; away < kLabelReach && seen(*nearest, 2.0, at))
			Drawlist->AddText(at, kMapColour, format("%s  %.0f km/h", line.stretch.name.c_str(), line.stretch.speed).c_str());
	}
	for (auto const &plan : m_rails_plans)
		draw_rails_plan(pen, plan);
	for (auto const &station : m_rails_data.stations)
	{
		auto const place{rails_world(station.at)};
		if (ImVec2 at; glm::distance(place, camera) < kLabelReach && seen(place, 6.0, at))
			Drawlist->AddText(at, kAreaColour, station.name.c_str());
	}
}

void editor_mode::render_rails_lines()
{
	auto const lines{rails_lines_in_area()};
	ImGui::TextUnformatted(format(STR_C("Stretches of lines through the area: %zu"), lines.size()).c_str());
	if (lines.empty() || false == ImGui::BeginTable("##railslines", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
		return;
	ImGui::TableSetupColumn(STR_C("Stretch##rails"));
	ImGui::TableSetupColumn(STR_C("km##rails"));
	ImGui::TableSetupColumn(STR_C("km/h##rails"));
	ImGui::TableSetupColumn(STR_C("Tracks##rails"));
	ImGui::TableSetupColumn(STR_C("Traction##rails"));
	ImGui::TableHeadersRow();
	for (auto const index : lines)
	{
		auto const &stretch{m_rails_lines[index].stretch};
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(stretch.name.c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%.1f - %.1f", stretch.about.km_start, stretch.about.km_end);
		ImGui::TableNextColumn();
		ImGui::Text("%.0f", stretch.speed);
		ImGui::TableNextColumn();
		ImGui::Text("%d", stretch.about.tracks >= 2 ? 2 : 1);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(electrified_name(stretch.about.electrified));
	}
	ImGui::EndTable();
}

void editor_mode::render_rails_plans()
{
	// the camera gone to another chunk, or the radius changed: other pieces wait to be laid
	if (auto const chunk{heightmap::chunk_at(Camera.Pos.x, Camera.Pos.z)}; m_rails_counted != std::array<int, 3>{chunk.first, chunk.second, m_rails.radius})
		rails_count();
	if (m_rails_plans.empty() || false == ImGui::BeginTable("##railsplans", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
		return;
	ImGui::TableSetupColumn(STR_C("Stretch##railsplan"));
	ImGui::TableSetupColumn(STR_C("m##railsplan"));
	ImGui::TableSetupColumn(STR_C("Curves##railsplan"));
	ImGui::TableSetupColumn(STR_C("Slower##railsplan"));
	ImGui::TableSetupColumn(STR_C("From the map (m)##railsplan"));
	ImGui::TableSetupColumn(STR_C("To lay##railsplan"));
	ImGui::TableHeadersRow();
	for (auto const &plan : m_rails_plans)
	{
		auto const &shape{plan.design.shape};
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(m_rails_lines[plan.line].stretch.name.c_str());
		ImGui::TableNextColumn();
		if (false == shape.valid)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", plan.design.error.c_str());
			continue;
		}
		ImGui::Text("%.0f", shape.length);
		ImGui::TableNextColumn();
		ImGui::Text("%zu", shape.curves.size());
		ImGui::TableNextColumn();
		ImGui::Text("%zu", plan.design.limits.size());
		ImGui::TableNextColumn();
		ImGui::TextColored(plan.deviation.worst > kFar ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%.1f / %.1f", plan.deviation.rms, plan.deviation.worst);
		ImGui::TableNextColumn();
		ImGui::Text("%zu / %zu", plan.waiting, plan.axis.size());
	}
	ImGui::EndTable();
	item_tooltip(STR_C("Slower: curves too sharp for the speed of the line, laid with the speed they allow. From the map: how far the line designed lies from the points of the map, "
	                   "on average and at the worst; red where that's far, to be looked at on the orthophoto before it's laid. To lay: pieces within the area not laid yet, of all of the stretch"));
}

void editor_mode::render_rails_settings()
{
	auto &settings{m_rails};
	ImGui::SetNextItemWidth(160.0f);
	ImGui::SliderInt(STR_C("Chunks round the one under the camera##rails"), &settings.radius, 0, 12);
	item_tooltip(STR_C("The area the tracks are laid in: the chunk of the terrain under the camera and as many chunks round it; drawn yellow on the ground"));
	ImGui::SameLine();
	ImGui::TextDisabled("%.0f m", (2 * settings.radius + 1) * heightmap::chunk_size);
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputFloat(STR_C("Between the tracks of a double line (m)##rails"), &settings.spacing, 0.05f, 0.5f, "%.2f");
	settings.spacing = std::clamp(settings.spacing, 3.5f, 10.0f);
	item_tooltip(STR_C("The map gives the axis of the line: its two tracks are laid half of this to either side of it"));
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputFloat(STR_C("Pieces of track up to (m)##rails"), &settings.piece, 10.0f, 50.0f, "%.0f");
	settings.piece = std::clamp(settings.piece, 20.0f, 500.0f);
	auto speed{static_cast<float>(settings.data.default_speed)};
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputFloat(STR_C("Speed where the map gives none (km/h)##rails"), &speed, 10.0f, 20.0f, "%.0f");
	settings.data.default_speed = std::clamp(static_cast<double>(speed), 20.0, 200.0);
	auto simplify{static_cast<float>(settings.data.simplify)};
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputFloat(STR_C("A point of the map is a bend from (m)##rails"), &simplify, 0.5f, 1.0f, "%.1f");
	settings.data.simplify = std::clamp(static_cast<double>(simplify), 0.5, 20.0);
	item_tooltip(STR_C("How far a point of the map has to lie from the straight through its neighbours to be a bend of the line: lower follows the map closer, "
	                   "and takes its inaccuracies for curves"));
	if (false == (settings.data == m_rails_cut))
		rails_make_lines();
}

void editor_mode::render_rails_ui()
{
	ImGui::TextWrapped("%s", STR_C("The lines of a railway map laid as tracks: designed by the norms from the axes of the map, with the speeds, the cants and the transition curves, "
	                               "their heights a grade line fitted to the ground. The tracks of stations, the switches and the shaping of the terrain under the tracks aren't made here yet"));
	if (false == m_streamer.manifest().georeferenced)
	{
		ImGui::TextWrapped("%s", STR_C("The terrain doesn't know where it lies (PUWG 1992): set its origin first"));
		return;
	}
	if (false == m_rails_read)
		rails_read();
	if (ImGui::Button(STR_C("Read the map again##rails")))
		rails_read();
	item_tooltip(STR_C("The files .geojson (PUWG 1992, EPSG:2180) of the folder tory/ beside the terrain of the scenery: the axes of the lines with LN, km_s, km_e, v_k, NT, EL"));
	ImGui::SameLine();
	ImGui::TextDisabled("%s", rails_folder().c_str());
	if (m_rails_lines.empty())
	{
		ImGui::TextWrapped("%s", m_rails_status.c_str());
		return;
	}
	render_rails_settings();
	render_rails_lines();
	if (main_button(STR_C("Design the tracks of the area##rails"), ImVec2(-FLT_MIN, 0.0f)))
		run_busy(STR_C("Designing the tracks"), [this]() { rails_design(); });
	item_tooltip(STR_C("Each stretch of a line going through the area is designed whole, which takes up to some seconds a stretch; the tracks are shown green, "
	                   "orange where a curve makes them slower than the line, red where they lie far from the map"));
	render_rails_plans();
	ImGui::BeginDisabled(m_rails_plans.empty());
	if (main_button(STR_C("Lay the tracks of the area##rails"), ImVec2(-FLT_MIN, tall_button_height())))
		run_busy(STR_C("Laying the tracks"), [this]() { rails_lay(); });
	ImGui::EndDisabled();
	item_tooltip(STR_C("The pieces of the tracks designed which lie within the area are laid, the ones laid before left as they are; they go on from the tracks "
	                   "laid in the areas next to it. Ctrl+Z takes all of it back"));
	if (false == m_rails_status.empty())
		ImGui::TextWrapped("%s", m_rails_status.c_str());
}
