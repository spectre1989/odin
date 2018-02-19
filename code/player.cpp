#include "player.h"

#include <math.h>


// todo(jbr) pass many players and inputs
void tick_player(Player_Visual_State* player_visual_state, Player_Nonvisual_State* player_nonvisual_state, Player_Input* player_input)
{
	if (player_input->up)
	{
		player_nonvisual_state->speed += c_acceleration * c_seconds_per_tick;
		if (player_nonvisual_state->speed > c_max_speed)
		{
			player_nonvisual_state->speed = c_max_speed;
		}
	}
	if (player_input->down)
	{
		player_nonvisual_state->speed -= c_acceleration * c_seconds_per_tick;
		if (player_nonvisual_state->speed < 0.0f)
		{
			player_nonvisual_state->speed = 0.0f;
		}
	}
	if (player_input->left)
	{
		player_visual_state->facing += c_turn_speed * c_seconds_per_tick;
	}
	if (player_input->right)
	{
		player_visual_state->facing -= c_turn_speed * c_seconds_per_tick;
	}

	player_visual_state->x += player_nonvisual_state->speed * c_seconds_per_tick * -sinf(player_visual_state->facing);
	player_visual_state->y += player_nonvisual_state->speed * c_seconds_per_tick * -cosf(player_visual_state->facing);
}