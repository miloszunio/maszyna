/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// a tree written as a text model (.t3d) of the game, to be looked at as it is: a static model, a time of the year chosen at writing,
// the levels of detail as submodels drawn at their distances. this is the way to see the generator before the renderer draws its
// trees itself; the model is plain, without the wind and the changes of the season. engine-free

#include <ostream>
#include <vector>

#include "world/trees/treemesh.h"

namespace trees
{

enum class season
{
	spring, // flowering, young leaves
	summer,
	autumn, // coloured, falling, fruit
	winter, // bare, but for the evergreens and the dry leaves of the oak
};

// how a time of the year looks on a species: what share of the leaves is on, the colours of the leaves by their rank
struct season_look
{
	float leaves{1.f}; // the share of the leaves on, 0..1
	glm::ivec3 first{0}; // the colour of the leaves of the lowest rank
	glm::ivec3 last{0}; // and of the highest
	bool flowers{false};
	bool fruits{false};
};

season_look look_of(species const &Species, season Season);

struct export_settings
{
	season time{season::summer};
	std::vector<float> distances{150.f, 600.f}; // metres, how far each level of detail is drawn
};

// the levels of detail of one tree in the text model format
void write_t3d(std::ostream &Out, std::vector<tree_mesh> const &Levels, species const &Species, export_settings const &Settings);

} // namespace trees
