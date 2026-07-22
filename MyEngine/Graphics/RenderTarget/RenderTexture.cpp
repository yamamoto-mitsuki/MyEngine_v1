#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"

#include <format>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

using namespace Microsoft::WRL;


//=============================================================================
// enum format → DXGI_FORMAT変換
//=============================================================================
DXGI_FORMAT RenderTexture::ToDXGIFormat(RenderTextureFormat format) {
	switch (format) {
	case RenderTextureFormat::SDR: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // SDR
	case RenderTextureFormat::HDR: return DXGI_FORMAT_R16G16B16A16_FLOAT;  // HDR
	}
	MY_ASSERT_MSG(false, "未対応のRenderTextureFormatです");
	return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}


//=============================================================================
// コンストラクタ・デストラクタ
//=============================================================================
// ===== コンストラクタ =====
RenderTexture::RenderTexture(uint32_t width, uint32_t height, RenderTextureFormat format, bool useDepth) {
	// これから作るTextureの設定
	width_ = width;
	height_ = height;
	format_ = ToDXGIFormat(format);
	useDepth_ = useDepth;

	// RTV, SRV作成
	CreateResource();
	// DSV作成するか
	if (useDepth_) {
		CreateDSVResource();
	}

	LogManager::Log(std::format("RenderTexture created {}x{} SRVSlot={}", width, height, srvSlot_));
}

// ===== デストラクタ =====
RenderTexture::~RenderTexture(){}


//=============================================================================
// 描画開始
//=============================================================================
void RenderTexture::PreDraw() {
	// ===== ResourceStateを切り替える =====
	// PIXEL_SHADER_RESOURCE（ImGuiが読める状態）→ RENDER_TARGET（描画先として使える状態）
	DirectXCommon::TransitionBarrier(resource_.Get(),
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // ImGuiが読んでいた状態
	    D3D12_RESOURCE_STATE_RENDER_TARGET);        // 描画先として使える状態

	// ===== RTVとDSVをセットしてクリア =====
	auto* cmdList = DirectXCommon::GetCommandList();
	if (useDepth_) {
		cmdList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle_);
		cmdList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	} else {
		// 深度なし（IBL畳み込みなどフルスクリーンパス用）
		cmdList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
	}
	cmdList->ClearRenderTargetView(rtvHandle_, RenderWindow::kClearColor, 0, nullptr);
}


//=============================================================================
// 描画終了
//=============================================================================
void RenderTexture::PostDraw() {
	// ===== ResourceStateを元に戻す =====
	// RENDER_TARGET（描画先として使える状態）→ PIXEL_SHADER_RESOURCE（ImGuiが読める状態）
	DirectXCommon::TransitionBarrier(resource_.Get(),
	    D3D12_RESOURCE_STATE_RENDER_TARGET,          // 描画先として使っていた状態
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // ImGuiが読める状態
}


//=============================================================================
// RTVとSRV共通のリソースを生成してRTV/SRVを登録する
//=============================================================================
void RenderTexture::CreateResource() {
	// Resource
	resource_ = DirectXCommon::CreateRenderTargetTextureResource(width_, height_, format_, RenderWindow::kClearColor);
	resource_->SetName(L"RenderTexture");

	// ===== RTV登録 =====
	// rtvがどこのアドレスにあるか登録
	rtvDescriptorHeap_ = DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvDescriptorHeap_->SetName(L"RenderTexture_RTVHeap");
	// view情報
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format_;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	DirectXCommon::GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

	// ===== SRV登録 =====
	// srvがどこのアドレスにあるか登録
	srvSlot_ = DirectXCommon::AllocateSRVSlot(); // DescriptorHeapからスロット番号取得
	srvHandleCPU_ = DirectXCommon::GetCPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	srvHandleGPU_ = DirectXCommon::GetGPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	// view情報
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format_;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	DirectXCommon::GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
}


//=============================================================================
// 深度バッファを生成する
//=============================================================================
void RenderTexture::CreateDSVResource() {
	// Resource
	depthResource_ = DirectXCommon::CreateDepthStencilTextureResource(static_cast<int32_t>(width_), static_cast<int32_t>(height_));
	// DSVDescriptorHeap
	dsvDescriptorHeap_ = DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	// DSV
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DirectXCommon::GetDevice()->CreateDepthStencilView(depthResource_.Get(), &dsvDesc, dsvHandle_);
	dsvDescriptorHeap_->SetName(L"RenderTexture_DSVHeap");
}