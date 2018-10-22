#include <Windows.h>
#include <TlHelp32.h>
#include <math.h>
#include <thread>
#include <iostream>

#include <d2d1.h>
#pragma comment (lib, "d2d1.lib")

#include <d2d1helper.h>

//#include <dwrite.h>
//#pragma comment(lib, "dwrite.lib")

#include "offsets.h"
#include "datatypes.h"

const int MAX_PLAYERS = 64;
#define WINDOW_NAME "Counter-Strike: Global Offensive"

#define UNINITIALIZED 0xFFFFFFFF

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// Creates overlay window class and handle
bool InitOverlayWindow(HWND& hwnd, HINSTANCE& hInstance);

struct Process {
	HWND window;
	DWORD id;
	HANDLE handle;
	RECT wndRect;
};

// Returns base address of a process module
DWORD GetModuleBase(DWORD processId, const char* moduleName);

// Reads memory relative to CSGO processs
template <typename T>
T Read(DWORD addr);

// Repeatedly calls a function with specified delay after each call
void RepeatedlyCall(void(*func)(), int delay);

// ...
void Aimbot();

// Converts 3D position vector to 2D projection of it
inline void WorldToScreen(Vector3D&, float&, float&, float&);

void Render();

// Returns a bone 3D position vector
inline Vector3D GetBonePos(Entity& ent, int id);

bool InitD2D(HWND& overlayWindow);

namespace Drawings {
	inline void DrawESP();
}

namespace GAMEDATA {
	inline void UpdateCoordinates();
	inline void UpdateMisc();
}