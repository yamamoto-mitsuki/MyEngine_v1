#include "RootSignatureManager.h"
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
// vs, psなどペアのシェーダーを結びつける
//=============================================================================
std::vector<MergedBind> RootSignatureManager::MergeStages(const std::vector<ShaderResourceBinding>& vs, const std::vector<ShaderResourceBinding>& ps) {
	std::vector<MergedBind> out;
	out.reserve(vs.size() + ps.size());
	// VS側は全部 VERTEX として入れる
	for (const auto& v : vs) {
		out.push_back({v, D3D12_SHADER_VISIBILITY_VERTEX});
	}
	// PS側は、同じリソース(type/register/space が一致)があれば ALL に昇格、
	// 無ければ PS専用として追加
	for (const auto& p : ps) {
		auto it = std::find_if(out.begin(), out.end(), [&](const MergedBind& m) { 
			return m.bind.name == p.name &&                     // 変数名が一致しているか
				   m.bind.type == p.type &&                     // CBufferなど型が一致しているか
				   m.bind.registerNumber == p.registerNumber && // レジスタ番号が一致しているか
				   m.bind.space == p.space; });                 // space番号が一致しているか
		if (it != out.end()) {
			it->visibility = D3D12_SHADER_VISIBILITY_ALL; // VSにもPSにもある
		} else {
			out.push_back({p, D3D12_SHADER_VISIBILITY_PIXEL}); // PS専用
		}
	}
	return out;
}


//=============================================================================
// hlslの変数名を RootBindに変換
//=============================================================================
std::optional<RootBind> RootSignatureManager::NameToRole(std::string_view name) {
	static const std::unordered_map<std::string_view, RootBind> table = {
	    {"gTransformationMatrix", RootBind::TransformationMatrix},
	    {"gMaterial",             RootBind::Material            },
	    {"gDirectionalLight",     RootBind::DirectionalLight    },
	    {"gCamera",               RootBind::Camera              },
	    {"gPointLights",          RootBind::PointLights         },
	    {"gParticles",            RootBind::Particles           },
	    {"gWindowSize",           RootBind::WindowSize          },
	    {"gSkybox",               RootBind::Skybox              },
	    {"gIBL",	              RootBind::IBL                 },
	    {"gTextures",             RootBind::BindlessTexture     },
	    {"gTexturesCube",         RootBind::BindlessTextureCube },
	};

	if (auto it = table.find(name); it != table.end()) {
		return it->second;
	}
	return std::nullopt; // 役割不明（slotOfに載せない）
}


//=============================================================================
// Resourceから RootParameter 作成
//=============================================================================
D3D12_ROOT_PARAMETER1 RootSignatureManager::MakeRootParameter(const MergedBind& m, std::vector<D3D12_DESCRIPTOR_RANGE1>& ranges) {
	const ShaderResourceBinding& b = m.bind;
	D3D12_ROOT_PARAMETER1 param{};
	param.ShaderVisibility = m.visibility;

	if (b.type == D3D_SIT_CBUFFER) {
		// --- ルートCBV ---
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.Descriptor.ShaderRegister = b.registerNumber;
		param.Descriptor.RegisterSpace = b.space;
	} else if (b.type == D3D_SIT_STRUCTURED && b.count != 0) {
		// --- 単体SRV（配列でないStructuredBuffer, gParticles）---
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		param.Descriptor.ShaderRegister = b.registerNumber;
		param.Descriptor.RegisterSpace = b.space;
	} else {
		// --- テクスチャ配列 = DescriptorTable（bindless）---
		D3D12_DESCRIPTOR_RANGE1& range = ranges.emplace_back();
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.NumDescriptors = (b.count == 0) ? UINT_MAX : b.count; // 0=無制限
		range.BaseShaderRegister = b.registerNumber;
		range.RegisterSpace = b.space;
		range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = 1;
		param.DescriptorTable.pDescriptorRanges = &range; // ranges.reserve済み前提
	}
	return param;
}


//=============================================================================
// Resourceから Sampler 作成
//=============================================================================
D3D12_STATIC_SAMPLER_DESC RootSignatureManager::MakeStaticSampler(const MergedBind& m) {
	D3D12_STATIC_SAMPLER_DESC s{};
	s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // フィルタはreflectionで取れないので規約
	s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	s.MaxLOD = D3D12_FLOAT32_MAX;
	s.ShaderRegister = m.bind.registerNumber;
	s.RegisterSpace = m.bind.space;
	s.ShaderVisibility = m.visibility;
	return s;
}


//=============================================================================
// MergeBind から RootParameter 作成
//=============================================================================
RootSignatureInfo RootSignatureManager::MakeRootSignatureInfo(const std::vector<MergedBind>& binds) {
	std::vector<D3D12_ROOT_PARAMETER1> params;
	std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;
	ranges.reserve(binds.size()); // 再確保でポインタが飛ばないよう予約
	std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
	std::unordered_map<RootBind, UINT> slotOf;

	// 1. MergedBind を部品に振り分け
	for (const auto& m : binds) {
		if (m.bind.type == D3D_SIT_SAMPLER) {
			samplers.push_back(MakeStaticSampler(m)); // StaticSamplerはスロットを消費しない
			continue;
		}
		const UINT slot = static_cast<UINT>(params.size());
		params.push_back(MakeRootParameter(m, ranges));
		auto role = NameToRole(m.bind.name);
		slotOf[*role] = slot;
	}


	// ===== シリアライズして作成 =====;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
	desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	desc.Desc_1_1.NumParameters = static_cast<UINT>(params.size());
	desc.Desc_1_1.pParameters = params.data();
	desc.Desc_1_1.NumStaticSamplers = static_cast<UINT>(samplers.size());
	desc.Desc_1_1.pStaticSamplers = samplers.data();
	desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT 
		| D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED; // 手書き版に合わせる

	RootSignatureInfo result;
	result.rootSignature = CreateRootSignature(desc);
	result.slotOf = std::move(slotOf);
	return result;

}


//=============================================================================
// Desc から RootSignature作成
//=============================================================================
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