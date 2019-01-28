#include "player.h"

#include <math.h>


// todo(jbr) pass many players and inputs
void tick_player(Player_Snapshot_State* player_snapshot_state, Player_Extra_State* player_extra_state, Player_Input* player_input)
{
	if (player_input->up)
	{
		player_extra_state->speed += c_acceleration * c_seconds_per_tick;
		if (player_extra_state->speed > c_max_speed)
		{
			player_extra_state->speed = c_max_speed;
		}
	}
	if (player_input->down)
	{
		player_extra_state->speed -= c_acceleration * c_seconds_per_tick;
		if (player_extra_state->speed < 0.0f)
		{
			player_extra_state->speed = 0.0f;
		}
	}
	if (player_input->left)
	{
		player_snapshot_state->facing += c_turn_speed * c_seconds_per_tick;
	}
	if (player_input->right)
	{
		player_snapshot_state->facing -= c_turn_speed * c_seconds_per_tick;
	}

	player_snapshot_state->x += player_extra_state->speed * c_seconds_per_tick * -sinf(player_snapshot_state->facing);
	player_snapshot_state->y += player_extra_state->speed * c_seconds_per_tick * cosf(player_snapshot_state->facing);
}