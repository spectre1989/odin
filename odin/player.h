#pragma once

#include "core.h"
#include "maths.h"



struct Player_Input
{
	bool32 up, down, left, right, jump;
	float32 pitch;
	float32 yaw;
};

struct Player_Snapshot_State
{
	Vec_3f position;
	float32 pitch;
	float32 yaw;
};

struct Player_Extra_State
{
	Vec_3f velocity;
};

void tick_player(	Player_Snapshot_State* player_snapshot_state, 
					Player_Extra_State* player_extra_state, 
					float32 dt, 
					Player_Input* player_input);