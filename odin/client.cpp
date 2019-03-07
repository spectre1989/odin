// std
#include <atomic>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <thread>
// odin
#include "core.h"
#include "graphics.h"
#include "net.h"
#include "net_msgs.h"
#include "player.h"
#include "server.h"



struct Client_Globals
{
	Player_Input player_input;
};

// todo(jbr) input thread
static void update_input(Player_Input* player_input, WPARAM keycode, bool32 value)
{
	switch (keycode)
	{
		case 'A':
			player_input->left = value;
		break;

		case 'D':
			player_input->right = value;
		break;

		case 'W':
			player_input->up = value;
		break;

		case 'S':
			player_input->down = value;
		break;
	}
}

static Client_Globals* get_client_globals(HWND window_handle)
{
	return (Client_Globals*)GetWindowLongPtr(window_handle, 0);
}

LRESULT CALLBACK WindowProc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
	switch (message)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		break;

		case WM_KEYDOWN:
			update_input(&get_client_globals(window_handle)->player_input, w_param, 1);
			return 0;
		break;

		case WM_KEYUP:
			update_input(&get_client_globals(window_handle)->player_input, w_param, 0);
			return 0;
		break;
	}

	return DefWindowProc(window_handle, message, w_param, l_param);
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE /*prev_instance*/, LPSTR /*cmd_line*/, int cmd_show)
{
	WNDCLASS window_class;
	window_class.style = 0;
	window_class.lpfnWndProc = WindowProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = sizeof(Client_Globals*);
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = 0;
	window_class.hbrBackground = 0;
	window_class.lpszMenuName = 0;
	window_class.lpszClassName = "odin_window_class";

	ATOM window_class_atom = RegisterClass(&window_class);

	assert(window_class_atom);

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

		window_handle 			= CreateWindowA(window_class.lpszClassName, window_name, style, x, y, c_window_width, c_window_height, parent_window, menu, instance, param);

		assert(window_handle);
	}
	ShowWindow(window_handle, cmd_show);

	// This stuff needs to be done before starting server thread
	UINT sleep_granularity_ms = 1;
	bool32 sleep_granularity_was_set = timeBeginPeriod(sleep_granularity_ms) == TIMERR_NOERROR;

	if (!Net::init())
	{
		return 0;
	}

	std::atomic_bool server_should_run = true;
	std::thread server_thread(&server_main, &server_should_run);

	Linear_Allocator allocator;
	linear_allocator_create(&allocator, megabytes(16));

	Linear_Allocator temp_allocator;
	linear_allocator_create_sub_allocator(&allocator, &temp_allocator, megabytes(8));

	Client_Globals* client_globals = (Client_Globals*)linear_allocator_alloc(&allocator, sizeof(Client_Globals));
	client_globals->player_input = {};

	SetWindowLongPtr(window_handle, 0, (LONG_PTR)client_globals);
	
	// init graphics
	Graphics::State* graphics_state = (Graphics::State*)linear_allocator_alloc(&allocator, sizeof(Graphics::State));
	Graphics::init(graphics_state, window_handle, instance, 
					c_window_width, c_window_height, c_max_clients,
					&allocator, &temp_allocator);

	Net::Socket sock;
	if (!Net::socket(&sock))
	{
		return 0;
	}
	Net::socket_set_fake_lag_s(&sock, 0.2f, &allocator); // 200ms of fake lag

	constexpr uint32 c_socket_buffer_size = c_packet_budget_per_tick;
	uint8* socket_buffer = linear_allocator_alloc(&allocator, c_socket_buffer_size);
	Net::IP_Endpoint server_endpoint = Net::ip_endpoint(127, 0, 0, 1, c_port);

	uint32 join_msg_size = Net::client_msg_join_write(socket_buffer);
	if (!Net::socket_send(&sock, socket_buffer, join_msg_size, &server_endpoint))
	{
		return 0;
	}

	Player_Snapshot_State* player_snapshot_states	= (Player_Snapshot_State*)	linear_allocator_alloc(&allocator, sizeof(Player_Snapshot_State) * c_max_clients);
	bool32* players_present							= (bool32*)					linear_allocator_alloc(&allocator, sizeof(bool32) * c_max_clients);
	Matrix_4x4* mvp_matrices						= (Matrix_4x4*)				linear_allocator_alloc(&allocator, sizeof(Matrix_4x4) * (c_max_clients + 1));

	Player_Snapshot_State*	local_player_snapshot_state			= (Player_Snapshot_State*)	linear_allocator_alloc(&allocator, sizeof(Player_Snapshot_State));
	Player_Extra_State*		local_player_extra_state			= (Player_Extra_State*)	linear_allocator_alloc(&allocator, sizeof(Player_Extra_State));

	constexpr int32			c_prediction_history_capacity		= 512;
	constexpr int32			c_prediction_history_mask			= c_prediction_history_capacity - 1;
	Player_Snapshot_State*	prediction_history_snapshot_state	= (Player_Snapshot_State*)	linear_allocator_alloc(&allocator, sizeof(Player_Snapshot_State) * c_prediction_history_capacity);
	Player_Extra_State*		prediction_history_extra_state		= (Player_Extra_State*)		linear_allocator_alloc(&allocator, sizeof(Player_Extra_State) * c_prediction_history_capacity);
	float32*				prediction_history_dt				= (float32*)				linear_allocator_alloc(&allocator, sizeof(float32) * c_prediction_history_capacity);
	Player_Input*			prediction_history_input			= (Player_Input*)			linear_allocator_alloc(&allocator, sizeof(Player_Input) * c_prediction_history_capacity);

	constexpr float32	c_fov_y			= 60.0f * c_deg_to_rad;
	constexpr float32	c_aspect_ratio	= c_window_width / (float32)c_window_height;
	constexpr float32	c_near_plane	= 1.0f;
	constexpr float32	c_far_plane		= 100.0f;
	Matrix_4x4			projection_matrix;
	matrix_4x4_projection(&projection_matrix, c_fov_y, c_aspect_ratio, c_near_plane, c_far_plane);

	constexpr int32 c_tick_rate = 60; // todo(jbr) adaptive stable tick rate
	constexpr float32 c_seconds_per_tick = 1.0f / c_tick_rate;

	uint32 local_player_slot = (uint32)-1;
	uint32 prediction_id = 0; // todo(jbr) rolling sequence number, could maybe get away with 8 bits, certainly 9 or 10

	Timer tick_timer = timer();
	
	// main loop
	int exit_code = 0;
	while (true)
	{
		// Windows messages
		bool32 got_quit_message = 0;
		MSG message;
		HWND hwnd = 0; // WM_QUIT is not associated with a window, so this must be 0
		UINT filter_min = 0;
		UINT filter_max = 0;
		UINT remove_message = PM_REMOVE;
		while (PeekMessage(&message, hwnd, filter_min, filter_max, remove_message))
		{
			if (message.message == WM_QUIT)
			{
				exit_code = (int)message.wParam;
				got_quit_message = 1;
				break;
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (got_quit_message)
		{
			break;
		}


		// Process Packets
		uint32 bytes_received;
		Net::IP_Endpoint from;
		while (Net::socket_receive(&sock, socket_buffer, c_socket_buffer_size, &bytes_received, &from))
		{
			switch ((Net::Server_Message)socket_buffer[0])
			{
				case Net::Server_Message::Join_Result:
				{
					bool32 success;
					Net::server_msg_join_result_read(socket_buffer, &success, &local_player_slot);
					if (!success)
					{
						log("[client] server didn't let us in\n");
					}
				}
				break;

				case Net::Server_Message::State:
				{
					uint32 received_prediction_id;
					Player_Extra_State received_local_player_extra_state;
					Net::server_msg_state_read(
						socket_buffer, 
						&received_prediction_id, 
						&received_local_player_extra_state, 
						player_snapshot_states, 
						players_present,
						c_max_clients);

					int32 ticks_ahead = prediction_id - received_prediction_id;
					assert(ticks_ahead > -1);
					assert(ticks_ahead <= c_prediction_history_capacity); // todo(jbr) cope better with this case
					
					Player_Snapshot_State* received_local_player_snapshot_state = &player_snapshot_states[local_player_slot];

					uint32 index = received_prediction_id & c_prediction_history_mask;
					float32 dx = prediction_history_snapshot_state[index].x - received_local_player_snapshot_state->x;
					float32 dy = prediction_history_snapshot_state[index].y - received_local_player_snapshot_state->y;
					constexpr float32 c_max_error = 0.001f; // 0.1cm
					constexpr float32 c_max_error_sq = c_max_error * c_max_error;
					float32 error_sq = (dx * dx) + (dy * dy);
					if (error_sq > c_max_error_sq)
					{
						log("[client]error of %f detected at prediction id %d, rewinding and replaying\n", sqrtf(error_sq), received_prediction_id);
						
						*local_player_snapshot_state = *received_local_player_snapshot_state;
						*local_player_extra_state = received_local_player_extra_state;

						for (uint32 replaying_prediction_id = received_prediction_id + 1; 
							replaying_prediction_id < prediction_id; 
							++replaying_prediction_id)
						{
							uint32 replaying_index = replaying_prediction_id & c_prediction_history_mask;

							tick_player(local_player_snapshot_state, 
										local_player_extra_state, 
										prediction_history_dt[replaying_index], 
										&prediction_history_input[replaying_index]);

							prediction_history_snapshot_state[replaying_index] = *local_player_snapshot_state;
							prediction_history_extra_state[replaying_index] = *local_player_extra_state;
						}
					}
				}
				break;
			}
		}

		
		// tick player if we have one
		if (local_player_slot != (uint32)-1)
		{
			float32 dt = c_seconds_per_tick;
			
			uint32 input_msg_size = Net::client_msg_input_write(socket_buffer, local_player_slot, dt, &client_globals->player_input, prediction_id);
			Net::socket_send(&sock, socket_buffer, input_msg_size, &server_endpoint);

			tick_player(local_player_snapshot_state, 
						local_player_extra_state, 
						dt, 
						&client_globals->player_input);

			uint32 index = prediction_id & c_prediction_history_mask;
			prediction_history_snapshot_state[index]	= *local_player_snapshot_state;
			prediction_history_extra_state[index]		= *local_player_extra_state;
			prediction_history_dt[index]				= dt;
			prediction_history_input[index]				= client_globals->player_input;
			
			player_snapshot_states[local_player_slot] = *local_player_snapshot_state;

			++prediction_id;
		}

		// Create view-projection matrix
		constexpr float32 c_camera_offset_distance = 3.0f;
		Vec_3f camera_pos = vec_3f(
			local_player_snapshot_state->x + (c_camera_offset_distance * sinf(local_player_snapshot_state->facing)), 
			local_player_snapshot_state->y - (c_camera_offset_distance * cosf(local_player_snapshot_state->facing)), 
			1.0f);
		Vec_3f player_pos = vec_3f(local_player_snapshot_state->x, local_player_snapshot_state->y, 0.0f);
		
		Matrix_4x4 view_matrix;
		matrix_4x4_lookat(	&view_matrix, 
							camera_pos, 				// position
							player_pos, 				// target
							vec_3f(0.0f, 0.0f, 1.0f)); 	// up
		
		Matrix_4x4 view_projection_matrix;
		matrix_4x4_mul(&view_projection_matrix, &projection_matrix, &view_matrix);

		// Create mvp matrix for scenery (just copy view-projection, scenery is not moved)
		mvp_matrices[0] = view_projection_matrix;

		// Create mvp matrix for each player
		Matrix_4x4 temp_translation_matrix;
		Matrix_4x4 temp_rotation_matrix;
		
		bool32* players_present_end = &players_present[c_max_clients];
		Player_Snapshot_State* player_snapshot_state = &player_snapshot_states[0];
		Matrix_4x4* player_mvp_matrix = &mvp_matrices[1];
		Matrix_4x4 temp_model_matrix;
		for (bool32* players_present_iter = &players_present[0];
			players_present_iter != players_present_end;
			++players_present_iter, ++player_snapshot_state)
		{
			if (*players_present_iter)
			{
				matrix_4x4_rotation_z(&temp_rotation_matrix, player_snapshot_state->facing);
				matrix_4x4_translation(&temp_translation_matrix, player_snapshot_state->x, player_snapshot_state->y, 0.0f);
				matrix_4x4_mul(&temp_model_matrix, &temp_translation_matrix, &temp_rotation_matrix);
				matrix_4x4_mul(player_mvp_matrix, &view_projection_matrix, &temp_model_matrix);
				
				++player_mvp_matrix;
			}
		}
		uint32 num_matrices = (uint32)(player_mvp_matrix - &mvp_matrices[1]);
		Graphics::update_and_draw(graphics_state, mvp_matrices, num_matrices);

		timer_wait_until(&tick_timer, c_seconds_per_tick, sleep_granularity_was_set);
		timer_shift_start(&tick_timer, c_seconds_per_tick);
	}

	uint32 leave_msg_size = Net::client_msg_leave_write(socket_buffer, local_player_slot);
	Net::socket_send(&sock, socket_buffer, leave_msg_size, &server_endpoint);
	Net::socket_close(&sock);

	server_should_run = false;
	server_thread.join();

	return exit_code;
}