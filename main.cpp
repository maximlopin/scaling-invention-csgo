#include "read-mem.h"

// D2D Render target
ID2D1HwndRenderTarget* g_pRenderTarget = NULL;

// D2D Brushes
namespace BRUSH {
	ID2D1SolidColorBrush* p_EnemyBrush = NULL;
	ID2D1SolidColorBrush* p_AllyBrush = NULL;
};

// client_panorama.dll base address
DWORD g_dwClientBase;

// Player
Player g_LocalPlayer;

// Array of players on server
Entity g_Entity[MAX_PLAYERS];

Process g_GameProcess;

// Game dimensions
int G_GAME_WIDTH;
int G_GAME_HEIGHT;

// Overlay window handle
HWND g_hOverlayWnd = NULL;

// Settings (can only be changed here since there is no GUI or an .ini)
const int g_PlayersOnServer = 10; // including yourself
const bool g_bStatusWH = true; // is ON?
const bool g_bStatusAimbot = true; // is ON?
const float g_fAimSmoothness = 0.09f; // the smaller the smoother
const float g_fAimFOV = 45.0f; // won't shoot at those who are out of screen range
const int AIM_TARGET_BONE = 10; // 10 is supposed to be the head i guess, but it targets the neck
const bool g_bRunning = true; // is hack running
const float g_AimScatter = 0.5f; // significance of aimbot randomness

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Create CSGO process handle
	while (!g_GameProcess.window) { g_GameProcess.window = FindWindow(0, WINDOW_NAME); Sleep(1000); }

	GetWindowThreadProcessId(g_GameProcess.window, &g_GameProcess.id);

	g_GameProcess.handle = OpenProcess(PROCESS_VM_READ, FALSE, g_GameProcess.id);
	if (!g_GameProcess.handle) { return 1; }

	// Get CSGO window size
	GetWindowRect(g_GameProcess.window, &g_GameProcess.wndRect);
	G_GAME_WIDTH = g_GameProcess.wndRect.right - g_GameProcess.wndRect.left;
	G_GAME_HEIGHT = g_GameProcess.wndRect.bottom - g_GameProcess.wndRect.top;

	// Create overlay window
	if (!InitOverlayWindow(g_hOverlayWnd, hInstance)) { return 1; };
	ShowWindow(g_hOverlayWnd, nCmdShow);

	// Initialize D2D 
	if (!InitD2D(g_hOverlayWnd)) { return 1; };

	// Get client_panorama.dll base address
	g_dwClientBase = GetModuleBase(g_GameProcess.id, "client_panorama.dll");

	// Get LocalPlayer base address
	g_LocalPlayer.baseAddr = Read<DWORD>(g_dwClientBase + OFFSET_LOCAL_PLAYER);

	// Get base addresses for all players on server
	for (int i = 0; i < g_PlayersOnServer; i++)
		g_Entity[i].baseAddr = Read<DWORD>(g_dwClientBase + OFFSET_ENTITY_LIST + (0x10 * i) + 0x10);

	// Thread reading essential game data
	std::thread updateCoordinates(
		RepeatedlyCall, GAMEDATA::UpdateCoordinates, 50
	);

	// Thread reading miscellaneous data that doesn't require being updated too frequently
	std::thread updMisc(
		RepeatedlyCall, GAMEDATA::UpdateMisc, 1000
	);

	// Render ESP stuff
	std::thread render(
		RepeatedlyCall, Render, 30
	);

	// Overlay window message loop
	MSG msg;
	while (!!GetMessage(&msg, NULL, 0, 0))
	{
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(100);
	}

	// Join threads
	updateCoordinates.join();
	updMisc.join();
	render.join();
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(1);
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
		break;
	}
	return 0;
}

bool InitOverlayWindow(HWND& hwnd, HINSTANCE& hInstance)
{
	WNDCLASSEX wc{};

	// Class settings
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = hInstance;
	wc.lpszClassName = "OverlayWindow";
	wc.lpfnWndProc = WindowProc;
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
	wc.lpszMenuName = 0;

	if (!RegisterClassEx(&wc)) { return false; }

	// Create window handle
	hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, 
		"OverlayWindow",
		"esp-overlay",
		WS_EX_TOPMOST | WS_POPUP,
		0, 
		0,
		G_GAME_WIDTH,
		G_GAME_HEIGHT,
		0,
		0,
		hInstance,
		0
	);
	if (!hwnd) { return false; }

	SetLayeredWindowAttributes(g_hOverlayWnd, RGB(0, 0, 0), 255, ULW_COLORKEY | LWA_ALPHA);
	return true;
}

void RepeatedlyCall(void(*func)(), int delay)
{
	while (g_bRunning)
	{
		func();
		Sleep(delay);
	}
}

namespace GAMEDATA {
	void UpdateCoordinates()
	{
		// LocalPlayer health
		g_LocalPlayer.health = Read<int>(g_LocalPlayer.baseAddr + OFFSET_HEALTH);

		// LocalPlayer perspective matrix
		g_LocalPlayer.perspective = Read<Matrix4x4>(g_dwClientBase + OFFSET_VIEW_MATRIX);

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			// Player feet coordinates
			g_Entity[i].posFeet = Read<Vector3D>(g_Entity[i].baseAddr + OFFSET_ORIGIN);

			// Player head coordinates
			g_Entity[i].posHead = GetBonePos(g_Entity[i], AIM_TARGET_BONE);

			// Player feet position on the screen
			WorldToScreen(g_Entity[i].posFeet, g_Entity[i].screenFeet.x, g_Entity[i].screenFeet.y, g_Entity[i].w);

			// Player head position on the screen
			WorldToScreen(g_Entity[i].posHead, g_Entity[i].screenHead.x, g_Entity[i].screenHead.y, g_Entity[i].w);

			// Player health
			g_Entity[i].health = Read<int>(g_Entity[i].baseAddr + OFFSET_HEALTH);

			// Is the player visible on the screen
			g_Entity[i].visible = (g_Entity[i].health != 0 && g_Entity[i].screenFeet.x < (float)G_GAME_WIDTH && g_Entity[i].screenFeet.x  > 0.0f && g_Entity[i].screenFeet.y < (float)G_GAME_HEIGHT && g_Entity[i].screenFeet.y > 0.0f && g_Entity[i].w >= 0.1f);
		}
		if (!!g_bStatusAimbot) { Aimbot(); }
	}
	void UpdateMisc()
	{
		// Local player team
		g_LocalPlayer.team = Read<int>(g_LocalPlayer.baseAddr + OFFSET_TEAM);

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			// Player team
			g_Entity[i].team = Read<int>(g_Entity[i].baseAddr + OFFSET_TEAM);

			// D2D brush color
			g_Entity[i].team == g_LocalPlayer.team ? g_Entity[i].brush = BRUSH::p_AllyBrush : g_Entity[i].brush = BRUSH::p_EnemyBrush;
		}

	}
}

namespace Drawings {
	inline void DrawESP()
	{
		for (int i = 0; i < g_PlayersOnServer; i++)
			if (g_Entity[i].visible)
			{
				float feetY = (g_Entity[i].screenFeet.y);
				float headY = (g_Entity[i].screenHead.y);
				float posX = (g_Entity[i].screenFeet.x);

				// Box height
				float height = fabs(feetY - headY);

				// Box padding
				float padding = height / 3;

				// Draw box
				g_pRenderTarget->DrawRectangle(
					D2D1::RectF(
						posX - padding,
						feetY,
						posX + padding,
						headY),
					g_Entity[i].brush
				);

				// Draw HP bar:

				float hpBarLength = (float)((float)g_Entity[i].health / 100.0f) * (float)(padding * 2);

				D2D1_POINT_2F point0;
				point0.x = posX - padding;
				point0.y = feetY + padding / 8;

				D2D1_POINT_2F point1;
				point1.x = posX - padding + (int)round(hpBarLength);
				point1.y = feetY + padding / 8;

				g_pRenderTarget->DrawLine(point0, point1, g_Entity[i].brush, padding / 4, 0);
			}
	}
}

void Render()
{
	// If CSGO window is not active then clear screen and don't render anything
	if (GetForegroundWindow() != g_GameProcess.window)
	{
		g_pRenderTarget->BeginDraw();
		g_pRenderTarget->Clear();
		g_pRenderTarget->EndDraw();
		Sleep(1000);
		return;
	}
	g_pRenderTarget->BeginDraw();
	g_pRenderTarget->Clear();

	if (g_bStatusWH)
		Drawings::DrawESP();

	g_pRenderTarget->EndDraw();	
}

inline void WorldToScreen(Vector3D& pos, float& screenX, float& screenY, float& _w)
{
	// Dot product of position vector and the view matrix
	float x = g_LocalPlayer.perspective._[0] * pos.x + g_LocalPlayer.perspective._[1] * pos.y + g_LocalPlayer.perspective._[2] * pos.z + g_LocalPlayer.perspective._[3];
	float y = g_LocalPlayer.perspective._[4] * pos.x + g_LocalPlayer.perspective._[5] * pos.y + g_LocalPlayer.perspective._[6] * pos.z + g_LocalPlayer.perspective._[7];
	float w = g_LocalPlayer.perspective._[12] * pos.x + g_LocalPlayer.perspective._[13] * pos.y + g_LocalPlayer.perspective._[14] * pos.z + g_LocalPlayer.perspective._[15];
	_w = w;

	screenX = (float)((G_GAME_WIDTH / 2) + (0.5 * x / w) * G_GAME_WIDTH + 0.5);
	screenY = (float)((G_GAME_HEIGHT / 2) - (0.5 * y / w) * G_GAME_HEIGHT + 0.5);
}

inline Vector3D GetBonePos(Entity& ent, int id)
{
	DWORD dwBoneBase = Read<DWORD>(ent.baseAddr + 0x2698);

	Vector3D bonePos;
	bonePos.x = Read<float>(dwBoneBase + id * 0x30 + 0xC);
	bonePos.y = Read<float>(dwBoneBase + id * 0x30 + 0x1C);
	bonePos.z = Read<float>(dwBoneBase + id * 0x30 + 0x2C);

	return bonePos;
}

void Aimbot()
{
	// If aimbot is turned off or CSGO is not the foreground window then don't do anything
	if (!g_bStatusAimbot || GetForegroundWindow() != g_GameProcess.window) { Sleep(2000); return; }

	if (GetKeyState(VK_LBUTTON) & 0x8000)
	{
		// Index of target to shoot
		int idx = 0;
		float leastError = sqrt(pow(g_Entity[idx].screenHead.x - G_GAME_WIDTH / 2, 2) + pow(g_Entity[idx].screenHead.y - G_GAME_HEIGHT, 2));
		
		bool found = false;

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			// If it's not visible on the screen
			if (!g_Entity[i].visible) continue;

			// If it's a teammate
			if (g_Entity[i].team == g_LocalPlayer.team) continue;

			found = true;

			float currError = sqrt(pow(g_Entity[i].screenHead.x - G_GAME_WIDTH / 2, 2) + pow(g_Entity[i].screenHead.y - G_GAME_HEIGHT / 2, 2));

			if (currError < leastError) { leastError = currError; idx = i; }
		}

		// If couldn't find a decent target or the target is out of FOV
		if (!found || leastError > g_fAimFOV) return;

		float dX = g_Entity[idx].screenHead.x - G_GAME_WIDTH / 2;
		float dY = g_Entity[idx].screenHead.y - G_GAME_HEIGHT / 2;

		int randX = (int)round(fabs(dX * g_AimScatter));
		int randY = (int)round(fabs(dY * g_AimScatter));

		randX == 0 ? dX = 0 : dX += rand() % randX - randX / 2;
		randY == 0 ? dY = 0 : dY += rand() % randY - randY / 2;

		POINT currPos;
		GetCursorPos(&currPos);
		SetCursorPos(currPos.x + (int)round(dX * g_fAimSmoothness), currPos.y + (int)round(dY * g_fAimSmoothness));
	}
}

bool InitD2D(HWND& overlayWindow)
{
	// Get overlay window dimensions
	RECT rc;
	GetWindowRect(overlayWindow, &rc);

	// Create d2d factory
	ID2D1Factory* pD2DFactory = NULL;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
	if (!pD2DFactory) { return false; }

	// Render target dimentions
	D2D1_SIZE_U size = D2D1::SizeU(
		rc.right - rc.left,
		rc.bottom - rc.top
	);

	// Create render target
	pD2DFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(overlayWindow, size),
		&g_pRenderTarget
	);
	if (!g_pRenderTarget) { return false; }

	// Enemy brush
	g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &BRUSH::p_EnemyBrush);

	// Ally brush
	g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::ForestGreen), &BRUSH::p_AllyBrush);

	return true;
}

DWORD GetModuleBase(DWORD processId, const char* moduleName)
{
	DWORD dwModuleBase = UNINITIALIZED;

	// Make a snapshot of the process
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
	MODULEENTRY32 moduleEntry;
	moduleEntry.dwSize = sizeof(MODULEENTRY32);

	// Get first module
	if (!Module32First(hSnapshot, &moduleEntry))
	{
		exit(1);
	}

	if (!strcmp(moduleEntry.szModule, moduleName))
		dwModuleBase = (DWORD)(moduleEntry.modBaseAddr);

	// Find the module
	do
	{
		if (!strcmp(moduleEntry.szModule, moduleName))
			dwModuleBase = (DWORD)(moduleEntry.modBaseAddr);
	} while (Module32Next(hSnapshot, &moduleEntry) && dwModuleBase == UNINITIALIZED);

	if (dwModuleBase == UNINITIALIZED)
	{
		exit(1);
	}

	return dwModuleBase;
}

template <typename T>
T Read(DWORD addr)
{
	T _read;
	ReadProcessMemory(g_GameProcess.handle, (const void*)addr, &_read, sizeof(T), NULL);
	return _read;
}