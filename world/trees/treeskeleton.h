/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the skeleton of one tree: its branches as a tree of points, the bones the wind moves, grown from the profile of its species and
// the few numbers a scenery keeps of it. engine-free; the same numbers give the same skeleton everywhere (treemath.h)

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "world/trees/treespecies.h"

namespace trees
{

// where the tree grew up, which shapes its crown
enum class form
{
	open, // alone: a low, wide crown
	edge, // at the edge of a wood
	forest, // among others: a high, narrow crown
};

// what a scenery keeps of one tree
struct instance
{
	std::uint32_t seed{0};
	float height{15.f}; // metres
	float vitality{1.f}; // 0..1: under 1 the crown thins and has dead branches
	form grown{form::open};
	bool pollarded{false}; // a willow (or a lime) cut back to the trunk again and again
	float trim_height{0.f}; // metres above the foot: nothing grows higher (a line over it); 0: no such limit
};

// a point of a branch; the branch runs to it from its parent
struct node
{
	glm::vec3 position{0.f}; // metres, from the foot of the tree, y up
	int parent{-1}; // -1: the foot of a stem
	float radius{0.f}; // metres
	int order{0}; // 0 the trunk, 1 its branches, 2 theirs...
	int chain{0}; // the branch it's a point of
	int bone{0};
	bool dead{false}; // no leaves
};

// a branch from its fork to its end: the points in order, the first being the point of the parent branch it starts from
// (absent for a stem, which starts at the ground)
struct chain
{
	std::vector<int> nodes;
	int attached{-1}; // the point of the parent branch, -1 for a stem
	int order{0};
	int bone{-1}; // its own bone, -1: it moves with the branch it grows from
};

// a part of the tree the wind moves as one: the trunk in a few pieces, the big branches
struct bone
{
	int parent{-1};
	glm::vec3 pivot{0.f}; // where it turns
	glm::vec3 tip{0.f};
	float radius{0.f}; // of the branch at the pivot
	float stiffness{1.f}; // relative, from the thickness and the species
};

struct skeleton
{
	std::vector<node> nodes; // a parent is always before its children
	std::vector<chain> chains;
	std::vector<bone> bones;
	glm::vec3 minimum{0.f}; // the box round the branches
	glm::vec3 maximum{0.f};
};

// the most bones of a tree, for the wind
int constexpr bones_most{48};

skeleton grow(species const &Species, instance const &Tree);

} // namespace trees
