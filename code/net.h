#pragma once

#include "core.h"


namespace Net
{


bool init(Log_Function* p_log_function);


struct IP_Endpoint
{
	uint32 address;
	uint16 port;
};
IP_Endpoint ip_endpoint_create(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port);
bool		ip_endpoint_equals(IP_Endpoint* a, IP_Endpoint* b);
SOCKADDR_IN ip_endpoint_to_sockaddr_in(IP_Endpoint* ip_endpoint);


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

// just capping at 1 second for now
// todo(jbr) urgh, not this
constexpr uint32 c_packet_buffer_size = 60;

struct Packet_Buffer
{
	uint32 head;
	uint32 tail;
	LARGE_INTEGER times[c_packet_buffer_size];
	IP_Endpoint endpoints[c_packet_buffer_size];
	uint32 packet_sizes[c_packet_buffer_size];
	uint8 packets[c_packet_buffer_size][1024]; // todo(jbr) :(
};

// todo(jbr) custom allocator
// just allocate bytes and use like a circular buffer, rather than full 1024
// per packet
// jitter simulation
// packet loss simulation
// out of order delivery simulation
// duplication simulation
struct Socket
{
	Internal::Socket sock;
	float32 fake_lag_s;
	Packet_Buffer send_buffer;
	Packet_Buffer receive_buffer;
};

#endif // #ifdef FAKE_LAG

bool socket_create(Socket* out_socket);
void socket_close(Socket* sock);
bool socket_bind(Socket* sock, IP_Endpoint* local_endpoint);
bool socket_send(Socket* sock, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint);
bool socket_receive(Socket* sock, uint8* buffer, uint32 buffer_size, uint32* out_packet_size, IP_Endpoint* out_from);
void socket_set_fake_lag_s(Socket* sock, float32 fake_lag_s);


} // namespace Net