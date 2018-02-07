#pragma once

#include "net.h"



struct Player_Input;
struct Player_Visual_State;
struct Player_Nonvisual_State;



namespace Net
{


enum class Client_Message : uint8
{
	Join,		// tell server we're new here
	Leave,		// tell server we're leaving
	Input 		// tell server our user input
};
uint32	client_msg_join_write(uint8* buffer);
uint32	client_msg_leave_write(uint8* buffer, uint32 slot);
void	client_msg_leave_read(uint8* buffer, uint32* out_slot);
uint32	client_msg_input_write(uint8* buffer, uint32 slot, Player_Input* input, uint32 timestamp, uint32 tick_number);
void	client_msg_input_read(uint8* buffer, uint32* slot, Player_Input* input, uint32* timestamp, uint32* tick_number);


enum class Server_Message : uint8
{
	Join_Result,// tell client they're accepted/rejected
	State 		// tell client game state
};
uint32	server_msg_join_result_write(uint8* buffer, bool32 success, uint32 slot);
void	server_msg_join_result_read(uint8* buffer, bool32* out_success, uint32* out_slot);
uint32	server_msg_state_write(
	uint8* buffer, 
	uint32 tick_number, 
	uint32 client_timestamp, 
	Player_Visual_State* local_player_visual_state,
	Player_Nonvisual_State* local_player_nonvisual_state,
	IP_Endpoint* player_endpoints,
	Player_Visual_State* player_visual_states,
	uint32 num_players);
void	server_msg_state_read(
	uint8* buffer,
	uint32* tick_number,
	uint32* client_timestamp, // most recent time stamp server had from client at the time of writing this packet
	Player_Visual_State* local_player_visual_state,
	Player_Nonvisual_State* local_player_nonvisual_state,
	Player_Visual_State* remote_player_visual_states,
	uint32* num_remote_players,
	uint32 max_players); // max number of remote players the client can handle
	


} // namespace Net