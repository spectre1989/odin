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

typedef void (Log_Function)(const char*, va_list);



constexpr uint16 	c_port = 9999;
constexpr uint32 	c_packet_budget_per_tick = 1024;
constexpr uint32	c_max_clients = 32;
constexpr uint32	c_ticks_per_second = 60;
constexpr float32	c_seconds_per_tick = 1.0f / (float32)c_ticks_per_second;
constexpr float32 	c_turn_speed = 1.0f;	// how fast player turns
constexpr float32 	c_acceleration = 20.0f;
constexpr float32 	c_max_speed = 50.0f;



#ifndef RELEASE
#define assert(x) if (!(x)) { int* p = 0; *p = 0; }
#else
#define assert(x)
#endif



struct Circular_Index
{
	uint32 head;
	uint32 tail;
	uint32 available;
	uint32 size;
};
void circular_index_create(Circular_Index* index, uint32 size);
bool32 circular_index_is_full(Circular_Index* index);
bool32 circular_index_is_empty(Circular_Index* index);
void circular_index_push(Circular_Index* index);
void circular_index_pop(Circular_Index* index);


struct Timer
{
	LARGE_INTEGER start;
};
Timer	timer_create();
void	timer_restart(Timer* timer);
float32 timer_get_s(Timer* timer);
void	timer_wait_until(Timer* timer, float32 wait_time_s);


struct Memory_Allocator
{
	uint8* memory;
	uint8* next;
	uint64 bytes_remaining;
};
void memory_allocator_create(Memory_Allocator* allocator, uint8* memory, uint64 size);
uint8* memory_allocator_alloc(Memory_Allocator* allocator, uint64 size);
uint8* alloc_permanent(uint64 size);
uint8* alloc_temp(uint64 size);


struct Globals
{
	Memory_Allocator permanent_allocator;
	Memory_Allocator temp_allocator;
	Log_Function* log_function;
	LARGE_INTEGER clock_frequency;
	bool sleep_granularity_was_set;
};
extern Globals* globals;
void globals_init(Log_Function* log_func);
void log(const char* format, ...);