/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "editor/editorRailData.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <numbers>
#include <optional>
#include <utility>

#include <nlohmann/json.hpp>

namespace rail_data
{

namespace
{

using json = nlohmann::json;
using polyline = std::vector<glm::dvec2>;

double const kSamePoint{0.05}; // m, points nearer than this are one
double const kSpikeAngle{30.0}; // degrees, a sharper bend by a short side is an artefact of the drawing
double const kSpikeSide{30.0}; // m
double const kTurnBack{150.0}; // degrees, a bend this sharp is one whatever its sides

double number_of(json const &Properties, char const *Key, double const Default)
{
	auto const found{Properties.find(Key)};
	if (found == Properties.end())
		return Default;
	if (found->is_number())
		return found->get<double>();
	if (found->is_string())
	{
		auto const &text{found->get_ref<std::string const &>()};
		double value{Default};
		auto const read{std::from_chars(text.data(), text.data() + text.size(), value)};
		return read.ec == std::errc{} ? value : Default;
	}
	return Default;
}

std::string text_of(json const &Properties, char const *Key)
{
	auto const found{Properties.find(Key)};
	if (found == Properties.end())
		return {};
	if (found->is_string())
		return found->get<std::string>();
	if (found->is_number_integer())
		return std::to_string(found->get<long long>());
	return {};
}

polyline points_of(json const &Coordinates)
{
	polyline points;
	if (false == Coordinates.is_array())
		return points;
	for (auto const &pair : Coordinates)
		if (pair.is_array() && pair.size() >= 2 && pair[0].is_number() && pair[1].is_number())
			points.emplace_back(pair[0].get<double>(), pair[1].get<double>());
	return points;
}

// Part put at the end of Line or before its start, turned round where that brings their ends nearer
void join_part(polyline &Line, polyline Part)
{
	auto const end_to_start{glm::distance(Line.back(), Part.front())};
	auto const end_to_end{glm::distance(Line.back(), Part.back())};
	auto const start_to_end{glm::distance(Line.front(), Part.back())};
	auto const start_to_start{glm::distance(Line.front(), Part.front())};
	auto const nearest{std::min({end_to_start, end_to_end, start_to_end, start_to_start})};
	auto const after{nearest == end_to_start || nearest == end_to_end};
	if (nearest == end_to_end || nearest == start_to_start)
		std::ranges::reverse(Part);
	if (false == after)
		std::swap(Line, Part);
	// Part goes on from Line now
	auto const from{glm::distance(Line.back(), Part.front()) < kSamePoint ? std::size_t{1} : std::size_t{0}};
	Line.insert(Line.end(), Part.begin() + static_cast<std::ptrdiff_t>(from), Part.end());
}

// the parts of a line of the map as one line
polyline line_of(json const &Geometry)
{
	auto const type{Geometry.value("type", std::string{})};
	auto const coordinates{Geometry.find("coordinates")};
	if (coordinates == Geometry.end())
		return {};
	if (type == "LineString")
		return points_of(*coordinates);
	polyline line;
	if (type != "MultiLineString" || false == coordinates->is_array())
		return line;
	for (auto const &part : *coordinates)
	{
		auto points{points_of(part)};
		if (points.size() < 2)
			continue;
		if (line.empty())
			line = std::move(points);
		else
			join_part(line, std::move(points));
	}
	return line;
}

void read_line(json const &Feature, json const &Properties, dataset &Data)
{
	section read;
	read.points = line_of(Feature.at("geometry"));
	if (read.points.size() < 2)
		return;
	read.about.line = text_of(Properties, "LN");
	read.about.km_start = number_of(Properties, "km_s", 0.0);
	read.about.km_end = number_of(Properties, "km_e", 0.0);
	read.about.speed = number_of(Properties, "v_k", 0.0);
	read.about.tracks = static_cast<int>(number_of(Properties, "NT", 1.0));
	read.about.electrified = static_cast<int>(number_of(Properties, "EL", 0.0));
	Data.sections.push_back(std::move(read));
}

void read_station(json const &Feature, json const &Properties, dataset &Data)
{
	auto const &geometry{Feature.at("geometry")};
	auto const coordinates{geometry.find("coordinates")};
	if (coordinates == geometry.end() || false == coordinates->is_array() || coordinates->size() < 2)
		return;
	station read;
	read.name = text_of(Properties, "SN");
	read.type = text_of(Properties, "TYPE");
	read.at = {(*coordinates)[0].get<double>(), (*coordinates)[1].get<double>()};
	Data.stations.push_back(std::move(read));
}

void read_feature(json const &Feature, dataset &Data)
{
	if (false == Feature.contains("geometry") || false == Feature.at("geometry").is_object())
		return;
	auto const properties = Feature.value("properties", json::object());
	auto const type{Feature.at("geometry").value("type", std::string{})};
	if (type == "Point")
		read_station(Feature, properties, Data);
	else
		read_line(Feature, properties, Data);
}

// degrees the line turns by at each point between its ends
std::vector<double> turns_of(polyline const &Points)
{
	std::vector<double> turns(Points.size(), 0.0);
	for (std::size_t i = 1; i + 1 < Points.size(); ++i)
	{
		auto const in{Points[i] - Points[i - 1]};
		auto const out{Points[i + 1] - Points[i]};
		turns[i] = std::abs(std::atan2(in.x * out.y - in.y * out.x, glm::dot(in, out))) * 180.0 / std::numbers::pi;
	}
	return turns;
}

// the point which is the worst artefact of the drawing, 0 where there's none: a sharp bend by a short side, the whiskers left
// where the sections of the map meet, or the line turning back
std::size_t worst_spike(polyline const &Points)
{
	auto const turns{turns_of(Points)};
	std::size_t worst{0};
	for (std::size_t i = 1; i + 1 < Points.size(); ++i)
	{
		auto const shortside{std::min(glm::distance(Points[i], Points[i - 1]), glm::distance(Points[i + 1], Points[i])) < kSpikeSide};
		auto const spike{(turns[i] > kSpikeAngle && shortside) || turns[i] > kTurnBack};
		if (spike && (worst == 0 || turns[i] > turns[worst]))
			worst = i;
	}
	return worst;
}

void remove_spikes(chain &Chain)
{
	while (Chain.points.size() > 2)
	{
		auto const spike{worst_spike(Chain.points)};
		if (spike == 0)
			return;
		Chain.points.erase(Chain.points.begin() + static_cast<std::ptrdiff_t>(spike));
		Chain.section_of.erase(Chain.section_of.begin() + static_cast<std::ptrdiff_t>(spike));
		++Chain.spikes;
	}
}

// the section put at the end of the chain if it goes on from it; the first section of the chain is turned round where the
// next one goes on from its start
bool extend(chain &Chain, section const &Next, double const Tolerance)
{
	auto points{Next.points};
	auto const single{Chain.sections.size() == 1};
	auto const ahead{std::min(glm::distance(Chain.points.back(), points.front()), glm::distance(Chain.points.back(), points.back()))};
	auto const behind{std::min(glm::distance(Chain.points.front(), points.front()), glm::distance(Chain.points.front(), points.back()))};
	if (single && behind < ahead && behind <= Tolerance)
		std::ranges::reverse(Chain.points);
	if (glm::distance(Chain.points.back(), points.back()) < glm::distance(Chain.points.back(), points.front()))
		std::ranges::reverse(points);
	if (glm::distance(Chain.points.back(), points.front()) > Tolerance)
		return false;
	auto const index{Chain.sections.size()};
	Chain.sections.push_back(Next.about);
	for (std::size_t i = 1; i < points.size(); ++i)
	{
		if (glm::distance(points[i], Chain.points.back()) <= kSamePoint)
			continue;
		Chain.points.push_back(points[i]);
		Chain.section_of.push_back(index);
	}
	return true;
}

chain chain_from(section const &First)
{
	chain made;
	made.sections.push_back(First.about);
	for (auto const &point : First.points)
		if (made.points.empty() || glm::distance(point, made.points.back()) > kSamePoint)
		{
			made.points.push_back(point);
			made.section_of.push_back(0);
		}
	return made;
}

std::string lowercase_name(std::string const &Line)
{
	std::string name{"l"};
	for (auto const letter : Line)
		name += std::isalnum(static_cast<unsigned char>(letter)) != 0 ? static_cast<char>(std::tolower(static_cast<unsigned char>(letter))) : '_';
	return name;
}

// the chains of one line, its sections in the order of their kilometres
void chains_of_line(std::vector<section const *> Sections, double const Tolerance, std::vector<chain> &Chains)
{
	std::ranges::sort(Sections, [](section const *Left, section const *Right) {
		return std::pair{Left->about.km_start, Left->about.km_end} < std::pair{Right->about.km_start, Right->about.km_end};
	});
	auto const base{lowercase_name(Sections.front()->about.line)};
	std::vector<chain> made;
	for (auto const *next : Sections)
		if (made.empty() || false == extend(made.back(), *next, Tolerance))
			made.push_back(chain_from(*next));
	std::size_t number{0};
	for (auto &one : made)
	{
		remove_spikes(one);
		if (one.points.size() < 2)
			continue;
		++number;
		one.name = number == 1 ? base : base + "_" + std::to_string(number);
		Chains.push_back(std::move(one));
	}
}

// the distance along the line to each of its points
std::vector<double> lengths_along(polyline const &Points)
{
	std::vector<double> along(Points.size(), 0.0);
	for (std::size_t i = 1; i < Points.size(); ++i)
		along[i] = along[i - 1] + glm::distance(Points[i], Points[i - 1]);
	return along;
}

// a run of the straights between the bends of a chain which the map gives one speed and one number of tracks
struct run
{
	std::size_t first{0}; // legs
	std::size_t last{0};
	std::size_t section{0}; // the one its speed and its tracks are of
	double speed{0.0};
	bool pair{false};
	double length{0.0};
	bool same_kind(run const &Other) const { return speed == Other.speed && pair == Other.pair; }
};

// what's needed of a chain to cut it into stretches
struct legs
{
	chain const *source{nullptr};
	std::vector<std::size_t> kept; // the points which are bends, and the two ends
	std::vector<double> along; // of the points of the chain
	std::size_t count() const { return kept.size() - 1; }
	double length(std::size_t const Leg) const { return along[kept[Leg + 1]] - along[kept[Leg]]; }
	double middle(std::size_t const Leg) const { return 0.5 * (along[kept[Leg + 1]] + along[kept[Leg]]); }
	// the section the map gives for the middle of the leg
	std::size_t section(std::size_t const Leg) const
	{
		auto const at{std::ranges::lower_bound(along, middle(Leg))};
		auto const point{std::min(static_cast<std::size_t>(at - along.begin()), along.size() - 1)};
		return source->section_of[point];
	}
};

std::vector<run> runs_of(legs const &Legs, double const DefaultSpeed)
{
	std::vector<run> runs;
	for (std::size_t leg = 0; leg < Legs.count(); ++leg)
	{
		run one;
		one.first = leg;
		one.last = leg;
		one.section = Legs.section(leg);
		auto const &about{Legs.source->sections[one.section]};
		one.speed = about.speed > 0.0 ? about.speed : DefaultSpeed;
		one.pair = about.tracks >= 2;
		one.length = Legs.length(leg);
		if (false == runs.empty() && runs.back().same_kind(one))
		{
			runs.back().last = leg;
			runs.back().length += one.length;
		}
		else
			runs.push_back(one);
	}
	return runs;
}

// the run joined to the neighbour given, which keeps its speed and its tracks
void absorb(std::vector<run> &Runs, std::size_t const Short, std::size_t const Into)
{
	auto &kept{Runs[Into]};
	kept.first = std::min(kept.first, Runs[Short].first);
	kept.last = std::max(kept.last, Runs[Short].last);
	kept.length += Runs[Short].length;
	Runs.erase(Runs.begin() + static_cast<std::ptrdiff_t>(Short));
}

// the neighbours of one kind made one run
void coalesce(std::vector<run> &Runs)
{
	for (std::size_t i = 0; i + 1 < Runs.size();)
	{
		if (Runs[i].same_kind(Runs[i + 1]))
			absorb(Runs, i + 1, i);
		else
			++i;
	}
}

// the runs too short to be a stretch of their own go with their longer neighbour: the speed of the map changes at a
// kilometre, the tracks need room for what they change with
void merge_short(std::vector<run> &Runs, double const Shortest)
{
	while (Runs.size() > 1)
	{
		auto const shortest{std::ranges::min_element(Runs, {}, &run::length)};
		if (shortest->length >= Shortest)
			return;
		auto const index{static_cast<std::size_t>(shortest - Runs.begin())};
		auto const before{index > 0};
		auto const after{index + 1 < Runs.size()};
		auto const into{before && (false == after || Runs[index - 1].length >= Runs[index + 1].length) ? index - 1 : index + 1};
		absorb(Runs, index, into);
		coalesce(Runs);
	}
}

// a place a chain is cut at: on the straight of a leg, Share of its length from its start
struct cut
{
	std::size_t leg{0};
	double share{0.5};
};

double const kCutRoom{500.0}; // m, left on the leg cut either side of the cut, for the curves of its ends; half of a shorter leg

// where the chain is cut for the run beginning with leg First: on the longest leg reaching within Reach of the bend the run
// begins at, past the leg cut before, as near that bend as the room for the curves lets. None where there's no such leg
std::optional<cut> cut_for(legs const &Legs, std::size_t const First, std::size_t const Lowest, double const Reach)
{
	auto const bend{Legs.along[Legs.kept[First]]};
	std::optional<std::size_t> best;
	for (auto leg{Lowest}; leg < Legs.count(); ++leg)
	{
		if (Legs.along[Legs.kept[leg]] > bend + Reach)
			break;
		auto const reaches{Legs.along[Legs.kept[leg + 1]] >= bend - Reach};
		if (reaches && (false == best.has_value() || Legs.length(leg) > Legs.length(*best)))
			best = leg;
	}
	if (false == best.has_value())
		return std::nullopt;
	auto const length{Legs.length(*best)};
	auto const start{Legs.along[Legs.kept[*best]]};
	auto const room{std::min(0.5 * length, kCutRoom)};
	auto const at{std::clamp(bend, start + room, start + length - room)};
	return cut{*best, length > 0.0 ? (at - start) / length : 0.5};
}

glm::dvec2 point_of(legs const &Legs, cut const &Cut)
{
	auto const &points{Legs.source->points};
	return glm::mix(points[Legs.kept[Cut.leg]], points[Legs.kept[Cut.leg + 1]], Cut.share);
}

// where a stretch begins and where it ends: in the middle of a leg, or at an end of the chain (no leg)
struct span
{
	std::optional<cut> from;
	std::optional<cut> to;
	run of;
};

stretch stretch_of(legs const &Legs, span const &Span, std::size_t const Number)
{
	auto const &source{*Legs.source};
	stretch made;
	std::array<char, 32> suffix{};
	std::snprintf(suffix.data(), suffix.size(), "_s%02zu", Number);
	made.name = source.name + suffix.data();
	made.about = source.sections[Span.of.section];
	made.speed = Span.of.speed;
	made.about.tracks = Span.of.pair ? std::max(2, made.about.tracks) : 1;
	auto const firstbend{Span.from.has_value() ? Span.from->leg + 1 : std::size_t{0}};
	auto const lastbend{Span.to.has_value() ? Span.to->leg : Legs.count()};
	if (Span.from.has_value())
		made.bends.push_back(point_of(Legs, *Span.from));
	for (auto bend{firstbend}; bend <= lastbend; ++bend)
		made.bends.push_back(source.points[Legs.kept[bend]]);
	if (Span.to.has_value())
		made.bends.push_back(point_of(Legs, *Span.to));
	made.first = Legs.kept[Span.from.has_value() ? Span.from->leg : std::size_t{0}];
	made.last = Legs.kept[Span.to.has_value() ? Span.to->leg + 1 : Legs.count()];
	made.about.km_start = source.sections[source.section_of[made.first]].km_start;
	made.about.km_end = source.sections[source.section_of[made.last]].km_end;
	return made;
}

// the runs as spans: each change of the kind moved to the middle of a straight near it; a change with no straight to be cut
// on is left out, the run after it going with the one before
std::vector<span> spans_of(legs const &Legs, std::vector<run> const &Runs, double const Reach)
{
	std::vector<span> spans;
	spans.push_back({std::nullopt, std::nullopt, Runs.front()});
	std::size_t lowest{0};
	for (std::size_t i = 1; i < Runs.size(); ++i)
	{
		auto const place{cut_for(Legs, Runs[i].first, lowest, Reach)};
		if (false == place.has_value())
			continue;
		spans.back().to = place;
		spans.push_back({place, std::nullopt, Runs[i]});
		lowest = place->leg + 1;
	}
	return spans;
}

// the leg a span of too many bends is cut on next, past the leg From: the longest of the legs after its bends 'Least' to
// 'Most', the first straight after them where none of those is one. None when the span ends before
std::optional<std::size_t> block_leg(legs const &Legs, std::size_t const From, std::size_t const End, stretch_settings const &Settings)
{
	auto const least{From + std::max<std::size_t>(2, Settings.bends / 2)};
	auto const most{From + std::max<std::size_t>(3, Settings.bends)};
	if (most >= End)
		return std::nullopt;
	auto best{least};
	for (auto leg{least}; leg <= most; ++leg)
		if (Legs.length(leg) > Legs.length(best))
			best = leg;
	while (Legs.length(best) < Settings.straight && best + 1 < End)
		++best;
	return best + 1 < End ? std::optional{best} : std::nullopt;
}

// the spans of more bends than are designed together cut on their straights, in their middles
std::vector<span> blocks_of(legs const &Legs, std::vector<span> const &Spans, stretch_settings const &Settings)
{
	std::vector<span> blocks;
	for (auto const &whole : Spans)
	{
		auto rest{whole};
		auto const end{whole.to.has_value() ? whole.to->leg : Legs.count()};
		auto from{whole.from.has_value() ? whole.from->leg : std::size_t{0}};
		while (auto const leg{block_leg(Legs, from, end, Settings)})
		{
			auto part{rest};
			part.to = cut{*leg, 0.5};
			blocks.push_back(part);
			rest.from = part.to;
			from = *leg;
		}
		blocks.push_back(rest);
	}
	return blocks;
}

} // namespace

bool read_geojson(std::string_view const Text, dataset &Data, std::string &Error)
{
	try
	{
		auto const document = json::parse(Text); // not braces: they would make a list of it
		auto const crs{document.contains("crs") ? document.at("crs").dump() : std::string{}};
		if (crs.find("2180") == std::string::npos)
		{
			Error = "the file isn't in PUWG 1992 (EPSG:2180)";
			return false;
		}
		auto const features{document.find("features")};
		if (features == document.end() || false == features->is_array())
		{
			Error = "the file has no features";
			return false;
		}
		for (auto const &feature : *features)
			read_feature(feature, Data);
		return true;
	}
	catch (json::exception const &problem)
	{
		Error = problem.what();
		return false;
	}
}

std::vector<chain> chains_of(dataset const &Data, double const JoinTolerance)
{
	std::map<std::string, std::vector<section const *>, std::less<>> lines;
	for (auto const &one : Data.sections)
		lines[one.about.line].push_back(&one);
	std::vector<chain> chains;
	for (auto const &[line, sections] : lines)
		chains_of_line(sections, JoinTolerance, chains);
	return chains;
}

std::vector<std::size_t> simplified(std::vector<glm::dvec2> const &Points, double const Tolerance)
{
	if (Points.size() < 3)
	{
		std::vector<std::size_t> all(Points.size());
		for (std::size_t i = 0; i < all.size(); ++i)
			all[i] = i;
		return all;
	}
	std::vector<char> keep(Points.size(), 0);
	keep.front() = 1;
	keep.back() = 1;
	std::vector<std::pair<std::size_t, std::size_t>> stack{{0, Points.size() - 1}};
	while (false == stack.empty())
	{
		auto const [from, to]{stack.back()};
		stack.pop_back();
		auto const chord{Points[to] - Points[from]};
		auto const length{glm::length(chord)};
		double farthest{0.0};
		std::size_t at{from};
		for (auto i{from + 1}; i < to; ++i)
		{
			auto const offset{Points[i] - Points[from]};
			auto const away{length < 1e-9 ? glm::length(offset) : std::abs(offset.x * chord.y - offset.y * chord.x) / length};
			if (away > farthest)
			{
				farthest = away;
				at = i;
			}
		}
		if (farthest <= Tolerance)
			continue;
		keep[at] = 1;
		stack.emplace_back(from, at);
		stack.emplace_back(at, to);
	}
	std::vector<std::size_t> kept;
	for (std::size_t i = 0; i < keep.size(); ++i)
		if (keep[i] != 0)
			kept.push_back(i);
	return kept;
}

std::vector<stretch> stretches_of(chain const &Chain, stretch_settings const &Settings)
{
	std::vector<stretch> stretches;
	if (Chain.points.size() < 2)
		return stretches;
	legs straights;
	straights.source = &Chain;
	straights.kept = simplified(Chain.points, Settings.simplify);
	straights.along = lengths_along(Chain.points);
	auto runs{runs_of(straights, Settings.default_speed)};
	merge_short(runs, Settings.shortest);
	auto const spans{blocks_of(straights, spans_of(straights, runs, Settings.reach), Settings)};
	for (std::size_t i = 0; i < spans.size(); ++i)
		stretches.push_back(stretch_of(straights, spans[i], i + 1));
	return stretches;
}

} // namespace rail_data
