#include <stdio.h>
#include <vulkan\vulkan.h>
#include <windows.h>

typedef int bool32;

static bool32 g_is_running;

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
	char buffer[512];
	snprintf( buffer, sizeof( buffer ), "Vulkan:[%s]%s\n", layerPrefix, msg );
	OutputDebugStringA( buffer );

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


	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstance vulkan_instance; // todo( jbr ) custom allocator
	{
		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		#ifndef RELEASE
		create_info.enabledLayerCount = 1;
		const char* validation_layers[] = {"VK_LAYER_LUNARG_standard_validation"};
		create_info.ppEnabledLayerNames = validation_layers;
		#endif
		const char* extensions[] = {VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
		create_info.enabledExtensionCount = sizeof( extensions ) / sizeof( extensions[0] );
		create_info.ppEnabledExtensionNames = extensions;
		
		VkResult result = vkCreateInstance( &create_info, 0, &vulkan_instance );
		assert( result == VK_SUCCESS );
	}

	VkDebugReportCallbackEXT debug_callback;
	{
		VkDebugReportCallbackCreateInfoEXT create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		create_info.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		create_info.pfnCallback = vulkan_debug_callback;

		auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( vulkan_instance, "vkCreateDebugReportCallbackEXT" );
		assert( vkCreateDebugReportCallbackEXT );

		VkResult result = vkCreateDebugReportCallbackEXT( vulkan_instance, &create_info, 0, &debug_callback );
		assert( result == VK_SUCCESS );
	}

	uint32_t physical_device_count;
	{
		VkResult result = vkEnumeratePhysicalDevices( vulkan_instance, &physical_device_count, 0 );
		assert( result == VK_SUCCESS );
		assert( physical_device_count > 0 );
	}

	VkPhysicalDevice physical_device = 0;
	{
		// todo( jbr ) custom memory allocator
		VkPhysicalDevice* physical_devices = new VkPhysicalDevice[physical_device_count];

		VkResult result = vkEnumeratePhysicalDevices( vulkan_instance, &physical_device_count, physical_devices );
		assert( result == VK_SUCCESS );

		for( uint32_t i = 0; i < physical_device_count; ++i )
		{
			VkPhysicalDeviceProperties device_properties;
			//VkPhysicalDeviceFeatures device_features; TODO( jbr ) pick best device based on type and features

			vkGetPhysicalDeviceProperties( physical_devices[i], &device_properties );
			//vkGetPhysicalDeviceFeatures( physical_devices[i], &device_features );

			// for now just try to pick a discrete gpu, otherwise anything
			if( device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
			{
				physical_device = physical_devices[i];
				break;
			}
		}

		if( !physical_device )
		{
			physical_device = physical_devices[0];
		}

		delete[] physical_devices;
	}

	VkDevice logical_device = 0;
	{
		VkDeviceQueueCreateInfo device_queue_create_info = {};
		device_queue_create_info
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