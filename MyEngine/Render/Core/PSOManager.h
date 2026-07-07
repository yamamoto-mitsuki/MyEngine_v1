#pragma once
#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Render/Core/ShaderStructs.h"
#include <array>
#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

//==========================================
// エンジン組み込みPSOのキー
//==========================================
namespace BuiltinPSO {
std::string ModelKey(ShadingModel shading, bool instanced);
inline constexpr const char* Line3d = "Line3d";
inline constexpr const char* Sprite2d = "Sprite2d";
}

//==========================================
// エンジン組み込みRootSignatureのキー
//==========================================
namespace BuiltinRootSig {
std::string ModelKey(ShadingModel shading, bool instanced);
inline constexpr const char* Line3d = "Line3d";
inline constexpr const char* Sprite2d = "Sprite2d";
}

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
	/// PSOをstringキーで取得する。エンジン組み込み: BuiltinPSO::xxx を使う
	/// </summary>
	static ID3D12PipelineState* GetPSO(const std::string& key);

	/// <summary>
	/// RootSignatureをstringキーで取得する。エンジン組み込み: BuiltinRootSig::xxx を使う
	/// </summary>
	static ID3D12RootSignature* GetRootSignature(const std::string& key);

	//==========================================
	// 登録（プロジェクト側のシェーダー追加時に使う）
	//==========================================

	/// <summary>
	/// プロジェクト側からPSOを登録する
	/// </summary>
	static void RegisterPSO(const std::string& key, Microsoft::WRL::ComPtr<ID3D12PipelineState> pso);

	/// <summary>
	/// プロジェクト側からRootSignatureを登録する
	/// </summary>
	static void RegisterRootSignature(const std::string& key, Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig);

	/// <summary>
	/// PSOが登録済みか確認
	/// </summary>
	static bool HasPSO(const std::string& key) { return instance_->psoMap_.count(key) > 0; }

	/// <summary>
	/// RootSignatureが登録済みか確認
	/// </summary>
	static bool HasRootSignature(const std::string& key) { return instance_->rootSigMap_.count(key) > 0; }

	//==========================================
	// RootParameter生成
	//==========================================

	/// <summary>
	/// バインドレスSRVのDescriptorTableパラメータを生成する。必ずparams[0]に配置すること
	/// </summary>
	static D3D12_ROOT_PARAMETER CreateDescriptorTableSRV(D3D12_DESCRIPTOR_RANGE& outRange, UINT registerSpace = 0);

	/// <summary>
	/// 定数バッファ(CBV)のRootParameterを生成する
	/// </summary>
	static D3D12_ROOT_PARAMETER CreateCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister);

	//==========================================
	// RootSignature生成
	//==========================================

	/// <summary>
	/// RootSignatureを生成する。DirectXCommonがシングルトン化されたためdxCommon引数不要
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(D3D12_ROOT_PARAMETER* params, UINT paramCount, bool hasSampler, bool hasInputLayout);

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
	static D3D12_STATIC_SAMPLER_DESC CreateSampler(UINT registerIndex, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode);

	/// <summary>
	/// シャドウマップ専用サンプラーを生成する
	/// </summary>
	static D3D12_STATIC_SAMPLER_DESC CreateShadowMapSampler(UINT registerIndex);

	//==========================================
	// バッファ生成
	// DirectXCommonがシングルトン化されたためdxCommon引数不要
	//==========================================

	/// <summary>
	/// 頂点バッファを生成してViewを返す
	/// </summary>
	template<typename T> 
	static void CreateVertexBuffer(const T* data, UINT count, Microsoft::WRL::ComPtr<ID3D12Resource>& outVB, D3D12_VERTEX_BUFFER_VIEW& outView) {
		size_t bufferSize = sizeof(T) * count;
		// GPU上にバッファ領域を確保
		outVB = DirectXCommon::CreateBufferResource(bufferSize);
		void* mapped = nullptr;
		// GPUへデータ転送
		outVB->Map(0, nullptr, &mapped);
		memcpy(mapped, data, bufferSize);
		outVB->Unmap(0, nullptr);
		// GPUがこのバッファを頂点データとして読むための情報を設定
		outView.BufferLocation = outVB->GetGPUVirtualAddress();
		outView.SizeInBytes = static_cast<UINT>(bufferSize);
		outView.StrideInBytes = sizeof(T);
	}

	/// <summary>
	/// インデックスバッファを生成してViewを返す
	/// </summary>
	template<typename T> 
    static void CreateIndexBuffer(const T* data, UINT count, Microsoft::WRL::ComPtr<ID3D12Resource>& outIB, D3D12_INDEX_BUFFER_VIEW& outView) {
		static_assert(std::is_same_v<T, UINT> || std::is_same_v<T, USHORT>, "インデックスバッファはUINTかUSHORTのみ対応");
		size_t bufferSize = sizeof(T) * count;
		// GPU上にバッファ領域を確保
		outIB = DirectXCommon::CreateBufferResource(bufferSize);
		void* mapped = nullptr;
		// GPUへデータ転送
		outIB->Map(0, nullptr, &mapped);
		memcpy(mapped, data, bufferSize);
		outIB->Unmap(0, nullptr);
		// GPUがこのバッファをインデックスデータとして読むための情報を設定
		outView.BufferLocation = outIB->GetGPUVirtualAddress();
		outView.SizeInBytes = static_cast<UINT>(bufferSize);
		outView.Format = std::is_same_v<T, UINT> ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	}

private:
	PSOManager() = default;
	~PSOManager() = default;

	static PSOManager* instance_;
	void InternalInit();

	static D3D12_ROOT_PARAMETER CreateRootSRV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT registerSpace);

	// 全シェーダー共通のサンプラー配列を返す
	std::array<D3D12_STATIC_SAMPLER_DESC, 2> GetSamplers();

	// 個別RootSignature生成（DirectXCommonがシングルトン化されたためdxCommon引数不要）
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dLit();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dNoLit();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dInstancedLit();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dInstancedNoLit();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature2d();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureLine3d();

	// PSO・RootSignatureをstringキーで管理
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> psoMap_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSigMap_;
};