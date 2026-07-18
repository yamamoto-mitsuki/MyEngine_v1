#pragma once
#include <cstdint>

#include <wrl.h>
#include <d3d12.h>

#include "MyEngine/Graphics/Renderer/DrawRequest.h"

// 前方宣言
class RenderWindow;


/// <summary>
/// リングバッファへの書き込みとDrawCallの発行だけを行う実行層
/// <para>状態切替（RootSignature/PSO）はRenderQueueの仕事</para>
/// </summary>
class RenderContext {
public:
	static constexpr size_t kMaxVertices = 100000;
	static constexpr size_t kMaxLineVertices = 65536;
	static constexpr size_t kMaxParticleInstances = 4096; 
	static constexpr size_t kMaxDrawCalls = 1024;

	RenderContext(const RenderContext&) = delete;
	RenderContext& operator=(const RenderContext&) = delete;
	// 初期化・解放
	static void Initialize();
	static void Release();

	/// <summary>3Dメッシュを1つ描画する（動的=Primitive / 静的=Model の両対応）</summary>
	static void DrawMesh(const MeshRequest& req);

	/// <summary>2Dスプライトを1つ描画する</summary>
	static void DrawSprite(const SpriteRequest& req, RenderWindow* renderWindow);

	/// <summary>パーティクルを描画する</summary>
	static void DrawParticles(const ParticleRequest& req);

	/// <summary>3Dライン群を描画する（LINELIST。奇数個の頂点は最後を切り捨て）</summary>
	static void DrawLines(const LineRequest& req);

	/// <summary>GPUページフォルトのアドレスがどのリングバッファ内かログに出力する</summary>
	static void LogFaultResource(D3D12_GPU_VIRTUAL_ADDRESS faultVA);

	/// <summary>DraCallIndexのリセット</summary>
	static void ResetDrawCallIndex();

private:
	RenderContext() = default;
	~RenderContext() = default;

	static RenderContext* instance_;

	void InitInternal();
	static size_t AlignTo256(size_t size);

	// --- パーティクル用の共通Quad ---
	Microsoft::WRL::ComPtr<ID3D12Resource> particleQuadVB_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleQuadIB_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW particleQuadVBV_{};
	D3D12_INDEX_BUFFER_VIEW particleQuadIBV_{};

	// --- リングバッファ ---
	// 共通
	Microsoft::WRL::ComPtr<ID3D12Resource> matricesDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightDataRingBuffer_ = nullptr;
	// 頂点
	Microsoft::WRL::ComPtr<ID3D12Resource> vertex2dDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertex3dDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexLineDataRingBuffer_ = nullptr;
	// インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> index2dDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> index3dDataRingBuffer_ = nullptr;
	// マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> material2dDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> material3dDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialParticleDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialLineDataRingBuffer_ = nullptr;
	// パーティクルがVSに送る情報
	Microsoft::WRL::ComPtr<ID3D12Resource> particleDataRingBuffer_ = nullptr;

	// --- 永続マップポインタ ---
	uint8_t* matricesDataMapperPtr_ = nullptr;
	uint8_t* cameraDataMappedPtr_ = nullptr;
	uint8_t* directionalLightDataMappedPtr_ = nullptr;
	uint8_t* pointLightDataMappedPtr_ = nullptr;
	uint8_t* vertex2dDataMappedPtr_ = nullptr;
	uint8_t* vertex3dDataMappedPtr_ = nullptr;
	uint8_t* vertexLineDataMappedPtr_ = nullptr;
	uint8_t* particleDataMappedPtr_ = nullptr;
	uint8_t* index2dDataMappedPtr_ = nullptr;
	uint8_t* index3dDataMappedPtr_ = nullptr;
	uint8_t* material2dDataMappedPtr_ = nullptr;
	uint8_t* material3dDataMappedPtr_ = nullptr;
	uint8_t* materialParticleDataMappedptr_ = nullptr;
	uint8_t* materialLineDataMappedPtr_ = nullptr;

	// --- CBufferスロットサイズ ---
	size_t alignedMatricesDataSlotSize_ = AlignTo256(sizeof(TransformationMatrixData));
	size_t alignedCameraDataSlotSize_ = AlignTo256(sizeof(CameraData));
	size_t alignedDirectionlLightDataSlotSize_ = AlignTo256(sizeof(DirectionalLightData));
	size_t alignedPointLightDataSlotSize_ = AlignTo256(sizeof(PointLightListData));
	size_t alignedMaterial2dDataSlotSize_ = AlignTo256(sizeof(Material2dData));
	size_t alignedMaterial3dDataSlotSize_ = AlignTo256(sizeof(Material3dData));
	size_t alignedMaterialParticleDataSlotSize_ = AlignTo256(sizeof(MaterialParticleData));
	size_t alignedMaterialLineDataSlotSize_ = AlignTo256(sizeof(MaterialLineData));

	// --- カウント ---
	size_t drawCallIndex_ = 0;
	size_t drawCallLineIndex_ = 0;
	size_t drawCallParticleIndex_ = 0;
	size_t vertex3dIndex_ = 0;
	size_t vertex2dIndex_ = 0;
	size_t vertexLineIndex_ = 0;
	size_t index3dIndex_ = 0;
	size_t index2dIndex_ = 0;
	size_t particleIndex_ = 0;
};