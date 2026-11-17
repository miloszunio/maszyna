/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "world/RoadBarrier.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <tuple>

namespace road_barrier
{

namespace
{

constexpr std::size_t most_entries{16};
constexpr double cap_length{0.5}; // of a barrier cut short
constexpr double cap_bend{0.25}; // how far the end of such barrier is bent away from the road
constexpr double buried{-0.05}; // where the top of the rail is at the end of a barrier led into the ground
constexpr double post_depth{1.0}; // of the posts under the plane of the roadway, so they reach the ground of a bank as well
constexpr double post_gap{0.06}; // between the back of the rail and the post
constexpr double post_width{0.055};
constexpr double post_length{0.1};
constexpr double spacer_height{0.2};
constexpr double shortest_rail{0.25}; // a post isn't put where the barrier is lower than this part of its height
constexpr double texture_length{2.0};

glm::dvec3 const up{0.0, 1.0, 0.0};

constexpr std::array<std::string_view, 3> side_names{"left", "right", "middle"};
constexpr std::array<std::string_view, 3> kind_names{"wbeam", "wbeam2", "concrete"};
constexpr std::array<std::string_view, 3> ending_names{"ramp", "flare", "cap"};

// the rail seen along the road, the road being to the left of it: distance from its face and height over its middle
struct corner
{
	double depth;
	double height;
};
constexpr std::array<corner, 10> rail_shape{corner{0.083, 0.156}, corner{0.06, 0.15},  corner{0.0, 0.105},  corner{0.0, 0.055},  corner{0.07, 0.012},
                                            corner{0.07, -0.012}, corner{0.0, -0.055}, corner{0.0, -0.105}, corner{0.06, -0.15}, corner{0.083, -0.156}};

template <std::size_t Count_>
std::optional<std::size_t> index_of(std::array<std::string_view, Count_> const &Names, std::string_view const Name)
{
	auto const found{std::ranges::find(Names, Name)};
	if (found == Names.end())
		return std::nullopt;
	return static_cast<std::size_t>(std::distance(Names.begin(), found));
}

bool read_number(std::string_view const Text, float &Value)
{
	float value{0.f};
	auto const *last{Text.data() + Text.size()};
	auto const result{std::from_chars(Text.data(), last, value)};
	if (result.ec != std::errc{} || result.ptr != last || false == std::isfinite(value))
		return false;
	Value = value;
	return true;
}

std::vector<std::string_view> fields_of(std::string_view Text)
{
	std::vector<std::string_view> fields;
	auto more{true};
	while (more)
	{
		auto const colon{Text.find(':')};
		fields.emplace_back(Text.substr(0, colon));
		more = (colon != std::string_view::npos);
		if (more)
			Text.remove_prefix(colon + 1);
	}
	return fields;
}

// true if the two are the same barrier but for where it starts and ends
bool same_barrier(entry const &Left, entry const &Right)
{
	return Left.where == Right.where && Left.type == Right.type && Left.offset == Right.offset && Left.material == Right.material;
}

// the values of an entry within their limits; the range is seen to once the length of the axis is known
void confine(entry &Entry)
{
	Entry.offset = std::clamp(std::isfinite(Entry.offset) ? Entry.offset : 1.f, 0.25f, 50.f);
	for (auto &length : Entry.lengths)
		length = std::clamp(std::isfinite(length) ? length : 12.f, 0.f, 30.f);
	if (false == std::isfinite(Entry.from) || Entry.from <= snap)
		Entry.from = 0.f;
	if (false == std::isfinite(Entry.to) || Entry.to < 0.f)
		Entry.to = -1.f;
	if (Entry.material.empty())
		Entry.material = "none";
}

// a barrier which gets to the end of the axis, or nearly, ends with the axis whatever its length
void fit(entry &Entry, double const Length)
{
	if (Entry.to >= 0.f && Entry.to >= Length - snap)
		Entry.to = -1.f;
}

bool ahead(entry const &Left, entry const &Right)
{
	return std::tie(Left.where, Left.type, Left.offset, Left.material, Left.from) < std::tie(Right.where, Right.type, Right.offset, Right.material, Right.from);
}

// makes one of the barriers which touch or overlap. the entries are in order
void merge(std::vector<entry> &Entries, double const Length)
{
	std::vector<entry> merged;
	for (auto const &next : Entries)
	{
		if (merged.empty() || false == same_barrier(merged.back(), next) || next.from > end_of(merged.back(), Length) + 0.01)
		{
			merged.emplace_back(next);
			continue;
		}
		auto &last{merged.back()};
		if (end_of(next, Length) < end_of(last, Length))
			continue;
		// the one which gets further is where the barrier ends
		last.to = next.to;
		last.endings[1] = next.endings[1];
		last.lengths[1] = next.lengths[1];
	}
	Entries = std::move(merged);
}

// ends of a barrier as it's led into the ground or away from the road: how far it's got from an end, as a part of the length this takes
struct approach
{
	double start{1.0};
	double end{1.0};
	double startlength{0.0};
	double endlength{0.0};
};

double taken(end_data const &End)
{
	return End.style == ending::cap ? cap_length : End.length;
}

approach approach_at(layout const &Layout, double const Station)
{
	approach result;
	result.startlength = taken(Layout.ends[0]);
	result.endlength = taken(Layout.ends[1]);
	if (false == Layout.ends[0].far && false == Layout.ends[1].far)
	{
		// a barrier shorter than its two ends has them both made shorter
		auto const whole{Layout.ends[0].beyond + (Layout.to - Layout.from) + Layout.ends[1].beyond};
		if (auto const ends{result.startlength + result.endlength}; ends > whole && ends > 0.0)
		{
			result.startlength *= whole / ends;
			result.endlength *= whole / ends;
		}
	}
	if (false == Layout.ends[0].far && result.startlength > 0.0)
		result.start = std::clamp((Layout.ends[0].beyond + Station - Layout.from) / result.startlength, 0.0, 1.0);
	if (false == Layout.ends[1].far && result.endlength > 0.0)
		result.end = std::clamp((Layout.ends[1].beyond + Layout.to - Station) / result.endlength, 0.0, 1.0);
	return result;
}

// what an end does to the barrier at specified part of the way from it
profile end_profile(ending const Style, double const Part, double const Length)
{
	profile result;
	if (Style == ending::cap)
	{
		result.away = cap_bend * (1.0 - Part) * (1.0 - Part);
		return result;
	}
	result.rise = Part;
	if (Style == ending::flare)
		result.away = (1.0 - Part) * Length / flare_rate;
	return result;
}

// the direction away from the road, level
glm::dvec3 outwards(layout const &Layout, frame const &Frame)
{
	glm::dvec3 across{Frame.left.x, 0.0, Frame.left.z};
	if (glm::dot(across, across) < 1e-12)
		across = glm::dvec3{1.0, 0.0, 0.0};
	return glm::normalize(across) * (Layout.onleft ? 1.0 : -1.0);
}

// height of the middle of the rail over the plane of the roadway
double rail_middle(double const Rise)
{
	return std::lerp(buried, rail_top, Rise) - 0.5 * rail_height;
}

// a triangle seen from the side its normal points to
void face(std::vector<world_vertex> &Output, std::array<world_vertex, 3> Corners, glm::dvec3 const &Normal)
{
	if (glm::dot(glm::cross(Corners[1].position - Corners[0].position, Corners[2].position - Corners[0].position), Normal) < 0.0)
		std::swap(Corners[1], Corners[2]);
	for (auto &vertex : Corners)
	{
		vertex.normal = glm::vec3{Normal};
		Output.emplace_back(vertex);
	}
}

// a flat piece between four points going around it, seen from both its sides
struct sheet
{
	std::array<glm::dvec3, 4> points;
	std::array<glm::vec2, 4> texture;
	bool twosided{true};
};

void add_sheet(std::vector<world_vertex> &Output, sheet const &Sheet)
{
	auto normal{glm::cross(Sheet.points[1] - Sheet.points[0], Sheet.points[3] - Sheet.points[0])};
	if (glm::dot(normal, normal) < 1e-16)
		normal = glm::cross(Sheet.points[2] - Sheet.points[1], Sheet.points[3] - Sheet.points[1]);
	if (glm::dot(normal, normal) < 1e-16)
		return;
	normal = glm::normalize(normal);
	auto const corner = [&Sheet](std::size_t const Index) { return world_vertex{Sheet.points[Index], glm::vec3{0.f}, Sheet.texture[Index]}; };
	std::array const first{corner(0), corner(1), corner(2)};
	std::array const second{corner(0), corner(2), corner(3)};
	face(Output, first, normal);
	face(Output, second, normal);
	if (false == Sheet.twosided)
		return;
	face(Output, first, -normal);
	face(Output, second, -normal);
}

// cross-section of the rail at a distance from the start of the axis
struct ring
{
	std::array<glm::dvec3, rail_shape.size()> points;
	float along{0.f};
};

ring ring_at(layout const &Layout, double const Station)
{
	ring result;
	auto const frame{Layout.at(Station)};
	auto const out{outwards(Layout, frame)};
	auto const base{line_point(Layout, Station) + up * rail_middle(profile_at(Layout, Station).rise)};
	for (std::size_t idx = 0; idx < rail_shape.size(); ++idx)
		result.points[idx] = base + out * rail_shape[idx].depth + up * rail_shape[idx].height;
	result.along = static_cast<float>(Station / texture_length);
	return result;
}

void add_rail(std::vector<world_vertex> &Output, ring const &Previous, ring const &Current)
{
	for (std::size_t idx = 0; idx + 1 < rail_shape.size(); ++idx)
	{
		auto const top{static_cast<float>(idx) / static_cast<float>(rail_shape.size() - 1)};
		auto const bottom{static_cast<float>(idx + 1) / static_cast<float>(rail_shape.size() - 1)};
		sheet piece;
		piece.points = {Previous.points[idx], Previous.points[idx + 1], Current.points[idx + 1], Current.points[idx]};
		piece.texture = {glm::vec2{top, Previous.along}, glm::vec2{bottom, Previous.along}, glm::vec2{bottom, Current.along}, glm::vec2{top, Current.along}};
		add_sheet(Output, piece);
	}
}

// distances from the start of the axis the rail is cut at: closer together where it's led down or away
std::vector<double> rail_stations(layout const &Layout)
{
	std::vector<double> stations;
	auto const step{std::clamp(Layout.step, 0.5, 4.0)};
	auto station{Layout.from};
	while (station < Layout.to - 0.01 && stations.size() < 100000)
	{
		stations.emplace_back(station);
		auto const where{approach_at(Layout, station)};
		station += (where.start < 1.0 || where.end < 1.0 ? std::min(step, 1.0) : step);
	}
	stations.emplace_back(Layout.to);
	return stations;
}

// an upright box without a bottom, standing in the plane of the roadway
struct box
{
	glm::dvec3 centre; // of its footprint, at the height its sides start at
	glm::dvec3 along; // half of its size along the road
	glm::dvec3 across; // and across it
	double height{0.0};
	bool closed{false}; // it has a bottom as well
};

void add_box(std::vector<world_vertex> &Output, box const &Box)
{
	std::array const base{Box.centre - Box.along - Box.across, Box.centre + Box.along - Box.across, Box.centre + Box.along + Box.across, Box.centre - Box.along + Box.across};
	auto const rise{up * Box.height};
	auto const tall{static_cast<float>(Box.height / texture_length)};
	for (std::size_t idx = 0; idx < base.size(); ++idx)
	{
		auto const next{(idx + 1) % base.size()};
		sheet side;
		side.points = {base[idx], base[next], base[next] + rise, base[idx] + rise};
		side.texture = {glm::vec2{0.f, 0.f}, glm::vec2{0.1f, 0.f}, glm::vec2{0.1f, tall}, glm::vec2{0.f, tall}};
		side.twosided = false;
		// the sides look away from the middle of the box
		auto const outside{0.5 * (base[idx] + base[next]) - Box.centre};
		if (glm::dot(glm::cross(side.points[1] - side.points[0], side.points[3] - side.points[0]), outside) < 0.0)
			std::swap(side.points[1], side.points[3]);
		add_sheet(Output, side);
	}
	sheet lid;
	lid.points = {base[0] + rise, base[1] + rise, base[2] + rise, base[3] + rise};
	lid.texture = {glm::vec2{0.f, 0.f}, glm::vec2{0.1f, 0.f}, glm::vec2{0.1f, 0.1f}, glm::vec2{0.f, 0.1f}};
	lid.twosided = Box.closed;
	if (false == Box.closed && glm::cross(lid.points[1] - lid.points[0], lid.points[3] - lid.points[0]).y < 0.0)
		std::swap(lid.points[1], lid.points[3]);
	add_sheet(Output, lid);
	if (false == Box.closed)
		return;
	lid.points = {base[0], base[1], base[2], base[3]};
	add_sheet(Output, lid);
}

// a post behind the rail, with what holds the rail away from it
void add_post(std::vector<world_vertex> &Output, layout const &Layout, double const Station)
{
	auto const rise{profile_at(Layout, Station).rise};
	if (rise < shortest_rail)
		return;
	auto const frame{Layout.at(Station)};
	auto const out{outwards(Layout, frame)};
	glm::dvec3 along{frame.tangent.x, 0.0, frame.tangent.z};
	if (glm::dot(along, along) < 1e-12)
		return;
	along = glm::normalize(along);
	auto const foot{line_point(Layout, Station)};
	auto const middle{rail_middle(rise)};
	box post;
	post.centre = foot + out * (rail_depth + post_gap + 0.5 * post_width) - up * post_depth;
	post.along = along * (0.5 * post_length);
	post.across = out * (0.5 * post_width);
	post.height = post_depth + middle + 0.5 * rail_height;
	add_box(Output, post);
	box spacer;
	spacer.centre = foot + out * (rail_depth + 0.5 * post_gap) + up * (middle - 0.5 * spacer_height);
	spacer.along = along * (0.4 * post_length);
	spacer.across = out * (0.5 * post_gap);
	spacer.height = spacer_height;
	spacer.closed = true;
	add_box(Output, spacer);
}

void add_posts(std::vector<world_vertex> &Output, layout const &Layout)
{
	auto const length{Layout.to - Layout.from};
	auto const spans{std::max(1, static_cast<int>(std::lround(length / post_spacing)))};
	for (int span = 0; span <= spans; ++span)
	{
		if ((span == 0 && false == Layout.posted[0]) || (span == spans && false == Layout.posted[1]))
			continue;
		add_post(Output, Layout, Layout.from + length * span / spans);
	}
}

} // namespace

bool parse(std::string_view const Text, entry &Entry)
{
	auto const fields{fields_of(Text)};
	if (fields.size() != 10)
		return false;
	auto const where{index_of(side_names, fields[0])};
	auto const type{index_of(kind_names, fields[1])};
	auto const start{index_of(ending_names, fields[5])};
	auto const end{index_of(ending_names, fields[7])};
	if (false == where.has_value() || false == type.has_value() || false == start.has_value() || false == end.has_value() || fields[9].empty())
		return false;
	entry result;
	if (false == read_number(fields[2], result.offset) || false == read_number(fields[3], result.from) || false == read_number(fields[4], result.to) ||
	    false == read_number(fields[6], result.lengths[0]) || false == read_number(fields[8], result.lengths[1]))
		return false;
	result.where = static_cast<side>(*where);
	result.type = static_cast<kind>(*type);
	result.endings = {static_cast<ending>(*start), static_cast<ending>(*end)};
	result.material = std::string{fields[9]};
	confine(result);
	Entry = result;
	return true;
}

std::string format(entry const &Entry)
{
	return std::format("{}:{}:{}:{}:{}:{}:{}:{}:{}:{}", side_names[static_cast<std::size_t>(Entry.where)], kind_names[static_cast<std::size_t>(Entry.type)], Entry.offset, Entry.from, Entry.to,
	                   ending_names[static_cast<std::size_t>(Entry.endings[0])], Entry.lengths[0], ending_names[static_cast<std::size_t>(Entry.endings[1])], Entry.lengths[1],
	                   Entry.material.empty() ? std::string{"none"} : Entry.material);
}

double end_of(entry const &Entry, double const Length)
{
	return Entry.to < 0.f ? Length : std::min(static_cast<double>(Entry.to), Length);
}

void normalize(std::vector<entry> &Entries, double const Length)
{
	for (auto &item : Entries)
		confine(item);
	// without an axis there's nothing to fit the barriers to
	if (Length >= shortest)
	{
		for (auto &item : Entries)
			fit(item, Length);
		std::erase_if(Entries, [Length](entry const &Item) { return end_of(Item, Length) - Item.from < shortest; });
		std::ranges::stable_sort(Entries, ahead);
		merge(Entries, Length);
	}
	if (Entries.size() > most_entries)
		Entries.resize(most_entries);
}

std::vector<entry> part(std::vector<entry> const &Entries, double const Length, double const From, double const To)
{
	std::vector<entry> result;
	for (auto const &item : Entries)
	{
		auto const low{std::max(static_cast<double>(item.from), From)};
		auto const high{std::min(end_of(item, Length), To)};
		if (high - low < shortest)
			continue;
		auto cut{item};
		cut.from = (low - From <= snap ? 0.f : static_cast<float>(low - From));
		cut.to = (high >= To - snap ? -1.f : static_cast<float>(high - From));
		result.emplace_back(cut);
	}
	normalize(result, To - From);
	return result;
}

std::vector<entry> flipped(std::vector<entry> const &Entries, double const Length)
{
	std::vector<entry> result;
	for (auto const &item : Entries)
	{
		auto turned{item};
		if (item.where != side::middle)
			turned.where = (item.where == side::left ? side::right : side::left);
		auto const end{end_of(item, Length)};
		turned.from = (end >= Length - snap ? 0.f : static_cast<float>(Length - end));
		turned.to = (item.from <= snap ? -1.f : static_cast<float>(Length - item.from));
		turned.endings = {item.endings[1], item.endings[0]};
		turned.lengths = {item.lengths[1], item.lengths[0]};
		result.emplace_back(turned);
	}
	normalize(result, Length);
	return result;
}

std::vector<entry> joined(std::vector<entry> const &First, double const Firstlength, std::vector<entry> const &Second, double const Length)
{
	std::vector<entry> result;
	for (auto const &item : First)
	{
		auto kept{item};
		// what ended with the first piece ends where that piece did, unless the second one takes it further
		kept.to = static_cast<float>(end_of(item, Firstlength));
		result.emplace_back(kept);
	}
	for (auto const &item : Second)
	{
		auto moved{item};
		moved.from = static_cast<float>(item.from + Firstlength);
		if (item.to >= 0.f)
			moved.to = static_cast<float>(item.to + Firstlength);
		result.emplace_back(moved);
	}
	normalize(result, Length);
	return result;
}

profile profile_at(layout const &Layout, double const Station)
{
	auto const where{approach_at(Layout, Station)};
	auto const start{end_profile(Layout.ends[0].style, where.start, where.startlength)};
	auto const end{end_profile(Layout.ends[1].style, where.end, where.endlength)};
	profile result;
	result.rise = std::min(start.rise, end.rise);
	result.away = start.away + end.away;
	return result;
}

glm::dvec3 line_point(layout const &Layout, double const Station)
{
	auto const frame{Layout.at(Station)};
	auto const reach_{Layout.edge(Station) + Layout.offset};
	// the plane of the roadway carried on to where the barrier stands, then level from there to where it's led away
	return frame.position + frame.left * (Layout.onleft ? reach_ : -reach_) + outwards(Layout, frame) * profile_at(Layout, Station).away;
}

void build(layout const &Layout, mesh &Mesh)
{
	if (Layout.type != kind::wbeam || false == static_cast<bool>(Layout.at) || false == static_cast<bool>(Layout.edge))
		return;
	if (false == std::isfinite(Layout.from) || false == std::isfinite(Layout.to) || Layout.to - Layout.from < shortest)
		return;
	auto const stations{rail_stations(Layout)};
	auto previous{ring_at(Layout, stations.front())};
	for (std::size_t idx = 1; idx < stations.size(); ++idx)
	{
		auto const current{ring_at(Layout, stations[idx])};
		add_rail(Mesh.rail, previous, current);
		previous = current;
	}
	add_posts(Mesh.posts, Layout);
}

} // namespace road_barrier
