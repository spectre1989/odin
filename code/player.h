#pragma once

#include "core.h"



struct Player_Input
{
	bool32 up, down, left, right;
};

struct Player_Visual_State // todo(jbr) rename these two to something better, and preferably shorter, and also more true. At some point "visual state" might contain stuff like ammo or health. "networked" and "nonnetworked" is long and crap. Maybe "basic" and "detailed"/"additional"/"full"/"extra"
{
	float32 x, y, facing;
};

struct Player_Nonvisual_State
{
	float32 speed;
};

void tick_player(Player_Visual_State* player_visual_state, Player_Nonvisual_State* player_nonvisual_state, Player_Input* player_input);