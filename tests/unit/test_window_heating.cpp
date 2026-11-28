#include "stdafx.h"
#include "doctest.h"
#include "McZapkie/MOVER.h"
#include "vehicle.h"

#include <string>

// the heating of the windscreens (TMoverParameters::WindowHeating, windows_heated()): the snow lying on them melts fast where
// they are heated (gfx::windscreen_water, water_melt_time() of the shaders)

namespace {

// the wagon of the tests, with Extra put in before its Param line
std::string wagon_with(std::string const &Extra) {
	auto text{test::definition::load("wagon").text()};
	auto const param{text.find("Param")};
	REQUIRE(param != std::string::npos);
	text.insert(param, Extra);
	return text;
}

} // namespace

TEST_CASE("window heating: a vehicle without it in its definition has its windscreens heated all the time") {
	TMoverParameters wagon(0.0, "wagon", "test", 0);
	REQUIRE(wagon.LoadFIZ_Text(wagon_with("")));
	CHECK_FALSE(wagon.WindowHeating.fitted);
	CHECK(wagon.windows_heated());
	// the switch changes nothing there
	wagon.WindowHeatingSwitch(false);
	CHECK(wagon.windows_heated());
	// a Clima section without the entry neither
	TMoverParameters other(0.0, "wagon", "test", 0);
	REQUIRE(other.LoadFIZ_Text(wagon_with("Clima: Heating=Main\n")));
	CHECK_FALSE(other.WindowHeating.fitted);
	CHECK(other.windows_heated());
}

TEST_CASE("window heating: fitted, the windscreens are heated once switched on with the low voltage there") {
	TMoverParameters wagon(0.0, "wagon", "test", 0);
	REQUIRE(wagon.LoadFIZ_Text(wagon_with("Clima: WindowHeating=Yes\n")));
	REQUIRE(wagon.WindowHeating.fitted);
	CHECK_FALSE(wagon.WindowHeating.is_enabled);

	wagon.Power24vIsAvailable = true;
	CHECK_FALSE(wagon.windows_heated());
	CHECK(wagon.WindowHeatingSwitch(true));
	CHECK_FALSE(wagon.WindowHeatingSwitch(true));
	CHECK(wagon.windows_heated());

	// no low voltage, no heating
	wagon.Power24vIsAvailable = false;
	wagon.Power110vIsAvailable = false;
	CHECK_FALSE(wagon.windows_heated());
	wagon.Power110vIsAvailable = true;
	CHECK(wagon.windows_heated());

	CHECK(wagon.WindowHeatingSwitch(false));
	CHECK_FALSE(wagon.windows_heated());
}

TEST_CASE("window heating: the entry is read in any case") {
	TMoverParameters wagon(0.0, "wagon", "test", 0);
	REQUIRE(wagon.LoadFIZ_Text(wagon_with("Clima: WindowHeating=yes\n")));
	CHECK(wagon.WindowHeating.fitted);
	TMoverParameters none(0.0, "wagon", "test", 0);
	REQUIRE(none.LoadFIZ_Text(wagon_with("Clima: WindowHeating=No\n")));
	CHECK_FALSE(none.WindowHeating.fitted);
}
