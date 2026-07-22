#include "RenderTextureManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"

RenderTextureManager* RenderTextureManager::instance_ = nullptr;


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void RenderTextureManager::Initialize() { 
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()を2回以上呼んでいます");
	instance_ = new RenderTextureManager();
}

// ===== 解放 =====
void RenderTextureManager::Finalize() {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()より先にFinalize()が呼ばれています");
	instance_->renderTextures_.clear();
	delete instance_;
	instance_ = nullptr;
}


//=============================================================================
// 生成
//=============================================================================
RenderTextureManager::Handle RenderTextureManager::Create(uint32_t width, uint32_t height, RenderTextureFormat format, bool useDepth) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");

	uint32_t index = static_cast<uint32_t>(instance_->renderTextures_.size()); // 現在のrenderTexturesのサイズを index に
	instance_->renderTextures_.push_back(std::make_unique<RenderTexture>(width, height, format, useDepth));

	return Handle{index};
}


//=============================================================================
// 取得
//=============================================================================
RenderTexture* RenderTextureManager::Get(Handle handle) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	MY_ASSERT_MSG(handle.IsValid(), "無効なHandleです");
	MY_ASSERT_MSG(handle.index < instance_->renderTextures_.size(), "Handleが範囲外です");

	RenderTexture* rt = instance_->renderTextures_[handle.index].get();
	MY_ASSERT_MSG(rt != nullptr, "そのHandleのRenderTextureは既に破棄されています");
	return rt;
}