#include <Windows.h>
#include <math.h>
#include <thread>
#include <iostream>

#include <d2d1.h>
#pragma comment (lib, "d2d1.lib")

//#include <d2d1helper.h>

//#include <dwrite.h>
//#pragma comment(lib, "dwrite.lib")

#include "offsets.h"
#include "rmem.h"

const int MAX_PLAYERS = 64;
#define WINDOW_NAME "Counter-Strike: Global Offensive"

// For "world to screen" matrix
struct Matrix4x4 {
	float _[16];
};

struct Vector3D {
	float x;
	float y;
	float z;
};

struct Vector2D {
	float x;
	float y;
};

struct LocalPlayer {
	DWORD dwBase;

	Vector3D posFeet;
	int health;
	int team;
	Matrix4x4 perspective;
};

struct EntityPlayer {
	DWORD dwBase;
	DWORD dwBoneBase;

	int health;
	int team;

	Vector3D posFeet;
	Vector3D posHead;
	float w;

	Vector2D screenFeet;
	Vector2D screenHead;

	float boxX0;
	float boxY0;
	float boxX1;
	float boxY1;

	D2D1_POINT_2F hpBar0;
	D2D1_POINT_2F hpBar1;
	float hpBarThic;

	bool visible;

	ID2D1SolidColorBrush* brush;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

bool InitOverlayWindow(HWND* hwnd, HINSTANCE hInstance);

// Repeatedly calls a function with specified delay after each call
void RepeatedlyCall(void(*func)(), int delay);

void Aimbot();

void WorldToScreen(Vector3D*, Vector2D*, float*);

void Render();

Vector3D GetBonePos(EntityPlayer* ent, int id);

bool InitD2D(HWND* overlayWindow);

namespace Visuals {
	inline void DrawESP();
}

namespace GAMEDATA {
	void UpdateCoordinates();
	void DrawBoxes();
	void UpdateHealth();
	void UpdateMisc();
};
