/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// The tracks designed from the bends of a line of a map (GeoTools, Tracks): the bends of a stretch (editor/editorRailData)
// made a line of straights, transition curves and arcs by the alignment of the track editor, with the cants and the
// transitions of the norms. A curve too sharp for the speed of the line gets the speed it allows, as the lines have their
// local limits, and the cant and the transitions of that speed. Then the axes of two tracks either side of the line.
// The plan here is the one of the scenery, x and z.

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "editor/editorAlignment.hpp"
#include "world/Segment.h"

namespace rail_design
{

// a stretch of the line where it's slower than the line: a curve with its transitions
struct speed_limit
{
	double from{0.0}; // chainages
	double to{0.0};
	double speed{0.0}; // km/h
};

struct plan
{
	alignment::design design;
	alignment::result shape; // not valid: no line through the bends, Error says why
	std::vector<speed_limit> limits;
	std::size_t shortened{0}; // curves whose transitions had no room for the length of the norms
	std::string error;
};

// Bends: the two ends and the bends between them; Speed: of the line, km/h
plan design_of(std::vector<glm::dvec2> Bends, double Speed, alignment::limits const &Norms, alignment::transition_shape Shape);

// how far the points the line was made of lie from it, m
struct deviation
{
	double rms{0.0};
	double worst{0.0};
};
deviation deviation_of(alignment::result const &Shape, std::vector<glm::dvec2> const &Points);

// the pieces of the line, each about Length long
std::vector<segment_data> pieces_of(plan const &Plan, double Length);
// the speed of each piece: the one of the line, or of the curve it's a part of
std::vector<double> speeds_of(plan const &Plan, std::vector<segment_data> const &Pieces, double Speed);

// the pieces of a track running beside the pieces given, which follow one another: Offset m to their left, to their right
// when negative, at their heights and with their cants. Empty where a curve is sharper than the offset
std::vector<segment_data> beside(std::vector<segment_data> const &Pieces, double Offset);

} // namespace rail_design
