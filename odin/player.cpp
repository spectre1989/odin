#include "player.h"

#include <math.h>



constexpr float32 c_player_movement_speed = 10.0f;


void tick_player(	Player_Snapshot_State* player_snapshot_state, 
					Player_Extra_State* player_extra_state, 
					float32 dt, 
					Player_Input* player_input)
{
	float32 cos_yaw = cosf(player_input->yaw);
	float32 sin_yaw = sinf(player_input->yaw);

	Vec_3f right = vec_3f(cos_yaw, sin_yaw, 0.0f);
	Vec_3f forward = vec_3f(-sin_yaw, cos_yaw, 0.0f);

	Vec_3f velocity = vec_3f(0.0f, 0.0f, 0.0f);
	if (player_input->up)
	{
		velocity = vec_3f_add(velocity, forward);
	}
	if (player_input->down)
	{
		velocity = vec_3f_sub(velocity, forward);
	}
	if (player_input->left)
	{
		velocity = vec_3f_sub(velocity, right);
	}
	if (player_input->right)
	{
		velocity = vec_3f_add(velocity, right);
	}
	velocity = vec_3f_mul(vec_3f_normalised(velocity), c_player_movement_speed);

	player_snapshot_state->position = vec_3f_add(player_snapshot_state->position, vec_3f_mul(velocity, dt));
	player_snapshot_state->pitch = player_input->pitch;
	player_snapshot_state->yaw = player_input->yaw;
	player_extra_state->velocity = velocity;
}