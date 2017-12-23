// std
#include <stdio.h>
// odin
#include "core.h"
#include "net.h"
#include "net_msgs.h"
#include "player.h"



constexpr float32 c_client_timeout 	= 5.0f;



Globals* globals;



static void log_func(const char* format, va_list args)
{
	vprintf(format, args);
}

void main()
{
	globals_init(&log_func);

	if (!Net::init())
	{
		return;
	}

	Net::Socket sock;
	if (!Net::socket_create(&sock))
	{
		return;
	}

	Net::IP_Endpoint local_endpoint = {};
	local_endpoint.address = INADDR_ANY;
	local_endpoint.port = c_port;
	if (!Net::socket_bind(&sock, &local_endpoint))
	{
		return;
	}

	constexpr uint32 c_socket_buffer_size = c_packet_budget_per_tick;
	uint8* socket_buffer = alloc_permanent(c_socket_buffer_size);
	Net::IP_Endpoint* client_endpoints = (Net::IP_Endpoint*)alloc_permanent(sizeof(Net::IP_Endpoint) * c_max_clients);
	for (uint32 i = 0; i < c_max_clients; ++i)
	{
		client_endpoints[i] = {};
	}
	float32* time_since_heard_from_clients = (float32*)alloc_permanent(sizeof(float32) * c_max_clients);
	Player_State* client_objects = (Player_State*)alloc_permanent(sizeof(Player_State) * c_max_clients);
	Player_Input* client_inputs = (Player_Input*)alloc_permanent(sizeof(Player_Input) * c_max_clients);
	constexpr uint32 c_client_input_buffer_capacity = c_ticks_per_second;
	Player_Input** client_input_buffer_inputs = (Player_Input**)alloc_permanent(sizeof(Player_Input*) * c_client_input_buffer_capacity);
	uint32** client_input_buffer_test = (uint32**)alloc_permanent(sizeof(uint32*) * c_client_input_buffer_capacity);
	for (uint32 i = 0; i < c_client_input_buffer_capacity; ++i)
	{
		client_input_buffer_inputs[i] = (Player_Input*)alloc_permanent(sizeof(Player_Input) * c_max_clients);
		client_input_buffer_test[i] = (uint32*)alloc_permanent(sizeof(uint32) * c_max_clients);
		for (uint32 j = 0; j < c_max_clients; ++j)
		{
			client_input_buffer_test[i][j] = 0;
		}
	}
	uint32 client_input_buffer_head = 0;
	uint32* client_timestamps = (uint32*)alloc_permanent(sizeof(uint32) * c_max_clients);
	uint32 tick_number = 0;
	Timer tick_timer = timer_create();


	while (true)
	{
		timer_restart(&tick_timer);

		// read all available packets
		uint32 bytes_received;
		Net::IP_Endpoint from;
		while (Net::socket_receive(&sock, socket_buffer, c_socket_buffer_size, &bytes_received, &from))
		{
			switch (socket_buffer[0])
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
							client_objects[slot] = {};
							client_inputs[slot] = {};
							client_timestamps[slot] = 0;
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
					Player_Input input;
					uint32 timestamp;
					uint32 input_tick_number;
					Net::client_msg_input_read(socket_buffer, &slot, &input, &timestamp, &input_tick_number);

					if (Net::ip_endpoint_equals(&client_endpoints[slot], &from))
					{
						if (input_tick_number >= tick_number)
						{
							uint32 offset = input_tick_number - tick_number;
							if (offset < c_client_input_buffer_capacity)
							{
								uint32 write_pos = (client_input_buffer_head + offset) % c_client_input_buffer_capacity;
								client_input_buffer_inputs[write_pos][slot] = input;
								client_input_buffer_test[write_pos][slot] = 1;
							}
							else
							{
								log("[server] Input from %d ignored, too far ahead, it was for tick %d but we're on tick %d\n", slot, input_tick_number, tick_number);
							}
						}
						else
						{
							log("[server] Input from %d ignored, behind, it was for tick %d but we're on tick %d\n", slot, input_tick_number, tick_number);
						}
						
						client_timestamps[slot] = timestamp;
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

		// apply any buffered inputs from clients
		for (uint32 i = 0; i < c_max_clients; ++i)
		{
			Player_Input* a = &client_inputs[i];
			Player_Input* b = &client_input_buffer_inputs[client_input_buffer_head][i];
			uint32 t1 = client_input_buffer_test[client_input_buffer_head][i];
			uint32 t0 = 1 - t1;

			a->up = (a->up * t0) + (b->up * t1);
			a->down = (a->down * t0) + (b->down * t1);
			a->left = (a->left * t0) + (b->left * t1);
			a->right = (a->right * t0) + (b->right * t1);

			client_input_buffer_test[client_input_buffer_head][i] = 0; // clear for next time
		}
		client_input_buffer_head = (client_input_buffer_head + 1) % c_client_input_buffer_capacity;

		// update players
		for (uint32 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				tick_player(&client_objects[i], &client_inputs[i]);

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
		// todo(jbr) decouple tick rate from state packet rate
		for (uint32 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				uint32 target_player = i;
				uint32 state_msg_size = Net::server_msg_state_write(socket_buffer, client_endpoints, client_objects, c_max_clients, tick_number, target_player, client_timestamps[target_player]);

				if (!Net::socket_send(&sock, socket_buffer, state_msg_size, &client_endpoints[i]))
				{
					log("[server] sendto failed: %d\n", WSAGetLastError());
				}
			}
		}

		timer_wait_until(&tick_timer, c_seconds_per_tick);
	}

	Net::socket_close(&sock);
}