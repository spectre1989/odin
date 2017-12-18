#include "net.h"

#include <stdio.h>


namespace Net
{



bool init()
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

IP_Endpoint ip_endpoint_create(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port)
{
	IP_Endpoint ip_endpoint = {};
	ip_endpoint.address = (a << 24) | (b << 16) | (c << 8) | d;
	ip_endpoint.port = port;
	return ip_endpoint;
}

bool ip_endpoint_equals(IP_Endpoint* a, IP_Endpoint* b)
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

bool socket_create(Socket* out_socket)
{
	int address_family = AF_INET;
	int type = SOCK_DGRAM;
	int protocol = IPPROTO_UDP;
	SOCKET sock = socket(address_family, type, protocol);

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

bool socket_bind(Socket* sock, IP_Endpoint* local_endpoint)
{
	SOCKADDR_IN local_address = ip_endpoint_to_sockaddr_in(local_endpoint);
	if (bind(sock->handle, (SOCKADDR*)&local_address, sizeof(local_address)) == SOCKET_ERROR)
	{
		log("[net] bind() failed: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool socket_send(Socket* sock, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint)
{
	SOCKADDR_IN server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_addr.S_un.S_addr = htonl(endpoint->address);
	server_address.sin_port = htons(endpoint->port);
	int server_address_size = sizeof(server_address);

	// todo(jbr) handle failure of sendto on non-blocking sockets
	if (sendto(sock->handle, (const char*)packet, packet_size, 0, (SOCKADDR*)&server_address, server_address_size) == SOCKET_ERROR)
	{
		log("[net] sendto() failed: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool socket_receive(Socket* sock, uint8* buffer, uint32 buffer_size, uint32* out_packet_size, IP_Endpoint* out_from)
{
	int flags = 0;
	SOCKADDR_IN from;
	int from_size = sizeof(from);
	int bytes_received = recvfrom(sock->handle, (char*)buffer, buffer_size, flags, (SOCKADDR*)&from, &from_size);

	if (bytes_received == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK)
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

void socket_set_fake_lag_s(Socket* /*sock*/, float32 /*fake_lag_s*/)
{
}

#ifdef FAKE_LAG
} // namespace Internal


static bool packet_buffer_push(Packet_Buffer* packet_buffer, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint, float32 fake_lag_s)
{
	if ((packet_buffer->tail - packet_buffer->head) == c_packet_buffer_size)
	{
		return false;
	}

	LARGE_INTEGER clock_frequency;
	QueryPerformanceFrequency(&clock_frequency);

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	LARGE_INTEGER then;
	then.QuadPart = now.QuadPart + (LONGLONG)(clock_frequency.QuadPart * fake_lag_s);

	uint32 tail = packet_buffer->tail % c_packet_buffer_size;
	packet_buffer->times[tail] = then;
	packet_buffer->packet_sizes[tail] = packet_size;
	packet_buffer->endpoints[tail] = *endpoint;
	memcpy(packet_buffer->packets[tail], packet, packet_size);

	++packet_buffer->tail;

	return true;
}

static bool packet_buffer_pop(Packet_Buffer* packet_buffer, uint8* out_packet, uint32* out_packet_size, IP_Endpoint* out_endpoint)
{
	if (packet_buffer->head == packet_buffer->tail)
	{
		return false;
	}

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	uint32 head = packet_buffer->head % c_packet_buffer_size;
	if (packet_buffer->times[head].QuadPart <= now.QuadPart)
	{
		memcpy(out_packet, 
			packet_buffer->packets[head], 
			packet_buffer->packet_sizes[head]);
		*out_packet_size = packet_buffer->packet_sizes[head];
		*out_endpoint = packet_buffer->endpoints[head];

		++packet_buffer->head;

		return true;
	}

	return false;
}

bool socket_create(Socket* sock)
{
	*sock = {};

	if (!Internal::socket_create(&sock->sock))
	{
		return false;
	}

	sock->send_buffer = {};
	sock->receive_buffer = {};

	return true;
}

void socket_close(Socket* sock)
{
	while (sock->send_buffer.head < sock->send_buffer.tail)
	{
		uint32 head = sock->send_buffer.head % c_packet_buffer_size;
		Internal::socket_send(&sock->sock, 
			sock->send_buffer.packets[head],
			sock->send_buffer.packet_sizes[head],
			&sock->send_buffer.endpoints[head]);
		++sock->send_buffer.head;
	}

	Internal::socket_close(&sock->sock);
}

void socket_update(Socket* sock, uint8* buffer, uint32 buffer_size)
{
	// todo(jbr) fake lag socket should probably have its own internal buffer for this rather than
	// stealing the one passed in to send/receive by caller
	uint32 packet_size;
	IP_Endpoint endpoint;
	while (packet_buffer_pop(&sock->send_buffer, buffer, &packet_size, &endpoint))
	{
		bool success = Internal::socket_send(&sock->sock, buffer, packet_size, &endpoint);
		assert(success);
	}

	// todo(jbr) don't call receive if we haven't sent anything yet
	while (Internal::socket_receive(&sock->sock, buffer, buffer_size, &packet_size, &endpoint))
	{
		bool success = packet_buffer_push(&sock->receive_buffer, buffer, packet_size, &endpoint, sock->fake_lag_s);
		assert(success);
	}
}

bool socket_send(Socket* sock, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint)
{
	bool success = packet_buffer_push(&sock->send_buffer, packet, packet_size, endpoint, sock->fake_lag_s);
	assert(success);

	socket_update(sock, packet, 1024); // todo(jbr) :(

	return true;
}

bool socket_receive(Socket* sock, uint8* buffer, uint32 buffer_size, uint32* out_packet_size, IP_Endpoint* out_from)
{
	socket_update(sock, buffer, buffer_size);

	return packet_buffer_pop(&sock->receive_buffer, buffer, out_packet_size, out_from);
}

void socket_set_fake_lag_s(Socket* sock, float32 fake_lag_s)
{
	sock->fake_lag_s = fake_lag_s;
}

#endif // #ifdef FAKE_LAG


} // namespace Net