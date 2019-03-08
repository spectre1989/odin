#include "core.h"

#include <cmath>
#include <cstdio>



Timer timer()
{
	Timer timer = {};
	QueryPerformanceFrequency(&timer.frequency);
	QueryPerformanceCounter(&timer.start);
	return timer;
}

float32 timer_get_s(Timer* timer)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - timer->start.QuadPart) / (float32)timer->frequency.QuadPart;
}

void timer_wait_until(Timer* timer, float32 wait_time_s, bool sleep_granularity_is_set)
{
	float32 time_taken_s = timer_get_s(timer);

	while (time_taken_s < wait_time_s)
	{
		if (sleep_granularity_is_set)
		{
			DWORD time_to_wait_ms = (DWORD)((wait_time_s - time_taken_s) * 1000);
			if (time_to_wait_ms > 1) // Sleep frequently oversleeps by 1ms, so spin for everything smaller than 2
			{
				Sleep(time_to_wait_ms);
			}
		}

		time_taken_s = timer_get_s(timer);
	}
}

void timer_shift_start(Timer* timer, float32 accumulate_s)
{
	timer->start.QuadPart += (LONGLONG)(timer->frequency.QuadPart * accumulate_s);
}


void linear_allocator_create(Linear_Allocator* allocator, uint64 size)
{
	allocator->memory = new uint8[size];
	allocator->next = allocator->memory;
	allocator->bytes_remaining = size;
}

void linear_allocator_create_sub_allocator(Linear_Allocator* allocator, Linear_Allocator* sub_allocator, uint64 size)
{
	sub_allocator->memory = linear_allocator_alloc(allocator, size);
	sub_allocator->next = sub_allocator->memory;
	sub_allocator->bytes_remaining = size;
}

uint8* linear_allocator_alloc(Linear_Allocator* allocator, uint64 size)
{
	assert(allocator->bytes_remaining >= size);
	uint8* mem = allocator->next;
	allocator->next += size;
	allocator->bytes_remaining -= size;
	return mem;
}


void log(const char* format, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	OutputDebugStringA(buffer);
}