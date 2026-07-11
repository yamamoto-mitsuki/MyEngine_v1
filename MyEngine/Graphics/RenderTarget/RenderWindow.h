#pragma once
#include <vector>

#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 前方宣言
class Win32Window;


/// <summary> 
/// Windowに描画する準備（バリア変更、リセット）を行うクラス
/// </summary>
class RenderWindow {
public:
	// Windowの画面クリアの色（RGBAの順）
	static constexpr float kClearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};

public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="window">描画したいWindow</param>
	void Initialize(Win32Window* window);

	/// <summary>
	/// 描画開始処理（バリア設定・RTVとDSVのクリア）
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 描画後転送処理（バリアを戻す）
	/// </summary>
	void PostDraw();

	/// <summary>
	/// ウィンドウサイズ変更時の描画位置調整
	/// </summary>
	void Resize(int width, int height, bool needWait = true);

	// ゲッター
	IDXGISwapChain4* GetSwapChain() const { return swapChain_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const { return rtvHandles_[currentBackBufferIndex_]; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }
	ID3D12Resource* GetWindowSizeBuffer() const { return windowSizeBuffer_.Get(); }

private:
	// 包含
	Win32Window* window_ = nullptr;
	
	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[DirectXCommon::kSwapChainBufferCount] = {nullptr};
	// RTV
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[DirectXCommon::kSwapChainBufferCount]{};
	// DSV
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};
	// ウィンドウサイズ定数バッファ（2DシェーダーのNDC変換用）
	Microsoft::WRL::ComPtr<ID3D12Resource> windowSizeBuffer_ = nullptr;
	float* windowSizeMappedPtr_ = nullptr;

	UINT currentBackBufferIndex_ = 0;
};