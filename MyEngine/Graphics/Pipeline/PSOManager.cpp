#include "PSOManager.h"
#include <format>
#include <cassert>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"

using namespace Microsoft::WRL;
PSOManager* PSOManager::instance_ = nullptr;


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
	return (
		static_cast<uint64_t>(key.shaderProgramID) << 24) // ShadingType
     | (static_cast<uint64_t>(key.blendMode)       << 16) // BlendMOde
     | (static_cast<uint64_t>(key.rasterizerType)  << 8)  // Rasterizer
	 |  static_cast<uint64_t>(key.depthMode);             // DepthMode
}


//=============================================================================
// RootSignatureInfoを取得
//=============================================================================
const RootSignatureInfo& PSOManager::GetRootSignatureInfo(uint32_t id) {
	auto& cache = instance_->rootSignatureCache_;
	// キャッシュを確認
	if (id >= cache.size()) {
		cache.resize(ShaderPackageLoader::GetProgramCount());
	}
	
	RootSignatureInfo& slot = cache[id]; // RootSignature情報を取得
	// 取得できなかったら、新しく作って登録
	if (!slot.rootSignature) {
		const ShaderProgramInfo& p = ShaderPackageLoader::GetProgramAt(id);
		const ShaderReflection& vs = ShaderCompiler::GetShaderReflection(p.vsInfo.path, p.vsInfo.profile, p.vsInfo.entry);
		const ShaderReflection& ps = ShaderCompiler::GetShaderReflection(p.psInfo.path, p.psInfo.profile, p.psInfo.entry);
		auto merged = RootSignatureManager::MergeStages(vs.resources, ps.resources);
		slot = RootSignatureManager::BuildRootSignature(merged);
		slot.rootSignature->SetName(ConvertString("RootSig_" + p.name).c_str());
	}
	return slot;
}



//=============================================================================
// 描画情報 → ID・PSOKey
//=============================================================================
// ===== ShaderProgramID =====
uint32_t PSOManager::GetShaderProgramID(DrawCategory category, ShadingType type) { 
	return static_cast<uint32_t>(ShaderPackageLoader::GetProgramIndex(category, type));
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

// ===== InputLayoutID =====
InputLayoutID PSOManager::InputLayoutOf(DrawCategory c) {
	switch (c) {
	case DrawCategory::Model:
		return InputLayoutID::Model;
	case DrawCategory::Particle:
		return InputLayoutID::Particle;
	case DrawCategory::Sprite:
		return InputLayoutID::Sprite;
	case DrawCategory::Line:
		return InputLayoutID::Line;
	}
	return InputLayoutID::Model;
}


//=============================================================================
// ID → D3D12・Shader
//=============================================================================
ShaderProgram PSOManager::GetShaderProgram(uint32_t id) {
	const ShaderProgramInfo& p = ShaderPackageLoader::GetProgramAt(id);
	return {ShaderCompiler::GetShaderReflection(p.vsInfo.path, p.vsInfo.profile, p.vsInfo.entry).blob.Get(),  // VS
		ShaderCompiler::GetShaderReflection(p.psInfo.path, p.psInfo.profile, p.psInfo.entry).blob.Get()};     // PS
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
const char* PSOManager::GetStateName(const PSOKey& key) { return ShaderPackageLoader::GetProgramAt(key.shaderProgramID).name.c_str(); }

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
	// -- PipelineStateDescに必要な情報を取得 ---
	const ShaderProgramInfo& p = ShaderPackageLoader::GetProgramAt(key.shaderProgramID);
	ShaderProgram shader = GetShaderProgram(key.shaderProgramID);
	const RootSignatureInfo& rootSig = GetRootSignatureInfo(key.shaderProgramID);
	D3D12_INPUT_LAYOUT_DESC inputLayout = GetInputLayout(InputLayoutOf(p.drawCategory));
	D3D12_BLEND_DESC blend = RenderStates::MakeBlendDesc(key.blendMode);
	D3D12_RASTERIZER_DESC rasterizer = RenderStates::MakeRasterizerDesc(key.rasterizerType);
	D3D12_DEPTH_STENCIL_DESC depth = RenderStates::MakeDepthStencilDesc(key.depthMode);
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = GetTopologyType(GetTopologyID(p.drawCategory));

	// --- PipelineStateDesc に書き込む ---
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSig.rootSignature.Get();
	desc.InputLayout = inputLayout;
	desc.VS = {shader.vs->GetBufferPointer(), shader.vs->GetBufferSize()};
	desc.PS = {shader.ps->GetBufferPointer(), shader.ps->GetBufferSize()};
	desc.BlendState = blend;
	desc.RasterizerState = rasterizer;
	desc.DepthStencilState = depth;
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