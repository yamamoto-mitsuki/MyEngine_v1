#include "MyEngine/Window/Win32Window.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Utils/Const.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <d3d12.h>
#include <format>
#include <wrl.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void Win32Window::Init() {
	WindowConfig config;
	Init(config);
}

void Win32Window::Init(const WindowConfig& config) {
	config_ = config;
	width_ = config.width;
	height_ = config.height;
	title_ = config.title;

	// ===== ウィンドウクラスを登録する =====
	// ウィンドウプロシージャ
	wc_.lpfnWndProc = WindowProc;
	// ウィンドウクラス名(なんでも良い)
	wc_.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	// ウィンドウクラスを登録
	RegisterClass(&wc_);

	// ===== ウィンドウサイズを決める =====
	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, config.width, config.height };
	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ===== ウィンドウの生成 =====
	hwnd_ = CreateWindow(
		wc_.lpszClassName,    // 利用するクラス名
		config.title.c_str(), // タイトルバーの文字
		config.style,         // よく見るウィンドウスタイル
		config.x,             // 表示X座標
		config.y,             // 表示Y座標
		wrc.right - wrc.left, // ウィンドウ縦幅
		wrc.bottom - wrc.top, // ウィンドウ横幅
		nullptr,              // 親ウィンドウハンドル
		nullptr,              // メニューハンドル
		wc_.hInstance,        // インスタンスハンドル
		nullptr);             // オプション

	SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	// ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);

	// ログに出す
	LogManager::Log(std::format("Complete create Window!"));
}

bool Win32Window::ProcessMessage() {
	MSG msg{};
	// メッセージがある限りすべて処理する
	while (PeekMessage(&msg, hwnd_, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return IsWindow(hwnd_) != 0;
}

LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	// SetWindowLongPtrで保存した値を取り出す
	Win32Window* self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	// ImGui ターゲットに設定されているウィンドウだけ ImGui にメッセージを流す
	// これによりメインウィンドウの操作がサブウィンドウの ImGui に届かなくなる
	if (self && self->isImGuiTarget_) {
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
			return true;
		}
	}
#endif

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {

	// ウィンドウのサイズが変わったとき
	case WM_SIZE:
	{
		if (wparam == SIZE_MINIMIZED) {
			break;
		}
		Win32Window* self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (self) {
			int w = static_cast<int>(LOWORD(lparam));
			int h = static_cast<int>(HIWORD(lparam));
			// 最大化・復元時は即時リサイズ
			if (wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) {
				if (self->onResize_) {
					self->onResize_(w, h);
				}
			} else {
				// ドラッグ中などは保留
				self->pendingResize_ = true;
				self->pendingWidth_ = w;
				self->pendingHeight_ = h;
			}
		}
		break;
	}

	// ウィンドウの×ボタン
	case WM_CLOSE:
	{
		Win32Window* self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (self && self->onCanClose_) {
			if (!self->onCanClose_()) {
				return 0;
			}
		}
		DestroyWindow(hwnd);
		return 0;
	}

	// ウィンドウが破棄された
	case WM_DESTROY:
	{
		return 0;
	}

	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}