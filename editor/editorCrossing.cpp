/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "editor/editorCrossing.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace crossing
{

namespace
{

glm::dvec3 const kUp{0.0, 1.0, 0.0};
glm::ivec3 const kSteel{70, 78, 72};
glm::ivec3 const kRubber{35, 35, 35};
glm::ivec3 const kRust{96, 58, 40};
double constexpr kPi{3.14159265358979323846};
// m the edge of a hole lies inside a solid part from its face to the ground: the mask of the holes is a metre or so fine,
// its edge may lie half a metre either way of where it's asked to
double constexpr kInset{0.45};
double constexpr kFloorDrop{0.06}; // m the floor of the opening lies under the road
double constexpr kFooting{0.5}; // m the walls go under the floor
double constexpr kCap{1.0}; // m the top of a headwall reaches back into the embankment, over the edge of the hole behind it
double constexpr kCoping{0.25}; // m the cornice on top of a headwall
double constexpr kWingTop{0.08}; // m a wing stands over the ground beside it
double constexpr kBedBlock{0.25}; // m of the block of concrete a bearing stands on
double constexpr kBearing{0.12}; // m of a bearing
double constexpr kTrough{0.06}; // m of the plate of a ballast trough under the formation

bool open_kind(kind const Kind)
{
	return Kind == kind::deck_girders || Kind == kind::through_girders || Kind == kind::truss;
}

glm::dvec2 perpendicular(glm::dvec2 const &V)
{
	return {-V.y, V.x};
}

// the plan and the heights around the crossing: x across the road (to its left), s along it from the crossing, d the
// distance from the axis of the track
struct frame
{
	site const &c;
	glm::dvec2 origin;
	glm::dvec2 n; // across the road, the way x grows
	glm::dvec2 r; // along the road, the way s grows
	glm::dvec2 m; // across the track, the way d grows
	double rm;
	double nm;
	glm::dvec3 across;
	glm::dvec3 along;
	glm::dvec3 trackside; // the way d grows

	explicit frame(site const &Site)
	    : c{Site}, origin{Site.at.x, Site.at.z}, n{perpendicular(Site.road)}, r{Site.road}, m{perpendicular(Site.track)}, rm{glm::dot(r, m)}, nm{glm::dot(n, m)},
	      across{n.x, 0.0, n.y}, along{r.x, 0.0, r.y}, trackside{glm::normalize(along * rm + across * nm)}
	{
	}
	glm::dvec2 place(double const X, double const S) const { return origin + n * X + r * S; }
	glm::dvec3 point(double const X, double const S, double const Y) const
	{
		auto const p{place(X, S)};
		return {p.x, Y, p.y};
	}
	// where along the road the line at the distance D from the axis of the track lies, for a place across the road
	double s_at(double const X, double const D) const { return (D - X * nm) / rm; }
	glm::dvec3 node(double const X, double const D, double const Y) const { return point(X, s_at(X, D), Y); }
	double road(double const S) const { return c.at.y + c.grade * S; }
	double floor(double const S) const { return road(S) - kFloorDrop; }
	double x_of(glm::dvec2 const &P) const { return glm::dot(P - origin, n); }
	double s_of(glm::dvec2 const &P) const { return glm::dot(P - origin, r); }
	// the way along the road out of the structure past its face on the side Sign of the track
	double outward(int const Sign) const { return Sign * (rm > 0.0 ? 1.0 : -1.0); }
	bool ground(glm::dvec2 const &P, double &Height) const { return c.ground && c.ground(P, Height); }
	glm::dvec3 lift(glm::dvec2 const &P, double const Y) const { return {P.x, Y, P.y}; }
};

// the parts of the model and the builders putting faces into them
struct parts
{
	structure::model mesh;
	double tile;
	std::string material;
	std::string steel;
	structure::builder build(std::string const &Name, bool const Masonry, bool const Smooth = false)
	{
		auto const &texture{Masonry ? material : steel};
		auto const colour{Masonry || false == steel.empty() ? glm::ivec3{255, 255, 255} : kSteel};
		return {&mesh.add(Name, texture, colour, Smooth), tile};
	}
	structure::builder plain(std::string const &Name, glm::ivec3 const &Colour) { return {&mesh.add(Name, "", Colour), tile}; }
};

// a block standing on the plan between four points, from Bottom to Top
void block(structure::builder const &Builder, std::array<glm::dvec2, 4> const &Plan, double const Bottom, double const Top)
{
	Builder.solid({glm::dvec3{Plan[0].x, Bottom, Plan[0].y}, glm::dvec3{Plan[1].x, Bottom, Plan[1].y}, glm::dvec3{Plan[2].x, Bottom, Plan[2].y}, glm::dvec3{Plan[3].x, Bottom, Plan[3].y},
	               glm::dvec3{Plan[0].x, Top, Plan[0].y}, glm::dvec3{Plan[1].x, Top, Plan[1].y}, glm::dvec3{Plan[2].x, Top, Plan[2].y}, glm::dvec3{Plan[3].x, Top, Plan[3].y}});
}

// a plate standing up from a line: Width along the line, Thick across it, from Bottom to Top, its middle at Middle
void plate(structure::builder const &Builder, glm::dvec2 const &Middle, glm::dvec2 const &Along, double const Width, double const Thick, double const Bottom, double const Top)
{
	auto const a{Along * (0.5 * Width)};
	auto const t{perpendicular(Along) * (0.5 * Thick)};
	block(Builder, {Middle - a - t, Middle + a - t, Middle + a + t, Middle - a + t}, Bottom, Top);
}

// the outline moved out by Margin all round, its corners mitred (no further than three times the margin)
std::vector<glm::dvec2> grown(std::vector<glm::dvec2> const &Outline, double const Margin)
{
	auto const count{Outline.size()};
	if (count < 3)
		return Outline;
	double area{0.0};
	for (std::size_t i = 0; i < count; ++i)
	{
		auto const &a{Outline[i]};
		auto const &b{Outline[(i + 1) % count]};
		area += a.x * b.y - b.x * a.y;
	}
	auto const outward = [&](glm::dvec2 const &A, glm::dvec2 const &B) {
		auto const edge{B - A};
		auto const length{glm::length(edge)};
		if (length < 1e-9)
			return glm::dvec2{0.0};
		// counter-clockwise (area > 0): out is to the right of the way round
		auto const right{glm::dvec2{edge.y, -edge.x} / length};
		return area > 0.0 ? right : -right;
	};
	std::vector<glm::dvec2> result;
	result.reserve(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		auto const &previous{Outline[(i + count - 1) % count]};
		auto const &here{Outline[i]};
		auto const &next{Outline[(i + 1) % count]};
		auto const n1{outward(previous, here)}, n2{outward(here, next)};
		auto way{n1 + n2};
		if (glm::length(way) < 1e-9)
			way = n1;
		way = glm::normalize(way);
		auto const along{std::max(glm::dot(way, glm::length(n1) > 0.0 ? n1 : n2), 1.0 / 3.0)};
		result.push_back(here + way * (Margin / along));
	}
	return result;
}

// the part of the polygon inside the outline (convex), cut along its edges; as one piece in a list, none if they don't meet
std::vector<std::vector<glm::dvec2>> inside_of(std::vector<glm::dvec2> const &Polygon, std::vector<glm::dvec2> const &Outline)
{
	double area{0.0};
	for (std::size_t i = 0; i < Outline.size(); ++i)
		area += Outline[i].x * Outline[(i + 1) % Outline.size()].y - Outline[(i + 1) % Outline.size()].x * Outline[i].y;
	auto const keep{area >= 0.0 ? 1.0 : -1.0};
	auto rest{Polygon};
	for (std::size_t i = 0; i < Outline.size() && rest.size() >= 3; ++i)
	{
		auto const &a{Outline[i]};
		auto const &b{Outline[(i + 1) % Outline.size()]};
		std::vector<glm::dvec2> next;
		auto const side = [&](glm::dvec2 const &P) { return keep * ((b.x - a.x) * (P.y - a.y) - (b.y - a.y) * (P.x - a.x)); };
		for (std::size_t j = 0; j < rest.size(); ++j)
		{
			auto const &p{rest[j]};
			auto const &q{rest[(j + 1) % rest.size()]};
			auto const sp{side(p)}, sq{side(q)};
			if (sp >= 0.0)
				next.push_back(p);
			if ((sp >= 0.0) != (sq >= 0.0))
				next.push_back(p + (q - p) * (sp / (sp - sq)));
		}
		rest = std::move(next);
	}
	if (rest.size() < 3)
		return {};
	return {rest};
}

// what the kinds are made of, from the span along the track
struct dimensions
{
	double wall{1.0}; // of the abutments and the wings
	double depth{0.6}; // under the formation, over the soffit
	double pier{0.9};
	double splay{0.0}; // radians the wings turn out of the line of the road
	double headwall{0.6}; // of the face, into the embankment
};

dimensions dimensions_of(kind const Kind, double const Span)
{
	dimensions d;
	switch (Kind)
	{
	case kind::pipe:
		d.wall = 0.0;
		d.depth = 0.6; // the steel and the earth over the crown
		break;
	case kind::box:
		d.wall = 0.8;
		d.depth = 0.3 + 0.3 + 0.3; // the slab, the headwall over it, the earth over that
		d.splay = glm::radians(30.0);
		d.headwall = 0.4;
		break;
	case kind::arch:
		d.wall = 1.2;
		d.depth = std::clamp(0.3 + Span / 25.0, 0.35, 0.8) + 0.5; // the ring at the crown and the masonry and the fill over it
		d.pier = 1.2;
		d.splay = glm::radians(15.0);
		d.headwall = 0.9;
		break;
	case kind::frame:
		d.wall = 0.9;
		d.depth = std::clamp(0.35 + Span / 20.0, 0.45, 1.0) + 0.02;
		break;
	case kind::slab:
		d.wall = 1.3;
		d.splay = glm::radians(20.0);
		d.depth = std::clamp(0.25 + Span / 16.0, 0.5, 1.2) + 0.02;
		d.headwall = 0.5;
		break;
	case kind::deck_girders:
		d.wall = 1.7;
		d.depth = std::clamp(Span / 11.0, 0.8, 2.6) + kTrough;
		break;
	case kind::through_girders:
		d.wall = 1.7;
		d.depth = 0.55;
		break;
	case kind::truss:
		d.wall = 1.7;
		d.depth = 0.66;
		break;
	case kind::count:
		break;
	}
	return d;
}

class maker
{
  public:
	maker(site const &Site, options const &Options, result &Out) : f{Site}, o{Options}, out{Out}, k{Options.type}
	{
		mesh.tile = std::clamp(Options.tile, 0.25, 50.0);
		mesh.material = Options.material;
		mesh.steel = Options.steel;
	}

	void run()
	{
		if (std::abs(f.rm) < std::sin(glm::radians(20.0)))
		{
			out.refused = refusal::along_track;
			return;
		}
		lay_out();
		if (false == settle_height())
			return;
		if (k == kind::pipe)
			pipe();
		else
		{
			find_faces();
			find_wings();
			terrain();
			floor();
			walls();
			wings();
			if (open_kind(k))
				open_bridge();
			else
				buried();
		}
		out.mesh = std::move(mesh.mesh);
		out.built = true;
	}

  private:
	frame f;
	options const &o;
	result &out;
	kind k;
	parts mesh;
	dimensions dim;
	double half{3.5}; // between the walls
	int spans{1};
	double clear{7.0}; // of a span, across the road
	double span{7.0}; // of a span, along the track
	double high{0.0}; // the road at its highest under the structure
	double soffit{0.0};
	double top{0.0}; // of the faces, or of the abutments of a bridge
	std::array<double, 2> reach{3.5, 3.5}; // the faces, or the ends of the abutments, from the axis of the track, by side of it
	struct wing
	{
		glm::dvec2 start; // the inner edge where it leaves the face
		glm::dvec2 way; // along it, outward
		glm::dvec2 out; // to the ground behind it
		double length{0.0};
	};
	std::array<std::array<wing, 2>, 2> wing_of{}; // [side of the track][side of the road]
	double deck_half{3.5}; // of a ballast trough
	// the superstructures of a bridge side by side: one under all the tracks, or one for each group of them standing apart; a
	// bridge with the track between its girders has them outside the tracks, never between two of them
	struct deck
	{
		double centre{0.0}; // from the axis of the site
		double half{3.5}; // of its trough, the girders or the trusses right outside it
	};
	std::vector<deck> decks;
	double deck_top{0.0}; // of the deck of a frame or a slab

	double span_start(int const Span) const { return -half + Span * (clear + dim.pier); }
	int side_index(int const Sign) const { return Sign < 0 ? 0 : 1; }
	double reach_of(int const Sign) const { return reach[side_index(Sign)]; }

	void lay_out()
	{
		half = 0.5 * (o.width > 0.5 ? o.width : f.c.width + 0.5);
		spans = k == kind::pipe ? 1 : std::clamp(o.spans, 1, 6);
		dim = dimensions_of(k, 2.0 * half / std::abs(f.rm));
		while (spans > 1 && (2.0 * half - (spans - 1) * dim.pier) / spans < 2.5)
			--spans;
		clear = (2.0 * half - (spans - 1) * dim.pier) / spans;
		span = clear / std::abs(f.rm);
		dim = dimensions_of(k, span);
		deck_half = std::max(1.6, f.c.half_width);
		decks.clear();
		auto axes{f.c.axes.empty() ? std::vector<double>{0.0} : f.c.axes};
		std::sort(axes.begin(), axes.end());
		if (k == kind::through_girders || k == kind::truss)
		{
			// m from the axis of an outer track to the inner face of the girder beside it: the gauge and a walkway
			double constexpr beside_track{2.2};
			// tracks closer than that are carried by one superstructure, the girders outside them all; further apart each
			// group has its own, the ground between them open
			double constexpr apart{8.0};
			std::size_t first{0};
			for (std::size_t i = 1; i <= axes.size(); ++i)
				if (i == axes.size() || axes[i] - axes[i - 1] > apart)
				{
					auto const low{axes[first]}, highest{axes[i - 1]};
					decks.push_back({0.5 * (low + highest), 0.5 * (highest - low) + beside_track});
					first = i;
				}
		}
		else
			decks.push_back({0.0, deck_half});
	}

	// the clearance there's room for under the formation, and the soffit
	bool settle_height()
	{
		// the road at its highest under the structure: over the embankment's width at the height of the road, both ways
		auto const toe{f.c.half_width + std::max(0.0, f.c.formation - f.c.at.y) * f.c.slope};
		double extent{0.0};
		for (auto const x : {-(half + dim.wall), half + dim.wall})
			for (auto const d : {-toe, toe})
				extent = std::max(extent, std::abs(f.s_at(x, d)));
		high = f.c.at.y + std::abs(f.c.grade) * extent;
		out.room = f.c.formation - high - dim.depth;
		auto clearance{std::max(1.5, o.clearance)};
		if (out.room < clearance)
		{
			auto const least{o.fit ? kLeastClearance : clearance};
			if (out.room < least)
			{
				out.refused = refusal::too_low;
				out.short_by = least - out.room;
				return false;
			}
			out.lowered = true;
			clearance = out.room;
		}
		out.clearance = clearance;
		soffit = high + clearance;
		top = open_kind(k) ? f.c.formation - 0.02 : std::min(f.c.formation - 0.02, soffit + dim.depth - 0.3);
		// a frame or a slab: its deck as thick as it is, a headwall over it holding the foot of the slope where the embankment is
		// higher; right under the ballast where it isn't
		if (k == kind::frame || k == kind::slab)
		{
			deck_top = soffit + dim.depth - 0.02;
			top = deck_top + 0.6 >= f.c.formation - 0.02 ? f.c.formation - 0.02 : deck_top + 0.6;
		}
		return true;
	}

	// the faces where the embankment comes down to the top of the structure, by the ground as it is; a bridge: where its
	// abutments end beside the deck
	void find_faces()
	{
		if (open_kind(k))
		{
			auto const beside{k == kind::deck_girders ? 0.55 : k == kind::through_girders ? 0.95 : 1.15};
			reach = {0.0, 0.0};
			for (auto const &one : decks)
			{
				reach[0] = std::max(reach[0], one.half - one.centre + beside);
				reach[1] = std::max(reach[1], one.half + one.centre + beside);
			}
			return;
		}
		auto const toe{f.c.half_width + std::max(0.0, f.c.formation - f.c.at.y) * f.c.slope + 6.0};
		auto const sloped{f.c.half_width + std::max(0.0, f.c.formation - top) * f.c.slope};
		for (int sign : {-1, 1})
		{
			double found{0.0};
			bool any{false};
			auto const from{std::max(0.5, f.c.half_width - 1.0)};
			int const steps{static_cast<int>((toe - from) / 0.25)};
			for (auto const x : {-(half + dim.wall), 0.0, half + dim.wall})
				for (int step = 0; step < steps; ++step)
				{
					auto const d{from + 0.25 * step};
					double ground{0.0};
					if (false == f.ground(f.place(x, f.s_at(x, sign * d)), ground))
						break;
					any = true;
					if (ground < top - 0.05)
					{
						found = std::max(found, d);
						break;
					}
				}
			reach[side_index(sign)] = any && found > 0.0 ? found : sloped;
		}
	}

	// the wings out of each face along the road, turned out by the splay, as far as the ground beside them comes down to the road
	void find_wings()
	{
		for (int sign : {-1, 1})
			for (int side : {-1, 1})
			{
				auto &w{wing_of[side_index(sign)][side_index(side)]};
				auto const x{side * half};
				w.start = f.place(x, f.s_at(x, sign * reach_of(sign)));
				auto const ahead{f.r * f.outward(sign)};
				w.way = glm::normalize(ahead * std::cos(dim.splay) + f.n * (side * std::sin(dim.splay)));
				w.out = perpendicular(w.way);
				if (glm::dot(w.out, f.n * static_cast<double>(side)) < 0.0)
					w.out = -w.out;
				auto const toe{f.c.half_width + std::max(0.0, f.c.formation - f.c.at.y) * f.c.slope + 1.0};
				double u{0.0};
				for (int step = 0; step < 80; ++step)
				{
					auto const next{u + 0.5};
					auto const p{w.start + w.way * next};
					if (std::abs(glm::dot(p - f.origin, f.m)) > toe)
						break;
					double ground{0.0};
					if (false == f.ground(p + w.out * (dim.wall + 0.2), ground) || ground <= f.road(f.s_of(p)) + 0.3)
						break;
					u = next;
				}
				w.length = u + 0.5;
			}
		std::array<double, 4> ends{};
		for (int i = 0; i < 4; ++i)
		{
			auto const &w{wing_of[i / 2][i % 2]};
			ends[i] = f.s_of(w.start + w.way * w.length);
		}
		out.length = *std::max_element(ends.begin(), ends.end()) - *std::min_element(ends.begin(), ends.end());
	}

	double wing_top(wing const &W, double const U) const
	{
		auto const p{W.start + W.way * U};
		double ground{f.road(f.s_of(p)) + 0.3};
		f.ground(p + W.out * (dim.wall + 0.15), ground);
		auto const highest{open_kind(k) ? f.c.formation - 0.02 : top + kCoping};
		// out of the abutment of a bridge the wing comes down from the formation to the slope beside it over a few metres,
		// twice as steep as the slope: no step where the abutment, wider than the crown, ends
		auto const from_abutment{open_kind(k) ? f.c.formation - 0.02 - 2.0 * U * std::abs(glm::dot(W.way, f.m)) / f.c.slope : ground};
		return std::clamp(std::max(ground + kWingTop, from_abutment), f.road(f.s_of(p)) + 0.3, highest);
	}

	// the floor of the opening: under the structure from wall to wall, and under every hole and a little round it, so where the
	// mask cuts more than asked the floor is seen, never a gap
	void floor()
	{
		auto const floor_part{mesh.build("dno", true)};
		auto const lift = [&](glm::dvec2 const &P) { return f.lift(P, f.floor(f.s_of(P))); };
		if (false == open_kind(k))
		{
			// under the structure, a strip a metre long at a time, from the face to the face whichever the skew
			double first{0.0}, last{0.0};
			bool any{false};
			for (auto const x : {-half, half})
				for (auto const d : {-reach_of(-1), reach_of(1)})
				{
					auto const at{f.s_at(x, d)};
					first = any ? std::min(first, at) : at;
					last = any ? std::max(last, at) : at;
					any = true;
				}
			int const strips{std::max(1, static_cast<int>(std::ceil(last - first)))};
			for (int i = 0; i < strips; ++i)
			{
				auto const a{first + (last - first) * i / strips}, b{first + (last - first) * (i + 1) / strips};
				floor_part.quad(lift(f.place(-half, a)), lift(f.place(half, a)), lift(f.place(half, b)), lift(f.place(-half, b)), kUp);
			}
		}
		// over the holes the ground the road's cutting gives the land: flat beside the road, its slopes rising to the wings, the
		// land's own where it's lower; a little under it, so where the hole ends the ground beyond goes on from it with no step
		for (auto const &hole : out.holes)
		{
			auto const outline{grown(hole, 0.3)};
			glm::dvec2 low{outline.front()}, highest{outline.front()};
			for (auto const &p : outline)
			{
				low = glm::min(low, p);
				highest = glm::max(highest, p);
			}
			double constexpr step{1.0};
			int const nx{std::max(1, static_cast<int>(std::ceil((highest.x - low.x) / step)))};
			int const nz{std::max(1, static_cast<int>(std::ceil((highest.y - low.y) / step)))};
			auto const height = [&](glm::dvec2 const &P) { return cutting_at(P) - 0.05; };
			for (int iz = 0; iz < nz; ++iz)
				for (int ix = 0; ix < nx; ++ix)
				{
					glm::dvec2 const a{low.x + (highest.x - low.x) * ix / nx, low.y + (highest.y - low.y) * iz / nz};
					glm::dvec2 const b{low.x + (highest.x - low.x) * (ix + 1) / nx, low.y + (highest.y - low.y) * (iz + 1) / nz};
					// the squares of the grid within the grown hole, cut along its edges
					for (auto const &piece : inside_of({a, {b.x, a.y}, b, {a.x, b.y}}, outline))
						for (std::size_t i = 1; i + 1 < piece.size(); ++i)
							floor_part.triangle(f.lift(piece[0], height(piece[0])), f.lift(piece[i], height(piece[i])), f.lift(piece[i + 1], height(piece[i + 1])), kUp);
				}
		}
	}

	// the ground the road's cutting gives the land at a point near the road: level with the road over its bench, rising with
	// its slopes beyond, never higher than the land was nor lower than the road
	double cutting_at(glm::dvec2 const &P) const
	{
		auto const s{f.s_of(P)};
		auto const road{f.road(s)};
		auto const slope{road + std::max(0.0, std::abs(f.x_of(P)) - f.c.bench) / std::max(0.25, f.c.bank)};
		double land{road};
		if (f.c.natural && false == f.c.natural(P, land))
			land = road;
		return std::max(road, std::min(land, slope));
	}

	// the height of the opening over the floor across the road: the soffit, the arch, or the haunched corners
	double opening_at(double const X) const
	{
		if (k == kind::arch)
		{
			for (int i = 0; i < spans; ++i)
			{
				auto const a{span_start(i)};
				if (X < a - 1e-6 || X > a + clear + 1e-6)
					continue;
				auto const rise{arch_rise()};
				auto const halfspan{0.5 * clear};
				auto const radius{(rise * rise + halfspan * halfspan) / (2.0 * rise)};
				auto const x{X - (a + halfspan)};
				return soffit - radius + std::sqrt(std::max(0.0, radius * radius - x * x));
			}
			return soffit - arch_rise();
		}
		auto const haunch{k == kind::frame ? 0.6 : k == kind::box ? 0.25 : 0.0};
		for (int i = 0; i < spans; ++i)
		{
			auto const a{span_start(i)};
			if (X < a - 1e-6 || X > a + clear + 1e-6)
				continue;
			auto const edge{std::min(X - a, a + clear - X)};
			return soffit - std::max(0.0, haunch - edge);
		}
		return soffit - haunch;
	}

	double arch_rise() const
	{
		auto const halfspan{0.5 * clear};
		// a semicircle where it's high enough for one, else a segment springing 2 m over the road at least
		return std::min(halfspan, std::max(0.6, soffit - high - 2.0));
	}

	// the walls under the structure, from one face to the other, each side of the road, and the piers between the spans
	void walls()
	{
		auto const body{mesh.build("przyczolki", true)};
		std::vector<double> xs{-half};
		for (int i = 1; i < spans; ++i)
			xs.insert(xs.end(), {span_start(i) - dim.pier, span_start(i)});
		xs.push_back(half);
		auto const piers{mesh.build("filary", true)};
		for (std::size_t i = 0; i < xs.size(); ++i)
		{
			auto const x{xs[i]};
			auto const outer{i == 0 || i + 1 == xs.size()};
			// the left wall and the right faces of the piers look the way x grows, the right wall and the left faces of the piers back
			auto const facing{f.across * (i % 2 == 0 ? 1.0 : -1.0)};
			auto const &into{outer ? body : piers};
			auto const head{support_top(x, outer)};
			auto const d0{-reach_of(-1)}, d1{reach_of(1)};
			int const steps{std::max(1, static_cast<int>(std::ceil((d1 - d0) / 2.0)))};
			for (int j = 0; j < steps; ++j)
			{
				auto const da{d0 + (d1 - d0) * j / steps}, db{d0 + (d1 - d0) * (j + 1) / steps};
				auto const sa{f.s_at(x, da)}, sb{f.s_at(x, db)};
				into.quad(f.point(x, sa, f.floor(sa) - kFooting), f.point(x, sb, f.floor(sb) - kFooting), f.point(x, sb, head), f.point(x, sa, head), facing);
			}
		}
		// the piers: their tops (under a deck, a seat with a cap) and their ends in the faces
		for (int i = 1; i < spans; ++i)
		{
			auto const x0{span_start(i) - dim.pier}, x1{span_start(i)};
			auto const head{support_top(x0, false)};
			for (int sign : {-1, 1})
			{
				auto const d{sign * reach_of(sign)};
				piers.quad(f.node(x0, d, f.floor(f.s_at(x0, d)) - kFooting), f.node(x1, d, f.floor(f.s_at(x1, d)) - kFooting), f.node(x1, d, head), f.node(x0, d, head), f.trackside * static_cast<double>(sign));
			}
			piers.quad(f.node(x0, -reach_of(-1), head), f.node(x1, -reach_of(-1), head), f.node(x1, reach_of(1), head), f.node(x0, reach_of(1), head), kUp);
		}
	}

	// how high a wall or a pier at X reaches under the structure
	double support_top(double const X, bool const Outer) const
	{
		switch (k)
		{
		case kind::arch:
			return soffit - arch_rise();
		case kind::slab:
			return soffit - 0.05; // the joint of the bearing pads over it
		case kind::deck_girders:
		case kind::through_girders:
		case kind::truss:
			return seat();
		default:
			return Outer ? opening_at(X < 0.0 ? -half + 1e-3 : half - 1e-3) : soffit - (k == kind::frame ? 0.6 : 0.0);
		}
	}

	double seat() const { return soffit - kBearing - kBedBlock; }

	// the wings: the inner face, the top along the ground behind, the outer face, the end
	void wings()
	{
		auto const body{mesh.build("skrzydla", true)};
		for (int sign : {-1, 1})
			for (int side : {-1, 1})
			{
				auto const &w{wing_of[side_index(sign)][side_index(side)]};
				// from inside the abutment: the wall's own end under the face is closed by the wing going on from it
				// the face, or the end of an abutment, runs along the track, the wing starts square to its own way: begun as far back
				// as the skew and the splay take the outer face, it closes the corner
				auto const from{-dim.wall * (std::tan(dim.splay) + std::abs(f.nm / f.rm)) - 0.05};
				int const pieces{std::max(1, static_cast<int>(std::ceil((w.length - from) / 0.5)))};
				std::vector<double> us;
				for (int i = 0; i <= pieces; ++i)
					us.push_back(from + (w.length - from) * i / pieces);
				auto const low = [&](glm::dvec2 const &P) { return f.floor(f.s_of(P)) - kFooting; };
				for (std::size_t i = 0; i + 1 < us.size(); ++i)
				{
					auto const a{w.start + w.way * us[i]}, b{w.start + w.way * us[i + 1]};
					auto const ao{a + w.out * dim.wall}, bo{b + w.out * dim.wall};
					auto const ha{wing_top(w, std::max(0.0, us[i]))}, hb{wing_top(w, std::max(0.0, us[i + 1]))};
					body.quad(f.lift(a, low(a)), f.lift(b, low(b)), f.lift(b, hb), f.lift(a, ha), glm::dvec3{-w.out.x, 0.0, -w.out.y});
					body.quad(f.lift(a, ha), f.lift(b, hb), f.lift(bo, hb), f.lift(ao, ha), kUp);
					body.quad(f.lift(ao, low(ao)), f.lift(bo, low(bo)), f.lift(bo, hb), f.lift(ao, ha), glm::dvec3{w.out.x, 0.0, w.out.y});
				}
				auto const e{w.start + w.way * w.length};
				auto const eo{e + w.out * dim.wall};
				auto const h{wing_top(w, w.length)};
				body.quad(f.lift(e, low(e)), f.lift(eo, low(eo)), f.lift(eo, h), f.lift(e, h), glm::dvec3{w.way.x, 0.0, w.way.y});
			}
	}

	// the parts of the structures under the embankment: the soffit, the faces with their cornices and what stands on them
	void buried()
	{
		soffit_surface();
		for (int sign : {-1, 1})
		{
			headwall(sign);
			parapet(sign);
		}
		if (k == kind::slab)
		{
			slab_joints();
			slab_flanges();
		}
	}

	void soffit_surface()
	{
		auto const vault{mesh.build(k == kind::arch ? "sklepienie" : "strop", true, k == kind::arch)};
		int const parts{k == kind::arch ? 16 : 1};
		for (int i = 0; i < spans; ++i)
		{
			auto const a{span_start(i)};
			std::vector<double> xs{a};
			auto const haunch{k == kind::frame ? 0.6 : k == kind::box ? 0.25 : 0.0};
			if (haunch > 0.0)
				xs.insert(xs.end(), {a + haunch, a + clear - haunch});
			else
				for (int j = 1; j < parts; ++j)
					xs.push_back(a + clear * j / parts);
			xs.push_back(a + clear);
			for (std::size_t j = 0; j + 1 < xs.size(); ++j)
			{
				auto const x0{xs[j]}, x1{xs[j + 1]};
				auto const y0{opening_at(x0)}, y1{opening_at(x1)};
				auto const mid{0.5 * (x0 + x1)};
				auto const middle{f.point(a + 0.5 * clear, 0.0, soffit - (k == kind::arch ? arch_rise() : 2.0))};
				auto const away{middle - f.point(mid, 0.0, 0.5 * (y0 + y1))};
				vault.quad(f.node(x0, -reach_of(-1), y0), f.node(x1, -reach_of(-1), y1), f.node(x1, reach_of(1), y1), f.node(x0, reach_of(1), y0), away);
			}
		}
	}

	// the face over the opening and over the walls beside it, the cornice on it reaching back over the edge of the hole
	void headwall(int const Sign)
	{
		auto const fronts{mesh.build("czola", true)};
		auto const d{Sign * reach_of(Sign)};
		auto const outward{f.trackside * static_cast<double>(Sign)};
		auto const ends{half + dim.wall + 0.2};
		std::vector<double> xs{-ends, -half};
		for (int i = 0; i < spans; ++i)
		{
			int const parts{k == kind::arch ? 16 : 1};
			auto const a{span_start(i)};
			auto const haunch{k == kind::frame ? 0.6 : k == kind::box ? 0.25 : 0.0};
			if (i > 0)
				xs.push_back(a);
			if (haunch > 0.0)
				xs.insert(xs.end(), {a + haunch, a + clear - haunch});
			else
				for (int j = 1; j < parts; ++j)
					xs.push_back(a + clear * j / parts);
			if (i + 1 < spans)
				xs.push_back(a + clear);
		}
		xs.insert(xs.end(), {half, ends});
		auto const opening = [&](double const X) {
			for (int i = 0; i < spans; ++i)
				if (X >= span_start(i) - 1e-6 && X <= span_start(i) + clear + 1e-6)
					return opening_at(X);
			return f.floor(f.s_at(X, d)) - kFooting;
		};
		for (std::size_t j = 0; j + 1 < xs.size(); ++j)
		{
			auto const x0{xs[j]}, x1{xs[j + 1]};
			auto const mid{0.5 * (x0 + x1)};
			auto const inside{std::abs(mid) < half};
			if (k == kind::slab && inside)
				continue; // the edge of the slab and the headwall over it stand there
			auto const b0{inside ? opening(x0 + 1e-4) : f.floor(f.s_at(x0, d)) - kFooting};
			auto const b1{inside ? opening(x1 - 1e-4) : f.floor(f.s_at(x1, d)) - kFooting};
			// a slab: under it the abutments end at their caps, the slab's own edge stands out in front
			auto const head{k == kind::slab && false == inside ? soffit - 0.05 - 0.3 : top};
			fronts.quad(f.node(x0, d, b0), f.node(x1, d, b1), f.node(x1, d, head), f.node(x0, d, head), outward);
		}
		if (k == kind::slab)
			slab_edge(Sign);
		// the cornice: out of the face a little, back over the edge of the hole, the full width over the wings' starts
		auto const front{d + Sign * 0.12};
		auto const back{d - Sign * kCap};
		auto const cornice_top{top + kCoping};
		std::array<glm::dvec2, 4> const plan{f.place(-ends, f.s_at(-ends, back)), f.place(ends, f.s_at(ends, back)), f.place(ends, f.s_at(ends, front)), f.place(-ends, f.s_at(-ends, front))};
		block(mesh.build("gzymsy", true), plan, top, cornice_top);
		// the ring of an arch standing out of the face, the stones of it seen
		if (k == kind::arch)
			archivolt(Sign);
	}

	// the edge of a slab on its abutments: a band standing out of the face from the soffit to the top over the opening and the
	// seats, under its ends the caps of the abutments standing out too, the joint between them dark; beside it the backwalls
	// in the line of the face up to the top
	void slab_edge(int const Sign)
	{
		auto const d{Sign * reach_of(Sign)};
		auto const ends{half + dim.wall + 0.2};
		auto const edge{half + 0.5};
		auto const fronts{mesh.build("czola", true)};
		auto const caps{mesh.build("oczepy", true)};
		auto const plan = [&](double const Xa, double const Xb, double const Back, double const Front) {
			return std::array<glm::dvec2, 4>{f.place(Xa, f.s_at(Xa, d - Sign * Back)), f.place(Xb, f.s_at(Xb, d - Sign * Back)), f.place(Xb, f.s_at(Xb, d + Sign * Front)),
			                                 f.place(Xa, f.s_at(Xa, d + Sign * Front))};
		};
		block(mesh.build("plyta", true), plan(-edge, edge, 0.5, 0.15), soffit, deck_top);
		for (auto const side : {-1.0, 1.0})
		{
			auto const xa{side < 0.0 ? -ends : half - 0.15}, xb{side < 0.0 ? -half + 0.15 : ends};
			// the cap: along the face, and along the inner face of the wall under the slab
			block(caps, plan(xa, xb, 0.4, 0.1), soffit - 0.05 - 0.3, soffit - 0.05);
			// the backwall beside the slab, in the line of the face
			auto const xo{side * edge}, xe{side * ends};
			fronts.quad(f.node(std::min(xo, xe), d, soffit - 0.05), f.node(std::max(xo, xe), d, soffit - 0.05), f.node(std::max(xo, xe), d, top), f.node(std::min(xo, xe), d, top),
			            f.trackside * static_cast<double>(Sign));
		}
		// over the slab where the embankment is higher: the headwall holding the foot of the slope, back in the line of the face
		if (top > deck_top + 0.01)
			fronts.quad(f.node(-edge, d, deck_top), f.node(edge, d, deck_top), f.node(edge, d, top), f.node(-edge, d, top), f.trackside * static_cast<double>(Sign));
		for (auto const side : {-1.0, 1.0})
		{
			auto const x{side * half};
			auto const inner{x - side * 0.15};
			std::array<glm::dvec2, 4> const along{f.place(x, f.s_at(x, -reach_of(-1))), f.place(inner, f.s_at(inner, -reach_of(-1))), f.place(inner, f.s_at(inner, reach_of(1))),
			                                      f.place(x, f.s_at(x, reach_of(1)))};
			block(caps, along, soffit - 0.05 - 0.3, soffit - 0.05);
		}
	}

	void archivolt(int const Sign)
	{
		auto const ring{mesh.build("archiwolta", true)};
		auto const d{Sign * reach_of(Sign)};
		auto const proud{Sign * 0.1};
		auto const thick{dim.depth - 0.5};
		for (int i = 0; i < spans; ++i)
		{
			auto const a{span_start(i)};
			auto const halfspan{0.5 * clear};
			auto const rise{arch_rise()};
			auto const radius{(rise * rise + halfspan * halfspan) / (2.0 * rise)};
			auto const centre_y{soffit - radius};
			auto const middle{a + halfspan};
			auto const start{std::asin(std::clamp((soffit - rise - centre_y) / radius, -1.0, 1.0))};
			int const parts{16};
			for (int j = 0; j < parts; ++j)
			{
				auto const p0{start + (kPi - 2.0 * start) * j / parts}, p1{start + (kPi - 2.0 * start) * (j + 1) / parts};
				auto const in = [&](double const P, double const R) { return glm::dvec2{middle + R * std::cos(P), centre_y + R * std::sin(P)}; };
				auto const i0{in(p0, radius)}, i1{in(p1, radius)}, o0{in(p0, radius + thick)}, o1{in(p1, radius + thick)};
				auto const at = [&](glm::dvec2 const &Q, double const Out) { return f.node(Q.x, d + Out, Q.y); };
				ring.quad(at(i0, proud), at(i1, proud), at(o1, proud), at(o0, proud), f.trackside * static_cast<double>(Sign));
				ring.quad(at(o0, 0.0), at(o1, 0.0), at(o1, proud), at(o0, proud), glm::dvec3{std::cos(0.5 * (p0 + p1)) * f.across.x, std::sin(0.5 * (p0 + p1)), std::cos(0.5 * (p0 + p1)) * f.across.z});
			}
		}
	}

	// what stands on the cornice: a wall of the masonry over an arch, an edge beam and a railing over a frame or a slab
	void parapet(int const Sign)
	{
		// by the kind: a wall on an arch; a railing on a frame or a slab the track runs right over, none on one deep under the embankment
		auto const near_track{top >= f.c.formation - 0.1};
		auto const chosen{o.parapet > 0 ? o.parapet - 1 : default_parapet(near_track)};
		if (chosen == 0)
			return;
		auto const d{Sign * (reach_of(Sign) - 0.3)};
		auto const ends{half + dim.wall + 0.2};
		auto const base{top + kCoping};
		if (chosen == 2)
		{
			auto const t{Sign * 0.25};
			std::array<glm::dvec2, 4> const plan{f.place(-ends, f.s_at(-ends, d - t)), f.place(ends, f.s_at(ends, d - t)), f.place(ends, f.s_at(ends, d + t)), f.place(-ends, f.s_at(-ends, d + t))};
			block(mesh.build("attyka", true), plan, base, base + 0.9);
			return;
		}
		mesh.build("balustrady", false).railing(f.node(-ends, d, base), f.node(ends, d, base), 1.1, 2.0);
	}

	// by the kind: a wall of the masonry on an arch, a wall of concrete on a frame (cast with it), a railing on a slab; on a frame or
	// a slab only where the track runs right over it, none on one deep under the embankment
	int default_parapet(bool const Near_track) const
	{
		if (k == kind::arch)
			return 2;
		if (false == Near_track)
			return 0;
		if (k == kind::frame)
			return 2;
		return k == kind::slab ? 1 : 0;
	}

	// the soffit of a slab of encased beams: the bottom flanges of the rolled beams left bare, side by side across the opening
	// every 0.45 m or so, rust brown, a little under the concrete between them
	void slab_flanges()
	{
		auto const flanges{mesh.plain("stopki", kRust)};
		auto const d0{-reach_of(-1) + 0.35}, d1{reach_of(1) - 0.35};
		int const count{std::max(2, static_cast<int>(std::floor((d1 - d0) / 0.45)))};
		for (int i = 0; i <= count; ++i)
		{
			auto const d{d0 + (d1 - d0) * i / count};
			for (int j = 0; j < spans; ++j)
			{
				auto const a{span_start(j) - 0.25}, b{span_start(j) + clear + 0.25};
				std::array<glm::dvec2, 4> const plan{f.place(a, f.s_at(a, d - 0.11)), f.place(b, f.s_at(b, d - 0.11)), f.place(b, f.s_at(b, d + 0.11)), f.place(a, f.s_at(a, d + 0.11))};
				block(flanges, plan, soffit - 0.025, soffit + 0.002);
			}
		}
	}

	// a slab on its abutments and piers: the dark pads of the bearings under its ends, seen as a line at the top of each wall
	void slab_joints()
	{
		auto const pads{mesh.plain("lozyska", kRubber)};
		std::vector<std::pair<double, double>> under{{-half - 0.5, -half}, {half, half + 0.5}};
		for (int i = 1; i < spans; ++i)
			under.emplace_back(span_start(i) - dim.pier, span_start(i));
		for (auto const &[xa, xb] : under)
		{
			std::array<glm::dvec2, 4> const plan{f.place(xa, f.s_at(xa, -reach_of(-1))), f.place(xb, f.s_at(xb, -reach_of(-1))), f.place(xb, f.s_at(xb, reach_of(1))),
			                                     f.place(xa, f.s_at(xa, reach_of(1)))};
			block(pads, plan, soffit - 0.05, soffit);
		}
	}

	// the bridges: the abutments under the deck with their seats and backwalls, and the deck, the girders or the trusses
	void open_bridge()
	{
		abutment_seats();
		auto const steel{mesh.build(k == kind::truss ? "kratownice" : "dzwigary", false)};
		auto const deck_part{mesh.build("koryto", false)};
		auto const bearings{mesh.plain("lozyska", kRubber)};
		auto const blocks{mesh.build("ciosy", true)};
		for (int i = 0; i < spans; ++i)
		{
			// a span from the bearings on one support to those on the next
			// on a pier the bearings of the two spans stand side by side, the ends of the spans a little apart over its middle
			auto const a{i == 0 ? -half - 0.45 : span_start(i) - 0.5 * dim.pier + 0.22};
			auto const b{i + 1 == spans ? half + 0.45 : span_start(i + 1) - 0.5 * dim.pier - 0.22};
			auto const ea{i == 0 ? a - 0.35 : a - 0.2};
			auto const eb{i + 1 == spans ? b + 0.35 : b + 0.2};
			for (auto const &one : decks)
			{
				auto const lines{girder_lines(one)};
				for (auto const d : lines)
					for (auto const x : {a, b})
					{
						plate(blocks, f.place(x, f.s_at(x, d)), f.r, 0.4, 0.7, seat(), seat() + kBedBlock);
						plate(bearings, f.place(x, f.s_at(x, d)), f.r, 0.3, 0.45, seat() + kBedBlock, soffit);
					}
				trough(deck_part, ea, eb, one);
				if (k == kind::deck_girders)
					deck_girders(steel, ea, eb, lines);
				else if (k == kind::through_girders)
					through_girders(steel, ea, eb, one);
				else
					truss(steel, ea, eb, one);
			}
		}
	}

	std::vector<double> girder_lines(deck const &One) const
	{
		if (k == kind::through_girders)
			return {One.centre - (One.half + 0.3), One.centre + One.half + 0.3};
		if (k == kind::truss)
			return {One.centre - (One.half + 0.45), One.centre + One.half + 0.45};
		int const count{std::max(2, static_cast<int>(std::lround(2.0 * One.half / 2.0)))};
		std::vector<double> lines;
		for (int i = 0; i < count; ++i)
			lines.push_back(One.centre - (One.half - 0.6) + 2.0 * (One.half - 0.6) * i / (count - 1));
		return lines;
	}

	// under the deck the abutment's top is a seat for the bearings, behind them the backwall holds the embankment up to the
	// formation; the piers have a seat as wide as they are
	void abutment_seats()
	{
		auto const body{mesh.build("przyczolki", true)};
		auto const reach0{-reach_of(-1)}, reach1{reach_of(1)};
		for (int side : {-1, 1})
		{
			auto const front{side * (half + 0.9)};
			auto const back{side * (half + dim.wall)};
			auto const inner{side * half};
			auto const level{f.c.formation - 0.02};
			// the seat, the backwall's face to the deck, its top
			body.quad(f.node(inner, reach0, seat()), f.node(front, reach0, seat()), f.node(front, reach1, seat()), f.node(inner, reach1, seat()), kUp);
			body.quad(f.node(front, reach0, seat()), f.node(front, reach1, seat()), f.node(front, reach1, level), f.node(front, reach0, level), f.across * static_cast<double>(-side));
			body.quad(f.node(front, reach0, level), f.node(back, reach0, level), f.node(back, reach1, level), f.node(front, reach1, level), kUp);
			// the ends of the abutment where the wings go on: the step of the seat seen from beside
			for (int sign : {-1, 1})
			{
				auto const d{sign * reach_of(sign)};
				body.quad(f.node(inner, d, seat()), f.node(front, d, seat()), f.node(front, d, level), f.node(inner, d, level), f.trackside * static_cast<double>(sign));
			}
		}
	}

	double trough_bottom() const { return f.c.formation - 0.02 - kTrough + 0.02; }

	// the ballast trough: the plate under the formation and its sides, from one end of the span to the other
	void trough(structure::builder const &Deck, double const A, double const B, deck const &One)
	{
		auto const level{f.c.formation - 0.02};
		auto const d0{One.centre - One.half}, d1{One.centre + One.half};
		block(Deck, {f.place(A, f.s_at(A, d0)), f.place(B, f.s_at(B, d0)), f.place(B, f.s_at(B, d1)), f.place(A, f.s_at(A, d1))}, trough_bottom(), level);
		if (k != kind::deck_girders)
			return;
		// the sides of the trough holding the ballast, a walkway and a railing outside them
		for (int sign : {-1, 1})
		{
			auto const d{One.centre + sign * One.half};
			auto const t{sign * 0.15};
			block(Deck, {f.place(A, f.s_at(A, d - t)), f.place(B, f.s_at(B, d - t)), f.place(B, f.s_at(B, d)), f.place(A, f.s_at(A, d))}, level, level + 0.45);
			if (o.parapet != 1)
				mesh.build("balustrady", false).railing(f.node(A, d - t * 0.5, level + 0.45), f.node(B, d - t * 0.5, level + 0.45), 1.0, 2.0);
		}
	}

	// the stiffeners standing on the webs of a girder every so often, on one side or both
	void stiffeners(structure::builder const &Steel, double const A, double const B, double const D, double const Bottom, double const Top, int const Sides) const
	{
		auto const length{std::abs(B - A) / std::abs(f.rm)};
		int const count{std::max(2, static_cast<int>(std::lround(length / 1.6)))};
		for (int i = 0; i <= count; ++i)
		{
			auto const x{A + (B - A) * i / count};
			for (int side : {-1, 1})
				if (Sides == 0 || Sides == side)
					plate(Steel, f.place(x, f.s_at(x, D + side * 0.11)), glm::normalize(f.m), 0.16, 0.025, Bottom + 0.03, Top - 0.03);
		}
	}

	void deck_girders(structure::builder const &Steel, double const A, double const B, std::vector<double> const &Lines) const
	{
		auto const height{dim.depth - kTrough};
		for (auto const d : Lines)
		{
			Steel.girder(f.node(A, d, soffit), f.node(B, d, soffit), 0.42, height);
			stiffeners(Steel, A, B, d, soffit, soffit + height, 0);
		}
		// the bracing between the girders: a cross every third of the span and at its ends
		for (std::size_t i = 0; i + 1 < Lines.size(); ++i)
			for (int j = 0; j <= 3; ++j)
			{
				auto const x{A + (B - A) * (0.04 + 0.92 * j / 3.0)};
				auto const da{Lines[i] + 0.15}, db{Lines[i + 1] - 0.15};
				Steel.member(f.node(x, da, soffit + 0.15), f.node(x, db, soffit + height - 0.15), 0.08);
				Steel.member(f.node(x, da, soffit + height - 0.15), f.node(x, db, soffit + 0.15), 0.08);
				Steel.member(f.node(x, da, soffit + 0.1), f.node(x, db, soffit + 0.1), 0.08);
			}
	}

	void through_girders(structure::builder const &Steel, double const A, double const B, deck const &One) const
	{
		auto const height{std::clamp(span / 9.0, 1.4, 3.0)};
		auto const sides{girder_lines(One)};
		for (auto const d : sides)
		{
			Steel.girder(f.node(A, d, soffit), f.node(B, d, soffit), 0.5, height);
			stiffeners(Steel, A, B, d, soffit, soffit + height, d < One.centre ? -1 : 1);
		}
		// the cross beams under the trough, from one girder to the other
		auto const length{std::abs(B - A) / std::abs(f.rm)};
		int const count{std::max(2, static_cast<int>(std::lround(length / 1.6)))};
		for (int i = 0; i <= count; ++i)
		{
			auto const x{A + (B - A) * i / count};
			Steel.girder(f.node(x, sides[0], soffit + 0.04), f.node(x, sides[1], soffit + 0.04), 0.22, trough_bottom() - soffit - 0.04);
		}
	}

	void truss(structure::builder const &Steel, double const A, double const B, deck const &One) const
	{
		auto const length{std::abs(B - A) / std::abs(f.rm)};
		auto const through{length >= 32.0};
		auto const height{through ? std::clamp(length / 7.5, 5.5, 10.0) : std::clamp(length / 7.0, 2.4, 3.6)};
		auto const size{through ? 0.45 : 0.35};
		auto const panel{through ? 5.0 : height * 0.95};
		int const panels{std::max(4, 2 * static_cast<int>(std::lround(length / (2.0 * panel))))};
		auto const low{soffit + 0.5 * size}; // the middle of the bottom chord, on the bearings
		auto const high{low + height};
		auto const at = [&](int const I) { return A + (B - A) * I / panels; };
		auto const sides{girder_lines(One)};
		for (auto const d : sides)
		{
			Steel.member(f.node(A, d, low), f.node(B, d, low), size);
			Steel.member(f.node(at(1), d, high), f.node(at(panels - 1), d, high), size);
			// the end posts leaning in to the top chord
			Steel.member(f.node(A, d, low), f.node(at(1), d, high), size);
			Steel.member(f.node(B, d, low), f.node(at(panels - 1), d, high), size);
			for (int i = 1; i < panels; ++i)
				Steel.member(f.node(at(i), d, low), f.node(at(i), d, high), 0.6 * size);
			// the diagonals falling to the middle of the span
			for (int i = 1; i + 1 < panels; ++i)
			{
				auto const falls{2 * i < panels};
				Steel.member(f.node(at(i), d, falls ? high : low), f.node(at(i + 1), d, falls ? low : high), 0.6 * size);
			}
		}
		// the cross beams at the nodes under the trough
		for (int i = 0; i <= panels; ++i)
			Steel.girder(f.node(at(i), sides[0], soffit), f.node(at(i), sides[1], soffit), 0.25, trough_bottom() - soffit);
		if (through)
		{
			// over the track: a strut at each node of the top chord, a cross between them, a portal at each end
			for (int i = 1; i < panels; ++i)
			{
				Steel.member(f.node(at(i), sides[0], high), f.node(at(i), sides[1], high), 0.5 * size);
				if (i + 1 < panels)
				{
					Steel.member(f.node(at(i), sides[0], high), f.node(at(i + 1), sides[1], high), 0.3 * size);
					Steel.member(f.node(at(i), sides[1], high), f.node(at(i + 1), sides[0], high), 0.3 * size);
				}
			}
			for (int i : {1, panels - 1})
				for (auto const sign : {-1.0, 1.0})
					Steel.member(f.node(at(i), sides[sign < 0.0 ? 0 : 1], high - 1.2), f.node(at(i), One.centre + sign * 0.4 * (sides[1] - One.centre), high), 0.3 * size);
			return;
		}
		// a low truss: a U-frame at each vertical, the knee from the vertical down to the cross beam inside
		for (int i = 1; i < panels; ++i)
			for (auto const d : sides)
			{
				auto const inward{d < One.centre ? 0.9 : -0.9};
				Steel.member(f.node(at(i), d, low + 0.6 * height), f.node(at(i), d + inward, f.c.formation - 0.1), 0.18);
			}
	}

	// the terrain: holes before the faces between the walls out to the ends of the wings; over the structure the ground
	// raised to cover it. A bridge: the whole opening from the end of one wing to the other cut out, none raised
	void terrain()
	{
		auto const edge = [&](wing const &W, double const U) { return W.start + W.way * U + W.out * (dim.wall - kInset); };
		if (open_kind(k))
		{
			auto const &l0{wing_of[0][0]}, &r0{wing_of[0][1]}, &l1{wing_of[1][0]}, &r1{wing_of[1][1]};
			out.holes.push_back({edge(l0, l0.length), edge(r0, r0.length), edge(r1, r1.length), edge(l1, l1.length)});
			return;
		}
		for (int sign : {-1, 1})
		{
			auto const &l{wing_of[side_index(sign)][0]}, &r{wing_of[side_index(sign)][1]};
			// from inside the headwall, where the line of the edge along each wing meets the line the hole starts at, out to the
			// ends of the wings: four corners, never a notch
			auto const d{sign * (reach_of(sign) - kInset)};
			auto const back = [&](wing const &W) {
				auto const towards{glm::dot(W.way, f.m)};
				auto const from{glm::dot(edge(W, 0.0) - f.origin, f.m)};
				return std::abs(towards) < 1e-6 ? 0.0 : (d - from) / towards;
			};
			out.holes.push_back({edge(l, back(l)), edge(r, back(r)), edge(r, r.length), edge(l, l.length)});
		}
		auto const x{half + dim.wall + 0.3};
		auto const d0{-(reach_of(-1) - kInset)}, d1{reach_of(1) - kInset};
		out.raise.push_back({{f.place(-x, f.s_at(-x, d0)), f.place(x, f.s_at(x, d0)), f.place(x, f.s_at(x, d1)), f.place(-x, f.s_at(-x, d1))}, top + 0.1});
	}

	// a pipe of corrugated steel through the embankment, its ends cut along the slopes, a ring of concrete round them
	void pipe()
	{
		auto const height{soffit - (high - kFloorDrop)}; // of the crown over the floor
		auto const radius{(height * height + half * half) / (2.0 * height)};
		auto const centre{height - radius}; // over the floor
		// the slope at each end: where the ground comes down to the crown and to the road, along the axis of the road
		std::array<double, 2> crown_at{}, foot_at{};
		auto const crown_top{soffit + 0.08};
		for (int sign : {-1, 1})
		{
			auto const toe{f.c.half_width + std::max(0.0, f.c.formation - f.c.at.y) * f.c.slope};
			double crown_d{f.c.half_width + std::max(0.0, f.c.formation - crown_top) * f.c.slope};
			double foot_d{toe};
			bool crown_found{false};
			auto const from{std::max(0.5, f.c.half_width - 1.0)};
			int const steps{static_cast<int>((toe + 6.0 - from) / 0.25)};
			for (int step = 0; step < steps; ++step)
			{
				auto const d{from + 0.25 * step};
				double ground{0.0};
				if (false == f.ground(f.place(0.0, f.s_at(0.0, sign * d)), ground))
					break;
				if (false == crown_found && ground < crown_top)
				{
					crown_d = d;
					crown_found = true;
				}
				if (ground <= f.road(f.s_at(0.0, sign * d)) + 0.3)
				{
					foot_d = d;
					break;
				}
			}
			crown_at[side_index(sign)] = crown_d;
			foot_at[side_index(sign)] = std::max(foot_d, crown_d + 0.5);
		}
		// the distance of the end from the axis of the track at a height over the floor: on the plane of the slope
		auto const end_at = [&](int const Sign, double const Over) {
			auto const i{side_index(Sign)};
			auto const t{std::clamp(Over / height, 0.0, 1.2)};
			return foot_at[i] + (crown_at[i] - foot_at[i]) * t;
		};
		auto const floor_y = [&](double const X, double const D) { return f.floor(f.s_at(X, D)); };
		auto const low_angle{std::asin(std::clamp(-centre / radius, -1.0, 1.0))};
		int const parts{28};
		auto const shell{mesh.build("rura", false, true)};
		auto const angle = [&](int const I) { return low_angle + (kPi - 2.0 * low_angle) * I / parts; };
		for (int i = 0; i < parts; ++i)
		{
			auto const a0{angle(i)}, a1{angle(i + 1)};
			auto const x0{radius * std::cos(a0)}, x1{radius * std::cos(a1)};
			auto const y0{centre + radius * std::sin(a0)}, y1{centre + radius * std::sin(a1)};
			auto const da0{-end_at(-1, y0)}, da1{-end_at(-1, y1)}, db0{end_at(1, y0)}, db1{end_at(1, y1)};
			auto const middle{0.5 * (a0 + a1)};
			glm::dvec3 const inward{-std::cos(middle) * f.across.x, -std::sin(middle), -std::cos(middle) * f.across.z};
			shell.quad(f.node(x0, da0, floor_y(x0, da0) + y0), f.node(x1, da1, floor_y(x1, da1) + y1), f.node(x1, db1, floor_y(x1, db1) + y1), f.node(x0, db0, floor_y(x0, db0) + y0), inward);
		}
		// the floor in the pipe and out to the foot of the slope
		auto const floor_part{mesh.build("dno", true)};
		auto const da{-(foot_at[0] + 0.6)}, db{foot_at[1] + 0.6};
		// as wide as the rings: the edges of the holes at their feet fall on it, the walls of the pipe stand in it
		auto const apron{radius + 0.8};
		floor_part.quad(f.node(-apron, da, floor_y(-apron, da)), f.node(apron, da, floor_y(apron, da)), f.node(apron, db, floor_y(apron, db)), f.node(-apron, db, floor_y(-apron, db)), kUp);
		// the rings at the ends, on the slope, and the holes they hide the edges of
		auto const collar{mesh.build("wience", true)};
		for (int sign : {-1, 1})
		{
			auto const i{side_index(sign)};
			auto const run{(foot_at[i] - crown_at[i]) / height}; // of the slope for a metre of height
			glm::dvec3 const away{glm::normalize(f.trackside * static_cast<double>(sign) + kUp * run)};
			auto const on = [&](double const R, double const A, double const Proud) {
				auto const x{R * std::cos(A)};
				auto const y{std::max(0.0, centre + R * std::sin(A))};
				auto const d{sign * end_at(sign, y)};
				return f.node(x, d, floor_y(x, d) + y) + away * Proud;
			};
			std::vector<glm::dvec2> hole;
			for (int j = 0; j < parts; ++j)
			{
				auto const a0{angle(j)}, a1{angle(j + 1)};
				collar.quad(on(radius, a0, 0.12), on(radius + 0.5, a0, 0.12), on(radius + 0.5, a1, 0.12), on(radius, a1, 0.12), away);
				collar.quad(on(radius, a0, -0.2), on(radius, a0, 0.12), on(radius, a1, 0.12), on(radius, a1, -0.2), -away);
				collar.quad(on(radius + 0.5, a0, 0.12), on(radius + 0.5, a0, -0.2), on(radius + 0.5, a1, -0.2), on(radius + 0.5, a1, 0.12), away);
			}
			for (int j = 0; j <= parts; ++j)
			{
				auto const p{on(radius + 0.25, angle(j), 0.0)};
				hole.emplace_back(p.x, p.z);
			}
			out.holes.push_back(hole);
			reach[i] = crown_at[i];
		}
		auto const x{radius + 0.6};
		auto const d0{-(crown_at[0] - 0.3)}, d1{crown_at[1] - 0.3};
		out.raise.push_back({{f.place(-x, f.s_at(-x, d0)), f.place(x, f.s_at(x, d0)), f.place(x, f.s_at(x, d1)), f.place(-x, f.s_at(-x, d1))}, crown_top + 0.15});
		out.length = std::abs(f.s_at(0.0, -foot_at[0]) - f.s_at(0.0, foot_at[1]));
	}
};

} // namespace

double depth_under(kind const Type, double const Span)
{
	return dimensions_of(Type, Span).depth;
}

result build(site const &Site, options const &Options)
{
	result out;
	maker{Site, Options, out}.run();
	return out;
}

namespace
{

using flat_polygon = std::vector<glm::dvec2>;

// the part of the polygon on the side of the line through A and B where Keep says (Keep > 0: left of the way from A to B)
flat_polygon clipped(flat_polygon const &Polygon, glm::dvec2 const &A, glm::dvec2 const &B, double const Keep)
{
	flat_polygon result;
	auto const side = [&](glm::dvec2 const &P) { return Keep * ((B.x - A.x) * (P.y - A.y) - (B.y - A.y) * (P.x - A.x)); };
	for (std::size_t i = 0; i < Polygon.size(); ++i)
	{
		auto const &p{Polygon[i]};
		auto const &q{Polygon[(i + 1) % Polygon.size()]};
		auto const sp{side(p)}, sq{side(q)};
		if (sp >= 0.0)
			result.push_back(p);
		if ((sp >= 0.0) != (sq >= 0.0))
			result.push_back(p + (q - p) * (sp / (sp - sq)));
	}
	return result;
}

double area_of(flat_polygon const &Polygon)
{
	double area{0.0};
	for (std::size_t i = 0; i < Polygon.size(); ++i)
	{
		auto const &a{Polygon[i]};
		auto const &b{Polygon[(i + 1) % Polygon.size()]};
		area += a.x * b.y - b.x * a.y;
	}
	return 0.5 * area;
}

// the pieces of the polygon outside the convex hole: past each edge of the hole, within those before it
std::vector<flat_polygon> outside_of(flat_polygon const &Polygon, flat_polygon const &Hole)
{
	std::vector<flat_polygon> pieces;
	auto const turn{area_of(Hole) >= 0.0 ? 1.0 : -1.0}; // the inside of the hole is to the left of its edges when it's counter-clockwise
	auto rest{Polygon};
	for (std::size_t i = 0; i < Hole.size() && rest.size() >= 3; ++i)
	{
		auto const &a{Hole[i]};
		auto const &b{Hole[(i + 1) % Hole.size()]};
		if (glm::distance(a, b) < 1e-9)
			continue;
		auto outer{clipped(rest, a, b, -turn)};
		if (outer.size() >= 3 && std::abs(area_of(outer)) > 1e-8)
			pieces.push_back(std::move(outer));
		rest = clipped(rest, a, b, turn);
	}
	return pieces;
}

} // namespace

std::vector<glm::dvec3> ground_outside(std::vector<ground_cell> const &Cells, std::vector<std::vector<glm::dvec2>> const &Holes)
{
	std::vector<glm::dvec3> triangles;
	for (auto const &cell : Cells)
	{
		auto const x0{cell.low.x}, z0{cell.low.y}, x1{x0 + cell.size}, z1{z0 + cell.size};
		std::array<glm::dvec3, 4> const corner{glm::dvec3{x0, cell.heights[0], z0}, glm::dvec3{x1, cell.heights[1], z0}, glm::dvec3{x0, cell.heights[2], z1},
		                                       glm::dvec3{x1, cell.heights[3], z1}};
		// the two triangles of the cell as the terrain has them, cut along the diagonal from (x0, z1) to (x1, z0)
		for (auto const &triangle : {std::array<glm::dvec3, 3>{corner[0], corner[1], corner[2]}, std::array<glm::dvec3, 3>{corner[3], corner[2], corner[1]}})
		{
			auto const normal{glm::cross(triangle[1] - triangle[0], triangle[2] - triangle[0])};
			if (std::abs(normal.y) < 1e-12)
				continue;
			auto const height_at = [&](glm::dvec2 const &P) {
				return triangle[0].y - (normal.x * (P.x - triangle[0].x) + normal.z * (P.y - triangle[0].z)) / normal.y;
			};
			std::vector<flat_polygon> pieces{{{triangle[0].x, triangle[0].z}, {triangle[1].x, triangle[1].z}, {triangle[2].x, triangle[2].z}}};
			for (auto const &hole : Holes)
			{
				if (hole.size() < 3)
					continue;
				std::vector<flat_polygon> next;
				for (auto const &piece : pieces)
					for (auto &outer : outside_of(piece, hole))
						next.push_back(std::move(outer));
				pieces = std::move(next);
			}
			for (auto const &piece : pieces)
			{
				auto const counter{area_of(piece) > 0.0};
				for (std::size_t i = 1; i + 1 < piece.size(); ++i)
				{
					// seen from above (y up, x east, z south): counter-clockwise on the plan of x and z is clockwise from above
					auto const &a{piece[0]};
					auto const &b{piece[counter ? i + 1 : i]};
					auto const &c{piece[counter ? i : i + 1]};
					triangles.insert(triangles.end(), {glm::dvec3{a.x, height_at(a), a.y}, glm::dvec3{b.x, height_at(b), b.y}, glm::dvec3{c.x, height_at(c), c.y}});
				}
			}
		}
	}
	return triangles;
}

} // namespace crossing

namespace crossing
{

namespace
{

glm::dvec3 colour_of(structure::part const &Part)
{
	auto const &name{Part.name};
	if (name == "sklepienie" || name == "archiwolta" || name == "attyka")
		return {0.66, 0.42, 0.32}; // the masonry: brick
	if (name == "dno")
		return {0.50, 0.48, 0.45};
	if (name == "gzymsy" || name == "wience" || name == "ciosy" || name == "oczepy")
		return {0.82, 0.82, 0.80};
	if (Part.material.empty() && Part.colour != glm::ivec3{255, 255, 255})
		return glm::dvec3{Part.colour} / 255.0;
	if (name == "dzwigary" || name == "kratownice" || name == "koryto" || name == "balustrady" || name == "rura")
		return {0.30, 0.38, 0.34}; // the steel, painted green as the plain colour of the model
	return {0.68, 0.68, 0.66}; // concrete
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

} // namespace

std::vector<shaded_triangle> preview(options const &Options)
{
	double constexpr formation{7.0};
	site place;
	auto const angle{glm::radians(80.0)};
	place.track = {std::cos(angle), std::sin(angle)};
	place.formation = formation;
	place.half_width = 3.5;
	place.slope = 1.5;
	place.width = 7.0;
	place.bench = 4.0;
	auto const ground = [&](glm::dvec2 const &P) {
		glm::dvec2 const m{-place.track.y, place.track.x};
		auto const d{std::abs(glm::dot(P, m))};
		return std::max(0.0, formation - std::max(0.0, d - place.half_width) / place.slope);
	};
	place.ground = [&](glm::dvec2 const &P, double &Height) {
		Height = ground(P);
		return true;
	};
	auto const made{build(place, Options)};
	std::vector<shaded_triangle> out;
	if (false == made.built)
		return out;
	// the terrain: a grid of 2 m, the cells the holes reach into left out and laid again outside the holes
	double constexpr reach{30.0}, cell{2.0};
	int const count{static_cast<int>(2.0 * reach / cell)};
	std::vector<ground_cell> cut;
	glm::dvec3 const grass{0.36, 0.52, 0.24}, road{0.22, 0.22, 0.24};
	auto const raised = [&](glm::dvec2 const &P) {
		auto height{ground(P)};
		for (auto const &area : made.raise)
			if (in_outline(area.outline, P))
				height = std::max(height, area.height);
		return height;
	};
	for (int iz = 0; iz < count; ++iz)
		for (int ix = 0; ix < count; ++ix)
		{
			glm::dvec2 const low{-reach + ix * cell, -reach + iz * cell};
			std::array<glm::dvec2, 4> const plan{low, low + glm::dvec2{cell, 0.0}, low + glm::dvec2{0.0, cell}, low + glm::dvec2{cell}};
			std::array<double, 4> heights{};
			for (int i = 0; i < 4; ++i)
				heights[i] = raised(plan[i]);
			bool touched{false};
			for (auto const &hole : made.holes)
				for (double z = 0.0; z <= cell && false == touched; z += 0.25)
					for (double x = 0.0; x <= cell && false == touched; x += 0.25)
						touched = in_outline(hole, low + glm::dvec2{x, z});
			if (touched)
			{
				cut.push_back({low, cell, heights});
				continue;
			}
			auto const colour{std::abs(low.y + 1.0) < 3.5 && heights[0] < 0.05 ? road : grass};
			std::array<glm::dvec3, 4> const p{glm::dvec3{plan[0].x, heights[0], plan[0].y}, glm::dvec3{plan[1].x, heights[1], plan[1].y}, glm::dvec3{plan[2].x, heights[2], plan[2].y},
			                                  glm::dvec3{plan[3].x, heights[3], plan[3].y}};
			out.push_back({{p[0], p[1], p[2]}, colour});
			out.push_back({{p[3], p[2], p[1]}, colour});
		}
	auto const laid{ground_outside(cut, made.holes)};
	for (std::size_t i = 0; i + 2 < laid.size(); i += 3)
		out.push_back({{laid[i], laid[i + 1], laid[i + 2]}, grass});
	// the track's ballast on the formation
	glm::dvec2 const t{place.track}, m{-t.y, t.x};
	for (int i = -12; i < 12; ++i)
	{
		auto const a{t * (i * 2.5)}, b{t * (i * 2.5 + 2.5)};
		auto const on = [&](glm::dvec2 const &P, double const D, double const Y) { return glm::dvec3{P.x + m.x * D, Y, P.y + m.y * D}; };
		glm::dvec3 const ballast{0.55, 0.50, 0.45};
		out.push_back({{on(a, -1.7, formation + 0.35), on(b, -1.7, formation + 0.35), on(b, 1.7, formation + 0.35)}, ballast});
		out.push_back({{on(a, -1.7, formation + 0.35), on(b, 1.7, formation + 0.35), on(a, 1.7, formation + 0.35)}, ballast});
	}
	for (auto const &part : made.mesh.parts())
	{
		auto const colour{colour_of(part)};
		for (std::size_t i = 0; i + 2 < part.points.size(); i += 3)
			out.push_back({{part.points[i], part.points[i + 1], part.points[i + 2]}, colour});
	}
	return out;
}

} // namespace crossing
