#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Utils/ConvertString.h"
#include <cassert>
#include <format>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

// staticメンバの定義
DirectXCommon* DirectXCommon::instance_ = nullptr;

//=============================================================================
// NVIパターン
//=============================================================================
void DirectXCommon::Initialize() {
	assert(instance_ == nullptr && "DirectXCommon::Init()を2回呼んでいます");
	instance_ = new DirectXCommon();
	instance_->InitInternal();
}

//=============================================================================
// 解放
// Engine::Finalize()内でTextureManager等の解放が全て終わった後に呼ぶこと
//=============================================================================
void DirectXCommon::Release() {
	assert(instance_ != nullptr && "DirectXCommon::Release()がInit()より先に呼ばれています");
	CloseHandle(instance_->fenceEvent_);
	instance_->fenceEvent_ = nullptr;
	delete instance_;
	instance_ = nullptr;
}

//=============================================================================
// 初期化
// PSO/RootSignatureはPSOManager::Init()で行うため、ここでは生成しない
//=============================================================================
void DirectXCommon::InitInternal() {
#ifdef _DEBUG
	// デバッグレイヤーの有効化（デバッグビルドのみ）
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	// ===== DXGIファクトリーの生成 =====
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		LogManager::Log("[DirectXCommon::InitInternal] " + std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "DXGIファクトリーを生成できませんでした");
	}

	// ===== アダプタの選択 =====
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&instance_->adapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = instance_->adapter_->GetDesc3(&adapterDesc);
		if (FAILED(hr)) {
			LogManager::Log("[DirectXCommon::InitInternal] " + std::format("Error Code: 0x{:08X}", (uint32_t)hr));
			LogManager::Flush();
			assert(false && "アダプターの情報を取得できませんでした");
		}
		// ソフトウェアアダプタは除外する
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			LogManager::Log("[DirectXCommon::InitInternal] " + std::format("Use Adapter:{}", ConvertString(adapterDesc.Description)));
			break;
		}
		instance_->adapter_ = nullptr;
	}
	assert(instance_->adapter_ != nullptr && "適切なアダプタが見つかりませんでした");

	// ===== D3D12Deviceの生成（12.2→12.1→12.0の順で試みる）=====
	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
	const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(instance_->adapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		if (SUCCEEDED(hr)) {
			LogManager::Log("[DirectXCommon::InitInternal] " + std::format("FeatureLevel : {}", featureLevelStrings[i]));
			break;
		}
	}
	assert(device_ != nullptr && "デバイスの生成に失敗しました");
	LogManager::Log("[DirectXCommon::InitInternal] Complete create D3D12Device!!!");

#ifdef _DEBUG
	// デバッグ時にエラー・警告で止まるよう設定
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		// 抑制するメッセージ
		D3D12_MESSAGE_ID denyIds[] = {D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};
		D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
	}
#endif

	// ===== コマンドキューの生成 =====
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::InitInternal] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "コマンドキューの生成に失敗しました");
	}

	// ===== コマンドアロケータの生成 =====
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::InitInternal] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "コマンドアロケータを生成できませんでした");
	}

	// ===== コマンドリストの生成 =====
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::InitInternal] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "コマンドリストを生成できませんでした");
	}

	// ===== フェンス・イベントの生成 =====
	hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr) && "フェンスの生成に失敗しました");
	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent_ != nullptr && "フェンスイベントの生成に失敗しました");

	// ===== DXCコンパイラの初期化 =====
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr) && "DxcUtilsを生成できませんでした");
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr) && "DxcCompilerを生成できませんでした");
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr) && "DXC IncludeHandlerを生成できませんでした");

	// ===== SRVDescriptorHeapの生成 =====
	// スロット0: ImGui用、スロット1〜: AllocateSRVSlot()で発行
	srvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kSRVDescriptorHeap, true);

	// ===== DescriptorSizeをキャッシュ =====
	descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	LogManager::Log("[DirectXCommon::InitInternal] 初期化完了");
}

//=============================================================================
// GPUの処理完了待ち
//=============================================================================
void DirectXCommon::WaitForGPU() {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");
	instance_->fenceValue_++;
	instance_->commandQueue_->Signal(instance_->fence_.Get(), instance_->fenceValue_);

	if (instance_->fence_->GetCompletedValue() < instance_->fenceValue_) {
		instance_->fence_->SetEventOnCompletion(instance_->fenceValue_, instance_->fenceEvent_);
		WaitForSingleObject(instance_->fenceEvent_, INFINITE);
	}
}

//=============================================================================
// SRVスロット番号の発行
// スロット0はImGui予約済みのため1から開始する
//=============================================================================
uint32_t DirectXCommon::AllocateSRVSlot() {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");
	assert(instance_->nextSrvSlot_ < kSRVDescriptorHeap && "SRVDescriptorHeapのスロットが不足しています");
	return instance_->nextSrvSlot_++;
}

//=============================================================================
// シェーダーのコンパイル
//=============================================================================
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler, const std::vector<std::wstring>& defines) {

	wchar_t currentDir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, currentDir);
	LogManager::Log(ConvertString(std::format(L"[DirectXCommon::CompileShader] CurrentDir: {}", currentDir)));
	LogManager::Log(ConvertString(std::format(L"[DirectXCommon::CompileShader] FullPath: {}\\{}", currentDir, filePath)));
	LogManager::Log(ConvertString(std::format(L"[DirectXCommon::CompileShader] Begin CompileShader, path:{}, profile:{}", filePath, profile)));

	// コンパイル引数の設定
	std::vector<LPCWSTR> arguments = {
	    filePath.c_str(), L"-E", L"main", L"-T", profile, L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr",
	};
	// プリプロセッサ定義を追加
	for (const std::wstring& def : defines) {
		arguments.push_back(L"-D");
		arguments.push_back(def.c_str());
	}

	// ファイル読み込み
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CompileShader] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "シェーダーファイルを読み込めませんでした");
	}

	// コンパイル実行
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
	hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments.data(), static_cast<UINT32>(arguments.size()), includeHandler, IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr) && "Dxcが起動できませんでした");

	// エラーチェック
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		std::string s = "[DirectXCommon::CompileShader] ";
		LogManager::Log(s + shaderError->GetStringPointer());
		assert(false && "シェーダーをコンパイルできませんでした");
	}

	// バイナリ取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	if (FAILED(hr)) {
		LogManager::Flush();
		assert(false && "シェーダーバイナリの取得に失敗しました");
	}

	LogManager::Log(ConvertString(std::format(L"[DirectXCommon::CompileShader] Compile Succeeded, path:{}, profile:{}", filePath, profile)));
	return shaderBlob;
}

//=============================================================================
// PSOの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12PipelineState> DirectXCommon::CreatePSO(
    IDxcBlob* vs, IDxcBlob* ps, D3D12_INPUT_LAYOUT_DESC inputLayout, ID3D12RootSignature* rootSignature, D3D12_DEPTH_STENCIL_DESC depthStencilDesc, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType) {

	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");
	assert(vs != nullptr && "VSがnullです");
	assert(ps != nullptr && "PSがnullです");
	assert(rootSignature != nullptr && "RootSignatureがnullです");

	// ===== BlendStateの設定（アルファブレンド）=====
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;      
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;          // 加算
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;       // アルファのソースは1
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;     // アルファのデスティネーションは0
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;     // 加算

	// ===== RasterizerStateの設定 =====
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 裏面カリング
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ソリッド描画

	// ===== PSOの設定 =====
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature;
	desc.InputLayout = inputLayout;
	desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	desc.BlendState = blendDesc;
	desc.RasterizerState = rasterizerDesc;
	desc.DepthStencilState = depthStencilDesc;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.PrimitiveTopologyType = topologyType;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = instance_->device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CreatePSO] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "PSOの生成に失敗しました");
	}
	return pso;
}

//=============================================================================
// BufferResourceの生成
// 256バイトアライメントは内部で自動処理する
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");
	size_t alignedSize = (sizeInBytes + 255) & ~255;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = alignedSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CreateBufferResource] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "BufferResourceの生成に失敗しました");
	}
	return resource;
}

//=============================================================================
// DepthStencilTextureResourceの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CreateDepthStencilTextureResource] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "DepthStencilTextureResourceの生成に失敗しました");
	}
	return resource;
}

//=============================================================================
// DescriptorHeapの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = heapType;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	                           : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	HRESULT hr = instance_->device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CreateDescriptorHeap] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "DescriptorHeapの生成に失敗しました");
	}
	return descriptorHeap;
}

//=============================================================================
// トランジションバリアを1つ発行する
//=============================================================================
void DirectXCommon::TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	assert(instance_ && "[DirectXCommon::TransitionBarrier] Init()を先に呼んでください");
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	instance_->commandList_->ResourceBarrier(1, &barrier);
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
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		barriers.push_back(barrier);
	}
	// 1回のAPIコールで全バリアを発行する
	instance_->commandList_->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

//=============================================================================
// CPUDescriptorHandle取得
//=============================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index) {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");
	uint32_t descriptorSize = 0;
	switch (heapType) {
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: descriptorSize = instance_->descriptorSizeSRV_; break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:         descriptorSize = instance_->descriptorSizeRTV_; break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:         descriptorSize = instance_->descriptorSizeDSV_; break;
	default: assert(false && "未対応のDescriptorHeapTypeです"); break;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}

//=============================================================================
// GPUDescriptorHandle取得
//=============================================================================
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index) {
	assert(instance_ && "DirectXCommon::Init()を先に呼んでください");
	uint32_t descriptorSize = 0;
	switch (heapType) {
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: descriptorSize = instance_->descriptorSizeSRV_; break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:         descriptorSize = instance_->descriptorSizeRTV_; break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:         descriptorSize = instance_->descriptorSizeDSV_; break;
	default: assert(false && "未対応のDescriptorHeapTypeです"); break;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}

//=============================================================================
// ゲッター
//=============================================================================
IDXGIFactory7*             DirectXCommon::GetFactory()           { assert(instance_); return instance_->dxgiFactory_.Get(); }
IDXGIAdapter4*             DirectXCommon::GetAdapter4()          { assert(instance_); return instance_->adapter_.Get(); }
ID3D12Device*              DirectXCommon::GetDevice()            {  assert(instance_); return instance_->device_.Get();}
ID3D12CommandQueue*        DirectXCommon::GetCommandQueue()      { assert(instance_); return instance_->commandQueue_.Get(); }
ID3D12CommandAllocator*    DirectXCommon::GetCommandAllocator()  { assert(instance_); return instance_->commandAllocator_.Get(); }
ID3D12GraphicsCommandList* DirectXCommon::GetCommandList()       { assert(instance_); return instance_->commandList_.Get(); }
ID3D12DescriptorHeap*      DirectXCommon::GetSRVDescriptorHeap() { assert(instance_); return instance_->srvDescriptorHeap_.Get(); }
IDxcUtils*                 DirectXCommon::GetDxcUtils()          { assert(instance_); return instance_->dxcUtils_.Get(); }
IDxcCompiler3*             DirectXCommon::GetDxcCompiler()       { assert(instance_); return instance_->dxcCompiler_.Get(); }
IDxcIncludeHandler*        DirectXCommon::GetIncludeHandler()    { assert(instance_); return instance_->includeHandler_.Get(); }
uint32_t                   DirectXCommon::GetDescriptorSizeSRV() { assert(instance_); return instance_->descriptorSizeSRV_; }
uint32_t                   DirectXCommon::GetDescriptorSizeRTV() { assert(instance_); return instance_->descriptorSizeRTV_; }
uint32_t                   DirectXCommon::GetDescriptorSizeDSV() { assert(instance_); return instance_->descriptorSizeDSV_; }
uint32_t                   DirectXCommon::GetDrawCallCount()     { assert(instance_); return instance_->drawCallCount_;}