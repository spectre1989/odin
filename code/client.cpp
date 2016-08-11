#include <windows.h>

typedef int bool32;

static bool32 g_is_running;

#ifndef RELEASE
#define assert( x ) if( !( x ) ) { MessageBoxA( 0, #x, "Debug Assertion Failed", MB_OK ); }
#endif

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