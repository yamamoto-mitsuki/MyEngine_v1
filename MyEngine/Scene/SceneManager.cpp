#include "SceneManager.h"
#include <cassert>

//======================================================================================================
// 終了処理
//======================================================================================================
void SceneManager::Finalize() { 
	assert(currentScene_ != nullptr && "シーンが登録されておらず、終了できませんでした");
	currentScene_.get()->Finalize();
}

//======================================================================================================
// 更新
//======================================================================================================
void SceneManager::Update() { 
	assert(currentScene_ != nullptr && "シーンが登録されておらず、更新できませんでした"); 
	// 現在のシーンを更新
	currentScene_.get()->Update();
	// シーン遷移チェック
	auto nextScene = currentScene_->NextScene();
	if (nextScene) {
		nextScene->SetWindowTitle(currentScene_->GetWindowTitle());
		currentScene_->Finalize();
		currentScene_ = std::move(nextScene);
		currentScene_->Initialize();
	}
}

//======================================================================================================
// 描画
//======================================================================================================
void SceneManager::Draw() {
	assert(currentScene_ != nullptr && "シーンが登録されておらず、描画できませんでした");
	currentScene_.get()->Draw();
}