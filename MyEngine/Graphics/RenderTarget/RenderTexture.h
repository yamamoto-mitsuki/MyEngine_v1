#pragma once
#include <cstdint>

#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// IBL使用時にHDR画像として使いたいのでFormatを区別する
enum class RenderTextureFormat {
	SDR, 
	HDR,
};


/// <summary>
/// レンダリング結果を画像に焼き付けるクラス
/// </summary>
class RenderTexture {
public:
	// 1枚作る
	RenderTexture(uint32_t width, uint32_t height, RenderTextureFormat format, bool useDepth = true);
	// SRVスロットの解放
	~RenderTexture();

	// コピー・ムーブ禁止
	RenderTexture(const RenderTexture&) = delete;
	RenderTexture& operator=(const RenderTexture&) = delete;


	/// <summary>
	/// 描画開始
	/// <para>リソースをPIXEL_SHADER_RESOURCE→RENDER_TARGETに切り替えてRTV/DSVをクリアする</para>
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 描画終了
	/// <para>リソースをRENDER_TARGET→PIXEL_SHADER_RESOURCEに戻す</para>
	/// </summary>
	void PostDraw();

	// ゲッター
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() { return srvHandleGPU_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() { return rtvHandle_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return dsvHandle_; }
	uint32_t GetWidth() { return width_; }
	uint32_t GetHeight() { return height_; }


private:
	// ===== 内部ヘルパー =====
	void CreateResource();
	void CreateDSVResource();
	static DXGI_FORMAT ToDXGIFormat(RenderTextureFormat format);
	// RTV, SRV, DSV
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ = {};
	uint32_t srvSlot_ = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_ = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ = {};
	Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};
	// Format
	DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// サイズ
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	// Flag
	bool useDepth_;
};