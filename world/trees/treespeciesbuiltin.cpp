/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the species the game knows without files, written as their files would be. the numbers are tuned by eye against what grows in
// Poland; the botany they stand for (heights, shapes of crowns, the order of the year) is common knowledge, not anyone's work

#include "world/trees/treespecies.h"

#include <algorithm>
#include <array>
#include <utility>

namespace trees
{

namespace
{

constexpr std::string_view pine{R"tree(
id sosna_zwyczajna
name "Sosna zwyczajna"
growth colonisation
crown umbrella
leaves needle_tuft
height 14 30
crown_base 0.55
crown_width 0.4
trunk_radius 0.013
trunk_wobble 0.025
trunk_lean 0.025
attractors 3000
clumps 7
influence 12
kill 1.8
tropism_up 0.04
wander 0.28
twigs 5
twig_length 0.3
leaf_size 0.17
leaf_density 30
leaf_budget 11000
twig_radius 0.03
bark 135 85 60
leaf_summer 62 88 52
leaf_spring 70 98 55
leaf_autumn 62 88 52
evergreen 1
stiffness 1.1
)tree"};

constexpr std::string_view spruce{R"tree(
id swierk_pospolity
name "Świerk pospolity"
growth whorls
crown cone
leaves needle_brush
height 18 35
crown_base 0.08
crown_width 0.36
trunk_radius 0.012
trunk_wobble 0.012
trunk_lean 0.01
whorl_step 0.55
whorl_branches 5
branch_angle 78
branch_droop 0.32
branch_upturn 0.4
side_branches 9
twigs 2
twig_length 0.2
leaf_size 0.05
leaf_density 45
leaf_budget 14000
twig_radius 0.012
bark 95 70 60
leaf_summer 40 62 42
leaf_spring 70 105 55
leaf_autumn 40 62 42
evergreen 1
stiffness 1.0
)tree"};

constexpr std::string_view oak{R"tree(
id dab_szypulkowy
name "Dąb szypułkowy"
growth colonisation
crown round
leaves lobed
height 15 30
crown_base 0.22
crown_width 0.85
trunk_radius 0.028
trunk_wobble 0.05
trunk_lean 0.03
attractors 2800
influence 9
kill 1.6
tropism_up 0.04
droop 0.05
wander 0.38
twigs 3
twig_length 0.22
leaf_size 0.16
leaf_density 22
leaf_budget 7000
twig_radius 0.012
bark 82 76 68
leaf_summer 58 86 38
leaf_spring 120 150 60
leaf_autumn 140 95 45
fruit 120 105 45
fruit_size 0.022
keeps_dry_leaves 1
bud_day 120
full_leaf_day 145
fruit_day 250
colour_day 285
fall_day 315
stiffness 1.4
)tree"};

constexpr std::string_view birch{R"tree(
id brzoza_brodawkowata
name "Brzoza brodawkowata"
growth colonisation
crown ellipsoid
leaves oval
height 14 26
crown_base 0.32
crown_width 0.42
trunk_radius 0.015
trunk_wobble 0.03
trunk_lean 0.03
attractors 2600
influence 9
kill 1.6
tropism_up 0.22
droop 0.25
wander 0.18
twigs 3
twig_length 0.25
leaf_size 0.075
leaf_density 30
leaf_budget 12000
twig_radius 0.01
bark 222 218 208
leaf_summer 85 120 50
leaf_spring 130 170 70
leaf_autumn 215 175 50
bud_day 105
full_leaf_day 130
colour_day 265
fall_day 295
stiffness 0.8
hang 0.7
)tree"};

constexpr std::string_view mirabelle{R"tree(
id mirabelka
name "Śliwa wiśniowa (mirabelka)"
growth colonisation
crown round
leaves oval
height 3 8
stems 2 5
stem_spread 0.45
crown_base 0.1
crown_width 1.0
trunk_radius 0.018
trunk_wobble 0.07
trunk_lean 0.05
attractors 1300
influence 8
kill 1.5
tropism_up 0.1
droop 0.12
wander 0.32
twigs 3
twig_length 0.18
leaf_size 0.07
leaf_density 30
leaf_budget 11000
twig_radius 0.01
bark 72 58 52
leaf_summer 75 105 45
leaf_spring 120 160 70
leaf_autumn 175 140 55
flower 248 246 240
flower_size 0.022
fruit 215 175 45
fruit_size 0.024
bud_day 100
flower_day 98
full_leaf_day 125
fruit_day 200
colour_day 280
fall_day 305
stiffness 0.9
)tree"};

constexpr std::string_view weeping_willow{R"tree(
id wierzba_placzaca
name "Wierzba płacząca"
growth colonisation
crown weeping
leaves narrow
height 10 20
crown_base 0.3
crown_width 1.0
trunk_radius 0.03
trunk_wobble 0.05
trunk_lean 0.04
attractors 2000
influence 9
kill 1.6
tropism_up 0.2
droop 0.12
wander 0.15
twigs 1
twig_length 0.35
leaf_size 0.12
leaf_density 28
leaf_budget 15000
twig_radius 0.012
bark 100 90 72
leaf_summer 105 135 60
leaf_spring 150 180 80
leaf_autumn 195 180 70
bud_day 95
full_leaf_day 120
colour_day 290
fall_day 320
stiffness 0.6
hang 2.6
)tree"};

using builtin = std::pair<std::string_view, std::string_view>;
constexpr std::array builtins{builtin{"sosna_zwyczajna", pine},      builtin{"swierk_pospolity", spruce}, builtin{"dab_szypulkowy", oak},
                              builtin{"brzoza_brodawkowata", birch}, builtin{"mirabelka", mirabelle},     builtin{"wierzba_placzaca", weeping_willow}};

} // namespace

std::vector<std::string> builtin_species()
{
	std::vector<std::string> ids;
	for (auto const &entry : builtins)
		ids.emplace_back(entry.first);
	return ids;
}

std::string_view builtin_species_text(std::string_view const Id)
{
	auto const found{std::ranges::find(builtins, Id, &builtin::first)};
	return found == builtins.end() ? std::string_view{} : found->second;
}

} // namespace trees
