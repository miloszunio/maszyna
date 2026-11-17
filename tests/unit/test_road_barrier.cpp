#include "stdafx.h"
#include "doctest.h"
#include "world/RoadBarrier.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

using road_barrier::entry;

entry barrier(road_barrier::side const Side, float const From, float const To) {
	entry result;
	result.where = Side;
	result.from = From;
	result.to = To;
	return result;
}

// a barrier along a straight, level road going along z, its lanes 3.5 m to each side of the axis
road_barrier::layout straight(double const From, double const To, bool const Left = false) {
	road_barrier::layout layout;
	layout.onleft = Left;
	layout.offset = 1.0;
	layout.from = From;
	layout.to = To;
	layout.at = [](double const Station) { return road_barrier::frame{glm::dvec3{0.0, 0.0, Station}, glm::dvec3{0.0, 0.0, 1.0}, glm::dvec3{1.0, 0.0, 0.0}}; };
	layout.edge = [](double const) { return 3.5; };
	return layout;
}

// the highest and the lowest point of the mesh between two distances along the road
struct heights {
	double top{std::numeric_limits<double>::lowest()};
	double bottom{std::numeric_limits<double>::max()};
	int count{0};
};

heights heights_between(std::vector<world_vertex> const &Vertices, double const From, double const To) {
	heights result;
	for (auto const &vertex : Vertices) {
		if (vertex.position.z < From || vertex.position.z > To)
			continue;
		result.top = std::max(result.top, vertex.position.y);
		result.bottom = std::min(result.bottom, vertex.position.y);
		++result.count;
	}
	return result;
}

bool finite(std::vector<world_vertex> const &Vertices) {
	return std::ranges::all_of(Vertices, [](world_vertex const &Vertex) {
		return std::isfinite(Vertex.position.x) && std::isfinite(Vertex.position.y) && std::isfinite(Vertex.position.z) && std::isfinite(Vertex.normal.x) && std::isfinite(Vertex.normal.y) &&
		       std::isfinite(Vertex.normal.z);
	});
}

}

TEST_CASE("road barrier: an entry is read back from the text it's kept as") {
	entry written;
	written.where = road_barrier::side::left;
	written.offset = 1.25f;
	written.from = 40.f;
	written.to = 120.5f;
	written.endings = {road_barrier::ending::ramp, road_barrier::ending::cap};
	written.lengths = {8.f, 16.f};
	written.material = "steel/galvanised";
	auto const text{road_barrier::format(written)};
	CHECK(text == "left:wbeam:1.25:40:120.5:ramp:8:cap:16:steel/galvanised");
	// a single token, so a parser which doesn't know it stays in step
	CHECK(text.find(' ') == std::string::npos);
	entry read;
	REQUIRE(road_barrier::parse(text, read));
	CHECK(read == written);
	CHECK(road_barrier::format(read) == text);
}

TEST_CASE("road barrier: what isn't an entry is refused and leaves nothing behind") {
	entry read;
	auto const untouched{read};
	CHECK_FALSE(road_barrier::parse("", read));
	CHECK_FALSE(road_barrier::parse("right:wbeam:1:0:-1:flare:12:flare:12", read)); // a field short
	CHECK_FALSE(road_barrier::parse("right:wbeam:1:0:-1:flare:12:flare:12:none:more", read));
	CHECK_FALSE(road_barrier::parse("median:wbeam:1:0:-1:flare:12:flare:12:none", read));
	CHECK_FALSE(road_barrier::parse("right:rope:1:0:-1:flare:12:flare:12:none", read));
	CHECK_FALSE(road_barrier::parse("right:wbeam:1:0:-1:bent:12:flare:12:none", read));
	CHECK_FALSE(road_barrier::parse("right:wbeam:one:0:-1:flare:12:flare:12:none", read));
	CHECK_FALSE(road_barrier::parse("right:wbeam:1:0:nan:flare:12:flare:12:none", read));
	CHECK_FALSE(road_barrier::parse("right:wbeam:1:0:inf:flare:12:flare:12:none", read));
	CHECK(read == untouched);
}

TEST_CASE("road barrier: values out of their limits are brought within them") {
	entry read;
	REQUIRE(road_barrier::parse("right:wbeam:0:-5:-3:flare:500:ramp:-2:none", read));
	CHECK(read.offset == 0.25f);
	CHECK(read.from == 0.f);
	CHECK(read.to == -1.f);
	CHECK(read.lengths[0] == 30.f);
	CHECK(read.lengths[1] == 0.f);
}

TEST_CASE("road barrier: the barriers of a piece are kept in one form, and making it again changes nothing") {
	std::vector<entry> barriers{barrier(road_barrier::side::right, 60.f, 99.9f), barrier(road_barrier::side::right, 0.2f, 60.f), barrier(road_barrier::side::left, 10.f, 10.3f),
	                            barrier(road_barrier::side::left, 20.f, 150.f)};
	road_barrier::normalize(barriers, 100.0);
	// the two on the right touch and are one, from the start to the end of the piece; the short one on the left is gone
	REQUIRE(barriers.size() == 2);
	CHECK(barriers[0].where == road_barrier::side::left);
	CHECK(barriers[0].from == 20.f);
	CHECK(barriers[0].to == -1.f);
	CHECK(barriers[1].where == road_barrier::side::right);
	CHECK(barriers[1].from == 0.f);
	CHECK(barriers[1].to == -1.f);
	auto again{barriers};
	road_barrier::normalize(again, 100.0);
	CHECK(again == barriers);
	// with no axis to fit them to they're left where they are
	std::vector<entry> loose{barrier(road_barrier::side::right, 40.f, 80.f)};
	road_barrier::normalize(loose, 0.0);
	REQUIRE(loose.size() == 1);
	CHECK(loose[0].to == 80.f);
}

TEST_CASE("road barrier: barriers which differ aren't made one") {
	auto closer{barrier(road_barrier::side::right, 0.f, 50.f)};
	auto further{barrier(road_barrier::side::right, 50.f, -1.f)};
	further.offset = 2.f;
	std::vector<entry> barriers{closer, further};
	road_barrier::normalize(barriers, 100.0);
	CHECK(barriers.size() == 2);
}

TEST_CASE("road barrier: a piece cut in two and made one again has the barriers it had") {
	std::vector<entry> const whole{barrier(road_barrier::side::left, 0.f, -1.f), barrier(road_barrier::side::right, 30.f, 70.f)};
	for (auto const cut : {10.0, 35.0, 50.0, 65.0, 90.0}) {
		CAPTURE(cut);
		auto const first{road_barrier::part(whole, 100.0, 0.0, cut)};
		auto const second{road_barrier::part(whole, 100.0, cut, 100.0)};
		// the one along the whole piece is along the whole of each part, with nothing to work out
		REQUIRE_FALSE(first.empty());
		REQUIRE_FALSE(second.empty());
		CHECK(first[0].from == 0.f);
		CHECK(first[0].to == -1.f);
		CHECK(second[0].from == 0.f);
		CHECK(second[0].to == -1.f);
		auto const back{road_barrier::joined(first, cut, second, 100.0)};
		REQUIRE(back.size() == 2);
		CHECK(back[0] == whole[0]);
		CHECK(back[1].where == road_barrier::side::right);
		CHECK(back[1].from == doctest::Approx(30.0).epsilon(0.001));
		CHECK(back[1].to == doctest::Approx(70.0).epsilon(0.001));
	}
}

TEST_CASE("road barrier: a part without a barrier has none") {
	std::vector<entry> const whole{barrier(road_barrier::side::right, 30.f, 70.f)};
	CHECK(road_barrier::part(whole, 100.0, 0.0, 29.8).empty());
	CHECK(road_barrier::part(whole, 100.0, 70.2, 100.0).empty());
	auto const middle{road_barrier::part(whole, 100.0, 40.0, 60.0)};
	REQUIRE(middle.size() == 1);
	CHECK(middle[0].from == 0.f);
	CHECK(middle[0].to == -1.f);
}

TEST_CASE("road barrier: turned around twice the barriers are what they were") {
	auto one{barrier(road_barrier::side::left, 0.f, 40.f)};
	one.endings = {road_barrier::ending::ramp, road_barrier::ending::flare};
	one.lengths = {4.f, 16.f};
	std::vector<entry> const barriers{one, barrier(road_barrier::side::right, 25.f, -1.f)};
	auto const turned{road_barrier::flipped(barriers, 100.0)};
	REQUIRE(turned.size() == 2);
	// what was on the left from the start is on the right up to the end, its ends swapped
	auto const &moved{turned[1]};
	CHECK(moved.where == road_barrier::side::right);
	CHECK(moved.from == 60.f);
	CHECK(moved.to == -1.f);
	CHECK(moved.endings[0] == road_barrier::ending::flare);
	CHECK(moved.lengths[0] == 16.f);
	CHECK(turned[0].where == road_barrier::side::left);
	CHECK(turned[0].from == 0.f);
	CHECK(turned[0].to == 75.f);
	CHECK(road_barrier::flipped(turned, 100.0) == barriers);
}

TEST_CASE("road barrier: the rail is 0.75 m over the road, a metre from the edge of the lanes, on the side it's put on") {
	for (auto const left : {false, true}) {
		CAPTURE(left);
		auto layout{straight(0.0, 100.0, left)};
		layout.ends[0].distant = true;
		layout.ends[1].distant = true;
		road_barrier::mesh mesh;
		road_barrier::build(layout, mesh);
		REQUIRE_FALSE(mesh.rail.empty());
		CHECK(finite(mesh.rail));
		CHECK(finite(mesh.posts));
		CHECK(mesh.rail.size() % 3 == 0);
		auto const rail{heights_between(mesh.rail, 0.0, 100.0)};
		CHECK(rail.top == doctest::Approx(road_barrier::rail_top));
		CHECK(rail.bottom == doctest::Approx(road_barrier::rail_top - road_barrier::rail_height));
		// its face is at the offset, the rest of it and the posts are further from the road
		auto nearest{std::numeric_limits<double>::max()};
		for (auto const &vertex : mesh.rail)
			nearest = std::min(nearest, left ? vertex.position.x : -vertex.position.x);
		CHECK(nearest == doctest::Approx(4.5));
		for (auto const &vertex : mesh.posts)
			CHECK((left ? vertex.position.x : -vertex.position.x) > 4.5);
		// the posts get into the ground, and no higher than the rail
		auto const posts{heights_between(mesh.posts, 0.0, 100.0)};
		CHECK(posts.bottom < -0.5);
		CHECK(posts.top <= road_barrier::rail_top + 1e-9);
	}
}

TEST_CASE("road barrier: where it ends the rail is led into the ground, and away from the road if it's told to") {
	auto layout{straight(0.0, 100.0)};
	layout.ends[0].style = road_barrier::ending::flare;
	layout.ends[0].length = 12.0;
	layout.ends[1].style = road_barrier::ending::ramp;
	layout.ends[1].length = 8.0;
	// at its very ends nothing of it stands out of the ground
	CHECK(road_barrier::profile_at(layout, 0.0).rise == doctest::Approx(0.0));
	CHECK(road_barrier::profile_at(layout, 100.0).rise == doctest::Approx(0.0));
	CHECK(road_barrier::profile_at(layout, 50.0).rise == doctest::Approx(1.0));
	CHECK(road_barrier::profile_at(layout, 6.0).rise == doctest::Approx(0.5));
	// a metre away from the road for each 20 m along it
	CHECK(road_barrier::profile_at(layout, 0.0).away == doctest::Approx(12.0 / road_barrier::flare_rate));
	CHECK(road_barrier::profile_at(layout, 12.0).away == doctest::Approx(0.0));
	CHECK(road_barrier::profile_at(layout, 100.0).away == doctest::Approx(0.0));
	CHECK(road_barrier::line_point(layout, 0.0).x == doctest::Approx(-(4.5 + 0.6)));
	road_barrier::mesh mesh;
	road_barrier::build(layout, mesh);
	CHECK(finite(mesh.rail));
	CHECK(heights_between(mesh.rail, 0.0, 0.01).top < 0.0);
	CHECK(heights_between(mesh.rail, 99.99, 100.0).top < 0.0);
	CHECK(heights_between(mesh.rail, 40.0, 60.0).top == doctest::Approx(road_barrier::rail_top));
}

TEST_CASE("road barrier: an end on another piece shapes the barrier of this one") {
	// the barrier ends 5 m past the end of the piece, with 12 m of it led down
	auto layout{straight(0.0, 20.0)};
	layout.ends[0].distant = true;
	layout.ends[1].beyond = 5.0;
	layout.ends[1].style = road_barrier::ending::ramp;
	layout.ends[1].length = 12.0;
	CHECK(road_barrier::profile_at(layout, 20.0).rise == doctest::Approx(5.0 / 12.0));
	CHECK(road_barrier::profile_at(layout, 13.0).rise == doctest::Approx(1.0));
	CHECK(road_barrier::profile_at(layout, 0.0).rise == doctest::Approx(1.0));
}

TEST_CASE("road barrier: a barrier shorter than its ends has them made shorter, not crossed") {
	auto layout{straight(0.0, 6.0)};
	layout.ends[0].length = 12.0;
	layout.ends[1].length = 12.0;
	for (int step = 0; step <= 12; ++step) {
		auto const profile{road_barrier::profile_at(layout, 0.5 * step)};
		CAPTURE(step);
		CHECK(profile.rise >= 0.0);
		CHECK(profile.rise <= 1.0);
		CHECK(profile.away >= 0.0);
	}
	CHECK(road_barrier::profile_at(layout, 3.0).rise == doctest::Approx(1.0));
	CHECK(road_barrier::profile_at(layout, 0.0).rise == doctest::Approx(0.0));
	road_barrier::mesh mesh;
	road_barrier::build(layout, mesh);
	CHECK(finite(mesh.rail));
	CHECK(finite(mesh.posts));
}

TEST_CASE("road barrier: the post two pieces share is put by one of them") {
	auto const posts_at = [](road_barrier::layout const &Layout, double const Station) {
		road_barrier::mesh mesh;
		road_barrier::build(Layout, mesh);
		return heights_between(mesh.posts, Station - 0.2, Station + 0.2).count;
	};
	auto layout{straight(0.0, 20.0)};
	layout.ends[0].distant = true;
	layout.ends[1].distant = true;
	CHECK(posts_at(layout, 20.0) > 0);
	CHECK(posts_at(layout, 10.0) > 0);
	layout.posted[1] = false;
	CHECK(posts_at(layout, 20.0) == 0);
	CHECK(posts_at(layout, 0.0) > 0);
}

TEST_CASE("road barrier: nothing is made of what can't be a barrier") {
	road_barrier::mesh mesh;
	road_barrier::build(straight(10.0, 10.2), mesh);
	road_barrier::build(straight(10.0, 5.0), mesh);
	road_barrier::build(straight(0.0, std::numeric_limits<double>::infinity()), mesh);
	road_barrier::build(straight(std::numeric_limits<double>::quiet_NaN(), 10.0), mesh);
	road_barrier::layout empty;
	empty.to = 50.0;
	road_barrier::build(empty, mesh);
	auto unknown{straight(0.0, 50.0)};
	unknown.type = road_barrier::kind::concrete;
	road_barrier::build(unknown, mesh);
	CHECK(mesh.rail.empty());
	CHECK(mesh.posts.empty());
}
