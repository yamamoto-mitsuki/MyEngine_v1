#pragma once
#include "MyEngine/Math/Matrix4x4.h"
#include "externals/imgui/imgui.h"
#include <string>
#include <algorithm>
#include <vector>
#include <functional>

// 前方宣言
class RenderWindow;

// ゲーム画面をImGui::Imageとして表示するクラス
class ViewportRenderer {
public:
	ViewportRenderer(const ViewportRenderer&) = delete;
	ViewportRenderer& operator=(const ViewportRenderer&) = delete;

	static void Initialize();
	static void Release();

	/// <summary>
	/// ゲーム画面を描画する。
	/// <para>Debug: RenderTexture に焼いて ImGui::Image で表示。</para>
	/// <para>Release: 直接ウィンドウに描画。</para>
	/// </summary>
	static void Draw(RenderWindow* renderer, const std::wstring& windowTitle, float windowWidth, float windowHeight);

	/// <summary>
	/// PreRenderCallback を登録する。
	/// 戻り値: 後で削除するための ID
	/// </summary>
	static int AddPreRenderCallback(std::function<void()> callback);

	/// <summary>
	/// PreRenderCallbackの登録を解除する。
	/// </summary>
	/// <param name="id">削除するコールバックの ID</param>
	static void RemovePreRenderCallback(int id);

	// ゲッター
	static float GetGameViewWidth();
	static float GetGameViewHeight();
#ifdef USE_IMGUI
	static const ImVec2& GetImageMin();
	static const ImVec2& GetImageMax();
	static const ImVec2& GetImageSize();
#endif

private:
	ViewportRenderer() = default;
	~ViewportRenderer() = default;
	// 内部ヘルパー
	static void ExecutePreRenderCallbacks();

	static ViewportRenderer* instance_;

	float gameViewWidth_ = 0.0f;
	float gameViewHeight_ = 0.0f;
#ifdef USE_IMGUI
	ImVec2 imageMin_ = {};
	ImVec2 imageMax_ = {};
	ImVec2 imageSize_ = {};
#endif

	// コールバック管理
	struct CallbackEntry {
		int id;
		std::function<void()> func;
	};
	std::vector<CallbackEntry> preRenderCallbacks_;
	int nextCallbackId_ = 0;
};