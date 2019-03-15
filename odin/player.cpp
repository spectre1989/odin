#include "player.h"

#include <math.h>



constexpr float32 c_player_movement_speed = 10.0f;
constexpr float32 c_jumping_speed = 10.0f;


void tick_player(	Player_Snapshot_State* player_snapshot_state, 
					Player_Extra_State* player_extra_state, 
					float32 dt, 
					Player_Input* player_input)
{
	bool is_grounded = player_snapshot_state->position.z == 0.0f;

	Vec_3f velocity = vec_3f(0.0f, 0.0f, 0.0f);

	if (is_grounded)
	{
		float32 cos_yaw = cosf(player_input->yaw);
		float32 sin_yaw = sinf(player_input->yaw);

		Vec_3f right = vec_3f(cos_yaw, sin_yaw, 0.0f);
		Vec_3f forward = vec_3f(-sin_yaw, cos_yaw, 0.0f);

		velocity = vec_3f(0.0f, 0.0f, 0.0f);
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

		if (player_input->jump)
		{
			velocity.z += c_jumping_speed;
			is_grounded = false;
		}
	}
	
	player_snapshot_state->pitch = player_input->pitch;
	player_snapshot_state->yaw = player_input->yaw;

	if(!is_grounded)
	{
		// s = ut + 0.5at^2
		Vec_3f acceleration = vec_3f(0.0f, 0.0f, -9.81f);
		Vec_3f velocity_delta = vec_3f_mul(acceleration, dt);
		Vec_3f position_delta = vec_3f_add(vec_3f_mul(velocity, dt), vec_3f_mul(velocity_delta, 0.5f * dt));

		player_snapshot_state->position = vec_3f_add(player_snapshot_state->position, position_delta);
		player_extra_state->velocity = vec_3f_add(player_extra_state->velocity, velocity_delta);
	}
	else
	{
		player_snapshot_state->position = vec_3f_add(player_snapshot_state->position, vec_3f_mul(velocity, dt));
		player_extra_state->velocity = velocity;
	}
}