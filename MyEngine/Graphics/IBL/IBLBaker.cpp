#include "IBLBaker.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"

IBLBaker* IBLBaker::instance_ = nullptr;


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void IBLBaker::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()を2回以上呼んでいます");
	instance_ = new IBLBaker();

	instance_->equirectPass_.Initialize(ShaderFile::EquirectToCubePS);
	instance_->irradiancePass_.Initialize(ShaderFile::IrradiancePS);
	instance_->prefilterPass_.Initialize();
	// instance_->brdfPass_.Initialize();

	LogManager::Log("IBLBaker Initialized");
}

// ===== 解放 =====
void IBLBaker::Finalize() {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()より先にFinalize()が呼ばれています");
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("IBLBaker Finalized");
}