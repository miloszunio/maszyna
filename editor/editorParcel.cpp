/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorParcel.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <utility>

#include "editor/editorHttp.hpp"

namespace parcel
{
namespace
{

std::string const service{"https://uldk.gugik.gov.pl/?"};

// the outer rings of a POLYGON or a MULTIPOLYGON of the WKT, each without the point that closes it
std::vector<std::vector<glm::dvec2>> outer_rings(std::string const &Wkt)
{
	std::vector<std::vector<glm::dvec2>> rings;
	int depth{0};
	std::vector<glm::dvec2> ring;
	bool outer{false};
	std::string number;
	std::vector<double> values;
	auto const flush_number = [&]() {
		if (false == number.empty())
		{
			values.push_back(std::atof(number.c_str()));
			number.clear();
		}
	};
	auto const flush_point = [&]() {
		flush_number();
		if (values.size() >= 2 && outer)
			ring.emplace_back(values[0], values[1]);
		values.clear();
	};
	// the rings of a polygon are at the deepest level; the first one in each polygon is the outer one
	int const deepest{Wkt.find("MULTIPOLYGON") != std::string::npos ? 3 : 2};
	bool first_of_polygon{true};
	for (auto const c : Wkt)
	{
		if (c == '(')
		{
			++depth;
			if (depth == deepest - 1)
				first_of_polygon = true;
			if (depth == deepest)
			{
				ring.clear();
				outer = first_of_polygon;
				first_of_polygon = false;
			}
		}
		else if (c == ')')
		{
			if (depth == deepest)
			{
				flush_point();
				if (outer && ring.size() >= 4)
				{
					if (glm::distance(ring.front(), ring.back()) < 1e-6)
						ring.pop_back();
					rings.push_back(ring);
				}
			}
			--depth;
		}
		else if (depth == deepest && c == ',')
			flush_point();
		else if (depth == deepest && (c == ' ' || c == '\t'))
			flush_number();
		else if (depth == deepest)
			number += c;
	}
	return rings;
}

double area(std::vector<glm::dvec2> const &Ring)
{
	double sum{0.0};
	for (std::size_t i = 0, j = Ring.size() - 1; i < Ring.size(); j = i++)
		sum += (Ring[j].x * Ring[i].y) - (Ring[i].x * Ring[j].y);
	return sum * 0.5;
}

// a point of the cadastre to a centimetre: the parcels side by side share their points exactly
std::pair<std::int64_t, std::int64_t> key_of(glm::dvec2 const &Point)
{
	return {static_cast<std::int64_t>(std::llround(Point.x * 100.0)), static_cast<std::int64_t>(std::llround(Point.y * 100.0))};
}

std::string encoded(std::string const &Text)
{
	std::string out;
	for (auto const c : Text)
	{
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')
			out += c;
		else
		{
			char hex[4];
			std::snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
			out += hex;
		}
	}
	return out;
}

} // namespace

std::string url_at(double const East, double const North)
{
	char xy[64];
	std::snprintf(xy, sizeof(xy), "%.2f,%.2f,2180", East, North);
	return service + "request=GetParcelByXY&xy=" + xy + "&result=id,geom_wkt&srid=2180";
}

std::string url_of(std::string const &Id)
{
	return service + "request=GetParcelById&id=" + encoded(Id) + "&result=id,geom_wkt&srid=2180";
}

bool read_answer(std::string const &Text, found &Parcel, std::string &Error)
{
	std::istringstream lines{Text};
	std::string status;
	std::getline(lines, status);
	while (false == status.empty() && (status.back() == '\r' || status.back() == ' '))
		status.pop_back();
	if (status != "0")
	{
		// "-1 brak wyników" and such: the service says it in Polish
		Error = status.empty() ? std::string{"no answer"} : status;
		return false;
	}
	std::string line;
	std::getline(lines, line);
	auto const bar{line.find('|')};
	auto const wkt_at{line.find("POLYGON")};
	if (bar == std::string::npos || wkt_at == std::string::npos)
	{
		Error = "the answer has no outline";
		return false;
	}
	Parcel.id = line.substr(0, bar);
	auto const rings{outer_rings(line.substr(wkt_at > 6 && line.compare(wkt_at - 5, 5, "MULTI") == 0 ? wkt_at - 5 : wkt_at))};
	if (rings.empty())
	{
		Error = "the outline couldn't be read";
		return false;
	}
	// of a parcel of several parts, the largest
	Parcel.ring = rings.front();
	for (auto const &ring : rings)
		if (std::abs(area(ring)) > std::abs(area(Parcel.ring)))
			Parcel.ring = ring;
	return true;
}

bool fetch(std::string const &Url, found &Parcel, std::string &Error)
{
	editor_http::client client;
	std::vector<std::uint8_t> body;
	if (false == client.get(Url, body, Error, [] { return false; }))
		return false;
	return read_answer(std::string{body.begin(), body.end()}, Parcel, Error);
}

bool merge(std::vector<std::vector<glm::dvec2>> const &Rings, std::vector<glm::dvec2> &Merged)
{
	using key = std::pair<std::int64_t, std::int64_t>;
	// every side, all the rings turned the same way; a side two parcels share runs the opposite ways in them and is dropped
	std::map<std::pair<key, key>, int> sides;
	std::map<key, glm::dvec2> points;
	for (auto ring : Rings)
	{
		if (ring.size() < 3)
			continue;
		if (area(ring) < 0.0)
			std::reverse(ring.begin(), ring.end());
		for (std::size_t i = 0; i < ring.size(); ++i)
		{
			auto const a{key_of(ring[i])};
			auto const b{key_of(ring[(i + 1) % ring.size()])};
			if (a == b)
				continue;
			points[a] = ring[i];
			if (auto const opposite{sides.find({b, a})}; opposite != sides.end())
			{
				if (--opposite->second == 0)
					sides.erase(opposite);
			}
			else
				++sides[{a, b}];
		}
	}
	if (sides.empty())
		return false;
	// the sides left joined end to start
	std::map<key, key> next;
	for (auto const &side : sides)
	{
		if (next.count(side.first.first) != 0)
			return false; // two sides from one point: the parcels touch only there, or leave a hole
		next[side.first.first] = side.first.second;
	}
	Merged.clear();
	auto const start{next.begin()->first};
	auto at{start};
	do
	{
		Merged.push_back(points[at]);
		auto const found{next.find(at)};
		if (found == next.end())
			return false;
		at = found->second;
	} while (at != start && Merged.size() <= next.size());
	return Merged.size() == next.size();
}

} // namespace parcel
