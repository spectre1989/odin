#pragma once

#include "core.h"



struct Player_Input
{
	bool32 up, down, left, right;
};

struct Player_State
{
	float32 x, y, facing, speed;
};

struct Player_Visual_State
{
	float32 x, y, facing;
};

void tick_player(Player_State* player_state, Player_Input* player_input);