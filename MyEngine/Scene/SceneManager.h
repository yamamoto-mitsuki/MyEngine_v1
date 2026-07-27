#pragma once
#include <memory>
#include <vector>
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Camera/Camera.h"

/// <summary>
/// 描画リクエストを一括描画
/// </summary>
class SceneManager {
public:
	void Initialize();
	void Update();
	void Draw();
	void Finalize();
	
	// ===== ゲッター =====
	IScene* GetCurrentScene() { return currentScene_.get(); }

	// ===== セッター =====
	void SetScene(std::unique_ptr<IScene> currentScene) { currentScene_ = std::move(currentScene); }
	void RequestNextScene(std::unique_ptr<IScene> next) { nextScene_ = std::move(next); }

private:
	std::unique_ptr<IScene> currentScene_;
	std::unique_ptr<IScene> nextScene_;
};