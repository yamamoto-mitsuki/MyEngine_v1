#pragma once
#include <cstdint>

#include <d3d12.h>
#include <wrl.h>

#include "MyEngine/Graphics/GPU/FrameUploadBuffer.h"
#include "MyEngine/Graphics/Renderer/DrawRequest.h"

// 前方宣言
class RenderWindow;


/// <summary>
/// 描画1回分のデータをGPU用のバッファに書き、DrawCallを発行するだけの実行層
/// <para>状態切替（RootSignature/PSO）はRenderQueueの仕事</para>
/// </summary>
class RenderContext {
public:
	// 1フレームに書けるGPU用データの合計（頂点・インデックス・定数・パーティクルの合計）
	static constexpr size_t kFrameUploadBytes = 64 * 1024 * 1024;

	RenderContext(const RenderContext&) = delete;
	RenderContext& operator=(const RenderContext&) = delete;
	// 初期化・解放
	static void Initialize();
	static void Release();

	/// <summary>
	/// 3Dメッシュを1つ描画する（動的=Primitive / 静的=Model の両対応）
	/// </summary>
	static void DrawMesh(const MeshRequest& req);

	/// <summary>
	/// 2Dスプライトを1つ描画する
	/// </summary>
	static void DrawSprite(const SpriteRequest& req);

	/// <summary>
	/// パーティクルを描画する
	/// </summary>
	static void DrawParticles(const ParticleRequest& req);

	/// <summary>
	/// 3Dライン群を描画する（LINELIST。奇数個の頂点は最後を切り捨て）
	/// </summary>
	static void DrawLines(const LineRequest& req);

	/// <summary>
	/// GPUページフォルトのアドレスがどのバッファ内かログに出力する
	/// </summary>
	static void LogFaultResource(D3D12_GPU_VIRTUAL_ADDRESS faultVA);

	/// <summary>
	/// このフレームのGPU用データの書き込み位置を先頭に戻す。フレームの終わりに1回呼ぶ
	/// </summary>
	static void ResetFrame();

	/// <summary>
	/// このフレームのライトを書き込む。全描画で同じ場所を結ぶ
	/// </summary>
	static void SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights, const SpotLightListData& spotLights);

	// 前のフレームでGPU用に書いたデータの量（バイト）。Profiler用
	static size_t GetLastFrameUploadBytes() { return instance_->frameUploadBuffer_.GetLastFrameUsedBytes(); }
	// 1フレームに書ける量（バイト）
	static size_t GetFrameUploadCapacity() { return instance_->frameUploadBuffer_.GetCapacity(); }


private:
	RenderContext() = default;
	~RenderContext() = default;

	static RenderContext* instance_;

	void InitInternal();

	// --- パーティクル用の共通Quad（最初に1回だけ書く） ---
	Microsoft::WRL::ComPtr<ID3D12Resource> particleQuadVB_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleQuadIB_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW particleQuadVBV_{};
	D3D12_INDEX_BUFFER_VIEW particleQuadIBV_{};

	// --- 描画ごとに増えるデータ（定数・頂点・インデックス・パーティクル）。全部ここに先頭から詰めて書く ---
	FrameUploadBuffer frameUploadBuffer_;

	// --- ライト（1フレームに1個。全描画で共有する） ---
	Microsoft::WRL::ComPtr<ID3D12Resource> frameDirectionalLightBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> framePointLightsBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> frameSpotLightsBuffer_ = nullptr;
	DirectionalLightData* frameDirectionalLightMappedPtr_ = nullptr;
	PointLightListData* framePointLightsMappedPtr_ = nullptr;
	SpotLightListData* frameSpotLightsMappedPtr_ = nullptr;
};