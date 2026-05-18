#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include "MyEngine/Utils/Const.h"
#include "MyEngine/Math/Vector2.h"

// ウィンドウ設定用
struct WindowConfig {
	int width = kClientWidth;      // 横の大きさ
	int height = kClientHeight;    // 縦の大きさ
	std::wstring title = L"Title"; // タイトルバー
	int x = CW_USEDEFAULT;         // 表示座標X
	int y = CW_USEDEFAULT;         // 表示座標Y
	DWORD style = WS_OVERLAPPEDWINDOW; // ウィンドウスタイル
	// ゲームビュー設定
	float gameAspectRatio = 16.0f / 9.0f; // ゲーム画面のアスペクト比
	bool isImGui = false;                 // ImGuiを使うか
	Vector2 gameViewStart = { 0.0f,0.0f }; // ウィンドウ全体のどこからゲーム画面を描画するか
	Vector2 gameViewEnd = { 0.75f,-1.0f }; // ウィンドウ全体のどこまでゲーム画面を描画するか
};

class Win32Window {
public:

	/// <summary>
	/// 初期化(標準)
	/// </summary>
	void Init();

	/// <summary>
	/// 初期化(細かい設定用)
	/// </summary>
	/// <param name = "config">ウィンドウ設定</param>
	void Init(const WindowConfig& config);

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	// メッセージ処理
	bool ProcessMessage();

	// ゲッター
	HWND GetHWND() const { return hwnd_; }
	const std::wstring& GetTitle() const { return title_; }
	bool GetIsImGuiTarget() const { return isImGuiTarget_; }
	bool GetPendingResize() const { return pendingResize_; }
	int GetPendingWidth() const { return pendingWidth_; }
	int GetPendingHeight() const { return pendingHeight_; }
	WindowConfig GetWindowConfig() const { return config_; }

	// セッター
	void SetImGuiTarget(bool isTarget) { isImGuiTarget_ = isTarget; }
	void SetOnResize(std::function<void(int, int)> callback) { onResize_ = std::move(callback); }
	void ClearPendingResize() { pendingResize_ = false; }

private:
	HWND hwnd_ = nullptr;
	WNDCLASS wc_ = {};
	int width_ = 0;
	int height_ = 0;
	std::wstring title_;
	std::function<void(int, int)> onResize_;
	int pendingWidth_ = 0;
	int pendingHeight_ = 0;
	bool pendingResize_ = false;
	bool isImGuiTarget_ = false;
	WindowConfig config_;
};
