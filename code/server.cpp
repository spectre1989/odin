#include "core.cpp"
#include "net.cpp"


constexpr float32 c_client_timeout 	= 5.0f;

static void log_v(const char* format, va_list args)
{
	vprintf(format, args);
}

static void log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	log_v(format, args);
	va_end(args);
}

void main()
{
	if (!Net::init(&log_v))
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

	uint8 buffer[c_socket_buffer_size];
	Net::IP_Endpoint client_endpoints[c_max_clients];
	float32 time_since_heard_from_clients[c_max_clients];
	Player_State client_objects[c_max_clients];
	Player_Input client_inputs[c_max_clients];
	Player_Input client_input_buffer_inputs[c_ticks_per_second][c_max_clients];
	uint32 client_input_buffer_test[c_ticks_per_second][c_max_clients];
	uint32 client_input_buffer_head = 0;
	uint32 client_timestamps[c_max_clients];
	uint32 tick_number = 0;
	Timer tick_timer = timer_create();

	for (uint32 i = 0; i < c_max_clients; ++i)
	{
		client_endpoints[i] = {};
	}
	for (uint32 i = 0; i < c_ticks_per_second; ++i)
	{
		for (uint32 j = 0; j < c_max_clients; ++j)
		{
			client_input_buffer_test[i][j] = 0;
		}
	}	

	bool32 is_running = 1;
	while (is_running)
	{
		timer_restart(&tick_timer);

		// read all available packets
		uint32 bytes_received;
		Net::IP_Endpoint from;
		while (Net::socket_receive(&sock, buffer, &bytes_received, &from))
		{
			switch (buffer[0])
			{
				case Net::Client_Message::Join:
				{
					log("[server] Client_Message::Join from %u:%hu\n", from.address, from.port);

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
						uint32 join_result_msg_size = Net::server_msg_join_result_write(buffer, success, slot);
						if (Net::socket_send(&sock, buffer, join_result_msg_size, &from))
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
						uint32 join_result_msg_size = Net::server_msg_join_result_write(buffer, success, slot);
						Net::socket_send(&sock, buffer, join_result_msg_size, &from);
					}
				}
				break;

				case Net::Client_Message::Leave:
				{
					uint32 slot;
					Net::client_msg_leave_read(buffer, &slot);

					if (Net::ip_endpoint_equal(&client_endpoints[slot], &from))
					{
						client_endpoints[slot] = {};
						log("[server] Client_Message::Leave from %hu(%u:%hu)\n", 
							slot, from.address, from.port);
					}
					else
					{
						log("[server] Client_Message::Leave from %hu(%u:%hu), espected (%u:%hu)\n", 
							slot, from.address, from.port, 
							client_endpoints[slot].address, client_endpoints[slot].port);
					}
				}
				break;

				case Net::Client_Message::Input:
				{
					uint32 slot;
					Player_Input input;
					uint32 timestamp;
					uint32 input_tick_number;
					Net::client_msg_input_read(buffer, &slot, &input, &timestamp, &input_tick_number);

					if (Net::ip_endpoint_equal(&client_endpoints[slot], &from))
					{
						if (input_tick_number >= tick_number)
						{
							uint32 offset = input_tick_number - tick_number;
							if (offset < c_ticks_per_second)
							{
								uint32 write_pos = (client_input_buffer_head + offset) % c_ticks_per_second;
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
		client_input_buffer_head = (client_input_buffer_head + 1) % c_ticks_per_second;

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
		for (uint32 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				uint32 target_player = i;
				uint32 state_msg_size = Net::server_msg_state_write(buffer, client_endpoints, client_objects, c_max_clients, tick_number, target_player, client_timestamps[target_player]);

				if (!Net::socket_send(&sock, buffer, state_msg_size, &client_endpoints[i]))
				{
					log("[server] sendto failed: %d\n", WSAGetLastError());
				}
			}
		}

		timer_wait_until(&tick_timer, c_seconds_per_tick);
	}

	Net::socket_close(&sock);
}