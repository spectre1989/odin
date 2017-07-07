namespace Net
{


constexpr uint32 c_packet_buffer_size = c_ticks_per_second;
// todo(jbr) allocate these arrays based on how much fake latency is actually wanted
struct Packet_Buffer
{
	uint32 head;
	uint32 tail;
	LARGE_INTEGER[c_packet_buffer_size] times;
	uint32[c_packet_buffer_size] packet_sizes;
	uint8[c_packet_buffer_size][c_socket_buffer_size] packets;
};

struct Fake_Socket
{
	Socket socket;
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

static bool packet_buffer_push(Packet_Buffer* packet_buffer, LARGE_INTEGER time, uint8* packet, uint32 packet_size)
{
	if ((packet_buffer->tail - packet_buffer->head) == c_packet_buffer_size)
	{
		return false;
	}

	packet_buffer->times[packet_buffer->tail] = time;
	packet_buffer->packet_sizes[packet_buffer->tail] = packet_size;
	memcpy(packet_buffer->packets[packet_buffer->tail], packet, packet_size);

	++packet_buffer->tail;
}

static bool packet_buffer_pop(Packet_Buffer* packet_buffer, uint8* out_packet, uint32* out_packet_size)
{
	if(packet_buffer->head == packet_buffer->tail)
	{
		return false;
	}

	LARGE_INTEGER now;
	QueryPerformanceFrequency(&now);

	if(packet_buffer->times[packet_buffer->head] <= now)
	{
		memcpy(out_packet, 
			packet_buffer->packets[packet_buffer->head], 
			packet_buffer->packet_sizes[packet_buffer->head]);
		*out_packet_size = packet_buffer->packet_sizes[packet_buffer->head];

		++packet_buffer->head;

		return true;
	}

	return false;
}

static bool fake_socket_create(float32 latency, FakeSocket* out_fake_socket)
{
	*out_fake_socket = {};

	if(!socket_create(&out_fake_socket->socket))
	{
		return false;
	}

	out_fake_socket->latency = latency;

	return true;
}

static void fake_socket_send(FakeSocket* fake_socket, uint8* packet, uint32 packet_size)
{
	LARGE_INTEGER clock_frequency;
	QueryPerformanceFrequency(&clock_frequency);

	LARGE_INTEGER now;
	QueryPerformanceFrequency(&now);

	bool success = packet_buffer_push(fake_socket->send_buffer, now + (clock_frequency * packet_buffer, packet, packet_size);
	assert(success);
}


} // namespace Net