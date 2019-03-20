#include "net_msgs.h"

#include "player.h"



namespace Net
{



static void serialise_u8(uint8** buffer, uint8 u)
{
	**buffer = u;
	++(*buffer);
}

/*static void serialise_u16(uint8** buffer, uint16 u)
{
	memcpy(*buffer, &u, sizeof(u));
	*buffer += sizeof(u);
}*/

static void serialise_u32(uint8** buffer, uint32 u)
{
	memcpy(*buffer, &u, sizeof(u));
	*buffer += sizeof(u);
}

static void serialise_f32(uint8** buffer, float32 f)
{
	memcpy(*buffer, &f, sizeof(f));
	*buffer += sizeof(f);
}

static void serialise_vec_3f(uint8** buffer, Vec_3f v)
{
	serialise_f32(buffer, v.x);
	serialise_f32(buffer, v.y);
	serialise_f32(buffer, v.z);
}

static void serialise_input(uint8** buffer, Player_Input* input)
{
	// if buttons are non-zero they're not necessarily 1
	uint8 packed_buttons = 
		(uint8)(input->up	? 1 << 0 : 0) |
		(uint8)(input->down ? 1 << 1 : 0) |
		(uint8)(input->left ? 1 << 2 : 0) |
		(uint8)(input->right? 1 << 3 : 0) |
		(uint8)(input->jump ? 1 << 4 : 0);

	serialise_u8(buffer, packed_buttons);
	serialise_f32(buffer, input->pitch);
	serialise_f32(buffer, input->yaw);
}

static void serialise_player_snapshot_state(uint8** buffer, Player_Snapshot_State* player_snapshot_state)
{
	serialise_vec_3f(buffer, player_snapshot_state->position);
	serialise_f32(buffer, player_snapshot_state->pitch);
	serialise_f32(buffer, player_snapshot_state->yaw);
}

static void deserialise_u8(uint8** buffer, uint8* u)
{
	*u = **buffer;
	++(*buffer);
}

/*static void deserialise_u16(uint8** buffer, uint16* u)
{
	memcpy(u, *buffer, sizeof(*u));
	*buffer += sizeof(*u);
}*/

static void deserialise_u32(uint8** buffer, uint32* u)
{
	memcpy(u, *buffer, sizeof(*u));
	*buffer += sizeof(*u);
}

static void deserialise_f32(uint8** buffer, float32* f)
{
	memcpy(f, *buffer, sizeof(*f));
	*buffer += sizeof(*f);
}

static void deserialise_vec_3f(uint8** buffer, Vec_3f* v)
{
	deserialise_f32(buffer, &v->x);
	deserialise_f32(buffer, &v->y);
	deserialise_f32(buffer, &v->z);
}

static void deserialise_input(uint8** buffer, Player_Input* input)
{
	// if buttons are non-zero they're not necessarily 1
	uint8 packed_buttons;
	deserialise_u8(buffer, &packed_buttons);
	deserialise_f32(buffer, &input->pitch);
	deserialise_f32(buffer, &input->yaw);

	input->up		= packed_buttons & 1;
	input->down		= packed_buttons & (1 << 1);
	input->left		= packed_buttons & (1 << 2);
	input->right	= packed_buttons & (1 << 3);
	input->jump		= packed_buttons & (1 << 4);
}

static void deserialise_player_snapshot_state(uint8** buffer, Player_Snapshot_State* player_snapshot_state)
{
	deserialise_vec_3f(buffer, &player_snapshot_state->position);
	deserialise_f32(buffer, &player_snapshot_state->pitch);
	deserialise_f32(buffer, &player_snapshot_state->yaw);
}


uint32 client_msg_join_write(uint8* buffer)
{
	uint8* buffer_iter = buffer;
	
	serialise_u8(&buffer_iter, (uint8)Client_Message::Join);
	
	return (uint32)(buffer_iter - buffer);
}

uint32 client_msg_leave_write(uint8* buffer, uint32 slot)
{
	uint8* buffer_iter = buffer;
	
	serialise_u8(&buffer_iter, (uint8)Client_Message::Leave);
	serialise_u32(&buffer_iter, slot);

	return (uint32)(buffer_iter - buffer);
}
void client_msg_leave_read(uint8* buffer, uint32* out_slot)
{
	uint8* buffer_iter = buffer;

	uint8 message_type;
	deserialise_u8(&buffer_iter, &message_type);
	assert(message_type == (uint8)Client_Message::Leave);

	deserialise_u32(&buffer_iter, out_slot);
}

uint32 client_msg_input_write(uint8* buffer, uint32 slot, float32 dt, Player_Input* input, uint32 prediction_id)
{
	uint8* buffer_iter = buffer;
	
	serialise_u8(&buffer_iter, (uint8)Client_Message::Input);
	serialise_u32(&buffer_iter, slot);
	serialise_f32(&buffer_iter, dt);
	serialise_input(&buffer_iter, input);
	serialise_u32(&buffer_iter, prediction_id);

	return (uint32)(buffer_iter - buffer);
}
void client_msg_input_read(uint8* buffer, uint32* slot, float32* dt, Player_Input* input, uint32* prediction_id)
{
	uint8* buffer_iter = buffer;

	uint8 message_type;
	deserialise_u8(&buffer_iter, &message_type);
	assert(message_type == (uint8)Client_Message::Input);

	deserialise_u32(&buffer_iter, slot);
	deserialise_f32(&buffer_iter, dt);
	deserialise_input(&buffer_iter, input);
	deserialise_u32(&buffer_iter, prediction_id);
}



uint32 server_msg_join_result_write(uint8* buffer, bool32 success, uint32 slot)
{
	uint8* buffer_iter = buffer;

	serialise_u8(&buffer_iter, (uint8)Server_Message::Join_Result);
	serialise_u8(&buffer_iter, success ? 1 : 0);
	
	if (success)
	{
		serialise_u32(&buffer_iter, slot);
	}

	return (uint32)(buffer_iter - buffer);
}
void server_msg_join_result_read(uint8* buffer, bool32* out_success, uint32* out_slot)
{
	uint8* buffer_iter = buffer;

	uint8 message_type;
	deserialise_u8(&buffer_iter, &message_type);
	assert(message_type == (uint8)Server_Message::Join_Result);

	uint8 success;
	deserialise_u8(&buffer_iter, &success);
	*out_success = success;
	if (success)
	{
		deserialise_u32(&buffer_iter, out_slot);
	}
}

uint32 server_msg_state_write(
	uint8* buffer, 
	uint32 prediction_id, 
	Player_Extra_State* local_player_extra_state,
	IP_Endpoint* player_endpoints,
	Player_Snapshot_State* player_snapshot_states,
	uint32 max_players)
{
	uint8* buffer_iter = buffer;

	serialise_u8(&buffer_iter, (uint8)Server_Message::State);
	serialise_u32(&buffer_iter, prediction_id);
	serialise_vec_3f(&buffer_iter, local_player_extra_state->velocity);

	uint8* num_players_buffer_pos = buffer_iter; // written later
	serialise_u8(&buffer_iter, 0);

	uint8 num_players = 0;
	for (uint8 i = 0; i < max_players; ++i)
	{
		if (player_endpoints[i].address)
		{
			++num_players;

			serialise_u8(&buffer_iter, i);
			serialise_player_snapshot_state(&buffer_iter, &player_snapshot_states[i]);
		}
	}

	serialise_u8(&num_players_buffer_pos, num_players);

	return (uint32)(buffer_iter - buffer);
}
void server_msg_state_read(
	uint8* buffer,
	uint32* prediction_id, // most recent prediction id server has received for this player
	Player_Extra_State* local_player_extra_state,
	Player_Snapshot_State* player_snapshot_states, // snapshot state of all players
	bool32* players_present, // a 1 will be written to every slot actually used
	uint32 max_players) // max number of players the client can handle
{
	uint8* buffer_iter = buffer;

	uint8 message_type;
	deserialise_u8(&buffer_iter, &message_type);
	assert(message_type == (uint8)Server_Message::State);

	deserialise_u32(&buffer_iter, prediction_id);
	deserialise_vec_3f(&buffer_iter, &local_player_extra_state->velocity);

	bool32* players_present_end = &players_present[max_players];
	for (bool32* iter = players_present; iter != players_present_end; ++iter)
	{
		*iter = 0;
	}

	uint8 num_players;
	deserialise_u8(&buffer_iter, &num_players);
	for (uint8 i = 0; i < num_players; ++i)
	{
		uint8 slot;
		deserialise_u8(&buffer_iter, &slot);
		assert(slot < max_players);
		
		deserialise_player_snapshot_state(&buffer_iter, &player_snapshot_states[slot]);
		
		players_present[slot] = 1;
	}
}


} // namespace Net