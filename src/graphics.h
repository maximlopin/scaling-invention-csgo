#include <d2d1.h>

ID2D1HwndRenderTarget* renderTarget = NULL;

ID2D1SolidColorBrush* p_EnemyBrush = NULL;
ID2D1SolidColorBrush* p_AllyBrush = NULL;

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
		&renderTarget
	);
	if (!renderTarget) { return false; }

	renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &p_EnemyBrush);
	renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::ForestGreen), &p_AllyBrush);

	// Turn off v-sync
	D2D1::HwndRenderTargetProperties(*overlayWindow, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY);

	return true;
}
