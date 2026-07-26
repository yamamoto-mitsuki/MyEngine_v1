#include "Skybox.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Graphics/IBL/IBLBaker.h"
#include "MyEngine/Graphics/IBL/IBLConfig.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"

using namespace Microsoft::WRL;

namespace {
std::string vsName = "SkyboxVS";
std::string psName = "SkyboxPS";
}

//=============================================================================
// 初期化
//=============================================================================
void Skybox::Initialize() { 
	CreateRootSignature();
	CreatePSO();
	LogManager::Log("Initialized");
}


//=============================================================================
// 描画
//=============================================================================
void Skybox::Draw(const Camera* camera) {
	// 早期リターン
	if (!cube_) {
		LogManager::Warning("Cube No Set");
		return;
	}
	//	カメラの回転のみ
	Matrix4x4 view = camera->GetViewMatrix();
	view.m[3][0] = view.m[3][1] = view.m[3][2] = 0.0f;
	// CBV更新
	auto* cb = reinterpret_cast<SkyboxData*>(cbMapped_);
	cb->world = MakeRotateYMatrix(rotationY_);
	cb->viewProj = view * camera->GetProjectionMatrix();
	cb->intensity = intensity_;

	auto* cmdList = DirectXCommon::GetCommandList();
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootSignature(rsInfo_.rootSignature.Get()); // RootSignature
	cmdList->SetPipelineState(pso_.Get()); // PipelineState
	cmdList->SetGraphicsRootConstantBufferView(rsInfo_.slotOf.at(RootBind::Skybox), cb_->GetGPUVirtualAddress()); // b0
	cmdList->SetGraphicsRootDescriptorTable(1, cube_->GetSRVGPUHandle());      // t0
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(36, 1, 0, 0);
}


//=============================================================================
// RootSignatureInfo作成
//=============================================================================
void Skybox::CreateRootSignature() { 
	// ShaderReflection
	const ShaderReflection& vsRef = ShaderPackageLoader::GetShaderReflection(vsName);
	const ShaderReflection& psRef = ShaderPackageLoader::GetShaderReflection(psName);
	// RootSignatureInfo
	auto shaderResources = RootSignatureManager::MergeStages(vsRef.resources, psRef.resources);
	rsInfo_ = RootSignatureManager::MakeRootSignatureInfo(shaderResources);
}


//=============================================================================
// PSO作成
//=============================================================================
void Skybox::CreatePSO() { 
	// Shaderコンパイル結果
	IDxcBlob* vsBlob = ShaderPackageLoader::GetShaderReflection(vsName).blob.Get();
	IDxcBlob* psBlob = ShaderPackageLoader::GetShaderReflection(psName).blob.Get();
	// PipelineStateDesc
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rsInfo_.rootSignature.Get();
	desc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
	desc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
	desc.InputLayout = {nullptr, 0};
	

}

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