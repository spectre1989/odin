#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan\vulkan.h>

#include "core.h"


namespace Graphics
{


struct Vertex
{
	Vector3 pos; // todo(jbr) update layout when setting up pipeline
	Vector3 colour;
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
	Matrix4x4 projection_matrix;
	VkBuffer matrix_buffer;
	VkDeviceMemory matrix_buffer_memory;
};

void init(	State* out_state, 
			HWND window_handle, HINSTANCE instance, 
			uint32 window_width, uint32 window_height, 
			uint32 max_objects);
void update_and_draw(State* state, Matrix4x4* matrices, uint32 num_matrices);


} // namespace Graphics