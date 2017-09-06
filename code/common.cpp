#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <math.h>

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef long long int64;
typedef int int32;
typedef short int16;
typedef char int8;
typedef int32 bool32;
typedef float float32;
typedef double float64;


constexpr uint16 	c_port 				= 9999;
constexpr uint32 	c_socket_buffer_size = 1024;
constexpr uint16	c_max_clients 		= 32;
constexpr uint32	c_ticks_per_second 	= 60;
constexpr float32	c_seconds_per_tick 	= 1.0f / (float32)c_ticks_per_second;
constexpr float32 	c_turn_speed 		= 1.0f;	// how fast player turns
constexpr float32 	c_acceleration 		= 20.0f;
constexpr float32 	c_max_speed 		= 50.0f;




struct Timing_Info
{
	LARGE_INTEGER clock_frequency;
	bool32 sleep_granularity_was_set;
};

struct Player_Input
{
	bool32 up, down, left, right;
};

struct Player_State
{
	float32 x, y, facing, speed;
};

struct Player_Visual_State
{
	float32 x, y, facing;
};


#ifndef RELEASE
#define assert(x) if (!(x)) { int* p = 0; *p = 0; }
#else
#define assert(x)
#endif

// todo(jbr) logging system
static void log_warning(const char* fmt, int arg)
{
	char buffer[256];
	sprintf_s(buffer, sizeof(buffer), fmt, arg);
	OutputDebugStringA(buffer);
}

static float32 time_since(LARGE_INTEGER t, LARGE_INTEGER frequency)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - t.QuadPart) / (float32)frequency.QuadPart;
}

static Timing_Info timing_info_create()
{
	Timing_Info timing_info = {};

	UINT sleep_granularity_ms = 1;
	timing_info.sleep_granularity_was_set = timeBeginPeriod(sleep_granularity_ms) == TIMERR_NOERROR;

	QueryPerformanceFrequency(&timing_info.clock_frequency);

	return timing_info;
}

static void wait_for_tick_end(LARGE_INTEGER tick_start_time, Timing_Info* timing_info)
{
	float32 time_taken_s = time_since(tick_start_time, timing_info->clock_frequency);

	while (time_taken_s < c_seconds_per_tick)
	{
		if (timing_info->sleep_granularity_was_set)
		{
			DWORD time_to_wait_ms = (DWORD)((c_seconds_per_tick - time_taken_s) * 1000);
			if(time_to_wait_ms > 0)
			{
				Sleep(time_to_wait_ms);
			}
		}

		time_taken_s = time_since(tick_start_time, timing_info->clock_frequency);
	}
}

// todo(jbr) pass many players and inputs
static void tick_player(Player_State* player_state, Player_Input* player_input)
{
	if (player_input->up)
	{
		player_state->speed += c_acceleration * c_seconds_per_tick;
		if (player_state->speed > c_max_speed)
		{
			player_state->speed = c_max_speed;
		}
	}
	if (player_input->down)
	{
		player_state->speed -= c_acceleration * c_seconds_per_tick;
		if (player_state->speed < 0.0f)
		{
			player_state->speed = 0.0f;
		}
	}
	if (player_input->left)
	{
		player_state->facing -= c_turn_speed * c_seconds_per_tick;
	}
	if (player_input->right)
	{
		player_state->facing += c_turn_speed * c_seconds_per_tick;
	}

	player_state->x += player_state->speed * c_seconds_per_tick * sinf(player_state->facing);
	player_state->y += player_state->speed * c_seconds_per_tick * cosf(player_state->facing);
}