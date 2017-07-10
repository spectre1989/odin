namespace Net
{


constexpr uint32 c_packet_buffer_size = c_ticks_per_second;
// todo(jbr) allocate these arrays based on how much fake latency is actually wanted
struct Packet_Buffer
{
	uint32 head;
	uint32 tail;
	LARGE_INTEGER times[c_packet_buffer_size];
	uint32 packet_sizes[c_packet_buffer_size];
	IP_Endpoint endpoints[c_packet_buffer_size];
	uint8 packets[c_packet_buffer_size][c_socket_buffer_size];
};

// todo(jbr) make this more sophisticated:
// jitter - collect real world data to help model this more accurately
// packet loss - same
// duplicated packets
// out of order delivery
// maybe move the socket buffer used for normal Socket, into the Socket
// struct, and then the Socket struct and implementation can just be ifdefed
// depending on whether fake lag is required for that build, then all code
// in client.cpp can just be the same
struct Fake_Socket
{
	Socket sock;
	float32 latency_s;
	Packet_Buffer send_buffer;
	Packet_Buffer receive_buffer;
};


static IP_Endpoint ip_endpoint_create(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port)
{
	IP_Endpoint ip_endpoint = {};
	ip_endpoint.address = (a << 24) | (b << 16) | (c << 8) | d;
	ip_endpoint.port = port;
	return ip_endpoint;
}

static bool packet_buffer_push(Packet_Buffer* packet_buffer, LARGE_INTEGER time, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint)
{
	if ((packet_buffer->tail - packet_buffer->head) == c_packet_buffer_size)
	{
		return false;
	}

	uint32 tail = packet_buffer->tail % c_packet_buffer_size;
	packet_buffer->times[tail] = time;
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

static bool fake_socket_create(float32 latency_s, Fake_Socket* out_fake_socket)
{
	*out_fake_socket = {};

	if (!socket_create(&out_fake_socket->sock))
	{
		return false;
	}

	out_fake_socket->latency_s = latency_s;

	return true;
}

static LARGE_INTEGER fake_socket_get_packet_time(Fake_Socket* fake_socket)
{
	LARGE_INTEGER clock_frequency;
	QueryPerformanceFrequency(&clock_frequency);

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	LARGE_INTEGER then;
	then.QuadPart = now.QuadPart + (LONGLONG)(clock_frequency.QuadPart * fake_socket->latency_s);
	return then;
}

static bool fake_socket_send(Fake_Socket* fake_socket, uint8* packet, uint32 packet_size, IP_Endpoint* endpoint)
{
	return packet_buffer_push(
		&fake_socket->send_buffer, 
		fake_socket_get_packet_time(fake_socket), 
		packet, 
		packet_size, 
		endpoint);
}

static bool fake_socket_receive(Fake_Socket* fake_socket, uint8* buffer, IP_Endpoint* out_from, uint32* out_bytes_received)
{
	return packet_buffer_pop(&fake_socket->receive_buffer, buffer, out_bytes_received, out_from);
}

static void fake_socket_update(Fake_Socket* fake_socket, uint8* buffer, uint32 buffer_size)
{
	uint32 packet_size;
	IP_Endpoint endpoint;

	while (packet_buffer_pop(&fake_socket->send_buffer, buffer, &packet_size, &endpoint))
	{
		bool success = socket_send(&fake_socket->sock, buffer, packet_size, &endpoint);
		assert(success);
	}

	while (socket_receive(&fake_socket->sock, buffer, buffer_size, &endpoint, &packet_size))
	{
		bool success = packet_buffer_push(
			&fake_socket->receive_buffer, 
			fake_socket_get_packet_time(fake_socket), 
			buffer, 
			packet_size, 
			&endpoint);
		assert(success);
	}
}


} // namespace Net