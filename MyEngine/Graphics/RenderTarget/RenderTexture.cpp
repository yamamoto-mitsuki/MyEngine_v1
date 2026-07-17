#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"

#include <format>
#include <cassert>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

using namespace Microsoft::WRL;
RenderTexture* RenderTexture::instance_ = nullptr;


//=============================================================================
// 初期化
//=============================================================================
void RenderTexture::Initialize(uint32_t width, uint32_t height) {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()を2回以上呼んでいます");
	instance_ = new RenderTexture();
	instance_->width_ = width;
	instance_->height_ = height;
	instance_->CreateResource();
	instance_->CreateDSVResource();
	LogManager::Log(std::format("Initialized {}x{} SRVSlot={}", width, height, instance_->srvSlot_));
	LogManager::Log("Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void RenderTexture::Release() {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()より先にRelease()が呼ばれています");
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}

//=============================================================================
// 描画開始
//=============================================================================
void RenderTexture::PreDraw() {
	MY_ASSERT_MSG(instance_ ,"Initialize()を先に呼んでください");

	// ===== ResourceStateを切り替える =====
	// PIXEL_SHADER_RESOURCE（ImGuiが読める状態）→ RENDER_TARGET（描画先として使える状態）
	DirectXCommon::TransitionBarrier(instance_->resource_.Get(),
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // ImGuiが読んでいた状態
	    D3D12_RESOURCE_STATE_RENDER_TARGET);        // 描画先として使える状態

	// ===== RTVとDSVをセットしてクリア =====
	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->OMSetRenderTargets(1, &instance_->rtvHandle_, false, &instance_->dsvHandle_);
	cmdList->ClearRenderTargetView(instance_->rtvHandle_, RenderWindow::kClearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(instance_->dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

//=============================================================================
// 描画終了
//=============================================================================
void RenderTexture::PostDraw() {
	MY_ASSERT_MSG(instance_ , "Initialize()を先に呼んでください");

	// ===== ResourceStateを元に戻す =====
	// RENDER_TARGET（描画先として使える状態）→ PIXEL_SHADER_RESOURCE（ImGuiが読める状態）
	DirectXCommon::TransitionBarrier(instance_->resource_.Get(),
	    D3D12_RESOURCE_STATE_RENDER_TARGET,          // 描画先として使っていた状態
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // ImGuiが読める状態
}

//=============================================================================
// RTVとSRV共通のリソースを生成してRTV/SRVを登録する
//=============================================================================
void RenderTexture::CreateResource() {
	// Resource
	resource_ = DirectXCommon::CreateRenderTargetTextureResource(width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, RenderWindow::kClearColor);
	resource_->SetName(L"RenderTexture");

	// ===== RTV登録 =====
	// rtvがどこのアドレスにあるか登録
	rtvDescriptorHeap_ = DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvDescriptorHeap_->SetName(L"RenderTexture_RTVHeap");
	// view情報
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	DirectXCommon::GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

	// ===== SRV登録 =====
	// srvがどこのアドレスにあるか登録
	srvSlot_ = DirectXCommon::AllocateSRVSlot(); // DescriptorHeapからスロット番号取得
	srvHandleCPU_ = DirectXCommon::GetCPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	srvHandleGPU_ = DirectXCommon::GetGPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	// view情報
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
	    D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0, // R
	    D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1, // G
	    D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2, // B
	    D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);          // A = 1.0 に強制
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