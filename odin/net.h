#pragma once

#include "core.h"


namespace Net
{


bool32 init();


struct IP_Endpoint
{
	uint32 address;
	uint16 port;
};
IP_Endpoint ip_endpoint(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port);
bool32		ip_endpoint_equals(IP_Endpoint* a, IP_Endpoint* b);
SOCKADDR_IN ip_endpoint_to_sockaddr_in(IP_Endpoint* ip_endpoint);
void		ip_endpoint_to_str(char* out_str, size_t out_str_size, IP_Endpoint* ip_endpoint);


#ifdef FAKE_LAG
// when using fake lag, put normal socket in this namespace so the socket with
// fake lag can use them while implementing the same interface
namespace Internal
{
#endif // #ifdef FAKE_LAG

	struct Socket
	{
		SOCKET handle;
	};

#ifdef FAKE_LAG
} // namespace Internal


struct Packet_Buffer
{
	uint32 index;
	uint32 size;
	uint8* packets;
	uint32* packet_sizes;
	IP_Endpoint* endpoints;
	LARGE_INTEGER* times;
};

// todo(jbr) jitter simulation
// packet loss simulation
// out of order delivery simulation
// duplication simulation
struct Socket
{
	Internal::Socket sock;
	float32 fake_lag_s;
	Packet_Buffer send_buffer;
	Packet_Buffer recv_buffer;
	bool32 can_receive;
};

#endif // #ifdef FAKE_LAG

bool32 socket(Socket* out_socket);
void socket_close(Socket* sock);
bool32 socket_bind(Socket* sock, IP_Endpoint* local_endpoint);
bool32 socket_send(Socket* sock, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint);
bool32 socket_receive(Socket* sock, uint8* buffer, uint32 buffer_size, uint32* out_packet_size, IP_Endpoint* out_from);
void socket_set_fake_lag_s(	Socket* sock, 
							float32 fake_lag_s, 
							Linear_Allocator* allocator);


} // namespace Net