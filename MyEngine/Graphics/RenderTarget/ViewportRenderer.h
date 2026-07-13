#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#include <externals/imgui/imgui.h>

#include "MyEngine/Math/Matrix4x4.h"

// 前方宣言
class RenderWindow;


/// <summary>
/// ゲーム画面をImGui::Imageとして表示するクラス
/// </summary>
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

	/// <summary>
	/// シーンを1回分描画する
	/// <para>Debug/Release共通の描画列</para>
	/// </summary>
	static void RenderScene(RenderWindow* renderer, const std::wstring& windowTitle, float width, float height);

	/// <summary>
	/// ビューポートとシザー矩形を設定する
	/// </summary>
	static void SetViewportAndScissor(float width, float height, float offsetX = 0.0f, float offsetY = 0.0f);

	// インスタンス
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