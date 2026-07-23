#include "IBLEnvironment.h"
#include "MyEngine/Graphics/IBL/IBLIncludes.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"


//=============================================================================
//
//=============================================================================
void IBLEnvironment::MakeFromHDR(const std::string& filePath) { 
	// 読み込み
	uint32_t handle = TextureManager::Load(filePath); 
	auto equirectSRV = TextureManager::GetTextureData(handle)->srvHandleGPU;
	// 焼き先を確保
	env_ = std::make_unique<RenderTextureCube>(IBLConfig::kEnvironmentSize, RenderTextureFormat::HDR, 1);
	irradiance_ = std::make_unique<RenderTextureCube>(IBLConfig::kIrradianceSize, RenderTextureFormat::HDR, 1);
	// 記録
	IBLBaker::Equirect().Record(*env_, equirectSRV);

	// まとめて1回Execute + Wait
	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->Close();
	ID3D12CommandList* lists[] = {cmdList};
	DirectXCommon::GetCommandQueue()->ExecuteCommandLists(1, lists);
	DirectXCommon::WaitForGPU();
	cmdList->Reset(DirectXCommon::GetCommandAllocator(), nullptr);
}