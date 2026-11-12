// The lines of a railway map made into what the tracks are designed from (editor/editorRailData): the GeoJSON read, the
// sections of a line joined into chains, the chains cut into stretches of one speed and one number of tracks.

#include "stdafx.h"
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "editor/editorRailData.hpp"
#include "editor/editorRailDesign.hpp"

namespace
{

// a line of three sections: 100 km/h single, 160 km/h double (given turned round), 100 km/h again but only 100 m long;
// between the first two it bends by a right angle, and a whisker is left where they meet
char const *const kMap{R"({"type":"FeatureCollection","crs":{"type":"name","properties":{"name":"urn:ogc:def:crs:EPSG::2180"}},"features":[
{"type":"Feature","properties":{"kind":"line","LN":"7","km_s":0,"km_e":4,"v_k":100,"NT":1,"EL":3},
 "geometry":{"type":"MultiLineString","coordinates":[[[0,0],[2000,0]],[[2000,0],[4000,0],[4001.5,0]]]}},
{"type":"Feature","properties":{"kind":"line","LN":"7","km_s":4,"km_e":8,"v_k":160,"NT":2,"EL":3},
 "geometry":{"type":"MultiLineString","coordinates":[[[4000,4000],[4000,2000],[4000,0]]]}},
{"type":"Feature","properties":{"kind":"line","LN":"7","km_s":8,"km_e":8.1,"v_k":100,"NT":1,"EL":3},
 "geometry":{"type":"MultiLineString","coordinates":[[[4000,4000],[4000,4100]]]}},
{"type":"Feature","properties":{"kind":"line","LN":"9","km_s":0,"km_e":1,"v_k":null,"NT":1},
 "geometry":{"type":"MultiLineString","coordinates":[[[9000,0],[9000,1000]]]}},
{"type":"Feature","properties":{"kind":"station_main","SN":"Proba","TYPE":"ST"},"geometry":{"type":"Point","coordinates":[4000,0]}}]})"};

} // namespace

TEST_CASE("rail data: the map is read, a file of another system refused")
{
	rail_data::dataset data;
	std::string error;
	REQUIRE(rail_data::read_geojson(kMap, data, error));
	CHECK(data.sections.size() == 4);
	REQUIRE(data.stations.size() == 1);
	CHECK(data.stations.front().name == "Proba");
	CHECK(data.sections.front().points.size() == 4); // the parts of the first one joined, their common point once
	CHECK(data.sections[3].about.speed == 0.0);

	rail_data::dataset other;
	CHECK_FALSE(rail_data::read_geojson(R"({"type":"FeatureCollection","features":[]})", other, error));
	CHECK_FALSE(rail_data::read_geojson("not a map", other, error));
}

TEST_CASE("rail data: the sections of a line make one chain, the whisker gone")
{
	rail_data::dataset data;
	std::string error;
	REQUIRE(rail_data::read_geojson(kMap, data, error));
	auto const chains{rail_data::chains_of(data)};
	REQUIRE(chains.size() == 2);
	auto const &line{chains.front()};
	CHECK(line.name == "l7");
	CHECK(line.sections.size() == 3);
	CHECK(line.spikes == 1);
	CHECK(line.points.front() == glm::dvec2{0.0, 0.0});
	CHECK(line.points.back() == glm::dvec2{4000.0, 4100.0});
	CHECK(chains.back().name == "l9");
}

TEST_CASE("rail data: a chain is cut in the middle of a straight where its speed changes")
{
	rail_data::dataset data;
	std::string error;
	REQUIRE(rail_data::read_geojson(kMap, data, error));
	auto const chains{rail_data::chains_of(data)};
	rail_data::stretch_settings settings;
	auto const stretches{rail_data::stretches_of(chains.front(), settings)};
	// the 100 m at the end are too short to be a stretch
	REQUIRE(stretches.size() == 2);
	CHECK(stretches[0].name == "l7_s01");
	CHECK(stretches[0].speed == 100.0);
	CHECK(stretches[0].about.tracks == 1);
	CHECK(stretches[1].speed == 160.0);
	CHECK(stretches[1].about.tracks == 2);
	// they meet at one point, on one straight
	CHECK(stretches[0].bends.back() == stretches[1].bends.front());
	auto const before{stretches[0].bends.back() - stretches[0].bends[stretches[0].bends.size() - 2]};
	auto const after{stretches[1].bends[1] - stretches[1].bends.front()};
	CHECK(std::abs(before.x * after.y - before.y * after.x) < 1e-6);
	CHECK(stretches[1].bends.back() == glm::dvec2{4000.0, 4100.0});
	// a line with no speed on the map gets the default one
	auto const disused{rail_data::stretches_of(chains.back(), settings)};
	REQUIRE(disused.size() == 1);
	CHECK(disused.front().speed == settings.default_speed);
}

TEST_CASE("rail design: the bends of a stretch become a line of the norms, from end to end")
{
	// two straights of 2 km meeting at a right angle
	std::vector<glm::dvec2> const bends{{0.0, 0.0}, {2000.0, 0.0}, {2000.0, 2000.0}};
	auto const plan{rail_design::design_of(bends, 120.0, alignment::limits{}, alignment::transition_shape::cubic_parabola)};
	REQUIRE(plan.shape.valid);
	REQUIRE(plan.shape.curves.size() == 1);
	CHECK(plan.shape.curves.front().cant > 0.0);
	CHECK(plan.shape.curves.front().transition_in > 0.0);
	CHECK(plan.limits.empty());
	auto const pieces{rail_design::pieces_of(plan, 100.0)};
	REQUIRE(pieces.size() >= 30);
	CHECK(glm::distance(pieces.front().points[segment_data::point::start], glm::dvec3{0.0, 0.0, 0.0}) < 1e-6);
	CHECK(glm::distance(pieces.back().points[segment_data::point::end], glm::dvec3{2000.0, 0.0, 2000.0}) < 1e-6);
	for (std::size_t i = 1; i < pieces.size(); ++i)
		CHECK(glm::distance(pieces[i].points[segment_data::point::start], pieces[i - 1].points[segment_data::point::end]) < 1e-6);
	CHECK(rail_design::speeds_of(plan, pieces, 120.0) == std::vector<double>(pieces.size(), 120.0));
	// points of the line itself lie no way from it, one 3 m to its side lies 3 m from it
	auto const on{alignment::evaluate(plan.shape, 0.3 * plan.shape.length).position};
	auto const along{alignment::evaluate(plan.shape, 0.3 * plan.shape.length).direction};
	CHECK(rail_design::deviation_of(plan.shape, {{on.x, on.z}}).worst < 1e-3);
	auto const aside{glm::normalize(glm::dvec2{-along.z, along.x}) * 3.0};
	CHECK(rail_design::deviation_of(plan.shape, {{on.x + aside.x, on.z + aside.y}}).worst == doctest::Approx(3.0).epsilon(0.01));
}

TEST_CASE("rail design: a curve too sharp for the line is slower, its cant and transitions the ones of its speed")
{
	// a bend of 60 degrees with 150 m of straight either side: no room for the radius 160 km/h asks for
	std::vector<glm::dvec2> const bends{{0.0, 0.0}, {150.0, 0.0}, {225.0, 129.9}};
	auto const plan{rail_design::design_of(bends, 160.0, alignment::limits{}, alignment::transition_shape::cubic_parabola)};
	REQUIRE(plan.shape.valid);
	REQUIRE(plan.limits.size() == 1);
	CHECK(plan.limits.front().speed < 160.0);
	CHECK(plan.limits.front().speed >= 10.0);
	auto const pieces{rail_design::pieces_of(plan, 20.0)};
	auto const speeds{rail_design::speeds_of(plan, pieces, 160.0)};
	REQUIRE(speeds.size() == pieces.size());
	CHECK(*std::ranges::min_element(speeds) == plan.limits.front().speed);
}

TEST_CASE("rail design: the two tracks of a double line run the spacing apart")
{
	std::vector<glm::dvec2> const bends{{0.0, 0.0}, {2000.0, 0.0}, {2000.0, 2000.0}};
	auto const plan{rail_design::design_of(bends, 120.0, alignment::limits{}, alignment::transition_shape::cubic_parabola)};
	REQUIRE(plan.shape.valid);
	auto const axis{rail_design::pieces_of(plan, 100.0)};
	auto const left{rail_design::beside(axis, 2.0)};
	auto const right{rail_design::beside(axis, -2.0)};
	REQUIRE(left.size() == axis.size());
	REQUIRE(right.size() == axis.size());
	for (std::size_t i = 0; i < axis.size(); ++i)
	{
		CHECK(glm::distance(left[i].points[segment_data::point::end], right[i].points[segment_data::point::end]) == doctest::Approx(4.0).epsilon(0.001));
		if (i > 0)
			CHECK(glm::distance(left[i].points[segment_data::point::start], left[i - 1].points[segment_data::point::end]) < 1e-9);
	}
	// a curve sharper than the offset has no track beside it on its inside
	CHECK(rail_design::beside(axis, 5000.0).empty());
}
