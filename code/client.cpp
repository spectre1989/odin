#include "odin.h"

#include <stdio.h>
#include <winsock2.h>

void main()
{
	WORD winsock_version = 0x202;
	WSADATA winsock_data;
	if( WSAStartup( winsock_version, &winsock_data ) )
	{
		printf( "WSAStartup failed: %d", WSAGetLastError() );
		return;
	}

	int address_family = AF_INET;
	int type = SOCK_DGRAM;
	int protocol = IPPROTO_UDP;
	SOCKET sock = socket( address_family, type, protocol );

	if( sock == INVALID_SOCKET )
	{
		printf( "socket failed: %d", WSAGetLastError() );
		WSACleanup();
		return;
	}

	SOCKADDR_IN server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons( PORT );
	server_address.sin_addr.S_un.S_un_b.s_b1 = 127;
	server_address.sin_addr.S_un.S_un_b.s_b2 = 0;
	server_address.sin_addr.S_un.S_un_b.s_b3 = 0;
	server_address.sin_addr.S_un.S_un_b.s_b4 = 1;

	char message[SOCKET_BUFFER_SIZE];
	gets_s( message, SOCKET_BUFFER_SIZE );

	int message_length = int( strlen( message ) );
	int flags = 0;
	SOCKADDR* to = (SOCKADDR*)&server_address;
	int to_length = sizeof( server_address );
	if( sendto( sock, message, message_length, flags, to, to_length ) == SOCKET_ERROR )
	{
		printf( "sendto failed: %d", WSAGetLastError() );
		WSACleanup();
		return;
	}
	
	printf( "done" );

	WSACleanup();
}