typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef long long int64;
typedef int int32;
typedef short int16;
typedef char int8;
typedef int bool32;
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

struct Player_State
{
	float32 x, y, facing, speed;
};


static float32 time_since(LARGE_INTEGER t, LARGE_INTEGER frequency)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - t.QuadPart) / (float32)frequency.QuadPart;
}