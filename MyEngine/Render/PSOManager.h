#pragma once
#include <array>
#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

// 前方宣言
class DirectXCommon;

//==========================================
// エンジン組み込みPSOのキー
// プロジェクト側でGetPSO()を呼ぶときはこれを使う
//==========================================
namespace BuiltinPSO {
inline constexpr const char* Model3dLitTex = "Model3dLitTex";             // 3D・ランバート反射・テクスチャあり
inline constexpr const char* Model3dLitNoTex = "Model3dLitNoTex";         // 3D・ランバート反射・テクスチャなし
inline constexpr const char* Model3dHalfLitTex = "Model3dHalfLitTex";     // 3D・ハーフランバート・テクスチャあり
inline constexpr const char* Model3dHalfLitNoTex = "Model3dHalfLitNoTex"; // 3D・ハーフランバート・テクスチャなし
inline constexpr const char* Model3dNoLitTex = "Model3dNoLitTex";         // 3D・ライティングなし・テクスチャあり
inline constexpr const char* Model3dNoLitNoTex = "Model3dNoLitNoTex";     // 3D・ライティングなし・テクスチャなし
inline constexpr const char* Line3d = "Line3d";                           // 3Dライン描画
inline constexpr const char* Sprite2dTex = "Sprite2dTex";                 // 2Dスプライト・テクスチャあり
inline constexpr const char* Sprite2dNoTex = "Sprite2dNoTex";             // 2Dスプライト・テクスチャなし
}

//==========================================
// エンジン組み込みRootSignatureのキー
//==========================================
namespace BuiltinRootSig {
inline constexpr const char* Model3dLit = "Model3dLit";     // 3D・ランバート/ハーフランバート共通
inline constexpr const char* Model3dNoLit = "Model3dNoLit"; // 3D・ライティングなし
inline constexpr const char* Line3d = "Line3d";             // 3Dライン
inline constexpr const char* Sprite2d = "Sprite2d";         // 2Dスプライト
} // namespace BuiltinRootSig

/// <summary>
/// PSO・RootSignatureを一元管理するクラス
/// エンジン組み込みはInit()で登録、プロジェクト固有はRegisterPSO()/RegisterRootSignature()で登録する
/// </summary>
class PSOManager {
public:
	// シングルトン
	static PSOManager* GetInstance();
	PSOManager() = default;
	~PSOManager() = default;
	PSOManager(const PSOManager&) = delete;
	PSOManager& operator=(const PSOManager&) = delete;
	PSOManager(PSOManager&&) = delete;
	PSOManager& operator=(PSOManager&&) = delete;

	/// <summary>
	/// エンジン組み込みのPSO/RootSignatureを初期化する
	/// </summary>
	void Init(DirectXCommon* dxCommon);

	/// <summary>
	/// 全リソースを解放する
	/// </summary>
	void Finalize();

	//==========================================
	// ゲッター
	//==========================================

	/// <summary>
	/// PSOをstringキーで取得する
	/// エンジン組み込み: BuiltinPSO::xxx を使う
	/// プロジェクト独自: RegisterPSO()時に指定したキー文字列を使う
	/// </summary>
	static ID3D12PipelineState* GetPSO(const std::string& key);

	/// <summary>
	/// RootSignatureをstringキーで取得する
	/// エンジン組み込み: BuiltinRootSig::xxx を使う
	/// プロジェクト独自: RegisterRootSignature()時に指定したキー文字列を使う
	/// </summary>
	static ID3D12RootSignature* GetRootSignature(const std::string& key);

	//==========================================
	// 登録（プロジェクト側のシェーダー追加時に使う）
	//==========================================

	/// <summary>
	/// プロジェクト側からPSOを登録する
	/// シーンのInitialize()でシェーダーをコンパイルしてCreatePSO()した後に呼ぶ
	/// </summary>
	/// <param name="key">GetPSO()で使うキー名。タイポ防止のためnamespace定数を推奨</param>
	/// <param name="pso">登録するPSO</param>
	void RegisterPSO(const std::string& key, Microsoft::WRL::ComPtr<ID3D12PipelineState> pso);

	/// <summary>
	/// プロジェクト側からRootSignatureを登録する
	/// </summary>
	/// <param name="key">GetRootSignature()で使うキー名。タイポ防止のためnamespace定数を推奨</param>
	/// <param name="rootSig">登録するRootSignature</param>
	void RegisterRootSignature(const std::string& key, Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig);

	/// <summary>
	/// バインドレスSRVのDescriptorTableパラメータを生成する
	/// <para>全テクスチャにインデックスでアクセスできるようになる</para>
	/// <para>必ずparams[0]に配置すること</para>
	/// <para>params[0]: CreateDescriptorTableSRV() ← バインドレステクスチャ（必ず先頭）</para>
	/// </summary>
	/// <param name="outRange">SRVレンジの出力先（CreateRootSignatureに渡すまで生存させること）</param>
	/// <param name="registerSpace">レジスタ空間（通常は0固定）</param>
	D3D12_ROOT_PARAMETER CreateDescriptorTableSRV(D3D12_DESCRIPTOR_RANGE& outRange, UINT registerSpace = 0);

	/// <summary>
	/// 定数バッファ(CBV)のRootParameterを生成する
	/// <para>params[0]: CreateDescriptorTableSRV() ← バインドレステクスチャ（必ず先頭）</para>
	/// <para>params[1]: CreateCBV(PIXEL,  b0)      ← PS用マテリアル等</para>
	/// <para>params[2]: CreateCBV(VERTEX, b0)      ← VS用行列等</para>
	/// <para>params[3~]: CreateCBV(PIXEL,  b1)     ← 追加の定数バッファ</para>
	/// </summary>
	/// <param name="visibility">参照するシェーダー: PIXEL=PSのみ / VERTEX=VSのみ / ALL=全シェーダー</param>
	/// <param name="shaderRegister">HLSLのregister(bN)のN番号</param>
	D3D12_ROOT_PARAMETER CreateCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister);

	//==========================================
	// RootSignature生成
	//==========================================

	/// <summary>
	/// RootSignatureを生成
	/// サンプラー設定・シリアライズ・生成の定型処理を共通化している
	/// </summary>
	/// <param name="params">CreateCBV()/CreateDescriptorTableSRV()で生成したパラメータ配列</param>
	/// <param name="paramCount">パラメータの数</param>
	/// <param name="hasSampler">テクスチャSRVを使う場合true（サンプラーをセット）</param>
	/// <param name="hasInputLayout">頂点バッファを使う場合true（レイマーチング等はfalse）</param>
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(DirectXCommon* dxCommon, D3D12_ROOT_PARAMETER* params, UINT paramCount, bool hasSampler, bool hasInputLayout);

	//==========================================
	// DepthStencilDescヘルパー
	//==========================================

	/// <summary>3D用（深度テストあり・深度書き込みあり）</summary>
	D3D12_DEPTH_STENCIL_DESC DepthStencilDesc3d();

	/// <summary>2D用（深度テストなし・深度書き込みなし）</summary>
	D3D12_DEPTH_STENCIL_DESC DepthStencilDesc2d();

	/// <summary>深度なし（レイマーチング・フルスクリーンエフェクト用）</summary>
	D3D12_DEPTH_STENCIL_DESC DepthStencilDescNone();

	//==========================================
	// Samplerヘルパー
	//==========================================

	/// <summary>
	/// サンプラーを生成する
	/// </summary>
	/// <param name="registerIndex">HLSLのregister番号</param>
	/// <param name="filter">補間方法: LINEAR=なめらか / POINT=くっきり / ANISOTROPIC=斜めもなめらか</param>
	/// <param name="addressMode">UV範囲外の挙動: WRAP=繰り返す / CLAMP=端を引き伸ばす</param>
	D3D12_STATIC_SAMPLER_DESC CreateSampler(UINT registerIndex, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode);

	/// <summary>
	/// シャドウマップ専用サンプラーを生成する
	/// </summary>
	D3D12_STATIC_SAMPLER_DESC CreateShadowMapSampler(UINT registerIndex);

private:
	//==========================================
	// 内部ヘルパー（エンジン組み込みのみ使用）
	//==========================================

	void InternalInit(DirectXCommon* dxCommon);

	// 全シェーダー共通のサンプラー配列を返す
	// s0: LinearWrap（通常テクスチャ用）
	// s1: ShadowMap（シャドウマップ実装時に使用）
	std::array<D3D12_STATIC_SAMPLER_DESC, 2> GetSamplers();

	// 個別RootSignature生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dLit(DirectXCommon* dxCommon);
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dNoLit(DirectXCommon* dxCommon);
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature2d(DirectXCommon* dxCommon);
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureLine3d(DirectXCommon* dxCommon);

	// PSO・RootSignatureをstringキーで管理
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> psoMap_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSigMap_;
};