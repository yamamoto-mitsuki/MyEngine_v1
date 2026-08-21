#include "SceneManager.h"
#include <functional>
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Time/Time.h"

void SceneManager::Initialize() {
	MY_ASSERT_MSG(sceneFactory_ != nullptr, "SetSceneFactory()でシーンの作り方を登録してください");
	ReloadImmediate();
}

void SceneManager::Update() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした");

	// フレームの先頭で作り直す
	if (isReloadRequested_) {
		isReloadRequested_ = false;
		ReloadImmediate();
	}
	// 停止中・一時停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ == PlayState::Editing) {
		return;
	}
	// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
	Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
	currentScene_->Update();
	// シーン遷移チェック
	auto nextScene = currentScene_->NextScene();
	if (nextScene) {
		nextScene->SetWindowTitle(currentScene_->GetWindowTitle());
		currentScene_->Finalize();
		currentScene_ = std::move(nextScene);
		currentScene_->Initialize();
	}
}

void SceneManager::Draw() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、描画できませんでした");
	currentScene_->Draw();
}

//======================================================================================================
// 再生コントロール
//======================================================================================================
void SceneManager::Play() {
	if (playState_ == PlayState::Editing) {
		RequestReload(); // 停止状態からの再生は最初から
	}
	playState_ = PlayState::Playing;
}

void SceneManager::Pause() {
	if (playState_ == PlayState::Playing) {
		playState_ = PlayState::Paused;
	} else if (playState_ == PlayState::Paused) {
		playState_ = PlayState::Playing;
	}
}

void SceneManager::Stop() {
	playState_ = PlayState::Editing;
	RequestReload();
}

void SceneManager::ReloadImmediate() {
	// 作り直せないときは落とさず、今のシーンを維持する
	MY_ASSERT_MSG(sceneFactory_ != nullptr, "SetSceneFactory()でシーンの作り方を登録してください");
	if (currentScene_) {
		currentScene_->Finalize();
	}
	currentScene_ = sceneFactory_();
	currentScene_->SetWindowTitle(windowTitle_);
	currentScene_->Initialize();
}