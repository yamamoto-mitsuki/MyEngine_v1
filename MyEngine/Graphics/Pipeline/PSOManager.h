#pragma once
#include <array>
#include <string>
#include <unordered_map>

#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>

#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/ShaderStructs.h"

// Shaderプログラム（VS,PSの組）。Shaderを増やすたびに追加
enum class ShaderID {
	Model3dLambert,
	Model3dHalfLambert,
	Model3dUnlit,
	Line3d,
	Sprite2d,
};

// PSO識別キー
struct PSOKey {
	ShaderID shader; // 使用するShader
	BlendMode blend = BlendMode::Normal; // 使用するBlend
	bool operator==(const PSOKey& other) const = default; // 比較用関数
	// 3Dモデル用の検索キー
	static PSOKey Model(ShadingType shading, BlendMode blend = BlendMode::Normal);
};
struct PSOKeyHash {
	size_t operator()(const PSOKey& key) const { 
		return (static_cast<size_t>(key.shader) << 8) | static_cast<size_t>(key.blend);
	}
};
// エンジン組み込みRootSignatureのキー
namespace RootSignatureKey {
std::string ModelKey(ShadingType shading); // 3Dモデル検索用関数
inline constexpr const char* Line3d = "Line3d";
inline constexpr const char* Sprite2d = "Sprite2d";
}

// PSO作成に必要なパラメータをまとめた構造体
struct PSODesc {
	IDxcBlob* vs = nullptr;                                                          // 頂点シェーダー
	IDxcBlob* ps = nullptr;                                                          // ピクセルシェーダー
	D3D12_INPUT_LAYOUT_DESC inputLayout{};                                           // 入力レイアウト
	ID3D12RootSignature* rootSignature = nullptr;                                    // ルートシグネチャ
	D3D12_DEPTH_STENCIL_DESC depthStencil{};                                         // 深度ステンシル設定
	BlendMode blend = BlendMode::Normal;                                             // ブレンドモード
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // トポロジ
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

	/// <summary>
	/// エンジン組み込みのPSO/RootSignatureを初期化する
	/// <para>DirectXCommon::Init()の後に呼ぶ</para>
	/// </summary>
	static void Initialize();

	/// <summary>
	/// 全リソースを解放する
	/// </summary>
	static void Release();

	//==========================================
	// ゲッター
	//==========================================

	/// <summary>
	/// PSOKeyでPSOを取得する。未生成なら内部で生成してキャッシュ
	/// </summary>
	/// <param name = "key">使用するShaderID,BlendModeを指定して作るPSOkey</param>
	static ID3D12PipelineState* GetPSO(const PSOKey& key);

	

	/// <summary>
	/// RootSignatureをstringキーで取得する。エンジン組み込み: RootSignature::xxx を使う
	/// </summary>
	static ID3D12RootSignature* GetRootSignature(const std::string& key);

	//==========================================
	// RootParameter生成
	//==========================================

	/// <summary>
	/// バインドレスSRVのDescriptorTableパラメータを生成する。必ずparams[0]に配置すること
	/// </summary>
	static D3D12_ROOT_PARAMETER MakeRootParamBindlessTable(D3D12_DESCRIPTOR_RANGE& outRange, UINT registerSpace = 0);

	/// <summary>
	/// 定数バッファ(CBV)のRootParameterを生成する
	/// </summary>
	static D3D12_ROOT_PARAMETER MakeRootParameterCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister);


	static D3D12_ROOT_PARAMETER MakeRootParamsSRV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT registerSpace);

	//==========================================
	// RootSignature生成
	//==========================================

	/// <summary>
	/// RootSignatureを生成する。
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(D3D12_ROOT_PARAMETER* params, 
		UINT paramCount, bool hasSampler, bool hasInputLayout);

	//==========================================
	// DepthStencilDescヘルパー
	//==========================================

	/// <summary>3D用（深度テストあり・深度書き込みあり）</summary>
	static D3D12_DEPTH_STENCIL_DESC DepthStencilDesc3d();

	/// <summary>2D用（深度テストなし・深度書き込みなし）</summary>
	static D3D12_DEPTH_STENCIL_DESC DepthStencilDesc2d();

	/// <summary>深度なし（レイマーチング・フルスクリーンエフェクト用）</summary>
	static D3D12_DEPTH_STENCIL_DESC DepthStencilDescNone();

	//==========================================
	// Sampler
	//==========================================

	/// <summary>
	/// サンプラーを生成する
	/// </summary>
	static D3D12_STATIC_SAMPLER_DESC MakeSampler(UINT registerIndex, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode);

	/// <summary>
	/// シャドウマップ専用サンプラーを生成する
	/// </summary>
	static D3D12_STATIC_SAMPLER_DESC MakeShadowMapSampler(UINT registerIndex);

	// 全シェーダー共通のサンプラー配列を生成
	std::array<D3D12_STATIC_SAMPLER_DESC, 2> MakeSamplers();


	/// <summary>
	/// GraphicsPipelineStateObjectを生成する
	/// <para>プロジェクト側でカスタムシェーダーを作る場合は直接呼んでもよい</para>
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(const PSODesc& desc);

private:
	PSOManager() = default;
	~PSOManager() = default;

	static PSOManager* instance_;
	void InternalInit();
	// PSOKey から PSODesc を組み立てる
	PSODesc MakePSODesc(const PSOKey& key);

	// 個別RootSignature生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature2d();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dLit();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dNoLit();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureLine3d();

    // --- コンパイルするシェーダー ---
	// VS
	Microsoft::WRL::ComPtr<IDxcBlob> sprite2dVS_;
	Microsoft::WRL::ComPtr<IDxcBlob> object3dVS_;
	Microsoft::WRL::ComPtr<IDxcBlob> line3dVS_;
	// PS
	Microsoft::WRL::ComPtr<IDxcBlob> sprite2dPS_;
	Microsoft::WRL::ComPtr<IDxcBlob> lambertPS_;
	Microsoft::WRL::ComPtr<IDxcBlob> halfLambertPS_;
	Microsoft::WRL::ComPtr<IDxcBlob> unlitPS_;
	Microsoft::WRL::ComPtr<IDxcBlob> line3dPS_;
	

	// PSO・RootSignatureをキーで管理
	std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>, PSOKeyHash> psoMap_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSigMap_;
};