#include "MyEngine/Graphics/GPU/DirectXCommon.h"

#include <array>
#include <cassert>
#include <format>

#include <externals/DirectXTex/d3dx12.h>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Diagnostics/GraphicsDebug.h"
#include "MyEngine/String/ConvertString.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// staticメンバの定義
std::unique_ptr<DirectXCommon> DirectXCommon::instance_ = nullptr;


//=============================================================================
// 初期化（呼び出し用）
//=============================================================================
void DirectXCommon::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()を2回以上呼んでいます");
	instance_ = std::make_unique<DirectXCommon>(ConstructorKey());
	instance_->InitInternal();
}

//=============================================================================
// 解放
// Engine::Finalize()内でTextureManager等の解放が全て終わった後に呼ぶこと
//=============================================================================
void DirectXCommon::Release() {
	MY_ASSERT_MSG(instance_ != nullptr, "DirectXCommon::Release()がInitialize()より先に呼ばれています");
	// フェンス
	CloseHandle(instance_->fenceEvent_);
	instance_->fenceEvent_ = nullptr;
	// インスタンス
	instance_.reset();
}

//=============================================================================
// 初期化
// PSO/RootSignatureはPSOManager::Initialize()で行うため、ここでは生成しない
//=============================================================================
void DirectXCommon::InitInternal() {
	// ===== デバッグレイヤー・DRED（Debugだけ。デバイスより先）=====
	// Graphics Toolsがある場合はDXGIのデバッグを有効化するため、フラグが返ってくる
	UINT dxgiFactoryFlags = GraphicsDebug::EnableDebugLayer();

	// ===== DXGIファクトリーの生成 =====
	HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "DXGIファクトリーを生成できませんでした");
	}

	// ===== アダプタの選択 =====
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = adapter_->GetDesc3(&adapterDesc);
		if (FAILED(hr)) {
			LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
			MY_ASSERT_MSG(false, "アダプターの情報を取得できませんでした");
		}
		// ソフトウェアアダプタ（GPUではなく、CPUで描画するエミュレータ）は除外する
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			LogManager::Log(std::format("Use Adapter:{}", ConvertString(adapterDesc.Description)));
			break;
		}
		adapter_ = nullptr;
	}
	MY_ASSERT_MSG(adapter_ != nullptr, "適切なアダプタが見つかりませんでした");

	// ===== D3D12Deviceの生成（12.2→12.1→12.0の順で試みる）=====
	const std::array<D3D_FEATURE_LEVEL, 3> featureLevels = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
	const std::array<const char*, 3> featureLevelStrings = {"12.2", "12.1", "12.0"};
	for (size_t i = 0; i < featureLevels.size(); ++i) {
		hr = D3D12CreateDevice(adapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		if (SUCCEEDED(hr)) {
			LogManager::Log(std::format("FeatureLevel : {}", featureLevelStrings[i]));
			break;
		}
	}
	MY_ASSERT_MSG(device_ != nullptr, "デバイスの生成に失敗しました");
	LogManager::Log("Complete create D3D12Device!!!");
#ifdef _DEBUG
	device_->SetName(L"MainDevice");
#endif

	// ===== エラー・警告で止まる・ログに出す（Debugだけ。デバイスの後）=====
	GraphicsDebug::SetupInfoQueue(device_.Get());

	// ===== コマンドキューの生成 =====
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "コマンドキューの生成に失敗しました");
	}
	commandQueue_->SetName(L"MainCommandQueue");

	// ===== コマンドアロケータの生成 =====
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "コマンドアロケータを生成できませんでした");
	}
	commandAllocator_->SetName(L"MainCommandAllocator");

	// ===== コマンドリストの生成 =====
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "コマンドリストを生成できませんでした");
	}
	commandList_->SetName(L"MainCommandList");

	// ===== フェンス・イベントの生成 =====
	hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "フェンスの生成に失敗しました");
	}
	fence_->SetName(L"MainFence");
	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	MY_ASSERT_MSG(fenceEvent_ != nullptr, "フェンスイベントの生成に失敗しました");

	// ===== DXCコンパイラの初期化 =====
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "DxcUtilsを生成できませんでした");
	}
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "DxcCompilerを生成できませんでした");
	}
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "DXC IncludeHandlerを生成できませんでした");
	}

	// ===== DescriptorSizeをキャッシュ =====
	descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// ===== SRVDescriptorHeapの生成 =====
	// スロット0: ImGui用（取っておく）、スロット1〜: AllocateSRVSlot()で発行
	srvAllocator_ = std::make_unique<DescriptorAllocator>(kSRVDescriptorHeap, 1, L"SRVDescriptorHeap");

	LogManager::Log("Initialized");
}

//=============================================================================
// GPUの処理完了待ち
//=============================================================================
void DirectXCommon::WaitForGPU() {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	// 目標値を一つ決める
	instance_->fenceValue_++;
	instance_->commandQueue_->Signal(instance_->fence_.Get(), instance_->fenceValue_);

	if (instance_->fence_->GetCompletedValue() < instance_->fenceValue_) {
		// イベントを設定
		instance_->fence_->SetEventOnCompletion(instance_->fenceValue_, instance_->fenceEvent_);
		// イベントを待つ
		WaitForSingleObject(instance_->fenceEvent_, INFINITE);
	}
}

//=============================================================================
// 描画のコマンドリストを実行して待ち、また積める状態に戻す
//=============================================================================
void DirectXCommon::ExecuteAndWait() {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	ID3D12GraphicsCommandList* commandList = instance_->commandList_.Get();
	// 積んだコマンドを確定して、GPUに渡す
	HRESULT hr = commandList->Close();
	MY_ASSERT_MSG(SUCCEEDED(hr), "コマンドリストのクローズに失敗しました");
	std::array<ID3D12CommandList*, 1> commandLists = {commandList};
	instance_->commandQueue_->ExecuteCommandLists(static_cast<UINT>(commandLists.size()), commandLists.data());
	// 終わるまで待つ（待たずにResetすると、GPUが読んでいる途中のコマンドを消してしまう）
	WaitForGPU();
	// また積めるようにする
	instance_->commandAllocator_->Reset();
	commandList->Reset(instance_->commandAllocator_.Get(), nullptr);
}

//=============================================================================
// SRVスロット（中身は DescriptorAllocator）
//=============================================================================
// ===== 発行 =====
uint32_t DirectXCommon::AllocateSRVSlot() {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	return instance_->srvAllocator_->Allocate();
}

// ===== 返却 =====
void DirectXCommon::FreeSRVSlot(uint32_t slot) {
	// 終了のとき、DirectXCommonより後にRenderTextureが消えることがある（ヒープごと無いので、返す先も無い）
	if (instance_ == nullptr) {
		return;
	}
	instance_->srvAllocator_->Free(slot);
}

// ===== 返してもらったスロットを、また配れるようにする =====
void DirectXCommon::RecycleFreedSRVSlots() {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	instance_->srvAllocator_->RecycleFreed();
}

// ===== 今使っている数 =====
uint32_t DirectXCommon::GetUsedSRVSlotCount() {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	return instance_->srvAllocator_->GetUsedCount();
}

//=============================================================================
// バッファを作る（Upload・Default・Readback で共通）
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBuffer(D3D12_HEAP_TYPE heapType, size_t sizeInBytes, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState) {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	// --- どのヒープに置くか ---
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = heapType;
	// --- バッファの形（1列に並んだバイト）---
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = flags; // UAVとして使う（CSから書く）なら ALLOW_UNORDERED_ACCESS が要る
	// --- 作る ---
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, nullptr, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, std::format("バッファの生成に失敗しました（ヒープの種類 = {}, {} バイト）", static_cast<int>(heapType), sizeInBytes));
	}
	return resource;
}

// ===== Uploadヒープ（CPU → GPU）=====
// 定数バッファにも使うので、大きさを256バイトの倍数にそろえる
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUploadBuffer(size_t sizeInBytes) {
	const size_t alignedSize = (sizeInBytes + 255) & ~size_t(255);
	return CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, alignedSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
}

// ===== Defaultヒープ（GPU専用・高速）=====
// バッファは作るときの状態の指定が無視されて COMMON から始まるので、COMMON を渡す
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDefaultBuffer(size_t sizeInBytes, D3D12_RESOURCE_FLAGS flags) {
	const size_t alignedSize = (sizeInBytes + 255) & ~size_t(255);
	return CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, alignedSize, flags, D3D12_RESOURCE_STATE_COMMON);
}

// ===== Readbackヒープ（GPU → CPU）=====
// 読み戻しは定数バッファにしないので、そろえない。Readbackヒープはいつも COPY_DEST（コピーされる先）
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateReadbackBuffer(size_t sizeInBytes) {
	return CreateBuffer(D3D12_HEAP_TYPE_READBACK, sizeInBytes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
}

//=============================================================================
// Uploadバッファを生成し、そのまま永続Mapして先頭ポインタを返す
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateMappedUploadBuffer(size_t sizeInBytes, void** outMappedPtr) {
	auto resource = CreateUploadBuffer(sizeInBytes);
	resource->Map(0, nullptr, outMappedPtr);
	return resource;
}

//=============================================================================
// 2DTextureResourceの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(
    uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clearValue, uint16_t arraySize, uint16_t mipLevels) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	// 設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2DTextureとして扱う
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = arraySize;
	resourceDesc.MipLevels = mipLevels;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = flags;
	// ヒープ
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	// 作成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, clearValue, IID_PPV_ARGS(&resource));
	// エラーチェック
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "TextureResourceの生成に失敗しました");
	}

	return resource;
}

//=============================================================================
// RenderTargetTextureResourceの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTargetTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const float clearColor[4]) {
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = clearColor[0];
	clearValue.Color[1] = clearColor[1];
	clearValue.Color[2] = clearColor[2];
	clearValue.Color[3] = clearColor[3];

	return CreateTextureResource(width, height, format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue);
}

//=============================================================================
// DepthStencilTextureResourceの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDepthStencilTextureResource(int32_t width, int32_t height, bool isShaderReadable) {
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // クリアは深度として書く形（DSV と同じ形）

	// D24_UNORM_S8_UINT で作ると、深度としてしか使えない（読む窓口 SRV を作れない）
	// 型なし（TYPELESS）で作ると、窓口ごとに形を決められる（DSV は D24_UNORM_S8_UINT、SRV は R24_UNORM_X8_TYPELESS）
	const DXGI_FORMAT format = isShaderReadable ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
	return CreateTextureResource(static_cast<uint32_t>(width), static_cast<uint32_t>(height), format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue);
}

//=============================================================================
// DescriptorHeapの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = heapType;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	HRESULT hr = instance_->device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "DescriptorHeapの生成に失敗しました");
	}
	return descriptorHeap;
}

//=============================================================================
// トランジションバリアを1つ発行する
//=============================================================================
// ===== 描画のコマンドリストへ =====
void DirectXCommon::TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	TransitionBarrier(instance_->commandList_.Get(), resource, before, after);
}

// ===== 渡したコマンドリストへ =====
void DirectXCommon::TransitionBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	// 中身の詰め方（全サブリソース・フラグなし）は d3dx12.h の CD3DX12_RESOURCE_BARRIER::Transition にまかせる
	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
	commandList->ResourceBarrier(1, &barrier);
}

//=============================================================================
// トランジションバリアを複数まとめて発行する
//=============================================================================
void DirectXCommon::TransitionBarriers(std::initializer_list<std::tuple<ID3D12Resource*, D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES>> resources) {
	assert(instance_ && "[DirectXCommon::TransitionBarriers] Init()を先に呼んでください");
	assert(resources.size() > 0 && "[DirectXCommon::TransitionBarriers] resourcesが空です");

	// バリアを配列に詰める
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(resources.size());
	for (const auto& [resource, before, after] : resources) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after));
	}
	// 1回のAPIコールで全バリアを発行する
	instance_->commandList_->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

//=============================================================================
// DescriptorHandle取得
//=============================================================================
// ===== ディスクリプタ1個分の大きさ =====
uint32_t DirectXCommon::DescriptorSizeOf(D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	switch (heapType) {
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		return instance_->descriptorSizeSRV_;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
		return instance_->descriptorSizeRTV_;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
		return instance_->descriptorSizeDSV_;
	default:
		MY_ASSERT_MSG(false, "未対応のDescriptorHeapTypeです");
		return 0;
	}
}

// ===== CPU =====
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += DescriptorSizeOf(heapType) * index;
	return handle;
}

// ===== GPU =====
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += DescriptorSizeOf(heapType) * index;
	return handle;
}

// ===== SRV のヒープを見せる =====
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::BindSRVHeap(ID3D12GraphicsCommandList* cmdList) {
	std::array<ID3D12DescriptorHeap*, 1> heaps = {GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
	return heaps[0]->GetGPUDescriptorHandleForHeapStart();
}

//=============================================================================
// デバイスが削除されたか確認
//=============================================================================
bool DirectXCommon::CheckDeviceRemoved() {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	HRESULT reason = instance_->device_->GetDeviceRemovedReason();
	if (FAILED(reason)) {
		GraphicsDebug::ReportDeviceRemoved(instance_->device_.Get(), reason);
		return true;
	}
	return false;
}

//=============================================================================
// ゲッター
//=============================================================================
IDXGIFactory7* DirectXCommon::GetFactory() {
	assert(instance_);
	return instance_->dxgiFactory_.Get();
}
IDXGIAdapter4* DirectXCommon::GetAdapter4() {
	assert(instance_);
	return instance_->adapter_.Get();
}
ID3D12Device* DirectXCommon::GetDevice() {
	assert(instance_);
	return instance_->device_.Get();
}
ID3D12CommandQueue* DirectXCommon::GetCommandQueue() {
	assert(instance_);
	return instance_->commandQueue_.Get();
}
ID3D12CommandAllocator* DirectXCommon::GetCommandAllocator() {
	assert(instance_);
	return instance_->commandAllocator_.Get();
}
ID3D12GraphicsCommandList* DirectXCommon::GetCommandList() {
	assert(instance_);
	return instance_->commandList_.Get();
}
ID3D12DescriptorHeap* DirectXCommon::GetSRVDescriptorHeap() {
	assert(instance_);
	return instance_->srvAllocator_->GetHeap();
}
IDxcUtils* DirectXCommon::GetDxcUtils() {
	assert(instance_);
	return instance_->dxcUtils_.Get();
}
IDxcCompiler3* DirectXCommon::GetDxcCompiler() {
	assert(instance_);
	return instance_->dxcCompiler_.Get();
}
IDxcIncludeHandler* DirectXCommon::GetIncludeHandler() {
	assert(instance_);
	return instance_->includeHandler_.Get();
}
uint32_t DirectXCommon::GetDescriptorSizeSRV() {
	assert(instance_);
	return instance_->descriptorSizeSRV_;
}
uint32_t DirectXCommon::GetDescriptorSizeRTV() {
	assert(instance_);
	return instance_->descriptorSizeRTV_;
}
uint32_t DirectXCommon::GetDescriptorSizeDSV() {
	assert(instance_);
	return instance_->descriptorSizeDSV_;
}
uint32_t DirectXCommon::GetDrawCallCount() {
	assert(instance_);
	return instance_->drawCallCount_;
}