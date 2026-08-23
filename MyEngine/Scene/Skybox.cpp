#include "Skybox.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Graphics/IBL/IBLBaker.h"
#include "MyEngine/Graphics/IBL/IBLConfig.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"

using namespace Microsoft::WRL;

namespace {
const std::string vsName = "SkyboxVS";
const std::string psName = "SkyboxPS";
const std::string kDefaultSkyPath = "MyEngine/Resources/Textures/skybox.hdr";
}


//=============================================================================
// 初期化
//=============================================================================
void Skybox::Initialize() { 
	CreateRootSignature();
	CreatePSO();
	cb_ = DirectXCommon::CreateMappedUploadBuffer(kSlotSize * kMaxViewsPerFrame, reinterpret_cast<void**>(&cbMapped_));
	cb_->SetName(L"SkyboxCB");
	LoadDefaultTexture();
	LogManager::Log("Initialized");
}


//=============================================================================
// デフォルト天球	
//=============================================================================
void Skybox::LoadDefaultTexture() { 
	SetEquirect(TextureManager::Load(kDefaultSkyPath)); 
}

//=============================================================================
// 描画
//=============================================================================
void Skybox::Draw(const Camera* camera) {
	// 早期リターン
	MY_ASSERT_MSG(cube_, "テクスチャが設定されていません。SetEquirectで設定してください。");
	MY_ASSERT_MSG(camera, "カメラが設定されていません。");
	MY_ASSERT_MSG(slot_ < kMaxViewsPerFrame, "1フレームのビュー数が上限を超えました");

	//	カメラの回転のみ
	Matrix4x4 view = camera->GetViewMatrix();
	view.m[3][0] = view.m[3][1] = view.m[3][2] = 0.0f;
	// CBV更新。コマンドの実行はフレーム末。1スロットを使い回すと後のビューの値で上書きされる
	size_t offset = slot_ * kSlotSize;
	auto* cb = reinterpret_cast<SkyboxData*>(cbMapped_ + offset);
	cb->world = MakeRotateYMatrix(rotationY_);
	cb->viewProj = view * camera->GetProjectionMatrix();
	cb->intensity = intensity_;
	cb->cubeIndex = cube_->GetSRVSlot();
	++slot_;

	auto* cmdList = DirectXCommon::GetCommandList();
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootSignature(rsInfo_.rootSignature.Get()); // RootSignature
	cmdList->SetPipelineState(pso_.Get()); // PipelineState
	cmdList->SetGraphicsRootConstantBufferView(rsInfo_.slotOf.at(RootBind::Skybox), cb_->GetGPUVirtualAddress() + offset); // b0
	cmdList->SetGraphicsRootDescriptorTable(rsInfo_.slotOf.at(RootBind::BindlessTextureCube), 
		DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()); // t0
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
	// --- Shaderコンパイル結果 ---
	IDxcBlob* vsBlob = ShaderPackageLoader::GetShaderReflection(vsName).blob.Get();
	IDxcBlob* psBlob = ShaderPackageLoader::GetShaderReflection(psName).blob.Get();

	// --- PipelineStateDesc ---
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rsInfo_.rootSignature.Get();
	desc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
	desc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
	desc.InputLayout = {nullptr, 0};
	desc.BlendState = RenderStates::MakeBlendDesc(BlendMode::None); // Blend
	desc.RasterizerState = RenderStates::MakeRasterizerDesc(RasterizerType::SolidNone);  // Rasterizer
	desc.DepthStencilState = RenderStates::MakeDepthStencilDesc(DepthMode::TestNoWrite); // 深度
	// シーンの描画先と一致させる
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 作成
	HRESULT hr = DirectXCommon::GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_));
	MY_ASSERT_MSG(SUCCEEDED(hr), "Skybox PSO作成失敗");
}


//=============================================================================
// セッター
//=============================================================================
void Skybox::SetEquirect(uint32_t handle) { 
	auto equirectSRV = TextureManager::GetTextureData(handle)->srvHandleGPU;
	// 表示用のEnvキューブだけ確保して焼く。サイズは元画像から決める
	Vector2 texSize = TextureManager::GetTextureSize(handle);
	uint32_t cubeSize = CalcCubeSize(texSize.x);
	LogManager::Log("Skybox cube size = " + std::to_string(cubeSize) + " (source " + std::to_string(static_cast<int>(texSize.x)) + "x" + std::to_string(static_cast<int>(texSize.y)) + ")");
	ownedCube_ = std::make_unique<RenderTextureCube>(cubeSize, RenderTextureFormat::HDR, 1);
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


//=============================================================================
// equirectの解像度からキューブ1面のサイズを決める
//=============================================================================
uint32_t Skybox::CalcCubeSize(float equirectWidth) {
	// equirectの横幅は360度ぶん。1面は90度なので width/4 が等倍になる
	uint32_t ideal = static_cast<uint32_t>(equirectWidth) / 4;
	// 2のべき乗へ切り上げる（ミップやサンプラの都合が良い）
	uint32_t size = IBLConfig::kSkyboxSizeMin;
	while (size < ideal && size < IBLConfig::kSkyboxSizeMax) {
		size <<= 1;
	}
	return size;
}