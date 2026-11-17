/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "world/Road.h"

// what a change of the layout of a piece of a road means for the other pieces of that road
namespace road_layout
{

// a road is a string of pieces, and the pieces needn't be alike: an asphalt road goes on as a dirt one, a piece has a sidewalk
// the next one lacks. so a change made to a piece is carried over to another as the change it is, not as the whole layout of
// the piece it was made to: what the two differed in before, they differ in afterwards.
// Before, After: the changed piece, the way it was and the way it is now. Piece: another piece of the road.
// returns: what that piece is to be like. its axis, its widths at its ends, what's between its directions at its ends, the lanes
// it's short of at its ends and its rules of changing the lane are never taken from the changed piece
road_node::state carried_over(road_node::state const &Before, road_node::state const &After, road_node::state const &Piece);

} // namespace road_layout
