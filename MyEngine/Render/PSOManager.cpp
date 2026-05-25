#include "MyEngine/Render/PSOManager.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Render/DirectXCommon.h"
#include <cassert>
#include <format>

using namespace Microsoft::WRL;

// シェーダーのパス定数
namespace {
const std::wstring kShader3D = L"MyEngine/Shader/3D/";
const std::wstring kShader2D = L"MyEngine/Shader/2D/";
const wchar_t* kVSProfile = L"vs_6_0";
const wchar_t* kPSProfile = L"ps_6_0";
}

//=============================================================================
// シングルトン
//=============================================================================
PSOManager* PSOManager::GetInstance() {
	static PSOManager instance;
	return &instance;
}

//=============================================================================
// 初期化
//=============================================================================
void PSOManager::Init(DirectXCommon* dxCommon) { GetInstance()->InternalInit(dxCommon); }

//=============================================================================
// 終了処理
//=============================================================================
void PSOManager::Finalize() {
	GetInstance()->psoMap_.clear();
	GetInstance()->rootSigMap_.clear();
	LogManager::Log("[PSOManager] 解放完了");
}

//=============================================================================
// ゲッター
//=============================================================================
ID3D12PipelineState* PSOManager::GetPSO(const std::string& key) {
	auto& map = GetInstance()->psoMap_;
	assert(map.count(key) && "指定したPSOキーが登録されていません");
	return map.at(key).Get();
}

ID3D12RootSignature* PSOManager::GetRootSignature(const std::string& key) {
	auto& map = GetInstance()->rootSigMap_;
	assert(map.count(key) && "指定したRootSignatureキーが登録されていません");
	return map.at(key).Get();
}

//=============================================================================
// 登録
//=============================================================================
void PSOManager::RegisterPSO(const std::string& key, ComPtr<ID3D12PipelineState> pso) {
	assert(pso && "nullのPSOは登録できません");
	GetInstance()->psoMap_[key] = pso;
	LogManager::Log("[PSOManager] PSO登録: " + key);
}

void PSOManager::RegisterRootSignature(const std::string& key, ComPtr<ID3D12RootSignature> rootSig) {
	assert(rootSig && "nullのRootSignatureは登録できません");
	GetInstance()->rootSigMap_[key] = rootSig;
	LogManager::Log("[PSOManager] RootSignature登録: " + key);
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
	outRange.OffsetInDescriptorsFromTableStart = 0; // Heapの先頭から

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
	param.Descriptor.ShaderRegister = shaderRegister; // HLSLのregister番号
	param.Descriptor.RegisterSpace = 0;
	return param;
}

//=============================================================================
// RootSignature生成
//=============================================================================
ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature(DirectXCommon* dxCommon, D3D12_ROOT_PARAMETER* params, UINT paramCount, bool hasSampler, bool hasInputLayout) {
	// デフォルトサンプラーを取得
	auto samplers = GetSamplers();
	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = hasInputLayout ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT // 頂点バッファあり
	                            : D3D12_ROOT_SIGNATURE_FLAG_NONE;                              // 頂点バッファなし（レイマーチング等）
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
		assert(false && "RootSignatureのシリアライズ失敗");
	}

	// 生成
	ComPtr<ID3D12RootSignature> rootSignature;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(false && "RootSignatureの生成失敗");
	}
	return rootSignature;
}

//=============================================================================
// DepthStencilDesc
//=============================================================================
// 3D用: 深度テストあり・書き込みあり（通常の3D描画）
D3D12_DEPTH_STENCIL_DESC PSOManager::DepthStencilDesc3d() {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = TRUE;                           // 深度テスト有効
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;  // 深度バッファへの書き込みあり
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 近いものを描画
	return desc;
}

// 2D用: 深度テストなし（UI・スプライトは描画順で制御）
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
	sampler.Filter = filter;        // 補間方法: LINEAR=なめらか / POINT=くっきり / ANISOTROPIC=斜めもなめらか
	sampler.AddressU = addressMode; // U方向: WRAP=繰り返す / CLAMP=端を引き伸ばす
	sampler.AddressV = addressMode; // V方向
	sampler.AddressW = addressMode; // W方向（3Dテクスチャ用）
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 通常テクスチャはNEVER固定
	sampler.MaxLOD = D3D12_FLOAT32_MAX; // 全ミップレベルを使う
	sampler.MinLOD = 0.0f;              // 最高解像度から使う
	sampler.MipLODBias = 0.0f;          // ミップ選択オフセット: 0=自動 / プラス=ぼける / マイナス=シャープ
	sampler.MaxAnisotropy = (filter == D3D12_FILTER_ANISOTROPIC) ? 16 : 1; // ANISOTROPIC時は16(最高品質)
	sampler.RegisterSpace = 0;
	sampler.ShaderRegister = registerIndex;                       // HLSLのregister番号
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;     // PSのみ参照可能
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // BORDERモード時の範囲外の色（BORDER以外では無視）
	return sampler;
}

D3D12_STATIC_SAMPLER_DESC PSOManager::CreateShadowMapSampler(UINT registerIndex) {
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 比較サンプラー専用フィルター（縮小・拡大はLinear・ミップはPoint）
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER; // 範囲外をBorderColorで塗る（シャドウマップ外を「影なし」にする）
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;    // 深度比較: ピクセル深度<=シャドウマップ深度なら影なし
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // 白=深度1.0=最も遠い=影なし
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
// 新しいサンプラーが必要になったらここに追加すると全RootSignatureに反映される
std::array<D3D12_STATIC_SAMPLER_DESC, 2> PSOManager::GetSamplers() {
	return {
	    // s0: 通常テクスチャ用
	    CreateSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
	    // s1: シャドウマップ用
	    CreateShadowMapSampler(1),                                                          
	};
}

//=============================================================================
// エンジン組み込みのRootSignature生成
//=============================================================================
// ===== 3DLit用 =====
    ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dLit(DirectXCommon* dxCommon) {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[5] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1),  // [3] b1 DirectionalLight(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2),  // [4] b2 CameraData(PS)
	};
	return CreateRootSignature(dxCommon, params, _countof(params), true, true);
}

// ===== 3DNoLit用 =====
ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature3dNoLit(DirectXCommon* dxCommon) {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	};
	return CreateRootSignature(dxCommon, params, _countof(params), true, true);
}

// ===== 2D用 =====
ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignature2d(DirectXCommon* dxCommon) {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレステクスチャ
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 マテリアル(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 ウィンドウサイズ(VS)
	};
	return CreateRootSignature(dxCommon, params, _countof(params), true, true);
}

// ===== Line3D用（テクスチャ未使用・バインドレスは統一のため入れておく）=====
    ComPtr<ID3D12RootSignature> PSOManager::CreateRootSignatureLine3d(DirectXCommon* dxCommon) {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	D3D12_ROOT_PARAMETER params[3] = {
	    CreateDescriptorTableSRV(srvRange),           // [0] t0〜 バインドレス（Line3Dでは未使用）
	    CreateCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0),  // [1] b0 LineMaterial(PS)
	    CreateCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0), // [2] b0 行列(VS)
	};
	// Line3Dはテクスチャを使わないのでサンプラーなし
	return CreateRootSignature(dxCommon, params, _countof(params), false, true);
}

//=============================================================================
// 内部初期化
//=============================================================================
void PSOManager::InternalInit(DirectXCommon* dxCommon) {
	auto* utils = dxCommon->GetDxcUtils();
	auto* compiler = dxCommon->GetDxcCompiler();
	auto* handler = dxCommon->GetIncludeHandler();

	// ===== VSコンパイル =====
	ComPtr<IDxcBlob> vs3d = dxCommon->CompileShader(kShader3D + L"Object3d.VS.hlsl", kVSProfile, utils, compiler, handler);
	ComPtr<IDxcBlob> vs2d = dxCommon->CompileShader(kShader2D + L"Sprite2d.VS.hlsl", kVSProfile, utils, compiler, handler);
	ComPtr<IDxcBlob> vsLine3d = dxCommon->CompileShader(kShader3D + L"Line3d.VS.hlsl", kVSProfile, utils, compiler, handler);

	// ===== PSコンパイル =====
	ComPtr<IDxcBlob> ps3dLitTex = dxCommon->CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_TEXTURE"});
	ComPtr<IDxcBlob> ps3dLitNoTex = dxCommon->CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> ps3dHalfLitTex = dxCommon->CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_TEXTURE", L"USE_HALF_LAMBERT"});
	ComPtr<IDxcBlob> ps3dHalfLitNoTex = dxCommon->CompileShader(kShader3D + L"Object3dLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_HALF_LAMBERT"});
	ComPtr<IDxcBlob> ps3dNoLitTex = dxCommon->CompileShader(kShader3D + L"Object3dNoLit.PS.hlsl", kPSProfile, utils, compiler, handler, {L"USE_TEXTURE"});
	ComPtr<IDxcBlob> ps3dNoLitNoTex = dxCommon->CompileShader(kShader3D + L"Object3dNoLit.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> ps2dTex = dxCommon->CompileShader(kShader2D + L"Sprite2dTex.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> ps2dNoTex = dxCommon->CompileShader(kShader2D + L"Sprite2dNoTex.PS.hlsl", kPSProfile, utils, compiler, handler, {});
	ComPtr<IDxcBlob> psLine3d = dxCommon->CompileShader(kShader3D + L"Line3d.PS.hlsl", kPSProfile, utils, compiler, handler, {});

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
	rootSigMap_[BuiltinRootSig::Model3dLit] = CreateRootSignature3dLit(dxCommon);
	rootSigMap_[BuiltinRootSig::Model3dNoLit] = CreateRootSignature3dNoLit(dxCommon);
	rootSigMap_[BuiltinRootSig::Sprite2d] = CreateRootSignature2d(dxCommon);
	rootSigMap_[BuiltinRootSig::Line3d] = CreateRootSignatureLine3d(dxCommon);

	auto* rsLit = rootSigMap_[BuiltinRootSig::Model3dLit].Get();
	auto* rsNoLit = rootSigMap_[BuiltinRootSig::Model3dNoLit].Get();
	auto* rs2d = rootSigMap_[BuiltinRootSig::Sprite2d].Get();
	auto* rsLine = rootSigMap_[BuiltinRootSig::Line3d].Get();

	// ===== PSO生成・登録 =====
	psoMap_[BuiltinPSO::Model3dLitTex] = dxCommon->CreatePSO(vs3d.Get(), ps3dLitTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dLitNoTex] = dxCommon->CreatePSO(vs3d.Get(), ps3dLitNoTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dHalfLitTex] = dxCommon->CreatePSO(vs3d.Get(), ps3dHalfLitTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dHalfLitNoTex] = dxCommon->CreatePSO(vs3d.Get(), ps3dHalfLitNoTex.Get(), layout3d, rsLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dNoLitTex] = dxCommon->CreatePSO(vs3d.Get(), ps3dNoLitTex.Get(), layout3d, rsNoLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Model3dNoLitNoTex] = dxCommon->CreatePSO(vs3d.Get(), ps3dNoLitNoTex.Get(), layout3d, rsNoLit, DepthStencilDesc3d());
	psoMap_[BuiltinPSO::Line3d] = dxCommon->CreatePSO(vsLine3d.Get(), psLine3d.Get(), layoutLine, rsLine, DepthStencilDesc3d(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	psoMap_[BuiltinPSO::Sprite2dTex] = dxCommon->CreatePSO(vs2d.Get(), ps2dTex.Get(), layout2d, rs2d, DepthStencilDesc2d());
	psoMap_[BuiltinPSO::Sprite2dNoTex] = dxCommon->CreatePSO(vs2d.Get(), ps2dNoTex.Get(), layout2d, rs2d, DepthStencilDesc2d());

	LogManager::Log("[PSOManager] 初期化完了");
}