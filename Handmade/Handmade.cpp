#include <Windows.h>

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	MessageBoxA(0, "This is cookie", "Handmade", MB_OK | MB_ICONINFORMATION);
	return 0;
}