#include "stdafx.h"
#include "doctest.h"
#include "editor/editorRoadLayout.hpp"

namespace {

// a piece of a road as the editor holds it: an entry for each lane and for each pair of neighbours
road_node::state piece(int const Forward, int const Backward) {
	road_node::state state;
	state.forward = Forward;
	state.backward = Backward;
	state.lanes.assign(static_cast<std::size_t>(Forward + Backward), road_node::lane_data{});
	state.changes.assign(state.lanes.size() - 1, road_node::change_both);
	return state;
}

// an asphalt piece, and the dirt road which goes on from it with the same lanes
road_node::state asphalt() {
	auto state{piece(1, 1)};
	state.surface = "asphaltdark1";
	state.sides[0].type = road_node::side_type::sidewalk;
	state.sides[0].width = 2.f;
	state.sides[0].material = "paving";
	return state;
}

road_node::state dirt() {
	auto state{piece(1, 1)};
	state.surface = "dirt";
	state.markings = road_node::marking_colour::none;
	state.friction = 0.6f;
	state.rough = 0.05f;
	state.textureangle = 90.f;
	state.lanewidth = 2.5f;
	for (auto &lane : state.lanes)
		lane.width = 2.5f;
	state.axis.points[segment_data::point::end] = glm::dvec3{0.0, 0.0, 80.0};
	state.taper = {1.4f, 1.f};
	state.median.width = {0.f, 3.f};
	state.banks[1].set = true;
	state.banks[1].width = {2.f, 4.f};
	state.changes.assign(1, road_node::change_none);
	return state;
}

}

TEST_CASE("road layout: a change of one thing leaves the rest of another piece as it was") {
	// conditions: the kerb of the asphalt piece made higher
	auto const before{asphalt()};
	auto after{before};
	after.kerbheight = 0.2f;
	auto const other{dirt()};
	// action
	auto const result{road_layout::carried_over(before, after, other)};
	// outcome: the dirt road has the kerb height, and is still a dirt road
	CHECK(result.kerbheight == 0.2f);
	CHECK(result.surface == "dirt");
	CHECK(result.markings == road_node::marking_colour::none);
	CHECK(result.friction == 0.6f);
	CHECK(result.rough == 0.05f);
	CHECK(result.textureangle == 90.f);
	CHECK(result.lanewidth == 2.5f);
	CHECK(result.lanes[0].width == 2.5f);
	CHECK(result.sides[0].type == road_node::side_type::none);
	CHECK(result.changes[0] == road_node::change_none);
}

TEST_CASE("road layout: nothing changed, nothing carried over") {
	auto const before{asphalt()};
	auto const other{dirt()};
	auto const result{road_layout::carried_over(before, before, other)};
	CHECK(result.surface == other.surface);
	CHECK(result.kerbheight == other.kerbheight);
	CHECK(result.sides[0].width == other.sides[0].width);
	CHECK(result.banks == other.banks);
	CHECK(result.lanes.size() == other.lanes.size());
}

TEST_CASE("road layout: what a piece has for itself is never taken from the changed one") {
	auto const before{asphalt()};
	auto after{before};
	after.surface = "concrete";
	after.axis.points[segment_data::point::end] = glm::dvec3{50.0, 0.0, 0.0};
	after.taper = {2.f, 2.f};
	after.median.width = {5.f, 5.f};
	after.median.round = {1.f, 1.f};
	after.missing[1] = {1, 0};
	auto const other{dirt()};
	auto const result{road_layout::carried_over(before, after, other)};
	CHECK(result.surface == "concrete");
	CHECK(result.axis.points[segment_data::point::end] == other.axis.points[segment_data::point::end]);
	CHECK(result.taper == other.taper);
	CHECK(result.median.width == other.median.width);
	CHECK(result.median.round == other.median.round);
	CHECK(result.missing == other.missing);
}

TEST_CASE("road layout: a side is changed on the side it was changed on") {
	auto const before{asphalt()};
	auto after{before};
	after.sides[1].type = road_node::side_type::shoulder;
	after.sides[1].width = 1.5f;
	after.kerbs[1] = true;
	auto other{dirt()};
	other.sides[0].type = road_node::side_type::shoulder;
	other.sides[0].width = 0.75f;
	auto const result{road_layout::carried_over(before, after, other)};
	CHECK(result.sides[1].type == road_node::side_type::shoulder);
	CHECK(result.sides[1].width == 1.5f);
	CHECK(result.kerbs[1]);
	CHECK(result.sides[0].type == road_node::side_type::shoulder);
	CHECK(result.sides[0].width == 0.75f);
	CHECK_FALSE(result.kerbs[0]);
}

TEST_CASE("road layout: another set of lanes goes to every piece, with the default rules for changing them") {
	auto const before{asphalt()};
	auto after{piece(2, 1)};
	after.surface = before.surface;
	after.sides = before.sides;
	after.lanewidth = 3.f;
	for (auto &lane : after.lanes)
		lane.width = 3.f;
	auto const other{dirt()};
	auto const result{road_layout::carried_over(before, after, other)};
	CHECK(result.forward == 2);
	CHECK(result.backward == 1);
	CHECK(result.lanes.size() == 3);
	CHECK(result.lanewidth == 3.f);
	CHECK(result.changes == after.changes);
	// the look of the piece stays its own
	CHECK(result.surface == "dirt");
}

TEST_CASE("road layout: a change to a lane is made to the same lane, where the piece has the same lanes") {
	auto const before{asphalt()};
	auto after{before};
	after.lanes[1].velocity = 50.f;
	auto const other{dirt()};
	auto const result{road_layout::carried_over(before, after, other)};
	CHECK(result.lanes[1].velocity == 50.f);
	CHECK(result.lanes[1].width == 2.5f);
	CHECK(result.lanes[0].velocity == other.lanes[0].velocity);
	// a piece with other lanes keeps them the way they are
	auto const wider{piece(2, 2)};
	auto const kept{road_layout::carried_over(before, after, wider)};
	CHECK(kept.lanes.size() == 4);
	CHECK(kept.lanes[1].velocity == wider.lanes[1].velocity);
}

TEST_CASE("road layout: the banks go along only when it's them which were changed") {
	auto const before{asphalt()};
	auto after{before};
	after.bankmaterial = "grass";
	auto const other{dirt()};
	CHECK(road_layout::carried_over(before, after, other).banks == other.banks);
	after.banks[0].set = true;
	after.banks[0].width = {3.f, 3.f};
	CHECK(road_layout::carried_over(before, after, other).banks == after.banks);
}

TEST_CASE("road layout: an uneven road made even is made even all the way") {
	auto const before{dirt()};
	auto after{before};
	after.rough = 0.f;
	auto other{dirt()};
	other.surface = "gravel";
	auto const result{road_layout::carried_over(before, after, other)};
	CHECK(result.rough == 0.f);
	CHECK(result.roughscale == other.roughscale);
	CHECK(result.surface == "gravel");
}
