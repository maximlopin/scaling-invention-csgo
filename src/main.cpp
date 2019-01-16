#include "main.h"
#define TARGET_BONE 10

const int g_PlayersOnServer = 9; // excluding local player

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
	{
		CSGO::hWnd = FindWindow(0, WINDOW_NAME);
		Sleep(1000);
	}

	GetWindowThreadProcessId(CSGO::hWnd, &CSGO::procId);

	CSGO::procHandle = OpenProcess(PROCESS_VM_READ, FALSE, CSGO::procId);
	if (!CSGO::procHandle)
		return 1;

	CSGO::mem = new RMEM(CSGO::procId, CSGO::procHandle);
	
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

	std::thread thUpdateCoordinates(
		RepeatedlyCall, UpdateCoordinates, 50
	);

	std::thread thUpdateHealths(
		RepeatedlyCall, UpdateHealths, 1000
	);

	std::thread thUpdateWndStatus(
		RepeatedlyCall, UpdateWndStatus, 100
	);

	std::thread thUpdateTeams(
		RepeatedlyCall, UpdateTeams, 100
	);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(50);
	}

	thUpdateCoordinates.join();
	thUpdateHealths.join();
	thUpdateWndStatus.join();
	thUpdateTeams.join();

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
	while (1)
	{
		func();
		Sleep(delay);
	}
}

void UpdateCoordinates()
{
	g_LocalPlayer.view = CSGO::mem->Read<Matrix4x4>(CSGO::dwClientBase + OFFSET_VIEW_MATRIX);

	for (int i = 0; i < g_PlayersOnServer; i++)
	{
		Vector3D posFeet, posHead;
		posFeet = CSGO::mem->Read<Vector3D>(g_Entity[i].dwBase + OFFSET_ORIGIN);
		posHead = GetBonePos(&g_Entity[i], TARGET_BONE);

		Vector2D screenFeet, screenHead;
		float _w;
		WorldToScreen(&posFeet, &screenFeet, &_w, CSGO::WIDTH, CSGO::HEIGHT);
		WorldToScreen(&posHead, &screenHead, &_w, CSGO::WIDTH, CSGO::HEIGHT);

		g_Entity[i].visible = (g_Entity[i].health != 0 && _w >= 0.1f);
		if (!g_Entity[i].visible) continue;

		float padding = fabs(screenFeet.y - screenHead.y) / 3;

		g_Entity[i].box.x0 = screenFeet.x - padding;
		g_Entity[i].box.y0 = screenFeet.y;
		g_Entity[i].box.x1 = screenFeet.x + padding;
		g_Entity[i].box.y1 = screenHead.y;

		float hpBarLength = (float)((float)g_Entity[i].health / 100.0f) * (float)(padding * 2);
		g_Entity[i].hpBar0.x = screenFeet.x - padding;;
		g_Entity[i].hpBar0.y = screenFeet.y + padding / 8;;
		g_Entity[i].hpBar1.x = screenFeet.x - padding + (int)round(hpBarLength);;
		g_Entity[i].hpBar1.y = screenFeet.y + padding / 8;

		g_Entity[i].hpBarThickness = padding / 4;
	}
}

void UpdateHealths()
{
	g_LocalPlayer.health = CSGO::mem->Read<int>(g_LocalPlayer.dwBase + OFFSET_HEALTH);

	for (int i = 0; i < g_PlayersOnServer; i++)
	{
		g_Entity[i].health = CSGO::mem->Read<int>(g_Entity[i].dwBase + OFFSET_HEALTH);
	}
}

void UpdateWndStatus()
{
	CSGO::isForeground = (GetForegroundWindow() == CSGO::hWnd);
}

void UpdateTeams()
{
	g_LocalPlayer.team = CSGO::mem->Read<int>(g_LocalPlayer.dwBase + OFFSET_TEAM);

	for (int i = 0; i < g_PlayersOnServer; i++)
	{
		g_Entity[i].team = CSGO::mem->Read<int>(g_Entity[i].dwBase + OFFSET_TEAM);
		g_Entity[i].team == g_LocalPlayer.team ? g_Entity[i].brush = p_AllyBrush : g_Entity[i].brush = p_EnemyBrush;
	}
}

void Render()
{
	/* Don't draw if CSGO is not the foreground window */
	if (!CSGO::isForeground)
	{
		renderTarget->BeginDraw();
		renderTarget->Clear();
		renderTarget->EndDraw();
		Sleep(1000);
		return;
	}
	renderTarget->BeginDraw();
	renderTarget->Clear();

	for (int i = 0; i < g_PlayersOnServer; i++)
	if (g_Entity[i].visible)
	{
		/* Box */
		renderTarget->DrawRectangle(D2D1::RectF(g_Entity[i].box.x0, g_Entity[i].box.y0 ,g_Entity[i].box.x1, g_Entity[i].box.y1), g_Entity[i].brush);

		/* HP bar */
		renderTarget->DrawLine(g_Entity[i].hpBar0, g_Entity[i].hpBar1, g_Entity[i].brush, g_Entity[i].hpBarThickness, 0);
	}

	renderTarget->EndDraw();
}

void WorldToScreen(Vector3D* pos, Vector2D* screenPos, float* _w, int W, int H)
{
	/* Dot product of position vector and the view matrix */
	float x = g_LocalPlayer.view[0] * pos->x + g_LocalPlayer.view[1] * pos->y + g_LocalPlayer.view[2] * pos->z + g_LocalPlayer.view[3];

	float y = g_LocalPlayer.view[4] * pos->x + g_LocalPlayer.view[5] * pos->y + g_LocalPlayer.view[6] * pos->z + g_LocalPlayer.view[7];

	float w = g_LocalPlayer.view[12] * pos->x + g_LocalPlayer.view[13] * pos->y + g_LocalPlayer.view[14] * pos->z + g_LocalPlayer.view[15];
	*_w = w;

	screenPos->x = (float)((W / 2) + (0.5 * x / w) * W + 0.5);
	screenPos->y = (float)((H / 2) - (0.5 * y / w) * H + 0.5);
}

Vector3D GetBonePos(EntityPlayer* ent, int id)
{
	Vector3D bonePos;
	bonePos.x = CSGO::mem->Read<float>(ent->dwBoneBase + id * 0x30 + 0xC);
	bonePos.y = CSGO::mem->Read<float>(ent->dwBoneBase + id * 0x30 + 0x1C);
	bonePos.z = CSGO::mem->Read<float>(ent->dwBoneBase + id * 0x30 + 0x2C);

	return bonePos;
}
