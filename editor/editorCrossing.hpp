/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// A structure carrying a track over a road where the road gets through the embankment of the track: the model and what
// the terrain around it needs, engine-free so it can be made and looked at outside the editor.
// The kinds are those of the Polish lines, by what they're made of and where the track lies:
//  - a pipe of corrugated steel, its ends cut along the slope of the embankment, a ring of concrete round them;
//  - a box culvert of reinforced concrete: a headwall at each end and wings splayed out along the slopes;
//  - a masonry arch: the ring of the arch standing out of the spandrel walls, a cornice, a parapet wall;
//  - a frame of reinforced concrete: the slab and the walls in one, the corners haunched, an edge beam and a railing;
//  - a slab of encased steel beams on abutments: thin, on a seat, the joint over the abutment showing;
//  - a deck plate girder bridge: the girders under the ballast trough, stiffened and braced, on bearings;
//  - a through plate girder bridge: the girders beside the track, the trough on cross beams between them;
//  - a through truss: the trusses beside the track, braced over it when the span is long, a U-frame when it's short.
// The track and its ballast are the editor's: the structure ends at the formation, the bottom of the ballast.
// The terrain: wherever the structure stands, the ground is either over it (raised to cover it) or cut out (a hole), and
// every edge of a hole lies inside a solid part of the structure (a wall, a wing, a headwall), never in the open.

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "editor/editorStructure.hpp"

namespace crossing
{

enum class kind : int
{
	pipe,
	box,
	arch,
	frame,
	slab,
	deck_girders,
	through_girders,
	truss,
	count
};

// where the road meets the formation of a track
struct site
{
	glm::dvec3 at{0.0}; // on the axis of the road
	glm::dvec2 road{1.0, 0.0}; // its way seen from above, unit
	double grade{0.0}; // of the road along that way
	glm::dvec2 track{0.0, 1.0}; // the way of the track, unit
	double formation{0.0}; // height of the formation of the track there
	double half_width{3.5}; // of the formation
	double slope{1.5}; // run of its slopes for a metre of height
	double width{7.0}; // of the road with what's beside it
	// the axes of the tracks on the formation, their distances from At across the track (the way of perpendicular(track)):
	// one track at its middle, or two and more side by side
	std::vector<double> axes{0.0};
	// the height of the terrain as it is at a point of the plan; false where there's none
	std::function<bool(glm::dvec2 const &, double &)> ground;
	// the ground of the land before the embankments and the roads shaped it; none: level with the road
	std::function<bool(glm::dvec2 const &, double &)> natural;
	// the cutting of the road through the land: flat this far from its axis, its slopes rising a metre over this run beyond
	double bench{4.0};
	double bank{1.5};
};

struct options
{
	kind type{kind::box};
	double clearance{4.7}; // m over the road to the soffit, the crown, or the lowest steel
	double width{0.0}; // of the opening between the walls; 0: the road with what's beside it
	int spans{1};
	bool fit{true}; // a lower clearance, down to the least, where the one asked for has no room
	int parapet{0}; // 0 by the kind, 1 nothing, 2 a railing, 3 a wall
	double tile{2.0}; // m the texture repeats
	std::string material; // of the masonry or the concrete
	std::string steel; // of the steel; empty: plain colour
};

// a piece of the terrain to be raised to a height at least, to cover the structure
struct raise_area
{
	std::vector<glm::dvec2> outline;
	double height{0.0};
};

enum class refusal
{
	none,
	along_track, // the road runs along the track there, under 20 degrees
	too_low // the formation is too low over the road for the structure with the least clearance
};

struct result
{
	bool built{false};
	refusal refused{refusal::none};
	double room{0.0}; // m from the road up to the formation, less what the structure takes under it: the clearance there's room for
	double short_by{0.0}; // m the road would have to go down for the least clearance, when refused for want of height
	double clearance{0.0}; // as built
	bool lowered{false}; // built with a lower clearance than asked for
	structure::model mesh;
	std::vector<std::vector<glm::dvec2>> holes; // cut out of the terrain, each edge inside a solid part
	std::vector<raise_area> raise;
	double length{0.0}; // along the road, from the end of one wing to the end of the other
};

// m of the structure under the formation over its soffit for a span of that length along the track: how much higher
// than the clearance the formation has to be over the road
double depth_under(kind Type, double Span);
// the least clearance a structure is built with where the one asked for has no room
double constexpr kLeastClearance{2.5};

result build(site const &Site, options const &Options);

// a cell of the grid of the terrain cut out whole under a hole: from Low, Size wide, the heights of its corners (x0, z0) (x1, z0)
// (x0, z1) (x1, z1); the terrain cuts it along the diagonal from (x0, z1) to (x1, z0)
struct ground_cell
{
	glm::dvec2 low{0.0};
	double size{2.0};
	std::array<double, 4> heights{};
};
// the ground of the cells where the holes (convex) don't reach: the terrain's own triangles, on the same planes, cut along the
// edges of the holes; three points a triangle, counter-clockwise seen from above. Laid into the cells cut out whole, it meets
// the terrain around edge to edge and the structure inside the holes
// a triangle of a picture of a structure: its corners and its colour (0..1)
struct shaded_triangle
{
	std::array<glm::dvec3, 3> corners;
	glm::dvec3 colour{0.7};
};
// the structure of the options as it's built on an embankment 7 m high over a level road crossing it at 80 degrees, with the
// terrain round it cut and laid as the editor does it and the track on top: what the list of the kinds shows beside it
std::vector<shaded_triangle> preview(options const &Options);

std::vector<glm::dvec3> ground_outside(std::vector<ground_cell> const &Cells, std::vector<std::vector<glm::dvec2>> const &Holes);

} // namespace crossing
