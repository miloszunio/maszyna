/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// The railway lines of a map as data (GeoTools, Tracks): the axes of the lines with what the map says of them, read from a
// GeoJSON file in PUWG 1992 (EPSG:2180). The first source is the railway map of the Lower Silesia geoportal (IRT: Instytut
// Rozwoju Terytorialnego, free with the source named), whose file holds the axis of each line in sections, each with the number
// of the line (LN), its kilometres (km_s, km_e), its speed (v_k), the number of its tracks (NT) and its electrification (EL),
// and the stations as points (SN, TYPE).
// The axes are of the lines, not of the tracks, drawn for a map: a point every 50 m or so, in places every 2 km. What's made
// of them here is what the tracks are designed from: the sections of a line joined into chains, the artefacts of the drawing
// taken out, and each chain cut into stretches of one speed and one number of tracks, each a row of bends of straights.
// Nothing here knows the scenery: the points stay in PUWG 1992, east and north.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace rail_data
{

// what the map says of a section of a line
struct attributes
{
	std::string line; // its number, 132
	double km_start{0.0};
	double km_end{0.0};
	double speed{0.0}; // km/h, 0 where the map gives none (a line out of use)
	int tracks{1};
	int electrified{0}; // as the map codes it, 0 not
};

struct section
{
	attributes about;
	std::vector<glm::dvec2> points; // east, north
};

struct station
{
	std::string name;
	std::string type; // ST a station, PO a stop, as the map has it
	glm::dvec2 at{0.0};
};

struct dataset
{
	std::vector<section> sections;
	std::vector<station> stations;
};

// the features of the GeoJSON text added to the data; false with the reason where it isn't one in EPSG:2180
bool read_geojson(std::string_view Text, dataset &Data, std::string &Error);

// the sections of a line joined end to end in the order of their kilometres
struct chain
{
	std::string name; // l132, or l132_2 for the second chain of the line
	std::vector<attributes> sections;
	std::vector<glm::dvec2> points;
	std::vector<std::size_t> section_of; // of each point, the section it came with
	std::size_t spikes{0}; // artefacts of the drawing taken out
};

// JoinTolerance: m, how far apart the ends of two sections of a line may lie and still be one chain
std::vector<chain> chains_of(dataset const &Data, double JoinTolerance = 2.0);

struct stretch_settings
{
	double simplify{2.0}; // m, the points nearer than this to the line through their neighbours aren't bends
	double default_speed{60.0}; // km/h, where the map gives none
	double shortest{300.0}; // m, a shorter run of another speed or number of tracks goes with its longer neighbour
	double reach{1500.0}; // m, how far from the change a straight is looked for to cut the chain on
	// a stretch of more bends than this is cut on its straights into shorter ones: the curves of a stretch are designed
	// together, which takes the longer the more of them there are, and a place is designed the same whatever area is asked for
	// only when the cuts don't depend on the area
	std::size_t bends{12};
	double straight{150.0}; // m, a leg this long is a straight to cut on rather than a side of a curve drawn with points
	bool operator==(stretch_settings const &) const = default;
};

// a part of a chain of one speed and one number of tracks, of no more bends than designed together well. It begins and ends
// on a straight between two bends, or at an end of the chain, so the stretch before it and the one after it meet on one
// straight line
struct stretch
{
	std::string name; // of its chain and its number in it: l132_s03
	attributes about; // the kilometres of the first and of the last section it takes
	double speed{60.0}; // km/h, the one of the map or the default
	std::vector<glm::dvec2> bends; // its ends and the bends between them
	std::size_t first{0}; // the points of the chain it was made of, to measure how far the tracks designed part from them
	std::size_t last{0};
};

std::vector<stretch> stretches_of(chain const &Chain, stretch_settings const &Settings);

// the indices of the points kept by the Douglas-Peucker simplification, the first and the last always
std::vector<std::size_t> simplified(std::vector<glm::dvec2> const &Points, double Tolerance);

} // namespace rail_data
