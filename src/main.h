#include <Windows.h>
#include <math.h>
#include <thread>
#include <iostream>

#include "offsets.h"
#include "rmem.h"
#include "graphics.h"

const int MAX_PLAYERS = 64;
#define WINDOW_NAME "Counter-Strike: Global Offensive"

// For "world to screen" matrix
struct Matrix4x4 {
	float _[16];
	float operator[] (int idx) const
	{
		return this->_[idx];
	}
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
	Matrix4x4 view;
};

struct EntityPlayer {
	DWORD dwBase;
	DWORD dwBoneBase;

	int health;
	int team;

	struct {
		float x0;
		float y0;
		float x1;
		float y1;
	} box;

	D2D1_POINT_2F hpBar0;
	D2D1_POINT_2F hpBar1;
	float hpBarThickness;

	bool visible;

	ID2D1SolidColorBrush* brush;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

bool InitOverlayWindow(HWND* hwnd, HINSTANCE hInstance);

// Repeatedly calls a function with specified delay after each call
void RepeatedlyCall(void(*func)(), int delay);
void WorldToScreen(Vector3D*, Vector2D*, float*, int, int);
void Render();

Vector3D GetBonePos(EntityPlayer* ent, int id);

bool InitD2D(HWND* overlayWindow);

void UpdateWndStatus();
void UpdateCoordinates();
void UpdateHealths();
void UpdateTeams();
