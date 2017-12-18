#include "core.h"



void timer_restart(Timer* timer)
{
	QueryPerformanceCounter(&timer->start);
}

Timer timer_create()
{
	Timer timer = {};
	timer_restart(&timer);
	return timer;
}

float32 timer_get_s(Timer* timer)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - timer->start.QuadPart) / (float32)globals->clock_frequency.QuadPart;
}

void timer_wait_until(Timer* timer, float32 wait_time_s)
{
	float32 time_taken_s = timer_get_s(timer);

	while (time_taken_s < wait_time_s)
	{
		if (globals->sleep_granularity_was_set)
		{
			DWORD time_to_wait_ms = (DWORD)((wait_time_s - time_taken_s) * 1000);
			if (time_to_wait_ms > 0)
			{// todo(jbr) test this for possible oversleep
				Sleep(time_to_wait_ms);
			}
		}

		time_taken_s = timer_get_s(timer);
	}
}

void memory_allocator_create(Memory_Allocator* allocator, uint8* memory, uint64 size)
{
	allocator->memory = memory;
	allocator->next = memory;
	allocator->bytes_remaining = size;
}

uint8* memory_allocator_alloc(Memory_Allocator* allocator, uint64 size)
{
	assert(allocator->bytes_remaining >= size);
	uint8* mem = allocator->next;
	allocator->next += size;
	return mem;
}

uint8* allocate_permanent(uint64 size)
{
	return memory_allocator_alloc(&globals->permanent_allocator, size);
}

uint8* allocate_temp(uint64 size)
{
	return memory_allocator_alloc(&globals->temp_allocator, size);
}


static constexpr uint64 kilobytes(uint32 kb)
{
	return kb * 1024;
}

static constexpr uint64 megabytes(uint32 mb)
{
	return kilobytes(mb * 1024);
}

static constexpr uint64 gigabytes(uint32 gb)
{
	return megabytes(gb * 1024);
}

void globals_init(Log_Function* log_func)
{
	constexpr uint64 c_permanent_memory_size = megabytes(64);
	constexpr uint64 c_temp_memory_size = megabytes(64);
	constexpr uint64 c_total_memory_size = c_permanent_memory_size + c_temp_memory_size;

	uint8* memory = (uint8*)VirtualAlloc((LPVOID)gigabytes(1), c_total_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	// put globals at the start of permanent memory block
	globals = (Globals*)memory;
	
	memory_allocator_create(&globals->permanent_allocator, memory + sizeof(Globals), c_permanent_memory_size - sizeof(Globals));
	memory_allocator_create(&globals->temp_allocator, memory + c_permanent_memory_size, c_temp_memory_size);
	globals->log_function = log_func;
	QueryPerformanceFrequency(&globals->clock_frequency);
	UINT sleep_granularity_ms = 1;
	globals->sleep_granularity_was_set = timeBeginPeriod(sleep_granularity_ms) == TIMERR_NOERROR;
}

void log(const char* format, ...)
{
	assert(globals->log_function);

	va_list args;
	va_start(args, format);
	globals->log_function(format, args);
	va_end(args);
}