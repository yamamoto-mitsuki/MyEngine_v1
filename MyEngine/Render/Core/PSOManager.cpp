#include "MyEngine/Render/Core/PSOManager.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Debug/MyAssert.h"
#include <cassert>
#include <format>

using namespace Microsoft::WRL;
PSOManager* PSOManager::instance_ = nullptr;


// シェーダーのパス定数
namespace {
const std::wstring kShader3D = L"MyEngine/Shader/3D/";
const std::wstring kShader2D = L"MyEngine/Shader/2D/";
const wchar_t* kVSProfile = L"vs_6_0";
const wchar_t* kPSProfile = L"ps_6_0";
} 

//=============================================================================
// 初期化
//=============================================================================
void PSOManager::Initialize() { 
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼び出されています");
	instance_ = new PSOManager(); 
	instance_->InternalInit();
	LogManager::Log("Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void PSOManager::Release() {
	instance_->psoMap_.clear();
	instance_->rootSigMap_.clear();
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}

//=============================================================================
// ゲッター
//=============================================================================
ID3D12PipelineState* PSOManager::GetPSO(const std::string& key) {
	auto& map = instance_->psoMap_;
	assert(map.count(key) && "指定したPSOキーが登録されていません");
	return map.at(key).Get();
}

ID3D12RootSignature* PSOManager::GetRootSignature(const std::string& key) {
	auto& map = instance_->rootSigMap_;
	assert(map.count(key) && "指定したRootSignatureキーが登録されていません");
	return map.at(key).Get();
}

//=============================================================================
// 登録
//=============================================================================
void PSOManager::RegisterPSO(const std::string& key, ComPtr<ID3D12PipelineState> pso) {
	assert(pso && "nullのPSOは登録できません");
	instance_->psoMap_[key] = pso;
	LogManager::Log("PSO登録: " + key);
}

void PSOManager::RegisterRootSignature(const std::string& key, ComPtr<ID3D12RootSignature> rootSig) {
	assert(rootSig && "nullのRootSignatureは登録できません");
	instance_->rootSigMap_[key] = rootSig;
	LogManager::Log("RootSignature登録: " + key);
}

//=============================================================================
// RootParameter生成
//=============================================================================
D3D12_ROOT_PARAMETER PSOManager::CreateDescriptorTableSRV(D3D12_DESCRIPTOR_RANGE& outRange, UINT registerSpace) {
	outRange = {};
	outRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	outRange.NumDescriptors = UINT_MAX; // バインドレス：SRVHeap全体を開放
	outRange.BaseShaderRegister = 0;    // t0から開始
	outRange.RegisterSpace = registerSpace;
	outRange.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PSからテクスチャを参照
	param.DescriptorTable.NumDescriptorRanges = 1;
	param.DescriptorTable.pDescriptorRanges = &outRange;
	return param;
}

D3D12_ROOT_PARAMETER PSOManager::CreateCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister) {
	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.ShaderVisibility = visibility;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = 0;
	return param;
}

D3D12_ROOT_PARAMETER PSOManager::CreateRootSRV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT registerSpace) {
	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	param.ShaderVisibility = visibility;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	return param;
}

//=============================================================================
// RootSignature生成
//=============================================================================
ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature(D3D12_ROOT_PARAMETER* params, UINT paramCount, bool hasSampler, bool hasInputLayout) {
	auto samplers = instance_->GetSamplers();
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = hasInputLayout ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;
	// バインドレスSRVを使うためのフラグ
	desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	desc.pParameters = params;
	desc.NumParameters = paramCount;
	desc.pStaticSamplers = hasSampler ? samplers.data() : nullptr;
	desc.NumStaticSamplers = hasSampler ? static_cast<UINT>(samplers.size()) : 0;

	// シリアライズ
	ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (FAILED(hr)) {
		if (error) {
			LogManager::Log(reinterpret_cast<char*>(error->GetBufferPointer()));
		}
		LogManager::Flush();
		assert(false && "[PSOManager::CreateRootSignature] RootSignatureのシリアライズ失敗");
	}

	// 生成
	ComPtr<ID3D12RootSignature> rootSignature;
	hr = DirectXCommon::GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr)) {
		LogManager::Log(std::format("[PSOManager::CreateRootSignature] Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "[PSOManager::CreateRootSignature] RootSignatureの生成失敗");
	}
	return rootSignature;
}

//=============================================================================
// DepthStencilDesc
//=============================================================================
// 3D用: 深度テストあり・書き込みあり
D3D12_DEPTH_STENCIL_DESC PSOManager::DepthStencilDesc3d() {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	return desc;
}

// 2D用: 深度テストなし（描画順で制御）
D3D12_DEPTH_STENCIL_DESC PSOManager::DepthStencilDesc2d() {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	return desc;
}

// 深度なし: レイマーチング・フルスクリーンエフェクト用
D3D12_DEPTH_STENCIL_DESC PSOManager::DepthStencilDescNone() {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	return desc;
}

//=============================================================================
// Sampler
//=============================================================================
D3D12_STATIC_SAMPLER_DESC PSOManager::CreateSampler(UINT registerIndex, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode) {
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = filter;
	sampler.AddressU = addressMode;
	sampler.AddressV = addressMode;
	sampler.AddressW = addressMode;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.MinLOD = 0.0f;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = (filter == D3D12_FILTER_ANISOTROPIC) ? 16 : 1;
	sampler.RegisterSpace = 0;
	sampler.ShaderRegister = registerIndex;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	return sampler;
}

D3D12_STATIC_SAMPLER_DESC PSOManager::CreateShadowMapSampler(UINT registerIndex) {
	D3D12_STATIC_SAMPLER_DESC sampler{};
	// 比較サンプラー専用フィルター（縮小・拡大はLinear・ミップはPoint）
	sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	// 範囲外をBorderColorで塗る（シャドウマップ外を「影なし」にする）
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	// 深度比較: ピクセル深度<=シャドウマップ深度なら影なし
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// 白=深度1.0=最も遠い=影なし
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.MinLOD = 0.0f;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 1; // 比較サンプラーはANISOTROPIC不使用
	sampler.RegisterSpace = 0;
	sampler.ShaderRegister = registerIndex;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	return sampler;
}

// 全シェーダー共通のサンプラー配列
std::array<D3D12_STATIC_SAMPLER_DESC, 2> PSOManager::GetSamplers() {
	return {
	    CreateSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP), // s0: 通常テクスチャ用
	    CreateShadowMapSampler(1),                                                          // s1: シャドウマップ用
	};
}

//=============================================================================
// エンジン組み込みのRootSignature生成
//=============================================================================
ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dLit() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[5] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1),  // [3] b1 DirectionalLight(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2),  // [4] b2 CameraData(PS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dNoLit() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dInstancedLit() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[5] = {
	    CreateDescriptorTableSRV(srvRange),                  // [0] t0 space0 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),         // [1] b0 マテリアル(PS)
	    CreateRootSRV(D3D12_SHADER_VISIBILITY_VERTEX, 0, 1), // [2] t0 space1 インスタンス配列(VS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1),         // [3] b1 ライト(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2),         // [4] b2 カメラ(PS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dInstancedNoLit() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),                  // [0] t0 space0 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),         // [1] b0 マテリアル(PS)
	    CreateRootSRV(D3D12_SHADER_VISIBILITY_VERTEX, 0, 1), // [2] t0 space1 インスタンス配列(VS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature2d() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 ウィンドウサイズ(VS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignatureLine3d() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレス（Line3Dでは未使用）
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 LineMaterial(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	};
	// Line3Dはテクスチャを使わないのでサンプラーなし
	return CreateRootSignature(params, _countof(params), false, true);
}

//=============================================================================
// 内部初期化
//=============================================================================
void PSOManager::InternalInit() {
	auto* utils = DirectXCommon::GetDxcUtils();
	auto* compiler = DirectXCommon::GetDxcCompiler();
	auto* handler = DirectXCommon::GetIncludeHandler();

	// ===== VSコンパイル =====
	ComPtr<IDxcBlob> vs3d = DirectXCommon::CompileShader(kShader3D + L"Object3d.VS.hlsl", kVSProfile, utils, compiler, handler);
	ComPtr<IDxcBlob> vs2d = DirectXCommon::CompileShader(kShader2D + L"Sprite2d.VS.hlsl", kVSProfile, utils, compiler, handler);
	ComPtr<IDxcBlob> vsLine3d = DirectXCommon::CompileShader(kShader3D + L"Line3d.VS.hlsl", kVSProfile, utils, compiler, handler);

	// ===== PSコンパイル =====
	ComPtr<IDxcBlob> ps3dLitTex = DirectXCommon::CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_TEXTURE"});
	ComPtr<IDxcBlob> ps3dLitNoTex = DirectXCommon::CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> ps3dHalfLitTex = DirectXCommon::CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_TEXTURE", L"USE_HALF_LAMBERT"});
	ComPtr<IDxcBlob> ps3dHalfLitNoTex = DirectXCommon::CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_HALF_LAMBERT"});
	ComPtr<IDxcBlob> ps3dNoLitTex = DirectXCommon::CompileShader(kShader3D + L"Object3dNoLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_TEXTURE"});
	ComPtr<IDxcBlob> ps3dNoLitNoTex = DirectXCommon::CompileShader(kShader3D + L"Object3dNoLit.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> vs3dInst = DirectXCommon::CompileShader(kShader3D + L"Object3dInstanced.VS.hlsl", kVSProfile, utils, compiler, handler);
	ComPtr<IDxcBlob> ps2dTex = DirectXCommon::CompileShader(kShader2D + L"Sprite2dTex.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> ps2dNoTex = DirectXCommon::CompileShader(kShader2D + L"Sprite2dNoTex.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> psLine3d = DirectXCommon::CompileShader(kShader3D + L"Line3d.PS.hlsl", kPSProfile, utils, compiler, handler, {});

	// ===== InputLayout =====
	// 3D用: POSITION(float4) / TEXCOORD(float2) / NORMAL(float3)
	D3D12_INPUT_ELEMENT_DESC inputElem3d[3] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC layout3d = {inputElem3d, _countof(inputElem3d)};

	// 2D用: POSITION(float4) / TEXCOORD(float2)
	D3D12_INPUT_ELEMENT_DESC inputElem2d[2] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC layout2d = {inputElem2d, _countof(inputElem2d)};

	// Line3D用: POSITION(float4) / COLOR(float4)
	D3D12_INPUT_ELEMENT_DESC inputElemLine[2] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC layoutLine = {inputElemLine, _countof(inputElemLine)};

	// ===== RootSignature生成・登録 =====
	rootSigMap_[BuiltinRootSig::Model3dLit] = CreateRootSignature3dLit();
	rootSigMap_[BuiltinRootSig::Model3dNoLit] = CreateRootSignature3dNoLit();
	rootSigMap_[BuiltinRootSig::Sprite2d] = CreateRootSignature2d();
	rootSigMap_[BuiltinRootSig::Line3d] = CreateRootSignatureLine3d();
	rootSigMap_[BuiltinRootSig::Model3dInstLit] = CreateRootSignature3dInstancedLit();
	rootSigMap_[BuiltinRootSig::Model3dInstNoLit] = CreateRootSignature3dInstancedNoLit();

	auto* rsLit = rootSigMap_[BuiltinRootSig::Model3dLit].Get();
	auto* rsNoLit = rootSigMap_[BuiltinRootSig::Model3dNoLit].Get();
	auto* rsInstLit = rootSigMap_[BuiltinRootSig::Model3dInstLit].Get();
	auto* rsInstNoLit = rootSigMap_[BuiltinRootSig::Model3dInstNoLit].Get();
	auto* rs2d = rootSigMap_[BuiltinRootSig::Sprite2d].Get();
	auto* rsLine = rootSigMap_[BuiltinRootSig::Line3d].Get();

	// ===== PSO生成・登録 =====
	psoMap_[BuiltinPSO::Model3dLitTex] = DirectXCommon::CreatePSO(vs3d.Get(), ps3dLitTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dLitNoTex] = DirectXCommon::CreatePSO(vs3d.Get(), ps3dLitNoTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dHalfLitTex] = DirectXCommon::CreatePSO(vs3d.Get(), ps3dHalfLitTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dHalfLitNoTex] = DirectXCommon::CreatePSO(vs3d.Get(), ps3dHalfLitNoTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dNoLitTex] = DirectXCommon::CreatePSO(vs3d.Get(), ps3dNoLitTex.Get(), layout3d, rsNoLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dNoLitNoTex] = DirectXCommon::CreatePSO(vs3d.Get(), ps3dNoLitNoTex.Get(), layout3d, rsNoLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dInstLitTex] = DirectXCommon::CreatePSO(vs3dInst.Get(), ps3dLitTex.Get(), layout3d, rsInstLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dInstLitNoTex] = DirectXCommon::CreatePSO(vs3dInst.Get(), ps3dLitNoTex.Get(), layout3d, rsInstLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dInstHalfLitTex] = DirectXCommon::CreatePSO(vs3dInst.Get(), ps3dHalfLitTex.Get(), layout3d, rsInstLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dInstHalfLitNoTex] = DirectXCommon::CreatePSO(vs3dInst.Get(), ps3dHalfLitNoTex.Get(), layout3d, rsInstLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dInstNoLitTex] = DirectXCommon::CreatePSO(vs3dInst.Get(), ps3dNoLitTex.Get(), layout3d, rsInstNoLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dInstNoLitNoTex] = DirectXCommon::CreatePSO(vs3dInst.Get(), ps3dNoLitNoTex.Get(), layout3d, rsInstNoLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Line3d] = DirectXCommon::CreatePSO(vsLine3d.Get(), psLine3d.Get(), layoutLine, rsLine, DepthStencilDesc3d(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	psoMap_[BuiltinPSO::Sprite2dTex] = DirectXCommon::CreatePSO(vs2d.Get(), ps2dTex.Get(), layout2d, rs2d, DepthStencilDesc2d());
	psoMap_[BuiltinPSO::Sprite2dNoTex] = DirectXCommon::CreatePSO(vs2d.Get(), ps2dNoTex.Get(), layout2d, rs2d, DepthStencilDesc2d());
}