#pragma once
#include "MyEngine/Debug/ImGuiManager.h"
#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/Window/Win32Window.h"
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

// 前方宣言
class RenderWindow;
class DirectXCommon;

class WindowManager {
public:
	/// <summary>
	/// 終了処理。whileループを抜けた後、return 0の前に必ず呼ぶ
	/// </summary>
	void Finalize();

	/// <summary>
	/// ウィンドウを追加
	/// </summary>
	/// <param name = "config">ウィンドウ設定</param>
	/// <param name = "dxCommon">DirectXの共通機能</param>
	/// <param name> = "initialScene">設定したいシーン</param>
	void AddWindow(const WindowConfig& config, DirectXCommon* dxCommon, std::unique_ptr<IScene> initialScene = nullptr);

	/// <summary>
	/// 全ウィンドウのメッセージ処理
	/// </summary>
	bool ProcessMessage();

	/// <summary>
	/// 全ウィンドウの更新処理
	/// </summary>
	void UpdateAll();

	/// <summary>
	/// 全ウィンドウの描画
	/// </summary>
	void DrawAll();

	/// <summary>
	/// 全ウィンドウの描画開始
	/// </summary>
	void PreRenderAll();

	/// <summary>
	/// 全ウィンドウの描画命令
	/// </summary>
	void PostRenderAll();

	/// <summary>
	/// コマンドリストの実行
	/// </summary>
	void ExecuteOnly();

	// ゲッター
	HWND GetMainHWND() const { return windows_.empty() ? nullptr : windows_[0].window->GetHWND(); }

#ifdef USE_IMGUI
	std::wstring GetImGuiTargetWindow() const;
	void SetImGuiTargetWindow(const std::wstring& windowTitle);
#endif

private:
	void RenderImGui();
	std::tuple<float, float, float, float> CalcGameViewRect(const WindowConfig& cfg, float totalWidth, float totalHeight);

	// ウィンドウとウィンドウに描画するクラスをまとめた構造体
	struct WindowSet {
		std::unique_ptr<Win32Window> window;
		std::unique_ptr<RenderWindow> renderer;
		std::unique_ptr<SceneManager> sceneManager;
	};

	std::vector<WindowSet> windows_;
	DirectXCommon* dxCommon_ = nullptr;

#ifdef USE_IMGUI
	std::wstring imguiTargetWindow_;
#endif
};
