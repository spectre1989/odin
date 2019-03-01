#include "net.h"

#include "core.h"

#include <stdio.h>


namespace Net
{



bool32 init()
{
	WORD winsock_version = 0x202;
	WSADATA winsock_data;
	if (WSAStartup(winsock_version, &winsock_data))
	{
		log("[net] WSAStartup failed: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

IP_Endpoint ip_endpoint(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port)
{
	IP_Endpoint ip_endpoint = {};
	ip_endpoint.address = (a << 24) | (b << 16) | (c << 8) | d;
	ip_endpoint.port = port;
	return ip_endpoint;
}

bool32 ip_endpoint_equals(IP_Endpoint* a, IP_Endpoint* b)
{
	return a->address == b->address && a->port == b->port;
}

SOCKADDR_IN ip_endpoint_to_sockaddr_in(IP_Endpoint* ip_endpoint)
{
	SOCKADDR_IN sockaddr_in;
	sockaddr_in.sin_family = AF_INET;
	sockaddr_in.sin_addr.s_addr = htonl(ip_endpoint->address);
	sockaddr_in.sin_port = htons(ip_endpoint->port);
	return sockaddr_in;
}

void ip_endpoint_to_str(char* out_str, size_t out_str_size, IP_Endpoint* ip_endpoint)
{
	snprintf(out_str, out_str_size, "%d.%d.%d.%d:%hu",
			ip_endpoint->address >> 24,
			(ip_endpoint->address >> 16) & 0xFF,
			(ip_endpoint->address >> 8) & 0xFF,
			ip_endpoint->address & 0xFF,
			ip_endpoint->port);
}


#ifdef FAKE_LAG
// when using fake lag, put normal socket in this namespace so the socket with
// fake lag can use them while implementing the same interface
namespace Internal
{
#endif // #ifdef FAKE_LAG

static bool32 set_sock_opt(SOCKET sock, int opt, int val)
{
	int len = sizeof(int);
	if (setsockopt(sock, SOL_SOCKET, opt, (char*)&val, len) == SOCKET_ERROR)
	{
		return false;
	}

	int actual;
	if (getsockopt(sock, SOL_SOCKET, opt, (char*)&actual, &len) == SOCKET_ERROR)
	{
		return false;
	}

	return val == actual;
}

bool32 socket(Socket* out_socket)
{
	int address_family = AF_INET;
	int type = SOCK_DGRAM;
	int protocol = IPPROTO_UDP;
	SOCKET sock = ::socket(address_family, type, protocol);

	if (!set_sock_opt(sock, SO_RCVBUF, (int)megabytes(1)))
	{
		log("failed to set rcvbuf size");
	}
	if (!set_sock_opt(sock, SO_SNDBUF, (int)megabytes(1)))
	{
		log("failed to set sndbuf size");
	}

	if (sock == INVALID_SOCKET)
	{
		log("[net] socket() failed: %d\n", WSAGetLastError());
		return false;
	}

	// put socket in non-blocking mode
	u_long enabled = 1;
	int result = ioctlsocket(sock, FIONBIO, &enabled);
	if (result == SOCKET_ERROR)
	{
		log("[net] ioctlsocket() failed: %d\n", WSAGetLastError());
		return false;
	}

	*out_socket = {};
	out_socket->handle = sock;

	return true;
}

void socket_close(Socket* sock)
{
	int result = closesocket(sock->handle);
	assert(result != SOCKET_ERROR);
}

bool32 socket_bind(Socket* sock, IP_Endpoint* local_endpoint)
{
	SOCKADDR_IN local_address = ip_endpoint_to_sockaddr_in(local_endpoint);
	if (bind(sock->handle, (SOCKADDR*)&local_address, sizeof(local_address)) == SOCKET_ERROR)
	{
		log("[net] bind() failed: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool32 socket_send(Socket* sock, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint)
{
	SOCKADDR_IN server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_addr.S_un.S_addr = htonl(endpoint->address);
	server_address.sin_port = htons(endpoint->port);
	int server_address_size = sizeof(server_address);

	if (sendto(sock->handle, (const char*)packet, packet_size, 0, (SOCKADDR*)&server_address, server_address_size) == SOCKET_ERROR)
	{
		log("[net] sendto() failed: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool32 socket_receive(Socket* sock, uint8* buffer, uint32 buffer_size, uint32* out_packet_size, IP_Endpoint* out_from)
{
	int flags = 0;
	SOCKADDR_IN from;
	int from_size = sizeof(from);
	int bytes_received = recvfrom(sock->handle, (char*)buffer, buffer_size, flags, (SOCKADDR*)&from, &from_size);

	if (bytes_received == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK && error != WSAECONNRESET)
		{
			log("[net] recvfrom() returned SOCKET_ERROR, WSAGetLastError() %d\n", error);
		}
		
		return false;
	}

	*out_packet_size = bytes_received;

	*out_from = {};
	out_from->address = ntohl(from.sin_addr.S_un.S_addr);
	out_from->port = ntohs(from.sin_port);

	return true;
}

void socket_set_fake_lag_s(	Socket* /*sock*/, 
							float32 /*fake_lag_s*/, 
							Linear_Allocator* /*allocator*/)
{
}

#ifdef FAKE_LAG
} // namespace Internal


constexpr uint32 c_packet_buffer_capacity = 512;
constexpr uint32 c_packet_buffer_mask = c_packet_buffer_capacity - 1;

static LARGE_INTEGER clock_frequency;

static Packet_Buffer packet_buffer(Linear_Allocator* allocator)
{
	QueryPerformanceFrequency(&clock_frequency);

	Packet_Buffer packet_buffer = {};
	packet_buffer.index = 0;
	packet_buffer.size = 0;
	packet_buffer.packets =							linear_allocator_alloc(allocator, c_packet_buffer_capacity * c_packet_budget_per_tick);
	packet_buffer.packet_sizes =	(uint32*)		linear_allocator_alloc(allocator, sizeof(uint32) * c_packet_buffer_capacity);
	packet_buffer.endpoints =		(IP_Endpoint*)	linear_allocator_alloc(allocator, sizeof(IP_Endpoint) * c_packet_buffer_capacity);
	packet_buffer.times =			(LARGE_INTEGER*)linear_allocator_alloc(allocator, sizeof(LARGE_INTEGER) * c_packet_buffer_capacity);
	return packet_buffer;
}

static bool32 packet_buffer_is_full(Packet_Buffer* packet_buffer)
{
	return packet_buffer->size == c_packet_buffer_capacity;
}

static void packet_buffer_push(Packet_Buffer* packet_buffer, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint, float32 fake_lag_s)
{
	assert(!packet_buffer_is_full(packet_buffer));

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	LARGE_INTEGER then;
	then.QuadPart = now.QuadPart + (LONGLONG)(clock_frequency.QuadPart * fake_lag_s);

	uint32 index = packet_buffer->index;
	packet_buffer->times[index] = then;
	packet_buffer->packet_sizes[index] = packet_size;
	packet_buffer->endpoints[index] = *endpoint;

	uint8* dst_packet = &packet_buffer->packets[index * c_packet_budget_per_tick];
	memcpy(dst_packet, packet, packet_size);

	++packet_buffer->size;
	packet_buffer->index = (packet_buffer->index + 1) & c_packet_buffer_mask;
}

static void packet_buffer_force_pop(Packet_Buffer* packet_buffer, uint8** out_packet, uint32* out_packet_size, IP_Endpoint* out_endpoint)
{
	assert(packet_buffer->size);

	uint32 index = (packet_buffer->index - packet_buffer->size) & c_packet_buffer_mask;
	*out_packet = &packet_buffer->packets[index * c_packet_budget_per_tick]; 
	*out_packet_size = packet_buffer->packet_sizes[index];
	*out_endpoint = packet_buffer->endpoints[index];

	--packet_buffer->size;
}

static bool32 packet_buffer_pop(Packet_Buffer* packet_buffer, uint8** out_packet, uint32* out_packet_size, IP_Endpoint* out_endpoint)
{
	assert(packet_buffer->size);

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	uint32 index = (packet_buffer->index - packet_buffer->size) & c_packet_buffer_mask;
	if (packet_buffer->times[index].QuadPart <= now.QuadPart)
	{
		*out_packet = &packet_buffer->packets[index * c_packet_budget_per_tick]; 
		*out_packet_size = packet_buffer->packet_sizes[index];
		*out_endpoint = packet_buffer->endpoints[index];

		--packet_buffer->size;

		return true;
	}

	return false;
}

bool32 socket(Socket* sock)
{
	*sock = {};

	if (!Internal::socket(&sock->sock))
	{
		return false;
	}

	sock->send_buffer = {};
	sock->recv_buffer = {};

	return true;
}

void socket_close(Socket* sock)
{
	while (sock->send_buffer.size)
	{
		uint8* packet;
		uint32 packet_size;
		IP_Endpoint destination;
		packet_buffer_force_pop(&sock->send_buffer, &packet, &packet_size, &destination);
		Internal::socket_send(&sock->sock, packet, packet_size, &destination);
	}

	Internal::socket_close(&sock->sock);
}

bool32 socket_bind(Socket* sock, IP_Endpoint* local_endpoint)
{
	if (Internal::socket_bind(&sock->sock, local_endpoint))
	{
		sock->can_receive = 1;
		return true;
	}

	return false;
}

bool32 socket_send(Socket* sock, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint)
{
	if (packet_buffer_is_full(&sock->send_buffer))
	{
		log("packet send buffer is full");
		uint8* pending_packet;
		uint32 pending_packet_size;
		IP_Endpoint pending_packet_destination;
		packet_buffer_force_pop(&sock->send_buffer, &pending_packet, &pending_packet_size, &pending_packet_destination);
		Internal::socket_send(&sock->sock, pending_packet, pending_packet_size, &pending_packet_destination);
	}
	packet_buffer_push(&sock->send_buffer, packet, packet_size, endpoint, sock->fake_lag_s);

	return true;
}

bool32 socket_receive(Socket* sock, uint8* buffer, uint32 buffer_size, uint32* out_packet_size, IP_Endpoint* out_from)
{
	// when the calling code is checking for received packets each tick, 
	// use this to do a quick update of any packets that need sending
	uint32 packet_size;
	IP_Endpoint endpoint;
	uint8* packet;
	while (sock->send_buffer.size &&
			packet_buffer_pop(&sock->send_buffer, &packet, &packet_size, &endpoint))
	{
		bool32 success = Internal::socket_send(&sock->sock, packet, packet_size, &endpoint);
		assert(success);
		sock->can_receive = 1;
	}

	if (!sock->can_receive)
	{
		return false;
	}

	if (packet_buffer_is_full(&sock->recv_buffer))
	{
		log("packet recv buffer is full");
	}

	while (!packet_buffer_is_full(&sock->recv_buffer) &&
			Internal::socket_receive(&sock->sock, buffer, buffer_size, &packet_size, &endpoint))
	{
		packet_buffer_push(&sock->recv_buffer, buffer, packet_size, &endpoint, sock->fake_lag_s);
	}

	if (sock->recv_buffer.size &&
		packet_buffer_pop(&sock->recv_buffer, &packet, out_packet_size, out_from))
	{
		memcpy(buffer, packet, *out_packet_size);
		return true;
	}
	return false;
}

void socket_set_fake_lag_s(	Socket* sock, 
							float32 fake_lag_s, 
							Linear_Allocator* allocator)
{
	sock->fake_lag_s = fake_lag_s;
	sock->send_buffer = packet_buffer(allocator);
	sock->recv_buffer = packet_buffer(allocator);
}

#endif // #ifdef FAKE_LAG


} // namespace Net