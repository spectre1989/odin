#include "odin.h"

#include <math.h>
#include <stdio.h>
#include <winsock2.h>
#include <windows.h> // windows.h must be included AFTER winsock2.h

const float32 	TURN_SPEED = 1.0f;	// how fast player turns
const float32 	ACCELERATION = 20.0f;
const float32 	MAX_SPEED = 50.0f;
const uint32	TICKS_PER_SECOND = 60;
const float32	SECONDS_PER_TICK = 1.0f / float32( TICKS_PER_SECOND );
const uint16	MAX_CLIENTS = 32;
const float32	CLIENT_TIMEOUT = 5.0f;

enum class Client_Message : uint8
{
	Join,		// tell server we're new here
	Leave,		// tell server we're leaving
	Input 		// tell server our user input
};

enum class Server_Message : uint8
{
	Join_Result,// tell client they're accepted/rejected
	State 		// tell client game state
};

struct Player_State
{
	float32 x, y, facing, speed;
};

struct Player_Input
{
	bool32 up, down, left, right;
};

static float32 time_since( LARGE_INTEGER t, LARGE_INTEGER frequency )
{
	LARGE_INTEGER now;
	QueryPerformanceCounter( &now );

	return float32( now.QuadPart - t.QuadPart ) / float32( frequency.QuadPart );
}

void main()
{
	WORD winsock_version = 0x202;
	WSADATA winsock_data;
	if( WSAStartup( winsock_version, &winsock_data ) )
	{
		printf( "WSAStartup failed: %d", WSAGetLastError() );
		return;
	}

	// todo( jbr ) make sure internal socket buffer is big enough
	int address_family = AF_INET;
	int type = SOCK_DGRAM;
	int protocol = IPPROTO_UDP;
	SOCKET sock = socket( address_family, type, protocol );

	if( sock == INVALID_SOCKET )
	{
		printf( "socket failed: %d", WSAGetLastError() );
		return;
	}

	SOCKADDR_IN local_address;
	local_address.sin_family = AF_INET;
	local_address.sin_port = htons( PORT );
	local_address.sin_addr.s_addr = INADDR_ANY;
	if( bind( sock, (SOCKADDR*)&local_address, sizeof( local_address ) ) == SOCKET_ERROR )
	{
		printf( "bind failed: %d", WSAGetLastError() );
		return;
	}

	// put socket in non-blocking mode
	u_long enabled = 1;
	ioctlsocket( sock, FIONBIO, &enabled );

	UINT sleep_granularity_ms = 1;
	bool32 sleep_granularity_was_set = timeBeginPeriod( sleep_granularity_ms ) == TIMERR_NOERROR;

	LARGE_INTEGER clock_frequency;
	QueryPerformanceFrequency( &clock_frequency );

	int8 buffer[SOCKET_BUFFER_SIZE];
	uint32 client_addresses[MAX_CLIENTS];
	float32 time_since_heard_from_clients[MAX_CLIENTS];
	Player_State client_objects[MAX_CLIENTS];
	Player_Input client_inputs[MAX_CLIENTS];

	for( uint16 i = 0; i < MAX_CLIENTS; ++i )
	{
		client_addresses[i] = 0;
	}

	bool32 is_running = 1;
	while( is_running )
	{
		LARGE_INTEGER tick_start_time;
		QueryPerformanceCounter( &tick_start_time );

		// read all available packets
		while( true )
		{
			int flags = 0;
			SOCKADDR_IN from;
			int from_size = sizeof( from );
			int bytes_received = recvfrom( sock, buffer, SOCKET_BUFFER_SIZE, flags, (SOCKADDR*)&from, &from_size );
			
			if( bytes_received == SOCKET_ERROR )
			{
				int error = WSAGetLastError();
				if( error != WSAEWOULDBLOCK )
				{
					printf( "recvfrom returned SOCKET_ERROR, WSAGetLastError() %d", error );
				}
				
				break;
			}

			uint32 from_ip = from.sin_addr.S_un.S_addr;

			switch( buffer[0] )
			{
				case Client_Message::Join:
				{
					printf( "Client_Message::Join from %u\n", from_ip );

					uint16 slot = uint16( -1 );
					for( uint16 i = 0; i < MAX_CLIENTS; ++i )
					{
						if( client_addresses[i] == 0 )
						{
							slot = i;
						}
					}

					buffer[0] = (int8)Server_Message::Join_Result;
					if( slot != uint16( -1 ) )
					{
						buffer[1] = 1;
						memcpy( &buffer[2], &slot, 2 );

						flags = 0;
						if( sendto( sock, buffer, 4, flags, (SOCKADDR*)&from, from_size ) != SOCKET_ERROR )
						{
							client_addresses[slot] = from_ip;
							time_since_heard_from_clients[slot] = 0.0f;
							client_objects[slot] = {};
							client_inputs[slot] = {};
						}
						else
						{
							printf( "sendto failed: %d", WSAGetLastError() );
						}
					}
					else
					{
						buffer[1] = 0;

						flags = 0;
						if( sendto( sock, buffer, 2, flags, (SOCKADDR*)&from, from_size ) == SOCKET_ERROR )
						{
							printf( "sendto failed: %d", WSAGetLastError() );
						}
					}
				}
				break;

				case Client_Message::Leave:
				{
					printf( "Client_Message::Leave from %u\n", from_ip );

					uint16 slot;
					memcpy( &slot, &buffer[1], 2 );

					if( client_addresses[slot] == from_ip )
					{
						client_addresses[slot] = 0;
					}
				}
				break;

				case Client_Message::Input:
					printf( "Client_Message::Input from %u\n", from_ip );

					uint16 slot;
					memcpy( &slot, &buffer[1], 2 );

					if( client_addresses[slot] == from_ip )
					{
						uint8 input = buffer[3];

						client_inputs[slot].up = input & 0x1;
						client_inputs[slot].down = input & 0x2;
						client_inputs[slot].left = input & 0x4;
						client_inputs[slot].right = input & 0x8;

						time_since_heard_from_clients[slot] = 0.0f;
					}
				break;
			}
		}
		
		// process input and update state
		for( uint16 i = 0; i < MAX_CLIENTS; ++i )
		{
			if( client_addresses[i] )
			{
				if( client_inputs[i].up )
				{
					client_objects[i].speed += ACCELERATION * SECONDS_PER_TICK;
					if( client_objects[i].speed > MAX_SPEED )
					{
						client_objects[i].speed = MAX_SPEED;
					}
				}
				if( client_inputs[i].down )
				{
					client_objects[i].speed -= ACCELERATION * SECONDS_PER_TICK;
					if( client_objects[i].speed < 0.0f )
					{
						client_objects[i].speed = 0.0f;
					}
				}
				if( client_inputs[i].left )
				{
					client_objects[i].facing -= TURN_SPEED * SECONDS_PER_TICK;
				}
				if( client_inputs[i].right )
				{
					client_objects[i].facing += TURN_SPEED * SECONDS_PER_TICK;
				}

				client_objects[i].x += client_objects[i].speed * SECONDS_PER_TICK * sinf( client_objects[i].facing );
				client_objects[i].y += client_objects[i].speed * SECONDS_PER_TICK * cosf( client_objects[i].facing );

				time_since_heard_from_clients[i] += SECONDS_PER_TICK;
				if( time_since_heard_from_clients[i] > CLIENT_TIMEOUT )
				{
					printf( "client %u timed out\n", client_addresses[i] );
					client_addresses[i] = 0;
				}
			}
		}
		
		// create state packet
		int32 bytes_written = 0;
		for( uint16 i = 0; i < MAX_CLIENTS; ++i )
		{
			if( client_addresses[i] )
			{
				memcpy( &buffer[bytes_written], &i, sizeof( i ) );
				bytes_written += sizeof( i );

				memcpy( &buffer[bytes_written], &client_objects[i].x, sizeof( client_objects[i].x ) );
				bytes_written += sizeof( client_objects[i].x );

				memcpy( &buffer[bytes_written], &client_objects[i].y, sizeof( client_objects[i].y ) );
				bytes_written += sizeof( client_objects[i].y );

				memcpy( &buffer[bytes_written], &client_objects[i].facing, sizeof( client_objects[i].facing ) );
				bytes_written += sizeof( client_objects[i].facing );
			}
		}

		// send back to clients
		int flags = 0;
		SOCKADDR_IN to;
		to.sin_family = AF_INET;
		to.sin_port = htons( PORT );
		int to_length = sizeof( to );

		for( uint16 i = 0; i < MAX_CLIENTS; ++i )
		{
			if( client_addresses[i] )
			{
				to.sin_addr.S_un.S_addr = client_addresses[i];

				if( sendto( sock, buffer, bytes_written, flags, (SOCKADDR*)&to, to_length ) == SOCKET_ERROR )
				{
					printf( "sendto failed: %d\n", WSAGetLastError() );
				}
			}
		}

		// wait until tick complete
		float32 time_taken_s = time_since( tick_start_time, clock_frequency );

		while( time_taken_s < SECONDS_PER_TICK )
		{
			if( sleep_granularity_was_set )
			{
				DWORD time_to_wait_ms = DWORD( ( SECONDS_PER_TICK - time_taken_s ) * 1000 );
				if( time_to_wait_ms > 0 )
				{
					Sleep( time_to_wait_ms );
				}
			}

			time_taken_s = time_since( tick_start_time, clock_frequency );
		}
	}
}