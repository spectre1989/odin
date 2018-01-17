#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan\vulkan.h>

#include "core.h"


namespace Graphics
{


struct Vertex
{
	float32 pos_x;
	float32 pos_y;
	float32 col_r;
	float32 col_g;
	float32 col_b;
};

struct State
{
	VkDevice device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkDeviceMemory vertex_buffer_memory;
	VkSwapchainKHR swapchain;
	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;
	VkCommandBuffer* command_buffers;
	float projection_matrix[16];
	VkBuffer projection_matrix_buffer;
	VkDeviceMemory projection_matrix_buffer_memory;
};

void init(	State* out_state, 
			HWND window_handle, HINSTANCE instance, 
			uint32 window_width, uint32 window_height, 
			float32 fov_y, float32 near_plane, float32 far_plane, 
			uint32 num_vertices, uint16* indices, uint32 num_indices);
void update_and_draw(Vertex* vertices, uint32 num_vertices, State* state);


} // namespace Graphics