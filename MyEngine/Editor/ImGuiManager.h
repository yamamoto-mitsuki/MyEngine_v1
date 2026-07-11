#pragma once
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <utility>
#include <functional>

#include <wrl.h>
#include <d3d12.h>

#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Window/Win32Window.h"

#ifdef USE_IMGUI


/// <summary>
/// ImGuiの描画を管理するクラス。
/// </summary>
class ImGuiManager {
public:
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

	/// <summary>
	/// 初期化。Engine::Initialize() 内で1度だけ呼ぶ。
	/// </summary>
	static void Initialize(Win32Window* window);

	/// <summary>
	/// 解放
	/// </summary>
	static void Release();

	/// <summary>
	/// フレームの開始処理。毎フレーム最初に呼ぶ。
	/// </summary>
	static void Begin();

	/// <summary>
	/// ImGuiウィンドウの描画リクエストを追加する。
	/// </summary>
	/// <param name="drawFunc">ImGui::Begin / End を含むラムダ式</param>
	static void AddDrawRequest(std::function<void()> drawFunc);

	/// <summary>
	/// AddDrawRequest() で積まれた描画リクエストを全て実行する。
	/// </summary>
	static void ProcessRequests();

	/// <summary>
	/// 描画データを確定させる（ImGui::Render() のラッパー）。
	/// </summary>
	static void Render();

	/// <summary>
	/// 確定した描画データをコマンドリストに積む。
	/// </summary>
	static void RenderDrawData();

	/// <summary>
	/// 全リクエストをクリアする。
	/// </summary>
	static void ClearRequests();

private:
	static ImGuiManager* instance_;

	ImGuiManager() = default;
	~ImGuiManager() = default;

	/// <summary>
	/// 現在のImGuiStyleを imgui_style.ini に書き出す。
	/// <para>色・Alpha・サイズ系パラメータを全て保存する。</para>
	/// </summary>
	/// <param name="path">保存先ファイルパス</param>
	void SaveStyle(const std::string& path);

	/// <summary>
	/// imgui_style.ini からImGuiStyleを読み込む。
	/// <para>ファイルが存在しない場合はスキップする（初回起動時は正常動作）</para>
	/// </summary>
	/// <param name="path">読み込み元ファイルパス</param>
	void LoadStyle(const std::string& path);

	// 描画リクエストのキュー
	std::vector<std::function<void()>> requests_;
	// メインウィンドウのハンドル
	HWND hwnd_ = nullptr;

	// Setting メニューの表示状態フラグ
	bool isShowStyleEditor_ = false;
};

#else

/// <summary>
/// USE_IMGUI が定義されていない場合の何もしないスタブ。
/// </summary>
class ImGuiManager {
public:
	static void Initialize(Win32Window*, const WindowConfig&) {}
	static void Release() {}
	static void Begin() {}
	static void Render() {}
	static void RenderDrawData() {}
	static void ClearRequests() {}
	static void ProcessRequests() {}
	static void AddDrawRequest(std::function<void()>) {}
};

#endif