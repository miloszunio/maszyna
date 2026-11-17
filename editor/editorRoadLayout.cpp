/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorRoadLayout.hpp"

#include <cstddef>

namespace
{

// takes the new value if the value was changed
template <class Value_>
void carry(Value_ const &Before, Value_ const &After, Value_ &Target)
{
	if (false == (Before == After))
		Target = After;
}

// true if the two have lanes of the same directions, so a lane of one is the same lane of the other
bool same_lanes(road_node::state const &Left, road_node::state const &Right)
{
	return Left.forward == Right.forward && Left.backward == Right.backward && Left.lanes.size() == Right.lanes.size();
}

// the lanes: another set of them replaces the one the piece has, a change to the lanes which stay is made to the same lanes
void carry_lanes(road_node::state const &Before, road_node::state const &After, road_node::state &Target)
{
	if (false == same_lanes(Before, After))
	{
		Target.forward = After.forward;
		Target.backward = After.backward;
		Target.lanewidth = After.lanewidth;
		Target.velocity = After.velocity;
		Target.lanes = After.lanes;
		// the rules of changing the lane are the piece's own for as long as they're rules for the same lanes
		if (Target.changes.size() != After.changes.size())
			Target.changes = After.changes;
		return;
	}
	carry(Before.lanewidth, After.lanewidth, Target.lanewidth);
	carry(Before.velocity, After.velocity, Target.velocity);
	// a piece with other lanes than the changed one had keeps its own
	if (false == same_lanes(Before, Target))
		return;
	for (std::size_t lane = 0; lane < Target.lanes.size(); ++lane)
	{
		carry(Before.lanes[lane].width, After.lanes[lane].width, Target.lanes[lane].width);
		carry(Before.lanes[lane].velocity, After.lanes[lane].velocity, Target.lanes[lane].velocity);
	}
}

// what the road looks like
void carry_look(road_node::state const &Before, road_node::state const &After, road_node::state &Target)
{
	carry(Before.surface, After.surface, Target.surface);
	carry(Before.texturelength, After.texturelength, Target.texturelength);
	carry(Before.markings, After.markings, Target.markings);
	for (std::size_t side = 0; side < Target.sides.size(); ++side)
	{
		carry(Before.sides[side].type, After.sides[side].type, Target.sides[side].type);
		carry(Before.sides[side].width, After.sides[side].width, Target.sides[side].width);
		carry(Before.sides[side].material, After.sides[side].material, Target.sides[side].material);
		carry(Before.kerbs[side], After.kerbs[side], Target.kerbs[side]);
	}
	carry(Before.kerbheight, After.kerbheight, Target.kerbheight);
	carry(Before.kerbwidth, After.kerbwidth, Target.kerbwidth);
	carry(Before.kerbmaterial, After.kerbmaterial, Target.kerbmaterial);
	carry(Before.slope, After.slope, Target.slope);
	// the banks led to the ground differ from piece to piece, so they go along only when it's them which were changed
	carry(Before.banks, After.banks, Target.banks);
	carry(Before.bankmaterial, After.bankmaterial, Target.bankmaterial);
	// how far apart the directions are led, and how the ends of what's between them are rounded, is set for each piece on its own
	carry(Before.median.type, After.median.type, Target.median.type);
	carry(Before.median.material, After.median.material, Target.median.material);
}

// what the lanes are told
void carry_traffic(road_node::state const &Before, road_node::state const &After, road_node::state &Target)
{
	carry(Before.friction, After.friction, Target.friction);
	carry(Before.sounddistance, After.sounddistance, Target.sounddistance);
	carry(Before.quality, After.quality, Target.quality);
	carry(Before.environment, After.environment, Target.environment);
	carry(Before.weight, After.weight, Target.weight);
	carry(Before.roundabout, After.roundabout, Target.roundabout);
}

} // namespace

road_node::state road_layout::carried_over(road_node::state const &Before, road_node::state const &After, road_node::state const &Piece)
{
	auto result{Piece};
	carry_lanes(Before, After, result);
	carry_look(Before, After, result);
	carry_traffic(Before, After, result);
	return result;
}
