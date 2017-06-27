#include <windows.h>
#include <time.h>

#include "common.cpp"
#include "client_graphics.cpp"


struct Input
{
	bool32 up, down, left, right;
};

static bool32 g_is_running;
static Input g_input;


static void update_input(WPARAM keycode, bool32 value)
{
	switch (keycode)
	{
		case 'A':
			g_input.left = value;
		break;

		case 'D':
			g_input.right = value;
		break;

		case 'W':
			g_input.up = value;
		break;

		case 'S':
			g_input.down = value;
		break;
	}
}

LRESULT CALLBACK WindowProc( HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param )
{
	switch (message)
	{
		case WM_QUIT:
		case WM_DESTROY:
			g_is_running = 0;
		break;

		case WM_KEYDOWN:
			update_input(w_param, 1);
		break;

		case WM_KEYUP:
			update_input(w_param, 0);
		break;
	}

	return DefWindowProc( window_handle, message, w_param, l_param );
}

// todo(jbr) logging system
static void log_warning(const char* msg)
{
	OutputDebugStringA(msg);
}

static void log_warning(const char* fmt, int arg)
{
	char buffer[256];
	sprintf_s(buffer, sizeof(buffer), fmt, arg);
	OutputDebugStringA(buffer);
}

int CALLBACK WinMain( HINSTANCE instance, HINSTANCE /*prev_instance*/, LPSTR /*cmd_line*/, int cmd_show )
{
	WNDCLASS window_class;
	window_class.style = 0;
	window_class.lpfnWndProc = WindowProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = 0;
	window_class.hbrBackground = 0;
	window_class.lpszMenuName = 0;
	window_class.lpszClassName = "app_window_class";

	ATOM window_class_atom = RegisterClass( &window_class );

	assert( window_class_atom );


	uint32 window_width = 1280;
	uint32 window_height = 720;

	HWND window_handle;
	{
		LPCSTR 	window_name 	= "";
		DWORD 	style 			= WS_OVERLAPPED;
		int 	x 				= CW_USEDEFAULT;
		int 	y 				= 0;
		HWND 	parent_window 	= 0;
		HMENU 	menu 			= 0;
		LPVOID 	param 			= 0;

		window_handle 			= CreateWindow( window_class.lpszClassName, window_name, style, x, y, window_width, window_height, parent_window, menu, instance, param );

		assert( window_handle );
	}
	ShowWindow( window_handle, cmd_show );

	// init graphics
	constexpr uint32 c_num_vertices = 4 * c_max_clients;
	Graphics::Vertex vertices[c_num_vertices];

	srand((unsigned int)time(0));
	for (uint32 i = 0; i < c_max_clients; ++i)
	{
		// generate colour for client
		float32 r = 0.0f;
		float32 g = 0.0f;
		float32 b = 0.0f;

		float32 temp = (float32)(rand() % 100) / 100.0f;

		switch (rand() % 6)
		{
			case 0:
				r = 1.0f;
				g = temp;
			break;

			case 1:
				r = temp;
				g = 1.0f;
			break;

			case 2:
				g = 1.0f;
				b = temp;
			break;

			case 3:
				g = temp;
				b = 1.0f;
			break;

			case 4:
				r = 1.0f;
				b = temp;
			break;

			case 5:
				r = temp;
				b = 1.0f;
			break;
		}

		// assign colour to all 4 verts, and zero positions to begin with
		uint32 start_verts = i * 4;
		for (uint32 j = 0; j < 4; ++j)
		{
			vertices[start_verts + j].pos_x = 0.0f;
			vertices[start_verts + j].pos_y = 0.0f;
			vertices[start_verts + j].col_r = r;
			vertices[start_verts + j].col_g = g;
			vertices[start_verts + j].col_b = b;
		}
	}

	constexpr uint32 c_num_indices = 6 * c_max_clients;
	uint16 indices[c_num_indices];
	
	for(uint16 index = 0, vertex = 0; index < c_num_indices; index += 6, vertex += 4)
	{
		// quads will be top left, bottom left, bottom right, top right
		indices[index] = vertex;				// 0
		indices[index + 1] = vertex + 2;		// 2
		indices[index + 2] = vertex + 1;		// 1
		indices[index + 3] = vertex;			// 0
		indices[index + 4] = vertex + 3;		// 3
		indices[index + 5] = vertex + 2;		// 2
	}

	Graphics::State graphics_state;
	Graphics::init(window_handle, instance, window_width, window_height, c_num_vertices, indices, c_num_indices, &graphics_state);

	// init winsock
	WORD winsock_version = 0x202;
	WSADATA winsock_data;
	if (WSAStartup(winsock_version, &winsock_data))
	{
		log_warning("WSAStartup failed: %d\n", WSAGetLastError());
		return 0;
	}

	// todo( jbr ) make sure internal socket buffer is big enough
	int address_family = AF_INET;
	int type = SOCK_DGRAM;
	int protocol = IPPROTO_UDP;
	SOCKET sock = socket(address_family, type, protocol);

	if (sock == INVALID_SOCKET)
	{
		log_warning("socket failed: %d\n", WSAGetLastError());
		return 0;
	}

	// put socket in non-blocking mode
	u_long enabled = 1;
	ioctlsocket(sock, FIONBIO, &enabled);

	struct Player_State
	{
		float32 x, y, facing;
	};
	Player_State objects[c_max_clients];
	uint32 num_objects = 0;

	UINT sleep_granularity_ms = 1;
	bool32 sleep_granularity_was_set = timeBeginPeriod(sleep_granularity_ms) == TIMERR_NOERROR;

	LARGE_INTEGER clock_frequency;
	QueryPerformanceFrequency(&clock_frequency);

	uint8 buffer[c_socket_buffer_size];
	SOCKADDR_IN server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(c_port);
	server_address.sin_addr.S_un.S_un_b.s_b1 = 127;
	server_address.sin_addr.S_un.S_un_b.s_b2 = 0;
	server_address.sin_addr.S_un.S_un_b.s_b3 = 0;
	server_address.sin_addr.S_un.S_un_b.s_b4 = 1;
	int server_address_size = sizeof(server_address);

	buffer[0] = (uint8)Client_Message::Join;
	if (sendto(sock, (const char*)buffer, 1, 0, (SOCKADDR*)&server_address, server_address_size) == SOCKET_ERROR)
	{
		log_warning("sendto failed: %d\n", WSAGetLastError());
	}
	uint16 slot = 0xFFFF;


	// main loop
	g_is_running = 1;
	while( g_is_running )
	{
		LARGE_INTEGER tick_start_time;
		QueryPerformanceCounter(&tick_start_time);

		// Windows messages
		MSG message;
		UINT filter_min = 0;
		UINT filter_max = 0;
		UINT remove_message = PM_REMOVE;
		while( PeekMessage( &message, window_handle, filter_min, filter_max, remove_message ) )
		{
			TranslateMessage( &message );
			DispatchMessage( &message );
		}


		// Process Packets
		while (true)
		{
			int flags = 0;
			SOCKADDR_IN from;
			int from_size = sizeof(from);
			int bytes_received = recvfrom(sock, (char*)buffer, c_socket_buffer_size, flags, (SOCKADDR*)&from, &from_size);

			if (bytes_received == SOCKET_ERROR)
			{
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
				{
					log_warning("recvfrom returned SOCKET_ERROR, WSAGetLastError() %d\n", error);
				}
				
				break;
			}

			switch (buffer[0])
			{
				case Server_Message::Join_Result:
				{
					if(buffer[1])
					{
						memcpy(&slot, &buffer[2], sizeof(slot));
					}
					else
					{
						log_warning("server didn't let us in\n");
					}
				}
				break;

				case Server_Message::State:
				{
					num_objects = 0;
					int bytes_read = 1;
					while (bytes_read < bytes_received)
					{
						uint16 id; // unused
						memcpy(&id, &buffer[bytes_read], sizeof(id));
						bytes_read += sizeof(id);

						memcpy(&objects[num_objects].x, &buffer[bytes_read], sizeof(objects[num_objects].x));
						bytes_read += sizeof(objects[num_objects].x);

						memcpy(&objects[num_objects].y, &buffer[bytes_read], sizeof(objects[num_objects].y));
						bytes_read += sizeof(objects[num_objects].y);

						memcpy(&objects[num_objects].facing, &buffer[bytes_read], sizeof(objects[num_objects].facing));
						bytes_read += sizeof(objects[num_objects].facing);

						++num_objects;
					}
				}
				break;
			}
		}

		// Send input
		if (slot != 0xFFFF)
		{
			buffer[0] = (uint8)Client_Message::Input;
			int bytes_written = 1;

			memcpy(&buffer[bytes_written], &slot, sizeof(slot));
			bytes_written += sizeof(slot);

			uint8 input = 	(uint8)g_input.up | 
							((uint8)g_input.down << 1) | 
							((uint8)g_input.left << 2) | 
							((uint8)g_input.right << 3);
			buffer[bytes_written] = input;
			++bytes_written;

			if (sendto(sock, (const char*)buffer, bytes_written, 0, (SOCKADDR*)&server_address, server_address_size) == SOCKET_ERROR)
			{
				log_warning("sendto failed: %d\n", WSAGetLastError());
			}			
		}


		// Draw
		for (uint32 i = 0; i < num_objects; ++i)
		{
			constexpr float32 size = 0.05f;
			float32 x = objects[i].x * 0.01f;
			float32 y = objects[i].y * -0.01f;

			uint32 verts_start = i * 4;
			vertices[verts_start].pos_x = x - size; // TL (hdc y is +ve down screen)
			vertices[verts_start].pos_y = y - size;
			vertices[verts_start + 1].pos_x = x - size; // BL
			vertices[verts_start + 1].pos_y = y + size;
			vertices[verts_start + 2].pos_x = x + size; // BR
			vertices[verts_start + 2].pos_y = y + size;
			vertices[verts_start + 3].pos_x = x + size; // TR
			vertices[verts_start + 3].pos_y = y - size;
		}
		for (uint32 i = num_objects; i < c_max_clients; ++i)
		{
			uint32 verts_start = i * 4;
			for (uint32 j = 0; j < 4; ++j)
			{
				vertices[verts_start + j].pos_x = vertices[verts_start + j].pos_y = 0.0f;
			}
		}
		Graphics::update_and_draw(vertices, c_num_vertices, &graphics_state);

		// wait until tick complete
		float32 time_taken_s = time_since(tick_start_time, clock_frequency);

		while (time_taken_s < c_seconds_per_tick)
		{
			if (sleep_granularity_was_set)
			{
				DWORD time_to_wait_ms = (DWORD)((c_seconds_per_tick - time_taken_s) * 1000);
				if(time_to_wait_ms > 0)
				{
					Sleep(time_to_wait_ms);
				}
			}

			time_taken_s = time_since(tick_start_time, clock_frequency);
		}
	}

	// todo( jbr ) return wParam of WM_QUIT
	return 0;
}