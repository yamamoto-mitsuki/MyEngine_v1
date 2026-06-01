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


/// <summary>
/// 全ウィンドウの管理クラス
/// </summary>
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
	/// <param name> = "initialScene">設定したいシーン</param>
	void AddWindow(const WindowConfig& config, std::unique_ptr<IScene> initialScene = nullptr);

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

	/// <summary>
	/// PreRenderAll()内の3D描画前に呼ぶ処理を登録する
	/// <para>用途: RaymarchRenderer::Flush() など、プロジェクト固有の描画Flush処理</para>
	/// <para> 戻り値: 登録したコールバックのID。後で削除したいときに使う</para>
	/// </summary>
	/// <param name="callback">登録したい処理</param>
	/// <param name="tarhetTitle">描画したいウィンドウ（指定しない場合、全てのウィンドウに実行）</param>
	int AddPreRenderCallback(std::function<void()> callback, const std::wstring& targetTitle = L"");

	/// <summary>
	/// PostRenderAll()内に呼ぶ処理を登録する
	/// <para>用途: RaymarchRenderer::ClearRequests() など、プロジェクト固有のクリア処理</para>
	/// </summary>
	/// <param name="callback">登録したい処理</param>
	/// <param name="tarhetTitle">描画したいウィンドウ（指定しない場合、全てのウィンドウに実行）</param>
	int AddPostRenderCallback(std::function<void()> callback, const std::wstring& targetTitle = L"");

	/// <summary>
	/// AddPreRenderCallBack()で登録したコールバックを解除
	/// <param name="id">AddPreEnderCallback()の戻り値のid</param>
	void RemovePreRenderCallback(int id);

	/// <summary>
	/// AddPostRenderCallBack()で登録したコールバックを解除
	/// <param name="id">AddPostRnderCallback()の戻り値のid</param>
	void RemovePostRenderCallback(int id);

	// ゲッター
	HWND GetMainHWND() const { return windows_.empty() ? nullptr : windows_[0].window->GetHWND(); }
	Win32Window* GetWindowByTitle(const std::wstring& title);
	IScene* GetSceneByTitle(const std::wstring& title);
	float GetGameViewWidth() const { return gameViewWidth_; }
	float GetGameViewHeight() const { return gameViewHeight_; }

private:
	// ウィンドウとウィンドウに描画するクラスをまとめた構造体
	struct WindowSet {
		std::unique_ptr<Win32Window> window;
		std::unique_ptr<RenderWindow> renderer;
		std::unique_ptr<SceneManager> sceneManager;
	};
	// 全ウィンドウ
	std::vector<WindowSet> windows_;

	// コールバックをIDと一緒に管理する
	struct CallbackEntry {
		int id;
		std::wstring targetWindowTitle;
		std::function<void()> func;
	};
	// コールバックIDの管理
	int nextCallbackId_ = 0;
	// プロジェクト側から登録する追加の描画処理
	std::vector<CallbackEntry> preRenderCallbacks_;
	std::vector<CallbackEntry> postRenderCallbacks_;

	// ゲーム画面のウィンドウの大きさ
	float gameViewWidth_ = 0.0f;
	float gameViewHeight_ = 0.0f;

#ifdef USE_IMGUI
	std::wstring imguiTargetWindow_;
	void RenderImGui();
	void DrawGameToRenderTexture(WindowSet& window);
#endif
};
