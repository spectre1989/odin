#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan\vulkan.h>

#include "maths.h"


namespace Graphics
{


struct Vertex
{
	Vec_3f pos;
	Vec_3f colour;
};

struct State
{
	VkDevice device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSwapchainKHR swapchain;
	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;
	VkCommandPool* command_pools;
	VkCommandBuffer* command_buffers;
	bool32* command_buffers_in_use;
	VkFence* fences;
	VkBuffer matrix_buffer;
	VkDeviceMemory matrix_buffer_memory;
	uint32 num_matrix_buffer_padding_bytes;
	VkBuffer cube_vertex_buffer;
	VkBuffer cube_index_buffer;
	uint32 cube_num_indices;
	VkBuffer scenery_vertex_buffer;
	VkBuffer scenery_index_buffer;
	uint32 scenery_num_indices;
	VkRenderPass render_pass;
	VkFramebuffer* swapchain_framebuffers;
	VkExtent2D swapchain_extent;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkDescriptorSet descriptor_set;
};

void init(	State* out_state, 
			HWND window_handle, HINSTANCE instance, 
			uint32 window_width, uint32 window_height, 
			uint32 max_players,
			Linear_Allocator* allocator, Linear_Allocator* temp_allocator);
void update_and_draw(State* state, Matrix_4x4* matrices, uint32 num_players);


} // namespace Graphics