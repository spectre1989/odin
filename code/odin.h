#pragma once

#include "core.h"


constexpr uint16 	c_port = 9999;
constexpr uint32 	c_socket_buffer_size = 1024;
constexpr uint32	c_max_clients = 32;
constexpr uint32	c_ticks_per_second = 60;
constexpr float32	c_seconds_per_tick = 1.0f / (float32)c_ticks_per_second;
constexpr float32 	c_turn_speed = 1.0f;	// how fast player turns
constexpr float32 	c_acceleration = 20.0f;
constexpr float32 	c_max_speed = 50.0f;


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