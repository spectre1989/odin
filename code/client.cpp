#include <stdio.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan\vulkan.h>
#include <windows.h>

#include "odin.h"

typedef int bool32;

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


	HWND window_handle;
	{
		LPCSTR 	window_name 	= "";
		DWORD 	style 			= WS_OVERLAPPED;
		int 	x 				= CW_USEDEFAULT;
		int 	y 				= 0;
		int 	width 			= 1280;
		int 	height 			= 720;
		HWND 	parent_window 	= 0;
		HMENU 	menu 			= 0;
		LPVOID 	param 			= 0;

		window_handle 			= CreateWindow( window_class.lpszClassName, window_name, style, x, y, width, height, parent_window, menu, instance, param );

		assert( window_handle );
	}

	ShowWindow( window_handle, cmd_show );


	VkInstance vulkan_instance; // todo( jbr ) custom allocator
	VkDevice device;
	VkSurfaceKHR surface;
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
			}
		}

		if( !physical_device )
		{
			physical_device = physical_devices[0];
		}

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
		float32 queue_priority = 1.0f;

		assert( graphics_queue_family_index != uint32( -1 ) );
		assert( present_queue_family_index != uint32( -1 ) );

		delete[] queue_families;

		VkDeviceQueueCreateInfo device_queue_create_infos[2];
		device_queue_create_infos[0] = {};
		device_queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		device_queue_create_infos[0].queueFamilyIndex = graphics_queue_family_index;
		device_queue_create_infos[0].queueCount = 1;
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
	}

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
	}

	// todo( jbr ) return wParam of WM_QUIT
	return 0;
}