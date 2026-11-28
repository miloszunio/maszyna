/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "world/trees/treemesh.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "world/trees/treemath.h"

namespace trees
{

namespace
{

constexpr glm::vec3 up{0.f, 1.f, 0.f};
constexpr float two_pi{2.f * std::numbers::pi_v<float>};
constexpr float golden_angle{2.39996323f}; // radians, the turn between two leaves of a spiral
constexpr float bark_tile{0.6f}; // metres of bark one repeat of its texture covers

// what a level of detail keeps
struct detail
{
	float thinnest{0.f}; // branches thinner than this have no bark
	int sides_most{14};
	float leaf_share{1.f}; // of the leaves of the full tree
	float leaf_scale{1.f}; // and how much larger they are for it
};

detail detail_of(int const Level)
{
	if (Level <= 0)
		return detail{};
	return detail{0.02f, 6, 0.22f, 1.9f};
}

// the outline of a leaf: its width at points along it, the length being 1, from the stalk to the tip. the triangles go between the
// points, so lobes and notches are only a matter of the widths
struct outline_point
{
	float along;
	float half_width;
};

std::span<outline_point const> outline_of(foliage const Leaves)
{
	static constexpr std::array oval{outline_point{0.f, 0.02f}, outline_point{0.18f, 0.3f}, outline_point{0.45f, 0.42f}, outline_point{0.75f, 0.3f}, outline_point{1.f, 0.f}};
	static constexpr std::array lobed{outline_point{0.f, 0.04f},  outline_point{0.15f, 0.26f}, outline_point{0.28f, 0.14f}, outline_point{0.44f, 0.4f},
	                                  outline_point{0.58f, 0.2f}, outline_point{0.74f, 0.38f}, outline_point{0.88f, 0.18f}, outline_point{1.f, 0.f}};
	static constexpr std::array palmate{outline_point{0.f, 0.08f},  outline_point{0.12f, 0.5f}, outline_point{0.3f, 0.22f}, outline_point{0.48f, 0.55f},
	                                    outline_point{0.66f, 0.2f}, outline_point{0.82f, 0.3f}, outline_point{1.f, 0.f}};
	static constexpr std::array heart{outline_point{0.f, 0.18f}, outline_point{0.1f, 0.47f}, outline_point{0.38f, 0.5f}, outline_point{0.7f, 0.32f}, outline_point{1.f, 0.f}};
	static constexpr std::array narrow{outline_point{0.f, 0.02f}, outline_point{0.2f, 0.09f}, outline_point{0.6f, 0.08f}, outline_point{1.f, 0.f}};
	static constexpr std::array pinnate{outline_point{0.f, 0.02f},   outline_point{0.12f, 0.3f}, outline_point{0.2f, 0.04f},  outline_point{0.32f, 0.32f}, outline_point{0.4f, 0.04f},
	                                    outline_point{0.52f, 0.32f}, outline_point{0.6f, 0.04f}, outline_point{0.74f, 0.28f}, outline_point{0.82f, 0.04f}, outline_point{1.f, 0.f}};
	switch (Leaves)
	{
	case foliage::lobed:
		return lobed;
	case foliage::palmate:
		return palmate;
	case foliage::heart:
		return heart;
	case foliage::narrow:
		return narrow;
	case foliage::pinnate:
		return pinnate;
	default:
		return oval;
	}
}

bool needles(foliage const Leaves)
{
	return Leaves == foliage::needle_tuft || Leaves == foliage::needle_brush || Leaves == foliage::needle_flat;
}

// one leaf, a tuft or a spray of needles to be made
struct leaf
{
	glm::vec3 base{0.f}; // where it sits on the twig
	glm::vec3 along{0.f, 1.f, 0.f}; // the way the leaf points
	glm::vec3 face{0.f, 1.f, 0.f}; // the way its upper side faces
	glm::vec3 twig{0.f, 1.f, 0.f}; // the way the twig runs
	float size{0.1f};
	float rank{0.f};
	float phase{0.f};
	std::uint8_t bone{0};
};

class builder
{
  public:
	builder(skeleton const &Skeleton, species const &Species, instance const &Tree, int const Level)
	    : m_skeleton{Skeleton}, m_species{Species}, m_detail{detail_of(Level)}, m_random{Tree.seed ^ hash_text(Species.id) ^ 0x6d657368ULL}
	{
		m_mesh.level = Level;
		children();
	}

	tree_mesh build()
	{
		make_bark();
		make_leaves();
		make_flowers_and_fruits();
		std::erase_if(m_mesh.parts, [](mesh_part const &Part) { return Part.indices.empty(); });
		return std::move(m_mesh);
	}

  private:
	void children()
	{
		m_children.assign(m_skeleton.nodes.size(), 0);
		for (auto const &point : m_skeleton.nodes)
			if (point.parent >= 0)
				++m_children[point.parent];
	}

	mesh_part &part_of(part const Kind, bool const TwoSided)
	{
		auto found{std::ranges::find(m_mesh.parts, Kind, &mesh_part::kind)};
		if (found != m_mesh.parts.end())
			return *found;
		mesh_part made;
		made.kind = Kind;
		made.two_sided = TwoSided;
		m_mesh.parts.push_back(std::move(made));
		return m_mesh.parts.back();
	}

	static std::uint32_t add(mesh_part &Part, vertex const &Vertex)
	{
		Part.vertices.push_back(Vertex);
		return static_cast<std::uint32_t>(Part.vertices.size() - 1);
	}

	static void triangle(mesh_part &Part, std::uint32_t const A, std::uint32_t const B, std::uint32_t const C)
	{
		Part.indices.push_back(A);
		Part.indices.push_back(B);
		Part.indices.push_back(C);
	}

	// --- bark

	// the points a tube of a branch goes through: from the fork it starts at, or from under the ground for a stem
	struct ring_point
	{
		glm::vec3 position;
		float radius;
		std::uint8_t bone;
	};

	std::vector<ring_point> points_of(chain const &Branch) const
	{
		std::vector<ring_point> points;
		auto const &first{m_skeleton.nodes[Branch.nodes.front()]};
		if (Branch.attached >= 0)
		{
			auto const &fork{m_skeleton.nodes[Branch.attached]};
			points.push_back({fork.position, first.radius, static_cast<std::uint8_t>(fork.bone)});
		}
		else
		{
			// the foot widens into the roots, and goes a little under the ground
			points.push_back({first.position - up * 0.3f, first.radius * 1.35f, static_cast<std::uint8_t>(first.bone)});
		}
		for (auto const index : Branch.nodes)
		{
			auto const &point{m_skeleton.nodes[index]};
			points.push_back({point.position, point.radius, static_cast<std::uint8_t>(point.bone)});
		}
		if (Branch.attached < 0 && points.size() > 2)
			points[1].radius *= 1.15f;
		return points;
	}

	void make_bark()
	{
		auto &bark{part_of(part::bark, false)};
		for (std::size_t index = 0; index < m_skeleton.chains.size(); ++index)
		{
			auto const &branch{m_skeleton.chains[index]};
			auto const radius{m_skeleton.nodes[branch.nodes.front()].radius};
			if (branch.order > 0 && radius < m_detail.thinnest)
				continue;
			auto const points{points_of(branch)};
			if (points.size() < 2)
				continue;
			auto const sides{std::clamp(static_cast<int>(3.f + radius * 90.f), 3, m_detail.sides_most)};
			tube(bark, points, sides, branch.order, static_cast<float>(index % 97U) / 97.f);
		}
	}

	// a tube through the points, closed with a point at its end
	void tube(mesh_part &Bark, std::vector<ring_point> const &Points, int const Sides, int const Order, float const Phase) const
	{
		auto const count{Points.size()};
		auto const first_vertex{static_cast<std::uint32_t>(Bark.vertices.size())};
		auto const wraps{std::max(1.f, std::floor(two_pi * Points.front().radius / bark_tile + 0.5f))};
		glm::vec3 normal{0.f};
		auto length{0.f};
		for (std::size_t index = 0; index < count; ++index)
		{
			auto const &point{Points[index]};
			auto const behind{Points[index == 0 ? 0 : index - 1].position};
			auto const ahead{Points[index + 1 < count ? index + 1 : count - 1].position};
			auto const along{unit(ahead - behind)};
			// carried along the tube so it doesn't twist
			normal = index == 0 ? square_to(along) : unit(normal - along * glm::dot(normal, along), square_to(along));
			auto const side{glm::cross(along, normal)};
			if (index > 0)
				length += glm::distance(point.position, Points[index - 1].position);
			for (int step = 0; step <= Sides; ++step)
			{
				auto const angle{two_pi * static_cast<float>(step) / static_cast<float>(Sides)};
				auto const c{cosine(angle)};
				auto const s{sine(angle)};
				auto const outward{normal * c + side * s};
				vertex made;
				made.position = point.position + outward * point.radius;
				made.normal = outward;
				made.uv = glm::vec2{wraps * static_cast<float>(step) / static_cast<float>(Sides), length / bark_tile};
				made.tangent = glm::vec4{side * c - normal * s, 1.f};
				made.bone = point.bone;
				made.phase = Phase;
				made.flex = std::min(1.f, static_cast<float>(Order) * 0.25f);
				Bark.vertices.push_back(made);
			}
		}
		auto const row{static_cast<std::uint32_t>(Sides + 1)};
		for (std::uint32_t ring = 0; ring + 1 < count; ++ring)
		{
			for (std::uint32_t step = 0; step < static_cast<std::uint32_t>(Sides); ++step)
			{
				auto const a{first_vertex + ring * row + step};
				auto const b{a + 1};
				auto const c{b + row};
				auto const d{a + row};
				triangle(Bark, a, b, c);
				triangle(Bark, a, c, d);
			}
		}
		// the end: a point a little beyond the last ring
		auto const &last{Points.back()};
		auto const end_along{unit(last.position - Points[count - 2].position)};
		vertex tip;
		tip.position = last.position + end_along * (last.radius * 1.5f);
		tip.normal = end_along;
		tip.uv = glm::vec2{0.5f * wraps, (length + last.radius) / bark_tile};
		tip.bone = last.bone;
		tip.phase = Phase;
		tip.flex = std::min(1.f, static_cast<float>(Order) * 0.25f);
		auto const apex{add(Bark, tip)};
		auto const last_ring{first_vertex + static_cast<std::uint32_t>(count - 1) * row};
		for (std::uint32_t step = 0; step < static_cast<std::uint32_t>(Sides); ++step)
			triangle(Bark, last_ring + step, last_ring + step + 1, apex);
	}

	// --- leaves

	// where the leaves grow: the thin, living branches
	bool leafy(node const &Point) const
	{
		return Point.parent >= 0 && false == Point.dead && Point.order >= 1 && Point.radius <= m_species.twig_radius;
	}

	std::vector<leaf> place_leaves()
	{
		auto random{m_random.fork(0x1eafU)};
		auto const &points{m_skeleton.nodes};
		auto expected{0.f};
		for (auto const &point : points)
			if (leafy(point))
				expected += glm::distance(point.position, points[point.parent].position) * m_species.leaf_density;
		auto const budget{std::max(1.f, m_species.leaf_budget * m_detail.leaf_share)};
		auto const keep{std::min(1.f, budget / std::max(1.f, expected))};
		// fewer leaves are larger, so the crown stays as full
		auto const enlarge{std::clamp(std::sqrt(std::max(1.f, expected * m_detail.leaf_share) / budget), 1.f, 2.4f) * m_detail.leaf_scale};
		std::vector<leaf> leaves;
		auto turn{0.f};
		for (auto const &point : points)
		{
			if (false == leafy(point))
				continue;
			auto const &parent{points[point.parent]};
			auto const span{point.position - parent.position};
			auto const count{static_cast<int>(glm::length(span) * m_species.leaf_density + random.unit())};
			for (int index = 0; index < count; ++index)
			{
				turn += golden_angle;
				if (false == random.chance(keep))
					continue;
				leaves.push_back(leaf_at(parent.position + span * random.unit(), unit(span), turn, static_cast<std::uint8_t>(point.bone), enlarge, random));
			}
		}
		// in the order they come and go: the renderer draws as many of the first ones as the season has
		std::ranges::stable_sort(leaves, {}, &leaf::rank);
		return leaves;
	}

	leaf leaf_at(glm::vec3 const &Base, glm::vec3 const &Twig, float const Turn, std::uint8_t const Bone, float const Enlarge, random &Random) const
	{
		auto const across{square_to(Twig)};
		auto const radial{turned(across, Twig, Turn + Random.spread(0.4f))};
		leaf made;
		made.base = Base;
		made.twig = Twig;
		if (needles(m_species.leaves))
		{
			made.along = Twig;
			made.face = radial;
		}
		else
		{
			made.along = unit(Twig * 0.45f + radial * 0.85f - up * 0.25f + Random.direction() * 0.25f);
			// the upper side towards the sky, as much as the leaf lets it
			auto const face{up + Random.direction() * 0.35f};
			made.face = unit(face - made.along * glm::dot(face, made.along), radial);
		}
		made.size = m_species.leaf_size * Enlarge * Random.range(0.75f, 1.25f);
		made.rank = Random.unit();
		made.phase = Random.unit();
		made.bone = Bone;
		return made;
	}

	void make_leaves()
	{
		auto const leaves{place_leaves()};
		auto &part{part_of(part::leaves, true)};
		for (auto const &placed : leaves)
		{
			switch (m_species.leaves)
			{
			case foliage::needle_tuft:
				needle_tuft(part, placed);
				break;
			case foliage::needle_brush:
				needle_comb(part, placed, 6, false);
				break;
			case foliage::needle_flat:
				needle_comb(part, placed, 6, true);
				break;
			default:
				leaf_blade(part, placed);
				break;
			}
		}
	}

	static vertex leaf_vertex(leaf const &Leaf, glm::vec3 const &Position, glm::vec3 const &Normal, glm::vec2 const &Uv, glm::vec3 const &Tangent)
	{
		vertex made;
		made.position = Position;
		made.normal = Normal;
		made.uv = Uv;
		made.tangent = glm::vec4{Tangent, 1.f};
		made.bone = Leaf.bone;
		made.phase = Leaf.phase;
		made.flex = 1.f;
		made.rank = Leaf.rank;
		return made;
	}

	// a leaf: a strip of triangles between the points of its outline, folded along the midrib and curled a little
	void leaf_blade(mesh_part &Part, leaf const &Leaf) const
	{
		auto const shape{outline_of(m_species.leaves)};
		auto const side{glm::cross(Leaf.along, Leaf.face)};
		constexpr float fold{0.18f};
		constexpr float curl{0.12f};
		auto const stalk{Leaf.base + Leaf.along * (Leaf.size * 0.12f)};
		auto const left_normal{unit(Leaf.face + side * fold)};
		auto const right_normal{unit(Leaf.face - side * fold)};
		std::uint32_t previous_left{0};
		std::uint32_t previous_right{0};
		for (std::size_t index = 0; index < shape.size(); ++index)
		{
			auto const &point{shape[index]};
			auto const centre{stalk + Leaf.along * (point.along * Leaf.size) + Leaf.face * (curl * point.along * point.along * Leaf.size * -1.f)};
			auto const width{point.half_width * Leaf.size};
			auto const rise{Leaf.face * (fold * width)};
			auto const left{add(Part, leaf_vertex(Leaf, centre - side * width + rise, left_normal, {0.5f - point.half_width, point.along}, side))};
			auto const right{add(Part, leaf_vertex(Leaf, centre + side * width + rise, right_normal, {0.5f + point.half_width, point.along}, side))};
			if (index > 0)
			{
				triangle(Part, previous_left, previous_right, right);
				triangle(Part, previous_left, right, left);
			}
			previous_left = left;
			previous_right = right;
		}
	}

	// a needle: a long, thin triangle from its foot on the twig to its point
	static void needle(mesh_part &Part, leaf const &Leaf, glm::vec3 const &Foot, glm::vec3 const &Direction, float const Length)
	{
		auto const width{std::max(0.0012f, Length * 0.04f)};
		auto const side{square_to(Direction)};
		auto const normal{unit(glm::cross(side, Direction))};
		auto const a{add(Part, leaf_vertex(Leaf, Foot - side * width, normal, {0.f, 0.f}, side))};
		auto const b{add(Part, leaf_vertex(Leaf, Foot + side * width, normal, {1.f, 0.f}, side))};
		auto const c{add(Part, leaf_vertex(Leaf, Foot + Direction * Length, normal, {0.5f, 1.f}, side))};
		triangle(Part, a, b, c);
	}

	// pine: a tuft of long needles round the shoot, open forwards
	void needle_tuft(mesh_part &Part, leaf const &Leaf) const
	{
		constexpr int count{8};
		auto const across{square_to(Leaf.twig)};
		for (int index = 0; index < count; ++index)
		{
			auto const radial{turned(across, Leaf.twig, two_pi * static_cast<float>(index) / static_cast<float>(count) + Leaf.phase * two_pi)};
			auto const direction{unit(Leaf.twig * 0.75f + radial * 0.66f - up * 0.1f)};
			needle(Part, Leaf, Leaf.base, direction, Leaf.size);
		}
	}

	// the turn of a needle of a comb round the shoot: to either side for a flat spray, all round otherwise
	static float comb_turn(int const Index, int const Count, bool const Flat, float const Phase)
	{
		if (Flat)
			return Index % 2 == 0 ? 0.f : std::numbers::pi_v<float>;
		return two_pi * (static_cast<float>(Index) / static_cast<float>(Count) + Phase);
	}

	// spruce: needles all round a short piece of the shoot; fir and yew: only to the sides, in a flat spray
	void needle_comb(mesh_part &Part, leaf const &Leaf, int const Count, bool const Flat) const
	{
		auto const across{Flat ? unit(glm::cross(Leaf.twig, up), square_to(Leaf.twig)) : square_to(Leaf.twig)};
		for (int index = 0; index < Count; ++index)
		{
			auto const turn{comb_turn(index, Count, Flat, Leaf.phase)};
			auto const radial{turned(across, Leaf.twig, turn)};
			auto const foot{Leaf.base + Leaf.twig * (Leaf.size * 0.5f * static_cast<float>(index) / static_cast<float>(Count))};
			needle(Part, Leaf, foot, unit(radial + Leaf.twig * 0.55f), Leaf.size);
		}
	}

	// --- flowers and fruits, at the ends of the twigs

	std::vector<int> tips() const
	{
		std::vector<int> found;
		for (std::size_t index = 0; index < m_skeleton.nodes.size(); ++index)
			if (m_children[index] == 0 && leafy(m_skeleton.nodes[index]))
				found.push_back(static_cast<int>(index));
		return found;
	}

	void make_flowers_and_fruits()
	{
		if (m_species.flower_size <= 0.f && m_species.fruit_size <= 0.f)
			return;
		auto random{m_random.fork(0xf10eU)};
		auto const ends{tips()};
		auto const share{m_detail.leaf_share};
		if (m_species.flower_size > 0.f)
		{
			auto &flowers{part_of(part::flowers, true)};
			auto const keep{std::min(1.f, 2400.f * share / std::max(1.f, static_cast<float>(ends.size()) * 3.f))};
			for (auto const index : ends)
				for (int flower = 0; flower < 3; ++flower)
					if (random.chance(keep))
						blossom(flowers, index, m_species.flower_size * m_detail.leaf_scale, random);
		}
		if (m_species.fruit_size > 0.f)
		{
			auto &fruits{part_of(part::fruits, false)};
			auto const keep{std::min(1.f, 700.f * share / std::max(1.f, static_cast<float>(ends.size())))};
			for (auto const index : ends)
				if (random.chance(keep))
					fruit(fruits, index, m_species.fruit_size * std::sqrt(m_detail.leaf_scale), random);
		}
	}

	// five petals round a centre, facing out and up
	void blossom(mesh_part &Part, int const Index, float const Size, random &Random) const
	{
		auto const &point{m_skeleton.nodes[Index]};
		auto const centre{point.position + Random.in_ball() * (Size * 2.f)};
		auto const face{unit(up + Random.direction() * 0.6f)};
		auto const across{square_to(face)};
		leaf petal;
		petal.bone = static_cast<std::uint8_t>(point.bone);
		petal.phase = Random.unit();
		petal.rank = Random.unit();
		auto const middle{add(Part, leaf_vertex(petal, centre + face * (Size * 0.15f), face, {0.5f, 0.5f}, across))};
		constexpr int count{5};
		std::array<std::uint32_t, count * 2> rim{};
		for (int index = 0; index < count * 2; ++index)
		{
			auto const angle{two_pi * static_cast<float>(index) / static_cast<float>(count * 2)};
			auto const reach{index % 2 == 0 ? Size : Size * 0.45f};
			auto const direction{turned(across, face, angle)};
			rim[index] = add(Part, leaf_vertex(petal, centre + direction * reach, face, {0.5f + 0.5f * cosine(angle), 0.5f + 0.5f * sine(angle)}, across));
		}
		for (int index = 0; index < count * 2; ++index)
			triangle(Part, middle, rim[index], rim[(index + 1) % (count * 2)]);
	}

	// a small round fruit hanging under the twig: eight faces
	void fruit(mesh_part &Part, int const Index, float const Size, random &Random) const
	{
		auto const &point{m_skeleton.nodes[Index]};
		auto const centre{point.position - up * (Size * 1.4f) + Random.in_ball() * Size};
		leaf hanging;
		hanging.bone = static_cast<std::uint8_t>(point.bone);
		hanging.phase = Random.unit();
		hanging.rank = Random.unit();
		std::array const corners{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec3{-1.f, 0.f, 0.f}, glm::vec3{0.f, 0.f, -1.f}};
		auto const top{add(Part, leaf_vertex(hanging, centre + up * Size, up, {0.5f, 1.f}, corners[0]))};
		auto const bottom{add(Part, leaf_vertex(hanging, centre - up * (Size * 1.1f), -up, {0.5f, 0.f}, corners[0]))};
		std::array<std::uint32_t, 4> middle{};
		for (std::size_t index = 0; index < corners.size(); ++index)
			middle[index] = add(Part, leaf_vertex(hanging, centre + corners[index] * Size, corners[index], {0.25f * static_cast<float>(index), 0.5f}, up));
		for (std::size_t index = 0; index < corners.size(); ++index)
		{
			auto const next{(index + 1) % corners.size()};
			triangle(Part, top, middle[next], middle[index]);
			triangle(Part, bottom, middle[index], middle[next]);
		}
	}

	skeleton const &m_skeleton;
	species const &m_species;
	detail m_detail;
	random m_random;
	tree_mesh m_mesh;
	std::vector<int> m_children;
};

} // namespace

std::size_t tree_mesh::triangles() const
{
	std::size_t count{0};
	for (auto const &piece : parts)
		count += piece.indices.size() / 3;
	return count;
}

mesh_part const *tree_mesh::find(part const Kind) const
{
	auto const found{std::ranges::find(parts, Kind, &mesh_part::kind)};
	return found == parts.end() ? nullptr : &*found;
}

tree_mesh build_mesh(skeleton const &Skeleton, species const &Species, instance const &Tree, int const Level)
{
	return builder{Skeleton, Species, Tree, Level}.build();
}

} // namespace trees
