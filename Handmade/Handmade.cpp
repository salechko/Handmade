#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>

// TODO: This is a global for now
static bool Running;

struct win32_offscreen_buffer {
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static win32_offscreen_buffer globalBackBuffer;
static LPDIRECTSOUNDBUFFER globalSecondaryBuffer;

static void Win32LoadXInput() {
	HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	if (XInputLibrary) {
		XInputGetState = reinterpret_cast<x_input_get_state *>(GetProcAddress(XInputLibrary, "XInputGetState"));
		XInputSetState = reinterpret_cast<x_input_set_state *>(GetProcAddress(XInputLibrary, "XInputSetState"));
	}
}

static void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize) {
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if (DSoundLibrary) {
		direct_sound_create* directSoundCreate = reinterpret_cast<direct_sound_create *>(GetProcAddress(DSoundLibrary, "DirectSoundCreate"));

		LPDIRECTSOUND directSound;
		if (directSoundCreate && SUCCEEDED(directSoundCreate(0, &directSound, 0))) {
			WAVEFORMATEX waveFormat = {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = SamplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
			waveFormat.cbSize = 0;

			if (SUCCEEDED(directSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
				DSBUFFERDESC  bufferDescription = {};
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				bufferDescription.dwSize = sizeof(bufferDescription);

				LPDIRECTSOUNDBUFFER primaryBuffer;			
				if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0))) {	
					if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
						OutputDebugStringA("Primary buffer set");
					} else {
						OutputDebugStringA("Primary buffer error");
						// TODO: diagnostic
					}
				} else {
					// TODO: diagonstic
				}
			} else	{
				// TODO : Diagonstic
			}

			DSBUFFERDESC  bufferDescription = {};
			bufferDescription.dwFlags = 0;
			bufferDescription.dwSize = sizeof(bufferDescription);
			bufferDescription.dwBufferBytes = BufferSize;
			bufferDescription.lpwfxFormat = &waveFormat;


			if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &globalSecondaryBuffer, 0))) {
				OutputDebugStringA("Primary buffer set");
			}
			else {
				OutputDebugStringA("Secondary buffer error");
			}

		} else {
			// TODO: Diagnostic
		}
	}
}

static win32_window_dimension Win32GetWindowDimension(HWND Window) {
	win32_window_dimension result;

	RECT clientRect;
	GetClientRect(Window, &clientRect);
	result.Width = clientRect.right - clientRect.left;
	result.Height = clientRect.bottom - clientRect.top;

	return result;
}

static void RenderWeirdGradient(win32_offscreen_buffer& buffer, int BlueOffset, int GreenOffset) {
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
	int bytesPerPixel = 4;

	buffer.Info.bmiHeader.biSize = sizeof(buffer.Info.bmiHeader);
	buffer.Info.bmiHeader.biWidth = buffer.Width;
	buffer.Info.bmiHeader.biHeight = -buffer.Height;
	buffer.Info.bmiHeader.biPlanes = 1;
	buffer.Info.bmiHeader.biBitCount = 32;
	buffer.Info.bmiHeader.biCompression = BI_RGB;


	int bitmapMemorySize = (buffer.Width * buffer.Height) * bytesPerPixel;
	buffer.Memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	buffer.Pitch = Width * bytesPerPixel;
}

static void Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer& buffer, int windowWidth, int windowHeight) {
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

static LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;
	switch (Message) {
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

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			uint32_t vkCode = WParam;
			bool wasDown = ((LParam & (1 << 30)) != 0);
			bool isDown = ((LParam & (1 << 31)) == 0);
			if (wasDown != isDown) {
				if (vkCode == 'W') {

				}
				else if (vkCode == 'A') {

				}
				else if (vkCode == 'S') {

				}
				else if (vkCode == 'D') {

				}
				else if (vkCode == 'A') {

				}
				else if (vkCode == 'Q') {

				}
				else if (vkCode == 'E') {

				}
				else if (vkCode == VK_UP) {

				}
				else if (vkCode == VK_DOWN) {

				}
				else if (vkCode == VK_LEFT) {

				}
				else if (vkCode == VK_RIGHT) {

				}
				else if (vkCode == VK_ESCAPE) {

				}
				else if (vkCode == VK_ESCAPE) {

				}
			}

			bool altKeyWasDown = ((LParam & (1 << 29)) != 0);
			if ((vkCode == VK_F4) && altKeyWasDown){
				Running = false;
			}

		}break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC deviceContext = BeginPaint(Window, &Paint);
			int x = Paint.rcPaint.left;
			int y = Paint.rcPaint.top;
			int width = Paint.rcPaint.right - Paint.rcPaint.left;
			int height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			win32_window_dimension dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(deviceContext, globalBackBuffer, dimension.Width, dimension.Height);
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
	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(globalBackBuffer, 1280, 720);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
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
			HDC DeviceContext = GetDC(Window);

			// graphic test
			int XOffset = 0;
			int YOffset = 0;

			// sound test
			int toneHz = 256;
			int toneVolume = 1000;
			int samplesPerSecond = 48000;
			int bytesPerSample = sizeof(int16_t) * 2;
			uint32_t runningSampleIndex = 0;
			int squareWavePeriod = samplesPerSecond / toneHz;
			int halfSquareWavePriod = squareWavePeriod / 2;
			int secondaryBufferSize = samplesPerSecond * bytesPerSample;

			Win32InitDSound(Window, samplesPerSecond, secondaryBufferSize);

			int isPlaying = false;
			

			Running = true;
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

				for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
					XINPUT_STATE controllerState;
					if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {
						XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
						bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
						bool start = pad->wButtons & XINPUT_GAMEPAD_START;
						bool back = pad->wButtons & XINPUT_GAMEPAD_BACK;
						bool leftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
						bool rightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
						bool aButton = pad->wButtons & XINPUT_GAMEPAD_A;
						bool bButton = pad->wButtons & XINPUT_GAMEPAD_B;
						bool xButton = pad->wButtons & XINPUT_GAMEPAD_X;
						bool yButton = pad->wButtons & XINPUT_GAMEPAD_Y;

						int16_t stickX = pad->sThumbLX;
						int16_t stickY = pad->sThumbLY;
					} else {

					}
				}

				RenderWeirdGradient(globalBackBuffer, XOffset++, YOffset++);
				
				// direct sound output test
				DWORD playCursor;
				DWORD writeCursor;
				if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
					DWORD bytesToLock = runningSampleIndex * bytesPerSample % secondaryBufferSize;
					DWORD bytesToWrite;
					if (bytesToLock == playCursor) {
						bytesToWrite = secondaryBufferSize;
					}
					else if (bytesToLock > playCursor) {
						bytesToWrite = secondaryBufferSize - bytesToLock;
						bytesToWrite += playCursor;
					} else {
						bytesToWrite = playCursor - bytesToLock;
					}

					void* region1;
					DWORD region1Size;
					void* region2;
					DWORD region2Size;
					
					if (SUCCEEDED(globalSecondaryBuffer->Lock(bytesToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
						int16_t* sampleOut = (int16_t *)region1;
						DWORD region1SampleCount = region1Size / bytesPerSample;
						for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
							int16_t sampleValue = ((runningSampleIndex++ / halfSquareWavePriod) % 2) ? toneVolume : -toneVolume;
							*sampleOut++ = sampleValue;
							*sampleOut++ = sampleValue;
						}

						sampleOut = (int16_t *)region2;
						DWORD region2SampleCount = region2Size / bytesPerSample;
						for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
							int16_t sampleValue = ((runningSampleIndex++ / halfSquareWavePriod) % 2) ? toneVolume : -toneVolume;
							*sampleOut++ = sampleValue;
							*sampleOut++ = sampleValue;
						}

						globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
					}
				}

				if (!isPlaying) {
					globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
					isPlaying = true;
				}

				win32_window_dimension dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, globalBackBuffer, dimension.Width, dimension.Height);
			}
			
		} else {
			// TODO: Logging
		}
	} else {
		// TODO: Logging
	}
	return 0;
}