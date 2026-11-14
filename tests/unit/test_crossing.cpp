#include "stdafx.h"

#include "doctest.h"

#include "editor/editorCrossing.hpp"

#include <algorithm>
#include <cmath>

namespace
{

// an embankment of a track at Angle degrees to the road along x, its crown 7 m wide at Formation, the slopes 1:1.5, the
// ground and the road at 0
crossing::site embankment(double const Formation, double const Angle)
{
	crossing::site site;
	auto const a{glm::radians(Angle)};
	site.track = {std::cos(a), std::sin(a)};
	site.formation = Formation;
	site.ground = [site](glm::dvec2 const &P, double &Height) {
		glm::dvec2 const m{-site.track.y, site.track.x};
		auto const d{std::abs(glm::dot(P, m))};
		Height = std::max(0.0, site.formation - std::max(0.0, d - site.half_width) / site.slope);
		return true;
	};
	return site;
}

bool in_triangle(glm::dvec2 const &P, glm::dvec2 const &A, glm::dvec2 const &B, glm::dvec2 const &C)
{
	auto const side = [](glm::dvec2 const &U, glm::dvec2 const &V, glm::dvec2 const &W) { return (V.x - U.x) * (W.y - U.y) - (W.x - U.x) * (V.y - U.y); };
	auto const a{side(A, B, P)}, b{side(B, C, P)}, c{side(C, A, P)};
	return (a >= -1e-9 && b >= -1e-9 && c >= -1e-9) || (a <= 1e-9 && b <= 1e-9 && c <= 1e-9);
}

bool in_outline(std::vector<glm::dvec2> const &Outline, glm::dvec2 const &P)
{
	bool inside{false};
	for (std::size_t i = 0, j = Outline.size() - 1; i < Outline.size(); j = i++)
	{
		auto const &a{Outline[i]}, &b{Outline[j]};
		if ((a.y > P.y) != (b.y > P.y) && P.x < (b.x - a.x) * (P.y - a.y) / (b.y - a.y) + a.x)
			inside = !inside;
	}
	return inside;
}

// every point of the holes, and half a metre round them, over a floor of the model (the pipe's own floor, or the opening's)
bool floor_under_holes(crossing::result const &Made)
{
	std::vector<std::array<glm::dvec2, 3>> floor;
	for (auto const &part : Made.mesh.parts())
		if (part.name == "dno")
			for (std::size_t i = 0; i + 2 < part.points.size(); i += 3)
				floor.push_back({glm::dvec2{part.points[i].x, part.points[i].z}, glm::dvec2{part.points[i + 1].x, part.points[i + 1].z}, glm::dvec2{part.points[i + 2].x, part.points[i + 2].z}});
	for (auto const &hole : Made.holes)
	{
		glm::dvec2 low{hole.front()}, high{hole.front()};
		for (auto const &p : hole)
		{
			low = glm::min(low, p);
			high = glm::max(high, p);
		}
		for (double z = low.y; z <= high.y; z += 0.25)
			for (double x = low.x; x <= high.x; x += 0.25)
			{
				glm::dvec2 const p{x, z};
				if (false == in_outline(hole, p))
					continue;
				// the floor reaches a little past the hole, so no edge of it is seen at the edge of the ground laid round the hole
				for (auto const &offset : {glm::dvec2{0.0}, glm::dvec2{0.25, 0.0}, glm::dvec2{-0.25, 0.0}, glm::dvec2{0.0, 0.25}, glm::dvec2{0.0, -0.25}})
					if (std::none_of(floor.begin(), floor.end(), [&](auto const &T) { return in_triangle(p + offset, T[0], T[1], T[2]); }))
						return false;
			}
	}
	return true;
}

} // namespace

TEST_CASE("crossing: every kind is built on a high embankment, its holes over a floor of its own")
{
	for (int kind = 0; kind < static_cast<int>(crossing::kind::count); ++kind)
		for (auto const angle : {90.0, 70.0})
		{
			CAPTURE(kind);
			CAPTURE(angle);
			crossing::options options;
			options.type = static_cast<crossing::kind>(kind);
			auto const made{crossing::build(embankment(6.5, angle), options)};
			REQUIRE(made.built);
			CHECK(made.mesh.triangles() > 100);
			CHECK(false == made.holes.empty());
			for (auto const &hole : made.holes)
				CHECK(hole.size() >= 3);
			CHECK(floor_under_holes(made));
			// the faces where the slopes come down to the structure: no longer than the embankment and its wings
			CHECK(made.length < 2.0 * (3.5 + 6.5 * 1.5) / std::sin(glm::radians(angle)) + 16.0);
		}
}

TEST_CASE("crossing: a structure too deep for a low embankment is refused, saying by how much; a through girder bridge fits")
{
	crossing::options arch;
	arch.type = crossing::kind::arch;
	auto const refused{crossing::build(embankment(3.4, 90.0), arch)};
	CHECK(false == refused.built);
	CHECK(refused.refused == crossing::refusal::too_low);
	CHECK(refused.short_by > 0.0);
	crossing::options through;
	through.type = crossing::kind::through_girders;
	auto const built{crossing::build(embankment(3.4, 90.0), through)};
	CHECK(built.built);
	CHECK(built.lowered);
	CHECK(built.clearance >= crossing::kLeastClearance);
	CHECK(crossing::depth_under(crossing::kind::through_girders, 8.0) < crossing::depth_under(crossing::kind::deck_girders, 8.0));
}

TEST_CASE("crossing: a road along the track gets no structure")
{
	crossing::options options;
	auto const made{crossing::build(embankment(6.0, 10.0), options)};
	CHECK(false == made.built);
	CHECK(made.refused == crossing::refusal::along_track);
}

TEST_CASE("crossing: two tracks get the girders outside both of them, never between")
{
	for (auto const type : {crossing::kind::through_girders, crossing::kind::truss, crossing::kind::deck_girders})
	{
		auto site{embankment(6.5, 90.0)};
		site.axes = {-2.25, 2.25};
		site.half_width = 2.25 + 3.5;
		crossing::options options;
		options.type = type;
		options.clearance = 4.0;
		auto const made{crossing::build(site, options)};
		REQUIRE(made.built);
		glm::dvec2 const m{-site.track.y, site.track.x};
		for (auto const &part : made.mesh.parts())
		{
			if (part.name != "dzwigary" && part.name != "kratownice" && part.name != "balustrady")
				continue;
			for (auto const &p : part.points)
			{
				if (p.y < site.formation + 0.1)
					continue; // under the trough: the cross beams go from one girder to the other
				auto const d{glm::dot(glm::dvec2{p.x, p.z}, m)};
				// over the formation steel stands only outside the tracks, 2 m off the outer axes at least
				CHECK(std::abs(d) > 2.25 + 1.9);
			}
		}
	}
}

TEST_CASE("crossing: the ground laid round a hole covers its cells outside it, on the terrain's planes")
{
	// a cell 2 m wide sloping along x, a hole over its left half
	crossing::ground_cell cell{{0.0, 0.0}, 2.0, {1.0, 3.0, 1.0, 3.0}};
	std::vector<std::vector<glm::dvec2>> const holes{{{-1.0, -1.0}, {1.0, -1.0}, {1.0, 3.0}, {-1.0, 3.0}}};
	auto const triangles{crossing::ground_outside({cell}, holes)};
	REQUIRE(triangles.size() % 3 == 0);
	double area{0.0};
	for (std::size_t i = 0; i < triangles.size(); i += 3)
	{
		auto const &a{triangles[i]}, &b{triangles[i + 1]}, &c{triangles[i + 2]};
		area += 0.5 * std::abs((b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z));
		for (auto const *p : {&a, &b, &c})
		{
			CHECK(p->x >= 1.0 - 1e-9); // nothing inside the hole
			CHECK(p->y == doctest::Approx(1.0 + p->x)); // on the plane of the terrain
		}
	}
	CHECK(area == doctest::Approx(2.0));
	// no hole: the whole cell; a hole over all of it: nothing
	auto const whole{crossing::ground_outside({cell}, {})};
	CHECK(whole.size() == 6);
	CHECK(crossing::ground_outside({cell}, {{{-1.0, -1.0}, {3.0, -1.0}, {3.0, 3.0}, {-1.0, 3.0}}}).empty());
}

TEST_CASE("crossing: the holes are convex, so the ground laid round them is cut right")
{
	for (int type = 0; type < static_cast<int>(crossing::kind::count); ++type)
		for (auto const angle : {90.0, 70.0, 55.0})
			for (auto const height : {6.5, 9.0})
			{
				auto const site{embankment(height, angle)};
				crossing::options options;
				options.type = static_cast<crossing::kind>(type);
				options.clearance = 4.0;
				auto const made{crossing::build(site, options)};
				if (false == made.built)
					continue;
				for (auto const &hole : made.holes)
				{
					REQUIRE(hole.size() >= 3);
					int turn{0};
					bool convex{true};
					for (std::size_t i = 0; i < hole.size(); ++i)
					{
						auto const &a{hole[i]}, &b{hole[(i + 1) % hole.size()]}, &c{hole[(i + 2) % hole.size()]};
						auto const cross{(b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x)};
						if (std::abs(cross) < 1e-9)
							continue;
						int const sign{cross > 0.0 ? 1 : -1};
						if (turn == 0)
							turn = sign;
						else if (sign != turn)
							convex = false;
					}
					CHECK(convex);
				}
			}
}
