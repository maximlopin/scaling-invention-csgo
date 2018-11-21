#include "main.h"

ID2D1HwndRenderTarget* g_pRenderTarget = NULL;

namespace BRUSH {
	ID2D1SolidColorBrush* p_EnemyBrush = NULL;
	ID2D1SolidColorBrush* p_AllyBrush = NULL;
};

const int g_PlayersOnServer = 9; // excluding local player
bool g_bStatusWH = true;
bool g_bStatusAimbot = false;
const float g_fAimSmoothness = 0.09f; // the smaller the smoother
const float g_fAimFOV = 45.0f; // won't shoot at those who are out of screen range
const int AIM_TARGET_BONE = 10; // 10 is supposed to be the head i guess, but it targets the neck
const bool g_bRunning = true;
const float g_AimScatter = 0.5f; // significance of aimbot randomness

namespace KEY {
	bool bArrowUp;
	bool bArrowDown;
	bool bReturn;
};

namespace CSGO {
	HWND hWnd;
	DWORD procId;
	HANDLE procHandle;
	RMEM *mem;

	DWORD dwClientBase;

	int WIDTH;
	int HEIGHT;

	bool isForeground;
};

namespace OVERLAY {
	HWND hWnd;

	int WIDTH;
	int HEIGHT;
};

LocalPlayer g_LocalPlayer;

EntityPlayer g_Entity[MAX_PLAYERS];

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	while (!CSGO::hWnd)
		CSGO::hWnd = FindWindow(0, WINDOW_NAME); Sleep(1000);

	GetWindowThreadProcessId(CSGO::hWnd, &CSGO::procId);

	CSGO::procHandle = OpenProcess(PROCESS_VM_READ, FALSE, CSGO::procId);
	if (!CSGO::procHandle) 
		return 1;

	CSGO::mem = new RMEM(CSGO::procId, CSGO::procHandle);
	
	// Get CSGO window size
	RECT rc;
	GetWindowRect(CSGO::hWnd, &rc);
	CSGO::WIDTH = rc.right - rc.left;
	CSGO::HEIGHT = rc.bottom - rc.top;
	
	if (!InitOverlayWindow(&OVERLAY::hWnd, hInstance))
		return 1;

	ShowWindow(OVERLAY::hWnd, nCmdShow);
	
	if (!InitD2D(&OVERLAY::hWnd))
		return 1;
	
	CSGO::dwClientBase = CSGO::mem->GetModuleBase("client_panorama.dll");
	
	g_LocalPlayer.dwBase = CSGO::mem->Read<DWORD>(CSGO::dwClientBase + OFFSET_LOCAL_PLAYER);
	
	for (int i = 0; i < g_PlayersOnServer; i++)
	{
		g_Entity[i].dwBase = CSGO::mem->Read<DWORD>(CSGO::dwClientBase + OFFSET_ENTITY_LIST + (0x10 * i) + 0x10);

		g_Entity[i].dwBoneBase = CSGO::mem->Read<DWORD>(g_Entity[i].dwBase + OFFSET_BONE_MATRIX);
	}	
	
	std::thread UpdateCoordinates(
		RepeatedlyCall, GAMEDATA::UpdateCoordinates, 50
	);

	std::thread updMisc(
		RepeatedlyCall, GAMEDATA::UpdateMisc, 1000
	);

	std::thread DrawBoxes(
		RepeatedlyCall, GAMEDATA::DrawBoxes, 25
	);
	
	std::thread updateHealth(
		RepeatedlyCall, GAMEDATA::UpdateHealth, 100
	);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(25);
	}

	UpdateCoordinates.join();
	updMisc.join();
	updateHealth.join();
	DrawBoxes.join();

	delete CSGO::mem;
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KEYUP:
	{
		const char* msg = "Hello!";
		OutputDebugString(msg);
		break;
	}
	case WM_PAINT:
	{
		Render();
		break;
	}
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

bool InitOverlayWindow(HWND *hwnd, HINSTANCE hInstance)
{
	WNDCLASSEX wc{};

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

	*hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		"OverlayWindow",
		"",
		WS_EX_TOPMOST | WS_POPUP,
		0,
		0,
		CSGO::WIDTH,
		CSGO::HEIGHT,
		0,
		0,
		hInstance,
		0
	);
	if (!(*hwnd)) { return false; }

	SetLayeredWindowAttributes(OVERLAY::hWnd, RGB(0, 0, 0), 255, ULW_COLORKEY | LWA_ALPHA);
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
		
		// LocalPlayer perspective matrix
		g_LocalPlayer.perspective = CSGO::mem->Read<Matrix4x4>(CSGO::dwClientBase + OFFSET_VIEW_MATRIX);

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			// Player feet coordinates
			g_Entity[i].posFeet = CSGO::mem->Read<Vector3D>(g_Entity[i].dwBase + OFFSET_ORIGIN);

			// Coordinates of head
			g_Entity[i].posHead = GetBonePos(&g_Entity[i], AIM_TARGET_BONE);

			// Player feet position on the screen
			WorldToScreen(&g_Entity[i].posFeet, &g_Entity[i].screenFeet, &g_Entity[i].w);

			// Player head position on the screen
			WorldToScreen(&g_Entity[i].posHead, &g_Entity[i].screenHead, &g_Entity[i].w);

			// Is the player visible on the screen?
			g_Entity[i].visible = (g_Entity[i].health != 0 && g_Entity[i].w >= 0.1f);
		}
		
		// Aimbot must be syncronized with current coordinates so it's inside of UpdateCoordinates()
		if (g_bStatusAimbot) { Aimbot(); }
	}
	void UpdateHealth()
	{
		g_LocalPlayer.health = CSGO::mem->Read<int>(g_LocalPlayer.dwBase + OFFSET_HEALTH);

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			g_Entity[i].health = CSGO::mem->Read<int>(g_Entity[i].dwBase + OFFSET_HEALTH);
		}
	}
	void DrawBoxes()
	{
		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			// Box padding width
			float padding = fabs(g_Entity[i].screenFeet.y - g_Entity[i].screenHead.y) / 3;

			// Box coordinates on the screen
			g_Entity[i].boxX0 = g_Entity[i].screenFeet.x - padding;
			g_Entity[i].boxY0 = g_Entity[i].screenFeet.y;
			g_Entity[i].boxX1 = g_Entity[i].screenFeet.x + padding;
			g_Entity[i].boxY1 = g_Entity[i].screenHead.y;

			// Health bar coordinates on the screen
			float hpBarLength = (float)((float)g_Entity[i].health / 100.0f) * (float)(padding * 2);
			g_Entity[i].hpBar0.x = g_Entity[i].screenFeet.x - padding;;
			g_Entity[i].hpBar0.y = g_Entity[i].screenFeet.y + padding / 8;;
			g_Entity[i].hpBar1.x = g_Entity[i].screenFeet.x - padding + (int)round(hpBarLength);;
			g_Entity[i].hpBar1.y = g_Entity[i].screenFeet.y + padding / 8;

			// Health bar thickness
			g_Entity[i].hpBarThic = padding / 4;
		}
	}
	void UpdateMisc()
	{
		CSGO::isForeground = (GetForegroundWindow() == CSGO::hWnd);

		g_LocalPlayer.team = CSGO::mem->Read<int>(g_LocalPlayer.dwBase + OFFSET_TEAM);

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			g_Entity[i].team = CSGO::mem->Read<int>(g_Entity[i].dwBase + OFFSET_TEAM);

			// D2D brush color depending on team
			g_Entity[i].team == g_LocalPlayer.team ? g_Entity[i].brush = BRUSH::p_AllyBrush : g_Entity[i].brush = BRUSH::p_EnemyBrush;
		}

	}
}

namespace Visuals {
	inline void DrawESP()
	{
		for (int i = 0; i < g_PlayersOnServer; i++)
			if (g_Entity[i].visible)
			{
				// Draw box
				g_pRenderTarget->DrawRectangle(D2D1::RectF(g_Entity[i].boxX0, g_Entity[i].boxY0 ,g_Entity[i].boxX1, g_Entity[i].boxY1), g_Entity[i].brush);

				// Draw HP bar
				g_pRenderTarget->DrawLine(g_Entity[i].hpBar0, g_Entity[i].hpBar1, g_Entity[i].brush, g_Entity[i].hpBarThic, 0);
			}
	}
}

void Render()
{
	if (!CSGO::isForeground)
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
		Visuals::DrawESP();

	g_pRenderTarget->EndDraw();
}

void WorldToScreen(Vector3D* pos, Vector2D* screenPos, float* _w)
{
	// Dot product of position vector and the view matrix
	float x = g_LocalPlayer.perspective._[0] * pos->x + g_LocalPlayer.perspective._[1] * pos->y + g_LocalPlayer.perspective._[2] * pos->z + g_LocalPlayer.perspective._[3];
	
	float y = g_LocalPlayer.perspective._[4] * pos->x + g_LocalPlayer.perspective._[5] * pos->y + g_LocalPlayer.perspective._[6] * pos->z + g_LocalPlayer.perspective._[7];
	
	float w = g_LocalPlayer.perspective._[12] * pos->x + g_LocalPlayer.perspective._[13] * pos->y + g_LocalPlayer.perspective._[14] * pos->z + g_LocalPlayer.perspective._[15];
	*_w = w;

	screenPos->x = (float)((CSGO::WIDTH / 2) + (0.5 * x / w) * CSGO::WIDTH + 0.5);
	screenPos->y = (float)((CSGO::HEIGHT / 2) - (0.5 * y / w) * CSGO::HEIGHT + 0.5);
}

Vector3D GetBonePos(EntityPlayer* ent, int id)
{
	Vector3D bonePos;
	bonePos.x = CSGO::mem->Read<float>(ent->dwBoneBase + id * 0x30 + 0xC);
	bonePos.y = CSGO::mem->Read<float>(ent->dwBoneBase + id * 0x30 + 0x1C);
	bonePos.z = CSGO::mem->Read<float>(ent->dwBoneBase + id * 0x30 + 0x2C);

	return bonePos;
}

void Aimbot()
{
	if (CSGO::isForeground) { Sleep(2000); return; }

	if (GetKeyState(VK_LBUTTON) & 0x8000)
	{
		// Index of target to shoot
		int idx = 0;
		float leastError = sqrt(pow(g_Entity[idx].screenHead.x - CSGO::WIDTH / 2, 2) + pow(g_Entity[idx].screenHead.y - CSGO::HEIGHT, 2));

		bool found = false;

		for (int i = 0; i < g_PlayersOnServer; i++)
		{
			// If it's not visible on the screen
			if (!g_Entity[i].visible) continue;

			// If it's a teammate
			if (g_Entity[i].team == g_LocalPlayer.team) continue;

			found = true;

			float currError = sqrt(pow(g_Entity[i].screenHead.x - CSGO::WIDTH / 2, 2) + pow(g_Entity[i].screenHead.y - CSGO::HEIGHT / 2, 2));

			if (currError < leastError) { leastError = currError; idx = i; }
		}

		// If couldn't find a decent target or the target is out of FOV
		if (!found || leastError > g_fAimFOV) return;

		float dX = g_Entity[idx].screenHead.x - CSGO::WIDTH / 2;
		float dY = g_Entity[idx].screenHead.y - CSGO::HEIGHT / 2;

		int randX = (int)round(fabs(dX * g_AimScatter));
		int randY = (int)round(fabs(dY * g_AimScatter));

		randX == 0 ? dX = 0 : dX += rand() % randX - randX / 2;
		randY == 0 ? dY = 0 : dY += rand() % randY - randY / 2;

		POINT currPos;
		GetCursorPos(&currPos);
		SetCursorPos(currPos.x + (int)round(dX * g_fAimSmoothness), currPos.y + (int)round(dY * g_fAimSmoothness));
	}
}

bool InitD2D(HWND* overlayWindow)
{
	RECT rc;
	GetWindowRect(*overlayWindow, &rc);

	ID2D1Factory* pD2DFactory = NULL;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
	if (!pD2DFactory) { return false; }

	D2D1_SIZE_U size = D2D1::SizeU(
		rc.right - rc.left,
		rc.bottom - rc.top
	);

	pD2DFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(*overlayWindow, size),
		&g_pRenderTarget
	);
	if (!g_pRenderTarget) { return false; }

	// Enemy brush
	g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &BRUSH::p_EnemyBrush);

	// Ally brush
	g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::ForestGreen), &BRUSH::p_AllyBrush);

	// Turn off v-sync
	D2D1::HwndRenderTargetProperties(*overlayWindow, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY);

	return true;
}
