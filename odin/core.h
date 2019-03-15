#pragma once

#include <stdarg.h>
#include <windows.h>



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
constexpr uint32 	c_packet_budget_per_tick = 1024;
constexpr uint32	c_max_clients = 32;
constexpr int32		c_max_client_tick_rate = 120;
constexpr int32		c_max_server_tick_rate = 60;


constexpr uint64 kilobytes(uint32 kb)
{
	return kb * 1024;
}
constexpr uint64 megabytes(uint32 mb)
{
	return kilobytes(mb * 1024);
}
constexpr uint64 gigabytes(uint32 gb)
{
	return megabytes(gb * 1024);
}


#ifndef RELEASE
#define assert(x) if (!(x)) { int* p = 0; *p = 0; }
#else
#define assert(x)
#endif


struct Timer
{
	LARGE_INTEGER start;
	LARGE_INTEGER frequency;
};

struct Linear_Allocator
{
	uint8* memory;
	uint8* next;
	uint64 bytes_remaining;
};


Timer	timer();
float32 timer_get_s(Timer* timer);
void	timer_wait_until(Timer* timer, float32 wait_time_s, bool sleep_granularity_is_set);
void	timer_shift_start(Timer* timer, float32 accumulate_s);

void linear_allocator_create(Linear_Allocator* allocator, uint64 size);
void linear_allocator_create_sub_allocator(Linear_Allocator* allocator, Linear_Allocator* sub_allocator, uint64 size);
uint8* linear_allocator_alloc(Linear_Allocator* allocator, uint64 size);

void log(const char* format, ...);