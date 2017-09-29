#include "net_msgs.h"


namespace Net
{


uint32 client_msg_join_write(uint8* buffer)
{
	buffer[0] = (uint8)Client_Message::Join;
	return 1;
}

uint32 client_msg_leave_write(uint8* buffer, uint32 slot)
{
	buffer[0] = (uint8)Client_Message::Leave;
	memcpy(&buffer[1], &slot, 2);

	return 3;
}
void client_msg_leave_read(uint8* buffer, uint32* out_slot)
{
	assert(buffer[0] == (uint8)Client_Message::Leave);

	*out_slot = 0;
	memcpy(out_slot, &buffer[1], 2);
}

uint32 client_msg_input_write(uint8* buffer, uint32 slot, Player_Input* input, uint32 timestamp, uint32 tick_number)
{
	// if up/down/left/right are non-zero they're not necessarily 1
	uint8 packed_input = (uint8)(input->up ? 1 : 0) |
		(uint8)(input->down ? 1 << 1 : 0) |
		(uint8)(input->left ? 1 << 2 : 0) |
		(uint8)(input->right ? 1 << 3 : 0);

	buffer[0] = (uint8)Client_Message::Input;
	memcpy(&buffer[1], &slot, 2);
	buffer[3] = packed_input;
	memcpy(&buffer[4], &timestamp, 4);
	memcpy(&buffer[8], &tick_number, 4);

	return 12;
}
void client_msg_input_read(uint8* buffer, uint32* slot, Player_Input* input, uint32* timestamp, uint32* tick_number)
{
	assert(buffer[0] == (uint8)Client_Message::Input);

	*slot = 0;
	memcpy(slot, &buffer[1], 2);

	input->up = buffer[3] & 1;
	input->down = buffer[3] & 1 << 1;
	input->left = buffer[3] & 1 << 2;
	input->right = buffer[3] & 1 << 3;

	memcpy(timestamp, &buffer[4], 4);
	memcpy(tick_number, &buffer[8], 4);
}



uint32 server_msg_join_result_write(uint8* buffer, bool32 success, uint32 slot)
{
	buffer[0] = (uint8)Server_Message::Join_Result;
	buffer[1] = success ? 1 : 0;

	if (success)
	{
		memcpy(&buffer[2], &slot, 2);
		return 4;
	}

	return 2;
}
void server_msg_join_result_read(uint8* buffer, bool32* out_success, uint32* out_slot)
{
	assert(buffer[0] == (uint8)Server_Message::Join_Result);

	*out_success = buffer[1];
	if (*out_success)
	{
		*out_slot = 0;
		memcpy(out_slot, &buffer[2], 2);
	}
}

uint32 server_msg_state_write(uint8* buffer, IP_Endpoint* player_endpoints, Player_State* player_states, uint32 num_players, uint32 tick_number, uint32 target_player, uint32 target_player_client_timestamp)
{
	buffer[0] = (uint8)Server_Message::State;

	memcpy(&buffer[1], &tick_number, 4);

	memcpy(&buffer[5], &target_player_client_timestamp, 4);

	uint32 bytes_written = 9;
	assert(player_endpoints[target_player].address);
	memcpy(&buffer[bytes_written], &player_states[target_player].x, sizeof(player_states[target_player].x));
	bytes_written += sizeof(player_states[target_player].x);
	memcpy(&buffer[bytes_written], &player_states[target_player].y, sizeof(player_states[target_player].y));
	bytes_written += sizeof(player_states[target_player].y);
	memcpy(&buffer[bytes_written], &player_states[target_player].facing, sizeof(player_states[target_player].facing));
	bytes_written += sizeof(player_states[target_player].facing);
	memcpy(&buffer[bytes_written], &player_states[target_player].speed, sizeof(player_states[target_player].speed));
	bytes_written += sizeof(player_states[target_player].speed);

	uint8* num_additional_players_data = &buffer[bytes_written]; // done later
	bytes_written += 2;

	uint32 num_additional_players = 0;
	for (uint32 i = 0; i < num_players; ++i)
	{
		if (i != target_player && player_endpoints[i].address)
		{
			++num_additional_players;

			memcpy(&buffer[bytes_written], &player_states[i].x, sizeof(player_states[i].x));
			bytes_written += sizeof(player_states[i].x);
			memcpy(&buffer[bytes_written], &player_states[i].y, sizeof(player_states[i].y));
			bytes_written += sizeof(player_states[i].y);
			memcpy(&buffer[bytes_written], &player_states[i].facing, sizeof(player_states[i].facing));
			bytes_written += sizeof(player_states[i].facing);
		}
	}

	memcpy(num_additional_players_data, &num_additional_players, 2);

	return bytes_written;
}
void server_msg_state_read(
	uint8* buffer,
	uint32* tick_number,
	Player_State* player_state, // clients full player state
	uint32* client_timestamp, // most recent time stamp server had from client at the time of writing this packet
	Player_Visual_State* additional_player_states, // visual state of other players will be stored here
	uint32 num_additional_player_states, // max number of other players the client can handle
	uint32* num_additional_player_states_received) // number of additional players stored here
{
	assert(buffer[0] == (uint8)Server_Message::State);

	memcpy(tick_number, &buffer[1], 4);

	memcpy(client_timestamp, &buffer[5], 4);

	uint32 bytes_read = 9;
	memcpy(&player_state->x, &buffer[bytes_read], sizeof(player_state->x));
	bytes_read += sizeof(player_state->x);
	memcpy(&player_state->y, &buffer[bytes_read], sizeof(player_state->y));
	bytes_read += sizeof(player_state->y);
	memcpy(&player_state->facing, &buffer[bytes_read], sizeof(player_state->facing));
	bytes_read += sizeof(player_state->facing);
	memcpy(&player_state->speed, &buffer[bytes_read], sizeof(player_state->speed));
	bytes_read += sizeof(player_state->speed);

	uint32 num_additional_player_states_in_packet = 0;
	memcpy(&num_additional_player_states_in_packet, &buffer[bytes_read], 2);
	bytes_read += 2;

	uint32 i;
	for (i = 0; i < num_additional_player_states_in_packet && i < num_additional_player_states; ++i)
	{
		memcpy(&additional_player_states[i].x, &buffer[bytes_read], sizeof(additional_player_states[i].x));
		bytes_read += sizeof(additional_player_states[i].x);
		memcpy(&additional_player_states[i].y, &buffer[bytes_read], sizeof(additional_player_states[i].y));
		bytes_read += sizeof(additional_player_states[i].y);
		memcpy(&additional_player_states[i].facing, &buffer[bytes_read], sizeof(additional_player_states[i].facing));
		bytes_read += sizeof(additional_player_states[i].facing);
	}

	*num_additional_player_states_received = i;
}


} // namespace Net