#include <windows.h>
#include <stdint.h>

// TODO: This is a global for now
static bool Running;

struct win32_offscreen_buffer {
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

static win32_offscreen_buffer globalBackBuffer;


static win32_window_dimension Win32GetWindowDimension(HWND Window) {
	win32_window_dimension result;

	RECT clientRect;
	GetClientRect(Window, &clientRect);
	result.Width = clientRect.right - clientRect.left;
	result.Height = clientRect.bottom - clientRect.top;

	return result;
}

static void RenderWeirdGradient(win32_offscreen_buffer buffer, int BlueOffset, int GreenOffset) {
	int width = buffer.Width;
	uint8_t *row = (uint8_t *)buffer.Memory;
	for (int y = 0; y < buffer.Height; ++y) {
		uint32_t *pixel = (uint32_t *)row;
		for (int x = 0; x < buffer.Width; ++x) {
			uint8_t blue = x + BlueOffset;
			uint8_t green = y + GreenOffset;
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer.Pitch;
	}
}

static void Win32ResizeDIBSection(win32_offscreen_buffer& buffer, int Width, int Height) {

	if (buffer.Memory) {
		VirtualFree(buffer.Memory, 0, MEM_RELEASE);
	}

	buffer.Width = Width;
	buffer.Height = Height;
	buffer.BytesPerPixel = 4;


	buffer.Info.bmiHeader.biSize = sizeof(buffer.Info.bmiHeader);
	buffer.Info.bmiHeader.biWidth = buffer.Width;
	buffer.Info.bmiHeader.biHeight = -buffer.Height;
	buffer.Info.bmiHeader.biPlanes = 1;
	buffer.Info.bmiHeader.biBitCount = 32;
	buffer.Info.bmiHeader.biCompression = BI_RGB;
	
	
	int bitmapMemorySize = (buffer.Width * buffer.Height) * buffer.BytesPerPixel;
	buffer.Memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	buffer.Pitch = Width * buffer.BytesPerPixel;
	//RenderWeirdGradient(0, 0);
}

static void Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer buffer, int windowWidth, int windowHeight, int X, int Y, int Width, int Height) {
	StretchDIBits(DeviceContext,
		/*X, Y, Width, Height,
		X, Y, Width, Height, */
		0, 0, windowWidth, windowHeight,
		0, 0, buffer.Width, buffer.Height,
		buffer.Memory,
		&buffer.Info,
		DIB_RGB_COLORS,
		SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;
	switch (Message){
		case WM_SIZE:
		{
			//win32_window_dimension dimension = Win32GetWindowDimension(Window);
			//Win32ResizeDIBSection(globalBackBuffer, dimension.Width, dimension.Height);
		} break;

		case WM_DESTROY:
		{
			Running = false;
		} break;

		case WM_CLOSE:
		{	
			Running = false;
		} break;

		case WM_ACTIVATEAPP:
		{
		
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC deviceContext = BeginPaint(Window, &Paint);
			int x = Paint.rcPaint.left;
			int y = Paint.rcPaint.top;
			int width = Paint.rcPaint.right - Paint.rcPaint.left;
			int height = Paint.rcPaint.bottom - Paint.rcPaint.top;



			win32_window_dimension dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(deviceContext, globalBackBuffer, dimension.Width, dimension.Height, x, y, width, height);
			EndPaint(Window, &Paint);
		} break;
		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return Result;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode) {
	WNDCLASS WindowClass = {};

	Win32ResizeDIBSection(globalBackBuffer, 1280, 720);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClassA(&WindowClass)) {
		HWND Window = CreateWindowExA(
				0, 
				WindowClass.lpszClassName, 
				"Handmade Hero",
				WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
				CW_USEDEFAULT, 
				CW_USEDEFAULT, 
				CW_USEDEFAULT, 
				CW_USEDEFAULT,
				0, 
				0, 
				Instance, 
				0);

		if (Window) {
			Running = true;
			int XOffset = 0;
			int YOffset = 0;
			while (Running) {
				MSG Message;
				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}

					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}

				RenderWeirdGradient(globalBackBuffer, XOffset++, YOffset++);

				HDC DeviceContext = GetDC(Window);
				win32_window_dimension dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, globalBackBuffer, dimension.Width, dimension.Height, 0, 0, dimension.Width, dimension.Height);
				ReleaseDC(Window, DeviceContext);
			}
			
		} else {
			// TODO: Logging
		}
	} else {
		// TODO: Logging
	}
	return 0;
}