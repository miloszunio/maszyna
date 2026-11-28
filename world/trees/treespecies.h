/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the profile of a species of tree or shrub: the numbers its trees grow from, not a shape. read from a text file of lines
// "key value ...", '#' to the end of a line is a comment; the profiles the game knows without a file are written the same way
// (treespeciesbuiltin.cpp). engine-free, so it can be tested on its own

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace trees
{

// how the branches are made
enum class growth
{
	colonisation, // they grow towards the free space of the crown (the broadleaved trees, the pine, the shrubs)
	whorls, // tiers of branches round a straight trunk (the spruce, the fir, the larch)
};

// the shape of the space the crown fills
enum class crown
{
	ellipsoid,
	round,
	cone,
	column,
	umbrella,
	weeping,
};

// what grows on the twigs
enum class foliage
{
	oval, // birch, beech, the plums
	lobed, // oak
	palmate, // maple, sycamore
	heart, // lime
	pinnate, // ash, rowan, robinia, walnut
	narrow, // willows
	needle_tuft, // pines: needles in tufts at the ends of the shoots
	needle_brush, // spruce: needles all round the shoot
	needle_flat, // fir, yew: needles in a flat spray
};

struct species
{
	std::string id; // the name of the file, as a scenery names the species
	std::string name; // shown in the editor
	growth kind{growth::colonisation};
	crown shape{crown::ellipsoid};
	foliage leaves{foliage::oval};

	// the height of a grown tree, metres; a tree of a scenery can be anything from a sapling up
	float height_min{10.f};
	float height_max{20.f};
	int stems_min{1}; // shrubs and coppice have more than one
	int stems_max{1};
	float stem_spread{0.f}; // how far the stems lean out, 0..1
	float crown_base{0.3f}; // the part of the height under the crown, of a tree grown in the open
	float crown_width{0.6f}; // the width of the crown to the height
	float trunk_radius{0.02f}; // the radius at the foot to the height
	float trunk_wobble{0.03f}; // how much the trunk wanders, to the height
	float trunk_lean{0.02f}; // how much the whole trunk leans

	// colonisation
	float attractors{2500.f}; // points the crown grows towards, for a tree of height_max
	float influence{9.f}; // how far a branch feels them, segments
	float kill{1.7f}; // how near a branch reaches a point before it's taken, segments
	float tropism_up{0.12f}; // the pull upwards
	float droop{0.f}; // the pull down of the outer branches (the birch, the weeping willow)
	float wander{0.15f}; // the randomness of the direction (the gnarled oak)
	float clumps{0.f}; // the crown in so many separate masses of foliage, 0: one
	float twigs{2.f}; // short twigs at the ends of the branches
	float twig_length{0.25f}; // metres
	float hang{0.f}; // metres, the length of the shoots hanging from the ends of the branches

	// whorls
	float whorl_step{0.5f}; // metres between the tiers
	float whorl_branches{5.f};
	float branch_angle{75.f}; // degrees from the trunk
	float branch_droop{0.25f}; // how the branch sags
	float branch_upturn{0.3f}; // how its end turns up again
	float side_branches{6.f}; // along each branch

	// leaves or needles
	float leaf_size{0.08f}; // metres, the length of a leaf or a needle
	float leaf_density{25.f}; // per metre of a twig
	float leaf_budget{14000.f}; // the most the nearest tree carries; more are fewer and larger
	float twig_radius{0.012f}; // the leaves grow on what's thinner

	// colours 0..255
	glm::ivec3 bark{95, 85, 75};
	glm::ivec3 leaf_summer{70, 100, 45};
	glm::ivec3 leaf_spring{110, 150, 60};
	glm::ivec3 leaf_autumn{170, 120, 40};
	glm::ivec3 flower{240, 240, 235};
	glm::ivec3 fruit{160, 40, 40};
	float flower_size{0.f}; // metres, 0: no flowers worth drawing
	float fruit_size{0.f};

	// the year, days of it
	bool evergreen{false};
	bool keeps_dry_leaves{false}; // oak, young beech: brown leaves stay over the winter
	float bud_day{110.f};
	float flower_day{0.f}; // 0: no flowering worth showing
	float full_leaf_day{140.f};
	float fruit_day{0.f};
	float colour_day{275.f};
	float fall_day{300.f};

	float stiffness{1.f}; // to the wind; the aspen is less, the oak more
};

// reads a profile; on a fault Error says which line and why
std::optional<species> parse_species(std::string_view Text, std::string &Error);

// the profiles the game has without files
std::vector<std::string> builtin_species();
// one of them by its id, or nothing
std::optional<species> builtin_species(std::string_view Id);
// the text of one of them, as a file of it would read
std::string_view builtin_species_text(std::string_view Id);

} // namespace trees
