#define NOMINMAX
#include "BloomPass.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"

using namespace Microsoft::WRL;


//=============================================================================
// 初期化
//=============================================================================
void BloomPass::Initialize(uint32_t width, uint32_t height) {
	// 縮小テクスチャを2列作る。加算合成を使わずに済むよう、行きと帰りで別々に持つ
	uint32_t w = width;
	uint32_t h = height;
	for (uint32_t i = 0; i < kLevelCount; ++i) {
		w = std::max(w / 2u, 1u);
		h = std::max(h / 2u, 1u);
		// 足し込みで飽和しないようHDRにする。深度は使わない
		down_[i] = std::make_unique<RenderTexture>(w, h, RenderTextureFormat::HDR, false);
		up_[i] = std::make_unique<RenderTexture>(w, h, RenderTextureFormat::HDR, false);
	}
	// パスごとに別スロットへ書く。コマンドの実行はフレーム末なので使い回すと壊れる
	cb_ = DirectXCommon::CreateMappedUploadBuffer(kSlotSize * kMaxPassPerFrame, reinterpret_cast<void**>(&cbMapped_));
	cb_->SetName(L"BloomCB");

	CreateRootSignature();
	brightPSO_ = CreatePSO("BloomBrightPS");
	downPSO_ = CreatePSO("BloomDownPS");
	upPSO_ = CreatePSO("BloomUpPS");
	compositePSO_ = CreatePSO("BloomCompositePS");
	LogManager::Log("Initialized");
}

//=============================================================================
// RootSignature / PSO
//=============================================================================
void BloomPass::CreateRootSignature() {
	const ShaderReflection& vs = ShaderPackageLoader::GetShaderReflection("FullscreenVS");
	const ShaderReflection& ps = ShaderPackageLoader::GetShaderReflection("BloomCompositePS");
	auto resources = RootSignatureManager::MergeStages(vs.resources, ps.resources);
	rsInfo_ = RootSignatureManager::MakeRootSignatureInfo(resources);
}

ComPtr<ID3D12PipelineState> BloomPass::CreatePSO(const char* psName) {
	IDxcBlob* vs = ShaderPackageLoader::GetShaderReflection("FullscreenVS").blob.Get();
	IDxcBlob* ps = ShaderPackageLoader::GetShaderReflection(psName).blob.Get();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rsInfo_.rootSignature.Get();
	desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	desc.InputLayout = {nullptr, 0};
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.NumRenderTargets = 1;
	// 中間はHDR、合成先はSDR。フォーマットが違うのでPSOを分ける
	bool isComposite = (std::string(psName) == "BloomCompositePS");
	desc.RTVFormats[0] = isComposite ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DirectXCommon::GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	MY_ASSERT_MSG(SUCCEEDED(hr), "Bloom PSO作成失敗");
	return pso;
}

//=============================================================================
// 1パス分のバインドと描画
//=============================================================================
void BloomPass::BindAndDraw(ID3D12PipelineState* pso, const Param& param) {
	MY_ASSERT_MSG(slot_ < kMaxPassPerFrame, "1フレームのブルームパス数が上限を超えました");
	auto* cmdList = DirectXCommon::GetCommandList();

	size_t offset = slot_ * kSlotSize;
	std::memcpy(cbMapped_ + offset, &param, sizeof(Param));
	++slot_;

	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootSignature(rsInfo_.rootSignature.Get());
	cmdList->SetPipelineState(pso);
	cmdList->SetGraphicsRootConstantBufferView(rsInfo_.slotOf.at(RootBind::Bloom), cb_->GetGPUVirtualAddress() + offset);
	cmdList->SetGraphicsRootDescriptorTable(rsInfo_.slotOf.at(RootBind::BindlessTexture), DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0); // フルスクリーン三角形
}

//=============================================================================
// 光の層を作る
//=============================================================================
void BloomPass::Record(RenderTexture& scene) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// 描画先ごとにビューポートを張り直す必要がある
	auto setTarget = [&](RenderTexture& target) {
		target.PreDraw();
		D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(target.GetWidth()), static_cast<float>(target.GetHeight()), 0.0f, 1.0f};
		D3D12_RECT sc{0, 0, static_cast<LONG>(target.GetWidth()), static_cast<LONG>(target.GetHeight())};
		cmdList->RSSetViewports(1, &vp);
		cmdList->RSSetScissorRects(1, &sc);
	};

	// --- 明るい部分を抜き出して半分の解像度へ ---
	{
		Param p{};
		p.texelSize[0] = 1.0f / scene.GetWidth();
		p.texelSize[1] = 1.0f / scene.GetHeight();
		p.threshold = kThreshold;
		p.knee = std::max(kKnee, 0.0001f);
		p.srcIndex = scene.GetSRVSlot();
		setTarget(*down_[0]);
		BindAndDraw(brightPSO_.Get(), p);
		down_[0]->PostDraw();
	}

	// --- 段々に縮めながらぼかす ---
	for (uint32_t i = 1; i < kLevelCount; ++i) {
		Param p{};
		p.texelSize[0] = 1.0f / down_[i - 1]->GetWidth();
		p.texelSize[1] = 1.0f / down_[i - 1]->GetHeight();
		p.srcIndex = down_[i - 1]->GetSRVSlot();
		setTarget(*down_[i]);
		BindAndDraw(downPSO_.Get(), p);
		down_[i]->PostDraw();
	}

	// --- 一番小さい段から戻しながら、同じ大きさの縮小結果を足していく ---
	for (int i = static_cast<int>(kLevelCount) - 2; i >= 0; --i) {
		RenderTexture& src = (i == static_cast<int>(kLevelCount) - 2) ? *down_[kLevelCount - 1] : *up_[i + 1];
		Param p{};
		p.texelSize[0] = 1.0f / src.GetWidth();
		p.texelSize[1] = 1.0f / src.GetHeight();
		p.srcIndex = src.GetSRVSlot();
		p.addIndex = down_[i]->GetSRVSlot();
		setTarget(*up_[i]);
		BindAndDraw(upPSO_.Get(), p);
		up_[i]->PostDraw();
	}
}

//=============================================================================
// 合成（描画先は呼び出し側が設定済み）
//=============================================================================
void BloomPass::Composite(RenderTexture& scene, float width, float height) {
	auto* cmdList = DirectXCommon::GetCommandList();
	D3D12_VIEWPORT vp{0.0f, 0.0f, width, height, 0.0f, 1.0f};
	D3D12_RECT sc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);

	Param p{};
	p.intensity = kIntensity;
	p.srcIndex = scene.GetSRVSlot();
	p.addIndex = up_[0]->GetSRVSlot();
	BindAndDraw(compositePSO_.Get(), p);
}