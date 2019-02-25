#pragma once

#include "core.h"



struct Player_Input
{
	bool32 up, down, left, right;
};

struct Player_Snapshot_State
{
	float32 x, y, facing;
};

struct Player_Extra_State
{
	float32 speed;
};

void tick_player(Player_Snapshot_State* player_snapshot_state, Player_Extra_State* player_extra_state, float32 dt, Player_Input* player_input);