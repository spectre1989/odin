#pragma once

#include "net.h"



struct Player_Input;
struct Player_Snapshot_State;
struct Player_Extra_State;



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
uint32	client_msg_input_write(uint8* buffer, uint32 slot, float32 dt, Player_Input* input, uint32 prediction_id);
void	client_msg_input_read(uint8* buffer, uint32* slot, float32* dt, Player_Input* input, uint32* prediction_id);


enum class Server_Message : uint8
{
	Join_Result,// tell client they're accepted/rejected
	State 		// tell client game state
};
uint32	server_msg_join_result_write(uint8* buffer, bool32 success, uint32 slot);
void	server_msg_join_result_read(uint8* buffer, bool32* out_success, uint32* out_slot);
uint32	server_msg_state_write(
	uint8* buffer, 
	uint32 prediction_id, 
	Player_Extra_State* local_player_extra_state,
	IP_Endpoint* player_endpoints,
	Player_Snapshot_State* player_snapshot_states,
	uint32 max_players);
void	server_msg_state_read(
	uint8* buffer,
	uint32* prediction_id, // most recent prediction id server has received for this player
	Player_Extra_State* local_player_extra_state,
	Player_Snapshot_State* player_snapshot_states, // snapshot state of all players
	bool32* players_present, // a 1 will be written to every slot actually used
	uint32 max_players); // max number of players the client can handle
	


} // namespace Net