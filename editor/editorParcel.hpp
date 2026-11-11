/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

// the cadastral parcels of the ULDK service of GUGiK (uldk.gugik.gov.pl): the outline of a parcel found by a point in it or by
// its number, in PUWG 1992 (EPSG:2180), as an outline to cut or to fill by
namespace parcel
{

struct found
{
	std::string id; // as 021903_2.0003.176/5
	std::vector<glm::dvec2> ring; // east, north; the outer ring, without its last point repeating the first
};

// the queries: the parcel under the point (east, north of PUWG 1992), or the parcel of the number
std::string url_at(double East, double North);
std::string url_of(std::string const &Id);
// the answer of the service read: true with the parcel, false with Error saying why (nothing there, a wrong number)
bool read_answer(std::string const &Text, found &Parcel, std::string &Error);
// fetched (blocking) and read
bool fetch(std::string const &Url, found &Parcel, std::string &Error);
// the outlines of parcels lying side by side made one: the sides they share dropped, the rest joined in a ring; false if
// they don't make one ring (not touching, or a hole between them)
bool merge(std::vector<std::vector<glm::dvec2>> const &Rings, std::vector<glm::dvec2> &Merged);

} // namespace parcel
