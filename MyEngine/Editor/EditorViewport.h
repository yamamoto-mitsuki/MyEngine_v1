#pragma once
#include <memory>
#include <string>
#include "MyEngine/Camera/DebugCamera.h"
#include "MyEngine/Editor/ViewportWindow.h"
#include "MyEngine/Graphics/RenderTarget/RenderTextureManager.h"

// 前方宣言
class Camera;
class RenderWindow;


/// <summary>
/// エディタの編集ビュー（Game / Scene の2画面）を1ウィンドウ分まとめて持つ。
/// <para>描画は SceneRenderer に任せ、結果を ViewportWindow で表示する。</para>
/// </summary>
class EditorViewport {
public:
	// RT2枚の確保・デバッグカメラ初期化
	void Initialize(float aspectRatio = 1280.0f / 720.0f);

	// gameCamera視点でGame、debugCamera視点でSceneを描いて表示する
	void Render(const Camera* gameCamera, RenderWindow* rw, const std::wstring& windowTitle);

	// ゲッター
	DebugCamera* GetDebugCamera() { return debugCamera_.get(); }
	bool IsGameHovered() const { return gameView_.IsHovered(); }
	bool IsSceneHovered() const { return sceneView_.IsHovered(); }

private:
	std::unique_ptr<DebugCamera> debugCamera_;
	RenderTextureManager::Handle gameRT_;
	RenderTextureManager::Handle sceneRT_;
	ViewportWindow gameView_{"Game"};
	ViewportWindow sceneView_{"Scene"};
};