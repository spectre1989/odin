#include <winsock2.h> // must include this before windows.h
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
constexpr uint32	c_max_clients 		= 32;
constexpr uint32	c_ticks_per_second 	= 60;
constexpr float32	c_seconds_per_tick 	= 1.0f / (float32)c_ticks_per_second;
constexpr float32 	c_turn_speed 		= 1.0f;	// how fast player turns
constexpr float32 	c_acceleration 		= 20.0f;
constexpr float32 	c_max_speed 		= 50.0f;




typedef void (Log_Function)(const char*, va_list);

struct Timer
{
	LARGE_INTEGER start;
}
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


static LARGE_INTEGER get_clock_frequency()
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return frequency;
}
static bool try_set_sleep_granularity()
{
	UINT sleep_granularity_ms = 1;
	return timeBeginPeriod(sleep_granularity_ms) == TIMERR_NOERROR;
}

static LARGE_INTEGER g_timer_clock_frequency = get_clock_frequency();
static bool32 g_timer_sleep_granularity_was_set = try_set_sleep_granularity();

static void timer_restart(Timer* timer)
{
	QueryPerformanceCounter(&timer->start);
}
static Timer timer_create()
{
	Timer timer = {};
	timer_restart(&timer);
	return timer;
}
static float32 timer_get_s(Timer* timer)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - timer->start.QuadPart) / (float32)g_timer_clock_frequency.QuadPart;
}
static void timer_wait_until(Timer* timer, float32 wait_time_s)
{
	float32 time_taken_s = timer_get_s(timer);

	while (time_taken_s < wait_time_s)
	{
		if (g_timer_sleep_granularity_was_set)
		{
			DWORD time_to_wait_ms = (DWORD)((wait_time_s - time_taken_s) * 1000);
			if(time_to_wait_ms > 0)
			{
				Sleep(time_to_wait_ms);
			}
		}

		time_taken_s = timer_get_s(timer);
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