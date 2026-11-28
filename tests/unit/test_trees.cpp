// the tree generator (world/trees): the profiles of the species, the growth of the skeleton, the geometry and the text model

#include "doctest.h"

#include "world/trees/treeexport.h"
#include "world/trees/treemath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

namespace
{

trees::species species_named(std::string_view const Id)
{
	auto found{trees::builtin_species(Id)};
	REQUIRE(found.has_value());
	return *found;
}

trees::instance tree_of(trees::species const &Species, std::uint32_t const Seed, float const Share = 0.5f)
{
	trees::instance tree;
	tree.seed = Seed;
	tree.height = Species.height_min + (Species.height_max - Species.height_min) * Share;
	return tree;
}

// the positions of the points to a centimetre, folded into one number: what a different growth would change
std::uint64_t fingerprint(trees::skeleton const &Skeleton)
{
	auto hash{trees::hash_text("skeleton")};
	auto const fold{[&hash](std::int64_t const Value)
	                {
		                hash ^= static_cast<std::uint64_t>(Value);
		                hash *= 0x100000001b3ULL;
	                }};
	fold(static_cast<std::int64_t>(Skeleton.nodes.size()));
	for (auto const &point : Skeleton.nodes)
	{
		fold(point.parent);
		fold(std::llround(point.position.x * 100.f));
		fold(std::llround(point.position.y * 100.f));
		fold(std::llround(point.position.z * 100.f));
	}
	return hash;
}

bool finite(glm::vec3 const &Vector)
{
	return std::isfinite(Vector.x) && std::isfinite(Vector.y) && std::isfinite(Vector.z);
}

} // namespace

TEST_CASE("trees: every species the game knows reads, with a name and sensible numbers")
{
	auto const ids{trees::builtin_species()};
	CHECK(ids.size() >= 6);
	for (auto const &id : ids)
	{
		CAPTURE(id);
		std::string error;
		auto const species{trees::parse_species(trees::builtin_species_text(id), error)};
		REQUIRE_MESSAGE(species.has_value(), error);
		CHECK(species->id == id);
		CHECK(false == species->name.empty());
		CHECK(species->height_min > 0.f);
		CHECK(species->height_min <= species->height_max);
		CHECK(species->leaf_size > 0.f);
	}
}

TEST_CASE("trees: a fault in a profile is told with its line")
{
	std::string error;
	CHECK_FALSE(trees::parse_species("id test\nheight 5\n", error).has_value());
	CHECK(error.find("line 2") != std::string::npos);
	CHECK_FALSE(trees::parse_species("id test\nbranches 4\n", error).has_value());
	CHECK(error.find("unknown key") != std::string::npos);
	CHECK_FALSE(trees::parse_species("name \"Bez id\"\n", error).has_value());
	auto const read{trees::parse_species("# a comment\nid test # and another\nname \"Drzewo testowe\"\nheight 4 9\nleaf_summer 1 2 3\nevergreen 1\n", error)};
	REQUIRE(read.has_value());
	CHECK(read->name == "Drzewo testowe");
	CHECK(read->height_max == doctest::Approx(9.f));
	CHECK(read->leaf_summer == glm::ivec3{1, 2, 3});
	CHECK(read->evergreen);
}

TEST_CASE("trees: the same seed grows the same tree, another seed another one")
{
	auto const oak{species_named("dab_szypulkowy")};
	auto const first{trees::grow(oak, tree_of(oak, 7))};
	auto const again{trees::grow(oak, tree_of(oak, 7))};
	auto const other{trees::grow(oak, tree_of(oak, 8))};
	CHECK(fingerprint(first) == fingerprint(again));
	CHECK(fingerprint(first) != fingerprint(other));
	auto const mesh{trees::build_mesh(first, oak, tree_of(oak, 7), 0)};
	auto const mesh_again{trees::build_mesh(again, oak, tree_of(oak, 7), 0)};
	CHECK(mesh.triangles() == mesh_again.triangles());
}

TEST_CASE("trees: a seed gives the same tree on every compiler")
{
	// written down from a build with GCC 13; a build that grows another tree has arithmetic that differs (see treemath.h), and
	// the trees of the sceneries would not be the same for every player
	auto const birch{species_named("brzoza_brodawkowata")};
	auto const skeleton{trees::grow(birch, tree_of(birch, 2026))};
	CHECK(fingerprint(skeleton) == 0x3973b55d262a3c8cULL);
}

TEST_CASE("trees: every species grows as tall as asked, whole, with sound branches")
{
	for (auto const &id : trees::builtin_species())
	{
		CAPTURE(id);
		auto const species{species_named(id)};
		auto const tree{tree_of(species, 11)};
		auto const skeleton{trees::grow(species, tree)};
		REQUIRE(skeleton.nodes.size() > 50);
		CHECK(skeleton.maximum.y == doctest::Approx(tree.height).epsilon(0.15));
		for (std::size_t index = 0; index < skeleton.nodes.size(); ++index)
		{
			auto const &point{skeleton.nodes[index]};
			REQUIRE(point.parent < static_cast<int>(index));
			REQUIRE(finite(point.position));
			REQUIRE(point.radius > 0.f);
			REQUIRE(point.bone >= 0);
			REQUIRE(point.bone < static_cast<int>(skeleton.bones.size()));
		}
		// a branch is never thicker than the one it grows from
		for (auto const &point : skeleton.nodes)
			if (point.parent >= 0)
				REQUIRE(point.radius <= skeleton.nodes[point.parent].radius * 1.01f);
	}
}

TEST_CASE("trees: the bones are few and hang together")
{
	auto const willow{species_named("wierzba_placzaca")};
	auto const skeleton{trees::grow(willow, tree_of(willow, 3))};
	REQUIRE(false == skeleton.bones.empty());
	CHECK(skeleton.bones.size() <= trees::bones_most);
	CHECK(skeleton.bones.front().parent == -1);
	for (std::size_t index = 0; index < skeleton.bones.size(); ++index)
	{
		// up the parents to the trunk, without a loop
		auto bone{static_cast<int>(index)};
		auto steps{0};
		while (bone >= 0 && steps <= trees::bones_most)
		{
			bone = skeleton.bones[bone].parent;
			++steps;
		}
		CHECK(bone == -1);
		CHECK(skeleton.bones[index].stiffness > 0.f);
	}
}

TEST_CASE("trees: nothing grows above a line over the tree")
{
	auto const oak{species_named("dab_szypulkowy")};
	auto tree{tree_of(oak, 5)};
	tree.trim_height = 9.f;
	auto const skeleton{trees::grow(oak, tree)};
	for (auto const &point : skeleton.nodes)
		REQUIRE(point.position.y <= 9.f + 0.05f);
	CHECK(skeleton.maximum.y < 9.5f);
}

TEST_CASE("trees: a tree of the forest has its crown higher, a poor one has dead branches")
{
	auto const oak{species_named("dab_szypulkowy")};
	auto const lowest_leafy{[](trees::skeleton const &Skeleton)
	                        {
		                        auto lowest{1e9f};
		                        for (auto const &point : Skeleton.nodes)
			                        if (point.order >= 1)
				                        lowest = std::min(lowest, point.position.y);
		                        return lowest;
	                        }};
	auto open{tree_of(oak, 9)};
	auto forest{open};
	forest.grown = trees::form::forest;
	CHECK(lowest_leafy(trees::grow(oak, forest)) > lowest_leafy(trees::grow(oak, open)) + 1.f);
	CHECK(std::ranges::none_of(trees::grow(oak, open).nodes, &trees::node::dead));
	auto poor{open};
	poor.vitality = 0.3f;
	CHECK(std::ranges::any_of(trees::grow(oak, poor).nodes, &trees::node::dead));
}

TEST_CASE("trees: the geometry is sound, within its budget, and the leaves come in the order of their rank")
{
	for (auto const &id : trees::builtin_species())
	{
		CAPTURE(id);
		auto const species{species_named(id)};
		auto const tree{tree_of(species, 21, 1.f)};
		auto const skeleton{trees::grow(species, tree)};
		auto const near{trees::build_mesh(skeleton, species, tree, 0)};
		auto const far{trees::build_mesh(skeleton, species, tree, 1)};
		CHECK(near.triangles() < 260000);
		CHECK(far.triangles() < near.triangles());
		for (auto const &part : near.parts)
		{
			REQUIRE(part.indices.size() % 3 == 0);
			for (auto const index : part.indices)
				REQUIRE(index < part.vertices.size());
			for (auto const &point : part.vertices)
			{
				REQUIRE(finite(point.position));
				REQUIRE(glm::length(point.normal) == doctest::Approx(1.f).epsilon(0.01));
				REQUIRE(point.bone < skeleton.bones.size());
			}
		}
		auto const *leaves{near.find(trees::part::leaves)};
		REQUIRE(leaves != nullptr);
		auto const sorted{std::ranges::is_sorted(leaves->indices, {}, [leaves](std::uint32_t const Index) { return leaves->vertices[Index].rank; })};
		CHECK(sorted);
	}
}

TEST_CASE("trees: the triangles of the bark face out")
{
	auto const oak{species_named("dab_szypulkowy")};
	auto const tree{tree_of(oak, 4)};
	auto const mesh{trees::build_mesh(trees::grow(oak, tree), oak, tree, 0)};
	auto const *bark{mesh.find(trees::part::bark)};
	REQUIRE(bark != nullptr);
	std::size_t outward{0};
	std::size_t counted{0};
	for (std::size_t index = 0; index + 2 < bark->indices.size(); index += 3)
	{
		auto const &a{bark->vertices[bark->indices[index]]};
		auto const &b{bark->vertices[bark->indices[index + 1]]};
		auto const &c{bark->vertices[bark->indices[index + 2]]};
		auto const face{glm::cross(b.position - a.position, c.position - a.position)};
		if (glm::length(face) < 1e-9f)
			continue;
		++counted;
		if (glm::dot(face, a.normal + b.normal + c.normal) > 0.f)
			++outward;
	}
	CHECK(static_cast<double>(outward) >= 0.97 * static_cast<double>(counted));
}

TEST_CASE("trees: the text model has every level of detail and drops the leaves of the winter")
{
	auto const oak{species_named("dab_szypulkowy")};
	auto const birch{species_named("brzoza_brodawkowata")};
	auto const write{[](trees::species const &Species, trees::season const Time)
	                 {
		                 auto const tree{tree_of(Species, 1, 0.f)};
		                 auto const skeleton{trees::grow(Species, tree)};
		                 std::vector<trees::tree_mesh> const levels{trees::build_mesh(skeleton, Species, tree, 0), trees::build_mesh(skeleton, Species, tree, 1)};
		                 std::ostringstream out;
		                 trees::export_settings settings;
		                 settings.time = Time;
		                 trees::write_t3d(out, levels, Species, settings);
		                 return out.str();
	                 }};
	auto const summer{write(birch, trees::season::summer)};
	CHECK(summer.find("Name: lod0_bark") != std::string::npos);
	CHECK(summer.find("Name: lod1_bark") != std::string::npos);
	CHECK(summer.find("Name: lod0_leaves0") != std::string::npos);
	CHECK(summer.find("MinDistance: 150") != std::string::npos);
	auto const winter{write(birch, trees::season::winter)};
	CHECK(winter.find("leaves") == std::string::npos);
	// the oak keeps its dry leaves over the winter
	CHECK(write(oak, trees::season::winter).find("lod0_leaves0") != std::string::npos);
}
