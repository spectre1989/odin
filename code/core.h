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


#ifndef RELEASE
#define assert(x) if (!(x)) { int* p = 0; *p = 0; }
#else
#define assert(x)
#endif


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
}; // todo(jbr) stick allocators in globals, and then make convenient functions like alloc_permanent alloc_temp etc
// todo(jbr) strip back the DI, only use where actually needed - i.e. hooking up client to use debug output, and server printf, but store that callback in globals, stop prematurely abstracting
// todo(jbr) should some "create" functions be renamed to "init"?
void memory_allocator_create(Memory_Allocator* allocator, uint8* memory, uint64 size);
uint8* memory_allocator_alloc(Memory_Allocator* allocator, uint64 size);
uint8* allocate_permanent(uint64 size);
uint8* allocate_temp(uint64 size);


struct Globals
{
	Memory_Allocator permanent_allocator;
	Memory_Allocator temp_allocator;
	LARGE_INTEGER clock_frequency;
	bool sleep_granularity_was_set;
};
extern Globals* globals;
void globals_init();