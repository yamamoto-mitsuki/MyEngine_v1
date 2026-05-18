#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Utils/Const.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Render/ShaderStructs.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Window/Win32Window.h"
#include <cassert>
#include <format>

void RenderWindow::Init(Win32Window* window, DirectXCommon* dxCommon) {
	// 対象情報の保持
	window_ = window;
	dxCommon_ = dxCommon;
	RECT wr;
	GetClientRect(window_->GetHWND(), &wr);

	// ===== スワップチェーンの生成 =====
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = wr.right - wr.left;                     // 画面の幅。ウィンドウのクライアント領域と同じにしておく
	swapChainDesc.Height = wr.bottom - wr.top;                    // 画面の高さ。ウィンドウのクライアント領域と同じにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;            // 色の形式
	swapChainDesc.SampleDesc.Count = 1;                           // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = dxCommon_->kSwapChainBufferCount; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;     // モニタにうつしたら、中身を破棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	HRESULT hr = dxCommon_->GetFactory()->CreateSwapChainForHwnd(
	    dxCommon_->GetCommandQueue(), window_->GetHWND(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(false && "スワップチェーンが生成できませんでした");
	}

	// ===== スワップチェーンからResourceを取得 =====
	hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(false && "スワップチェーンが持っている1つ目のResourceを取得できませんでした");
	}
	hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(false && "スワップチェーンが持っている2つ目のResourceを取得できませんでした");
	}

	// ===== RTVDescriptorHeapを作成 =====
	rtvDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, dxCommon_->kSwapChainBufferCount, false);

	// ===== RTVの作成 =====
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;      // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	// 1つ目
	rtvHandles_[0] = dxCommon_->GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);
	dxCommon_->GetDevice()->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);
	// 2つ目
	rtvHandles_[1] = dxCommon_->GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	dxCommon_->GetDevice()->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);

	// ===== DepthStencilResourceの生成 =====
	depthStencilResource_ = dxCommon_->CreateDepthStencilTextureResource(dxCommon_->GetDevice(), wr.right - wr.left, wr.bottom - wr.top);

	// ===== DSVDescriptorHeapの生成 =====
	dsvDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// ===== DSVの生成 =====
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;        // Resourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	dxCommon_->GetDevice()->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);

	// ===== ウィンドウサイズ定数バッファの生成（2DシェーダーのNDC変換用）=====
	windowSizeBuffer_ = dxCommon_->CreateBufferResource(sizeof(float) * 4);
	windowSizeBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&windowSizeMappedPtr_));

	// ウィンドウサイズ保存
	windowSizeMappedPtr_[0] = static_cast<float>(wr.right - wr.left);
	windowSizeMappedPtr_[1] = static_cast<float>(wr.bottom - wr.top);
	windowSizeMappedPtr_[2] = 0.0f;
	windowSizeMappedPtr_[3] = 0.0f;
}

void RenderWindow::PreDraw() {
	// これから書き込むバックバッファのインデックスを取得
	currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

	// ===== トランジションバリアを張る（PresentからRenderTargetへ）=====
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources_[currentBackBufferIndex_].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);

	// ===== RTVとDSVをセットしてクリア =====
	dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtvHandles_[currentBackBufferIndex_], false, &dsvHandle_);
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles_[currentBackBufferIndex_], clearColor, 0, nullptr);
	dxCommon_->GetCommandList()->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void RenderWindow::PostDraw() {
	// ===== トランジションバリアを戻す（RenderTargetからPresentへ）=====
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources_[currentBackBufferIndex_].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void RenderWindow::Resize(int width, int height, bool needWait) {
	if (width == 0 || height == 0) {
		return;
	}

	// 描画中の可能性があるためGPU完了を待つ
	if (needWait) {
		dxCommon_->WaitForGPU();
	}

	// スワップチェーンリソース解放
	swapChainResources_[0].Reset();
	swapChainResources_[1].Reset();

	// バッファリサイズ
	HRESULT hr = swapChain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(false && "ウィンドウサイズの変更でBufferSizeを変更できませんでした。");
	}

	// バッファ再取得
	swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
	swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));

	// RTV再生成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvHandles_[0] = dxCommon_->GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);
	dxCommon_->GetDevice()->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);
	rtvHandles_[1] = dxCommon_->GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
	dxCommon_->GetDevice()->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);

	// DSV再生成
	depthStencilResource_ = dxCommon_->CreateDepthStencilTextureResource(dxCommon_->GetDevice(), width, height);
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	dxCommon_->GetDevice()->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);

	// ウィンドウサイズバッファ更新
	if (windowSizeMappedPtr_) {
		windowSizeMappedPtr_[0] = static_cast<float>(width);
		windowSizeMappedPtr_[1] = static_cast<float>(height);
	}
}