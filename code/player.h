#pragma once

#include "core.h"



struct Player_Input
{
	bool32 up, down, left, right;
};

struct Player_Visual_State
{
	float32 x, y, facing;
};

struct Player_Nonvisual_State
{
	float32 speed;
};

void tick_player(Player_Visual_State* player_visual_state, Player_Nonvisual_State* player_nonvisual_state, Player_Input* player_input);