#include "MyEngine/Render/RenderTexture.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Render/RenderWindow.h"
#include <cassert>
#include <format>

using namespace Microsoft::WRL;

RenderTexture* RenderTexture::instance_ = nullptr;

//=============================================================================
// 初期化
//=============================================================================
void RenderTexture::Initialize(uint32_t width, uint32_t height) {
	assert(instance_ == nullptr && "[RenderTexture::Initialize] Initialize()を2回以上呼んでいます");
	instance_ = new RenderTexture();
	instance_->width_ = width;
	instance_->height_ = height;
	instance_->CreateResource();
	instance_->CreateDSVResource();
	LogManager::Log(std::format("[RenderTexture::Initialize] 初期化完了 {}x{} SRVslot={}", width, height, instance_->srvSlot_));
}

//=============================================================================
// 解放
//=============================================================================
void RenderTexture::Release() {
	assert(instance_ != nullptr && "[RenderTexture::Release] Initialize()より先にRelease()が呼ばれています");
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("[RenderTexture::Release] 解放完了");
}

//=============================================================================
// 描画開始
//=============================================================================
void RenderTexture::PreDraw() {
	assert(instance_ && "[RenderTexture::PreDraw] Initialize()を先に呼んでください");

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
	assert(instance_ && "[RenderTexture::PostDraw] Initialize()を先に呼んでください");

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
	// ===== Resourceの設定 =====
	D3D12_RESOURCE_DESC desc{};
	desc.Width = UINT(width_);
	desc.Height = UINT(height_);
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // RenderWindowと同じ色形式
	desc.MipLevels = 1;
	desc.DepthOrArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // RTVとして使うために必要なフラグ
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // GPUのみが高速にアクセスできるメモリ
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	clearValue.Color[0] = RenderWindow::kClearColor[0];
	clearValue.Color[1] = RenderWindow::kClearColor[1];
	clearValue.Color[2] = RenderWindow::kClearColor[2];
	clearValue.Color[3] = RenderWindow::kClearColor[3];
	HRESULT hr = DirectXCommon::GetDevice()->CreateCommittedResource(
	    &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 初期状態
	    &clearValue, IID_PPV_ARGS(&resource_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[RenderTexture::CreateResource] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "[RenderTexture::CreateResource] Resourceの生成に失敗しました");
	}

	// ===== RTV登録 =====
	rtvDescriptorHeap_ = DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	DirectXCommon::GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

	// ===== SRV登録 =====
	srvSlot_ = DirectXCommon::AllocateSRVSlot(); // DescriptorHeapからスロット番号取得
	srvHandleCPU_ = DirectXCommon::GetCPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	srvHandleGPU_ = DirectXCommon::GetGPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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
}