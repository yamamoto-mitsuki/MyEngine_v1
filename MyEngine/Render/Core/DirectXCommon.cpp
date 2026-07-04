#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Render/Core/RenderContext.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Utils/ConvertString.h"
#include "MyEngine/Debug/MyAssert.h"
#include <cassert>
#include <format>
#include <stacktrace>
#include <dxgidebug.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// staticメンバの定義
DirectXCommon* DirectXCommon::instance_ = nullptr;

//=============================================================================
// 初期化（呼び出し用）
//=============================================================================
void DirectXCommon::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "DirectXCommon::Initialize()を2回以上呼んでいます");
	instance_ = new DirectXCommon();
	instance_->InitInternal();
}

// Breadcrumbの名前を分かりやすいようにする
std::string DirectXCommon::BreadcrumbOpName(D3D12_AUTO_BREADCRUMB_OP op) {
	switch (op) {
	case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT:
		return "BeginEvent";
	case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT:
		return "EndEvent";
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED:
		return "DrawInstanced";
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:
		return "DrawIndexedInstanced";
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCH:
		return "Dispatch";
	case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE:
		return "CopyResource";
	case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER:
		return "ResourceBarrier";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:
		return "ClearRTV";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW:
		return "ClearDSV";
	case D3D12_AUTO_BREADCRUMB_OP_PRESENT:
		return "Present";
	case D3D12_AUTO_BREADCRUMB_OP_SETMARKER:
		return "SetMarker";
	default:
		return std::format("Op({})", static_cast<int>(op));
	}
}

// .source_file()のファイルパスを省略する
std::string ShortenPath(std::string_view path) { 
	constexpr std::string_view marker = "repos\\"; // ここより前を捨てる
	auto pos = path.rfind(marker);
	return (pos == std::string_view::npos) ? std::string(path) : std::string(path.substr(pos + marker.size()));
}

// 
std::string FuncOf(const std::stacktrace_entry& f) { 
	std::string d = f.description();
	auto bang = d.find('!');
	auto plus = d.rfind('+');
	size_t start = (bang == std::string::npos) ? 0 : bang + 1;
	size_t end = (plus == std::string::npos) ? d.size() : plus;
	return d.substr(start, end - start);
}

// D3D12のデバックメッセージをLogに出力する
void CALLBACK DirectXCommon::D3D12DebugMessageCallback(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, 
													   D3D12_MESSAGE_ID, LPCSTR description, void*) {
	// エラーメッセージ末尾の改行を削除する
	std::string text = description ? description : "";
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
		text.pop_back();
	}
	std::string msg = std::string("[D3D12] ") + text;
	// CPU検証かGBVか確認
	bool isGBV = (text.rfind("GPU-BASED VALIDATION", 0) == 0);
	if (isGBV) {
		msg = "<GBV: 実行時報告。このメッセージを呼んでください。>" + msg;
	} else {
		msg = "<CPU検証: 同期報告。スタックを見てください。>" + msg;
	}

	//  情報の重要度によってLog出力を変える
	switch (severity) {
	// --- やばい ---
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
	case D3D12_MESSAGE_SEVERITY_ERROR:
		LogManager::Error(msg);
		// 呼び出し元のスタックを取得してlogに出す
		for (const std::stacktrace_entry& frame : std::stacktrace::current()) {
			std::string file = frame.source_file();
			// 自分のコード以外スキップ
			if (file.empty() || file.find("repos\\") == std::string::npos) {
				continue;
			}
			
			// D3D12内部などソースファイルがないフレームは飛ばす
			if (!frame.source_file().empty()) {
				LogManager::Error(std::format(" at {}:{} {}", ShortenPath(file), frame.source_line(), FuncOf(frame)));
			}
		}
		MY_ASSERT_MSG(false, "D3D12DebugLayerがエラーを検出しました");
		break;
	// --- 警告 ---
	case D3D12_MESSAGE_SEVERITY_WARNING:
		LogManager::Warning(msg);
		break;
	}
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
	delete instance_;
	instance_ = nullptr;
}

//=============================================================================
// 初期化
// PSO/RootSignatureはPSOManager::Initialize()で行うため、ここでは生成しない
//=============================================================================
void DirectXCommon::InitInternal() {
	// Graphics Toolsがある場合はDXGIのデバッグを有効化するため、フラグを立てる
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	// ===== D3D12デバッグレイヤー =====
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバックレイヤーを有効化
		debugController->EnableDebugLayer();
		// GPU側でもチェックを行う
		debugController->SetEnableGPUBasedValidation(TRUE);
		// Graphics ToolsがあるのでDXGIのデバッグも有効化する
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	// ===== DRED（Device Removed Extended Data） =====
	Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings)))) {
		// 最後に実行したGPU処理を記録
		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		// ページフォルト情報：不正アクセスしたアドレス/リソースを記録
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	}
#endif

	// ===== DXGIファクトリーの生成 =====
	HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "DXGIファクトリーを生成できませんでした");
	}

	// ===== アダプタの選択 =====
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&instance_->adapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = instance_->adapter_->GetDesc3(&adapterDesc);
		if (FAILED(hr)) {
			LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
			MY_ASSERT_MSG(false, "アダプターの情報を取得できませんでした");
		}
		// ソフトウェアアダプタ（GPUではなく、CPUで描画するエミュレータ）は除外する
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			LogManager::Log(std::format("Use Adapter:{}", ConvertString(adapterDesc.Description)));
			break;
		}
		instance_->adapter_ = nullptr;
	}
	MY_ASSERT_MSG(instance_->adapter_ != nullptr, "適切なアダプタが見つかりませんでした");

	// ===== D3D12Deviceの生成（12.2→12.1→12.0の順で試みる）=====
	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
	const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(instance_->adapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
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

#ifdef _DEBUG
	// --- デバッグ時にエラー・警告で止まるよう設定（ID3D12） ---
	Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// エラー・警告でも止まるようにする
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		// Logにも出力するようにする
		DWORD cookie = 0;
		infoQueue->RegisterMessageCallback(D3D12DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &cookie);
		// 抑制するメッセージ
		D3D12_MESSAGE_ID denyIds[] = {D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};
		D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}
	// --- DXGI版 ---
	Microsoft::WRL::ComPtr<IDXGIInfoQueue> dxgiInfoQueue = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)))) {
		// エラー・警告で止まるようにする
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
	}
#endif

	// ===== コマンドキューの生成 =====
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "コマンドキューの生成に失敗しました");
	}

	// ===== コマンドアロケータの生成 =====
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "コマンドアロケータを生成できませんでした");
	}

	// ===== コマンドリストの生成 =====
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "コマンドリストを生成できませんでした");
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
// Uploadヒープ（CPU → GPU）にバッファを生成する
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUploadBuffer(size_t sizeInBytes) {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Init()を先に呼んでください");
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
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, 
															 &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CreateBufferResource] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		MY_ASSERT_MSG(false, "BufferResourceの生成に失敗しました");
	}
	return resource;
}

//=============================================================================
// Defaultヒープ（GPU専用・高速）にバッファを生成する
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDefaultBuffer(size_t sizeInBytes) { 
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に読んでください");
	size_t alignedSize = (sizeInBytes + 255) & ~255;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = alignedSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, 
															 &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));

	if (FAILED(hr)) {
		LogManager::Log(std::format("[DirectXCommon::CreateDefaultBuffer] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		MY_ASSERT_MSG(false, "DefaultBufferの生成に失敗しました");
	}

	return resource;
}

//=============================================================================
// Readbackヒープ（GPU → CPU）にバッファを生成する
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateReadbackBuffer(size_t sizeInBytes) { 
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に読んでください"); 
	// ReadBackは整列不要なので整列不要

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;

	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = instance_->device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "ReadbackBufferの生成に失敗しました");
	}
	return resource;
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
// Deviceロスト時に呼ぶ。DRED（Device Removed Extended Data）の記録をLogに出す
//=============================================================================
void DirectXCommon::ReportDeviceRemoved(HRESULT hr) {
	LogManager::Error(std::format("[DRED] Device removed! reason = 0x{:08X}", uint32_t(hr)));

#ifdef _DEBUG
	// DREDの読み出し
	Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData2> dred;
	if (SUCCEEDED(instance_->device_->QueryInterface(IID_PPV_ARGS(&dred)))) {
		// エラーの名前を安全に取り出す
		auto nameOf = [](const char* a, const wchar_t* w) -> std::string {
			if (w) {
				return ConvertString(std::wstring(w));
			}
			if (a) {
				return a;
			}
			return "(no name)";
		};

		// ===== 1.Auto-Breadcrumbs（GPUがどこまで処理を実行して、止まってしまったか） =====
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 bc{};
		if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&bc))) {
			// コマンドリストの数分ループする
			for (const D3D12_AUTO_BREADCRUMB_NODE1* cmdListNode = bc.pHeadAutoBreadcrumbNode; cmdListNode != nullptr; cmdListNode = cmdListNode->pNext) {
				// GPUが完了した数を取得する。
				UINT32 done = cmdListNode->pLastBreadcrumbValue ? *cmdListNode->pLastBreadcrumbValue : 0;
				// 全部完了していたら原因のコマンドリストではないではない
				if (done >= cmdListNode->BreadcrumbCount) {
					continue;
				}

				// 止まった命令（done）を囲んでいるマーカー名を探す
				// BreadcrumbIndex が done 以下で最大のものがいた区間
				std::string marker = "(no marker)";
				UINT best = 0;
				bool found = false;
				for (UINT i = 0; i < cmdListNode->BreadcrumbContextsCount; ++i) { // BreadcrumbContextsCount は SetMarker/BeginEvent/EndEvent の数
					const auto& c = cmdListNode->pBreadcrumbContexts[i];
					if (c.BreadcrumbIndex <= done && (!found || c.BreadcrumbIndex >= best)) {
						best = c.BreadcrumbIndex;
						marker = c.pContextString ? ConvertString(std::wstring(c.pContextString)) : "(no marker)";
						found = true;
					}
				}
				// コマンドリストの名前と、止まった命令の名前をLogに出す
				std::string clName = nameOf(cmdListNode->pCommandListDebugNameA, cmdListNode->pCommandListDebugNameW);
				std::string op = cmdListNode->pCommandHistory ? instance_->BreadcrumbOpName(cmdListNode->pCommandHistory[done]) : "?";
				LogManager::Error(std::format("[DRED] CmdList '{}' stopped at {}/{}  marker='{}'  operation={}", clName, done, cmdListNode->BreadcrumbCount, marker, op));
			}
		}

		// ===== 2.Page Fault（不正アクセス） =====
		D3D12_DRED_PAGE_FAULT_OUTPUT2 pageFault{};
		// ノードが ALLOCATION_NODE1 になる
		auto dumpAllocations = [&](const char* title, const D3D12_DRED_ALLOCATION_NODE1* head) {
			for (const D3D12_DRED_ALLOCATION_NODE1* node = head; node != nullptr; node = node->pNext) {
				std::string name = nameOf(node->ObjectNameA, node->ObjectNameW);
				LogManager::Error(std::format("[DRED] {} : {}", title, name));
			}
		};

		if (SUCCEEDED(dred->GetPageFaultAllocationOutput2(&pageFault)) && pageFault.PageFaultVA != 0) {
			LogManager::Error(std::format("[DRED] PageFault address = 0x{:016X} flags = 0x{:X}", (uint64_t)pageFault.PageFaultVA, (uint32_t)pageFault.PageFaultFlags));
			RenderContext::LogFaultResource(pageFault.PageFaultVA);
			dumpAllocations("alive（範囲外/状態不正）", pageFault.pHeadExistingAllocationNode);
			dumpAllocations("freed（解放後使用）", pageFault.pHeadRecentFreedAllocationNode);
		}


	} else {
		// DREDが有効でないことをLogに書く
		LogManager::Error("[DRED] ID3D12DeviceRemovedExtendedData2 is not available");
	}
#endif

	// Assertで止めとく
	MY_ASSERT_MSG(false, "GPUがクラッシュしました。DREDのlogを確認してください");
}

//=============================================================================
// デバイスが削除されたか確認
//=============================================================================
bool DirectXCommon::CheckDeviceRemoved() { 
	HRESULT reason = instance_->device_->GetDeviceRemovedReason();
	if (FAILED(reason)) {
		ReportDeviceRemoved(reason);
		return true;
	}
	return false;
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