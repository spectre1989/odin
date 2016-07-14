#include <stdio.h>
#include <winsock2.h>

void main()
{
	WORD winsock_version = ( 2 << 8 ) | 2;
	WSADATA winsock_data;
	if( WSAStartup( winsock_version, &winsock_data ) )
	{
		printf( "WSAStartup failed: %d", WSAGetLastError() );
		return;
	}

	

	
}