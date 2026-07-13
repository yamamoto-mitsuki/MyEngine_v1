#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"

#include <format>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"

// 静的メンバ変数
RootSignatureManager* RootSignatureManager::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void RootSignatureManager::Initialize() {
	instance_ = new RootSignatureManager();
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void RootSignatureManager::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}

//=============================================================================
// 描画情報 → ID
//=============================================================================
// ===== RootSignatureID =====
RootSignatureID RootSignatureManager::GetRootSignatureID(DrawCategory drawCategory, ShadingType type) {
	// --- 描画カテゴリで分岐 ---
	switch (drawCategory) {
	case DrawCategory::Sprite:
		return RootSignatureID::Sprite;

	case DrawCategory::Line:
		return RootSignatureID::Line;


	case DrawCategory::Model:
		// --- Modelのときだけshadingで分岐 ---
		switch (type) {
		case ShadingType::Lambert:
		case ShadingType::HalfLambert:
			return RootSignatureID::ModelLit;

		case ShadingType::Unlit:
			return RootSignatureID::ModelUnlit;
		}
		break;
	}
	// 未対応の組み合わせ
	MY_ASSERT_MSG(false, "未対応の DrawCategory / ShadingType");
	return RootSignatureID::ModelUnlit;
}

//=============================================================================
// ID → RootSignature
//=============================================================================
// ===== レジスタ番号 =====
std::optional<UINT> RootSignatureManager::GetBindSlot(RootSignatureID id, RootBind bind) {
	auto layout = GetRootParametersLayout(id);
	for (size_t i = 0; i < layout.size(); ++i) {
		if (layout[i].bind == bind) {
			return static_cast<UINT>(i);
		}
	}
	LogManager::Warning(std::format("RootBind '{}' is not defined for RootSignature '{}'", 
		magic_enum::enum_name(bind), magic_enum::enum_name(id)));

	return std::nullopt;
}

// ===== RootSignature =====
Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignatureManager::GetRootSignature(RootSignatureID id) {
	auto& cache = instance_->rootSignatures_[magic_enum::enum_index(id).value()];
	// 登録済みか確認
	if (cache) {
		return cache;
	}
	// 未登録のとき
	cache = MakeRootSignature(id);
	return cache;
}

//=============================================================================
// 1. ID → Layout（設計図）
//=============================================================================
// ===== ID → StaticSampler Layout =====
std::span<const StaticSampler> RootSignatureManager::GetStaticSamplerLayout(RootSignatureID id) {
	switch (id) {
	case RootSignatureID::Sprite:
	case RootSignatureID::ModelLit:
	case RootSignatureID::ModelUnlit:
		return kStaticSamplerDefaultLayout;
	case RootSignatureID::Line:
		return {}; // Lineはテクスチャを使わない
	default:
		MY_ASSERT_MSG(false, std::format("{} 存在しないRootSignatureIDです", magic_enum::enum_name(id)));
		return {};
	}
}

// ===== RootParameters Layout =====
std::span<const RootParameter> RootSignatureManager::GetRootParametersLayout(RootSignatureID id) {
	switch (id) {
	case RootSignatureID::Sprite:
		return kRootParametersSpriteLayout;
	case RootSignatureID::ModelLit:
		return kRootParametersModelLitLayout;
	case RootSignatureID::ModelUnlit:
		return kRootParametersModelUnlitLayout;
	case RootSignatureID::Line:
		return kRootParametersLineLayout;
	default:
		MY_ASSERT_MSG(false, std::format("{} 存在していないRootSignatureIDです", magic_enum::enum_name(id)));
		break;
	}

	return kRootParametersLineLayout;
}


//=============================================================================
// 2. Layout（設計図） → D3D12型
//=============================================================================
// ===== D3D12_ROOT_PARAMETER1 =====
std::vector<D3D12_ROOT_PARAMETER1> RootSignatureManager::MakeRootParameters(std::span<const RootParameter> layout, D3D12_DESCRIPTOR_RANGE1& outRange) {
	std::vector<D3D12_ROOT_PARAMETER1> params;
	params.reserve(layout.size());

	// --- layoutに格納された情報分、ループ ---
	for (const auto& rootParameter : layout) {
		// RootParameter のリソース型と使う場所によって分岐
		switch (rootParameter.type) {

		// ConstantBuffer型, VS
		case BindType::CBV_VS:
			params.push_back(CreateRootParameterCBV(D3D12_SHADER_VISIBILITY_VERTEX, rootParameter.shaderRegister));
			break;
		// ConstantBuffer型, PS
		case BindType::CBV_PS:
			params.push_back(CreateRootParameterCBV(D3D12_SHADER_VISIBILITY_PIXEL, rootParameter.shaderRegister));
			break;
		// バインドレステクスチャ専用
		case BindType::BindlessTexture:
			params.push_back(CreateRootParameterBindlessTable(outRange));
			break;
		}
	}

	return params;
}

// ===== D3D12_STATIC_SAMPLER_DESC =====
D3D12_STATIC_SAMPLER_DESC RootSignatureManager::MakeStaticSampler(const StaticSampler& sampler) {
	D3D12_STATIC_SAMPLER_DESC desc{};
	// --- 全タイプ共通 ---
	desc.MipLODBias = 0.0f;
	desc.MinLOD = 0.0f;
	desc.MaxLOD = D3D12_FLOAT32_MAX;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	desc.RegisterSpace = 0;
	desc.ShaderRegister = sampler.shaderRegister; // レジスタ番号
	desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// --- Typeで変わるところを上書き ---
	switch (sampler.type) {
	case SamplingType::LinearWrap:
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		break;
	case SamplingType::LinearClamp:
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		break;
	case SamplingType::PointClamp:
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		break;
	case SamplingType::ShadowMap:
		// 比較サンプラー：ピクセル深度 <= シャドウマップ深度 なら影なし
		desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // 白=遠=影なし
		break;
	}

	return desc;
}


//=============================================================================
// 3. D3D12 RootParameter 部品生成
//=============================================================================
// ===== バインドレスSRVのDescriptorTableパラメータを生成する。必ずparams[0]に配置すること =====
D3D12_ROOT_PARAMETER1 RootSignatureManager::CreateRootParameterBindlessTable(D3D12_DESCRIPTOR_RANGE1& outRange) {
	// Descriptor range
	outRange = {};
	outRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVとして使用
	outRange.NumDescriptors = UINT_MAX;                   // バインドレス: SRVHeap全体を開放
	outRange.BaseShaderRegister = 0;                      // t0から
	outRange.OffsetInDescriptorsFromTableStart = 0;
	outRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC; // 指すメモリが変わらないのでSTATIC
	// Root Parameter
	D3D12_ROOT_PARAMETER1 param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PSで使用
	param.DescriptorTable.NumDescriptorRanges = 1;
	param.DescriptorTable.pDescriptorRanges = &outRange; // outRangeはRootSignature生成まで生存させる
	return param;
}

// ===== SRVのRootParameterを生成する =====
D3D12_ROOT_PARAMETER1 RootSignatureManager::CreateRootParameterSRV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister) {
	D3D12_ROOT_PARAMETER1 param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; // SRVとして使用
	param.ShaderVisibility = visibility;                 // シェーダーの種類
	param.Descriptor.ShaderRegister = shaderRegister;    // レジスタ番号
	param.Descriptor.RegisterSpace = 0;                  // spaceは0として使用
	return param;
}

// ===== 定数バッファ(CBV)のRootParameterを生成する =====
D3D12_ROOT_PARAMETER1 RootSignatureManager::CreateRootParameterCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister) {
	D3D12_ROOT_PARAMETER1 param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVとして使用
	param.ShaderVisibility = visibility;                 // シェーダーの種類
	param.Descriptor.ShaderRegister = shaderRegister;    // レジスタ番号
	param.Descriptor.RegisterSpace = 0;                  // spaceは0として使用
	return param;
}


//=============================================================================
// 4. RootSignature作成
//=============================================================================
// ===== ID → RootSignature 設定 =====
Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignatureManager::MakeRootSignature(RootSignatureID id) {
	// 1. ID → Layout（設計図）
	auto paramsLayout = GetRootParametersLayout(id);
	auto samplerLayout = GetStaticSamplerLayout(id);

	// 2. RootParameters へ変換
	D3D12_DESCRIPTOR_RANGE1 srvRange{};
	std::vector<D3D12_ROOT_PARAMETER1> params = MakeRootParameters(paramsLayout, srvRange);

	// 3. StaticSampler へ変換
	std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
	samplers.reserve(samplerLayout.size());
	for (const auto& s : samplerLayout) {
		samplers.push_back(MakeStaticSampler(s));
	}

	// 4. RootSignatureDesc を作成
	D3D12_ROOT_SIGNATURE_DESC1 desc1{};
	desc1.pParameters = params.data();
	desc1.NumParameters = static_cast<UINT>(params.size());
	desc1.pStaticSamplers = samplers.data();
	desc1.NumStaticSamplers = static_cast<UINT>(samplers.size());
	desc1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT // InputAssemblerでInputLayoutを使う
		        | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED; // バインドレスでテクスチャを使用

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned{};
	versioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned.Desc_1_1 = desc1;

	return CreateRootSignature(versioned);
}

// ===== RootSignature作成 =====
Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignatureManager::CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc) {
	// シリアライズしてバイナリに
	Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &blob, &error);
	if (FAILED(hr)) {
		if (error) {
			LogManager::Error(reinterpret_cast<char*>(error->GetBufferPointer()));
		}
		MY_ASSERT_MSG(false, "RootSignatureのシリアライズ失敗");
	}

	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = DirectXCommon::GetDevice()->CreateRootSignature(
	    0,                        // nodeMask: GPUが1台なら0
	    blob->GetBufferPointer(), // 作ったバイナリの先頭
	    blob->GetBufferSize(),    // そのバイト数
	    IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", static_cast<uint32_t>(hr)));
		MY_ASSERT_MSG(false, "RootSignatureの生成失敗");
	}

	return rootSignature;
}