#include "common.cpp"
#include "common_net.cpp"


constexpr float32 c_client_timeout 	= 5.0f;



void main()
{
	if (!Net::init())
	{
		printf("Net::init failed\n");
		return;
	}

	Net::Socket sock;
	if (!Net::socket_create(&sock))
	{
		printf("socket_create failed\n");
		return;
	}

	Net::IP_Endpoint local_endpoint = {};
	local_endpoint.address = INADDR_ANY;
	local_endpoint.port = c_port;
	if (!Net::socket_bind(&sock, &local_endpoint))
	{
		printf("socket_bind failed");
		return;
	}

	Timing_Info timing_info = timing_info_create();

	uint8 buffer[c_socket_buffer_size];
	Net::IP_Endpoint client_endpoints[c_max_clients];
	float32 time_since_heard_from_clients[c_max_clients];
	Player_State client_objects[c_max_clients];
	Player_Input client_inputs[c_max_clients];

	for (uint16 i = 0; i < c_max_clients; ++i)
	{
		client_endpoints[i] = {};
	}

	bool32 is_running = 1;
	while (is_running)
	{
		LARGE_INTEGER tick_start_time;
		QueryPerformanceCounter(&tick_start_time);

		// read all available packets
		uint32 bytes_received;
		Net::IP_Endpoint from;
		while (Net::socket_receive(&sock, buffer, &bytes_received, &from))
		{
			switch (buffer[0])
			{
				case Client_Message::Join:
				{
					printf("Client_Message::Join from %u:%hu\n", from.address, from.port);

					uint16 slot = (uint16)-1;
					for (uint16 i = 0; i < c_max_clients; ++i)
					{
						if (client_endpoints[i].address == 0)
						{
							slot = i;
							break;
						}
					}

					buffer[0] = (int8)Server_Message::Join_Result;
					if (slot != (uint16)-1)
					{
						printf("client will be assigned to slot %hu\n", slot);
						buffer[1] = 1;
						memcpy(&buffer[2], &slot, 2);

						if (Net::socket_send(&sock, buffer, 4, &from))
						{
							client_endpoints[slot] = from;
							time_since_heard_from_clients[slot] = 0.0f;
							client_objects[slot] = {};
							client_inputs[slot] = {};
						}
						else
						{
							printf("sendto failed: %d\n", WSAGetLastError());
						}
					}
					else
					{
						printf("could not find a slot for player\n");
						buffer[1] = 0;

						if (!Net::socket_send(&sock, buffer, 2, &from))
						{
							printf("sendto failed: %d\n", WSAGetLastError());
						}
					}
				}
				break;

				case Client_Message::Leave:
				{
					uint16 slot;
					memcpy(&slot, &buffer[1], 2);

					if (Net::ip_endpoint_equal(&client_endpoints[slot], &from))
					{
						client_endpoints[slot] = {};
						printf("Client_Message::Leave from %hu(%u:%hu)\n", 
							slot, from.address, from.port);
					}
					else
					{
						printf("Client_Message::Leave from %hu(%u:%hu), espected (%u:%hu)", 
							slot, from.address, from.port, 
							client_endpoints[slot].address, client_endpoints[slot].port);
					}
				}
				break;

				case Client_Message::Input:
				{
					uint16 slot;
					memcpy(&slot, &buffer[1], 2);

					printf("%d %hu\n", bytes_received, slot);

					if (Net::ip_endpoint_equal(&client_endpoints[slot], &from))
					{
						uint8 input = buffer[3];

						client_inputs[slot].up = input & 0x1;
						client_inputs[slot].down = input & 0x2;
						client_inputs[slot].left = input & 0x4;
						client_inputs[slot].right = input & 0x8;

						time_since_heard_from_clients[slot] = 0.0f;

						printf("Client_Message::Input from %hu:%d\n", slot, (int32)input);
					}
					else
					{
						printf("Client_Message::Input discarded, was from %u:%hu but expected %u:%hu\n", from.address, from.port, client_endpoints[slot].address, client_endpoints[slot].port);
					}
				}
				break;
			}
		}
		
		// process input and update state
		for (uint16 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				tick_player(&client_objects[i], &client_inputs[i]);

				time_since_heard_from_clients[i] += c_seconds_per_tick;
				if (time_since_heard_from_clients[i] > c_client_timeout)
				{
					printf("client %hu timed out\n", i);
					client_endpoints[i] = {};
				}
			}
		}
		
		// create state packet
		buffer[0] = (int8)Server_Message::State;
		int32 bytes_written = 1;
		for (uint16 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				memcpy(&buffer[bytes_written], &i, sizeof(i));
				bytes_written += sizeof(i);

				memcpy(&buffer[bytes_written], &client_objects[i].x, sizeof(client_objects[i].x));
				bytes_written += sizeof(client_objects[i].x);

				memcpy(&buffer[bytes_written], &client_objects[i].y, sizeof(client_objects[i].y));
				bytes_written += sizeof(client_objects[i].y);

				memcpy(&buffer[bytes_written], &client_objects[i].facing, sizeof(client_objects[i].facing));
				bytes_written += sizeof(client_objects[i].facing);
			}
		}

		// send back to clients
		for (uint16 i = 0; i < c_max_clients; ++i)
		{
			if (client_endpoints[i].address)
			{
				if (!Net::socket_send(&sock, buffer, bytes_written, &client_endpoints[i]))
				{
					printf("sendto failed: %d\n", WSAGetLastError());
				}
			}
		}

		wait_for_tick_end(tick_start_time, &timing_info);
	}

	Net::socket_close(&sock);
}