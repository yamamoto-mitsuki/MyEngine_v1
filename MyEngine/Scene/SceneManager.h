#pragma once
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Scene/IScene.h"
#include <functional>
#include <memory>

// 実行状態
enum class PlayState {
	Editing, // 停止中。Updateを回さない
	Playing, // 再生中
	Paused,  // 一時停止
};


/// <summary>
/// シーン一括管理クラス
/// </summary>
class SceneManager {
public:
	void Initialize();
	void Update();
	void Draw();
	void Finalize();

	// ===== 再生コントロール =====
	void Play();  // 停止中なら最初から再生
	void Pause(); // 一時停止 / 再開
	void Stop();  // 停止して初期状態へ戻す
	void RequestReload() { isReloadRequested_ = true; }

	// ===== ゲッター =====
	IScene* GetCurrentScene() { return currentScene_.get(); }
	PlayState GetPlayState() const { return playState_; }

	// ===== セッター =====
	void SetScene(std::unique_ptr<IScene> currentScene) { currentScene_ = std::move(currentScene); }
	void RequestNextScene(std::unique_ptr<IScene> next) { nextScene_ = std::move(next); }
	void SetSceneFactory(std::function<std::unique_ptr<IScene>()> factory) { sceneFactory_ = std::move(factory); }
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }

private:
	void ReloadImmediate();

	std::unique_ptr<IScene> currentScene_;
	std::unique_ptr<IScene> nextScene_;
	SceneFactory sceneFactory_;
	std::wstring windowTitle_;
#ifdef USE_IMGUI
	PlayState playState_ = PlayState::Editing; // エディタでは停止状態から始める
#else
	PlayState playState_ = PlayState::Playing; // Release は即座に動かす
#endif
	bool isReloadRequested_ = false;
};