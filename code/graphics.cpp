#include "graphics.h"


namespace Graphics
{



// the return value indicates whether the calling layer should abort the vulkan call
static VkBool32 VKAPI_PTR 
vulkan_debug_callback(	VkDebugReportFlagsEXT /*flags*/,
						VkDebugReportObjectTypeEXT /*objType*/, uint64_t /*obj*/, 
						size_t /*location*/, int32_t /*code*/, 
						const char* layer_prefix, const char* msg, void* /*user_data*/) 
{
	log("[graphics::vulkan::%s] %s\n", layer_prefix, msg);
    return VK_FALSE;
}

static void create_buffer(VkPhysicalDevice physical_device, VkDevice device, const VkBufferCreateInfo* pc_buffer_create_info, VkBuffer* p_out_buffer, VkDeviceMemory* p_out_buffer_memory)
{
	VkBuffer buffer;
	VkResult result = vkCreateBuffer(device, pc_buffer_create_info, 0, &buffer);
	assert(result == VK_SUCCESS);

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

	VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);

	constexpr uint32 c_required_memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32 chosen_memory_type_index = (uint32)-1;
	for(uint32 i = 0; i < physical_device_memory_properties.memoryTypeCount; ++i) 
	{
	    if((memory_requirements.memoryTypeBits & (1 << i)) && 
	    	(physical_device_memory_properties.memoryTypes[i].propertyFlags & c_required_memory_properties) == c_required_memory_properties) 
	    {
	    	chosen_memory_type_index = i;
	    	break;
	    }
	}

	assert(chosen_memory_type_index != (uint32)-1);

	VkMemoryAllocateInfo memory_alloc_info = {};
	memory_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_alloc_info.allocationSize = memory_requirements.size;
	memory_alloc_info.memoryTypeIndex = chosen_memory_type_index;

	VkDeviceMemory buffer_memory;
	result = vkAllocateMemory(device, &memory_alloc_info, 0, &buffer_memory);
	assert(result == VK_SUCCESS);

	vkBindBufferMemory(device, buffer, buffer_memory, 0);

	*p_out_buffer = buffer;
	*p_out_buffer_memory = buffer_memory;
}

static void copy_to_buffer(VkDevice device, VkDeviceMemory buffer_memory, void* src, uint32 size)
{
	void* dst;
	vkMapMemory(device, buffer_memory, 0, size, 0, &dst);
	memcpy(dst, src, size);
	vkUnmapMemory(device, buffer_memory);
}

void init(	State* out_state, 
			HWND window_handle, HINSTANCE instance, 
			uint32 window_width, uint32 window_height, 
			float32 fov_y, float32 near_plane, float32 far_plane, 
			uint32 num_vertices, uint16* indices, uint32 num_indices)
{
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instance_create_info = {};
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pApplicationInfo = &app_info;
	#ifndef RELEASE
	instance_create_info.enabledLayerCount = 1;
	const char* validation_layers[] = {"VK_LAYER_LUNARG_standard_validation"};
	instance_create_info.ppEnabledLayerNames = validation_layers;
	#endif
	const char* enabled_extension_names[] = {VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
	instance_create_info.enabledExtensionCount = sizeof( enabled_extension_names ) / sizeof( enabled_extension_names[0] );
	instance_create_info.ppEnabledExtensionNames = enabled_extension_names;
	
	VkInstance vulkan_instance;
	VkResult result = vkCreateInstance( &instance_create_info, 0, &vulkan_instance );
	assert( result == VK_SUCCESS );

	VkDebugReportCallbackCreateInfoEXT debug_callback_create_info = {};
	debug_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debug_callback_create_info.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	debug_callback_create_info.pfnCallback = vulkan_debug_callback;

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugReportCallbackEXT" );
	assert( vkCreateDebugReportCallbackEXT );

	VkDebugReportCallbackEXT debug_callback;
	result = vkCreateDebugReportCallbackEXT( vulkan_instance, &debug_callback_create_info, 0, &debug_callback );
	assert( result == VK_SUCCESS );

	VkWin32SurfaceCreateInfoKHR surface_create_info = {};
	surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_create_info.hwnd = window_handle;
	surface_create_info.hinstance = instance;

	PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr( vulkan_instance, "vkCreateWin32SurfaceKHR" );
	assert( vkCreateWin32SurfaceKHR );

	VkSurfaceKHR surface;
	result = vkCreateWin32SurfaceKHR( vulkan_instance, &surface_create_info, 0, &surface );
	assert( result == VK_SUCCESS );

	uint32 physical_device_count;
	result = vkEnumeratePhysicalDevices( vulkan_instance, &physical_device_count, 0 );
	assert( result == VK_SUCCESS );
	assert( physical_device_count > 0 );
	
	VkPhysicalDevice* physical_devices = (VkPhysicalDevice*)alloc_temp(sizeof(VkPhysicalDevice) * physical_device_count);

	result = vkEnumeratePhysicalDevices( vulkan_instance, &physical_device_count, physical_devices );
	assert( result == VK_SUCCESS );

	VkPhysicalDevice physical_device = 0;
	VkSurfaceFormatKHR swapchain_surface_format = {};
	VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR ; // guaranteed to be supported
	VkExtent2D swapchain_extent = {};
	uint32 swapchain_image_count = 0;
	for( uint32 i = 0; i < physical_device_count; ++i )
	{
		VkPhysicalDeviceProperties device_properties;
		//VkPhysicalDeviceFeatures device_features; TODO( jbr ) pick best device based on type and features

		vkGetPhysicalDeviceProperties( physical_devices[i], &device_properties );
		//vkGetPhysicalDeviceFeatures( physical_devices[i], &device_features );

		// for now just try to pick a discrete gpu, otherwise anything
		if( device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
		{
			uint32 extension_count;
			result = vkEnumerateDeviceExtensionProperties( physical_devices[i], 0, &extension_count, 0 );
			assert( result == VK_SUCCESS );

			VkExtensionProperties* device_extensions = (VkExtensionProperties*)alloc_temp(sizeof(VkExtensionProperties) * extension_count);
			result = vkEnumerateDeviceExtensionProperties( physical_devices[i], 0, &extension_count, device_extensions );
			assert( result == VK_SUCCESS );

			for( uint32 j = 0; j < extension_count; ++j )
			{
				if( strcmp( device_extensions[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
				{
					physical_device = physical_devices[i];
					break;
				}
			}

			if( physical_device )
			{
				uint32 format_count;
				uint32 present_mode_count;

				result = vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, 0 );
				assert( result == VK_SUCCESS );

				result = vkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &present_mode_count, 0 );
				assert( result == VK_SUCCESS );

				if( format_count && present_mode_count ) 
				{
					VkSurfaceFormatKHR* surface_formats = (VkSurfaceFormatKHR*)alloc_temp(sizeof(VkSurfaceFormatKHR) * format_count);
				    result = vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, surface_formats );
				    assert( result == VK_SUCCESS );

				    if( format_count == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED )
				    {
				    	swapchain_surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
				    	swapchain_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				    }
				    else
				    {
				    	// todo( jbr ) choose best surface format
						swapchain_surface_format = surface_formats[0];
				    }

					VkPresentModeKHR* present_modes = (VkPresentModeKHR*)alloc_temp(sizeof(VkPresentModeKHR) * present_mode_count);
				    result = vkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &present_mode_count, present_modes );
				    assert( result == VK_SUCCESS );

				    for( uint32 j = 0; j < present_mode_count; ++j )
				    {
				    	if( present_modes[j] == VK_PRESENT_MODE_MAILBOX_KHR )
				    	{
				    		swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				    	}
				    }

					VkSurfaceCapabilitiesKHR surface_capabilities = {};
					result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_capabilities );
					assert( result == VK_SUCCESS );
					
					if( surface_capabilities.currentExtent.width == 0xFFFFFFFF )
					{
						swapchain_extent = {window_width, window_height};
					}
					else
					{
						swapchain_extent = surface_capabilities.currentExtent;
					}

					if( surface_capabilities.maxImageCount == 0 || surface_capabilities.maxImageCount >= 3 )
					{
						swapchain_image_count = 3;
					}
					else
					{
						swapchain_image_count = surface_capabilities.maxImageCount;
					}
				}
				else
				{
					// no formats, can't use this device
					physical_device = 0;
				}
			}
		}

		if( physical_device )
		{
			break;
		}
	}

	assert( physical_device );

	uint32 queue_family_count;
	vkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, 0 );
	assert( queue_family_count > 0 );

	VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)alloc_temp(sizeof(VkQueueFamilyProperties) * queue_family_count);

	vkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

	uint32 graphics_queue_family_index = uint32( -1 );
	uint32 present_queue_family_index = uint32( -1 );
	for( uint32 i = 0; i < queue_family_count; ++i )
	{
		if( queue_families[i].queueCount > 0 )
		{
			if( queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
			{
				graphics_queue_family_index = i;
			}

			VkBool32 present_support = false;
			result = vkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, surface, &present_support );
			assert( result == VK_SUCCESS );
			if( present_support )
			{
				present_queue_family_index = i;
			}

			if( graphics_queue_family_index != uint32( -1 ) && present_queue_family_index != uint32( -1 ) )
			{
				break;
			}
		}
	}

	assert( graphics_queue_family_index != uint32( -1 ) );
	assert( present_queue_family_index != uint32( -1 ) );

	VkDeviceQueueCreateInfo device_queue_create_infos[2];
	device_queue_create_infos[0] = {};
	device_queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	device_queue_create_infos[0].queueFamilyIndex = graphics_queue_family_index;
	device_queue_create_infos[0].queueCount = 1;
	float32 queue_priority = 1.0f;
	device_queue_create_infos[0].pQueuePriorities = &queue_priority;

	uint32 queue_count = 1;
	if( graphics_queue_family_index != present_queue_family_index )
	{
		queue_count = 2;

		device_queue_create_infos[1] = {};
		device_queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		device_queue_create_infos[1].queueFamilyIndex = present_queue_family_index;
		device_queue_create_infos[1].queueCount = 1;
		device_queue_create_infos[1].pQueuePriorities = &queue_priority;
	}

	VkPhysicalDeviceFeatures device_features = {};

	VkDeviceCreateInfo device_create_info = {};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.queueCreateInfoCount = queue_count;
	device_create_info.pQueueCreateInfos = device_queue_create_infos;
	device_create_info.pEnabledFeatures = &device_features;
	const char* enabled_device_extension_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	device_create_info.enabledExtensionCount = sizeof( enabled_device_extension_names ) / sizeof( enabled_device_extension_names[0] );
	device_create_info.ppEnabledExtensionNames = enabled_device_extension_names;

	result = vkCreateDevice( physical_device, &device_create_info, 0, &out_state->device );
	assert( result == VK_SUCCESS );
	
	vkGetDeviceQueue( out_state->device, graphics_queue_family_index, 0, &out_state->graphics_queue );

	if( graphics_queue_family_index != present_queue_family_index )
	{
		vkGetDeviceQueue( out_state->device, present_queue_family_index, 0, &out_state->present_queue );
	}
	else
	{
		out_state->present_queue = out_state->graphics_queue;
	}

	VkSwapchainCreateInfoKHR swapchain_create_info = {};
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = surface;
	swapchain_create_info.minImageCount = swapchain_image_count;
	swapchain_create_info.imageFormat = swapchain_surface_format.format;
	swapchain_create_info.imageColorSpace = swapchain_surface_format.colorSpace;
	swapchain_create_info.imageExtent = swapchain_extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32 queue_family_indices[] = {graphics_queue_family_index, present_queue_family_index};
	if( queue_count > 1 ) 
	{
	    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // todo( jbr ) use exclusive and transfer owenership properly
	    swapchain_create_info.queueFamilyIndexCount = 2;
	    swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
	} 
	else 
	{
	    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = swapchain_present_mode;
	swapchain_create_info.clipped = VK_TRUE;

	result = vkCreateSwapchainKHR( out_state->device, &swapchain_create_info, 0, &out_state->swapchain );
	assert( result == VK_SUCCESS );

	// implementation can create more than the number requested, so get the image count again here
	result = vkGetSwapchainImagesKHR( out_state->device, out_state->swapchain, &swapchain_image_count, 0 );
	assert( result == VK_SUCCESS );
	
	VkImage* swapchain_images = (VkImage*)alloc_temp(sizeof(VkImage) * swapchain_image_count);
	result = vkGetSwapchainImagesKHR( out_state->device, out_state->swapchain, &swapchain_image_count, swapchain_images );
	assert( result == VK_SUCCESS );

	VkImageView* swapchain_image_views = (VkImageView*)alloc_temp(sizeof(VkImageView) * swapchain_image_count);
	
	VkImageViewCreateInfo image_view_create_info = {};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = swapchain_surface_format.format;
	image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount = 1;
	for( uint32 i = 0; i <  swapchain_image_count; ++i )
	{
		image_view_create_info.image = swapchain_images[i];
	
		result = vkCreateImageView( out_state->device, &image_view_create_info, 0, &swapchain_image_views[i] );
		assert( result == VK_SUCCESS );
	}

	// load shaders
	HANDLE file = CreateFileA("data/vert.spv", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	assert(file != INVALID_HANDLE_VALUE);
	DWORD vert_shader_size = GetFileSize(file, 0);
	assert(vert_shader_size != INVALID_FILE_SIZE);
	uint8* vert_shader_bytes = alloc_temp(vert_shader_size);
	bool32 read_success = ReadFile(file, vert_shader_bytes, vert_shader_size, 0, 0);
	assert(read_success);

	file = CreateFileA("data/frag.spv", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	assert(file != INVALID_HANDLE_VALUE);
	DWORD frag_shader_size = GetFileSize(file, 0);
	assert(frag_shader_size != INVALID_FILE_SIZE);
	uint8* frag_shader_bytes = alloc_temp(frag_shader_size);
	read_success = ReadFile(file, frag_shader_bytes, frag_shader_size, 0, 0);
	assert(read_success);

	VkShaderModuleCreateInfo shader_module_create_info = {};
	shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_create_info.codeSize = vert_shader_size;
	shader_module_create_info.pCode = (uint32_t*)vert_shader_bytes;
	VkShaderModule vert_shader_module;
	result = vkCreateShaderModule(out_state->device, &shader_module_create_info, 0, &vert_shader_module);
	assert(result == VK_SUCCESS);

	shader_module_create_info = {};
	shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_create_info.codeSize = frag_shader_size;
	shader_module_create_info.pCode = (uint32_t*)frag_shader_bytes;
	VkShaderModule frag_shader_module;
	result = vkCreateShaderModule(out_state->device, &shader_module_create_info, 0, &frag_shader_module);
	assert(result == VK_SUCCESS);

	VkPipelineShaderStageCreateInfo shader_stage_create_info[2];
	shader_stage_create_info[0] = {};
	shader_stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stage_create_info[0].module = vert_shader_module;
	shader_stage_create_info[0].pName = "main";
	
	shader_stage_create_info[1] = {};
	shader_stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stage_create_info[1].module = frag_shader_module;
	shader_stage_create_info[1].pName = "main";

	VkVertexInputBindingDescription vertex_input_binding_desc = {};
	vertex_input_binding_desc.binding = 0;
	vertex_input_binding_desc.stride = sizeof(Vertex);
	vertex_input_binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertex_input_attribute_desc[2];
	vertex_input_attribute_desc[0] = {};
	vertex_input_attribute_desc[0].binding = 0;
	vertex_input_attribute_desc[0].location = 0;
	vertex_input_attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_input_attribute_desc[0].offset = 0;
	vertex_input_attribute_desc[1] = {};
	vertex_input_attribute_desc[1].binding = 0;
	vertex_input_attribute_desc[1].location = 1;
	vertex_input_attribute_desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_input_attribute_desc[1].offset = 8;

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &vertex_input_binding_desc;
	vertex_input_info.vertexAttributeDescriptionCount = 2;
	vertex_input_info.pVertexAttributeDescriptions = vertex_input_attribute_desc;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
	input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchain_extent.width;
	viewport.height = (float)swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapchain_extent;

	VkPipelineViewportStateCreateInfo viewport_create_info = {};
	viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_create_info.viewportCount = 1;
	viewport_create_info.pViewports = &viewport;
	viewport_create_info.scissorCount = 1;
	viewport_create_info.pScissors = &scissor;

 	VkPipelineRasterizationStateCreateInfo rasteriser_create_info = {};
 	rasteriser_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
 	rasteriser_create_info.polygonMode = VK_POLYGON_MODE_FILL;
 	rasteriser_create_info.lineWidth = 1.0f;
 	rasteriser_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
 	rasteriser_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisampling_create_info = {};
	multisampling_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colour_blend_attachment = {};
	colour_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colour_blend_state_create_info = {};
	colour_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colour_blend_state_create_info.attachmentCount = 1;
	colour_blend_state_create_info.pAttachments = &colour_blend_attachment;

	VkDescriptorSetLayoutBinding descriptor_set_layout_binding = {};
    descriptor_set_layout_binding.binding = 0;
    descriptor_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_set_layout_binding.descriptorCount = 1;
    descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descriptor_set_layout_binding.pImmutableSamplers = 0;

	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
	descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_layout_create_info.bindingCount = 1;
	descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;

	VkDescriptorSetLayout descriptor_set_layout;
	result = vkCreateDescriptorSetLayout(out_state->device, &descriptor_set_layout_create_info, 0, &descriptor_set_layout);
	assert(result == VK_SUCCESS);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;

	VkPipelineLayout pipeline_layout;
	result = vkCreatePipelineLayout(out_state->device, &pipeline_layout_create_info, 0, &pipeline_layout);
	assert(result == VK_SUCCESS);

	VkAttachmentDescription colour_attachment = {};
    colour_attachment.format = swapchain_surface_format.format;
    colour_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colour_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colour_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colour_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colour_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colour_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colour_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colour_attachment_ref = {};
	colour_attachment_ref.attachment = 0;
	colour_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass_desc = {};
	subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_desc.colorAttachmentCount = 1;
	subpass_desc.pColorAttachments = &colour_attachment_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_create_info = {};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pAttachments = &colour_attachment;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass_desc;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &dependency;

	VkRenderPass render_pass;
	result = vkCreateRenderPass(out_state->device, &render_pass_create_info, 0, &render_pass);
	assert(result == VK_SUCCESS);

	VkGraphicsPipelineCreateInfo pipeline_create_info = {};
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.stageCount = 2;
	pipeline_create_info.pStages = &shader_stage_create_info[0];
	pipeline_create_info.pVertexInputState = &vertex_input_info;
	pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
	pipeline_create_info.pViewportState = &viewport_create_info;
	pipeline_create_info.pRasterizationState = &rasteriser_create_info;
	pipeline_create_info.pMultisampleState = &multisampling_create_info;
	pipeline_create_info.pColorBlendState = &colour_blend_state_create_info;
	pipeline_create_info.layout = pipeline_layout;
	pipeline_create_info.renderPass = render_pass;
	pipeline_create_info.subpass = 0;
	
	VkPipeline graphics_pipeline;
	result = vkCreateGraphicsPipelines(out_state->device, 0, 1, &pipeline_create_info, 0, &graphics_pipeline);
	assert(result == VK_SUCCESS);
	
	VkFramebuffer* swapchain_framebuffers = (VkFramebuffer*)alloc_temp(sizeof(VkFramebuffer) * swapchain_image_count);
	for(uint32 i = 0; i < swapchain_image_count; ++i)
	{
		VkFramebufferCreateInfo framebuffer_create_info = {};
		framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_create_info.renderPass = render_pass;
		framebuffer_create_info.attachmentCount = 1;
		framebuffer_create_info.pAttachments = &swapchain_image_views[i];
		framebuffer_create_info.width = swapchain_extent.width;
		framebuffer_create_info.height = swapchain_extent.height;
		framebuffer_create_info.layers = 1;

		result = vkCreateFramebuffer(out_state->device, &framebuffer_create_info, 0, &swapchain_framebuffers[i]);
		assert(result == VK_SUCCESS);
	}

	// Create vertex buffer
	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = num_vertices * sizeof(Vertex);
	buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer vertex_buffer;
	create_buffer(physical_device, out_state->device, &buffer_create_info, &vertex_buffer, &out_state->vertex_buffer_memory);

	// Create index buffer
	const uint32 c_index_buffer_size = num_indices * sizeof(indices[0]);
	buffer_create_info.size = c_index_buffer_size;
	buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;
	create_buffer(physical_device, out_state->device, &buffer_create_info, &index_buffer, &index_buffer_memory);

	copy_to_buffer(out_state->device, index_buffer_memory, (void*)indices, c_index_buffer_size);

	// create projection matrix
	// Note: Vulkan NDC coordinates are top-left corner (-1, -1), z 0-1
	// 1/tan(fovx/2) 	0	0				0
	// 0				0	-1/tan(fovy/2)	0
	// 0				-c2	0				c1
	// 0				-1	0				0
	// this is stored column major
	// NDC Z = c1/w + c2
	// c1 = (near*far)/(near-far)
	// c2 = far/(far-near)
	float32 aspect_ratio = window_width / (float32)window_height;
	float32 fov_x = aspect_ratio * tanf(fov_y * 0.5f) * 2.0f;
	out_state->projection_matrix[0] = 1.0f / tanf(fov_x * 0.5f);
	out_state->projection_matrix[6] = -far_plane / (far_plane - near_plane);
	out_state->projection_matrix[7] = -1.0f;
	out_state->projection_matrix[9] = -1.0f / tanf(fov_y * 0.5f);
	out_state->projection_matrix[14] = (near_plane * far_plane) / (near_plane - far_plane);
	const uint32 c_projection_matrix_data_size = sizeof(float32) * 16;

	buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.pNext = NULL;
	buffer_create_info.flags = 0;
    buffer_create_info.size = c_projection_matrix_data_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = 0;
	
	create_buffer(	physical_device, 
					out_state->device, 
					&buffer_create_info, 
					&out_state->projection_matrix_buffer, 
					&out_state->projection_matrix_buffer_memory);

	copy_to_buffer(	out_state->device, 
					out_state->projection_matrix_buffer_memory, 
					(void*)out_state->projection_matrix, 
					c_projection_matrix_data_size);


	// Create command buffers
	VkCommandPoolCreateInfo command_pool_create_info = {};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = graphics_queue_family_index;

	VkCommandPool command_pool;
	vkCreateCommandPool(out_state->device, &command_pool_create_info, 0, &command_pool);

	out_state->command_buffers = (VkCommandBuffer*)alloc_permanent(sizeof(VkCommandBuffer) * swapchain_image_count);
	VkCommandBufferAllocateInfo command_buffer_alloc_info = {};
	command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_alloc_info.commandPool = command_pool;
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = swapchain_image_count;

	result = vkAllocateCommandBuffers(out_state->device, &command_buffer_alloc_info, out_state->command_buffers);
	assert(result == VK_SUCCESS);

	VkClearValue clear_colour = {0.0f, 0.0f, 0.0f, 1.0f};
	for (uint32 i = 0; i < swapchain_image_count; ++i)
	{
	    VkCommandBufferBeginInfo command_buffer_begin_info = {};
	    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	    vkBeginCommandBuffer(out_state->command_buffers[i], &command_buffer_begin_info);

	    VkRenderPassBeginInfo render_pass_begin_info = {};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = render_pass;
		render_pass_begin_info.framebuffer = swapchain_framebuffers[i];
		render_pass_begin_info.renderArea.offset = {0, 0};
		render_pass_begin_info.renderArea.extent = swapchain_extent;
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_colour;
		vkCmdBeginRenderPass(out_state->command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(out_state->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(out_state->command_buffers[i], 0, 1, &vertex_buffer, &offset);
		vkCmdBindIndexBuffer(out_state->command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT16);

		vkCmdDrawIndexed(out_state->command_buffers[i], num_indices, 1, 0, 0, 0);

		vkCmdEndRenderPass(out_state->command_buffers[i]);

		result = vkEndCommandBuffer(out_state->command_buffers[i]);
		assert(result == VK_SUCCESS);
	}

	VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
	result = vkCreateSemaphore(out_state->device, &semaphore_create_info, 0, &out_state->image_available_semaphore);
	assert(result == VK_SUCCESS);

	result = vkCreateSemaphore(out_state->device, &semaphore_create_info, 0, &out_state->render_finished_semaphore);
	assert(result == VK_SUCCESS);
}

void update_and_draw(Vertex* vertices, uint32 num_vertices, State* state)
{
	const uint32 c_vertex_data_size = num_vertices * sizeof(vertices[0]);
	copy_to_buffer(state->device, state->vertex_buffer_memory, (void*)vertices, c_vertex_data_size);

	uint32 image_index;
    VkResult result = vkAcquireNextImageKHR(state->device, state->swapchain, (uint64)-1, state->image_available_semaphore, 0, &image_index);
    assert(result == VK_SUCCESS);

    VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &state->image_available_semaphore;
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &state->command_buffers[image_index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &state->render_finished_semaphore;
	result = vkQueueSubmit(state->graphics_queue, 1, &submit_info, 0);
	assert(result == VK_SUCCESS);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &state->render_finished_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &state->swapchain;
	present_info.pImageIndices = &image_index;
	result = vkQueuePresentKHR(state->present_queue, &present_info);
	assert(result == VK_SUCCESS);
}

} // namespace Graphics