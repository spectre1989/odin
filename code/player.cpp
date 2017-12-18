#include "player.h"

#include <math.h>


// todo(jbr) pass many players and inputs
void tick_player(Player_State* player_state, Player_Input* player_input)
{
	if (player_input->up)
	{
		player_state->speed += c_acceleration * c_seconds_per_tick;
		if (player_state->speed > c_max_speed)
		{
			player_state->speed = c_max_speed;
		}
	}
	if (player_input->down)
	{
		player_state->speed -= c_acceleration * c_seconds_per_tick;
		if (player_state->speed < 0.0f)
		{
			player_state->speed = 0.0f;
		}
	}
	if (player_input->left)
	{
		player_state->facing -= c_turn_speed * c_seconds_per_tick;
	}
	if (player_input->right)
	{
		player_state->facing += c_turn_speed * c_seconds_per_tick;
	}

	player_state->x += player_state->speed * c_seconds_per_tick * sinf(player_state->facing);
	player_state->y += player_state->speed * c_seconds_per_tick * cosf(player_state->facing);
}