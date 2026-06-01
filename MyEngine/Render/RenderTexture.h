#pragma once
#include "MyEngine/Render/DirectXCommon.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

// ゲーム画面を1枚のテクスチャとして扱うクラス
class RenderTexture {
public:
	// コピー・ムーブ禁止
	RenderTexture(const RenderTexture&) = delete;
	RenderTexture& operator=(const RenderTexture&) = delete;

	/// <summary>
	/// 初期化
	/// </summary>
	static void Initialize(uint32_t width, uint32_t height);

	/// <summary>
	/// 解放
	/// </summary>
	static void Release();

	/// <summary>
	/// 描画開始
	/// <para>リソースをPIXEL_SHADER_RESOURCE→RENDER_TARGETに切り替えてRTV/DSVをクリアする</para>
	/// </summary>
	static void PreDraw();

	/// <summary>
	/// 描画終了
	/// <para>リソースをRENDER_TARGET→PIXEL_SHADER_RESOURCEに戻す</para>
	/// </summary>
	static void PostDraw();

	// ゲッター
	static D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() { return instance_->srvHandleGPU_; }
	static D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() { return instance_->rtvHandle_; }
	static D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return instance_->dsvHandle_; }
	static uint32_t GetWidth() { return instance_->width_; }
	static uint32_t GetHeight() { return instance_->height_; }

private:
	RenderTexture() = default;
	~RenderTexture() = default;

	static RenderTexture* instance_;

	// 内部ヘルパー
	void CreateResource();
	void CreateDSVResource();

	// RTVとSRVで共有するリソース（1つのリソースにRTVとSRVの両方を登録する）
	ComPtr<ID3D12Resource> resource_ = nullptr;

	// RTV
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ = {};
	// SRV
	uint32_t srvSlot_ = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_ = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ = {};
	// DSV
	ComPtr<ID3D12Resource> depthResource_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};
	// サイズ
	uint32_t width_ = 0;
	uint32_t height_ = 0;
};