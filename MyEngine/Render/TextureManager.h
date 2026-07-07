#pragma once
#include <string>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "MyEngine/Utils/ConvertString.h"

class TextureManager {
public:
	struct TextureData {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource; // TextureResource本体
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{};      // SRVCPUハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};      // SRVGPUハンドル
		uint32_t srvIndex = 0;                           // SRVDescriptorHeap上のスロット番号
	};

public:
	// コピー・ムーブ禁止
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	/// <summary>
	/// 初期化。DirectXCommon生成直後に1回だけ呼ぶ
	/// </summary>
	static void Initialize();

	/// <summary>
	/// 全テクスチャの解放。WinMainのreturn 0の前に呼ぶ
	/// </summary>
	static void Release();

	/// <summary>
	/// テクスチャを読み込んでSRVを登録する。
	/// </summary>
	/// <param name="filePath">読み込む画像ファイルのパス</param>
	/// <returns>登録したテクスチャのSRVスロット番号</returns>
	static uint32_t Load(const std::string& filePath);

	// ゲッター
	static const TextureData* GetTextureData(uint32_t srvIndex);
	static const uint32_t GetWhite1x1TextureHandle() { return instance_->white1x1TextureHandle_; }

private:
	TextureManager() = default;
	~TextureManager() = default;

	static TextureManager* instance_;

	/// <summary>
	/// Textureデータをファイルから読み込む
	/// </summary>
	static DirectX::ScratchImage LoadTextureFromFile(const std::string& filePath);

	/// <summary>
	/// DirectX12のTextureResourceを作る
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	/// <summary>
	/// TextureResourceにCPUからデータを転送する
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

	/// <summary>
	/// SRVをSRVDescriptorHeapに登録する
	/// </summary>
	static void RegisterSRV(ID3D12DescriptorHeap* srvHeap, TextureData& textureData, const DirectX::TexMetadata& metadata);

	// 画像のファイルパスをキー、TextureDataを値とするマップ
	std::unordered_map<std::string, TextureData> textures_;

	uint32_t white1x1TextureHandle_ = 0u; // エンジン組み込みの1x1の白い矩形
};