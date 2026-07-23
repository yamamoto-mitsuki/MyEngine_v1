#include "MyEngine/Graphics/Pipeline/PSOManager.h"

#include <cassert>
#include <format>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"

using namespace Microsoft::WRL;
PSOManager* PSOManager::instance_ = nullptr;

// シェーダーのパス定数
namespace {
// 使うシェーダーのバージョン
const wchar_t* kVSProfile = L"vs_6_0";
const wchar_t* kPSProfile = L"ps_6_0";

// ShaderProgramID から分かる PSO情報（ShaderProgramIDと順番一致させる！）
const std::array<PSODesc, magic_enum::enum_count<ShaderProgramID>()> kPSODescs = {
    {
		// --- Object3d.VS ---
		// Lambert
        {magic_enum::enum_name<ShaderProgramID::Model3dLambert>(), RootSignatureID::ModelLit, InputLayoutID::Model, TopologyID::Triangle, 
	    ShaderFile::Object3dVS, ShaderFile::LambertPS},
		// HalfLambert
        {magic_enum::enum_name<ShaderProgramID::Model3dHalfLambert>(), RootSignatureID::ModelLit, InputLayoutID::Model, TopologyID::Triangle, 
		ShaderFile::Object3dVS, ShaderFile::HalfLambertPS},
        // Phong
        {magic_enum::enum_name<ShaderProgramID::Model3dPhong>(), RootSignatureID::ModelLit, InputLayoutID::Model, TopologyID::Triangle, 
		ShaderFile::Object3dVS, ShaderFile::PhongPS},
	    // BlinnPhong
        {magic_enum::enum_name<ShaderProgramID::Model3dBlinnPhong>(), RootSignatureID::ModelLit, InputLayoutID::Model, TopologyID::Triangle, 
		ShaderFile::Object3dVS, ShaderFile::BlinnPhongPS},
		// PBR
        {magic_enum::enum_name<ShaderProgramID::Model3dPBR>(), RootSignatureID::ModelPBR, InputLayoutID::Model, TopologyID::Triangle, 
		ShaderFile::Object3dVS, ShaderFile::PBRPS},
		// Unlit
	    {magic_enum::enum_name<ShaderProgramID::Model3dUnlit>(), RootSignatureID::ModelUnlit, InputLayoutID::Model, TopologyID::Triangle, 
		ShaderFile::Object3dVS, ShaderFile::UnlitPS},

		// --- Particle.VS ---
        {magic_enum::enum_name<ShaderProgramID::Particle>(), RootSignatureID::Particle, InputLayoutID::Particle, TopologyID::Triangle,
        ShaderFile::ParticleVS, ShaderFile::ParticlePS},

		// --- Sprite2d.VS ---
	    {magic_enum::enum_name<ShaderProgramID::Sprite2d>(), RootSignatureID::Sprite, InputLayoutID::Sprite, TopologyID::Triangle,
        ShaderFile::Sprite2dVS, ShaderFile::Sprite2dPS},

		// --- Line3d.VS ---
        {magic_enum::enum_name<ShaderProgramID::Line3d>(), RootSignatureID::Line, InputLayoutID::Line, TopologyID::Line,
        ShaderFile::Line3dVS, ShaderFile::Line3dPS},
    }
};
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
//  ===== 初期化 =====
void PSOManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼び出されています");
	instance_ = new PSOManager();
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void PSOManager::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// SortKey
//=============================================================================
uint64_t PSOManager::GetSortKey(const PSOKey& key) { 
	RootSignatureID rs = kPSODescs[static_cast<size_t>(key.shaderProgramID)].rootSignatureID;
	return   (static_cast<uint64_t>(rs) << 32)                  // RootSignature
	       | (static_cast<uint64_t>(key.shaderProgramID) << 24) // 使用Shader
	       | (static_cast<uint64_t>(key.blendMode) << 16)       // BlendMode
		   | (static_cast<uint64_t>(key.rasterizerType) << 8)   // Rasterizer
		   |  static_cast<uint64_t>(key.depthMode);             // depthMode
}

//=============================================================================
// 描画情報 → ID・PSOKey
//=============================================================================
// ===== ShaderProgramID =====
ShaderProgramID PSOManager::GetShaderProgramID(DrawCategory category, ShadingType type) {
	// --- 描画したい形状 ---
	switch (category) {
	case DrawCategory::Sprite:
		return ShaderProgramID::Sprite2d;
	case DrawCategory::Line:
		return ShaderProgramID::Line3d;
	case DrawCategory::Particle:
		return ShaderProgramID::Particle;
	case DrawCategory::Model:
	    // シェーディング設定によってさらに分岐
		switch (type) {
		case ShadingType::Lambert:
			return ShaderProgramID::Model3dLambert;
		case ShadingType::HalfLambert:
			return ShaderProgramID::Model3dHalfLambert;
		case ShadingType::Phong:
			return ShaderProgramID::Model3dPhong;
		case ShadingType::BlinnPhong:
			return ShaderProgramID::Model3dBlinnPhong;
		case ShadingType::PBR:
			return ShaderProgramID::Model3dPBR;
		case ShadingType::Unlit:
			return ShaderProgramID::Model3dUnlit;
		}
		break;
	}
	MY_ASSERT_MSG(false, "未対応の DrawCategory / ShadingType");
	return ShaderProgramID::Model3dUnlit;
}

// ===== TopologyID =====
TopologyID PSOManager::GetTopologyID(DrawCategory category) {
	switch (category) {
	case DrawCategory::Line: // 線の描画のみIDがLine
		return TopologyID::Line;
	default:
		return TopologyID::Triangle;
	}
	return TopologyID::Triangle;
}

// ===== PSOKey =====
PSOKey PSOManager::GetPSOKey(DrawCategory category, ShadingType shading, BlendMode blend, RasterizerType raster, DepthMode depth) {
	return PSOKey{GetShaderProgramID(category, shading), blend, raster, depth};
}


//=============================================================================
// ID → D3D12・Shader
//=============================================================================
// ===== ShaderProgram =====
ShaderProgram PSOManager::GetShaderProgram(ShaderProgramID id) {
	const PSODesc& info = kPSODescs[static_cast<size_t>(id)];
	return {ShaderCompiler::GetShaderFile(info.vsFile), ShaderCompiler::GetShaderFile(info.psFile)};
}

// ===== Topology =====
D3D12_PRIMITIVE_TOPOLOGY_TYPE PSOManager::GetTopologyType(TopologyID id) {
	switch (id) {
	case TopologyID::Triangle: // Line以外
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case TopologyID::Line: // Line
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}


//=============================================================================
// Key → PSO
//=============================================================================
// ===== PSOName =====
const char* PSOManager::GetStateName(const PSOKey& key) { return kPSODescs[static_cast<size_t>(key.shaderProgramID)].stateName.data(); }

// ===== PipelineState =====
Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOManager::GetPSO(const PSOKey& key) {
	auto& map = instance_->psoMap_;
	// 登録済みの場合
	if (auto it = map.find(key); it != map.end()) {
		return it->second.Get();
	}
	// 未登録のとき
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = MakePSO(key);
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso = CreatePSO(psoDesc);
	// 名前をつける
	const PSODesc& info = kPSODescs[static_cast<size_t>(key.shaderProgramID)];
	pso->SetName(ConvertString(std::string(info.stateName)).c_str()); 
	map.emplace(key, pso);
	return pso;
}


//=============================================================================
// PSO作成
//=============================================================================
// ===== PSOKey → PipelineStateDesc =====
D3D12_GRAPHICS_PIPELINE_STATE_DESC PSOManager::MakePSO(const PSOKey& key) {
	// --- ShaderProgramID → 〇〇 ---
	// PSODesc
	const PSODesc& info = kPSODescs[static_cast<size_t>(key.shaderProgramID)];
	// IDxcBlob（ShaderCompile結果）
	ShaderProgram shader = GetShaderProgram(key.shaderProgramID);

	// --- PSODesc → 〇〇 ---
	// RootSignature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = RootSignatureManager::GetRootSignature(info.rootSignatureID);
	// InputLayout
	D3D12_INPUT_LAYOUT_DESC inputLayout = GetInputLayout(info.inputLayoutID);
	// Blend
	D3D12_BLEND_DESC blend = RenderStates::MakeBlendDesc(key.blendMode);
	// Rasterizer
	D3D12_RASTERIZER_DESC rasterizer = RenderStates::MakeRasterizerDesc(key.rasterizerType);
	// depthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencil = RenderStates::MakeDepthStencilDesc(key.depthMode);
	// Topology
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = GetTopologyType(info.topologyID);

	// --- PipelineStateDesc に書き込む ---
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature.Get();
	desc.InputLayout = inputLayout;
	desc.VS = {shader.vs->GetBufferPointer(), shader.vs->GetBufferSize()};
	desc.PS = {shader.ps->GetBufferPointer(), shader.ps->GetBufferSize()};
	desc.BlendState = blend;
	desc.RasterizerState = rasterizer;
	desc.DepthStencilState = depthStencil;
	desc.PrimitiveTopologyType = topology;
	// 書き込むRTVの情報
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// マルチサンプリングの設定
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	LogManager::Log("Created D3D12_GRAPHICS_PIPELINE_STATE_DESC");
	return desc;
}

// ===== PipelineStateDesc → CreatePSO =====
Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOManager::CreatePSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	HRESULT hr = DirectXCommon::GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "GraphicsPipelineStateの作成に失敗しました");
	}

	LogManager::Log("Create GraphicsPipelineState");
	return pipelineState;
}