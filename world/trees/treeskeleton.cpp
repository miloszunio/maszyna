/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "world/trees/treeskeleton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <unordered_map>

#include "world/trees/treemath.h"

namespace trees
{

namespace
{

constexpr glm::vec3 up{0.f, 1.f, 0.f};
constexpr float two_pi{2.f * std::numbers::pi_v<float>};
// the most points of one tree, whatever the profile asks for
constexpr std::size_t nodes_most{20000};

float square(float const Value)
{
	return Value * Value;
}

// the space the crown grows into
struct envelope
{
	crown shape{crown::ellipsoid};
	float bottom{0.f};
	float top{1.f};
	float radius{1.f};
	glm::vec2 centre{0.f};
	glm::vec2 phase{1.f, 0.f}; // the turn of the lobes, as a cosine and a sine
	std::array<float, 3> lobes{}; // how much the crown bulges in 2, 3 and 5 places round

	// the radius of the crown at a height, 0..1 of the widest, T 0..1 from its bottom to its top
	float profile(float const T) const
	{
		auto const u{2.f * T - 1.f};
		switch (shape)
		{
		case crown::round:
			return std::sqrt(std::max(0.f, 1.f - u * u * u * u));
		case crown::cone:
			return std::max(0.f, 1.f - T) * std::min(1.f, T * 12.f + 0.3f);
		case crown::column:
			return T < 0.8f ? std::sqrt(std::min(1.f, T * 8.f + 0.2f)) : std::sqrt(std::max(0.f, (1.f - T) / 0.2f));
		case crown::umbrella:
			return T < 0.55f ? 0.35f + 0.65f * T / 0.55f : std::sqrt(std::max(0.f, 1.f - square((T - 0.55f) / 0.45f)));
		case crown::ellipsoid:
		case crown::weeping:
		default:
			return std::sqrt(std::max(0.f, 1.f - u * u));
		}
	}

	bool inside(glm::vec3 const &Point) const
	{
		if (Point.y < bottom || Point.y > top)
			return false;
		glm::vec2 const offset{Point.x - centre.x, Point.z - centre.y};
		auto const distance2{glm::dot(offset, offset)};
		auto reach{radius * profile((Point.y - bottom) / (top - bottom))};
		if (distance2 > 1e-8f)
		{
			// cos of 2, 3 and 5 times the azimuth, from its cosine (Chebyshev), turned by the phase
			auto const c{(offset.x * phase.x - offset.y * phase.y) / std::sqrt(distance2)};
			auto const c2{2.f * c * c - 1.f};
			auto const c3{4.f * c * c * c - 3.f * c};
			auto const c5{16.f * c * c * c * c * c - 20.f * c * c * c + 5.f * c};
			reach *= 1.f + lobes[0] * c2 + lobes[1] * c3 + lobes[2] * c5;
		}
		return distance2 <= reach * reach;
	}
};

// the points of the tree in cubes, to find the nearest one quickly. what it finds doesn't depend on the order of the hash map:
// the cubes are searched in a fixed order and an equal distance goes to the lower index
class grid
{
  public:
	explicit grid(float const Cell) : m_cell{Cell} {}
	void add(int const Index, glm::vec3 const &Position)
	{
		m_cells[key(cell_of(Position))].push_back(Index);
	}
	// the nearest point within Reach, -1 if none
	int nearest(glm::vec3 const &Position, float const Reach, std::vector<node> const &Nodes) const
	{
		auto const centre{cell_of(Position)};
		auto const span{static_cast<int>(std::ceil(Reach / m_cell))};
		int found{-1};
		auto best{Reach * Reach};
		for (int dz = -span; dz <= span; ++dz)
			for (int dy = -span; dy <= span; ++dy)
				for (int dx = -span; dx <= span; ++dx)
					search(centre + glm::ivec3{dx, dy, dz}, Position, Nodes, found, best);
		return found;
	}

  private:
	glm::ivec3 cell_of(glm::vec3 const &Position) const
	{
		return glm::ivec3{static_cast<int>(std::floor(Position.x / m_cell)), static_cast<int>(std::floor(Position.y / m_cell)), static_cast<int>(std::floor(Position.z / m_cell))};
	}
	static std::int64_t key(glm::ivec3 const &Cell)
	{
		return (static_cast<std::int64_t>(Cell.x + 0x100000) << 42) | (static_cast<std::int64_t>(Cell.y + 0x100000) << 21) | static_cast<std::int64_t>(Cell.z + 0x100000);
	}
	void search(glm::ivec3 const &Cell, glm::vec3 const &Position, std::vector<node> const &Nodes, int &Found, float &Best) const
	{
		auto const cell{m_cells.find(key(Cell))};
		if (cell == m_cells.end())
			return;
		for (auto const index : cell->second)
		{
			auto const offset{Nodes[index].position - Position};
			auto const distance2{glm::dot(offset, offset)};
			if (distance2 < Best || (distance2 == Best && index < Found))
			{
				Best = distance2;
				Found = index;
			}
		}
	}

	float m_cell;
	std::unordered_map<std::int64_t, std::vector<int>> m_cells;
};

// the numbers the growing of one tree works with
struct plan
{
	float height{15.f};
	float segment{0.3f}; // metres, the step of growth
	float crown_bottom{3.f};
	float crown_radius{4.f};
	float base_radius{0.4f};
	int stems{1};
	float trim{0.f}; // nothing above it, 0: no limit
};

plan plan_of(species const &Species, instance const &Tree)
{
	plan result;
	result.height = std::max(0.5f, Tree.height);
	result.segment = std::clamp(result.height / 55.f, 0.06f, 0.45f);
	auto base{Species.crown_base};
	auto width{Species.crown_width};
	switch (Tree.grown)
	{
	case form::forest:
		base += (0.85f - base) * 0.4f;
		width *= 0.7f;
		break;
	case form::edge:
		base += (0.85f - base) * 0.15f;
		width *= 0.85f;
		break;
	case form::open:
	default:
		break;
	}
	if (Tree.pollarded)
	{
		base = std::max(base, 0.45f);
		width = std::min(width, 0.8f);
	}
	result.crown_bottom = base * result.height;
	result.crown_radius = 0.5f * width * result.height;
	result.base_radius = Species.trunk_radius * result.height;
	result.trim = Tree.trim_height;
	return result;
}

class grower
{
  public:
	grower(species const &Species, instance const &Tree) : m_species{Species}, m_tree{Tree}, m_plan{plan_of(Species, Tree)}, m_random{Tree.seed ^ hash_text(Species.id)} {}

	skeleton grow()
	{
		if (m_species.kind == growth::whorls)
			grow_whorls();
		else
			grow_colonised();
		add_twigs();
		smooth();
		set_radii();
		make_chains();
		mark_dead();
		make_bones();
		measure();
		return std::move(m_skeleton);
	}

  private:
	std::vector<node> &nodes()
	{
		return m_skeleton.nodes;
	}

	bool allowed(glm::vec3 const &Position) const
	{
		if (Position.y < 0.05f)
			return false;
		return m_plan.trim <= 0.f || Position.y <= m_plan.trim;
	}

	int add(glm::vec3 const &Position, int const Parent)
	{
		node point;
		point.position = Position;
		point.parent = Parent;
		nodes().push_back(point);
		m_reach.push_back(Parent < 0 ? 0.f : m_reach[Parent] + glm::distance(Position, nodes()[Parent].position));
		m_stem.push_back(false);
		return static_cast<int>(nodes().size()) - 1;
	}

	// a stem from Foot along Direction up to the height Top, wandering a little; its last point
	int add_stem(glm::vec3 const &Foot, glm::vec3 const &Direction, float const Top)
	{
		auto random{m_random.fork(0x57e3U)};
		auto position{Foot};
		auto index{add(position, -1)};
		m_stem[index] = true;
		auto direction{Direction};
		auto const wobble{m_species.trunk_wobble * 4.f};
		auto steps{0};
		while (position.y < Top && steps < 400)
		{
			++steps;
			direction = unit(direction + glm::vec3{random.spread(wobble), 0.f, random.spread(wobble)} + (Direction - direction) * 0.35f);
			position += direction * m_plan.segment;
			index = add(position, index);
			m_stem[index] = true;
		}
		return index;
	}

	// the stems of the tree; one for a tree, more for a shrub or coppice
	void add_stems(float const Top)
	{
		m_plan.stems = m_random.integer(m_species.stems_min, m_species.stems_max);
		auto const start{m_random.unit() * two_pi};
		for (int stem = 0; stem < m_plan.stems; ++stem)
		{
			auto const angle{start + two_pi * static_cast<float>(stem) / static_cast<float>(m_plan.stems) + m_random.spread(0.5f)};
			glm::vec3 const outward{cosine(angle), 0.f, sine(angle)};
			auto const many{m_plan.stems > 1};
			auto const foot{many ? outward * m_random.range(0.03f, 0.08f) * m_plan.height : glm::vec3{0.f}};
			auto const lean{many ? m_species.stem_spread * m_random.range(0.6f, 1.2f) : 0.f};
			auto const direction{unit(up + outward * lean + glm::vec3{m_random.spread(m_species.trunk_lean), 0.f, m_random.spread(m_species.trunk_lean)} * 2.f)};
			auto const top{many ? Top * m_random.range(0.7f, 1.1f) : Top};
			add_stem(foot, direction, std::max(top, m_plan.segment * 2.f));
		}
		m_plan.base_radius /= std::sqrt(static_cast<float>(m_plan.stems));
	}

	envelope envelope_of()
	{
		envelope shape;
		shape.shape = m_species.shape;
		shape.bottom = m_plan.crown_bottom;
		if (m_species.shape == crown::weeping)
			shape.bottom = std::min(shape.bottom, 0.12f * m_plan.height);
		shape.top = m_plan.height;
		shape.radius = m_plan.crown_radius;
		if (m_tree.grown == form::edge)
		{
			auto const side{m_random.unit() * two_pi};
			shape.centre = glm::vec2{cosine(side), sine(side)} * (0.2f * m_plan.crown_radius);
		}
		auto const turn{m_random.unit() * two_pi};
		shape.phase = glm::vec2{cosine(turn), sine(turn)};
		shape.lobes = {m_random.range(0.f, 0.14f), m_random.range(0.f, 0.1f), m_random.range(0.f, 0.06f)};
		return shape;
	}

	std::vector<glm::vec3> attractors_in(envelope const &Shape)
	{
		auto const scale{(0.45f + 0.55f * m_plan.height / m_species.height_max) * (0.5f + 0.5f * m_tree.vitality)};
		auto const count{static_cast<std::size_t>(std::max(50.f, m_species.attractors * scale))};
		std::vector<glm::vec3> points;
		points.reserve(count);
		auto const wide{Shape.radius * 1.35f};
		auto const clumps{clumps_in(Shape)};
		auto attempts{count * 40};
		while (points.size() < count && attempts > 0)
		{
			--attempts;
			glm::vec3 const point{Shape.centre.x + m_random.spread(wide), m_random.range(Shape.bottom, Shape.top), Shape.centre.y + m_random.spread(wide)};
			if (Shape.inside(point) && allowed(point) && in_clumps(point, clumps))
				points.push_back(point);
		}
		return points;
	}

	// a crown of separate masses of foliage (the old pine): the balls they fill, the radius being the fourth number
	std::vector<glm::vec4> clumps_in(envelope const &Shape)
	{
		std::vector<glm::vec4> clumps;
		if (m_species.clumps < 1.f)
			return clumps;
		auto const count{std::max(2, static_cast<int>(m_species.clumps + m_random.spread(2.f) + 0.5f))};
		auto attempts{count * 50};
		while (static_cast<int>(clumps.size()) < count && attempts > 0)
		{
			--attempts;
			glm::vec3 const centre{Shape.centre.x + m_random.spread(Shape.radius), m_random.range(Shape.bottom, Shape.top), Shape.centre.y + m_random.spread(Shape.radius)};
			if (Shape.inside(centre))
				clumps.emplace_back(centre, Shape.radius * m_random.range(0.28f, 0.45f));
		}
		return clumps;
	}

	static bool in_clumps(glm::vec3 const &Point, std::vector<glm::vec4> const &Clumps)
	{
		if (Clumps.empty())
			return true;
		return std::ranges::any_of(Clumps,
		                           [&Point](glm::vec4 const &Clump)
		                           {
			                           auto const offset{Point - glm::vec3{Clump}};
			                           return glm::dot(offset, offset) <= Clump.w * Clump.w;
		                           });
	}

	void grow_colonised()
	{
		add_stems(std::max(m_plan.crown_bottom, 0.4f));
		auto const shape{envelope_of()};
		auto attractors{attractors_in(shape)};
		colonise(attractors);
	}

	// the space colonisation of Runions, Lane and Prusinkiewicz (2007): every free point pulls the nearest branch towards it, the
	// branches pulled grow a step, a point reached is taken
	void colonise(std::vector<glm::vec3> &Attractors)
	{
		auto const segment{m_plan.segment};
		auto const influence{m_species.influence * segment};
		auto const kill{m_species.kill * segment};
		grid points{influence};
		for (int index = 0; index < static_cast<int>(nodes().size()); ++index)
			points.add(index, nodes()[index].position);
		auto random{m_random.fork(0xc010U)};
		std::vector<glm::vec3> pull;
		std::vector<int> pulled;
		for (int round = 0; round < 500 && false == Attractors.empty() && nodes().size() < nodes_most; ++round)
		{
			pull.assign(nodes().size(), glm::vec3{0.f});
			pulled.assign(nodes().size(), 0);
			std::erase_if(Attractors,
			              [&](glm::vec3 const &Attractor)
			              {
				              auto const nearest{points.nearest(Attractor, influence, nodes())};
				              if (nearest < 0)
					              return false;
				              auto const offset{Attractor - nodes()[nearest].position};
				              if (glm::dot(offset, offset) < kill * kill)
					              return true;
				              pull[nearest] += unit(offset);
				              ++pulled[nearest];
				              return false;
			              });
			if (false == sprout(pull, pulled, points, random))
				break;
		}
	}

	// the step of growth of every branch pulled; false if none grew
	bool sprout(std::vector<glm::vec3> const &Pull, std::vector<int> const &Pulled, grid &Points, random &Random)
	{
		auto grew{false};
		auto const count{static_cast<int>(Pull.size())};
		for (int index = 0; index < count && nodes().size() < nodes_most; ++index)
		{
			if (Pulled[index] == 0)
				continue;
			auto const position{nodes()[index].position + direction_of(index, Pull[index], Random) * m_plan.segment};
			if (false == allowed(position))
				continue;
			// a branch doesn't grow into one it already has
			if (Points.nearest(position, m_plan.segment * 0.35f, nodes()) >= 0)
				continue;
			auto const added{add(position, index)};
			Points.add(added, position);
			grew = true;
		}
		return grew;
	}

	glm::vec3 direction_of(int const Index, glm::vec3 const &Pull, random &Random) const
	{
		auto direction{unit(Pull)};
		direction += up * m_species.tropism_up;
		auto const outer{std::min(1.f, m_reach[Index] / std::max(0.5f, m_plan.crown_radius))};
		direction -= up * (m_species.droop * outer * outer);
		direction += Random.direction() * (m_species.wander * 0.5f);
		return unit(direction, up);
	}

	// spruce, fir, larch: tiers of branches round a straight trunk
	void grow_whorls()
	{
		m_plan.stems = 1;
		auto const segment{std::clamp(m_plan.height / 70.f, 0.15f, 0.4f)};
		m_plan.segment = segment;
		auto const lean{glm::vec3{m_random.spread(m_species.trunk_lean), 0.f, m_random.spread(m_species.trunk_lean)}};
		add_stem(glm::vec3{0.f}, unit(up + lean), m_plan.height);
		auto const trunk{static_cast<int>(nodes().size())};
		auto height{m_plan.crown_bottom + m_random.range(0.f, m_species.whorl_step)};
		auto random{m_random.fork(0x3a0bU)};
		while (height < m_plan.height - 0.3f && nodes().size() < nodes_most)
		{
			add_whorl(height, trunk, random);
			height += m_species.whorl_step * random.range(0.8f, 1.2f);
		}
	}

	// the point of the trunk nearest to a height
	int trunk_at(float const Height, int const Trunk) const
	{
		int best{0};
		auto distance{1e9f};
		for (int index = 0; index < Trunk; ++index)
		{
			if (auto const offset{std::abs(m_skeleton.nodes[index].position.y - Height)}; offset < distance)
			{
				distance = offset;
				best = index;
			}
		}
		return best;
	}

	void add_whorl(float const Height, int const Trunk, random &Random)
	{
		auto const foot{trunk_at(Height, Trunk)};
		auto const t{(Height - m_plan.crown_bottom) / std::max(0.1f, m_plan.height - m_plan.crown_bottom)};
		envelope shape;
		shape.shape = m_species.shape;
		auto const reach{m_plan.crown_radius * shape.profile(t)};
		auto const count{std::max(2, static_cast<int>(m_species.whorl_branches + Random.spread(1.5f) + 0.5f))};
		auto const start{Random.unit() * two_pi};
		for (int branch = 0; branch < count; ++branch)
		{
			auto const angle{start + two_pi * static_cast<float>(branch) / static_cast<float>(count) + Random.spread(0.35f)};
			auto const length{reach * Random.range(0.8f, 1.15f) + 0.15f};
			if (allowed(nodes()[foot].position + glm::vec3{0.f, 0.2f, 0.f}))
				add_tier_branch(foot, glm::vec3{cosine(angle), 0.f, sine(angle)}, length, Random);
		}
	}

	// one branch of a tier, with its side branches
	void add_tier_branch(int const Foot, glm::vec3 const &Outward, float const Length, random &Random)
	{
		auto const steps{std::max(2, static_cast<int>(Length / m_plan.segment))};
		auto const angle{radians(m_species.branch_angle)};
		auto const side_every{std::max(1, steps / std::max(1, static_cast<int>(m_species.side_branches)))};
		auto index{Foot};
		auto position{nodes()[Foot].position};
		auto left{Random.chance(0.5f)};
		for (int step = 1; step <= steps && nodes().size() < nodes_most; ++step)
		{
			auto const along{static_cast<float>(step) / static_cast<float>(steps)};
			auto const rise{cosine(angle) - m_species.branch_droop * along * 1.2f + m_species.branch_upturn * along * along * along * 2.f};
			auto const direction{unit(Outward * sine(angle) + up * rise + Random.direction() * 0.08f)};
			auto const next{position + direction * m_plan.segment};
			if (false == allowed(next))
				break;
			position = next;
			index = add(position, index);
			if (step % side_every == 0 && step < steps)
			{
				add_side_branch(index, direction, left, Length * 0.35f * (1.f - along) + 0.12f, Random);
				left = false == left;
			}
		}
	}

	void add_side_branch(int const From, glm::vec3 const &Along, bool const Left, float const Length, random &Random)
	{
		auto direction{unit(turned(Along, up, radians(Left ? 65.f : -65.f)) - up * (m_species.branch_droop * 0.3f))};
		auto index{From};
		auto const steps{std::max(1, static_cast<int>(Length / m_plan.segment))};
		for (int step = 0; step < steps && nodes().size() < nodes_most; ++step)
		{
			auto const position{nodes()[index].position + direction * m_plan.segment};
			if (false == allowed(position))
				return;
			index = add(position, index);
			direction = unit(direction + Random.direction() * 0.15f - up * 0.04f);
		}
	}

	// short twigs at the ends of the branches: the fine work of the crown, seen best in the winter
	void add_twigs()
	{
		auto random{m_random.fork(0x7319U)};
		auto const count{nodes().size()};
		std::vector<int> children(count, 0);
		for (auto const &point : nodes())
			if (point.parent >= 0)
				++children[point.parent];
		for (std::size_t index = 0; index < count && nodes().size() < nodes_most; ++index)
		{
			if (children[index] != 0 || m_stem[index] || nodes()[index].parent < 0)
				continue;
			add_twigs_at(static_cast<int>(index), random);
		}
	}

	void add_twigs_at(int const Index, random &Random)
	{
		if (m_species.hang > 0.f)
			add_hanging_shoot(Index, Random);
		auto const &point{nodes()[Index]};
		auto const along{unit(point.position - nodes()[point.parent].position)};
		auto const count{static_cast<int>(m_species.twigs * Random.range(0.6f, 1.4f) + 0.5f)};
		for (int twig = 0; twig < count; ++twig)
		{
			auto direction{unit(along + Random.direction() * 0.9f + up * m_species.tropism_up - up * (m_species.droop * 0.6f))};
			auto from{Index};
			for (int step = 0; step < 2; ++step)
			{
				auto const position{nodes()[from].position + direction * (m_species.twig_length * 0.5f * Random.range(0.7f, 1.3f))};
				if (false == allowed(position))
					break;
				from = add(position, from);
				direction = unit(direction + Random.direction() * 0.3f - up * (m_species.droop * 0.3f));
			}
		}
	}

	// a long, thin shoot falling from the end of a branch: the curtains of the weeping willow, the hanging twigs of the birch
	void add_hanging_shoot(int const Index, random &Random)
	{
		auto const &point{nodes()[Index]};
		auto direction{unit(point.position - nodes()[point.parent].position)};
		auto const length{m_species.hang * Random.range(0.5f, 1.2f)};
		auto const step{std::clamp(length / 8.f, 0.08f, 0.3f)};
		auto from{Index};
		auto const steps{static_cast<int>(length / step)};
		for (int done = 0; done < steps && nodes().size() < nodes_most; ++done)
		{
			direction = unit(direction - up * 0.45f + Random.direction() * 0.08f);
			auto const position{nodes()[from].position + direction * step};
			if (false == allowed(position))
				return;
			from = add(position, from);
		}
	}

	// the zigzag of the steps of growth evened out
	void smooth()
	{
		auto const count{nodes().size()};
		std::vector<int> children(count, 0);
		std::vector<int> child(count, -1);
		for (std::size_t index = 0; index < count; ++index)
		{
			if (auto const parent{nodes()[index].parent}; parent >= 0)
			{
				++children[parent];
				child[parent] = static_cast<int>(index);
			}
		}
		for (int pass = 0; pass < 2; ++pass)
		{
			std::vector<glm::vec3> moved(count);
			for (std::size_t index = 0; index < count; ++index)
			{
				auto const &point{nodes()[index]};
				moved[index] = point.position;
				if (point.parent < 0 || children[index] != 1)
					continue;
				moved[index] = point.position * 0.5f + (nodes()[point.parent].position + nodes()[child[index]].position) * 0.25f;
			}
			for (std::size_t index = 0; index < count; ++index)
				nodes()[index].position = moved[index];
		}
	}

	// the pipe model: a branch is as thick as all it carries (the area of its cross section is that of its children together)
	void set_radii()
	{
		constexpr float tip{0.0035f};
		auto const count{nodes().size()};
		std::vector<float> area(count, 0.f);
		for (std::size_t index = count; index-- > 0;)
		{
			auto &point{nodes()[index]};
			if (area[index] <= 0.f)
				area[index] = tip * tip;
			point.radius = std::sqrt(area[index]) * 1.003f;
			if (point.parent >= 0)
				area[point.parent] += square(point.radius);
		}
		auto widest{0.f};
		for (auto const &point : nodes())
			if (point.parent < 0)
				widest = std::max(widest, point.radius);
		// the twigs stay as thin as they are and the foot gets the radius of the species, what's between follows in proportion
		auto const target{std::max(m_plan.base_radius, 0.01f)};
		auto const scale{widest > tip ? (target - tip) / (widest - tip) : 1.f};
		for (auto &point : nodes())
			point.radius = std::max(tip + (point.radius - tip) * scale, 0.0025f);
	}

	// the branches: each point goes on with its thickest child, the others start branches of their own
	void make_chains()
	{
		auto &points{nodes()};
		auto const count{points.size()};
		std::vector<int> next(count, -1);
		for (std::size_t index = 0; index < count; ++index)
		{
			auto const parent{points[index].parent};
			if (parent < 0)
				continue;
			auto const current{next[parent]};
			if (current < 0 || points[index].radius > points[current].radius)
				next[parent] = static_cast<int>(index);
		}
		auto &chains{m_skeleton.chains};
		for (std::size_t index = 0; index < count; ++index)
		{
			auto &point{points[index]};
			if (point.parent >= 0 && next[point.parent] == static_cast<int>(index))
			{
				point.chain = points[point.parent].chain;
				point.order = points[point.parent].order;
				chains[point.chain].nodes.push_back(static_cast<int>(index));
				continue;
			}
			chain made;
			made.attached = point.parent;
			made.order = point.parent < 0 ? 0 : points[point.parent].order + 1;
			made.nodes.push_back(static_cast<int>(index));
			point.chain = static_cast<int>(chains.size());
			point.order = made.order;
			chains.push_back(std::move(made));
		}
	}

	// a tree in a poor state has whole branches dead
	void mark_dead()
	{
		if (m_tree.vitality >= 0.999f)
			return;
		auto random{m_random.fork(0xdeadU)};
		std::vector<bool> dead(m_skeleton.chains.size(), false);
		for (std::size_t index = 0; index < dead.size(); ++index)
			dead[index] = m_skeleton.chains[index].order == 1 && random.chance((1.f - m_tree.vitality) * 0.45f);
		for (auto &point : nodes())
			point.dead = dead[point.chain] || (point.parent >= 0 && nodes()[point.parent].dead);
	}

	// the bones: the trunk in pieces, then the thickest branches as long as there are bones left
	void make_bones()
	{
		auto &chains{m_skeleton.chains};
		auto &bones{m_skeleton.bones};
		std::vector<int> stems;
		std::vector<int> branches;
		for (std::size_t index = 0; index < chains.size(); ++index)
		{
			auto const &branch{chains[index]};
			if (branch.order == 0)
				stems.push_back(static_cast<int>(index));
			else if (branch.order <= 2 && branch.nodes.size() >= 3)
				branches.push_back(static_cast<int>(index));
		}
		std::ranges::stable_sort(branches, [this](int const Left, int const Right) { return radius_of(Left) > radius_of(Right); });
		// the trunk pieces: the bones of a stem are its own, in order, parents of each other
		std::vector<std::vector<int>> pieces(chains.size());
		auto const per_stem{stems.size() == 1 ? 3 : 2};
		for (auto const stem : stems)
		{
			for (int piece = 0; piece < per_stem && bones.size() < bones_most; ++piece)
			{
				pieces[stem].push_back(static_cast<int>(bones.size()));
				bones.emplace_back();
			}
		}
		for (auto const branch : branches)
		{
			if (bones.size() >= bones_most)
				break;
			chains[branch].bone = static_cast<int>(bones.size());
			bones.emplace_back();
		}
		assign_bones(pieces);
		shape_bones();
	}

	float radius_of(int const Chain) const
	{
		return m_skeleton.nodes[m_skeleton.chains[Chain].nodes.front()].radius;
	}

	void assign_bones(std::vector<std::vector<int>> const &Pieces)
	{
		auto &points{nodes()};
		auto const &chains{m_skeleton.chains};
		for (auto &point : points)
		{
			auto const &branch{chains[point.chain]};
			if (false == Pieces[point.chain].empty())
			{
				// which piece of the stem: by the place of the point in it
				auto const position{std::ranges::find(branch.nodes, static_cast<int>(&point - points.data())) - branch.nodes.begin()};
				auto const piece{static_cast<std::size_t>(position) * Pieces[point.chain].size() / branch.nodes.size()};
				point.bone = Pieces[point.chain][piece];
			}
			else if (branch.bone >= 0)
				point.bone = branch.bone;
			else
				point.bone = point.parent >= 0 ? points[point.parent].bone : 0;
		}
	}

	void shape_bones()
	{
		auto &bones{m_skeleton.bones};
		auto const &points{m_skeleton.nodes};
		std::vector<bool> started(bones.size(), false);
		for (auto const &point : points)
		{
			auto &bone{bones[point.bone]};
			if (false == started[point.bone])
			{
				started[point.bone] = true;
				auto const from{point.parent >= 0 ? point.parent : -1};
				bone.pivot = from >= 0 ? points[from].position : point.position;
				bone.parent = from >= 0 && points[from].bone != point.bone ? points[from].bone : -1;
				bone.radius = point.radius;
			}
			bone.tip = point.position;
		}
		auto const trunk{std::max(m_plan.base_radius, 0.01f)};
		for (auto &bone : bones)
			bone.stiffness = m_species.stiffness * square(bone.radius / trunk);
	}

	void measure()
	{
		glm::vec3 low{1e9f};
		glm::vec3 high{-1e9f};
		for (auto const &point : nodes())
		{
			low = glm::min(low, point.position - glm::vec3{point.radius});
			high = glm::max(high, point.position + glm::vec3{point.radius});
		}
		m_skeleton.minimum = nodes().empty() ? glm::vec3{0.f} : low;
		m_skeleton.maximum = nodes().empty() ? glm::vec3{0.f} : high;
	}

	species const &m_species;
	instance const &m_tree;
	plan m_plan;
	random m_random;
	skeleton m_skeleton;
	std::vector<float> m_reach; // the length of the way from the foot of the stem, of each point
	std::vector<bool> m_stem; // the point is a part of a stem
};

} // namespace

skeleton grow(species const &Species, instance const &Tree)
{
	return grower{Species, Tree}.grow();
}

} // namespace trees
