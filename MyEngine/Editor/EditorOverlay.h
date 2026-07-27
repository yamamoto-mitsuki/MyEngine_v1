#pragma once
#ifdef USE_IMGUI
#include <vector>
#include <string>
#include <externals/imgui/imgui.h>
#include <externals/imgui/ImGuizmo.h>
#include "MyEngine/Math/MathIncludes.h"

// 前方宣言
class Camera;


/// <summary>
/// Window上にカメラ軸などを表示するクラス
/// </summary>
class EditorOverlay {
public:
	EditorOverlay(const EditorOverlay&) = default;
	EditorOverlay& operator=(const EditorOverlay&) = default;

	static void Initialize();
	static void Release();

	/// <summary>
	/// オーバーレイを描画する。
	/// </summary>
	static void Draw(ImVec2 imageMin, ImVec2 imageMax, ImVec2 imageSize);

	/// <summary>
	/// 任意のEditorを描画するか確認するクラス
	/// </summary>
	static void DrawSettings();

	/// <summary>
	/// カメラを登録する。
	/// </summary>
	static void SetActiveCamera(Camera* camera) { instance_->activeCamera_ = camera; }
	static Camera* GetActiveCamera() { return instance_ ? instance_->activeCamera_ : nullptr; }



private:
	EditorOverlay() = default;
	~EditorOverlay() = default;

	// 内部ヘルパー
	static void DrawAxisGizmo(ImDrawList* dl, const Camera* cam, ImVec2 imageMin, ImVec2 imageMax, ImVec2 imageSize);

	static EditorOverlay* instance_;

	Camera* activeCamera_ = nullptr;
	bool isShowAxis_ = true;
};
#else
class EditorOverlay {
public:
	EditorOverlay(const EditorOverlay&) = default;
	EditorOverlay& operator=(const EditorOverlay&) = default;

	static void Initialize() {};
	static void Release() {};

	/// <summary>
	/// オーバーレイを描画する。
	/// </summary>
	static void Draw(ImVec2 imageMin, ImVec2 imageMax, ImVec2 imageSize) {};

	/// <summary>
	/// 任意のEditorを描画するか確認するクラス
	/// </summary>
	static void DrawSettings() {};

	/// <summary>
	/// カメラを登録する。
	/// </summary>
	static void SetActiveCamera(Camera* camera) {};
	static Camera* GetActiveCamera();

private:
	EditorOverlay() = default;
	~EditorOverlay() = default;

	static EditorOverlay* instance_;

	Camera* activeCamera_ = nullptr;
	bool isShowAxis_ = true;
};

#endif