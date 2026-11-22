#include "stdafx.h"
#include "doctest.h"
#include "environment/switchheating.h"

namespace {

// m of snow an hour
double hourly(double const Power, switch_heating::air const &Air) {
	return switch_heating::melt_rate(Power, Air) * 3600.0;
}

} // namespace

TEST_CASE("switch heating: the usual heaters melt a few centimetres of new snow an hour") {
	// 330 W a metre, a light frost, a calm, new snow of 100 kg/m3
	auto const calm{hourly(switch_heating::default_power, {-2.0, 0.0, 100.0})};
	CAPTURE(calm);
	CHECK(calm > 0.03);
	CHECK(calm < 0.06);
	// nothing without the power
	CHECK(switch_heating::melt_rate(0.0, {-2.0, 0.0, 100.0}) == doctest::Approx(0.0));
}

TEST_CASE("switch heating: the frost, the wind and the denser snow slow the melting, more power quickens it") {
	switch_heating::air const mild{-1.0, 0.0, 100.0};
	auto const reference{hourly(switch_heating::default_power, mild)};
	CHECK(hourly(switch_heating::default_power, {-20.0, 0.0, 100.0}) < reference);
	CHECK(hourly(switch_heating::default_power, {-1.0, 10.0, 100.0}) == doctest::Approx(reference * 0.5));
	CHECK(hourly(switch_heating::default_power, {-1.0, 0.0, 300.0}) == doctest::Approx(reference / 3.0));
	CHECK(hourly(2.0 * switch_heating::default_power, mild) == doctest::Approx(reference * 2.0));
}

TEST_CASE("switch heating: a switch first asked for has what its heaters keep in the long run") {
	switch_heating::state heated;
	CHECK(switch_heating::advance(heated, 0.5, 0.3, 100.0, 1e-5) == doctest::Approx(0.0));
	switch_heating::state cold;
	CHECK(switch_heating::advance(cold, 0.5, 0.3, 100.0, 0.0) == doctest::Approx(0.3));
}

TEST_CASE("switch heating: without the heaters the snow which comes stays at the blades, no more than lies around") {
	switch_heating::state state;
	switch_heating::advance(state, 0.0, 0.0, 0.0, 0.0);
	// 5 cm came over an hour
	CHECK(switch_heating::advance(state, 0.05, 0.05, 3600.0, 0.0) == doctest::Approx(0.05));
	// the wind brought 20 cm more into the track, but only 12 cm lies around
	CHECK(switch_heating::advance(state, 0.25, 0.12, 7200.0, 0.0) == doctest::Approx(0.12));
	// a thaw took all of it
	CHECK(switch_heating::advance(state, 0.25, 0.0, 10800.0, 0.0) == doctest::Approx(0.0));
}

TEST_CASE("switch heating: the heaters keep up with a snowfall lighter than they melt, and fall behind a heavier one") {
	auto const rate{switch_heating::melt_rate(switch_heating::default_power, {-2.0, 0.0, 100.0})};
	switch_heating::state light;
	switch_heating::state heavy;
	switch_heating::advance(light, 0.0, 1.0, 0.0, rate);
	switch_heating::advance(heavy, 0.0, 1.0, 0.0, rate);
	auto lightsnow{0.0};
	auto heavysnow{0.0};
	// three hours, asked every 10 s: 2 cm an hour, and 8 cm an hour
	for (int step = 1; step <= 1080; ++step) {
		auto const time{step * 10.0};
		lightsnow = switch_heating::advance(light, 0.02 * time / 3600.0, 1.0, time, rate);
		heavysnow = switch_heating::advance(heavy, 0.08 * time / 3600.0, 1.0, time, rate);
	}
	CHECK(lightsnow == doctest::Approx(0.0));
	CHECK(heavysnow > switch_heating::holding_snow);
	CHECK(switch_heating::holds(heavysnow, 0.0, -2.0, true));
	CHECK_FALSE(switch_heating::holds(lightsnow, 0.0, -2.0, true));
}

TEST_CASE("switch heating: the heaters switched on melt the snow which held the blades") {
	switch_heating::state state;
	// 10 cm lying at a switch nothing heated
	CHECK(switch_heating::advance(state, 0.3, 0.10, 0.0, 0.0) == doctest::Approx(0.10));
	CHECK(switch_heating::holds(state.snow, 0.0, -2.0, false));
	auto const rate{switch_heating::melt_rate(switch_heating::default_power, {-2.0, 0.0, 100.0})};
	// an hour later still held, three hours later free
	CHECK(switch_heating::holds(switch_heating::advance(state, 0.3, 0.10, 3600.0, rate), 0.0, -2.0, true));
	CHECK_FALSE(switch_heating::holds(switch_heating::advance(state, 0.3, 0.10, 3.0 * 3600.0, rate), 0.0, -2.0, true));
}

TEST_CASE("switch heating: the ice holds the blades of a switch nothing heats, in a frost only") {
	CHECK(switch_heating::holds(0.0, 0.8, -3.0, false));
	CHECK_FALSE(switch_heating::holds(0.0, 0.8, -3.0, true));
	CHECK_FALSE(switch_heating::holds(0.0, 0.8, 2.0, false));
	CHECK_FALSE(switch_heating::holds(0.0, 0.2, -3.0, false));
	// a snow restarted (set by hand) brings nothing back
	switch_heating::state state;
	switch_heating::advance(state, 0.4, 0.2, 0.0, 0.0);
	CHECK(switch_heating::advance(state, 0.0, 0.2, 10.0, 0.0) == doctest::Approx(0.2));
}
