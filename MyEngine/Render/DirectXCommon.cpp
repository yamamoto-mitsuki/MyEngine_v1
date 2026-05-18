#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Utils/ConvertString.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Math/Vector4.h"
#include <cassert>
#include <format>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

namespace {
const std::wstring kShader3D = L"MyEngine/Shader/3D/";
const std::wstring kShader2D = L"MyEngine/Shader/2D/";
}


DirectXCommon::~DirectXCommon() {
	CloseHandle(fenceEvent_);
	fenceEvent_ = nullptr;
}

//=============================================================================
// 初期化
//=============================================================================
void DirectXCommon::Init() {
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	// ===== DXGIファクトリーの生成 =====
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "DXGIファクトリーを生成できませんでした");
	}

	// ===== アダプタの選択 =====
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		if (FAILED(hr)) {
			assert(SUCCEEDED(hr) && "アダプターの情報を取得できませんでした");
		}
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			LogManager::Log(std::format("Use Adapter:{}", ConvertString(adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr && "適切なアダプタが見つかりませんでした");

	// ===== D3D12Deviceの生成 =====
	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
	const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		if (SUCCEEDED(hr)) {
			LogManager::Log(std::format("FeatureLevel : {}", featureLevelStrings[i]));
			break;
		}
	}
	assert(device_ != nullptr && "デバイスの生成に失敗しました");
	LogManager::Log("Complete create D3D12Device!!!");

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
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
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "コマンドキューの生成をできませんでした");
	}

	// ===== コマンドアロケータの生成 =====
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "コマンドアロケータを生成できませんでした");
	}

	// ===== コマンドリストの生成 =====
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "コマンドリストを生成できませんでした");
	}

	// ===== フェンスとイベントの生成 =====
	device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);

	// ===== DXCコンパイラの初期化 =====
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr) && "DxcUtilsを生成できませんでした");
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr) && "DxcCompilerを生成できませんでした");
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr) && "DXC IncludeHandlerを生成できませんでした");

	// ===== シェーダーのコンパイル =====
	// --- 頂点シェーダー ---
	Microsoft::WRL::ComPtr<IDxcBlob> vs3d = CompileShader(kShader3D + L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> vs2d = CompileShader(kShader2D + L"Sprite2d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> vsLine3d = CompileShader(kShader3D + L"Line3d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	// --- ピクセルシェーダー ---
	Microsoft::WRL::ComPtr<IDxcBlob> ps3dLitTex = CompileShader(kShader3D + L"Object3dLit.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get(), {L"USE_TEXTURE"});
	Microsoft::WRL::ComPtr<IDxcBlob> ps3dLitNoTex = CompileShader(kShader3D + L"Object3dLit.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> ps3dHalfLitTex = CompileShader(kShader3D + L"Object3dLit.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get(), {L"USE_TEXTURE", L"USE_HALF_LAMBERT"});
	Microsoft::WRL::ComPtr<IDxcBlob> ps3dHalfLitNoTex = CompileShader(kShader3D + L"Object3dLit.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get(), {L"USE_HALF_LAMBERT"});
	Microsoft::WRL::ComPtr<IDxcBlob> ps3dNoLitTex = CompileShader(kShader3D + L"Object3dNoLit.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get(), {L"USE_TEXTURE"});
	Microsoft::WRL::ComPtr<IDxcBlob> ps3dNoLitNoTex = CompileShader(kShader3D + L"Object3dNoLit.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> ps2dTex = CompileShader(kShader2D + L"Sprite2dTex.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> ps2dNoTex = CompileShader(kShader2D + L"Sprite2dNoTex.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> psLine3d = CompileShader(kShader3D + L"Line3d.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());

	// ===== InputLayoutの定義 =====
	// --- 3D用: POSITION0, TEXCOORD0 ---
	D3D12_INPUT_ELEMENT_DESC inputElementDescs3d[3] = {};
	// 座標
	inputElementDescs3d[0].SemanticName = "POSITION";
	inputElementDescs3d[0].SemanticIndex = 0;
	inputElementDescs3d[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs3d[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// UV座標
	inputElementDescs3d[1].SemanticName = "TEXCOORD";
	inputElementDescs3d[1].SemanticIndex = 0;
	inputElementDescs3d[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs3d[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// 法線
	inputElementDescs3d[2].SemanticName = "NORMAL";
	inputElementDescs3d[2].SemanticIndex = 0;
	inputElementDescs3d[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs3d[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc3d{};
	inputLayoutDesc3d.pInputElementDescs = inputElementDescs3d;
	inputLayoutDesc3d.NumElements = _countof(inputElementDescs3d);

	// --- 2D用: POSITION1, TEXCOORD1 ---
	D3D12_INPUT_ELEMENT_DESC inputElementDescs2d[2] = {};
	inputElementDescs2d[0].SemanticName = "POSITION";
	inputElementDescs2d[0].SemanticIndex = 0;
	inputElementDescs2d[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs2d[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs2d[1].SemanticName = "TEXCOORD";
	inputElementDescs2d[1].SemanticIndex = 0;
	inputElementDescs2d[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs2d[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc2d{};
	inputLayoutDesc2d.pInputElementDescs = inputElementDescs2d;
	inputLayoutDesc2d.NumElements = _countof(inputElementDescs2d);

	// --- Line3D用: POSITION0(float4), COLOR0(float4) ---
	D3D12_INPUT_ELEMENT_DESC inputElementDescsLine3d[2] = {};
	inputElementDescsLine3d[0].SemanticName = "POSITION";
	inputElementDescsLine3d[0].SemanticIndex = 0;
	inputElementDescsLine3d[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescsLine3d[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescsLine3d[1].SemanticName = "COLOR";
	inputElementDescsLine3d[1].SemanticIndex = 0;
	inputElementDescsLine3d[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescsLine3d[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDescLine3d{};
	inputLayoutDescLine3d.pInputElementDescs = inputElementDescsLine3d;
	inputLayoutDescLine3d.NumElements = _countof(inputElementDescsLine3d);

	// ===== RootSignatureの生成 =====
	rootSignature3dLit_ = CreateRootSignature3dLit();
	rootSignature3dNoLit_ = CreateRootSignature3dNoLit();
	rootSignature2d_ = CreateRootSignature2d();
	rootSignature3dLine_ = CreateRootSignatureLine3d();

	// ===== PSOの生成 =====
	// --- 3D ---
	pso3dLitTex_ = CreatePSO(vs3d.Get(), ps3dLitTex.Get(), inputLayoutDesc3d, rootSignature3dLit_.Get(), CreateDepthStencilDesc3d());
	pso3dLitNoTex_ = CreatePSO(vs3d.Get(), ps3dLitNoTex.Get(), inputLayoutDesc3d, rootSignature3dLit_.Get(), CreateDepthStencilDesc3d());
	pso3dHalfLitTex_ = CreatePSO(vs3d.Get(), ps3dHalfLitTex.Get(), inputLayoutDesc3d, rootSignature3dLit_.Get(), CreateDepthStencilDesc3d());
	pso3dHalfLitNoTex_ = CreatePSO(vs3d.Get(), ps3dHalfLitNoTex.Get(), inputLayoutDesc3d, rootSignature3dLit_.Get(), CreateDepthStencilDesc3d());
	pso3dNoLitTex_ = CreatePSO(vs3d.Get(), ps3dNoLitTex.Get(), inputLayoutDesc3d, rootSignature3dNoLit_.Get(), CreateDepthStencilDesc3d());
	pso3dNoLitNoTex_ = CreatePSO(vs3d.Get(), ps3dNoLitNoTex.Get(), inputLayoutDesc3d, rootSignature3dNoLit_.Get(), CreateDepthStencilDesc3d());
	pso3dLine_ = CreatePSO(vsLine3d.Get(), psLine3d.Get(), inputLayoutDescLine3d, rootSignature3dLine_.Get(), CreateDepthStencilDesc3d(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	// --- 2D ---
	pso2dTex_ = CreatePSO(vs2d.Get(), ps2dTex.Get(), inputLayoutDesc2d, rootSignature2d_.Get(), CreateDepthStencilDesc2d());
	pso2dNoTex_ = CreatePSO(vs2d.Get(), ps2dNoTex.Get(), inputLayoutDesc2d, rootSignature2d_.Get(), CreateDepthStencilDesc2d());

	// ===== SRVDescriptorHeapの生成 =====
	srvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kSRVDescriptorHeap, true);

	// ===== DescriptorSizeをキャッシュ =====
	descriptorSizeSRV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

//=============================================================================
// GPUの完了待ち
//=============================================================================
void DirectXCommon::WaitForGPU() {
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	if (fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}

//=============================================================================
// シェーダーのコンパイル
//=============================================================================
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, 
	IDxcIncludeHandler* includeHandler, const std::vector<std::wstring>& defines) {

	 wchar_t currentDir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, currentDir);
	LogManager::Log(ConvertString(std::format(L"CurrentDir: {}", currentDir)));
	LogManager::Log(ConvertString(std::format(L"FullPath: {}\\{}", currentDir, filePath)));

	LogManager::Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}", filePath, profile)));

	std::vector<LPCWSTR> arguments = {
	   filePath.c_str(), L"-E", L"main", L"-T", profile, L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr",
	}; 
	// defines を "-D", "DEFINE_NAME" の順で追加
	for (const std::wstring& def : defines) {
		arguments.push_back(L"-D");
		arguments.push_back(def.c_str());
	}

	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "ファイルを読み込めませんでした");
	}

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	 Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
	hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments.data(), static_cast<UINT32>(arguments.size()), 
		includeHandler, IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr) && "Dxcが起動できませんでした");

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		LogManager::Log(shaderError->GetStringPointer());
		assert(false && "Shaderをコンパイルできませんでした");
	}

	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	if (FAILED(hr)) {
		assert(SUCCEEDED(hr) && "シェーダーバイナリの取得に失敗しました");
	}

	LogManager::Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}", filePath, profile)));
	return shaderBlob;
}

//=============================================================================
// BlendStateの作成（内部ヘルパー）
// アルファブレンディング（通常合成）を設定する
//=============================================================================
static D3D12_BLEND_DESC CreateBlendDesc() {
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	return blendDesc;
}

//=============================================================================
// RasterizerStateの作成（内部ヘルパー）
// 裏面カリング・ソリッド描画を設定する
//=============================================================================
static D3D12_RASTERIZER_DESC CreateRasterizerDesc() {
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; // 裏面を表示しない
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	return rasterizerDesc;
}

//=============================================================================
// PSOの生成
// RootSignatureは外から渡す（3D用・2D用で分離済み）
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12PipelineState>
    DirectXCommon::CreatePSO(IDxcBlob* vs, IDxcBlob* ps, D3D12_INPUT_LAYOUT_DESC inputLayout, ID3D12RootSignature* rootSignature, 
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType) {

	assert(vs != nullptr && "頂点シェーダーがnullです");
	assert(ps != nullptr && "ピクセルシェーダーがnullです");
	assert(rootSignature != nullptr && "RootSignatureがnullです");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature;
	desc.InputLayout = inputLayout;
	desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	desc.BlendState = CreateBlendDesc();
	desc.RasterizerState = CreateRasterizerDesc();
	desc.DepthStencilState = depthStencilDesc;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.PrimitiveTopologyType = topologyType;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "PSOの生成に失敗しました");
	}
	return pso;
}

//=============================================================================
// BufferResourceの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
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
	HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "BufferResourceの作成に失敗しました");
	}
	return resource;
}

//=============================================================================
// DescriptorHeapの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = heapType;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "DescriptorHeapの作成に失敗しました");
	}
	return descriptorHeap;
}

//=============================================================================
// DepthStencilTextureResourceの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	// ===== 生成するResourceの設定 =====
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;                                   // Textureの幅
	resourceDesc.Height = height;                                 // Textureの高さ
	resourceDesc.MipLevels = 1;                                   // ミップマップレベル数
	resourceDesc.DepthOrArraySize = 1;                            // 奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;          // 2次元
	resourceDesc.SampleDesc.Count = 1;                            // サンプリング数。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使用するためのフラグ

	// ===== 利用するHeapの設定 =====
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAMに配置する

	// ===== 深度値のクリア設定 =====
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;              // 深度値は0.0f～1.0fの範囲で、1.0fが一番遠い
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

	// ===== Resourceの生成 =====
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
	    &heapProperties,                  // Heapの設定
	    D3D12_HEAP_FLAG_NONE,             // Heapの特殊な設定。特になし
	    &resourceDesc,                    // Resourceの設定
	    D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態で生成
	    &depthClearValue,                 // Clear最適値
	    IID_PPV_ARGS(&resource));         // 作成するResourceのポインタ
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(SUCCEEDED(hr) && "DepthStencilTextureResourceの生成に失敗しました");
	}

	return resource;
}

//=============================================================================
// Lightingあり3D用RootSignatureの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12RootSignature> DirectXCommon::CreateRootSignature3dLit() {
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[5] = {};
	// スロット0: マテリアル(b0, PS)
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[0].Descriptor.ShaderRegister = 0;
	// スロット1: TransformationMatrix(b0, VS)
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].Descriptor.ShaderRegister = 0;
	// スロット2: テクスチャ(t0, PS)
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[2].DescriptorTable.pDescriptorRanges = &srvRange;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	// スロット3: DirectionalLight(b1, PS)
	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[3].Descriptor.ShaderRegister = 1;
	// スロット4: CameraWorldPosition(b2, PS)
	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[4].Descriptor.ShaderRegister = 2;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = params;
	desc.NumParameters = _countof(params);
	desc.pStaticSamplers = &sampler;
	desc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	assert(SUCCEEDED(hr) && "3DLit RootSignatureのシリアライズ失敗");
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	assert(SUCCEEDED(hr) && "3DLit RootSignatureの生成失敗");
	return rootSig;
}

//=============================================================================
// Lightingなし3D用RootSignatureの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12RootSignature> DirectXCommon::CreateRootSignature3dNoLit() {
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[3] = {};
	// スロット0: マテリアル(b0, PS)
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[0].Descriptor.ShaderRegister = 0;
	// スロット1: TransformationMatrix(b0, VS)
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].Descriptor.ShaderRegister = 0;
	// スロット2: テクスチャ(t0, PS)
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[2].DescriptorTable.pDescriptorRanges = &srvRange;
	params[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = params;
	desc.NumParameters = _countof(params);
	desc.pStaticSamplers = &sampler;
	desc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	assert(SUCCEEDED(hr) && "3DNoLit RootSignatureのシリアライズ失敗");
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	assert(SUCCEEDED(hr) && "3DNoLit RootSignatureの生成失敗");
	return rootSig;
}

//=============================================================================
// 2D用RootSignatureの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12RootSignature> DirectXCommon::CreateRootSignature2d() {
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[3] = {};
	// スロット0: マテリアル(b0, PS)
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[0].Descriptor.ShaderRegister = 0;
	// スロット1: テクスチャ(t0, PS)
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[1].DescriptorTable.pDescriptorRanges = &srvRange;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	// スロット2: ウィンドウサイズ(b0, VS)
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[2].Descriptor.ShaderRegister = 0; // b0

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = params;
	desc.NumParameters = _countof(params);
	desc.pStaticSamplers = &sampler;
	desc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	assert(SUCCEEDED(hr) && "2D RootSignatureのシリアライズ失敗");
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	assert(SUCCEEDED(hr) && "2D RootSignatureの生成失敗");
	return rootSig;
}

//=============================================================================
// Line3D専用RootSignatureの生成
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12RootSignature> DirectXCommon::CreateRootSignatureLine3d() {
	D3D12_ROOT_PARAMETER params[2] = {};
	// スロット0: LineMaterialCB (b0, PS)
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[0].Descriptor.ShaderRegister = 0; // b0
	// スロット1: TransformationMatrix (b0, VS)
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].Descriptor.ShaderRegister = 0; // b0

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = params;
	desc.NumParameters = _countof(params);
	desc.pStaticSamplers = nullptr;
	desc.NumStaticSamplers = 0;

	Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (FAILED(hr)) {
		if (error) {
			LogManager::Log(reinterpret_cast<char*>(error->GetBufferPointer()));
		}
		assert(false && "Line3D RootSignatureのシリアライズ失敗");
	}
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	assert(SUCCEEDED(hr) && "Line3D RootSignatureの生成失敗");
	return rootSig;
}

//=============================================================================
// 3D用DepthStencilStateの生成
//=============================================================================
D3D12_DEPTH_STENCIL_DESC DirectXCommon::CreateDepthStencilDesc3d() {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = TRUE;                           // Depthの機能を有効化する
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;  // 書き込む
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 比較関数はLessEqual。つまり、近ければ描画される
	return desc;
}

//=============================================================================
// 2D用DepthStencilStateの生成
//=============================================================================
D3D12_DEPTH_STENCIL_DESC DirectXCommon::CreateDepthStencilDesc2d() {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = FALSE;                          // Depthの機能を有効化する
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 書き込む
	desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;     // 比較関数はLessEqual。つまり、近ければ描画される
	return desc;
}

//=============================================================================
// 指定インデックスのCPUDescriptorHandleを取得する
//=============================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index) {

	uint32_t descriptorSize = 0;
	switch (heapType) {
	// CBV,SRV,UAV
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		descriptorSize = descriptorSizeSRV;
		break;

	// RTV
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
		descriptorSize = descriptorSizeRTV;
		break;

	// DSV
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
		descriptorSize = descriptorSizeDSV;
		break;
	default:
		assert(false && "未対応のDescriptorHeapTypeです");
		break;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}

//=============================================================================
// 指定インデックスのGPUDescriptorHandleを取得する
//=============================================================================
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index) {
	
	uint32_t descriptorSize = 0;
	switch (heapType) {
	// CBV,SRV,UAV
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		descriptorSize = descriptorSizeSRV;
		break;

	// RTV
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
		descriptorSize = descriptorSizeRTV;
		break;

	// DSV
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
		descriptorSize = descriptorSizeDSV;
		break;
	default:
		assert(false && "未対応のDescriptorHeapTypeです");
		break;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}