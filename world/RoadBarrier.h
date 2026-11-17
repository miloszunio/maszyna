/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "model/vertex.h"

// safety barriers standing along a road. a piece of a road knows only the barriers it has itself: where along its axis each
// of them starts and ends. whether a barrier carries on along the next piece isn't written down anywhere, it's found out when
// the shapes are made, by the ends of the lines the barriers stand on meeting or not. so a piece taken out of the middle of
// a barrier leaves two barriers with proper ends, and putting it back makes them one again, with nothing to keep in step.
// scenery entry, among the properties of a road:
// barrier <side>:<kind>:<offset>:<from>:<to>:<start>:<length>:<end>:<length>:<material>
// side: left, right (facing along the axis), middle (between the directions of a road which keeps them apart),
// kind: wbeam (a steel rail on posts, facing the road), wbeam2 (rails on both sides of the posts), concrete,
// offset: m from the edge of the lanes out to the face of the rail, from, to: m along the axis, to below 0: up to its end,
// start, end: what the barrier is like where it really ends: ramp (led down into the ground), flare (the same, and led away
// from the road, 1 m for each 20), cap (cut short, bent away), each with the length it takes,
// material: none for the plain colour of galvanised steel.
// it's a single token past the keyword, so a release which doesn't know it reads on in step
namespace road_barrier
{

enum class side
{
	left,
	right,
	middle
};
enum class kind
{
	wbeam,
	wbeam2,
	concrete
};
enum class ending
{
	ramp,
	flare,
	cap
};

struct entry
{
	side where{side::right};
	kind type{kind::wbeam};
	float offset{1.f};
	float from{0.f};
	float to{-1.f};
	std::array<ending, 2> endings{ending::flare, ending::flare}; // at the start and at the end
	std::array<float, 2> lengths{12.f, 12.f};
	std::string material{"none"};

	bool operator==(entry const &Other) const = default;
};

// dimensions, metres
constexpr double rail_top{0.75}; // of the top of the rail over the plane of the roadway
constexpr double rail_height{0.312};
constexpr double rail_depth{0.083};
constexpr double post_spacing{2.0};
constexpr double flare_rate{20.0}; // metres along the road for a metre away from it
constexpr double reach{32.0}; // how far along a barrier its end is looked for; past that it doesn't matter where it is
constexpr double snap{0.25}; // a barrier this close to an end of the axis starts or ends at that end
constexpr double shortest{0.5};
constexpr double meeting{0.3}; // ends of the lines of two barriers this close are one barrier going on

// reads an entry from the text it's kept as in a scenery. returns: false if it isn't one
bool parse(std::string_view const Text, entry &Entry);
// the text an entry is kept as
std::string format(entry const &Entry);
// brings the barriers of a piece to the form they're kept in: within the piece and within their limits, the ones which touch
// made one, in a set order. doing it again changes nothing. Length: of the axis, 0 if it isn't known
void normalize(std::vector<entry> &Entries, double const Length);
// where a barrier ends along the axis
double end_of(entry const &Entry, double const Length);
// the barriers of a part of a piece, between two distances from the start of its axis
std::vector<entry> part(std::vector<entry> const &Entries, double const Length, double const From, double const To);
// the same barriers for the axis laid out the other way
std::vector<entry> flipped(std::vector<entry> const &Entries, double const Length);
// the barriers of two pieces made one, the second going on from the end of the first. Length: of the axis of that piece
std::vector<entry> joined(std::vector<entry> const &First, double const Firstlength, std::vector<entry> const &Second, double const Length);

// cross-section of the roadway
struct frame
{
	glm::dvec3 position{0.0}; // on the axis
	glm::dvec3 tangent{0.0, 0.0, 1.0};
	glm::dvec3 left{1.0, 0.0, 0.0}; // across the roadway, in its plane
};
// what's known of an end of a barrier
struct end_data
{
	bool distant{false}; // it's further than anyone would look
	double beyond{0.0}; // how much of the barrier lies past the piece on the way to it
	ending style{ending::flare};
	double length{12.0};
};
// a barrier of a piece, with what it takes to make its shape
struct layout
{
	kind type{kind::wbeam};
	bool onleft{false};
	double offset{1.0};
	double from{0.0};
	double to{0.0};
	std::array<end_data, 2> ends;
	std::array<bool, 2> posted{true, true}; // a post stands at that end of the range; where the barrier goes on the next piece puts it
	double step{2.0}; // longest piece of the rail
	std::function<frame(double)> at; // cross-section at a distance from the start of the axis
	std::function<double(double)> edge; // distance from the axis to the edge of the lanes there
};
struct mesh
{
	std::vector<world_vertex> rail;
	std::vector<world_vertex> posts;
};
// how much of its height the barrier has at a distance from the start of the axis, 0..1, and how far it's led away from the road there
struct profile
{
	double rise{1.0};
	double away{0.0};
};
profile profile_at(layout const &Layout, double const Station);
// the line the face of the rail follows: the point of it at a distance from the start of the axis, in the plane of the roadway
glm::dvec3 line_point(layout const &Layout, double const Station);
// adds the triangles of the barrier to the mesh
void build(layout const &Layout, mesh &Mesh);

} // namespace road_barrier
