#include "player.h"

#include <math.h>



constexpr float32 c_player_movement_speed = 10.0f;
constexpr float32 c_player_movement_speed_sq = c_player_movement_speed * c_player_movement_speed;
constexpr float32 c_player_jumping_speed = 4.5f;
constexpr float32 c_player_air_control = 2.0f;


void tick_player(	Player_Snapshot_State* player_snapshot_state, 
					Player_Extra_State* player_extra_state, 
					float32 dt, 
					Player_Input* player_input)
{
	// get desired movement direction, based on wasd input
	float32 cos_yaw = cosf(player_input->yaw);
	float32 sin_yaw = sinf(player_input->yaw);

	Vec_3f right = vec_3f(cos_yaw, sin_yaw, 0.0f);
	Vec_3f forward = vec_3f(-sin_yaw, cos_yaw, 0.0f);

	Vec_3f desired_movement_direction = vec_3f(0.0f, 0.0f, 0.0f);
	if (player_input->up)
	{
		desired_movement_direction = vec_3f_add(desired_movement_direction, forward);
	}
	if (player_input->down)
	{
		desired_movement_direction = vec_3f_sub(desired_movement_direction, forward);
	}
	if (player_input->left)
	{
		desired_movement_direction = vec_3f_sub(desired_movement_direction, right);
	}
	if (player_input->right)
	{
		desired_movement_direction = vec_3f_add(desired_movement_direction, right);
	}
	desired_movement_direction = vec_3f_normalised(desired_movement_direction);
	
	Vec_3f velocity;
	bool is_grounded = player_snapshot_state->position.z == 0.0f;
	if (is_grounded)
	{
		// if player is grounded, change velocity immediately
		velocity = vec_3f_mul(desired_movement_direction, c_player_movement_speed);

		if (player_input->jump)
		{
			velocity.z += c_player_jumping_speed;
			is_grounded = false;
		}
	}
	else
	{
		// if player isn't grounded, carry previous velocity
		velocity = player_extra_state->velocity;
	}
	
	player_snapshot_state->pitch = player_input->pitch;
	player_snapshot_state->yaw = player_input->yaw;

	if(!is_grounded)
	{
		Vec_3f gravity = vec_3f(0.0f, 0.0f, -9.81f);
		Vec_3f air_control = vec_3f_mul(desired_movement_direction, c_player_movement_speed * c_player_air_control); // air control uses acceleration so that it's frame rate independent
		Vec_3f acceleration = vec_3f_add(gravity, air_control); 
		Vec_3f velocity_delta = vec_3f_mul(acceleration, dt);
		Vec_3f final_velocity = vec_3f_add(velocity, velocity_delta);

		// make sure air control isn't used to speed up xy movement
		Vec_3f final_velocity_xy = vec_3f(final_velocity.x, final_velocity.y, 0.0f);
		if (vec_3f_length_sq(final_velocity_xy) > c_player_movement_speed_sq)
		{
			final_velocity_xy = vec_3f_mul(vec_3f_normalised(final_velocity_xy), c_player_movement_speed);
			final_velocity.x = final_velocity_xy.x;
			final_velocity.y = final_velocity_xy.y;
		}
		
		// s = (u + v) * 0.5 * t;
		Vec_3f position_delta = vec_3f_mul(vec_3f_add(velocity, final_velocity), 0.5f * dt);

		player_snapshot_state->position = vec_3f_add(player_snapshot_state->position, position_delta);
		player_snapshot_state->position.z = f32_max(player_snapshot_state->position.z, 0.0f);

		player_extra_state->velocity = final_velocity;
	}
	else
	{
		player_snapshot_state->position = vec_3f_add(player_snapshot_state->position, vec_3f_mul(velocity, dt));
		player_extra_state->velocity = velocity;
	}
}