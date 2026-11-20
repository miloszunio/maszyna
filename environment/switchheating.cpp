/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "environment/switchheating.h"

#include <algorithm>

namespace switch_heating
{

namespace
{

// J/kg to melt ice, and J/(kg K) to warm it
double constexpr FUSION{334000.0};
double constexpr ICE_HEAT{2100.0};
// m/s of the wind which halves the share of the heat reaching the snow
double constexpr WIND_HALVING{10.0};

} // namespace

double melt_rate(double const Power, air const &Air)
{
	if (Power <= 0.0)
	{
		return 0.0;
	}
	// W on a square metre of the snow, of what the wind leaves
	auto const share{efficiency / (1.0 + std::max(0.0, Air.wind) / WIND_HALVING)};
	auto const flux{share * Power / heated_width};
	// J to bring a cubic metre of the snow to 0 C and melt it
	auto const energy{std::max(20.0, Air.density) * (FUSION + ICE_HEAT * std::max(0.0, -Air.temperature))};
	return flux / energy;
}

double advance(state &State, double const Brought, double const Around, double const Time, double const Rate)
{
	if (false == State.known)
	{
		State.known = true;
		State.snow = (Rate > 0.0 ? 0.0 : Around);
	}
	else
	{
		State.snow += std::max(0.0, Brought - State.brought);
		State.snow -= Rate * std::max(0.0, Time - State.time);
	}
	// no more than lies around: the snow thaws there too
	State.snow = std::clamp(State.snow, 0.0, std::max(0.0, Around));
	State.brought = Brought;
	State.time = Time;
	return State.snow;
}

bool holds(double const Snow, double const Ice, double const Temperature, bool const Heated)
{
	if (Snow >= holding_snow)
	{
		return true;
	}
	return (false == Heated) && (Ice >= holding_ice) && (Temperature < 0.0);
}

} // namespace switch_heating
