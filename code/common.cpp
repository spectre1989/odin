#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

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


constexpr uint16 	c_port = 9999;
constexpr uint32 	c_socket_buffer_size = 1024;
constexpr uint16	c_max_clients 		= 32;
constexpr uint32	c_ticks_per_second 	= 60;
constexpr float32	c_seconds_per_tick 	= 1.0f / (float32)c_ticks_per_second;


enum class Client_Message : uint8
{
	Join,		// tell server we're new here
	Leave,		// tell server we're leaving
	Input 		// tell server our user input
};

enum class Server_Message : uint8
{
	Join_Result,// tell client they're accepted/rejected
	State 		// tell client game state
};


struct Timing_Info
{
	LARGE_INTEGER clock_frequency;
	bool32 sleep_granularity_was_set;
};


#ifndef RELEASE
#define assert( x ) if( !( x ) ) { int* p = 0; *p = 0; }
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