#include <time.h>

#include "core.cpp"
#define FAKE_LAG
#include "net.cpp"
#include "graphics.cpp"



static bool32 g_is_running;
static Player_Input g_input;


static void log_v(const char* format, va_list args)
{
	char buffer[512];
	vsnprintf(buffer, 512, format, args);
	OutputDebugStringA(buffer);
}

static void log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	log_v(format, args);
	va_end(args);
}

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


	constexpr uint32 c_window_width = 1280;
	constexpr uint32 c_window_height = 720;

	HWND window_handle;
	{
		LPCSTR 	window_name 	= "";
		DWORD 	style 			= WS_OVERLAPPED;
		int 	x 				= CW_USEDEFAULT;
		int 	y 				= 0;
		HWND 	parent_window 	= 0;
		HMENU 	menu 			= 0;
		LPVOID 	param 			= 0;

		window_handle 			= CreateWindow( window_class.lpszClassName, window_name, style, x, y, c_window_width, c_window_height, parent_window, menu, instance, param );

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
		float32 rgb[3] = {0.0f, 0.0f, 0.0f};

		// for a fully saturated colour, need 1 component at 1, and a second
		// component at <= 1, and the third must be zero
		int32 component = rand() % 3;
		rgb[component] = 1.0f;

		component += rand() % 2 ? 1 : -1;
		if(component < 0) component += 3;
		else if(component > 2) component -= 3;
		rgb[component] = (float32)(rand() % 100) / 100.0f;

		// assign colour to all 4 verts, and zero positions to begin with
		uint32 start_verts = i * 4;
		for (uint32 j = 0; j < 4; ++j)
		{
			vertices[start_verts + j].pos_x = 0.0f;
			vertices[start_verts + j].pos_y = 0.0f;
			vertices[start_verts + j].col_r = rgb[0];
			vertices[start_verts + j].col_g = rgb[1];
			vertices[start_verts + j].col_b = rgb[2];
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
	Graphics::init(window_handle, instance, c_window_width, c_window_height, c_num_vertices, indices, c_num_indices, &log_v, &graphics_state);

	if (!Net::init(&log_v))
	{
		return 0;
	}
	Net::Socket sock;
	if (!Net::socket_create(&sock))
	{
		return 0;
	}

	uint8 buffer[c_socket_buffer_size];
	Net::IP_Endpoint server_endpoint = Net::ip_endpoint_create(127, 0, 0, 1, c_port);

	uint32 join_msg_size = Net::client_msg_join_write(buffer);
	if (!Net::socket_send(&sock, buffer, join_msg_size, &server_endpoint))
	{
		return 0;
	}


	Player_State me {};
	constexpr uint32 c_max_predicted_ticks = c_ticks_per_second * 2;
	Player_State prediction_history[c_max_predicted_ticks];
	uint32 prediction_history_head = 0;
	uint32 prediction_history_tail = 0;
	uint32 prediction_history_head_tick_number = 0;
	Player_Visual_State objects[c_max_clients];
	uint32 num_objects = 0;
	uint32 slot = (uint32)-1;
	uint32 tick_number = (uint32)-1;
	uint32 target_tick_number = (uint32)-1;
	Timer local_timer = timer_create();
	Timer tick_timer = timer_create();

	
	// main loop
	g_is_running = 1;
	while (g_is_running)
	{
		timer_restart(&tick_timer);

		// Windows messages
		MSG message;
		UINT filter_min = 0;
		UINT filter_max = 0;
		UINT remove_message = PM_REMOVE;
		while (PeekMessage(&message, window_handle, filter_min, filter_max, remove_message))
		{
			TranslateMessage( &message );
			DispatchMessage( &message );
		}


		// Process Packets
		uint32 bytes_received;
		Net::IP_Endpoint from;
		while (Net::socket_receive(&sock, buffer, &bytes_received, &from))
		{
			switch (buffer[0])
			{
				case Net::Server_Message::Join_Result:
				{
					bool32 success;
					Net::server_msg_join_result_read(buffer, &success, &slot);
					if (!success)
					{
						log("[client] server didn't let us in\n");
					}
				}
				break;

				case Net::Server_Message::State:
				{
					uint32 state_tick_number;
					uint32 state_timestamp;
					Player_State state_my_object;
					uint32 state_num_other_objects;
					Net::server_msg_state_read(buffer, &state_tick_number, &state_my_object, &state_timestamp, objects, c_max_clients, &state_num_other_objects);

					num_objects = state_num_other_objects + 1;

					uint32 time_now_ms = (uint32)(timer_get_s(&local_timer) * 1000.0f);
					uint32 est_rtt_ms = time_now_ms - state_timestamp;
					log("[client] est RTT %dms\n", est_rtt_ms);
					
					// predict at half rtt, plus a bit
					// todo(jbr) better method of working out how much to predict
					float32 est_rtt_s = est_rtt_ms / 1000.0f;
					uint32 ticks_to_predict = (uint32)((est_rtt_s * 0.5f) / c_seconds_per_tick);
					ticks_to_predict += 15; // todo(jbr) not sure why this is so high?
					uint32 old_target_tick_number = target_tick_number;
					target_tick_number = state_tick_number + ticks_to_predict;
					log("[client] state tick %d received, target predicted tick %d (was %d)\n", state_tick_number, target_tick_number, old_target_tick_number);

					if (tick_number == (uint32)-1 ||
					 	state_tick_number >= tick_number)
					{
						// on first state message, or when the server manages to get ahead of us, just reset our prediction etc from this state message
						// todo(jbr) start at target tick number?
						me = state_my_object;
						tick_number = state_tick_number;
						prediction_history_head = 0;
						prediction_history_tail = 0;
						prediction_history_head_tick_number = tick_number + 1;
					}
					else
					{
						uint32 prediction_history_size = prediction_history_tail > prediction_history_head ? prediction_history_tail - prediction_history_head : c_max_predicted_ticks - (prediction_history_head - prediction_history_tail);
						while (prediction_history_size)
						{
							if (prediction_history_head_tick_number < state_tick_number)
							{
								// discard this one, not needed
								++prediction_history_head_tick_number;
								prediction_history_head = (prediction_history_head + 1) % c_max_predicted_ticks;
								--prediction_history_size;
								log("[client] discarding unneeded prediction history\n");
							}
							else if (prediction_history_head_tick_number == state_tick_number)
							{
								float32 dx = prediction_history[prediction_history_head].x - state_my_object.x;
								float32 dy = prediction_history[prediction_history_head].y - state_my_object.y;
								float32 error = sqrtf((dx * dx) + (dy * dy));
								log("[client] error %f\n", error);
								break;
							}
							else
							{
								// todo(jbr) can this happen other than when we're predicting to far? handle this
								assert(false);
							}
						}
					}
				}
				break;
			}
		}


		// tick player if we have one
		if (slot != (uint32)-1 && 
			tick_number != (uint32)-1)
		{
			uint32 time_ms = (uint32)(timer_get_s(&local_timer) * 1000.0f);
			uint32 input_msg_size = Net::client_msg_input_write(buffer, slot, &g_input, time_ms, tick_number);
			Net::socket_send(&sock, buffer, input_msg_size, &server_endpoint);

			while (tick_number < target_tick_number)
			{
				log("x = %f, y = %f, facing = %f, speed = %f, up = %d, down = %d, left = %d, right = %d", client_objects[i].x, client_objects[i].y, client_objects[i].facing, client_objects[i].speed, client_inputs[i].up, client_inputs[i].down, client_inputs[i].left, client_inputs[i].right);

				tick_player(&me, &g_input);
				++tick_number;

				// todo(jbr) detect and handle buffer being full
				prediction_history[prediction_history_tail] = me;
				prediction_history_tail = (prediction_history_tail + 1) % c_max_predicted_ticks;
			}

			++target_tick_number;

			// we're always the last object in the array
			objects[num_objects - 1].x = me.x;
			objects[num_objects - 1].y = me.y;
			objects[num_objects - 1].facing = me.facing;
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


		timer_wait_until(&tick_timer, c_seconds_per_tick);
	}

	uint32 leave_msg_size = Net::client_msg_leave_write(buffer, slot);
	Net::socket_send(&sock, buffer, leave_msg_size, &server_endpoint);
	Net::socket_close(&sock);

	// todo( jbr ) return wParam of WM_QUIT
	return 0;
}