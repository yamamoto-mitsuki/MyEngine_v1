#pragma once
#include <array>
#include <string>
#include <unordered_map>

#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>

#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"


//==========================================
// PSO全要素の情報
//==========================================
// 1つの描画に使うシェーダーの組
struct ShaderProgram {
	IDxcBlob* vs;
	IDxcBlob* ps;
};
// Shaderプログラム（VS,PSの組）。Shaderを増やすたびに追加
enum class ShaderProgramID {
	Model3dLambert,
	Model3dHalfLambert,
	Model3dPhong,
	Model3dBlinnPhong,
	Model3dUnlit,
	Sprite2d,
	Line3d,
};
// 入力されたプリミティブをどう解釈するか
enum class TopologyID {
	Triangle,
	Line,
};
// ShaderProgramIDで引く表
struct PSODesc {
	std::string_view stateName; // SetPipelineStateで使用するShaderの名前
	RootSignatureID rootSignatureID;
	InputLayoutID inputLayoutID;
	TopologyID topologyID;
	ShaderFile vsFile;
	ShaderFile psFile;
};
// PSOの一意キー
struct PSOKey {
	ShaderProgramID shaderProgramID;
	BlendMode blendMode;
	RasterizerType rasterizerType;
	DepthMode depthMode;
	bool operator==(const PSOKey&) const = default;
};
struct PSOHash {
	size_t operator()(const PSOKey& k) const { 
		size_t h = static_cast<size_t>(k.shaderProgramID);
		h = h * 31 + static_cast<size_t>(k.blendMode);
		h = h * 31 + static_cast<size_t>(k.rasterizerType);
		h = h * 31 + static_cast<size_t>(k.depthMode);
		return h;
	}
};


/// <summary>
/// PSO・RootSignatureを一元管理するクラス
/// </summary>
class PSOManager {
public:
	// コピー・ムーブ禁止
	PSOManager(const PSOManager&) = delete;
	PSOManager& operator=(const PSOManager&) = delete;
	PSOManager(PSOManager&&) = delete;
	PSOManager& operator=(PSOManager&&) = delete;

	static void Initialize();
	static void Release();

	//==========================================
	// 描画情報 → ID・PSOKey
	//==========================================

	/// <summary>
	/// 描画情報から使用する Shader が分かるIDを取得
	/// </summary>
	/// <param name="drawCategory">描画したい形状</param>
	/// <param name="shadingType">HalfLambert, Unlitなどの表現したいShading</param>
	/// <returns></returns>
	static ShaderProgramID GetShaderProgramID(DrawCategory drawCategory, ShadingType shadingType);

	/// <summary>
	/// 描画情報から TopologyID を入手
	/// </summary>
	/// <param name="category">描画したい形</param>
	static TopologyID GetTopologyID(DrawCategory category);

	/// <summary>
	/// GraphicsPipelineState を取得するための PSOKey を取得する
	/// </summary>
	/// <param name="drawCategory">描画する形</param>
	/// <param name="shadingType">シェーディング設定</param>
	/// <param name="blendMode">ブレンド設定</param>
	/// <param name="rasterizerType">ラスタライザ設定</param>
	/// <param name="depthMode">深度設定</param>
	static PSOKey GetPSOKey(DrawCategory drawCategory, ShadingType shadingType, BlendMode blendMode = BlendMode::Normal, 
		RasterizerType rasterizerType = RasterizerType::SolidBack, DepthMode depthMode = DepthMode::TestWrite);

	//==========================================
	// Key → PSO
	//==========================================
	
	/// <summary>
	/// PipelineStateの状態名を取得
	/// </summary>
	static const char* GetStateName(const PSOKey& key);

	/// <summary>
	/// PipelineStateを取得
	/// </summary>
	/// <param name="rootSignatureID"></param>
	/// <param name="inputLayoutID"></param>
	/// <param name="ShaderProgramID"></param>
	/// <param name="blendMode"></param>
	/// <param name="topologyID"></param>
	/// <returns></returns>
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPSO(const PSOKey& key);


	//==========================================
	// PSO作成
	//==========================================

	/// <summary>
	/// CreatePSO に必要な PipelineState設定 を作成する
	/// </summary>
	static D3D12_GRAPHICS_PIPELINE_STATE_DESC MakePSO(const PSOKey& key);

	/// <summary>
	/// PSOKey を元に ID3D12PipelineState を作成する
	/// </summary>
	/// <param name="key"></param>
	/// <returns></returns>
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);


	//==========================================
	// SortKey
	//==========================================

	/// <summary>
	/// 描画用のソートキーを取得を取得。値が近いほど描画状態の切り替えが少ない
	/// </summary>
	/// <param name="key"></param>
	/// <returns></returns>
	static uint64_t GetSortKey(const PSOKey& key);


private:
	//==========================================
	// ID → D3D12・ShaderProgram
	//==========================================

	/// <summary>
	/// ShaderProgramID を参照して CreatePSO で使うシェーダーを設定する
	/// </summary>
	static ShaderProgram GetShaderProgram(ShaderProgramID id);

	/// <summary>
	/// TopologyID から PipelineStateDesc の作成に使う設定を作成
	/// </summary>
	/// <param name="id"></param>
	/// <returns></returns>
	static D3D12_PRIMITIVE_TOPOLOGY_TYPE GetTopologyType(TopologyID id);


	PSOManager() = default;
	~PSOManager() = default;

	// インスタンス
	static PSOManager* instance_;
	
	// PSOをキーで管理
	std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>, PSOHash> psoMap_;
};