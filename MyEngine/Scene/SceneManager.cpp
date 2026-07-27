#include "SceneManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

namespace {
const uint32_t kCameraSlotSize = 256;
}

//======================================================================================================
// 終了
//======================================================================================================
// ===== 終了 =====
void SceneManager::Finalize() { 
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、終了できませんでした");
	currentScene_.get()->Finalize();
}


//======================================================================================================
// 更新
//======================================================================================================
void SceneManager::Update() { 
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした"); 
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
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、描画できませんでした");
	currentScene_.get()->Draw();
}