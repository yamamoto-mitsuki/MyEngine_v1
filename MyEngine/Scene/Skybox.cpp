#include "Skybox.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Graphics/IBL/IBLBaker.h"
#include "MyEngine/Graphics/IBL/IBLConfig.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

using namespace Microsoft::WRL;


//=============================================================================
// 初期化
//=============================================================================
void Skybox::Initialize() { 
	CreateRootSignature();
	CreatePSO();
	LogManager::Log("Initialize");
}


//=============================================================================
// 描画
//=============================================================================
void Skybox::Draw(const Camera* camera) {}


//=============================================================================
// セッター
//=============================================================================
void Skybox::SetEquirect(uint32_t handle) { 
	auto equirectSRV = TextureManager::GetTextureData(handle)->srvHandleGPU;
	// 表示用のEnvキューブだけ確保して焼く
	ownedCube_ = std::make_unique<RenderTextureCube>(IBLConfig::kEnvironmentSize, RenderTextureFormat::HDR, 1);
	IBLBaker::Equirect().Record(*ownedCube_, equirectSRV);
	// Recordはコマンドリストに積むだけなので実行と待ち
	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->Close();
	ID3D12CommandList* lists[] = {cmdList};
	DirectXCommon::GetCommandQueue()->ExecuteCommandLists(1, lists);
	DirectXCommon::WaitForGPU();
	cmdList->Reset(DirectXCommon::GetCommandAllocator(), nullptr);

	cube_ = ownedCube_.get();
}