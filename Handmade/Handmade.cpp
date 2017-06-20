#include <Windows.h>

// TODO: This is a global for now
static bool Running;
static BITMAPINFO bitmapInfo;
static void *bitmapMemory;
static HBITMAP bitmapHandle;
static HDC bitmapDeviceContext;


static void Win32ResizeDIBSection(int Width, int Height) {
	if (bitmapHandle) {
		DeleteObject(bitmapHandle);
	}
	if (!bitmapDeviceContext) {
		bitmapDeviceContext = CreateCompatibleDC(0);
	}
	
	bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
	bitmapInfo.bmiHeader.biWidth = Width;
	bitmapInfo.bmiHeader.biHeight = Height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;	
	
	bitmapHandle = CreateDIBSection(bitmapDeviceContext, &bitmapInfo, DIB_RGB_COLORS, &bitmapMemory, 0, 0);
}

static void Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height) {
	StretchDIBits(DeviceContext, X, Y, Width, Height, X, Y, Width, Height, bitmapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;
	switch (Message){
		case WM_SIZE:
		{
			RECT clientRect;
			GetClientRect(Window, &clientRect);
			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;
			Win32ResizeDIBSection(width, height);
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
			Win32UpdateWindow(deviceContext, x, y, width, height);
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

	WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClass(&WindowClass)) {
		HWND WindowHandle = CreateWindowEx(
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

		if (WindowHandle) {
			MSG Message;
			Running = true;
			while (Running) {
				BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
				if (MessageResult > 0) {
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				} else {
					break;
				}
			}
			
		} else {
			// TODO: Logging
		}
	} else {
		// TODO: Logging
	}
	return 0;
}