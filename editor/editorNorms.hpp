/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the limits of PKP PLK ST-T1-A6 "Uklady geometryczne torow" (in force since 17.04.2025) the editor checks what it lays against;
// docs/plk-normy.md lists them with their sources. Each limit comes as the three thresholds of the standard: P0 recommended,
// P1 normal, P2 the boundary one (which takes the consent of the railway's line office)

#include <array>
#include <cmath>
#include <algorithm>

namespace plk
{

enum class level : int
{
	p0 = 0,
	p1 = 1,
	p2 = 2
};

enum class track_kind : int
{
	main, // szlakowe, glowne zasadnicze
	side, // glowne dodatkowe, boczne
	connection // polaczenia torow (rozjazdy z odcinkami miedzy nimi)
};

using limits = std::array<double, 3>;

inline double at(limits const &Limits, level const Level)
{
	return Limits[static_cast<int>(Level)];
}

inline char const *name(level const Level)
{
	return Level == level::p0 ? "P0" : Level == level::p1 ? "P1" : "P2";
}

// cant deficiency of a curve of Radius at Speed with Cant: I = 11,8 V^2 / R - D [mm] (wzor 9.9)
inline double deficiency(double const Speed, double const Radius, double const Cant = 0.0)
{
	return Radius > 0.0 ? 11.8 * Speed * Speed / Radius - Cant : -Cant;
}

// the least radius (tab. 9.2); connections take the one of the tracks they are part of, as side tracks
inline limits radius_min(track_kind const Kind)
{
	return Kind == track_kind::main ? limits{600.0, 300.0, 180.0} : limits{450.0, 180.0, 180.0};
}

// the greatest cant deficiency (tab. 9.7), narrowed for tight curves
inline limits deficiency_max(track_kind const Kind, double const Radius)
{
	limits result{Kind == track_kind::main ? limits{110.0, 130.0, 150.0} : limits{80.0, 100.0, 100.0}};
	limits const narrow{Radius <= 200.0 ? limits{50.0, 70.0, 70.0} : Radius <= 250.0 ? limits{80.0, 100.0, 100.0} : limits{1e9, 1e9, 1e9}};
	for (int i = 0; i < 3; ++i)
		result[i] = std::min(result[i], narrow[i]);
	return result;
}

// the greatest sudden change of cant deficiency, where the curvature changes at a point (tab. 9.15)
inline limits sudden_deficiency_max(track_kind const Kind, double const Speed)
{
	if (Kind == track_kind::connection)
	{
		if (Speed <= 60.0)
			return {130.0, 130.0, 130.0};
		if (Speed <= 100.0)
			return {100.0, 100.0, 125.0};
		auto const formula{160.0 - 0.35 * Speed};
		return {Speed <= 160.0 ? 80.0 : 60.0, formula, formula};
	}
	if (Kind == track_kind::side)
		return {50.0, 100.0, 100.0};
	if (Speed <= 70.0)
		return {50.0, 50.0, 60.0};
	if (Speed <= 160.0)
	{
		auto const formula{64.0 - 0.2 * Speed};
		return {0.0, formula, std::max(formula, 50.0)};
	}
	return Speed <= 200.0 ? limits{0.0, 30.0, 50.0} : limits{0.0, 25.0, 25.0};
}

// the least length of the piece between two sudden changes of cant deficiency for them to count apart (tab. 9.16), connections
inline double between_sudden_min(double const Speed)
{
	if (Speed <= 60.0)
		return 6.0;
	return Speed <= 100.0 ? 0.15 * Speed : 0.25 * Speed;
}

// the least straight between reverse curves of radii R1, R2 so that the buffers of the vehicles don't lock (ST-T1-A6 pkt 9.2.9-10,
// EN 13803-2 rozdz. 8.4; the table as in A. Massel, "Projektowanie linii i stacji kolejowych", tabl. 7.4): of the equivalent
// radius R1 R2 / (R1 + R2), 0 from 110 m up; below 75 m no straight is enough (-1)
inline double buffer_straight_min(double const R1, double const R2)
{
	if (R1 <= 0.0 || R2 <= 0.0)
		return 0.0;
	auto const equivalent{R1 * R2 / (R1 + R2)};
	static double const table[][2]{{110.0, 0.0}, {100.0, 6.0}, {95.0, 7.0}, {92.5, 7.5}, {90.0, 8.0}, {85.0, 9.0}, {80.0, 10.2}, {75.0, 11.5}};
	if (equivalent >= table[0][0])
		return 0.0;
	if (equivalent < 75.0)
		return -1.0;
	for (int i = 1; i < 8; ++i)
		if (equivalent >= table[i][0])
		{
			// between the rows the longer straight, the table gives no rule for between
			return table[i - 1][0] - equivalent < 1e-9 ? table[i - 1][1] : table[i][1];
		}
	return table[7][1];
}

} // namespace plk
