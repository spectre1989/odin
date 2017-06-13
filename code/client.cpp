#include <stdio.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan\vulkan.h>
#include <windows.h>

#include "common.cpp"

static bool32 g_is_running;
static FILE* g_log_file;

#ifndef RELEASE
#define assert( x ) if( !( x ) ) { MessageBoxA( 0, #x, "Debug Assertion Failed", MB_OK ); }
#endif

// the return value indicates whether the calling layer should abort the vulkan call
static VkBool32 vulkan_debug_callback( 	VkDebugReportFlagsEXT /*flags*/, 
										VkDebugReportObjectTypeEXT /*objType*/, uint64_t /*obj*/, 
										size_t /*location*/, int32_t /*code*/, 
										const char* layerPrefix, const char* msg, void* /*userData*/) 
{
	// todo( jbr ) logging system
	if( !g_log_file )
	{
		errno_t error = fopen_s( &g_log_file, "log.txt", "w" );
		assert( !error );
	}

	fprintf( g_log_file, "Vulkan:[%s]%s\n", layerPrefix, msg );

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

	VkMemoryAllocateInfo memory_allocate_info = {};
	memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = chosen_memory_type_index;

	VkDeviceMemory buffer_memory;
	result = vkAllocateMemory(device, &memory_allocate_info, 0, &buffer_memory);
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

LRESULT CALLBACK WindowProc( HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param )
{
	switch( message )
	{
		case WM_QUIT:
		case WM_DESTROY:
			g_is_running = 0;
		break;
	}

	return DefWindowProc( window_handle, message, w_param, l_param );
}

int CALLBACK WinMain( HINSTANCE instance, HINSTANCE /*prev_instance*/, LPSTR /*cmd_line*/, int cmd_show )
{
	WNDCLASS window_class;
	window_class.style = 0;
	window_class.lpfnWndProc = WindowProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = 0;
	window_class.hbrBackground = 0;
	window_class.lpszMenuName = 0;
	window_class.lpszClassName = "app_window_class";

	ATOM window_class_atom = RegisterClass( &window_class );

	assert( window_class_atom );


	uint32 window_width = 1280;
	uint32 window_height = 720;

	HWND window_handle;
	{
		LPCSTR 	window_name 	= "";
		DWORD 	style 			= WS_OVERLAPPED;
		int 	x 				= CW_USEDEFAULT;
		int 	y 				= 0;
		HWND 	parent_window 	= 0;
		HMENU 	menu 			= 0;
		LPVOID 	param 			= 0;

		window_handle 			= CreateWindow( window_class.lpszClassName, window_name, style, x, y, window_width, window_height, parent_window, menu, instance, param );

		assert( window_handle );
	}

	ShowWindow( window_handle, cmd_show );

	
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
	
	// todo( jbr ) custom memory allocator
	VkPhysicalDevice* physical_devices = new VkPhysicalDevice[physical_device_count];

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

			// todo( jbr ) custom memory allocator
			VkExtensionProperties* device_extensions = new VkExtensionProperties[extension_count];
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
			
			delete[] device_extensions;

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
					// todo( jbr ) custom allocator
					VkSurfaceFormatKHR* surface_formats = new VkSurfaceFormatKHR[format_count];
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

				    delete[] surface_formats;

					VkPresentModeKHR* present_modes = new VkPresentModeKHR[present_mode_count];
				    result = vkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &present_mode_count, present_modes );
				    assert( result == VK_SUCCESS );

				    for( uint32 j = 0; j < present_mode_count; ++j )
				    {
				    	if( present_modes[j] == VK_PRESENT_MODE_MAILBOX_KHR )
				    	{
				    		swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				    	}
				    }

				    delete[] present_modes;

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

	delete[] physical_devices;

	uint32 queue_family_count;
	vkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, 0 );
	assert( queue_family_count > 0 );

	// todo( jbr ) custom memory allocator
	VkQueueFamilyProperties* queue_families = new VkQueueFamilyProperties[queue_family_count];

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

	delete[] queue_families;

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

	VkDevice device;
	result = vkCreateDevice( physical_device, &device_create_info, 0, &device );
	assert( result == VK_SUCCESS );
	
	VkQueue graphics_queue;
	VkQueue present_queue;
	vkGetDeviceQueue( device, graphics_queue_family_index, 0, &graphics_queue );

	if( graphics_queue_family_index != present_queue_family_index )
	{
		vkGetDeviceQueue( device, present_queue_family_index, 0, &present_queue );
	}
	else
	{
		present_queue = graphics_queue;
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

	VkSwapchainKHR swapchain;
	result = vkCreateSwapchainKHR( device, &swapchain_create_info, 0, &swapchain );
	assert( result == VK_SUCCESS );

	// implementation can create more than the number requested, so get the image count again here
	result = vkGetSwapchainImagesKHR( device, swapchain, &swapchain_image_count, 0 );
	assert( result == VK_SUCCESS );
	
	VkImage* swapchain_images = new VkImage[swapchain_image_count]; // todo( jbr ) custom allocator
	result = vkGetSwapchainImagesKHR( device, swapchain, &swapchain_image_count, swapchain_images );
	assert( result == VK_SUCCESS );

	VkImageView* swapchain_image_views = new VkImageView[swapchain_image_count]; // todo( jbr ) custom allocator
	
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
	
		result = vkCreateImageView( device, &image_view_create_info, 0, &swapchain_image_views[i] );
		assert( result == VK_SUCCESS );
	}

	// load shaders
	HANDLE file = CreateFileA("data/vert.spv", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	assert(file != INVALID_HANDLE_VALUE);
	DWORD vert_shader_size = GetFileSize(file, 0);
	assert(vert_shader_size != INVALID_FILE_SIZE);
	uint8* vert_shader_bytes = new uint8[vert_shader_size]; // todo(jbr) custom allocator
	bool32 read_success = ReadFile(file, vert_shader_bytes, vert_shader_size, 0, 0);
	assert(read_success);

	file = CreateFileA("data/frag.spv", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	assert(file != INVALID_HANDLE_VALUE);
	DWORD frag_shader_size = GetFileSize(file, 0);
	assert(frag_shader_size != INVALID_FILE_SIZE);
	uint8* frag_shader_bytes = new uint8[frag_shader_size]; // todo(jbr) custom allocator
	read_success = ReadFile(file, frag_shader_bytes, frag_shader_size, 0, 0);
	assert(read_success);

	VkShaderModuleCreateInfo shader_module_create_info = {};
	shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_create_info.codeSize = vert_shader_size;
	shader_module_create_info.pCode = (uint32_t*)vert_shader_bytes;
	VkShaderModule vert_shader_module;
	result = vkCreateShaderModule(device, &shader_module_create_info, 0, &vert_shader_module);
	assert(result == VK_SUCCESS);

	shader_module_create_info = {};
	shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_create_info.codeSize = frag_shader_size;
	shader_module_create_info.pCode = (uint32_t*)frag_shader_bytes;
	VkShaderModule frag_shader_module;
	result = vkCreateShaderModule(device, &shader_module_create_info, 0, &frag_shader_module);
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

	struct Vertex
	{
		float pos_x;
		float pos_y;
		float col_r;
		float col_g;
		float col_b;
	};

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

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkPipelineLayout pipeline_layout;
	result = vkCreatePipelineLayout(device, &pipeline_layout_create_info, 0, &pipeline_layout);
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
	result = vkCreateRenderPass(device, &render_pass_create_info, 0, &render_pass);
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
	result = vkCreateGraphicsPipelines(device, 0, 1, &pipeline_create_info, 0, &graphics_pipeline);
	assert(result == VK_SUCCESS);
	
	VkFramebuffer* swapchain_framebuffers = new VkFramebuffer[swapchain_image_count]; // todo(jbr) custom allocator
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

		result = vkCreateFramebuffer(device, &framebuffer_create_info, 0, &swapchain_framebuffers[i]);
		assert(result == VK_SUCCESS);
	}

	// Create vertex buffer
	constexpr uint32 c_num_vertices = 4 * c_max_clients;
	Vertex vertices[c_num_vertices];
	constexpr uint32 c_vertex_data_size = c_num_vertices * sizeof(vertices[0]);

	for(uint32 i = 0; i < c_num_vertices; ++i)
	{
		// hdc y axis points down
		float x = 0.0f, y = 0.0f;
		uint32 type = i % 4;
		if(type == 0 || type == 1) x = -0.4f;
		else x = 0.4f;
		if(type == 1 || type == 2) y = 0.4f;
		else y = -0.4f;

		float g = 0.0f;
		if(type == 1 || type == 2) g = 1.0f;

		vertices[i] = {};
		vertices[i].pos_x = x;
		vertices[i].pos_y = y;
		vertices[i].col_r = 1.0f;
		vertices[i].col_g = g;
	}

	VkBufferCreateInfo buffer_create_info = {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = c_vertex_data_size;
	buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;
	create_buffer(physical_device, device, &buffer_create_info, &vertex_buffer, &vertex_buffer_memory);

	copy_to_buffer(device, vertex_buffer_memory, (void*)vertices, c_vertex_data_size);

	// Create index buffer
	constexpr uint32 c_num_indices = 6 * c_max_clients;
	uint16 indices[c_num_indices];
	constexpr uint32 c_index_data_size = c_num_indices * sizeof(indices[0]);
	
	for(uint16 index = 0, vertex = 0; index < c_num_indices; index += 6, vertex += 4)
	{
		// quads will be top left, bottom left, bottom right, top right
		indices[index] = vertex;				// 0
		indices[index + 1] = vertex + 2;		// 2
		indices[index + 2] = vertex + 1;		// 1
		indices[index + 3] = vertex;			// 0
		indices[index + 4] = vertex + 3;		// 3
		indices[index + 5] = vertex + 2;		// 2
	}

	buffer_create_info.size = c_index_data_size;
	buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;
	create_buffer(physical_device, device, &buffer_create_info, &index_buffer, &index_buffer_memory);

	copy_to_buffer(device, index_buffer_memory, (void*)indices, c_index_data_size);

	VkCommandPoolCreateInfo command_pool_create_info = {};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = graphics_queue_family_index;

	VkCommandPool command_pool;
	vkCreateCommandPool(device, &command_pool_create_info, 0, &command_pool);

	VkCommandBuffer* command_buffers = new VkCommandBuffer[swapchain_image_count]; // todo(jbr) custom allocator
	VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
	command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_allocate_info.commandPool = command_pool;
	command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_allocate_info.commandBufferCount = swapchain_image_count;

	result = vkAllocateCommandBuffers(device, &command_buffer_allocate_info, command_buffers);
	assert(result == VK_SUCCESS);

	VkClearValue clear_colour = {0.0f, 0.0f, 0.0f, 1.0f};
	for (uint32 i = 0; i < swapchain_image_count; ++i)
	{
	    VkCommandBufferBeginInfo command_buffer_begin_info = {};
	    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	    vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info);

	    VkRenderPassBeginInfo render_pass_begin_info = {};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = render_pass;
		render_pass_begin_info.framebuffer = swapchain_framebuffers[i];
		render_pass_begin_info.renderArea.offset = {0, 0};
		render_pass_begin_info.renderArea.extent = swapchain_extent;
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_colour;
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT16);

		vkCmdDrawIndexed(command_buffers[i], c_num_indices, 1, 0, 0, 0);

		vkCmdEndRenderPass(command_buffers[i]);

		result = vkEndCommandBuffer(command_buffers[i]);
		assert(result == VK_SUCCESS);
	}

	VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore image_available_semaphore;
	result = vkCreateSemaphore(device, &semaphore_create_info, 0, &image_available_semaphore);
	assert(result == VK_SUCCESS);

	VkSemaphore render_finished_semaphore;
	result = vkCreateSemaphore(device, &semaphore_create_info, 0, &render_finished_semaphore);
	assert(result == VK_SUCCESS);



	g_is_running = 1;
	while( g_is_running )
	{
		MSG message;
		UINT filter_min = 0;
		UINT filter_max = 0;
		UINT remove_message = PM_REMOVE;
		while( PeekMessage( &message, window_handle, filter_min, filter_max, remove_message ) )
		{
			TranslateMessage( &message );
			DispatchMessage( &message );
		}


		uint32 image_index;
	    result = vkAcquireNextImageKHR(device, swapchain, (uint64)-1, image_available_semaphore, 0, &image_index);
	    assert(result == VK_SUCCESS);

	    VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &image_available_semaphore;
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submit_info.pWaitDstStageMask = &wait_stage;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffers[image_index];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &render_finished_semaphore;
		result = vkQueueSubmit(graphics_queue, 1, &submit_info, 0);
		assert(result == VK_SUCCESS);

		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &render_finished_semaphore;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swapchain;
		present_info.pImageIndices = &image_index;
		result = vkQueuePresentKHR(present_queue, &present_info);
		assert(result == VK_SUCCESS);
	}

	// todo( jbr ) return wParam of WM_QUIT
	return 0;
}