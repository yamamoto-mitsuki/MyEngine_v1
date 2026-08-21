#pragma once
#include <memory>
#include <string>

// 前方宣言
class Camera;
class RenderWindow;
class DebugCamera;

#include "MyEngine/Camera/DebugCamera.h"
#include "MyEngine/Editor/EditorGrid.h"
#include "MyEngine/Editor/ViewportWindow.h"
#include "MyEngine/Graphics/RenderTarget/RenderTextureManager.h"


/// <summary>
/// エディタの編集ビュー（Game / Scene の2画面）を1ウィンドウ分まとめて持つ。
/// <para>描画は SceneRenderer に任せ、結果を ViewportWindow で表示する。</para>
/// </summary>
class EditorViewport {
public:
	// RT2枚の確保・デバッグカメラ初期化
	void Initialize(float aspectRatio = 1280.0f / 720.0f);
	// 入力
	void Update();
	// グリッドなどEditorのモデルなどの描画リクエストを送る
	void Draw(Camera* camera);
	// gameCamera視点でGame、debugCamera視点でSceneを描いて表示する
	void Render(const Camera* gameCamera, const std::wstring& windowTitle);

	// ゲッター
#ifdef USE_IMGUI
	DebugCamera* GetDebugCamera() { return debugCamera_.get(); }
	bool IsGameHovered() const { return gameView_.IsHovered(); }
	bool IsSceneHovered() const { return sceneView_.IsHovered(); }
#endif

private:
	// --- 調整項目 ---
	void RegisterGV(); // 登録
	void ApplyGV();    // 保存値の反映
	void SyncGV();     // 毎フレームの同期
#ifdef USE_IMGUI
	std::unique_ptr<DebugCamera> debugCamera_;
	Vector3 lastTranslation_{}; // 前フレームのカメラ位置（操作されたかの判定用）
	Vector3 lastRotation_{};    // 前フレームのカメラ回転（操作されたかの判定用）
	RenderTextureManager::Handle gameRT_;
	RenderTextureManager::Handle sceneRT_;
	ViewportWindow gameView_{"Game"};
	ViewportWindow sceneView_{"Scene"};
	// グリッドなど
	std::unique_ptr<EditorGrid> grid_;
	bool isShowGrid_ = true;
	bool isShowCameraFrustum_ = true;
#endif
};