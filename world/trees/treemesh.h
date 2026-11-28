/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the geometry of a tree made from its skeleton, at one level of detail: the bark of the branches as tubes, the leaves and the
// needles as real triangles with their own outline (there's no alpha anywhere), the flowers and the fruits. every vertex knows its
// bone for the wind; the triangles of the leaves are in the order of their rank, so the renderer drops the leaves of the autumn
// one by one by drawing fewer of them. engine-free

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "world/trees/treeskeleton.h"

namespace trees
{

enum class part
{
	bark,
	leaves, // and the needles
	flowers,
	fruits,
};

struct vertex
{
	glm::vec3 position{0.f}; // metres from the foot of the tree, y up
	glm::vec3 normal{0.f, 1.f, 0.f};
	glm::vec2 uv{0.f};
	glm::vec4 tangent{1.f, 0.f, 0.f, 1.f};
	std::uint8_t bone{0};
	float phase{0.f}; // 0..1, where the flutter of this leaf or twig is in its cycle
	float flex{0.f}; // 0..1, how much it flutters
	float rank{0.f}; // 0..1, the order in which the leaves come and go
};

struct mesh_part
{
	part kind{part::bark};
	bool two_sided{false}; // seen from both sides: leaves, petals
	std::vector<vertex> vertices;
	std::vector<std::uint32_t> indices; // triangles
};

struct tree_mesh
{
	std::vector<mesh_part> parts;
	int level{0}; // of detail

	std::size_t triangles() const;
	mesh_part const *find(part Kind) const;
};

// the levels of detail made so far: 0 everything, 1 the twigs left out, the leaves fewer and larger
int constexpr levels{2};

tree_mesh build_mesh(skeleton const &Skeleton, species const &Species, instance const &Tree, int Level);

} // namespace trees
