#include "odin.h"

#include <math.h>
#include <stdio.h>
#include <winsock2.h>

const float32 TURN_SPEED = 0.01f;	// how fast player turns
const float32 ACCELERATION = 0.01f;
const float32 MAX_SPEED = 1.0f;

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

	int8 buffer[SOCKET_BUFFER_SIZE];
	float32 player_x = 0.0f;
	float32 player_y = 0.0f;
	float32 player_facing = 0.0f;
	float32 player_speed = 0.0f;

	bool32 is_running = 1;
	while( is_running )
	{
		// get input packet from player
		int flags = 0;
		SOCKADDR_IN from;
		int from_size = sizeof( from );
		int bytes_received = recvfrom( sock, buffer, SOCKET_BUFFER_SIZE, flags, (SOCKADDR*)&from, &from_size );
		
		if( bytes_received == SOCKET_ERROR )
		{
			printf( "recvfrom returned SOCKET_ERROR, WSAGetLastError() %d", WSAGetLastError() );
			break;
		}
		
		// process input and update state
		int8 client_input = buffer[0];
		printf( "%d.%d.%d.%d:%d - %d\n", from.sin_addr.S_un.S_un_b.s_b1, from.sin_addr.S_un.S_un_b.s_b2, from.sin_addr.S_un.S_un_b.s_b3, from.sin_addr.S_un.S_un_b.s_b4, from.sin_port, client_input );

		if( client_input & 0x1 )	// forward
		{
			player_speed += ACCELERATION;
			if( player_speed > MAX_SPEED )
			{
				player_speed = MAX_SPEED;
			}
		}
		if( client_input & 0x2 )	// back
		{
			player_speed -= ACCELERATION;
			if( player_speed < 0.0f )
			{
				player_speed = 0.0f;
			}
		}
		if( client_input & 0x4 )	// left
		{
			player_facing -= TURN_SPEED;
		}
		if( client_input & 0x8 )	// right
		{
			player_facing += TURN_SPEED;
		}

		player_x += player_speed * cosf( player_facing );
		player_y += player_speed * sinf( player_facing );
		
		// create state packet
		int32 bytes_written = 0;
		memcpy( &buffer[bytes_written], &player_x, sizeof( player_x ) );
		bytes_written += sizeof( player_x );

		memcpy( &buffer[bytes_written], &player_y, sizeof( player_y ) );
		bytes_written += sizeof( player_y );

		memcpy( &buffer[bytes_written], &player_facing, sizeof( player_facing ) );
		bytes_written += sizeof( player_facing );

		// send back to client
		flags = 0;
		SOCKADDR* to = (SOCKADDR*)&from;
		int to_length = sizeof( from );
		if( sendto( sock, buffer, bytes_written, flags, to, to_length ) == SOCKET_ERROR )
		{
			printf( "sendto failed: %d", WSAGetLastError() );
			return;
		}
	}
}