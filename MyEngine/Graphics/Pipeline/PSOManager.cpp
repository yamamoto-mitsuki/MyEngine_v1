#include "MyEngine/Graphics/Pipeline/PSOManager.h"

#include <format>
#include <cassert>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

using namespace Microsoft::WRL;
PSOManager* PSOManager::instance_ = nullptr;

// シェーダーのパス定数
namespace {
const std::wstring kShader3D = L"MyEngine/Shader/";
const std::wstring kShader2D = L"MyEngine/Shader/";
const wchar_t* kVSProfile = L"vs_6_0";
const wchar_t* kPSProfile = L"ps_6_0";


// PSO登録のキー
PSOKey PSOKey::Model(ShadingModel shading,BlendMode blend) {
	ShaderID id;
	switch (shading) {
	case ShadingModel::Lambert:
		id = ShaderID::Model3dLambert;
		break;
	case ShadingModel::HalfLambert:
		id = ShaderID::Model3dHalfLambert;
		break;
	case ShadingModel::Unlit:
	default:
		id = ShaderID::Model3dUnlit;
		break;
	}
	return PSOKey{id, blend};
}
// RootSignature登録のキー
std::string RootSignatureKey::ModelKey(ShadingModel model) {
	std::string base;
	switch (model) {
	case ShadingModel::Lambert:
	case ShadingModel::HalfLambert:
		base = "Model3dLit";
		break;
	case ShadingModel::Unlit:
	default:
		base = "Model3dUnLit";
		break;
	}
	return  base;
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
ID3D12PipelineState* PSOManager::GetPSO(const PSOKey& key) {
	auto& map = instance_->psoMap_;
	// あればキャッシュを返す
	if (auto it = map.find(key); it != map.end()) {
		return it->second.Get();
	}
	// 無ければ組み立てて生成し、キャッシュに入れる（get-or-create）
	ComPtr<ID3D12PipelineState> pso = DirectXCommon::CreatePSO(instance_->MakePSODesc(key));
	ID3D12PipelineState* raw = pso.Get();
	map[key] = std::move(pso);
	return raw;
}

ID3D12RootSignature* PSOManager::GetRootSignature(const std::string& key) {
	auto& map = instance_->rootSigMap_;
	MY_ASSERT_MSG(map.count(key), "指定したRootSignatureキーが登録されていません");
	return map.at(key).Get();
}

//=============================================================================
// RootParameter生成
//=============================================================================
D3D12_ROOT_PARAMETER PSOManager::MakeRootParamBindlessTable(D3D12_DESCRIPTOR_RANGE& outRange, UINT registerSpace) {
	outRange = {};
	outRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	outRange.NumDescriptors = UINT_MAX; // バインドレス: SRVHeap全体を開放
	outRange.BaseShaderRegister = 0;    // t0から
	outRange.RegisterSpace = registerSpace;
	outRange.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	param.DescriptorTable.NumDescriptorRanges = 1;
	param.DescriptorTable.pDescriptorRanges = &outRange; // outRangeはRootSignature生成まで生存させる
	return param;
}

D3D12_ROOT_PARAMETER PSOManager::MakeRootParameterCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister) {
	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.ShaderVisibility = visibility;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = 0;
	return param;
}

D3D12_ROOT_PARAMETER PSOManager::MakeRootParamsSRV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT registerSpace) {
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
	auto samplers = instance_->MakeSamplers();
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
			LogManager::Error(reinterpret_cast<char*>(error->GetBufferPointer()));
		}
		MY_ASSERT_MSG(false, "RootSignatureのシリアライズ失敗");
	}

	// 生成
	ComPtr<ID3D12RootSignature> rootSignature;
	hr = DirectXCommon::GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "RootSignatureの生成失敗");
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
D3D12_STATIC_SAMPLER_DESC PSOManager::MakeSampler(UINT registerIndex, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode) {
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

D3D12_STATIC_SAMPLER_DESC PSOManager::MakeShadowMapSampler(UINT registerIndex) {
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
std::array<D3D12_STATIC_SAMPLER_DESC, 2> PSOManager::MakeSamplers() {
	return {
	    MakeSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP), // s0: 通常テクスチャ用
	    MakeShadowMapSampler(1),                                                          // s1: シャドウマップ用
	};
}

//=============================================================================
// エンジン組み込みのRootSignature生成
//=============================================================================
ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dLit() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[5] = {
	    MakeRootParamBindlessTable(srvRange),                // [0] t0〜 バインドレステクスチャ
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1),  // [3] b1 DirectionalLight(PS)
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2),  // [4] b2 CameraData(PS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dNoLit() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    MakeRootParamBindlessTable(srvRange),                // [0] t0〜 バインドレステクスチャ
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature2d() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    MakeRootParamBindlessTable(srvRange),                // [0] t0〜 バインドレステクスチャ
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 ウィンドウサイズ(VS)
	};
	return CreateRootSignature(params, _countof(params), true, true);
}

ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignatureLine3d() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    MakeRootParamBindlessTable(srvRange),                // [0] t0〜 バインドレス（Line3Dでは未使用）
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 LineMaterial(PS)
	    MakeRootParameterCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	};
	// Line3Dはテクスチャを使わないのでサンプラーなし
	return CreateRootSignature(params, _countof(params), false, true);
}

//=============================================================================
// PSOKey → PSODesc
//=============================================================================
PSODesc PSOManager::MakePSODesc(const PSOKey& key) {
	auto* rsLit = GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Lambert));
	auto* rsUnlit = GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Unlit));

	// Model3d系の共通形（レイアウト3d・深度3d）
	auto model3d = [&](IDxcBlob* vs, IDxcBlob* ps, ID3D12RootSignature* rs) {
		return PSODesc{.vs = vs, .ps = ps, .inputLayout = kLayout3d, .rootSignature = rs, .depthStencil = DepthStencilDesc3d()};
	};

	PSODesc desc{};
	// 使用する InputLayout, RootSignature, 深度情報, VS, PS
	switch (key.shader) {
	case ShaderID::Model3dLambert:
		desc = model3d(object3dVS_.Get(), lambertPS_.Get(), rsLit);
		break;
	case ShaderID::Model3dHalfLambert:
		desc = model3d(object3dVS_.Get(), halfLambertPS_.Get(), rsLit);
		break;
	case ShaderID::Model3dUnlit:
		desc = model3d(object3dVS_.Get(), unlitPS_.Get(), rsUnlit);
		break;
	case ShaderID::Line3d:
		desc = PSODesc{
		    .vs = line3dVS_.Get(),
		    .ps = line3dPS_.Get(),
		    .inputLayout = kLayoutLine,
		    .rootSignature = GetRootSignature(RootSignatureKey::Line3d),
		    .depthStencil = DepthStencilDesc3d(),
		    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE};
		break;
	case ShaderID::Sprite2d:
		desc = PSODesc{.vs = sprite2dVS_.Get(), .ps = sprite2dPS_.Get(), .inputLayout = kLayout2d, .rootSignature = GetRootSignature(RootSignatureKey::Sprite2d), .depthStencil = DepthStencilDesc2d()};
		break;
	}
	// ブレンドモード
	desc.blend = key.blend; 

	return desc;
}

//=============================================================================
// 内部初期化
//=============================================================================
void PSOManager::InternalInit() {
	auto* utils = DirectXCommon::GetDxcUtils();
	auto* compiler = DirectXCommon::GetDxcCompiler();
	auto* handler = DirectXCommon::GetIncludeHandler();

	// ===== VS/PSコンパイル（メンバに保持）=====
	// Model
	object3dVS_ = DirectXCommon::CompileShader(kShader3D + L"Model/Object3d.VS.hlsl", kVSProfile, utils, compiler, handler);
	lambertPS_ = DirectXCommon::CompileShader(kShader3D + L"Model/Object3dLambert.PS.hlsl", kPSProfile, utils, compiler, handler);
	halfLambertPS_ = DirectXCommon::CompileShader(kShader3D + L"Model/Object3dHalfLambert.PS.hlsl", kPSProfile, utils, compiler, handler);
	unlitPS_ = DirectXCommon::CompileShader(kShader3D + L"Model/Object3dNoLit.PS.hlsl", kPSProfile, utils, compiler, handler);
	// Sprite
	sprite2dVS_ = DirectXCommon::CompileShader(kShader2D + L"Sprite/Sprite2d.VS.hlsl", kVSProfile, utils, compiler, handler);
	sprite2dPS_ = DirectXCommon::CompileShader(kShader2D + L"Sprite/Sprite2d.PS.hlsl", kPSProfile, utils, compiler, handler);
	// Line
	line3dVS_ = DirectXCommon::CompileShader(kShader3D + L"Line/Line3d.VS.hlsl", kVSProfile, utils, compiler, handler);
	line3dPS_ = DirectXCommon::CompileShader(kShader3D + L"Line/Line3d.PS.hlsl", kPSProfile, utils, compiler, handler);

	// ===== RootSignature生成・登録 =====
	rootSigMap_[RootSignatureKey::ModelKey(ShadingModel::Lambert)] = CreateRootSignature3dLit();
	rootSigMap_[RootSignatureKey::ModelKey(ShadingModel::Unlit)] = CreateRootSignature3dNoLit();
	rootSigMap_[RootSignatureKey::Sprite2d] = CreateRootSignature2d();
	rootSigMap_[RootSignatureKey::Line3d] = CreateRootSignatureLine3d();
}