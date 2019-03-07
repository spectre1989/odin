#include "server.h"

#include "core.h"
#include "net.h"
#include "net_msgs.h"
#include "player.h"



void server_main(std::atomic_bool* should_run)
{
	// todo(jbr) option to create a window and render on server

	Linear_Allocator allocator;
	linear_allocator_create(&allocator, megabytes(8));

	Net::Socket sock;
	if (!Net::socket(&sock))
	{
		log("[server] Net::socket() failed");
		return;
	}
	Net::socket_set_fake_lag_s(&sock, 0.0f, &allocator); // no fake lag on server

	Net::IP_Endpoint local_endpoint = {};
	local_endpoint.address = INADDR_ANY;
	local_endpoint.port = c_port;
	if (!Net::socket_bind(&sock, &local_endpoint))
	{
		log("[server] Net::socket_bind() failed");
		return;
	}

	constexpr uint32	c_socket_buffer_size	= c_packet_budget_per_tick;
	uint8*				socket_buffer			= linear_allocator_alloc(&allocator, c_socket_buffer_size);
	Net::IP_Endpoint*	client_endpoints		= (Net::IP_Endpoint*)linear_allocator_alloc(&allocator, sizeof(Net::IP_Endpoint) * c_max_clients);
	
	for (uint32 i = 0; i < c_max_clients; ++i)
	{
		client_endpoints[i] = {};
	}

	float32*				time_since_heard_from_clients	= (float32*)				linear_allocator_alloc(&allocator, sizeof(float32)					* c_max_clients);
	Player_Snapshot_State*	player_snapshot_states			= (Player_Snapshot_State*)	linear_allocator_alloc(&allocator, sizeof(Player_Snapshot_State)	* c_max_clients);
	Player_Extra_State*		player_extra_states				= (Player_Extra_State*)		linear_allocator_alloc(&allocator, sizeof(Player_Extra_State)		* c_max_clients);
	uint32*					player_prediction_ids			= (uint32*)					linear_allocator_alloc(&allocator, sizeof(uint32)					* c_max_clients);
	
	uint32 tick_number = 0;
	Timer tick_timer = timer();

	constexpr int32 c_tick_rate = 30;
	constexpr float32 c_seconds_per_tick = 1.0f / c_tick_rate;
	constexpr float32 c_client_timeout 	= 5.0f;

	while (should_run->load(std::memory_order_relaxed))
	{
		while (timer_get_s(&tick_timer) < c_seconds_per_tick) // todo(jbr) make this loop friendly to cloud hypervisors
		{
			// read all available packets
			uint32 bytes_received;
			Net::IP_Endpoint from;
			while (Net::socket_receive(&sock, socket_buffer, c_socket_buffer_size, &bytes_received, &from))
			{
				switch ((Net::Client_Message)socket_buffer[0])
				{
					case Net::Client_Message::Join:
					{
						char from_str[22];
						ip_endpoint_to_str(from_str, sizeof(from_str), &from);
						log("[server] Client_Message::Join from %s\n", from_str);

						uint32 slot = (uint32)-1;
						for (uint32 i = 0; i < c_max_clients; ++i)
						{
							if (client_endpoints[i].address == 0)
							{
								slot = i;
								break;
							}
						}

					
						if (slot != (uint32)-1)
						{
							log("[server] client will be assigned to slot %hu\n", slot);

							bool32 success = true;
							uint32 join_result_msg_size = Net::server_msg_join_result_write(socket_buffer, success, slot);
							if (Net::socket_send(&sock, socket_buffer, join_result_msg_size, &from))
							{
								client_endpoints[slot] = from;
								time_since_heard_from_clients[slot] = 0.0f;
								player_snapshot_states[slot] = {};
								player_extra_states[slot] = {};
								player_prediction_ids[slot] = 0;
							}
						}
						else
						{
							log("[server] could not find a slot for player\n");
						
							bool32 success = false;
							uint32 join_result_msg_size = Net::server_msg_join_result_write(socket_buffer, success, slot);
							Net::socket_send(&sock, socket_buffer, join_result_msg_size, &from);
						}
					}
					break;

					case Net::Client_Message::Leave:
					{
						uint32 slot;
						Net::client_msg_leave_read(socket_buffer, &slot);

						if (Net::ip_endpoint_equals(&client_endpoints[slot], &from))
						{
							client_endpoints[slot] = {};
							char from_str[22];
							ip_endpoint_to_str(from_str, sizeof(from_str), &from);
							log("[server] Client_Message::Leave from %hu(%s)\n", slot, from_str);
						}
						else
						{
							char from_str[22];
							char client_endpoint_str[22];
							ip_endpoint_to_str(from_str, sizeof(from_str), &from);
							ip_endpoint_to_str(client_endpoint_str, sizeof(client_endpoint_str), &client_endpoints[slot]);
							log("[server] Client_Message::Leave from %hu(%s), espected (%s)\n", 
								slot, from_str, client_endpoint_str);
						}
					}
					break;

					case Net::Client_Message::Input:
					{
						uint32 slot;
						float32 dt;
						Player_Input input;
						uint32 prediction_id;
						Net::client_msg_input_read(socket_buffer, &slot, &dt, &input, &prediction_id);

						if (Net::ip_endpoint_equals(&client_endpoints[slot], &from))
						{
							tick_player(&player_snapshot_states[slot], &player_extra_states[slot], dt, &input);
							
							player_prediction_ids[slot] = prediction_id; 
							time_since_heard_from_clients[slot] = 0.0f;
						}
						else
						{
							log("[server] Client_Message::Input discarded, was from %u:%hu but expected %u:%hu\n", from.address, from.port, client_endpoints[slot].address, client_endpoints[slot].port);
						}
					}
					break;
				}
			}
		}
		timer_shift_start(&tick_timer, c_seconds_per_tick);
		
		// update clients
		for (uint32 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				time_since_heard_from_clients[i] += c_seconds_per_tick;
				if (time_since_heard_from_clients[i] > c_client_timeout)
				{
					// todo(jbr) when receiving messages from a client whose slot doesn't match with
					// the endpoint we have, send a message back to them saying "go away"
					log("[server] client %hu timed out\n", i);
					client_endpoints[i] = {};
				}
			}
		}
		++tick_number;
		
		// create and send state packets
		for (uint32 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				uint32 state_msg_size = Net::server_msg_state_write(socket_buffer, player_prediction_ids[i], &player_extra_states[i], client_endpoints, player_snapshot_states, c_max_clients);

				if (!Net::socket_send(&sock, socket_buffer, state_msg_size, &client_endpoints[i]))
				{
					log("[server] sendto failed: %d\n", WSAGetLastError());
				}
			}
		}
	}

	Net::socket_close(&sock);
}