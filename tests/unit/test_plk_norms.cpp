#include "stdafx.h"
#include "doctest.h"
#include "editor/editorNorms.hpp"

// the limits of ST-T1-A6 the switch magic keeps to (editor/editorNorms.hpp), against the tables of the standard

TEST_CASE("plk: least radius by the kind of track (tab. 9.2)") {
	CHECK(plk::at(plk::radius_min(plk::track_kind::main), plk::level::p1) == doctest::Approx(300.0));
	CHECK(plk::at(plk::radius_min(plk::track_kind::main), plk::level::p2) == doctest::Approx(180.0));
	CHECK(plk::at(plk::radius_min(plk::track_kind::side), plk::level::p1) == doctest::Approx(180.0));
}

TEST_CASE("plk: cant deficiency narrowed in tight curves (tab. 9.7)") {
	CHECK(plk::at(plk::deficiency_max(plk::track_kind::main, 1000.0), plk::level::p1) == doctest::Approx(130.0));
	CHECK(plk::at(plk::deficiency_max(plk::track_kind::main, 190.0), plk::level::p2) == doctest::Approx(70.0));
	CHECK(plk::at(plk::deficiency_max(plk::track_kind::side, 220.0), plk::level::p1) == doctest::Approx(100.0));
	// a 1:9 R 190 branch at 40 km/h: 99 mm, more than R <= 200 m allows
	CHECK(plk::deficiency(40.0, 190.0) == doctest::Approx(99.4).epsilon(0.01));
	CHECK(plk::deficiency(40.0, 190.0) > plk::at(plk::deficiency_max(plk::track_kind::side, 190.0), plk::level::p2));
}

TEST_CASE("plk: sudden change of cant deficiency (tab. 9.15, 9.16)") {
	CHECK(plk::at(plk::sudden_deficiency_max(plk::track_kind::connection, 40.0), plk::level::p1) == doctest::Approx(130.0));
	CHECK(plk::at(plk::sudden_deficiency_max(plk::track_kind::connection, 80.0), plk::level::p2) == doctest::Approx(125.0));
	CHECK(plk::at(plk::sudden_deficiency_max(plk::track_kind::main, 120.0), plk::level::p1) == doctest::Approx(40.0));
	CHECK(plk::between_sudden_min(40.0) == doctest::Approx(6.0));
	CHECK(plk::between_sudden_min(80.0) == doctest::Approx(12.0));
	// reverse curves R 300 behind a 1:9 R 300 switch at 40 km/h meeting at a point: 63 + 63 mm, within 130 mm
	auto const each{plk::deficiency(40.0, 300.0)};
	CHECK(2.0 * each < plk::at(plk::sudden_deficiency_max(plk::track_kind::connection, 40.0), plk::level::p1));
}

TEST_CASE("plk: straight between reverse curves against buffer locking") {
	CHECK(plk::buffer_straight_min(300.0, 300.0) == doctest::Approx(0.0));
	// R 180 + R 180: equivalent 90 m, 8 m (A. Massel, przyklad 7.2)
	CHECK(plk::buffer_straight_min(180.0, 180.0) == doctest::Approx(8.0));
	CHECK(plk::buffer_straight_min(140.0, 140.0) == doctest::Approx(-1.0));
	CHECK(plk::buffer_straight_min(200.0, 200.0) == doctest::Approx(6.0));
}
