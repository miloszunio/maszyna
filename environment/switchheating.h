/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the electric heating of the switches (elektryczne ogrzewanie rozjazdów, EOR) and the snow and the ice which hold their blades
// (physics.weather.switches): heaters along the stock rails and the blades melt the snow which falls and drifts between them. the
// snow lying at the blades of a switch is what came since it was last known (snow_cover::brought()) less what the heaters melted,
// worked out when it's asked for; where it gets too deep, or the rails are iced over and nothing heats them, the drive can't
// press a blade to its stock rail and the switch isn't thrown (TTrack::SwitchCommand()).
// the heat a heater gives is the usual 330 W a metre of a rail: with both rails heated along some 14 m of the blades that's
// about 10 kW a switch, the mean of what's installed on the network. the rate of the melting follows from the balance of the
// energy: the share of the heat that reaches the snow, over the heat it takes to warm the snow to 0 C and to melt it
namespace switch_heating
{

// W a metre of a heated rail
double constexpr default_power{330.0};
// m across a rail its heater keeps clear, the ground between the stock rail and the open blade with what lies beside them
double constexpr heated_width{0.5};
// the share of the heat which reaches the snow in still air; the wind takes more of the rest away
double constexpr efficiency{0.6};
// m of snow between an open blade and its stock rail the drive can't press out
double constexpr holding_snow{0.04};
// the share of the ice on the heads of the rails (weather_physics::railhead::ice) from which the blades are frozen to their chairs
double constexpr holding_ice{0.5};

// the air at the switch: its temperature, C, the wind at the ground, m/s, and the density of the snow lying, kg/m3
struct air
{
	double temperature{0.0};
	double wind{0.0};
	double density{100.0};
};

// m of the depth of the snow the heaters of Power (W a metre of a rail) melt in a second
double melt_rate(double Power, air const &Air);

// the snow at the blades of a switch, as it was when last asked for
struct state
{
	double snow{0.0}; // m
	double brought{0.0}; // snow_cover::brought() then
	double time{0.0}; // s of the time of the simulation then
	bool known{false};
};

// the snow at the blades now, m: Brought as snow_cover::brought(), Around the most that lies there (the snow around, with its
// drifts), Time the time of the simulation, s, Rate what the heaters melt, m a second (0 without them). a switch asked for the first
// time has what its heaters keep in the long run: nothing where they work, the snow around where they don't
double advance(state &State, double Brought, double Around, double Time, double Rate);

// the blades are held: by the snow, also where the heaters don't keep up with it, or by the ice on rails nothing heats, in a frost
bool holds(double Snow, double Ice, double Temperature, bool Heated);

} // namespace switch_heating
