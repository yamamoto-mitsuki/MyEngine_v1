#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Debug/MyAssert.h"
#include <format>
#include "externals/DirectXTex/d3dx12.h"

TextureManager& TextureManager::GetInstance() {
	static TextureManager instance;
	return instance;
}

//=============================================================================
// 初期化
//=============================================================================
void TextureManager::Init(DirectXCommon* dxCommon) {
	GetInstance().dxCommon_ = dxCommon; 
}

//=============================================================================
// 解放
//=============================================================================
void TextureManager::Release() {
	GetInstance().textures_.clear();
}

//=============================================================================
// 読み込み
//=============================================================================
uint32_t TextureManager::Load(const std::string& filePath) {
	// dxCommon_ が初期化されているか確認
	auto& inst = GetInstance();
	MY_ASSERT_MSG(inst.dxCommon_ , "TextureManager::Init() を先に呼んでください");
	
	// すでに読み込まれているか確認
	auto it = inst.textures_.find(filePath);
	if (it != inst.textures_.end()) {
		return it->second.srvIndex;
	}
	// ファイルを読む
	DirectX::ScratchImage mipImages = LoadTextureFromFile(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	// TextureResourceを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = CreateTextureResource(metadata);
	// IntermediateResourceを作り、コマンドを積む
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(resource.Get(), mipImages);
	// CommandQueueでコピーコマンドを実行する
	HRESULT hr = inst.dxCommon_->GetCommandList()->Close();
	MY_ASSERT_MSG(SUCCEEDED(hr), "コマンドリストのクローズに失敗しました");
	ID3D12CommandList* commandLists[] = {inst.dxCommon_->GetCommandList()};
	inst.dxCommon_->GetCommandQueue()->ExecuteCommandLists(1, commandLists);
	// 実行完了を待つ
	inst.dxCommon_->WaitForGPU();
	// Resourceの状態をコピー元からテクスチャ用に遷移する
	inst.dxCommon_->GetCommandAllocator()->Reset();
	inst.dxCommon_->GetCommandList()->Reset(inst.dxCommon_->GetCommandAllocator(), nullptr);
	
	// SRVを登録
	TextureData textureData;
	textureData.resource = std::move(resource);
	textureData.srvIndex = inst.dxCommon_->AllocateSRVSlot();
	RegisterSRV(inst.dxCommon_, inst.dxCommon_->GetSRVDescriptorHeap(), textureData, metadata);
	// マップに登録
	uint32_t index = textureData.srvIndex;
	inst.textures_[filePath] = std::move(textureData);

	LogManager::Log(std::format("[TextureManager] Loaded: {} -> SRVslot {}", filePath, index));
	LogManager::Log(std::format("[TextureManager] filePath: {} format: {}", filePath, (uint32_t)metadata.format));
	return index;
}

//=============================================================================
//
//=============================================================================
DirectX::ScratchImage TextureManager::LoadTextureFromFile(const std::string& filePath) {
	// テクスチャファイルを読み込む
	DirectX::ScratchImage image{};
	std::wstring wFilePath = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	if (FAILED(hr)) {
		LogManager::Log(std::format("Failed to load texture from file: {}", filePath));
		LogManager::Flush();
		MY_ASSERT_MSG(SUCCEEDED(hr), "テクスチャの読み込みに失敗しました");
	}
	// ミップマップの生成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), 
		DirectX::TEX_FILTER_SRGB, 0, mipImages);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[TextureManager] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		MY_ASSERT_MSG(false, "GenerateMipMaps: ミップマップの生成に失敗しました");
	}

	return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata) {
	auto& inst = GetInstance();
	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き(3DTexture)または配列サイズ(TextureArray, CubeMap)
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // ミップマップの数
	resourceDesc.Format = DirectX::MakeSRGB(metadata.format);
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension); // Textureの次元数
	// 利用するヒープの設定
	 D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // GPUのみ高速で読み書きできる
	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = inst.dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定
	    &resourceDesc,        // Resourceの設定
	    D3D12_RESOURCE_STATE_COPY_DEST, // CPUがロードしたデータを転送するためこれ
	    nullptr,   // Clear最適値。使わないのでnullptr
	    IID_PPV_ARGS(&resource));  // 作成するResourceポインタへのポインタ
	if (FAILED(hr)) {
		LogManager::Log(std::format("[TextureManager] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		MY_ASSERT_MSG(false, "CreateTextureResource: TextureResourceの作成に失敗しました");
	}

	return resource;
}

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
	auto& inst = GetInstance();
	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	HRESULT hr = DirectX::PrepareUpload(inst.dxCommon_->GetDevice(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subResources);
	MY_ASSERT_MSG(SUCCEEDED(hr), "PrepareUpload: サブリソースの準備に失敗しました");

	// ===== IntermediateResource(UploadHeap上のBuffer)を作る =====
	// 必要なバッファサイズをDirectX12に計算させる
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, static_cast<UINT>(subResources.size()));
	
	// UploadHeapはCPUが書き込めてGPUが読めるヒープ。TextureResourceにデータを転送するための中継地点として使う。
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC uploadResourceDesc{};
	uploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadResourceDesc.Width = intermediateSize;
	uploadResourceDesc.Height = 1;
	uploadResourceDesc.DepthOrArraySize = 1;
	uploadResourceDesc.MipLevels = 1;
	uploadResourceDesc.SampleDesc.Count = 1;
	uploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	hr = inst.dxCommon_->GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, 
		&uploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource));
	MY_ASSERT_MSG(SUCCEEDED(hr), "IntermediateResource: 作成に失敗しました");

	// ===== コマンドを積む =====
	UpdateSubresources(inst.dxCommon_->GetCommandList(), texture, intermediateResource.Get(), 0, 0, static_cast<UINT>(subResources.size()), subResources.data());

	// ===== ResourceStateを変更 =====
	DirectXCommon::TransitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

	// Load()側でWaitForGPUが完了するまで解放されないようにreturnで返す
	return intermediateResource;
}

void TextureManager::RegisterSRV(DirectXCommon* dxCommon, ID3D12DescriptorHeap* srvHeap, TextureData& textureData, 
	const DirectX::TexMetadata& metadata) {
	// ===== ハンドル取得 =====
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = dxCommon->GetCPUDescriptorHandle(srvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, textureData.srvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = dxCommon->GetGPUDescriptorHandle(srvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, textureData.srvIndex);
	textureData.srvHandleCPU = cpuHandle;
	textureData.srvHandleGPU = gpuHandle;

	// ===== SRVDesc の設定 =====
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DirectX::MakeSRGB(metadata.format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// ===== SRVの生成 =====
	dxCommon->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, cpuHandle);
}

const TextureManager::TextureData* TextureManager::GetTextureData(uint32_t srvIndex) {
	for (auto& [path, data] : GetInstance().textures_) {
		if (data.srvIndex == srvIndex) {
			return &data;
		}
	}

	return nullptr;
}